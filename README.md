# Tree-Sitter-Lean4

Lean4 is a programming language, commonly used for mathematical theorem proving as a [proof assistant](https://en.wikipedia.org/wiki/Proof_assistant).

This project contains a Lean parser definition:

- Tree-Sitter grammar for parsing [Lean 4](github.com/leanprover/lean4) source code.
- Tree-Sitter queries for usage in the modal text editor [Helix](https://helix-editor.vercel.app/).

**Important**: Lean is a very extensible language. Therefore, the Tree-Sitter grammar is of limited use. For parsing advanced Lean programs you will need to use the Lean kernel. See also [Metaprogramming in Lean](https://github.com/leanprover-community/lean4-metaprogramming-book).

## Usage

### Nix Flake

This is the recommended approach for reproducible builds.

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    tree-sitter-lean.url = "github:tmurph/tree-sitter-lean";
  };

  outputs = { nixpkgs, tree-sitter-lean, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        # If you want to use ast-grep (see next section)
        packages = [ pkgs.ast-grep ];
        shellHook = ''
          # Create a symlink in the working directory for easy access (add to .gitignore)
          ln -sf ${tree-sitter-lean.packages.${system}.grammar}/parser tree-sitter-lean.so
        '';
      };
    };
}
```

See [`nixpkgs` tree-sitter documentation](https://nixos.org/manual/nixpkgs/stable/#tree-sitter) for more Tree-Sitter with Nix examples.

Push build cache to public cache server:

```bash
nix build . --print-out-paths | cachix push <your-cache-name>
```

## Development

Read [writing the grammar](https://tree-sitter.github.io/tree-sitter/creating-parsers/3-writing-the-grammar.html). Be careful of adding conflicts in the grammar as they cause exponential growth of the state space.

Use the `tree-sitter` CLI:

```bash
tree-sitter generate # Re-run this after every grammar rule change
tree-sitter parse Test.lean # A long Lean file that needs to be parsed correctly
```

## Tests

Add tests for the Tree-Sitter grammar to [./test/corpus](./test/corpus).

Tests are run with

```bash
tree-sitter test
```

Check the amount of failures with:

```bash
tree-sitter test --overview-only | grep ✗ | lines | length
```

Tests can be updated automatically when you change the grammar:

```bash
tree-sitter test --update
```

See [writing tests](https://tree-sitter.github.io/tree-sitter/creating-parsers/5-writing-tests.html)

You should also test the queries from `./queries`:

```bash
tree-sitter query higlights.scm Test.lean
```

This will report any errors such as incorrect node names in the query file.

## License

GPL-3.0-or-later; see [LICENSE](LICENSE).

Portions derive from MIT-licensed work by Julian Berman and Willem Vanhulle,
whose notice is retained in [NOTICE](NOTICE) as that license requires. The
combined work's redistribution terms are the GPL's.

## History

Forked from [wvhulle/tree-sitter-lean](https://github.com/wvhulle/tree-sitter-lean),
which derives from [Julian/tree-sitter-lean](https://github.com/Julian/tree-sitter-lean)
(Julian Berman, 2021). The grammar and external scanner have since been
substantially rewritten — the scanner is a full layout/indentation
implementation with no code remaining from the original, and the grammar has
been reorganised around parser-generation cost.

Earlier revisions of this README credited `Julian/lean.nvim` as the origin.
That is incorrect: `lean.nvim` is a Neovim plugin by the same author that
*consumes* a tree-sitter grammar and has never contained one. The actual
lineage is `Julian/tree-sitter-lean`.

## Known limitations

These are real, reproducible parse bugs with open issues. They are documented
here rather than only in the tracker because several produce a **wrong tree
with no ERROR node**, so nothing signals to a consumer that anything is amiss.

- A comment between an `if` body and its `else` silently breaks `else`
  parsing — `else` is lexed as an identifier applied to the else-body
  ([#65](https://github.com/tmurph/tree-sitter-lean/issues/65)).
- A dedented fragment mid-edit can escape into the module-level fallback and
  absorb arbitrary following text
  ([#48](https://github.com/tmurph/tree-sitter-lean/issues/48),
  fix tracked in [#60](https://github.com/tmurph/tree-sitter-lean/issues/60)).
- A layout block closed by end-of-file does not extend through trailing blank
  lines, unlike one closed by a dedent
  ([#63](https://github.com/tmurph/tree-sitter-lean/issues/63)).
- A blank line *between* two tactics or two do-statements belongs to neither
  ([#64](https://github.com/tmurph/tree-sitter-lean/issues/64)).
- Some Mathlib notation is unsupported and parses as an error: `√`, `∫`/`∂`,
  `∀ᶠ`/`∃ᶠ ... in ...`, `𝟙`
  ([#49](https://github.com/tmurph/tree-sitter-lean/issues/49)).
- `macro`, `macro_rules`, `elab` and mixfix declarations fall through a
  generic `notation` rule rather than having dedicated ones
  ([#50](https://github.com/tmurph/tree-sitter-lean/issues/50)).

## TODO

Nothing outstanding for the 1.0 release beyond the open issues listed under
Known limitations.
