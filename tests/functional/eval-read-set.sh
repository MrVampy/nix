#!/usr/bin/env bash

source common.sh

flake="$TEST_ROOT/eval-read-set-flake"
mkdir -p "$flake/directory"

cat > "$flake/flake.nix" <<'EOF'
{
  outputs = { self }: {
    selected =
      import ./used.nix
      + builtins.length (builtins.attrNames (builtins.readDir ./directory))
      + (if builtins.pathExists ./missing then 100 else 0);
    ignored = import ./unused.nix;
  };
}
EOF
printf '42\n' > "$flake/used.nix"
printf 'abort "must stay lazy"\n' > "$flake/unused.nix"
printf 'directory member\n' > "$flake/directory/child"

first="$TEST_ROOT/read-set-first.json"
second="$TEST_ROOT/read-set-second.json"

[[ $(nix eval --json "$flake#selected" --write-read-set "$first") == 43 ]]
jq -e '
  .schema_id == "nix-eval-read-set"
  and (.installable | endswith("#selected"))
  and (.locked_flake | contains("narHash="))
  and .pure_eval
  and any(.entries[]; .access == "file" and .outcome == "present" and .source_path == "/flake.nix" and .fingerprint != null)
  and any(.entries[]; .access == "file" and .outcome == "present" and .source_path == "/used.nix" and .fingerprint != null)
  and any(.entries[]; .access == "directory" and .outcome == "present" and .source_path == "/directory" and .fingerprint != null)
  and any(.entries[]; .access == "stat" and .outcome == "absent" and .source_path == "/missing" and .fingerprint != null)
  and all(.entries[]; .source_path != "/unused.nix")
' "$first" >/dev/null

[[ $(nix eval --json "$flake#selected" --write-read-set "$second") == 43 ]]
cmp "$first" "$second"

ambient="$TEST_ROOT/ambient-source"
printf 'ambient\n' > "$ambient"
ambientReadSet="$TEST_ROOT/read-set-ambient.json"
[[ $(nix eval --impure --raw --expr "builtins.readFile $ambient" --write-read-set "$ambientReadSet") == ambient ]]
jq -e '
  (.pure_eval | not)
  and any(.entries[]; .access == "file" and .fingerprint == null)
' "$ambientReadSet" >/dev/null

expectStderr 1 nix eval --json "$flake#selected" --write-read-set "$first" \
  | grepQuiet "already exists"
