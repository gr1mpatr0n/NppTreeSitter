; highlights.scm — Rust
; This is a minimal example.  For production use, copy the full highlights.scm
; from https://github.com/tree-sitter/tree-sitter-rust/blob/master/queries/highlights.scm

; ---- Keywords ----
[
  "as" "async" "await" "break" "const" "continue" "crate" "dyn" "else"
  "enum" "extern" "fn" "for" "if" "impl" "in" "let" "loop" "macro_rules!"
  "match" "mod" "move" "mut" "pub" "ref" "return" "self" "Self" "static"
  "struct" "super" "trait" "type" "union" "unsafe" "use" "where" "while"
  "yield"
] @keyword

"fn" @keyword.function
"return" @keyword.return

; ---- Literals ----
(string_literal) @string
(raw_string_literal) @string
(char_literal) @string
(boolean_literal) @constant.builtin
(integer_literal) @number
(float_literal) @float

; ---- Comments ----
(line_comment) @comment
(block_comment) @comment

; ---- Types ----
(type_identifier) @type
(primitive_type) @type.builtin
(self) @variable.builtin

; ---- Functions ----
(function_item name: (identifier) @function)
(call_expression function: (identifier) @function.call)
(call_expression function: (field_expression field: (field_identifier) @method.call))

; ---- Variables / fields ----
(identifier) @variable
(field_identifier) @field
(parameter (identifier) @variable.parameter)

; ---- Operators & punctuation ----
["+" "-" "*" "/" "%" "=" "==" "!=" "<" ">" "<=" ">=" "&&" "||" "!" "&" "|" "^" "~" "<<" ">>"] @operator
["(" ")" "[" "]" "{" "}"] @punctuation.bracket
["," ";" "::" ":" "." ".."] @punctuation.delimiter

; ---- Attributes ----
(attribute_item) @attribute
