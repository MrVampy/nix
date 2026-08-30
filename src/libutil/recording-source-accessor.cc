#include "nix/util/recording-source-accessor.hh"
#include "nix/util/source-path.hh"

#include <tuple>

namespace nix {

void SourceReadRecorder::record(SourceRead read)
{
    reads.lock()->insert(std::move(read));
}

void SourceReadRecorder::record(SourceReadType type, SourceReadOutcome outcome, const SourcePath & path)
{
    auto sourcePath = path.path;
    std::optional<std::string> fingerprint;
    try {
        std::tie(sourcePath, fingerprint) = path.accessor->getFingerprint(path.path);
    } catch (...) {
    }
    record(
        SourceRead{
            .type = type,
            .outcome = outcome,
            .logicalPath = path.path,
            .sourcePath = std::move(sourcePath),
            .fingerprint = std::move(fingerprint),
        });
}

std::vector<SourceRead> SourceReadRecorder::get() const
{
    auto current = reads.readLock();
    return {current->begin(), current->end()};
}

struct RecordingSourceAccessor : SourceAccessor
{
    ref<SourceAccessor> next;
    std::shared_ptr<SourceReadRecorder> recorder;

    void anchor() override {}

    RecordingSourceAccessor(ref<SourceAccessor> next, std::shared_ptr<SourceReadRecorder> recorder)
        : next(std::move(next))
        , recorder(std::move(recorder))
    {
    }

    void record(SourceReadType type, SourceReadOutcome outcome, const CanonPath & path)
    {
        recorder->record(type, outcome, SourcePath(next, path));
    }

    void readFile(const CanonPath & path, Sink & sink, fun<void(uint64_t)> sizeCallback) override
    {
        try {
            next->readFile(path, sink, sizeCallback);
        } catch (...) {
            record(SourceReadType::File, SourceReadOutcome::Failed, path);
            throw;
        }
        record(SourceReadType::File, SourceReadOutcome::Present, path);
    }

    std::optional<Stat> maybeLstat(const CanonPath & path) override
    {
        std::optional<Stat> stat;
        try {
            stat = next->maybeLstat(path);
        } catch (...) {
            record(SourceReadType::Stat, SourceReadOutcome::Failed, path);
            throw;
        }
        record(SourceReadType::Stat, stat ? SourceReadOutcome::Present : SourceReadOutcome::Absent, path);
        return stat;
    }

    DirEntries readDirectory(const CanonPath & path) override
    {
        DirEntries entries;
        try {
            entries = next->readDirectory(path);
        } catch (...) {
            record(SourceReadType::Directory, SourceReadOutcome::Failed, path);
            throw;
        }
        record(SourceReadType::Directory, SourceReadOutcome::Present, path);
        return entries;
    }

    std::string readLink(const CanonPath & path) override
    {
        std::string target;
        try {
            target = next->readLink(path);
        } catch (...) {
            record(SourceReadType::Symlink, SourceReadOutcome::Failed, path);
            throw;
        }
        record(SourceReadType::Symlink, SourceReadOutcome::Present, path);
        return target;
    }

    std::optional<std::filesystem::path> getPhysicalPath(const CanonPath & path) override
    {
        return next->getPhysicalPath(path);
    }

    std::string showPath(const CanonPath & path) override
    {
        return next->showPath(path);
    }

    std::pair<CanonPath, std::optional<std::string>> getFingerprint(const CanonPath & path) override
    {
        return next->getFingerprint(path);
    }

    std::optional<time_t> getLastModified() override
    {
        return next->getLastModified();
    }

    void invalidateCache() override
    {
        next->invalidateCache();
    }
};

ref<SourceAccessor> makeRecordingSourceAccessor(ref<SourceAccessor> next, std::shared_ptr<SourceReadRecorder> recorder)
{
    return make_ref<RecordingSourceAccessor>(std::move(next), std::move(recorder));
}

} // namespace nix
