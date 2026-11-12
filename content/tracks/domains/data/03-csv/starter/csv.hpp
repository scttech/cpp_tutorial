#pragma once
// YOUR CSV READER AND WRITER (RFC 4180, plus the real-world rules every CSV library ends up with).
//
// csv::Options { char delimiter = ','; bool rectangular = false; bool skip_empty_lines = false; bool crlf = true; }
//   delimiter         must not be '"', '\r' or '\n' (the reader then reports ErrorKind::BadOptions with line 0 and returns no records)
//   rectangular       every record must have as many fields as the first one; a record that does not is an error (RaggedRecord) and is not delivered
//   skip_empty_lines  a line with nothing on it is not a record (a line that holds only "" IS a record: one empty field)
//   crlf              writer only: records end in "\r\n" (true) or "\n" (false)
//
// THE FORMAT
//   * Records end at "\n" or "\r\n". The last record may or may not be terminated; a terminator at the very end does NOT start another (empty) record.
//   * A field is quoted when it starts with '"'. Inside quotes the delimiter, "\n" and "\r\n" are ordinary characters (kept exactly as they are), and "" stands for one quote.
//     The closing quote must be followed by the delimiter, a record end or the end of the text (otherwise TextAfterClosingQuote).
//   * An unquoted field runs to the next delimiter or record end and may not contain a '"' (QuoteInUnquotedField). A lone '\r' that is not part of "\r\n" is data.
//   * An empty line is a record with ONE empty field (unless skip_empty_lines). "a," has two fields; ",a" has two; "," has two. Empty text has no records.
//   * A UTF-8 byte order mark (EF BB BF) at the very start of the text is skipped. Anywhere else it is data.
//
// csv::Reader(text, options)   reads one record at a time without building a table.
//   next(fields) -> bool       fills `fields` with the next record and returns true; false at the end, or when an error stopped the reading (check error()).
//                              `fields` is REUSED: its strings keep their capacity between calls, so reading a long file of same-width records does not allocate per record.
//   error()                    std::optional<Error>: {kind, line, record}. `line` is the physical, 1-based line: for UnterminatedQuote the line on which the quote OPENED,
//                              for TextAfterClosingQuote and QuoteInUnquotedField the line where it was detected, for RaggedRecord the line on which the record starts.
//                              `record` is the 1-based index of the record being read (the number delivered so far + 1). After an error next() keeps returning false.
// csv::parse(text, options) -> ParseResult { Table table; optional<Error> error; }   table holds every complete record read before an error.
// csv::write(table, options) -> std::string
//   A field is quoted exactly when it contains the delimiter, '"', '\r' or '\n' -- and also when it is the only field of its record and is empty (otherwise the
//   line would look blank), and when it is the very first field of the output and starts with the bytes EF BB BF (otherwise a reader would take them for a BOM).
//   Quotes inside a quoted field are doubled. A row with no fields at all is skipped. Every record, also the last, ends with the line terminator.
//   parse(write(t)) must give t back for every table whose rows are not empty, for any delimiter and either line ending.
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace csv {

using Table = std::vector<std::vector<std::string>>;

enum class ErrorKind { BadOptions, UnterminatedQuote, QuoteInUnquotedField, TextAfterClosingQuote, RaggedRecord };
struct Error {
    ErrorKind kind;
    std::size_t line;
    std::size_t record;
};
struct Options {
    char delimiter = ',';
    bool rectangular = false;
    bool skip_empty_lines = false;
    bool crlf = true;
};
struct ParseResult {
    Table table;
    std::optional<Error> error;
};

class Reader {
public:
    explicit Reader(std::string_view text, Options options = {}) : text_(text), options_(options) {}
    // TODO
    bool next(std::vector<std::string>&) { return false; }
    const std::optional<Error>& error() const { return error_; }

private:
    std::string_view text_;
    Options options_;
    std::optional<Error> error_;
};

inline ParseResult parse(std::string_view text, Options options = {}) {
    ParseResult r;
    Reader reader(text, options);
    std::vector<std::string> fields;
    while (reader.next(fields)) r.table.push_back(fields);
    r.error = reader.error();
    return r;
}

inline std::string write(const Table&, Options = {}) { return ""; }   // TODO

}  // namespace csv
