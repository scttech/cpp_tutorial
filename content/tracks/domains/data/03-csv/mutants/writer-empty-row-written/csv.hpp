// MUTANT: a row with no fields is written as an empty line
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
    explicit Reader(std::string_view text, Options options = {}) : text_(text), options_(options) {
        if (options_.delimiter == '"' || options_.delimiter == '\n' || options_.delimiter == '\r') error_ = Error{ErrorKind::BadOptions, 0, 0};
        if (text_.size() >= 3 && text_.substr(0, 3) == "\xEF\xBB\xBF") pos_ = 3;
    }

    bool next(std::vector<std::string>& fields) {
        for (;;) {
            if (error_ || pos_ >= text_.size()) return false;
            const std::size_t record_line = line_;
            std::size_t k = 0;
            bool first_quoted = false;
            if (!read_record(fields, k, first_quoted)) return false;
            fields.resize(k);
            if (k == 1 && fields[0].empty() && !first_quoted && options_.skip_empty_lines) continue;
            if (options_.rectangular) {
                if (!width_) width_ = k;
                else if (*width_ != k) {
                    error_ = Error{ErrorKind::RaggedRecord, record_line, delivered_ + 1};
                    return false;
                }
            }
            ++delivered_;
            return true;
        }
    }

    const std::optional<Error>& error() const { return error_; }

private:
    bool at_record_end() const { return text_[pos_] == '\n' || (text_[pos_] == '\r' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '\n'); }

    static std::string& slot(std::vector<std::string>& fields, std::size_t k) {
        if (k >= fields.size()) fields.emplace_back();
        fields[k].clear();
        return fields[k];
    }

    bool fail(ErrorKind kind, std::size_t line) {
        error_ = Error{kind, line, delivered_ + 1};
        return false;
    }

    bool read_record(std::vector<std::string>& fields, std::size_t& k, bool& first_quoted) {
        for (;;) {
            std::string& f = slot(fields, k);
            if (pos_ < text_.size() && text_[pos_] == '"') {
                if (k == 0) first_quoted = true;
                const std::size_t opened = line_;
                ++pos_;
                for (;;) {
                    if (pos_ >= text_.size()) return fail(ErrorKind::UnterminatedQuote, opened);
                    const char c = text_[pos_];
                    if (c == '"') {
                        if (pos_ + 1 < text_.size() && text_[pos_ + 1] == '"') {
                            f.push_back('"');
                            pos_ += 2;
                            continue;
                        }
                        ++pos_;
                        break;
                    }
                    if (c == '\n') ++line_;
                    f.push_back(c);
                    ++pos_;
                }
                if (pos_ < text_.size() && text_[pos_] != options_.delimiter && !at_record_end()) return fail(ErrorKind::TextAfterClosingQuote, line_);
            } else {
                while (pos_ < text_.size() && text_[pos_] != options_.delimiter && !at_record_end()) {
                    if (text_[pos_] == '"') return fail(ErrorKind::QuoteInUnquotedField, line_);
                    f.push_back(text_[pos_]);
                    ++pos_;
                }
            }
            ++k;
            if (pos_ >= text_.size()) return true;
            if (text_[pos_] == options_.delimiter) {
                ++pos_;
                continue;   // another field follows, even if it is empty and the text ends here
            }
            pos_ += text_[pos_] == '\r' ? 2 : 1;   // the record end
            ++line_;
            return true;
        }
    }

    std::string_view text_;
    Options options_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t delivered_ = 0;
    std::optional<std::size_t> width_;
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

namespace detail {
inline bool needs_quotes(const std::string& f, char delimiter, bool only_field, bool very_first) {
    if (f.empty()) return only_field;
    if (very_first && f.size() >= 3 && f.compare(0, 3, "\xEF\xBB\xBF") == 0) return true;
    for (char c : f)
        if (c == delimiter || c == '"' || c == '\r' || c == '\n') return true;
    return false;
}
}  // namespace detail

inline std::string write(const Table& table, Options options = {}) {
    std::string out;
    bool very_first = true;
    for (const auto& row : table) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i > 0) out.push_back(options.delimiter);
            const std::string& f = row[i];
            if (detail::needs_quotes(f, options.delimiter, row.size() == 1, very_first && i == 0)) {
                out.push_back('"');
                for (char c : f) {
                    if (c == '"') out.push_back('"');
                    out.push_back(c);
                }
                out.push_back('"');
            } else {
                out += f;
            }
        }
        very_first = false;
        out += options.crlf ? "\r\n" : "\n";
    }
    return out;
}

}  // namespace csv
