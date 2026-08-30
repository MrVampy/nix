R""(

# Examples

* Evaluate a Nix expression given on the command line:

  ```console
  # nix eval --expr '1 + 2'
  ```

* Evaluate a Nix expression to JSON:

  ```console
  # nix eval --json --expr '{ x = 1; }'
  {"x":1}
  ```

* Evaluate a Nix expression from a file:

  ```console
  # nix eval --file ./my-nixpkgs hello.name
  ```

* Get the current version of the `nixpkgs` flake:

  ```console
  # nix eval --raw nixpkgs#lib.version
  ```

* Print the store path of the Hello package:

  ```console
  # nix eval --raw nixpkgs#hello
  ```

* Get a list of checks in the `nix` flake:

  ```console
  # nix eval nix#checks.x86_64-linux --apply builtins.attrNames
  ```

* Record the source paths read while forcing one output:

  ```console
  # nix eval --json .#checks.x86_64-linux.package --write-read-set ./read-set.json
  ```

  The versionless `nix-eval-read-set` document records each source
  locked flake, source fingerprint, path relative to that source, logical
  path, access kind, and outcome. An entry without a fingerprint identifies
  an impure source that a consumer must handle explicitly. Read-set recording
  disables the flake evaluation cache so cached values cannot omit their
  original reads. Coercing a source path into a store input records a separate
  recursive-path access that binds the complete path content. The document's
  `recursive_path_dependencies` field declares that this operation is covered.

  Repeat `--read-set-fingerprint` to retain only reads attributed to those
  source fingerprints plus every unfingerprinted read. This bounds a consumer's
  projection without hiding access to an ambient or otherwise unattributed
  source.

* Generate a directory with the specified contents:

  ```console
  # nix eval --write-to ./out --expr '{ foo = "bar"; subdir.bla = "123"; }'
  # cat ./out/foo
  bar
  # cat ./out/subdir/bla
  123

# Description

This command evaluates the given Nix expression, and prints the result on standard output.

It also evaluates any nested attribute values and list items.

# Output format

`nix eval` can produce output in several formats:

* By default, the evaluation result is printed as a Nix expression.

* With `--json`, the evaluation result is printed in JSON format. Note
  that this fails if the result contains values that are not
  representable as JSON, such as functions.

* With `--raw`, the evaluation result must be a string, which is
  printed verbatim, without any quoting.

* With `--write-to` *path*, the evaluation result must be a string or
  a nested attribute set whose leaf values are strings. These strings
  are written to files named *path*/*attrpath*. *path* must not
  already exist.

)""
