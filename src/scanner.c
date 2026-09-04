/**
 * External scanner for Lean 4 tree-sitter grammar.
 * Handles layout/indentation-sensitive parsing.
 *
 * Lean uses indentation to delimit blocks after `do`, `where`, `:=`, `=>`.
 * The scanner maintains a stack of indent levels and emits:
 *   LAYOUT_START     — pushed when grammar enters a layout context
 *   LAYOUT_SEMICOLON — same-indent newline within a layout block
 *   LAYOUT_END       — indent decreased below current layout level
 *   MATCH_BODY_START — like LAYOUT_START but dedicated to match arm bodies
 *
 * MATCH_BODY_START is a separate token so match_arm can open a layout
 * block without creating grammar conflicts with _do_element.  It pushes
 * the same indent stack as LAYOUT_START and is closed by LAYOUT_END.
 *
 * Key design: a `queued_indent` field persists across scanner calls so that
 * a single newline can produce multiple LAYOUT_END tokens (one per popped
 * level) followed by a LAYOUT_SEMICOLON, across successive scanner invocations.
 *
 * Special case: `|` at the start of a line does NOT get a LAYOUT_SEMICOLON.
 * This matches Lean 4's parser where match arms are delimited by `|` tokens
 * rather than by the indentation-based semicolon mechanism.
 */

#include "tree_sitter/parser.h"
#include "tree_sitter/alloc.h"
#include <string.h>

enum TokenType {
  LAYOUT_START,
  LAYOUT_SEMICOLON,
  LAYOUT_END,
  MATCH_BODY_START,
  SYNTAX_QUOTATION_BODY,  // content inside `` `( ... ) `` up to matching `)`
  BRACE_FIELD_SEP,        // newline-as-separator inside `{ … }` struct instance
  BY_CASES_NAME,          // identifier immediately (mod spaces) followed by `:`
  TACTIC_COMMA,           // `,` between `tactic_use` arguments (see step 6)
  CALC_LAYOUT_START,      // like LAYOUT_START, but for `calc`'s step chain (see KIND_CALC_PENDING)
  ELSE_LAYOUT_START,      // like LAYOUT_START, but for do_if's `else` — see #47
};

#define MAX_DEPTH 64
#define NO_QUEUED UINT32_MAX

/* Why a layout level was opened — see #1. KIND_CALC_PENDING: a
   tactic_calc level whose anchor is still provisional — see #40.
   KIND_ELSE_CHAIN: a same-line `else if` transparent level, whose
   indent duplicates its enclosing frame's — see #47. */
enum LayoutKind { KIND_LAYOUT = 0, KIND_MATCH_BODY = 1, KIND_CALC_PENDING = 2, KIND_ELSE_CHAIN = 3 };

typedef struct {
  uint32_t indents[MAX_DEPTH];
  uint8_t  kinds[MAX_DEPTH];
  uint8_t  depth;
  uint32_t queued_indent; // indent of next non-blank line, or NO_QUEUED
} Scanner;

/* ── lifecycle ─────────────────────────────────────────────────── */

void *tree_sitter_lean_external_scanner_create(void) {
  Scanner *s = ts_calloc(1, sizeof(Scanner));
  s->depth = 0;
  s->queued_indent = NO_QUEUED;
  return s;
}

void tree_sitter_lean_external_scanner_destroy(void *payload) {
  ts_free(payload);
}

unsigned tree_sitter_lean_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *s = (Scanner *)payload;
  unsigned pos = 0;
  memcpy(buffer + pos, &s->depth, sizeof(s->depth));       pos += sizeof(s->depth);
  memcpy(buffer + pos, &s->queued_indent, sizeof(s->queued_indent)); pos += sizeof(s->queued_indent);
  unsigned indent_bytes = s->depth * sizeof(uint32_t);
  memcpy(buffer + pos, s->indents, indent_bytes);           pos += indent_bytes;
  memcpy(buffer + pos, s->kinds, s->depth);                 pos += s->depth;
  return pos;
}

