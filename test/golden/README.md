# Golden files

Real-world Lean source kept for **manual smoke testing**. These are
deliberately *not* wired into `tree-sitter test` — the corpus in `test/corpus/`
is the automated suite. These exist to answer "does the parser still cope with
actual Lean people write?", which a hand-written corpus tends not to capture.

## Running them

```bash
cc -O2 -fPIC -shared -I src src/parser.c src/scanner.c -o "$PWD/libtree-sitter-lean.so"
rm -f ~/.cache/tree-sitter/lib/lean.so
for f in test/golden/*.lean; do
  printf '%-16s %s\n' "$(basename "$f")" \
    "$(tree-sitter parse --lib-path "$PWD/libtree-sitter-lean.so" --lang-name lean "$f" \
       | grep -cE 'ERROR|MISSING')"
done
```

The `--lib-path` and cache removal are not optional: tree-sitter caches built
grammars at `~/.cache/tree-sitter/lib/<name>.so` keyed by grammar *name* only,
so without them you may be measuring a different worktree's build.

## Baseline as of 1.0.0

ERROR/MISSING node counts. **A rise is a regression; a fall is progress worth
noting in the changelog.** These are not all expected to be zero — several
exercise constructs with known open issues.

| file | nodes | notes |
| --- | --- | --- |
| `Chapter2.lean` | 0 | clean |
| `Chapter5.lean` | 0 | clean; this is the file that surfaced #48 |
| `Chapter7.lean` | 10 | |
| `Logic.lean` | 2 | |
| `Rule.lean` | 36 | heaviest user of unsupported notation |
| `Test.lean` | 8 | |

Nonzero counts trace mainly to the gaps listed under Known limitations in the
top-level README — unsupported Mathlib notation (#49) and the
`macro`/`macro_rules`/`elab`/mixfix declarations (#50) most of all.

## Provenance

Recorded because these are fixtures, not functionality, and some are derived
from other people's projects.

- **`Chapter2.lean`, `Chapter5.lean`, `Chapter7.lean`** — Trevor Murphy's own
  formalisation work following Rudin's *Principles of Mathematical Analysis*.
  Original work by this repository's maintainer.
- **`Logic.lean`** — generic example code written for this repository.
- **`Rule.lean`, `Test.lean`** — these `import Heron.*` and `import
  LeanPrism.*` and appear to originate from those unrelated third-party
  projects rather than having been written here. They predate the current
  maintainer and were already tracked at the repository root; they are kept as
  parser input only. If their provenance turns out to be a problem, they can be
  replaced with equivalent hand-written fixtures without affecting anything
  else — nothing depends on their specific contents.
