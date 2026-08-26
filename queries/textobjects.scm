; textobjects.scm — Helix textobject queries for tree-sitter-lean
;
; Enables `maf` (select around function), `mif` (inside), `mat` (around type),
; `maa` (around argument), `mac` (around comment), etc.

; ── Functions / theorems ─────────────────────────────────────────────────────
; `definition` covers def / theorem / lemma / abbrev / instance.
; The `decorated_declaration` wrapper additionally includes any leading
; doc comment, attributes, and modifiers so `maf` selects the whole thing.

(decorated_declaration
  declaration: (_) @function.inside) @function.around

(definition body: (_) @function.inside) @function.around

(example body: (_) @function.inside) @function.around

; Lambdas (fun / λ)
(fun body: (_) @function.inside) @function.around

; Where declarations
(where_decl body: (_) @function.inside) @function.around

; Initialize blocks (run at module load — function-like)
(initialize value: (_) @function.inside) @function.around

; Try/catch — handler bodies are function-like for textobject purposes
(try
  body: (_) @function.inside) @function.around

(try
  handler: (_) @function.inside) @function.around

; ── Types (structures / inductives) ──────────────────────────────────────────

(structure
  (structure_field) @class.inside) @class.around

(inductive
  (constructor) @class.inside) @class.around

; ── Parameters / binders ─────────────────────────────────────────────────────
; Lean binders are space-separated, not comma-separated:
;   (x : Nat) (y : Nat) {z : Prop} [inst : Decidable p]

(explicit_binder
  name: (identifier) @parameter.inside) @parameter.around

(implicit_binder
  name: (identifier) @parameter.inside) @parameter.around

(instance_binder
  name: (identifier) @parameter.inside) @parameter.around

; ── Comments ─────────────────────────────────────────────────────────────────

(comment) @comment.inside

(comment)+ @comment.around

; ── Entries (items in a list-like context) ───────────────────────────────────
; Useful for navigating constructors, match arms, calc steps, tactic subgoals.

(match_arm
  body: (_) @entry.inside) @entry.around

(constructor) @entry.around

(structure_field
  type: (_) @entry.inside) @entry.around

(calc_first_step) @entry.around

(calc_step) @entry.around

; Focused tactic subgoals `· tactic` and case splits `case h => tactic`
(tactic_focus) @entry.around

(tactic_case) @entry.around

; Do-block elements as entries — supports `]m`/`[m` movement between statements
(do_let) @entry.around

(let_bind) @entry.around

(let_mut) @entry.around

(do_return) @entry.around

(do_dbg_trace) @entry.around

(do_unless
  body: (_) @entry.inside) @entry.around

(do_while
  body: (_) @entry.inside) @entry.around

(do_if
  then: (_) @entry.inside) @entry.around

(do_if_let
  then: (_) @entry.inside) @entry.around

(do_match_arm
  body: (_) @entry.inside) @entry.around

; Field assignments inside structure instances
(field_assignment) @entry.around

; Imports (textobject for line-level navigation)
(import) @entry.around

(set_option) @entry.around
