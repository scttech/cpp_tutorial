# A CSV Reader and Writer (RFC 4180)

> **You're on the data engineering team.** Every pipeline starts with a CSV file someone exported from a spreadsheet, a database or a partner's system. "It's just commas" is how a customer name like `Smith, J.` shifts every column one place to the right, how a note with a line break turns one record into two, and how a file saved by Excel starts with three invisible bytes that become part of the first column's name. You will write the reader and the writer so they agree with each other, and so that when the file is broken they say *where*.

## The format in six rules

1. Records end at a line break (`\n` or `\r\n`); the last one needs no terminator, and a terminator at the very end is not the start of another record.
2. A field that starts with `"` is **quoted**. Inside quotes, commas and line breaks are data, and `""` is a single quote.
3. An unquoted field may not contain a `"`. A quote must be followed (after its closing) by a delimiter or a record end.
4. A blank line is a record with **one empty field** (spreadsheets write these; `skip_empty_lines` drops them). `a,` has two fields, `,` has two, and empty text has none.
5. A UTF-8 **byte order mark** at the start of the text is not data.
6. A lone `\r` that is not followed by `\n` is data.

The reader is a small state machine: *start of field -> in unquoted field / in quoted field -> after closing quote -> delimiter or end of record*. Write it as a loop over characters; the tempting `find(',')` / `split` approach cannot handle rule 2, which is why most hand-rolled CSV parsers corrupt data.

## Errors that carry a place

A parser that says "invalid CSV" is a parser nobody can use on a 2 GB file. `Error` carries a **kind**, the physical **line** (counting the line breaks *inside* quoted fields, because that is what an editor shows), and the **record** number; and the records read before the error are still returned. For an unterminated quote the useful line is the one where the quote **opened**, not the end of the file.

## The writer must undo the reader

The writer quotes a field exactly when it must: it contains the delimiter, a quote, `\r` or `\n`. Two cases that people miss:

- **A record whose only field is empty** must be written `""`, not as nothing: an empty line reads back as one empty field *in a different state* when it is the last line, and spreadsheets drop it.
- **A first field that starts with the BOM bytes** must be quoted, or the reader will swallow them.

The property to keep in mind: `parse(write(table)) == table` for every table. The Robust tier tests it on 1500 random tables made of the nastiest characters.

## Reading without allocating

`Reader::next(std::vector<std::string>& fields)` reuses the vector and the strings inside it: `clear()` keeps a string's capacity, so after the first few records a file of same-width records is read with no allocation at all. A `parse()` that builds the whole table is a convenience on top.

## The tiers

- **Works**: all six rules, empty fields and lines, BOM, other delimiters, errors with kind/line/record, `rectangular`, the writer's quoting.
- **Robust**: random round trips (any delimiter, both line endings, BOM-first fields), garbage input, the incremental reader, width changes, 1 MB fields, line numbers across quoted breaks.
- **Fast**: 200000 records read with at most a handful of allocations; the writer does not allocate per field; a 30 MB round trip.
- **Clean**: no raw memory or exceptions; no regex, string streams, `getline`, `strtok` or `sscanf`.