void tree_sitter_lean_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *s = (Scanner *)payload;
  if (length == 0) {
    s->depth = 0;
    s->queued_indent = NO_QUEUED;
    return;
  }
  unsigned pos = 0;
  memcpy(&s->depth, buffer + pos, sizeof(s->depth));        pos += sizeof(s->depth);
  memcpy(&s->queued_indent, buffer + pos, sizeof(s->queued_indent)); pos += sizeof(s->queued_indent);
  if (s->depth > MAX_DEPTH) { s->depth = 0; s->queued_indent = NO_QUEUED; return; }
  memcpy(s->indents, buffer + pos, s->depth * sizeof(uint32_t));
  pos += s->depth * sizeof(uint32_t);
  if (pos + s->depth <= length) memcpy(s->kinds, buffer + pos, s->depth);
  else memset(s->kinds, KIND_LAYOUT, s->depth);
}

/* ── helpers ───────────────────────────────────────────────────── */

static inline uint32_t top_indent(Scanner *s) {
  return s->depth > 0 ? s->indents[s->depth - 1] : 0;
}

/* The layout column just below the top — what we'd see after one pop. */
static inline uint32_t penultimate_indent(Scanner *s) {
  return s->depth > 1 ? s->indents[s->depth - 2] : 0;
}

static inline void push(Scanner *s, uint32_t indent, uint8_t kind) {
  if (s->depth < MAX_DEPTH) {
    s->kinds[s->depth] = kind;
    s->indents[s->depth++] = indent;
  }
}

/* Kind of the innermost open layout level. */
static inline uint8_t top_kind(Scanner *s) {
  return s->depth > 0 ? s->kinds[s->depth - 1] : KIND_LAYOUT;
}

static inline void pop(Scanner *s) {
  if (s->depth > 0) s->depth--;
}

static void skip_spaces(TSLexer *lexer) {
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t')
    lexer->advance(lexer, true);
}

static bool is_nl(int32_t c) { return c == '\n' || c == '\r'; }

/* Reserved words that can only ever open a brand-new top-level command,
   never continue an expression/tactic/mid-construct position — see #21. */
static const char *const COMMAND_KEYWORDS[] = {
  "def", "theorem", "lemma", "abbrev",
  "instance", "structure", "class", "inductive",
  "namespace", "section", "end",
  "open", "export", "variable",
  "universe", "universes",
  "syntax", "set_option", "include", "omit",
  "constant", "opaque", "axiom", "example",
  "attribute", "initialize", "builtin_initialize",
  "notation", "macro_rules", "macro", "elab",
  "prefix", "infix", "infixl", "infixr", "postfix",
};
#define NUM_COMMAND_KEYWORDS (sizeof(COMMAND_KEYWORDS) / sizeof(COMMAND_KEYWORDS[0]))

/* Character classes mirroring grammar.js's `identifier` regex (see #38
   for the derivation from real Lean's isLetterLike) — kept in sync by
   hand, used by BY_CASES_NAME and peek_command_keyword. Any mismatch
   just falls back to a plain `identifier` token — safe, not
   silently-wrong. */
static bool is_ident_start(int32_t c) {
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') return true;
  if (c >= 0x03B1 && c <= 0x03BA) return true; // lower greek, but λ (0x3BB)
  if (c >= 0x03BC && c <= 0x03C9) return true;
  if (c >= 0x0391 && c <= 0x039F) return true; // upper greek, but Π (0x3A0) and Σ (0x3A3)
  if (c >= 0x03A1 && c <= 0x03A2) return true;
  if (c >= 0x03A4 && c <= 0x03A9) return true;
  if (c >= 0x03CA && c <= 0x03FB) return true; // Coptic letters
  if (c >= 0x1F00 && c <= 0x1FFE) return true; // Polytonic Greek Extended
  if (c >= 0x2100 && c <= 0x214F) return true; // Letter-like Symbols (incl. ℕℤℚℝℂℵℓ℘)
  if (c >= 0x1D49C && c <= 0x1D59F) return true; // Script/Double-struck/Fraktur letters
  if (c >= 0x00C0 && c <= 0x00D6) return true; // Latin-1 supplement letters, but × (0xD7)
  if (c >= 0x00D8 && c <= 0x00F6) return true;
  if (c >= 0x00F8 && c <= 0x00FF) return true; // ..., but ÷ (0xF7)
  if (c >= 0x0100 && c <= 0x017F) return true; // Latin Extended-A
  if (c == 0x2207) return true; // ∇
  return false;
}

