; highlights.scm — for tree-sitter-lean (wvhulle fork)

; ── Variables ────────────────────────────────────────────────

(identifier) @variable

; Capitalize → type by convention
((identifier) @type
 (#match? @type "^[A-Z]"))

; ── Modifiers ──────────────────────────────────────────────────

[
  "noncomputable"
  "partial"
  "protected"
  "private"
  "unsafe"
] @keyword.storage.modifier

; ── Declarations ─────────────────────────────────────────────

(definition
  name: (identifier) @function)

(structure
  name: (identifier) @type)

(inductive
  name: (identifier) @type)

(class_inductive
  name: (identifier) @type)

(opaque
  name: (identifier) @function)

(constructor
  name: (identifier) @constructor)

(constant
  name: (identifier) @constant)

(axiom
  name: (identifier) @function)

(structure_field
  name: (identifier) @variable.other.member)

; ── Attributes ──────────────────────────────────────────────────

; `@[` is a single token now — see #1.
(attributes
  "@[" @punctuation.special
  "]" @punctuation.special)

(attribute_entry
  name: (identifier) @attribute)

(attribute_entry
  arg: (identifier) @attribute)

; ── Namespaces & imports ─────────────────────────────────────

(namespace
  name: (identifier) @namespace)

(open
  namespace: (identifier) @namespace)

(section
  name: (identifier) @namespace)

(import
  module: (_) @namespace)

; ── Explicit (@) ───────────────────────────────────────────────

(explicit
  "@" @operator)

; ── Patterns ─────────────────────────────────────────────────

(constructor_pattern
  constructor: (identifier) @constructor)

; ── Projections ──────────────────────────────────────────────

(projection
  name: (identifier) @variable.other.member)

(projection
  name: (number) @variable.other.member)

; ── Binders ──────────────────────────────────────────────────

(explicit_binder
  name: (identifier) @variable.parameter)

(implicit_binder
  name: (identifier) @variable.parameter)

(instance_binder
  name: (identifier) @variable.parameter)

(fun
  binders: (identifier) @variable.parameter)

(quantifier
  binders: (identifier) @variable.parameter)

(for_in
  var: (identifier) @variable.parameter)

; ── Let / have / mut / bind ──────────────────────────────────

(let
  name: (identifier) @variable)

(do_let
  pattern: (identifier) @variable)

(have
  name: (identifier) @variable)

(let_mut
  name: (identifier) @variable)

(let_bind
  name: (identifier) @variable)

(reassign
  name: (identifier) @variable)

; ── Field assignment ─────────────────────────────────────────

(field_assignment
  name: (identifier) @variable.other.member)

; ── Application ─────────────────────────────────────────────

(application
  name: (identifier) @function.call)

; ── Tactics ──────────────────────────────────────────────────

(tactic_apply
  name: (identifier) @function.builtin)

(tactic_have
  name: (identifier) @variable)

(tactic_let
  name: (identifier) @variable)

(tactic_case
  "case" @keyword)

(tactic_case
  "next" @keyword)

(tactic_rewrite
  "rw" @function.builtin)

(tactic_rewrite
  "rewrite" @function.builtin)

; See #3.
(tactic_sorry) @keyword.control.exception

(tactic_done) @function.builtin

(hash_command) @keyword.directive

; ── Keywords ─────────────────────────────────────────────────

; Function/definition-introducing keywords
[
  "def"
  "theorem"
  "lemma"
  "abbrev"
  "instance"
  "example"
] @keyword.function

; Type/storage-defining keywords
[
  "structure"
  "inductive"
  "class"
  "constant"
  "axiom"
  "opaque"
] @keyword.storage.type

[
  "deriving"
  "section"
  "namespace"
  "end"
  "attribute"
  "extends"
] @keyword

(prelude) @keyword
(module_header) @keyword

[
  "open"
  "import"
  "export"
  "scoped"
  "hiding"
  "meta"
  "all"
  "include"
  "omit"
] @keyword.control.import

[
  "public"
] @keyword.storage.modifier

[
  "variable"
  "universe"
  "universes"
  "set_option"
  "initialize"
  "builtin_initialize"
] @keyword.storage

[
  "let"
  "mut"
  "have"
  "where"
] @keyword

[
  "fun"
  "λ"
] @keyword.function

[
  "forall"
  "∀"
  "exists"
  "∃"
] @keyword

[
  "if"
  "then"
  "else"
] @keyword.control.conditional

[
  "for"
  "in"
  "while"
  "do"
  "unless"
] @keyword.control.repeat

(do_break) @keyword.control.repeat
(do_continue) @keyword.control.repeat

[
  "match"
  "with"
] @keyword.control.conditional

[
  "by"
  "show"
  "calc"
  "obtain"
  "suffices"
] @keyword

; Metaprogramming keywords
[
  "notation"
  "macro_rules"
  "syntax"
  "macro"
  "elab"
  "prefix"
  "infix"
  "infixl"
  "infixr"
  "postfix"
] @function.macro

"return" @keyword.control.return

; Debug
[
  "dbg_trace"
] @function.builtin

; Exception-like keywords
[
  "try"
  "catch"
] @keyword.control.exception

; ── Interpolated strings ─────────────────────────────────────

(interpolated_string
  prefix: _ @string.special)

(interpolated_string
  (string) @string)

; ── Set-option / pragmas ─────────────────────────────────────

(set_option
  name: (identifier) @variable.builtin)

; ── As-patterns ──────────────────────────────────────────────

(as_pattern
  binder: (identifier) @variable
  "@" @operator)

; ── Initialize ───────────────────────────────────────────────

(initialize
  name: (identifier) @variable)

; ── Include / Omit ───────────────────────────────────────────

(include
  name: (identifier) @variable)

(omit
  name: (identifier) @variable)

; ── Syntax declarations & quotations ─────────────────────────

(syntax
  attr: (identifier) @function.macro)

(syntax
  category: (identifier) @type)

(syntax_quotation
  "`" @punctuation.special
  "(" @punctuation.special
  ")" @punctuation.special)

(sorry) @keyword.control.exception

; ── Operators ────────────────────────────────────────────────

; `"operator"` is the alias every binary operator carries — see #1, #32
; (most levels no longer have individual node types after the #32 fold).
[
  "operator"
  "!"  "¬"
  "+"  "-"  "*"
  "::"
  "<|"  "$"
  "<;>"  "<;"
  "->"  "→"
  "<-"  "←"
  ":="
  "=>"
] @operator

; ── Literals ─────────────────────────────────────────────────

(number) @constant.numeric.integer
(float) @constant.numeric.float
(string) @string
(char) @constant.character
(hole) @variable.builtin
(synthetic_hole) @variable.builtin
[(true) (false)] @constant.builtin.boolean

; ── String internals ─────────────────────────────────────────

(escape_sequence) @constant.character.escape

(interpolation
  "{" @punctuation.special
  "}" @punctuation.special)

; ── Comments ─────────────────────────────────────────────────

; Doc comments are now their own grammar node (attached to declarations).
(doc_comment) @comment.block.documentation

(comment) @comment

; ── Punctuation ──────────────────────────────────────────────

["(" ")" "[" "]" "{" "}" "⟨" "⟩" "#["] @punctuation.bracket
["|" "," "." ":" ";" "//"] @punctuation.delimiter
