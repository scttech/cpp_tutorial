# URLs and Percent-Encoding

> **You're an engineer on the networking team** writing a crawler, a proxy and a redirect handler. Every one of them must answer "what does this link mean?" the same way as every browser and every other server, because the places where two programs read the same URL differently are where **open redirects, SSRF and cache-poisoning bugs** live. RFC 3986 is precise about it; you will implement its core.

## Anatomy

`scheme://userinfo@host:port/path?query#fragment`. Only the scheme and the host are case-insensitive (lower-case them); the path, query and fragment are case-sensitive. A `?` or `#` with nothing after it is *present but empty* (`http://a/p?` has the query `""`), which is different from absent, so the parts are `std::optional`. A fragment may contain `?` (it is just text after the first `#`), so split at `#` **before** splitting at `?`. The userinfo ends at the **last** `@` of the authority. An IPv6 literal is bracketed and contains colons, so you cannot split the authority at the last colon without looking for `]` first.

## Percent-encoding

Characters outside the **unreserved** set (`A-Z a-z 0-9 - . _ ~`) travel as `%XX`, the byte in hex (use upper case; `%2f` and `%2F` are the same but the normal form is upper). Decoding must be strict: `%` followed by anything but two hex digits is an error (a lenient decoder that passes `%zz` through is how `%252e%252e` and `..%c0%af` tricks work), and decode **once**: `%2541` is the text `%41`, not `A`. `+` means a space **only in `application/x-www-form-urlencoded` query strings**, never in paths.

## Dot segments (section 5.2.4)

`/a/b/../c/./d` is `/a/c/d`, `..` cannot climb above the root, and empty segments stay (`/a//b`). The RFC gives a precise rewriting algorithm over an *input buffer* and an *output buffer*; implement it with a `string_view` for the input (never erase from the front of a string: that is quadratic) and a `std::string` as the output where "remove the last segment" is `erase(rfind('/'))`.

## Reference resolution (section 5.2)

A link inside a page is *relative to the page's URL*. The cases, in order: it has a scheme (use it as is); it starts with `//` (new authority, same scheme); its path is empty (`?y`, `#s`, or nothing: keep the base path, and keep the base **query** only if the reference has none); its path starts with `/` (replace the path); otherwise **merge**: the base path up to and including its last `/`, plus the reference path (`/` + reference if the base has an authority and an empty path). Remove dot segments from the result. The reference's own fragment is used; the base's fragment never is. The 33 examples of RFC 3986 section 5.4 are in the tests.

## Query strings

`a=1&b=%20x&c&a=2`: split at `&`, each pair at its **first** `=`, `+` is a space, escapes are decoded (and an invalid escape is kept as text, because browsers are lenient here), repeated keys keep their order, empty pieces are skipped.

## The tiers

- **Works**: decoding and encoding rules, 22 dot-segment cases, parsing and rejecting, normalisation, **all 40 RFC 3986 resolution examples**, other bases, query strings.
- **Robust**: random byte strings round-trip through encode and decode, normalisation is idempotent, resolution is consistent, 20000 garbage strings through every function.
- **Fast**: 300000 segments followed by 300000 `..` (quadratic code never finishes), 8 MB of percent-coding, a 3 MB URL.
- **Clean**: no raw memory or exceptions, no regex or number parsing library calls, `string_view` inputs.