static bool is_ident_continue(int32_t c) {
  if (is_ident_start(c)) return true;
  if (c >= '0' && c <= '9') return true;
  if (c == '\'' || c == '?' || c == '!') return true;
  // Narrow subscript ranges only — the wider 0x2070-0x209F block also has
  // ⁻ (0x207B), the `inverse` token's superscript minus — see #38.
  if (c >= 0x2080 && c <= 0x2089) return true; // ₀-₉
  if (c >= 0x2090 && c <= 0x209C) return true; // ₐ-ₜ
  if (c >= 0x1D62 && c <= 0x1D6A) return true; // ᵢ-ᵪ
  if (c == 0x2C7C) return true; // ⱼ
  return false;
}

/* Peeks whether the lookahead is a COMMAND_KEYWORDS entry at a word
   boundary, or a hash_command (`#eval`, `#check`, ...). Pure lookahead
   (never mark_end()) — see #21. */
static bool peek_command_keyword(TSLexer *lexer) {
  if (lexer->lookahead == '#') {
    lexer->advance(lexer, false);
    int32_t c = lexer->lookahead;
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }
  if (!is_ident_start(lexer->lookahead)) return false;
  char buf[24];
  uint32_t len = 0;
  int32_t ch = lexer->lookahead;
  while (is_ident_continue(ch) && len < sizeof(buf) - 1) {
    buf[len++] = (ch >= 0 && ch < 128) ? (char)ch : '\x7f';
    lexer->advance(lexer, false);
    ch = lexer->lookahead;
  }
  for (size_t i = 0; i < NUM_COMMAND_KEYWORDS; i++) {
    size_t kwlen = strlen(COMMAND_KEYWORDS[i]);
    if (len == kwlen && memcmp(buf, COMMAND_KEYWORDS[i], len) == 0) return true;
  }
  return false;
}

/**
 * Skip newlines + leading whitespace, return column of first non-blank char.
 * If EOF is reached, return 0 (dedent everything).
 * After this call, lexer->lookahead is the first non-blank character.
 */
static uint32_t measure_indent(TSLexer *lexer) {
  while (is_nl(lexer->lookahead))
    lexer->advance(lexer, true);
  skip_spaces(lexer);
  if (lexer->eof(lexer)) return 0;
  return lexer->get_column(lexer);
}

/**
 * In Lean 4, match arms are delimited by `|` without semicolons.
 * A `|` at the start of a line should also not trigger LAYOUT_END,
 * because match arms may appear at a column lower than the enclosing
 * binding's layout (e.g. `def f := match x with | ...` puts arms
 * below the `match` column).
 */
static bool starts_with_pipe(TSLexer *lexer) {
  return lexer->lookahead == '|';
}

/* Word-boundary peek for `else` — see #44. */
static bool starts_with_else(TSLexer *lexer) {
  static const char kw[] = "else";
  for (size_t i = 0; i < 4; i++) {
    if (lexer->lookahead != kw[i]) return false;
    lexer->advance(lexer, false);
  }
  return !is_ident_continue(lexer->lookahead);
}

static bool should_suppress_semicolon(TSLexer *lexer) {
  return starts_with_pipe(lexer) || starts_with_else(lexer);
}

/* Word-boundary peek for `if` — used to detect a same-line `else if`
   chain — see #47. */
static bool starts_with_if(TSLexer *lexer) {
  static const char kw[] = "if";
  for (size_t i = 0; i < 2; i++) {
    if (lexer->lookahead != kw[i]) return false;
    lexer->advance(lexer, false);
  }
  return !is_ident_continue(lexer->lookahead);
}

