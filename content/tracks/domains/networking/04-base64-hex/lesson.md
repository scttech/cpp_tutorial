# Base64 and Hex Codecs

> **You're an engineer on the networking team.** Binary data (a hash, a key, an image, a signature) has to travel through places that only accept text: JSON bodies, HTTP headers, URLs, e-mail, config files. Base64 and hex are the two encodings you will write or debug every month. They look trivial; the production bugs are all in the *edges*: padding, URL-safe alphabets, whitespace, and decoders that accept more than one text for the same bytes (a classic way to slip past a signature or a filter).

## Base64 in one paragraph

Take 3 bytes (24 bits), split them into four 6-bit groups, write each as one of 64 characters. If the input is not a multiple of 3, the last group is short: 1 leftover byte gives 2 characters, 2 bytes give 3, and the text is padded with `=` up to a multiple of 4 (or not padded at all in the "unpadded" variants used by JWT and URLs). The standard alphabet ends in `+/`; the **URL-safe** one uses `-_` because `+`, `/` and `=` mean things in URLs.

## Strict decoding

A decoder that is lenient is a decoder with several valid texts per message. Refuse:

- characters outside the alphabet (including the *other* alphabet's `-_`/`+/`, whitespace unless opted in, and bytes above 127);
- wrong **padding**: count it exactly (`Zg==`, `Zm8=`), and never accept data after a `=`;
- a final group with **one** character (6 bits cannot make a byte);
- **non-canonical** trailing bits: in `Zg==` the last character carries 2 bits of data and 4 bits that must be zero; `Zh==` would decode to the same byte `f`, so two different texts would mean the same thing. RFC 4648 section 3.5 calls refusing this the strict mode.

## Safe in both directions

`encode_to`/`decode_to` write into a caller's buffer and must **never write a byte past it**: compute the exact size first (`encoded_size`), or check the capacity before every store. The tests surround each buffer with guard bytes, and test capacities of `need - 1`, `need`, and `need + 1`. `max_decoded_size` is an upper bound so a caller can allocate before decoding.

## Speed: a table, not a search

For encoding, index a 64-character string with 6-bit values. For decoding, a 256-entry table `value[byte]` (0xFF for "invalid") indexed by the unsigned char, **built at compile time** with a `constexpr` function. Searching the alphabet (`find`) for each character is 64 times slower. The 48 MB test is sized to show the difference.

## Hex

Two digits per byte, upper or lower case on output, either case on input, odd lengths and anything else rejected.

## The tiers

- **Works**: the RFC test vectors, unpadded forms, the alphabets, `encoded_size` and `max_decoded_size`, 40 invalid inputs by category, whitespace policy, buffer APIs, hex.
- **Robust**: every length 0 to 300 round-trips in both alphabets, encoder output is canonical, **no two texts decode to the same bytes** (every single-character substitution is tested), guard bytes around every buffer, 20000 garbage strings.
- **Fast**: zero allocations in the buffer APIs, one allocation per call otherwise, 48 MB in both directions.
- **Clean**: no raw memory or exceptions; no `find`/`strchr` in the codec; no streams or number-formatting helpers.
