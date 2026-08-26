; indents.scm — Helix indentation queries for tree-sitter-lean
;
; Lean uses layout-based parsing (virtual semicolons/blocks via the external
; scanner), so most structural indentation is already encoded in the tree.
; These rules handle bracket-based indentation and a few common patterns.

; ── Indent scopes (entering these nodes increases indent) ────────────────────

[
  ; Bracket-based
  (parenthesized)
  (tuple)
  (anonymous_constructor)
  (structure_instance)
  (array)
  (list)
  (tactic_config)
  (syntax_quotation)

  ; Block-forming expressions
  (by)
  (do)
  (match)
  (fun)
  (quantifier)
  (for_in)
  (if)
  (if_let)
  (let)
  (have)
  (try)
  (calc)

  ; Do-block elements
  (do_unless)
  (do_while)
  (do_match)
  (do_if)
  (do_if_let)

  ; Declarations
  (definition)
  (decorated_declaration)
  (structure)
  (inductive)
  (class_inductive)
  (constant)
  (axiom)
  (opaque)
  (initialize)
  (syntax)
  (notation)
  (attribute)
  (namespace)
  (section)
  (where_decl)

  ; Tactic blocks
  (tactic_focus)
  (tactic_case)
  (tactic_calc)
] @indent

; ── Outdent tokens (these tokens cancel the current indent level) ─────────────

[
  ")"
  "]"
  "}"
  "⟩"
  "end"
] @outdent
