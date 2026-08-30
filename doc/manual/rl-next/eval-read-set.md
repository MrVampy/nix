---
synopsis: `nix eval` can record source read sets
significance: significant
---

`nix eval --write-read-set PATH` writes a versionless JSON document containing
the source filesystem operations performed while forcing the requested output.
Entries bind the locked flake, source fingerprint, source-relative path,
logical path, access kind, and outcome. Reads without a source fingerprint are
reported explicitly so consumers can reject or handle impure inputs.
Source paths coerced into store inputs produce a distinct recursive-path entry,
including on a source-to-store cache hit, so the document retains the complete
content dependency rather than only the shallow reads used to evaluate it. The
document declares this coverage with `recursive_path_dependencies`.

Repeated `--read-set-fingerprint FINGERPRINT` arguments restrict the document
to reads attributed to those sources plus every unfingerprinted read. The
document records the requested fingerprints. Filtering cannot hide an ambient
or otherwise unattributed source read.

Read-set recording disables the flake evaluation cache for that command. This
ensures a cached value cannot produce an incomplete read set.
