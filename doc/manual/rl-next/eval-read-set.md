---
synopsis: `nix eval` can record source read sets
significance: significant
---

`nix eval --write-read-set PATH` writes a versionless JSON document containing
the source filesystem operations performed while forcing the requested output.
Entries bind the locked flake, pure-evaluation mode, source fingerprint,
source-relative path, logical path, access kind, and outcome. Unfingerprinted
content reads are reported explicitly so consumers can reject or handle impure
inputs. Directory reads retain the directory dependency, and absent `stat`
results retain the negative dependency.

Read-set recording disables the flake evaluation cache for that command. This
ensures a cached value cannot produce an incomplete read set.
