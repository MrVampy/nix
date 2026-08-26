#include "nix/cmd/command-installable-value.hh"
#include "nix/cmd/installable-flake.hh"
#include "nix/main/common-args.hh"
#include "nix/main/shared.hh"
#include "nix/store/store-api.hh"
#include "nix/expr/eval.hh"
#include "nix/expr/eval-inline.hh"
#include "nix/expr/value-to-json.hh"
#include "nix/util/file-system.hh"
#include "nix/util/recording-source-accessor.hh"

#include <nlohmann/json.hpp>

using namespace nix;

static std::string_view sourceReadTypeName(SourceReadType type)
{
    switch (type) {
    case SourceReadType::Stat:
        return "stat";
    case SourceReadType::File:
        return "file";
    case SourceReadType::Directory:
        return "directory";
    case SourceReadType::Symlink:
        return "symlink";
    }
    unreachable();
}

static std::string_view sourceReadOutcomeName(SourceReadOutcome outcome)
{
    switch (outcome) {
    case SourceReadOutcome::Present:
        return "present";
    case SourceReadOutcome::Absent:
        return "absent";
    case SourceReadOutcome::Failed:
        return "failed";
    }
    unreachable();
}

struct CmdEval : MixJSON, InstallableValueCommand, MixReadOnlyOption
{
    bool raw = false;
    std::optional<std::string> apply;
    std::optional<std::filesystem::path> writeTo;
    std::optional<std::filesystem::path> writeReadSet;

    CmdEval()
        : InstallableValueCommand()
    {
        addFlag({
            .longName = "raw",
            .description = "Print strings without quotes or escaping.",
            .handler = {&raw, true},
        });

        addFlag({
            .longName = "apply",
            .description = "Apply the function *expr* to each argument.",
            .labels = {"expr"},
            .handler = {&apply},
        });

        addFlag({
            .longName = "write-to",
            .description = "Write a string or attrset of strings to *path*.",
            .labels = {"path"},
            .handler = {&writeTo},
        });

        addFlag({
            .longName = "write-read-set",
            .description = "Write the source paths read while forcing the result to *path*.",
            .labels = {"path"},
            .handler = {[&](std::string path) {
                writeReadSet = path;
                evalSettings.traceSourceReads = true;
            }},
        });
    }

    std::string description() override
    {
        return "evaluate a Nix expression";
    }

    std::string doc() override
    {
        return
#include "eval.md"
            ;
    }

    Category category() override
    {
        return catSecondary;
    }

    void run(ref<Store> store, ref<InstallableValue> installable) override
    {
        if (raw && json)
            throw UsageError("--raw and --json are mutually exclusive");

        if (writeReadSet && pathExists(*writeReadSet))
            throw Error("path '%s' already exists", writeReadSet->string());

        auto state = getEvalState();

        auto [v, pos] = installable->toValue(*state);
        NixStringContext context;

        if (apply) {
            auto vApply = state->allocValue();
            state->eval(state->parseExprFromString(*apply, state->rootPath(".")), *vApply);
            auto vRes = state->allocValue();
            state->callFunction(*vApply, *v, *vRes, noPos);
            v = vRes;
        }

        if (writeTo) {
            logger->stop();

            if (pathExists(*writeTo))
                throw Error("path '%s' already exists", writeTo->string());

            [&](this const auto & recurse, Value & v, const PosIdx pos, const std::filesystem::path & path) -> void {
                state->forceValue(v, pos);
                if (v.type() == nString)
                    // FIXME: disallow strings with contexts?
                    writeFile(path, v.string_view());
                else if (v.type() == nAttrs) {
                    [[maybe_unused]] bool directoryCreated = std::filesystem::create_directory(path);
                    // Directory should not already exist
                    assert(directoryCreated);
                    for (auto & attr : *v.attrs()) {
                        std::string_view name = state->symbols[attr.name];
                        try {
                            if (name == "." || name == "..")
                                throw Error("invalid file name '%s'", name);
                            recurse(*attr.value, attr.pos, path / name);
                        } catch (Error & e) {
                            e.addTrace(
                                state->positions[attr.pos], HintFmt("while evaluating the attribute '%s'", name));
                            throw;
                        }
                    }
                } else
                    state->error<TypeError>("value at '%s' is not a string or an attribute set", state->positions[pos])
                        .debugThrow();
            }(*v, pos, *writeTo);
        }

        else if (raw) {
            logger->stop();
            writeFull(
                getStandardOutput(),
                *state->coerceToString(noPos, *v, context, "while generating the eval command output"));
        }

        else if (json) {
            printJSON(printValueAsJSON(*state, true, *v, pos, context, false));
        }

        else {
            logger->cout("%s", ValuePrinter(*state, *v, PrintOptions{.force = true, .derivationPaths = true}));
        }

        if (writeReadSet) {
            assert(state->sourceReadRecorder);
            auto lockedFlake = nlohmann::json(nullptr);
            if (auto flake = dynamic_cast<InstallableFlake *>(&*installable))
                lockedFlake = flake->getLockedFlake()->flake.lockedRef.to_string();
            auto entries = nlohmann::json::array();
            for (auto & read : state->sourceReadRecorder->get()) {
                entries.push_back({
                    {"access", sourceReadTypeName(read.type)},
                    {"outcome", sourceReadOutcomeName(read.outcome)},
                    {"logical_path", read.logicalPath.abs()},
                    {"source_path", read.sourcePath.abs()},
                    {"fingerprint", read.fingerprint ? nlohmann::json(*read.fingerprint) : nlohmann::json(nullptr)},
                });
            }
            auto document = nlohmann::json{
                {"schema_id", "nix-eval-read-set"},
                {"installable", installable->what()},
                {"locked_flake", std::move(lockedFlake)},
                {"entries", std::move(entries)},
            };
            writeFile(*writeReadSet, document.dump() + "\n");
        }
    }
};

static auto rCmdEval = registerCommand<CmdEval>("eval");
