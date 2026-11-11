# Tiny Shell Tokenizer

> **You're an engineer on a systems team** writing an admin console: operators type command lines such as
> `grep -R "TODO: fix" src/ | sort -u > 'my todos.txt' && echo "done: $USER"`
> and the console must split them into words and operators exactly as a shell would. Get it wrong and `rm "my file"` deletes two files that do not exist and misses the one that does. Before you can parse, run or even log a command, you need a **tokenizer** (a lexer): the part that turns characters into tokens.

## One pass, a few states

A tokenizer is a small state machine: walk the line once, left to right, keeping **a word under construction** and the offset where it started. Each character belongs to one of a handful of classes, and each class has one branch:

| Character | What happens |
|---|---|
| space / tab | the current word (if any) ends |
| `\|` `&` `;` `<` `>` | the current word ends; an operator token is produced (longest match: `&&` `\|\|` `>>`) |
| `\` | the next character is literal; the backslash itself is dropped |
| `'` | copy everything up to the next `'` literally |
| `"` | copy up to the closing `"`; only `\"` `\\` `\$` are escapes; `$NAME` is expanded |
| `$` | expand `$NAME` / `${NAME}` |
| `#` at the start of a word | a comment: stop |
| anything else | append to the current word |

## The rules that trip people up

- **Quotes glue pieces together.** `a"b c"d'e f'g` is *one* word: `ab cde fg`. Quotes are not delimiters, they switch the meaning of the characters inside.
- **An empty string is a word.** `echo "" ''` has three words, not one. But `a""b` is `ab`. So "is there a word" is a separate piece of state from "is the word empty".
- **Single quotes are absolute.** No escape works inside them, not even `\'`.
- **Inside double quotes a backslash is selective.** `"\n"` stays backslash-n; only `"`, `\` and `$` can be escaped.
- **Operators do not need spaces:** `a>>b` is three tokens, and `>>>` is `>>` then `>`.
- **`#` starts a comment only at the start of a word:** `echo a#b` has the word `a#b`.
- **Expansion substitutes characters, nothing more.** The value is not split into words and not re-read for quotes or operators. An empty *unquoted* expansion disappears (`echo $EMPTY x` has two words); a quoted one leaves an empty word.
- **Say where it went wrong.** An unterminated quote is reported at the *opening* quote, so a user can see which one. Tokens completed before the error are returned; the half-built word is dropped.

## The tiers

- **Works**: every rule above, positions, all five error kinds with positions.
- **Robust**: 5000 random lines are *written* with every kind of quoting by an independent quoter and must be read back exactly (text, kind and start position); 20000 garbage lines must always terminate with errors inside the line and a result that survives being quoted again; every prefix of a tricky line is run with poison characters sitting in memory right after the view (a tokenizer that peeks one character too far is caught); a 100000-token line.
- **Fast**: a 6 MB line with 1.3 million tokens inside the timeout, a 5 MB quoted word, no copy of the line per token (allocations are counted), empty lines allocate nothing.
- **Clean**: no raw memory or exceptions, a hand-written scanner (no regex, no streams, no `strtok`), and `string_view` for the input.

## Tips

Write small helpers (`is_name_start`, `expand`) so the main loop reads like the table above. Use `line.find('\'', i + 1)` for single quotes instead of a char-by-char loop. Never read `line[i + 1]` without checking `i + 1 < n`.
