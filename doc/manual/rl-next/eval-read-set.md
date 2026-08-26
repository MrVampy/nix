---
synopsis: `nix eval` can record source read sets
significance: significant
---

`nix eval --write-read-set PATH` writes a versionless JSON document containing
the source filesystem operations performed while forcing the requested output.
Entries bind the locked flake, source fingerprint, source-relative path,
logical path, access kind, and outcome. Reads without a source fingerprint are
reported explicitly so consumers can reject or handle impure inputs.

Read-set recording disables the flake evaluation cache for that command. This
ensures a cached value cannot produce an incomplete read set.