/**
 * Push indent for a layout-start-like token (shared by LAYOUT_START
 * and MATCH_BODY_START).
 */
static void push_layout_indent(Scanner *s, TSLexer *lexer, uint8_t kind) {
  skip_spaces(lexer);
  if (is_nl(lexer->lookahead)) {
    uint32_t indent = measure_indent(lexer);
    push(s, indent, kind);
  } else {
    push(s, lexer->get_column(lexer), kind);
  }
  s->queued_indent = NO_QUEUED;
}

/* Push indent for `_else_layout_start`. A same-line `else if`/`else if let`
   is a chain, not a new indented block: it reuses the enclosing indent
   (transparent level) instead of the `if`'s own column, so the chained
   if's own `else` dedents back out at the right level — see #47. */
static void push_else_layout_indent(Scanner *s, TSLexer *lexer) {
  skip_spaces(lexer);
  if (is_nl(lexer->lookahead)) {
    uint32_t indent = measure_indent(lexer);
    push(s, indent, KIND_LAYOUT);
  } else {
    uint32_t col = lexer->get_column(lexer);
    lexer->mark_end(lexer);
    if (starts_with_if(lexer)) {
      push(s, top_indent(s), KIND_ELSE_CHAIN);
    } else {
      push(s, col, KIND_LAYOUT);
    }
  }
  s->queued_indent = NO_QUEUED;
}

/* Push indent for `_calc_layout_start` — see #40. Tags the level
   KIND_CALC_PENDING when the first step shares `calc`'s own line (anchor
   is only a guess then); otherwise an ordinary KIND_LAYOUT push. */
static void push_calc_layout_indent(Scanner *s, TSLexer *lexer) {
  skip_spaces(lexer);
  if (is_nl(lexer->lookahead)) {
    uint32_t indent = measure_indent(lexer);
    push(s, indent, KIND_LAYOUT);
  } else {
    push(s, lexer->get_column(lexer), KIND_CALC_PENDING);
  }
  s->queued_indent = NO_QUEUED;
}

/* Attempt to resolve a KIND_CALC_PENDING level's provisional anchor — see
   #40. Called from both dedent-handling sites when the innermost level is
   still pending. Fires only if the candidate column is deeper than the
   enclosing level, the line starts with `_` (the calc-step placeholder),
   and LAYOUT_SEMICOLON is valid; one-shot (flips to plain KIND_LAYOUT). */
static bool try_finalize_calc_anchor(Scanner *s, TSLexer *lexer,
                                      uint32_t candidate,
                                      const bool *valid_symbols) {
  if (s->depth == 0 || top_kind(s) != KIND_CALC_PENDING) return false;
  if (candidate <= penultimate_indent(s)) return false;
  if (lexer->lookahead != '_') return false;
  if (!valid_symbols[LAYOUT_SEMICOLON]) return false;
  s->indents[s->depth - 1] = candidate;
  s->kinds[s->depth - 1] = KIND_LAYOUT;
  s->queued_indent = NO_QUEUED;
  lexer->result_symbol = LAYOUT_SEMICOLON;
  return true;
}

/* ── main scan ─────────────────────────────────────────────────── */

