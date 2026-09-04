# Changelog

All notable changes to this project are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-09-04

### Added

- `open`/`close` fields on every bracketed expression form, so a closing
  delimiter can be located structurally instead of by matching token text (#54).
- A field on each element of a tactic sequence, so "is this an element of a
  tactic sequence" is one query rather than one per enclosing construct (#55).
- Ten new supertypes, up from four: `_atom`, `_do_element`, `_pattern`,
  `_command`, `_bracketed_binder` (#53) and `_binding`, `_do_binding`, `_if`,
  `_do_if`, `_bracketed` (#58). Downstream consumers can name a category in a
  query instead of maintaining their own list of member node types.
- Trevor Murphy to the author lists.
- `LICENSE` and `NOTICE` files. The project had declared a license in its
  metadata since `9dbb7c3` while shipping no license text at all — that commit
  removed the `COPYING` file inherited from the original at the same time it
  added the metadata field.
- `test/golden/`, real-world Lean files kept for manual smoke testing, with a
  recorded baseline of parse-error counts. Not wired into `tree-sitter test`.

### Changed

- **Breaking:** the combined work is now licensed **GPL-3.0-or-later**,
  where the metadata previously declared MIT. Portions derive from
  MIT-licensed work by Julian Berman and Willem Vanhulle; that notice is
  retained in `NOTICE` as the MIT license requires, and those portions remain
  available under MIT from their original sources. This does not relicense
  their contributions.
- **Breaking:** `tactic_apply`'s fields renamed from `tactic`/`arg` to
  `name`/`arguments`, matching `application` (#57). Consumers reading the old
  names will silently match nothing rather than erroring.
- Layout blocks now end at the dedenting token rather than at their last
  content token, so a blank line inside a block belongs to that block instead
  of to no node (#59). Blocks closed by end-of-file are not yet covered (#63).
- `binders`, `parameters` and `where_decl`'s inline binder list unified onto a
  single `binders` node, including `instance`, which previously produced a
  differently-shaped `binders` field on the same `definition` node type (#56).

### Fixed

- Chained `else if` inside a `do` block no longer silently drops the final
  `else` branch into curried-application arguments, and no longer absorbs a
  following sibling statement (#47). Both failure modes produced a wrong tree
  with no ERROR node.
- A blank line between two `calc` steps now belongs to the preceding step
  rather than to neither (#62).

### Removed

- The Rust crate and crates.io packaging (`Cargo.toml`, `Cargo.lock`,
  `build.rs`, `src/lib.rs`). The published `tree-sitter-lean4` crate belongs
  to the upstream author; this fork does not ship one. Consume the grammar via
  the shared library or the tree-sitter CLI.

### Notes

Parser generation: `STATE_COUNT` 9909 → 10026, peak RSS 3.67 → 3.71 GiB,
generation time unchanged at roughly 34s. Corpus 297/315 → 311/329 with an
unchanged set of 18 known failures.

See the Known limitations section of the README for parse bugs that are open
and documented rather than fixed.
