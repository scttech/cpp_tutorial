# FIX-Style Message Parser

> **You're an engineer at a brokerage.** Orders and executions travel between you and the exchange as **FIX** messages (Financial Information eXchange, the protocol of the securities industry). The parser sits in front of the order gateway and handles hundreds of thousands of messages a second; every unnecessary copy shows up on the latency graph. And because the input comes from the network, it must treat every byte as hostile.

## The format

A message is a sequence of **fields** `tag=value`, each ended by the separator **SOH** (ASCII 1, `kSoh`):

```
8=FIX.4.4 | 9=65 | 35=D | 49=ACME | 55=MSFT | 54=1 | 38=100 | 44=250.50 | 10=127 |
begin string, body length, ........ body fields ........................., checksum
```

- **Field 8** is first: the begin string (protocol version). **Field 9** second: the **body length**, the number of bytes from the start of the field after `9=...` up to (not including) the start of `10=`. **Field 10** is last: the **checksum**, the sum of *all bytes before the `10=`* (as unsigned bytes) modulo 256, written with exactly **three digits** (`007`).
- A tag is one or more digits (an `int` greater than 0 that fits; `0035` is tag 35). A value may be empty and may contain `=` (only the first `=` splits a field) but never SOH. Tags may repeat (repeating groups): keep all fields, in order.
- Anything else is an error, with the **offset** where it was detected (see the table in the starter's comment): `Empty`, `MissingDelimiter`, `BadTag`, `BadBeginString`, `BadBodyLength`, `BadChecksum`, `TrailingData`.

## Zero copy, and what it costs

`Field::value` is a **`std::string_view`** pointing into the text you were given: the parser allocates **nothing per field** (just the vector of fields), however large the values. The price is a **lifetime** rule: the view is valid only while the input text is alive and unchanged. Returning a `ParseResult` whose views point into a temporary `std::string` is the classic dangling-view bug; the lesson's tests keep the text alive, and the `ParseResult` documents the rule in its header.

## Numbers without exceptions

`std::stoi` throws on bad input, accepts `" 5"`, `"+5"`, `"5abc"`; the parser must reject all of those. `std::from_chars` parses exactly the digits you give it, never allocates, never throws, and reports where it stopped and why. Check the **whole** field was consumed.

## The tiers

- **Works**: the checksum function, a valid message, repeated tags, empty values and `=` inside values, views into the input, wrong checksum and body length, empty text, a missing separator.
- **Robust**: seven kinds of bad tag with their offsets, bad begin strings, bad lengths (including overflow and non-digits, multi-byte characters counted as bytes), the checksum field (two, four, no digits), trailing data, bytes above 127, an empty body, `|` as data, and 400 random one-bit corruptions of valid messages (at least 380 must be rejected).
- **Fast**: at most 10 allocations for a message with a 100 KB value; 40 for 5000 fields; 200000 messages in time; a 20 MB message in one pass.
- **Clean**: no `std::string`, `to_string` or throwing conversion in the parser; `std::string_view` and `std::from_chars`.

## Tips

`text.find(kSoh, at)` locates the end of the field; `field.find('=')` splits it. Parse in one loop first (collecting fields and their start offsets), then check the frame (first field 8, second 9, last 10) and compute the body length from the start offsets.
