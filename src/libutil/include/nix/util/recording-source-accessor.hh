#pragma once

#include <memory>
#include <set>
#include <vector>

#include "nix/util/source-accessor.hh"
#include "nix/util/sync.hh"

namespace nix {

struct SourcePath;

enum class SourceReadType {
    RecursivePath,
    DerivedPath,
    Stat,
    File,
    Directory,
    Symlink,
};

enum class SourceReadOutcome {
    Present,
    Absent,
    Failed,
};

struct SourceRead
{
    SourceReadType type;
    SourceReadOutcome outcome;
    CanonPath logicalPath;
    CanonPath sourcePath;
    std::optional<std::string> fingerprint;

    auto operator<=>(const SourceRead &) const = default;
};

class SourceReadRecorder
{
    Sync<std::set<SourceRead>> reads;

public:
    void record(SourceRead read);
    void record(SourceReadType type, SourceReadOutcome outcome, const SourcePath & path);
    void recordDerivedPath(const CanonPath & logicalPath, const SourcePath & source);
    std::vector<SourceRead> get() const;
};

ref<SourceAccessor> makeRecordingSourceAccessor(ref<SourceAccessor> next, std::shared_ptr<SourceReadRecorder> recorder);

} // namespace nix
