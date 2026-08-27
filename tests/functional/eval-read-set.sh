#!/usr/bin/env bash

source common.sh

flake="$TEST_ROOT/eval-read-set-flake"
mkdir -p "$flake"

cat > "$flake/flake.nix" <<'EOF'
{
  outputs = { self }: {
    selected = import ./used.nix;
    ignored = import ./unused.nix;
  };
}
EOF
printf '42\n' > "$flake/used.nix"
printf 'abort "must stay lazy"\n' > "$flake/unused.nix"

first="$TEST_ROOT/read-set-first.json"
second="$TEST_ROOT/read-set-second.json"
filtered="$TEST_ROOT/read-set-filtered.json"

[[ $(nix eval --json "$flake#selected" --write-read-set "$first") == 42 ]]
jq -e '
  .schema_id == "nix-eval-read-set"
  and (.installable | endswith("#selected"))
  and (.locked_flake | contains("narHash="))
  and any(.entries[]; .access == "file" and .outcome == "present" and .source_path == "/flake.nix" and .fingerprint != null)
  and any(.entries[]; .access == "file" and .outcome == "present" and .source_path == "/used.nix" and .fingerprint != null)
  and all(.entries[]; .source_path != "/unused.nix")
' "$first" >/dev/null

[[ $(nix eval --json "$flake#selected" --write-read-set "$second") == 42 ]]
cmp "$first" "$second"

fingerprint=$(jq -r '.entries[] | select(.source_path == "/used.nix") | .fingerprint' "$first")
[[ $(nix eval --json "$flake#selected" --write-read-set "$filtered" \
  --read-set-fingerprint "$fingerprint") == 43 ]]
jq -e --arg fingerprint "$fingerprint" '
  .requested_fingerprints == [$fingerprint]
  and any(.entries[]; .source_path == "/used.nix" and .fingerprint == $fingerprint)
  and all(.entries[]; .fingerprint == null or .fingerprint == $fingerprint)
' "$filtered" >/dev/null

ambient="$TEST_ROOT/ambient-source"
printf 'ambient\n' > "$ambient"
ambientReadSet="$TEST_ROOT/read-set-ambient.json"
[[ $(nix eval --impure --raw --expr "builtins.readFile $ambient" --write-read-set "$ambientReadSet") == ambient ]]
jq -e 'any(.entries[]; .access == "file" and .fingerprint == null)' "$ambientReadSet" >/dev/null

expectStderr 1 nix eval --json "$flake#selected" --write-read-set "$first" \
  | grepQuiet "already exists"
expectStderr 1 nix eval --json "$flake#selected" \
  --read-set-fingerprint "$fingerprint" | grepQuiet "requires --write-read-set"
