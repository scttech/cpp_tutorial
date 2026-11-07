# `std::string` and `std::string_view`

## `std::string`

A `std::string` is a resizable sequence of **bytes** (`char`) that owns its memory, like a `vector<char>` with
text-friendly operations.

```cpp
std::string s = "hello";
s += ", world";           // append
s.size();                 // 12
s[0];                     // 'h'  (unchecked)
s.substr(7, 5);           // "world"  : a NEW string (a copy)
s.find("wor");            // 7, or std::string::npos if not found
s == "hello, world";      // true: compares contents
```

Short strings are stored inside the string object itself (the *small string optimization*, typically up to 15
characters); longer ones live on the heap. You do not need to care, except that copying a long string allocates.

### A string is bytes, not characters

`size()` counts **bytes**. For ASCII text that is the same as the number of characters, but text in UTF-8 uses 1 to 4
bytes per character:

```cpp
std::string cafe = "caf\xc3\xa9";   // "café": the é is the two bytes C3 A9
cafe.size();                         // 5, not 4
```

Every byte of a multi-byte character has its top bit set, so as a signed `char` it is **negative**. That matters for
`<cctype>`: functions such as `std::isspace(int)` are only defined for values of an `unsigned char` (or `EOF`).
Pass a plain `char` straight in and a negative value is undefined behavior. Cast first:

```cpp
std::isspace(static_cast<unsigned char>(c));
```

In UTF-8, continuation bytes always look like `10xxxxxx`. Every other byte starts a new character, which is how you
count characters without a library.

## `std::string_view`

A `string_view` is a **non-owning window** onto characters that live somewhere else: a pointer and a length. Passing
and slicing views never copies or allocates:

```cpp
void report(std::string_view text);       // accepts a std::string, a literal, a substring...
std::string_view v = s;                    // no copy
v.substr(7, 5);                            // another view into s: "world", still no copy
```

Prefer `std::string_view` for **read-only string parameters**, instead of `const std::string&`: it also accepts
string literals without building a temporary `std::string`.

### The dangling-view trap

A view does not keep its characters alive. It is only valid as long as the string it points into is:

```cpp
std::string_view bad() {
    std::string s = make_text();
    return s;                  // the view points into `s`, which is destroyed right here
}
std::string_view also_bad = std::string("temporary");   // the temporary dies at the semicolon
```

Rule of thumb: a `string_view` may *refer into* a string that outlives it (such as a parameter), but should never be
returned from a function that created the string. If a function makes new text, return a `std::string`.

## Your task

Implement these in `main.cpp`:

| Function | Behavior |
|---|---|
| `std::vector<std::string> tokenize(std::string_view)` | Split on ASCII whitespace; no empty tokens. |
| `std::map<std::string, int> word_frequencies(std::string_view)` | A word is a run of ASCII letters; everything else separates words. Lowercase the words. |
| `std::string_view trim(std::string_view)` | Remove leading and trailing whitespace, returning a **view** into the argument. |
| `std::string_view first_word(std::string_view)` | The first word, as a view into the argument. |
| `std::string join(const std::vector<std::string>&, std::string_view sep)` | Parts joined with `sep` between them. |
| `std::size_t utf8_length(std::string_view)` | The number of characters (not bytes) in valid UTF-8 text. |

**Fast tier:** `trim` and `first_word` must not allocate (the tests count allocations: a function that returns a view
cannot need any), and `join` must build its result with a single allocation.

Because `word_frequencies` counts only ASCII letters, `"café"` yields the word `caf`: the bytes of `é` are not letters.
That is a real limitation of byte strings and a good reminder of what `std::string` is.