bool tree_sitter_lean_external_scanner_scan(
    void *payload, TSLexer *lexer, const bool *valid_symbols) {

  Scanner *s = (Scanner *)payload;

  /* 0. Error recovery: if all layout symbols are valid simultaneously
        the parser is in error recovery mode — don't interfere. */
  if (valid_symbols[LAYOUT_START] &&
      valid_symbols[LAYOUT_SEMICOLON] &&
      valid_symbols[LAYOUT_END]) {
    return false;
  }

  /* 0b. BY_CASES_NAME — disambiguates by_cases's optional name from its
         bare condition, see #14. */
  if (valid_symbols[BY_CASES_NAME]) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') lexer->advance(lexer, true);
    if (is_ident_start(lexer->lookahead)) {
      lexer->advance(lexer, false);
      while (is_ident_continue(lexer->lookahead)) lexer->advance(lexer, false);
      // mark_end before peeking further — a skip-mode advance() after
      // mark_end retroactively collapses the boundary to zero-width.
      lexer->mark_end(lexer);
      while (lexer->lookahead == ' ' || lexer->lookahead == '\t') lexer->advance(lexer, false);
      if (lexer->lookahead == ':') {
        lexer->advance(lexer, false);
        if (lexer->lookahead != '=') {
          lexer->result_symbol = BY_CASES_NAME;
          return true;
        }
      }
      return false;
    }
  }

  /* 0c. TACTIC_COMMA — a `,` between comma-separated tactic arguments
         where everything after the first item is optional (`tactic_use`'s
         witness list, `tactic_induction`'s target list), always preferred
         over LAYOUT_END where valid — see #29, #43. */
  if (valid_symbols[TACTIC_COMMA] && lexer->lookahead == ',') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = TACTIC_COMMA;
    return true;
  }

  /* 1y. BRACE_FIELD_SEP — inside `{ … }`, a newline acts as a field
         separator. Emit only when the parser asks for it (between two
         field assignments). The structure-instance rule places this in the
         separator position so a missing comma is acceptable when newlines
         intervene. */
  if (valid_symbols[BRACE_FIELD_SEP]) {
    skip_spaces(lexer);
    if (is_nl(lexer->lookahead)) {
      lexer->mark_end(lexer);
      while (is_nl(lexer->lookahead)) {
        lexer->advance(lexer, true);
        skip_spaces(lexer);
      }
      // Don't consume up to `}` — let the parser see it as end-of-body.
      if (lexer->lookahead != '}' && !lexer->eof(lexer)) {
        lexer->result_symbol = BRACE_FIELD_SEP;
        return true;
      }
    }
  }

  /* 1z. SYNTAX_QUOTATION_BODY — the parser has just consumed `` `( `` and
         expects the body of a syntax quotation. Greedily read until the
         matching `)`, tracking nested parens, brackets, and braces.
         Stops just before the closing `)` so the grammar can consume it. */
  if (valid_symbols[SYNTAX_QUOTATION_BODY]) {
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    bool consumed_any = false;
    while (!lexer->eof(lexer)) {
      int32_t c = lexer->lookahead;
      if (c == ')' && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
        break;
      }
      if (c == '(') paren_depth++;
      else if (c == ')') { if (paren_depth > 0) paren_depth--; }
      else if (c == '[') bracket_depth++;
      else if (c == ']') { if (bracket_depth > 0) bracket_depth--; }
      else if (c == '{') brace_depth++;
      else if (c == '}') { if (brace_depth > 0) brace_depth--; }
      lexer->advance(lexer, false);
      consumed_any = true;
    }
    if (consumed_any) {
      lexer->mark_end(lexer);
      lexer->result_symbol = SYNTAX_QUOTATION_BODY;
      return true;
    }
    return false;
  }

  /* 1a. LAYOUT_START — grammar just saw `do`, `where`, `:=`, `=>`
         and wants to open a new layout block. */
  if (valid_symbols[LAYOUT_START]) {
    push_layout_indent(s, lexer, KIND_LAYOUT);
    lexer->result_symbol = LAYOUT_START;
    return true;
  }

  /* 1b. MATCH_BODY_START — grammar just saw `=>` in a match arm.
         Pushes indent identically to LAYOUT_START, but uses a
         distinct token so the parser can distinguish match arm
         context from general layout (avoiding conflicts with
         _do_element).  Closed by the same LAYOUT_END mechanism. */
  if (valid_symbols[MATCH_BODY_START]) {
    push_layout_indent(s, lexer, KIND_MATCH_BODY);
    lexer->result_symbol = MATCH_BODY_START;
    return true;
  }

  /* 1b2. ELSE_LAYOUT_START — see #47. */
  if (valid_symbols[ELSE_LAYOUT_START]) {
    push_else_layout_indent(s, lexer);
    lexer->result_symbol = ELSE_LAYOUT_START;
    return true;
  }

  /* 1c. CALC_LAYOUT_START — see #40. */
  if (valid_symbols[CALC_LAYOUT_START]) {
    push_calc_layout_indent(s, lexer);
    lexer->result_symbol = CALC_LAYOUT_START;
    return true;
  }

  /* 2. Process queued indent from a previous newline.
        Each call pops at most one layout level (LAYOUT_END) or emits
        LAYOUT_SEMICOLON, then returns so tree-sitter can re-enter. */
  if (s->queued_indent != NO_QUEUED && s->depth > 0) {
    uint32_t qi = s->queued_indent;
    uint32_t ci = top_indent(s);

    if (qi < ci && valid_symbols[LAYOUT_END]) {
      // Top-level match arms below the body's indent (`def f := match X with
      // | ...`): when `:=` is the only layout open and the next token is `|`,
      // the arms belong to the match. Closing the layout would terminate the
      // match prematurely. Deeper stacks pop normally so nested matches still
      // work — there an inner match needs LAYOUT_END to fire so the outer
      // match's arm at a lower column can fire.
      if (s->depth == 1 && top_kind(s) == KIND_LAYOUT) {
        skip_spaces(lexer);
        while (is_nl(lexer->lookahead)) {
          lexer->advance(lexer, true);
          skip_spaces(lexer);
        }
        // mark_end after the walk, not before — see #59.
        lexer->mark_end(lexer);
        if (starts_with_pipe(lexer)) {
          s->queued_indent = NO_QUEUED;
          return false;
        }
      } else if (top_kind(s) == KIND_CALC_PENDING) {
        // Peek past blank lines for try_finalize_calc_anchor — see #40.
        skip_spaces(lexer);
        while (is_nl(lexer->lookahead)) {
          lexer->advance(lexer, true);
          skip_spaces(lexer);
        }
        // mark_end after the walk, not before — see #59.
        lexer->mark_end(lexer);
        if (try_finalize_calc_anchor(s, lexer, qi, valid_symbols)) return true;
      }
      pop(s);
      lexer->result_symbol = LAYOUT_END;
      return true;
    }
    if (qi == ci && (valid_symbols[LAYOUT_SEMICOLON] || valid_symbols[LAYOUT_END])) {
      // Peek ahead past newlines+whitespace to see the actual next token.
      // Don't emit semicolon before `|` — match arms are delimited
      // by `|` tokens, not by layout semicolons.
      skip_spaces(lexer);
      while (is_nl(lexer->lookahead)) {
        lexer->advance(lexer, true);
        skip_spaces(lexer);
      }
      // Flush-left continuation, cascade step (second+ nested level) — see #21.
      lexer->mark_end(lexer);
      // Suppress-check must run before peek_command_keyword() — see #44.
      bool suppress = should_suppress_semicolon(lexer);
      // A transparent else-chain level never grows its own _do_seq past
      // the one nested if it was pushed for — close it instead of
      // extending it, and let the frame below decide — see #47.
      if (!suppress && top_kind(s) == KIND_ELSE_CHAIN && valid_symbols[LAYOUT_END]) {
        pop(s);
        lexer->result_symbol = LAYOUT_END;
        return true;
      }
      if (!suppress && valid_symbols[LAYOUT_END] && peek_command_keyword(lexer)) {
        pop(s);
        lexer->result_symbol = LAYOUT_END;
        return true;
      }
      if (valid_symbols[LAYOUT_SEMICOLON]) {
        if (suppress) {
          s->queued_indent = NO_QUEUED;
          return false;
        }
        s->queued_indent = NO_QUEUED;
        lexer->result_symbol = LAYOUT_SEMICOLON;
        return true;
      }
    }
    s->queued_indent = NO_QUEUED;
  }

  /* 3. Skip horizontal whitespace before inspecting the character. */
  skip_spaces(lexer);

  /* 4. Newline — measure indent of next line and start processing. */
  if (is_nl(lexer->lookahead) && s->depth > 0) {
    // Early mark_end covers the true-EOF case below, whose token must stay
    // at content-end — see #59 (extending it flips $.return/$.do_return-style
    // declared GLR conflicts that key off trailing-newline consumption).
    lexer->mark_end(lexer);
    uint32_t next = measure_indent(lexer);
    uint32_t ci   = top_indent(s);

    // True EOF, not just a coincidentally-column-0 next line — see #21.
    if (lexer->eof(lexer) && valid_symbols[LAYOUT_END]) {
      pop(s);
      lexer->result_symbol = LAYOUT_END;
      return true;
    }

    // Not EOF: re-mark at the post-walk position, before any further
    // peeking (peek_command_keyword() etc. below advance the lexer too,
    // and must not push the boundary past the dedent point) — see #59.
    lexer->mark_end(lexer);

    if (next < ci && valid_symbols[LAYOUT_END]) {
      if (try_finalize_calc_anchor(s, lexer, next, valid_symbols)) return true;
      // Mirror of the depth==1 / `|` guard in step 2.
      if (s->depth == 1 && top_kind(s) == KIND_LAYOUT && starts_with_pipe(lexer)) {
        return false;
      }
      pop(s);
      s->queued_indent = next;
      lexer->result_symbol = LAYOUT_END;
      return true;
    }
    if (next == ci) {
      // Flush-left continuation (body at the same column as its opening
      // line, so the dedent above never fires) — see #21. Suppress-check
      // must run before peek_command_keyword() — see #44.
      bool suppress = should_suppress_semicolon(lexer);
      // See the matching check in the queued-indent branch above — #47.
      if (!suppress && top_kind(s) == KIND_ELSE_CHAIN && valid_symbols[LAYOUT_END]) {
        pop(s);
        s->queued_indent = next;
        lexer->result_symbol = LAYOUT_END;
        return true;
      }
      if (!suppress && valid_symbols[LAYOUT_END] && peek_command_keyword(lexer)) {
        pop(s);
        s->queued_indent = next;
        lexer->result_symbol = LAYOUT_END;
        return true;
      }
      if (valid_symbols[LAYOUT_SEMICOLON]) {
        // Suppress semicolon before `|` or `else`
        if (suppress) {
          s->queued_indent = NO_QUEUED;
          return false;
        }
        s->queued_indent = NO_QUEUED;
        lexer->result_symbol = LAYOUT_SEMICOLON;
        return true;
      }
    }
    return false;
  }

  /* 5. EOF while in layout — emit LAYOUT_END to close all open blocks. */
  if (lexer->eof(lexer) && s->depth > 0 && valid_symbols[LAYOUT_END]) {
    pop(s);
    lexer->result_symbol = LAYOUT_END;
    return true;
  }

  /* 6. Same-line terminator — force-close one layout level. See #1. */
  if (valid_symbols[LAYOUT_END] && s->depth > 0) {
    int32_t c = lexer->lookahead;
    if (c == '|') {
      // Mirror of the depth==1 / `|` guard in steps 2 and 4 — see #20.
      if (s->depth == 1 && top_kind(s) == KIND_LAYOUT) {
        return false;
      }
      pop(s);
      lexer->result_symbol = LAYOUT_END;
      return true;
    }
    if (c == ')' || c == ']' || c == '}' || c == ',') {
      pop(s);
      lexer->result_symbol = LAYOUT_END;
      return true;
    }
    if (c == ':') {
      // Don't fire on the leading `:` of `:=`/`::` — see #19.
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      if (lexer->lookahead != '=' && lexer->lookahead != ':') {
        pop(s);
        lexer->result_symbol = LAYOUT_END;
        return true;
      }
      return false;
    }
    if (is_ident_start(c) || c == '#') {
      // Flush-left continuation, same-line variant — see #21.
      lexer->mark_end(lexer);
      if (peek_command_keyword(lexer)) {
        pop(s);
        lexer->result_symbol = LAYOUT_END;
        return true;
      }
      return false;
    }
  }

  return false;
}
