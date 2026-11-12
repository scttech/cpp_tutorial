# Incremental HTTP/1.1 Request Parser

> **You're an engineer on the networking team** writing the front door of a server. Bytes arrive from a socket in arbitrary pieces: half a request line, a header split in two, three pipelined requests glued together. Every byte comes from a stranger, and a parser that is lenient about the wrong thing becomes the entry point of **request smuggling**, header injection and denial of service. You will write a strict, incremental, bounded parser for the request side of HTTP/1.1 (RFC 9112).

## Incremental: `feed()` returns how much it used

`feed(data)` consumes what belongs to *this* request and returns `{state, consumed}`:

- `NeedMore`: everything was consumed; call again when more bytes arrive.
- `Complete`: the request ended; `consumed` stops **exactly at its last byte**, so the caller can `reset()` and feed the rest (a pipelined request).
- `Error`: `error()` says why; the parser stays in the error state and consumes nothing more.

The parser must give the **same answer for any split** of the same bytes (one at a time, in the middle of `\r\n`, in the middle of a chunk size). The classic mistake is to search the whole accumulated buffer for `\r\n` every time data arrives: correct but **quadratic** (a client that sends 1 byte per packet makes you do 1 + 2 + 3 + ... bytes of work). Only the *new* bytes need scanning.

## Strict: each rule closes a real attack

| Rule | Why |
|---|---|
| Lines end with **CRLF**; a lone LF or a stray CR is an error | Proxies and servers that disagree about line endings can be made to see different requests |
| No space before the colon (`Host : a`) | A front-end that ignores the header and a back-end that honours it |
| No obsolete line folding | Same: one side joins the lines, the other rejects or splits them |
| Method is `A-Z`, target starts with `/` or is `*`, version is exactly `HTTP/1.0`/`1.1` | Stops malformed requests from being interpreted differently downstream |
| `Content-Length` is digits only, fits in `size_t`, duplicates must agree | `+5`, `-1`, `5x`, `0x10` and overflow wrap-around have all been used to desynchronise a server from its proxy |
| `Content-Length` **and** `Transfer-Encoding` together is an error | The textbook smuggling setup (CL.TE / TE.CL): two components pick different framing |
| `Transfer-Encoding` must be exactly `chunked` (any case) | `gzip, chunked`, two TE headers, and `identity` are all ambiguities |

## Bounded: limits are applied as bytes arrive

Each line is limited to `max_line`, the header count to `max_headers`, the body to `max_body` (for chunked bodies: the *sum* of the chunk sizes). A client that sends 400 MB of `a` with no CRLF must be rejected after about `max_line` bytes without buffering anything: check the length **while appending**, not after the line is complete.

## Chunked bodies

`hex-size [;extension] CRLF data CRLF`, repeated, ending with a `0` chunk, optional trailer lines and a final blank line. The size is 1 to 16 hex digits (either case; no sign, no `0x`), extensions are ignored, trailers are validated like headers and dropped (and count towards the header limit). Chunk data is *exactly* `size` bytes followed by *exactly* `CRLF`.

## Small things that are easy to get wrong

- Header names are compared **case-insensitively** (`content-length` equals `Content-Length`) but stored as received; values are trimmed of spaces and tabs.
- `keep_alive()` reads `Connection` as a **comma-separated token list**: the token `closed` is not `close`, `keep-alives` is not `keep-alive`. HTTP/1.0 closes unless asked otherwise, HTTP/1.1 stays open unless asked otherwise, and `close` always wins.
- Do not use `stoul`/`strtoul`/`sscanf` for `Content-Length`: they accept leading spaces, `+`, `-`, trailing garbage and `0x` prefixes. Parse the digits yourself and check for overflow before multiplying.

## The tiers

- **Works**: 16 valid requests, **42 invalid ones with the exact error code**, truncation, header lookup, the Connection token list, pipelining with `reset()`, finality of `Complete`/`Error`, all three limits and the trailer limit.
- **Robust**: every valid request at **every two-way split and byte by byte**, every invalid request in five different delivery patterns, 50 random re-slicings of a pipelined stream, 5000 garbage inputs, body sizes around 255/256/4095/4096/65535/65536 in both framings.
- **Fast**: a 1 MB body and a 256 KB header line delivered **one byte at a time** (quadratic code never finishes), 400 MB of hostile input rejected after under 16 KB, 5000 parses of a request with 100 headers.
- **Clean**: no raw memory or exceptions, no regex or number-parsing library functions, `string_view` input.
