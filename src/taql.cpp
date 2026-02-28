#include "casacore_mini/taql.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/quantity.hpp"
#include "casacore_mini/measure_convert.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <regex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace casacore_mini {

// ===========================================================================
// Token types
// ===========================================================================

enum class TokenType : std::uint8_t { // NOLINT(performance-enum-size)
    // Literals
    integer_lit,
    float_lit,
    string_lit,
    regex_lit,
    datetime_lit,
    // Identifiers and keywords
    identifier,
    // Operators
    plus,
    minus,
    star,
    slash,
    int_divide,   // //
    percent,
    power,        // **
    ampersand,
    pipe,
    caret,
    tilde,
    bang,
    // Comparison
    eq,           // = or ==
    ne,           // != or <>
    lt,
    le,
    gt,
    ge,
    near_eq,      // ~=
    not_near_eq,  // !~=
    regex_match,  // ~
    not_regex,    // !~
    // Punctuation
    lparen,
    rparen,
    lbracket,
    rbracket,
    lbrace,
    rbrace,
    comma,
    dot,
    colon,
    double_colon, // ::
    semicolon,
    // Special
    end_of_input,
    error_tok
};

struct Token {
    TokenType type = TokenType::end_of_input;
    std::string_view text;
    TaqlSourceLoc loc;
    // For numeric literals:
    std::int64_t int_val = 0;
    double float_val = 0.0;
    std::string str_val; // for string/regex literals
};

// ===========================================================================
// Lexer
// ===========================================================================

class TaqlLexer {
  public:
    explicit TaqlLexer(std::string_view source)
        : source_(source), pos_(0), line_(1), col_(1) {}

    Token next() {
        skip_whitespace_and_comments();
        if (pos_ >= source_.size()) {
            return make_token(TokenType::end_of_input, pos_, 0);
        }

        char c = source_[pos_];

        // String literals
        if (c == '\'' || c == '"') return lex_string(c);

        // Regex literals: p/.../, m/.../, f/.../ or ~/.../ (handled later)
        if ((c == 'p' || c == 'm' || c == 'f') && pos_ + 1 < source_.size() &&
            source_[pos_ + 1] == '/') {
            return lex_regex_literal();
        }

        // Hex numbers (must be checked before generic numbers)
        if (c == '0' && pos_ + 1 < source_.size() &&
            (source_[pos_ + 1] == 'x' || source_[pos_ + 1] == 'X')) {
            return lex_hex();
        }

        // Numbers
        if (std::isdigit(c) != 0 || (c == '.' && pos_ + 1 < source_.size() &&
                                      std::isdigit(source_[pos_ + 1]) != 0)) {
            return lex_number();
        }

        // Identifiers and keywords
        if (std::isalpha(c) != 0 || c == '_' || c == '$') {
            return lex_identifier();
        }

        // Two-character operators
        if (pos_ + 1 < source_.size()) {
            char c2 = source_[pos_ + 1];
            if (c == '*' && c2 == '*') return make_token(TokenType::power, pos_, 2);
            if (c == '/' && c2 == '/') return make_token(TokenType::int_divide, pos_, 2);
            if (c == '=' && c2 == '=') return make_token(TokenType::eq, pos_, 2);
            if (c == '!' && c2 == '=') return make_token(TokenType::ne, pos_, 2);
            if (c == '<' && c2 == '>') return make_token(TokenType::ne, pos_, 2);
            if (c == '<' && c2 == '=') return make_token(TokenType::le, pos_, 2);
            if (c == '>' && c2 == '=') return make_token(TokenType::ge, pos_, 2);
            if (c == ':' && c2 == ':') return make_token(TokenType::double_colon, pos_, 2);
            if (c == '~' && c2 == '=') return make_token(TokenType::near_eq, pos_, 2);
            if (c == '!' && c2 == '~') {
                if (pos_ + 2 < source_.size() && source_[pos_ + 2] == '=') {
                    return make_token(TokenType::not_near_eq, pos_, 3);
                }
                return make_token(TokenType::not_regex, pos_, 2);
            }
            // && and || produce double tokens; parser treats them as logical
            // operators by checking two consecutive ampersand/pipe tokens.
        }

        // Single character operators/punctuation  NOLINT(bugprone-branch-clone)
        switch (c) {
        case '+': return make_token(TokenType::plus, pos_, 1); // NOLINT(bugprone-branch-clone)
        case '-': return make_token(TokenType::minus, pos_, 1);
        case '*': return make_token(TokenType::star, pos_, 1);
        case '/': return make_token(TokenType::slash, pos_, 1);
        case '%': return make_token(TokenType::percent, pos_, 1);
        case '&': return make_token(TokenType::ampersand, pos_, 1);
        case '|': return make_token(TokenType::pipe, pos_, 1);
        case '^': return make_token(TokenType::caret, pos_, 1);
        case '~': return make_token(TokenType::tilde, pos_, 1);
        case '!': return make_token(TokenType::bang, pos_, 1);
        case '=': return make_token(TokenType::eq, pos_, 1);
        case '<': return make_token(TokenType::lt, pos_, 1);
        case '>': return make_token(TokenType::gt, pos_, 1);
        case '(': return make_token(TokenType::lparen, pos_, 1);
        case ')': return make_token(TokenType::rparen, pos_, 1);
        case '[': return make_token(TokenType::lbracket, pos_, 1);
        case ']': return make_token(TokenType::rbracket, pos_, 1);
        case '{': return make_token(TokenType::lbrace, pos_, 1);
        case '}': return make_token(TokenType::rbrace, pos_, 1);
        case ',': return make_token(TokenType::comma, pos_, 1);
        case '.': return make_token(TokenType::dot, pos_, 1);
        case ':': return make_token(TokenType::colon, pos_, 1);
        case ';': return make_token(TokenType::semicolon, pos_, 1);
        default: break;
        }

        // Unknown character
        auto tok = make_token(TokenType::error_tok, pos_, 1);
        return tok;
    }

    TaqlSourceLoc current_loc() const {
        return {pos_, line_, col_};
    }

    std::string_view source() const { return source_; }

  private:
    std::string_view source_;
    std::size_t pos_;
    std::size_t line_;
    std::size_t col_;

    void skip_whitespace_and_comments() {
        while (pos_ < source_.size()) {
            char c = source_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if (c == '\n') { ++line_; col_ = 0; }
                advance(1);
                continue;
            }
            // Line comments: -- or //
            if (c == '-' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '-') {
                while (pos_ < source_.size() && source_[pos_] != '\n') advance(1);
                continue;
            }
            break;
        }
    }

    void advance(std::size_t n) {
        for (std::size_t i = 0; i < n && pos_ < source_.size(); ++i) {
            ++pos_;
            ++col_;
        }
    }

    Token make_token(TokenType type, std::size_t start, std::size_t len) {
        Token tok;
        tok.type = type;
        tok.text = source_.substr(start, len);
        tok.loc = {start, line_, col_};
        advance(len);
        return tok;
    }

    Token lex_string(char quote) {
        std::size_t start = pos_;
        advance(1); // skip opening quote
        std::string result;
        while (pos_ < source_.size() && source_[pos_] != quote) {
            if (source_[pos_] == '\\' && pos_ + 1 < source_.size()) {
                advance(1);
                switch (source_[pos_]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                default: result += quote; break;
                }
            } else {
                result += source_[pos_];
            }
            advance(1);
        }
        if (pos_ < source_.size()) advance(1); // skip closing quote
        Token tok;
        tok.type = TokenType::string_lit;
        tok.text = source_.substr(start, pos_ - start);
        tok.loc = {start, line_, col_};
        tok.str_val = std::move(result);
        return tok;
    }

    Token lex_regex_literal() {
        std::size_t start = pos_;
        char prefix = source_[pos_];
        advance(2); // skip prefix + /
        std::string pattern;
        while (pos_ < source_.size() && source_[pos_] != '/') {
            if (source_[pos_] == '\\' && pos_ + 1 < source_.size()) {
                // Escaped character in regex: keep both backslash and next char.
                pattern += source_[pos_];
                advance(1);
            }
            pattern += source_[pos_];
            advance(1);
        }
        if (pos_ < source_.size()) advance(1); // skip closing /
        // Check for flags (i for case-insensitive)
        std::string flags;
        while (pos_ < source_.size() && std::isalpha(source_[pos_]) != 0) {
            flags += source_[pos_];
            advance(1);
        }
        Token tok;
        tok.type = TokenType::regex_lit;
        tok.text = source_.substr(start, pos_ - start);
        tok.loc = {start, line_, col_};
        tok.str_val = std::string(1, prefix) + "/" + pattern + "/" + flags;
        return tok;
    }

    Token lex_number() {
        std::size_t start = pos_;
        bool is_float = false;

        // Integer part
        while (pos_ < source_.size() && std::isdigit(source_[pos_]) != 0) {
            advance(1);
        }

        // Decimal point
        if (pos_ < source_.size() && source_[pos_] == '.' &&
            (pos_ + 1 >= source_.size() || source_[pos_ + 1] != '.')) {
            is_float = true;
            advance(1);
            while (pos_ < source_.size() && std::isdigit(source_[pos_]) != 0) {
                advance(1);
            }
        }

        // Exponent
        if (pos_ < source_.size() && (source_[pos_] == 'e' || source_[pos_] == 'E')) {
            is_float = true;
            advance(1);
            if (pos_ < source_.size() && (source_[pos_] == '+' || source_[pos_] == '-')) {
                advance(1);
            }
            while (pos_ < source_.size() && std::isdigit(source_[pos_]) != 0) {
                advance(1);
            }
        }

        // Complex suffix (i or j)
        bool is_complex = false;
        if (pos_ < source_.size() && (source_[pos_] == 'i' || source_[pos_] == 'j')) {
            is_complex = true;
            advance(1);
        }

        auto text = source_.substr(start, pos_ - start);
        Token tok;
        tok.text = text;
        tok.loc = {start, line_, col_};

        if (is_complex) {
            tok.type = TokenType::float_lit;
            // Store imaginary part as float; parser handles complex construction
            std::string num_str(text.substr(0, text.size() - 1));
            tok.float_val = std::stod(num_str);
            tok.str_val = "complex";
        } else if (is_float) {
            tok.type = TokenType::float_lit;
            tok.float_val = std::stod(std::string(text));
        } else {
            tok.type = TokenType::integer_lit;
            tok.int_val = std::stoll(std::string(text));
            tok.float_val = static_cast<double>(tok.int_val);
        }
        return tok;
    }

    Token lex_hex() {
        std::size_t start = pos_;
        advance(2); // skip 0x
        while (pos_ < source_.size() && std::isxdigit(source_[pos_]) != 0) {
            advance(1);
        }
        auto text = source_.substr(start, pos_ - start);
        Token tok;
        tok.type = TokenType::integer_lit;
        tok.text = text;
        tok.loc = {start, line_, col_};
        tok.int_val = std::stoll(std::string(text), nullptr, 16);
        return tok;
    }

    Token lex_identifier() {
        std::size_t start = pos_;
        while (pos_ < source_.size() &&
               (std::isalnum(source_[pos_]) != 0 || source_[pos_] == '_' || source_[pos_] == '$')) {
            advance(1);
        }
        auto text = source_.substr(start, pos_ - start);
        Token tok;
        tok.type = TokenType::identifier;
        tok.text = text;
        tok.loc = {start, line_, col_};
        return tok;
    }
};

// ===========================================================================
// Keyword matching helpers
// ===========================================================================

namespace {

/// Case-insensitive string comparison.
bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

/// Check if token is an identifier matching a keyword (case-insensitive).
bool is_keyword(const Token& tok, std::string_view kw) {
    return tok.type == TokenType::identifier && iequals(tok.text, kw);
}

/// Upper-case a string_view into a new string.
std::string to_upper(std::string_view sv) {
    std::string result(sv);
    for (auto& c : result) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return result;
}

} // namespace

// ===========================================================================
// Recursive descent parser
// ===========================================================================

class TaqlParser {
  public:
    explicit TaqlParser(std::string_view source)
        : lexer_(source), source_(source) {
        advance(); // prime the first token
    }

    TaqlAst parse() {
        TaqlAst ast;

        // Optional TIMING
        if (is_keyword(current_, "TIMING")) {
            ast.timing = true;
            advance();
        }

        // Optional STYLE
        if (is_keyword(current_, "STYLE")) {
            advance();
            ast.style = std::string(current_.text);
            advance();
        }

        // Determine command type.
        // Bare expressions (no keyword) and SELECT/WITH all route to parse_select.
        if (is_keyword(current_, "UPDATE")) {
            ast.command = TaqlCommand::update_cmd;
            parse_update(ast);
        } else if (is_keyword(current_, "INSERT")) {
            ast.command = TaqlCommand::insert_cmd;
            parse_insert(ast);
        } else if (is_keyword(current_, "DELETE")) {
            ast.command = TaqlCommand::delete_cmd;
            parse_delete(ast);
        } else if (is_keyword(current_, "COUNT")) {
            ast.command = TaqlCommand::count_cmd;
            parse_count(ast);
        } else if (is_keyword(current_, "CALC")) {
            ast.command = TaqlCommand::calc_cmd;
            parse_calc(ast);
        } else if (is_keyword(current_, "CREATE") || is_keyword(current_, "CREATETAB")) {
            ast.command = TaqlCommand::create_table_cmd;
            parse_create_table(ast);
        } else if (is_keyword(current_, "ALTER") || is_keyword(current_, "ALTERTAB")) {
            ast.command = TaqlCommand::alter_table_cmd;
            parse_alter_table(ast);
        } else if (is_keyword(current_, "DROP") || is_keyword(current_, "DROPTAB")) {
            ast.command = TaqlCommand::drop_table_cmd;
            parse_drop_table(ast);
        } else if (is_keyword(current_, "SHOW")) {
            ast.command = TaqlCommand::show_cmd;
            parse_show(ast);
        } else {
            // SELECT, WITH, or bare expression all parse as select_cmd.
            ast.command = TaqlCommand::select_cmd;
            parse_select(ast);
        }

        return ast;
    }

  private:
    TaqlLexer lexer_;
    std::string_view source_;
    Token current_;
    Token previous_;

    void advance() {
        previous_ = current_;
        current_ = lexer_.next();
    }

    void expect_keyword(std::string_view kw) {
        if (!is_keyword(current_, kw)) {
            throw_error("expected '" + std::string(kw) + "'");
        }
        advance();
    }

    void expect(TokenType type) {
        if (current_.type != type) {
            throw_error("unexpected token '" + std::string(current_.text) + "'");
        }
        advance();
    }

    [[noreturn]] void throw_error(const std::string& msg) {
        TaqlParseError err;
        err.message = msg;
        err.location = current_.loc;
        err.token = std::string(current_.text);
        throw std::runtime_error("TaQL parse error at line " +
                                 std::to_string(err.location.line) + " col " +
                                 std::to_string(err.location.column) + ": " + msg +
                                 " (got '" + err.token + "')");
    }

    bool at_end() const { return current_.type == TokenType::end_of_input; }

    bool match_keyword(std::string_view kw) {
        if (is_keyword(current_, kw)) {
            advance();
            return true;
        }
        return false;
    }

    bool match(TokenType type) {
        if (current_.type == type) {
            advance();
            return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // SELECT
    // -----------------------------------------------------------------------
    void parse_select(TaqlAst& ast) {
        // WITH clause
        if (match_keyword("WITH")) {
            parse_with(ast);
        }

        if (match_keyword("SELECT")) {
            // DISTINCT / ALL
            if (match_keyword("DISTINCT")) {
                ast.select_distinct = true;
            } else {
                match_keyword("ALL");
            }
        }

        // Projection list (optional - may be empty for "SELECT FROM t")
        if (!is_keyword(current_, "FROM") && !at_end()) {
            parse_projection_list(ast);
        }

        // FROM clause
        if (match_keyword("FROM")) {
            parse_from(ast);
        }

        // JOIN
        while (is_keyword(current_, "JOIN")) {
            advance();
            TaqlJoin join;
            join.table = parse_table_ref();
            if (match_keyword("ON")) {
                join.on_expr = parse_expression();
            }
            ast.joins.push_back(std::move(join));
        }

        // WHERE
        if (match_keyword("WHERE")) {
            ast.where_expr = parse_expression();
        }

        // GROUPBY
        if (match_keyword("GROUPBY") || match_keyword("GROUP")) {
            match_keyword("BY");
            if (match_keyword("ROLLUP")) {
                ast.has_rollup = true;
            }
            parse_group_by(ast);
        }

        // HAVING
        if (match_keyword("HAVING")) {
            ast.having_expr = parse_expression();
        }

        // ORDERBY
        if (match_keyword("ORDERBY") || match_keyword("ORDER")) {
            match_keyword("BY");
            if (match_keyword("DISTINCT") || match_keyword("NODUPL")) {
                ast.order_distinct = true;
            }
            parse_order_by(ast);
        }

        // LIMIT / OFFSET
        parse_limit_offset(ast);

        // GIVING
        if (match_keyword("GIVING") || match_keyword("INTO")) {
            ast.giving_table = parse_table_ref();
            parse_table_options(ast);
        }

        // DMINFO
        if (match_keyword("DMINFO")) {
            parse_dminfo(ast);
        }
    }

    void parse_with(TaqlAst& ast) {
        do {
            ast.with_tables.push_back(parse_table_ref());
        } while (match(TokenType::comma));
    }

    void parse_projection_list(TaqlAst& ast) {
        do {
            TaqlProjection proj;
            if (current_.type == TokenType::star) {
                proj.expr.type = ExprType::wildcard;
                advance();
            } else {
                proj.expr = parse_expression();
            }
            if (match_keyword("AS")) {
                proj.alias = std::string(current_.text);
                advance();
                // Optional data type after alias
                if (current_.type == TokenType::identifier &&
                    !is_select_clause_keyword(current_.text)) {
                    proj.data_type = to_upper(current_.text);
                    advance();
                }
            }
            ast.projections.push_back(std::move(proj));
        } while (match(TokenType::comma));
    }

    void parse_from(TaqlAst& ast) {
        do {
            ast.tables.push_back(parse_table_ref());
        } while (match(TokenType::comma));
    }

    TaqlTableRef parse_table_ref() {
        TaqlTableRef ref;
        if (current_.type == TokenType::lparen) {
            // Subquery
            advance();
            auto sub_parser = TaqlParser(remaining_source());
            // Actually, for simplicity, just capture the table name approach
            // Real subquery parsing is complex; for now, capture as name
            ref.name = "(subquery)";
            // Skip to matching paren
            int depth = 1;
            while (!at_end() && depth > 0) {
                if (current_.type == TokenType::lparen) ++depth;
                if (current_.type == TokenType::rparen) --depth;
                if (depth > 0) advance();
            }
            expect(TokenType::rparen);
        } else if (current_.type == TokenType::string_lit) {
            ref.name = current_.str_val;
            advance();
        } else if (current_.type == TokenType::identifier || current_.type == TokenType::lbracket) {
            ref.name = parse_table_name();
        }

        // Shorthand alias
        if (match_keyword("AS") ||
            (current_.type == TokenType::identifier &&
             !is_select_clause_keyword(current_.text))) {
            // Explicit "AS alias" or implicit shorthand (next word is alias if not a keyword)
            ref.shorthand = std::string(current_.text);
            advance();
        }

        return ref;
    }

    std::string parse_table_name() {
        std::string name;
        if (current_.type == TokenType::lbracket) {
            // [path/to/table]
            advance();
            while (current_.type != TokenType::rbracket && !at_end()) {
                name += std::string(current_.text);
                advance();
            }
            expect(TokenType::rbracket);
        } else if (current_.type == TokenType::string_lit) {
            // 'path/to/table' or "path/to/table"
            name = current_.str_val;
            advance();
        } else {
            name = std::string(current_.text);
            advance();
            // Handle dotted names and :: subtable references
            while (current_.type == TokenType::dot ||
                   current_.type == TokenType::double_colon ||
                   current_.type == TokenType::slash) {
                name += std::string(current_.text);
                advance();
                if (current_.type == TokenType::identifier ||
                    current_.type == TokenType::integer_lit) {
                    name += std::string(current_.text);
                    advance();
                }
            }
        }
        return name;
    }

    void parse_group_by(TaqlAst& ast) {
        do {
            ast.group_by.push_back(parse_expression());
        } while (match(TokenType::comma));
    }

    void parse_order_by(TaqlAst& ast) {
        do {
            TaqlSortKey key;
            key.expr = parse_expression();
            if (match_keyword("ASC")) {
                key.order = SortOrder::ascending;
            } else if (match_keyword("DESC")) {
                key.order = SortOrder::descending;
            }
            ast.order_by.push_back(std::move(key));
        } while (match(TokenType::comma));
    }

    void parse_limit_offset(TaqlAst& ast) {
        if (match_keyword("LIMIT")) {
            ast.limit_expr = parse_expression();
            // Check for LIMIT start:end:step syntax
        }
        if (match_keyword("OFFSET")) {
            ast.offset_expr = parse_expression();
        }
    }

    void parse_table_options(TaqlAst& ast) {
        if (match_keyword("AS")) {
            // Parse key=value pairs
            while (current_.type == TokenType::identifier) {
                auto key = to_upper(current_.text);
                advance();
                std::string* target = nullptr;
                if (key == "TYPE") {
                    target = &ast.table_options.type;
                } else if (key == "ENDIAN") {
                    target = &ast.table_options.endian;
                } else if (key == "STORAGE") {
                    target = &ast.table_options.storage;
                } else if (key == "OVERWRITE") {
                    ast.table_options.overwrite = true;
                    continue;
                } else {
                    break;
                }
                if (target != nullptr) {
                    *target = to_upper(current_.text);
                    advance();
                }
            }
        }
    }

    void parse_dminfo(TaqlAst& ast) {
        // DMINFO is a record-like specification
        if (current_.type == TokenType::lbracket) {
            ast.dminfo.push_back(parse_expression());
        }
    }

    // -----------------------------------------------------------------------
    // UPDATE
    // -----------------------------------------------------------------------
    void parse_update(TaqlAst& ast) {
        advance(); // skip UPDATE

        // Table reference
        ast.tables.push_back(parse_table_ref());

        // SET
        expect_keyword("SET");
        parse_assignments(ast);

        // FROM (optional additional tables)
        if (match_keyword("FROM")) {
            parse_from(ast);
        }

        // WHERE
        if (match_keyword("WHERE")) {
            ast.where_expr = parse_expression();
        }

        // ORDERBY
        if (match_keyword("ORDERBY") || match_keyword("ORDER")) {
            match_keyword("BY");
            parse_order_by(ast);
        }

        parse_limit_offset(ast);
    }

    void parse_assignments(TaqlAst& ast) {
        do {
            TaqlAssignment asgn;
            // Handle (col, mask) = expr syntax
            if (current_.type == TokenType::lparen) {
                advance();
                asgn.column = std::string(current_.text);
                advance();
                expect(TokenType::comma);
                asgn.mask_column = std::string(current_.text);
                advance();
                expect(TokenType::rparen);
            } else {
                asgn.column = std::string(current_.text);
                advance();
                // Optional array indices
                if (current_.type == TokenType::lbracket) {
                    advance();
                    while (current_.type != TokenType::rbracket && !at_end()) {
                        asgn.indices.push_back(parse_expression());
                        if (!match(TokenType::comma)) break;
                    }
                    expect(TokenType::rbracket);
                }
            }
            expect(TokenType::eq);
            asgn.value = parse_expression();
            ast.assignments.push_back(std::move(asgn));
        } while (match(TokenType::comma));
    }

    // -----------------------------------------------------------------------
    // INSERT
    // -----------------------------------------------------------------------
    void parse_insert(TaqlAst& ast) {
        advance(); // skip INSERT
        match_keyword("INTO");

        ast.tables.push_back(parse_table_ref());

        // Optional column list
        if (current_.type == TokenType::lbracket || current_.type == TokenType::lparen) {
            auto close = (current_.type == TokenType::lbracket) ? TokenType::rbracket : TokenType::rparen;
            advance();
            while (current_.type != close && !at_end()) {
                ast.insert_columns.push_back(std::string(current_.text));
                advance();
                if (!match(TokenType::comma)) break;
            }
            expect(close);
        }

        if (match_keyword("SET")) {
            // INSERT SET col=val, col=val
            TaqlAssignment asgn;
            std::vector<TaqlExprNode> row;
            do {
                ast.insert_columns.push_back(std::string(current_.text));
                advance();
                expect(TokenType::eq);
                row.push_back(parse_expression());
            } while (match(TokenType::comma));
            ast.insert_values.push_back(std::move(row));
        } else if (match_keyword("VALUES") || match_keyword("VALUE")) {
            // INSERT VALUES (v1,v2), (v3,v4)
            do {
                expect(TokenType::lparen);
                std::vector<TaqlExprNode> row;
                do {
                    row.push_back(parse_expression());
                } while (match(TokenType::comma));
                expect(TokenType::rparen);
                ast.insert_values.push_back(std::move(row));
            } while (match(TokenType::comma));
        } else if (is_keyword(current_, "SELECT")) {
            // INSERT ... SELECT ...
            parse_select(ast);
        }

        parse_limit_offset(ast);
    }

    // -----------------------------------------------------------------------
    // DELETE
    // -----------------------------------------------------------------------
    void parse_delete(TaqlAst& ast) {
        advance(); // skip DELETE
        match_keyword("FROM");

        ast.tables.push_back(parse_table_ref());

        if (match_keyword("WHERE")) {
            ast.where_expr = parse_expression();
        }

        if (match_keyword("ORDERBY") || match_keyword("ORDER")) {
            match_keyword("BY");
            parse_order_by(ast);
        }

        parse_limit_offset(ast);
    }

    // -----------------------------------------------------------------------
    // COUNT
    // -----------------------------------------------------------------------
    void parse_count(TaqlAst& ast) {
        advance(); // skip COUNT

        // Optional column list
        if (!is_keyword(current_, "FROM") && !at_end()) {
            parse_projection_list(ast);
        }

        if (match_keyword("FROM")) {
            parse_from(ast);
        }

        if (match_keyword("WHERE")) {
            ast.where_expr = parse_expression();
        }
    }

    // -----------------------------------------------------------------------
    // CALC
    // -----------------------------------------------------------------------
    void parse_calc(TaqlAst& ast) {
        advance(); // skip CALC
        ast.calc_expr = parse_expression();

        if (match_keyword("FROM")) {
            parse_from(ast);
        }
    }

    // -----------------------------------------------------------------------
    // CREATE TABLE
    // -----------------------------------------------------------------------
    void parse_create_table(TaqlAst& ast) {
        advance(); // skip CREATE
        match_keyword("TABLE");

        ast.tables.push_back(parse_table_ref());

        parse_table_options(ast);

        // LIKE clause
        if (match_keyword("LIKE")) {
            ast.like_table = parse_table_name();
            if (match_keyword("DROP")) {
                match_keyword("COLUMN");
                do {
                    ast.like_drop_columns.push_back(std::string(current_.text));
                    advance();
                } while (match(TokenType::comma));
            }
        }

        // Column definitions
        if (current_.type == TokenType::lparen || current_.type == TokenType::lbracket) {
            auto close = (current_.type == TokenType::lparen) ? TokenType::rparen : TokenType::rbracket;
            advance();
            parse_column_defs(ast);
            expect(close);
        }

        parse_limit_offset(ast);

        if (match_keyword("DMINFO")) {
            parse_dminfo(ast);
        }
    }

    void parse_column_defs(TaqlAst& ast) {
        do {
            TaqlColumnDef col;
            col.name = std::string(current_.text);
            advance();

            // Data type
            if (current_.type == TokenType::identifier) {
                col.data_type = to_upper(current_.text);
                advance();
            }

            // Properties: NDIM, SHAPE, UNIT, COMMENT
            while (current_.type == TokenType::identifier &&
                   (iequals(current_.text, "NDIM") || iequals(current_.text, "SHAPE") ||
                    iequals(current_.text, "UNIT") || iequals(current_.text, "COMMENT"))) {
                auto prop = to_upper(current_.text);
                advance();
                if (prop == "NDIM") {
                    expect(TokenType::eq);
                    col.ndim = static_cast<int>(current_.int_val);
                    advance();
                } else if (prop == "SHAPE") {
                    expect(TokenType::eq);
                    expect(TokenType::lbracket);
                    while (current_.type != TokenType::rbracket && !at_end()) {
                        col.shape.push_back(current_.int_val);
                        advance();
                        if (!match(TokenType::comma)) break;
                    }
                    expect(TokenType::rbracket);
                } else {
                    // UNIT or COMMENT: both read a string/identifier value.
                    expect(TokenType::eq);
                    auto& dest = (prop == "UNIT") ? col.unit : col.comment;
                    dest = current_.str_val.empty() ? std::string(current_.text)
                                                    : current_.str_val;
                    advance();
                }
            }

            ast.create_columns.push_back(std::move(col));
        } while (match(TokenType::comma));
    }

    // -----------------------------------------------------------------------
    // ALTER TABLE
    // -----------------------------------------------------------------------
    void parse_alter_table(TaqlAst& ast) {
        advance(); // skip ALTER
        match_keyword("TABLE");

        ast.tables.push_back(parse_table_ref());

        // Sub-commands
        while (!at_end()) {
            TaqlAlterStep step;
            if (match_keyword("ADD") || match_keyword("ADDCOL")) {
                match_keyword("COLUMN");
                step.action = AlterAction::add_column;
                parse_alter_columns(step);
            } else if (match_keyword("COPY") || match_keyword("COPYCOL")) {
                match_keyword("COLUMN");
                step.action = AlterAction::copy_column;
                parse_alter_renames(step);
            } else if (match_keyword("RENAME") || match_keyword("RENAMECOL")) {
                match_keyword("COLUMN");
                step.action = AlterAction::rename_column;
                parse_alter_renames(step);
            } else if (match_keyword("DROP") || match_keyword("DROPCOL")) {
                match_keyword("COLUMN");
                step.action = AlterAction::drop_column;
                parse_alter_names(step);
            } else if (match_keyword("SET") || match_keyword("SETKEY")) {
                match_keyword("KEYWORD");
                step.action = AlterAction::set_keyword;
                parse_alter_keywords(step);
            } else if (match_keyword("COPYKEY")) {
                step.action = AlterAction::copy_keyword;
                parse_alter_renames(step);
            } else if (match_keyword("RENAMEKEY")) {
                step.action = AlterAction::rename_keyword;
                parse_alter_renames(step);
            } else if (match_keyword("DROPKEY")) {
                step.action = AlterAction::drop_keyword;
                parse_alter_names(step);
            } else if (match_keyword("ADDROW")) {
                step.action = AlterAction::add_row;
                step.row_count = parse_expression();
            } else {
                break;
            }
            ast.alter_steps.push_back(std::move(step));
        }
    }

    void parse_alter_columns(TaqlAlterStep& step) {
        if (current_.type == TokenType::lparen || current_.type == TokenType::lbracket) {
            auto close = (current_.type == TokenType::lparen) ? TokenType::rparen : TokenType::rbracket;
            advance();
            TaqlAst dummy;
            parse_column_defs(dummy);
            step.columns = std::move(dummy.create_columns);
            expect(close);
        } else {
            TaqlAst dummy;
            parse_column_defs(dummy);
            step.columns = std::move(dummy.create_columns);
        }
    }

    void parse_alter_renames(TaqlAlterStep& step) {
        do {
            auto from = std::string(current_.text);
            advance();
            expect_keyword("TO");
            auto to_name = std::string(current_.text);
            advance();
            step.renames.emplace_back(std::move(from), std::move(to_name));
        } while (match(TokenType::comma));
    }

    void parse_alter_names(TaqlAlterStep& step) {
        do {
            step.names.push_back(std::string(current_.text));
            advance();
        } while (match(TokenType::comma));
    }

    void parse_alter_keywords(TaqlAlterStep& step) {
        do {
            auto name = std::string(current_.text);
            advance();
            expect(TokenType::eq);
            auto val = parse_expression();
            step.keywords.emplace_back(std::move(name), std::move(val));
        } while (match(TokenType::comma));
    }

    // -----------------------------------------------------------------------
    // DROP TABLE
    // -----------------------------------------------------------------------
    void parse_drop_table(TaqlAst& ast) {
        advance(); // skip DROP
        match_keyword("TABLE");

        do {
            ast.drop_tables.push_back(parse_table_name());
        } while (match(TokenType::comma));
    }

    // -----------------------------------------------------------------------
    // SHOW
    // -----------------------------------------------------------------------
    void parse_show(TaqlAst& ast) {
        advance(); // skip SHOW
        if (!at_end()) {
            ast.show_topic = std::string(current_.text);
            advance();
        }
    }

    // -----------------------------------------------------------------------
    // Expression parser (precedence climbing)
    // -----------------------------------------------------------------------

    TaqlExprNode parse_expression() {
        return parse_or_expr();
    }

    TaqlExprNode parse_or_expr() {
        auto left = parse_and_expr();
        for (;;) {
            if (match_keyword("OR")) {
                // OK, consumed
            } else if (current_.type == TokenType::pipe && peek_next_is(TokenType::pipe)) {
                advance(); // first |
                advance(); // second |
            } else {
                break;
            }
            TaqlExprNode node(ExprType::logical_op);
            node.op = TaqlOp::logical_or;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_and_expr());
            left = std::move(node);
        }
        return left;
    }

    TaqlExprNode parse_and_expr() {
        auto left = parse_not_expr();
        for (;;) {
            if (match_keyword("AND")) {
                // OK, consumed
            } else if (current_.type == TokenType::ampersand && peek_next_is(TokenType::ampersand)) {
                advance(); // first &
                advance(); // second &
            } else {
                break;
            }
            TaqlExprNode node(ExprType::logical_op);
            node.op = TaqlOp::logical_and;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_not_expr());
            left = std::move(node);
        }
        return left;
    }

    TaqlExprNode parse_not_expr() {
        if (match_keyword("NOT") || match(TokenType::bang)) {
            TaqlExprNode node(ExprType::unary_op);
            node.op = TaqlOp::logical_not;
            node.children.push_back(parse_comparison_expr());
            return node;
        }
        return parse_comparison_expr();
    }

    TaqlExprNode parse_comparison_expr() {
        auto left = parse_bitwise_or();

        // Comparison operators
        auto make_cmp = [&](TaqlOp op) {
            advance();
            TaqlExprNode node(ExprType::comparison);
            node.op = op;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_bitwise_or());
            return node;
        };

        if (current_.type == TokenType::eq) return make_cmp(TaqlOp::eq);
        if (current_.type == TokenType::ne) return make_cmp(TaqlOp::ne);
        if (current_.type == TokenType::lt) return make_cmp(TaqlOp::lt);
        if (current_.type == TokenType::le) return make_cmp(TaqlOp::le);
        if (current_.type == TokenType::gt) return make_cmp(TaqlOp::gt);
        if (current_.type == TokenType::ge) return make_cmp(TaqlOp::ge);
        if (current_.type == TokenType::near_eq) return make_cmp(TaqlOp::near);
        if (current_.type == TokenType::not_near_eq) return make_cmp(TaqlOp::not_near);

        // LIKE / ILIKE
        if (is_keyword(current_, "LIKE")) {
            advance();
            TaqlExprNode node(ExprType::like_expr);
            node.op = TaqlOp::like;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_bitwise_or());
            return node;
        }
        if (is_keyword(current_, "ILIKE")) {
            advance();
            TaqlExprNode node(ExprType::like_expr);
            node.op = TaqlOp::ilike;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_bitwise_or());
            return node;
        }
        if (is_keyword(current_, "NOT")) {
            // NOT LIKE, NOT IN, NOT BETWEEN
            advance();
            if (match_keyword("LIKE")) {
                TaqlExprNode node(ExprType::like_expr);
                node.op = TaqlOp::not_like;
                node.children.push_back(std::move(left));
                node.children.push_back(parse_bitwise_or());
                return node;
            }
            if (match_keyword("IN")) {
                TaqlExprNode node(ExprType::in_expr);
                node.op = TaqlOp::not_in_set;
                node.children.push_back(std::move(left));
                node.children.push_back(parse_set_or_range());
                return node;
            }
            if (match_keyword("BETWEEN")) {
                return parse_between(std::move(left), TaqlOp::not_between);
            }
            throw_error("expected LIKE, IN, or BETWEEN after NOT");
        }

        // Regex match
        if (current_.type == TokenType::tilde) {
            advance();
            TaqlExprNode node(ExprType::regex_expr);
            node.op = TaqlOp::regex_match;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_primary());
            return node;
        }
        if (current_.type == TokenType::not_regex) {
            advance();
            TaqlExprNode node(ExprType::regex_expr);
            node.op = TaqlOp::not_regex;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_primary());
            return node;
        }

        // IN
        if (is_keyword(current_, "IN")) {
            advance();
            TaqlExprNode node(ExprType::in_expr);
            node.op = TaqlOp::in_set;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_set_or_range());
            return node;
        }

        // BETWEEN
        if (is_keyword(current_, "BETWEEN")) {
            advance();
            return parse_between(std::move(left), TaqlOp::between);
        }

        // AROUND
        if (is_keyword(current_, "AROUND")) {
            advance();
            TaqlExprNode node(ExprType::around_expr);
            node.op = TaqlOp::around;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_bitwise_or());
            if (match_keyword("IN")) {
                node.children.push_back(parse_bitwise_or());
            }
            return node;
        }

        // INCONE
        if (is_keyword(current_, "INCONE")) {
            advance();
            TaqlExprNode node(ExprType::cone_expr);
            node.op = TaqlOp::incone;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_set_or_range());
            return node;
        }

        // EXISTS
        if (is_keyword(current_, "EXISTS")) {
            advance();
            TaqlExprNode node(ExprType::exists_expr);
            node.op = TaqlOp::exists;
            node.children.push_back(std::move(left));
            return node;
        }

        return left;
    }

    TaqlExprNode parse_between(TaqlExprNode left, TaqlOp op) {
        TaqlExprNode node(ExprType::between_expr);
        node.op = op;
        node.children.push_back(std::move(left));
        node.children.push_back(parse_bitwise_or());
        expect_keyword("AND");
        node.children.push_back(parse_bitwise_or());
        return node;
    }

    TaqlExprNode parse_set_or_range() {
        TaqlExprNode node(ExprType::set_expr);
        if (current_.type == TokenType::lbracket || current_.type == TokenType::lparen ||
            current_.type == TokenType::lbrace) {
            auto open = current_.type;
            // lparen/lbrace denote open ends; lbracket denotes closed.
            node.range_incl = (open == TokenType::lparen || open == TokenType::lbrace)
                                  ? RangeIncl::left_open
                                  : RangeIncl::closed;
            advance();
            do {
                node.children.push_back(parse_expression());
            } while (match(TokenType::comma));
            // Close bracket
            if (current_.type == TokenType::rbracket || current_.type == TokenType::rparen ||
                current_.type == TokenType::rbrace || current_.type == TokenType::gt) {
                advance();
            }
        } else {
            // Single expression as set
            node.children.push_back(parse_expression());
        }
        return node;
    }

    // Bitwise OR
    TaqlExprNode parse_bitwise_or() {
        auto left = parse_bitwise_xor();
        while (current_.type == TokenType::pipe && !peek_next_is(TokenType::pipe)) {
            advance();
            TaqlExprNode node(ExprType::binary_op);
            node.op = TaqlOp::bit_or;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_bitwise_xor());
            left = std::move(node);
        }
        return left;
    }

    TaqlExprNode parse_bitwise_xor() {
        auto left = parse_bitwise_and();
        while (match(TokenType::caret)) {
            TaqlExprNode node(ExprType::binary_op);
            node.op = TaqlOp::bit_xor;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_bitwise_and());
            left = std::move(node);
        }
        return left;
    }

    TaqlExprNode parse_bitwise_and() {
        auto left = parse_additive();
        while (current_.type == TokenType::ampersand && !peek_next_is(TokenType::ampersand)) {
            advance();
            TaqlExprNode node(ExprType::binary_op);
            node.op = TaqlOp::bit_and;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_additive());
            left = std::move(node);
        }
        return left;
    }

    // Additive: + -
    TaqlExprNode parse_additive() {
        auto left = parse_multiplicative();
        while (current_.type == TokenType::plus || current_.type == TokenType::minus) {
            auto op = (current_.type == TokenType::plus) ? TaqlOp::plus : TaqlOp::minus;
            advance();
            TaqlExprNode node(ExprType::binary_op);
            node.op = op;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_multiplicative());
            left = std::move(node);
        }
        return left;
    }

    // Multiplicative: * / // %
    TaqlExprNode parse_multiplicative() {
        auto left = parse_power();
        while (current_.type == TokenType::star || current_.type == TokenType::slash ||
               current_.type == TokenType::int_divide || current_.type == TokenType::percent) {
            TaqlOp op;
            switch (current_.type) {
            case TokenType::star: op = TaqlOp::multiply; break;
            case TokenType::slash: op = TaqlOp::divide; break;
            case TokenType::int_divide: op = TaqlOp::int_divide; break;
            case TokenType::percent: op = TaqlOp::modulo; break;
            default: op = TaqlOp::multiply; break;
            }
            advance();
            TaqlExprNode node(ExprType::binary_op);
            node.op = op;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_power());
            left = std::move(node);
        }
        return left;
    }

    // Power: ** (right-associative)
    TaqlExprNode parse_power() {
        auto left = parse_unary();
        if (current_.type == TokenType::power) {
            advance();
            TaqlExprNode node(ExprType::binary_op);
            node.op = TaqlOp::power;
            node.children.push_back(std::move(left));
            node.children.push_back(parse_power()); // right-associative
            return node;
        }
        return left;
    }

    // Unary: - + ~ !
    TaqlExprNode parse_unary() {
        if (current_.type == TokenType::minus) {
            advance();
            TaqlExprNode node(ExprType::unary_op);
            node.op = TaqlOp::negate;
            node.children.push_back(parse_postfix());
            return node;
        }
        if (current_.type == TokenType::plus) {
            advance();
            return parse_postfix();
        }
        if (current_.type == TokenType::tilde) {
            advance();
            TaqlExprNode node(ExprType::unary_op);
            node.op = TaqlOp::bit_not;
            node.children.push_back(parse_postfix());
            return node;
        }
        return parse_postfix();
    }

    // Postfix: array indexing [...]
    TaqlExprNode parse_postfix() {
        auto node = parse_primary();

        // Array subscript
        while (current_.type == TokenType::lbracket) {
            advance();
            TaqlExprNode sub(ExprType::subscript);
            sub.children.push_back(std::move(node));
            while (current_.type != TokenType::rbracket && !at_end()) {
                sub.children.push_back(parse_subscript_element());
                if (!match(TokenType::comma)) break;
            }
            expect(TokenType::rbracket);
            node = std::move(sub);
        }

        // Unit suffix (identifier after expression without space, or just identifier)
        if (current_.type == TokenType::identifier && is_unit_name(current_.text)) {
            node.unit = std::string(current_.text);
            advance();
        }

        return node;
    }

    TaqlExprNode parse_subscript_element() {
        // Could be: expr, expr:expr, expr:expr:expr, or just :
        if (current_.type == TokenType::colon) {
            // :end or :end:step
            TaqlExprNode range(ExprType::range_expr);
            advance();
            if (current_.type != TokenType::rbracket && current_.type != TokenType::comma) {
                range.children.push_back(parse_expression());
            }
            if (match(TokenType::colon)) {
                range.children.push_back(parse_expression());
            }
            return range;
        }

        auto first = parse_expression();

        if (match(TokenType::colon)) {
            TaqlExprNode range(ExprType::range_expr);
            range.children.push_back(std::move(first));
            if (current_.type != TokenType::rbracket && current_.type != TokenType::comma &&
                current_.type != TokenType::colon) {
                range.children.push_back(parse_expression());
            }
            if (match(TokenType::colon)) {
                range.children.push_back(parse_expression());
            }
            return range;
        }

        return first;
    }

    // Primary: literals, identifiers, function calls, parenthesized expressions
    TaqlExprNode parse_primary() {
        // Boolean constants
        if (is_keyword(current_, "TRUE") || is_keyword(current_, "T")) {
            advance();
            TaqlExprNode node(ExprType::literal);
            node.value = true;
            return node;
        }
        if (is_keyword(current_, "FALSE") || is_keyword(current_, "F")) {
            advance();
            TaqlExprNode node(ExprType::literal);
            node.value = false;
            return node;
        }

        // Integer literal
        if (current_.type == TokenType::integer_lit) {
            TaqlExprNode node(ExprType::literal);
            node.value = current_.int_val;
            advance();
            return node;
        }

        // Float literal (including complex)
        if (current_.type == TokenType::float_lit) {
            TaqlExprNode node(ExprType::literal);
            if (current_.str_val == "complex") { // NOLINT(bugprone-branch-clone)
                node.value = std::complex<double>(0.0, current_.float_val);
            } else {
                node.value = current_.float_val;
            }
            advance();
            return node;
        }

        // String literal
        if (current_.type == TokenType::string_lit) {
            TaqlExprNode node(ExprType::literal);
            node.value = current_.str_val;
            advance();
            return node;
        }

        // Regex literal
        if (current_.type == TokenType::regex_lit) {
            TaqlExprNode node(ExprType::literal);
            node.value = current_.str_val;
            node.type = ExprType::literal;
            advance();
            return node;
        }

        // Parenthesized expression or subquery
        if (current_.type == TokenType::lparen) {
            advance();
            auto expr = parse_expression();
            expect(TokenType::rparen);
            return expr;
        }

        // Array literal [...]
        if (current_.type == TokenType::lbracket) {
            advance();
            TaqlExprNode node(ExprType::set_expr);
            if (current_.type != TokenType::rbracket) {
                do {
                    // Check for key=value (record literal)
                    if (current_.type == TokenType::identifier) {
                        auto saved = current_;
                        advance();
                        if (current_.type == TokenType::eq) {
                            advance();
                            // Record literal
                            node.type = ExprType::record_literal;
                            TaqlExprNode pair(ExprType::literal);
                            pair.name = std::string(saved.text);
                            pair.children.push_back(parse_expression());
                            node.children.push_back(std::move(pair));
                            while (match(TokenType::comma)) {
                                TaqlExprNode p(ExprType::literal);
                                p.name = std::string(current_.text);
                                advance();
                                expect(TokenType::eq);
                                p.children.push_back(parse_expression());
                                node.children.push_back(std::move(p));
                            }
                            expect(TokenType::rbracket);
                            return node;
                        }
                        // Not a record; need to re-parse as expression
                        // This is tricky with our single-token lookahead.
                        // For now, construct a column ref from saved token
                        TaqlExprNode col_ref(ExprType::column_ref);
                        col_ref.name = std::string(saved.text);
                        node.children.push_back(std::move(col_ref));
                    } else {
                        node.children.push_back(parse_expression());
                    }
                } while (match(TokenType::comma));
            }
            expect(TokenType::rbracket);
            return node;
        }

        // IIF function
        if (is_keyword(current_, "IIF")) {
            advance();
            expect(TokenType::lparen);
            TaqlExprNode node(ExprType::iif_expr);
            node.children.push_back(parse_expression()); // condition
            expect(TokenType::comma);
            node.children.push_back(parse_expression()); // true value
            expect(TokenType::comma);
            node.children.push_back(parse_expression()); // false value
            expect(TokenType::rparen);
            return node;
        }

        // Identifier: column reference or function call
        if (current_.type == TokenType::identifier) {
            auto name = std::string(current_.text);
            advance();

            // Check for :: (keyword reference)
            if (current_.type == TokenType::double_colon) {
                advance();
                TaqlExprNode node(ExprType::keyword_ref);
                node.name = name + "::" + std::string(current_.text);
                advance();
                // Handle nested keywords (a::b.c.d)
                while (current_.type == TokenType::dot) {
                    node.name += ".";
                    advance();
                    node.name += std::string(current_.text);
                    advance();
                }
                return node;
            }

            // Function call
            if (current_.type == TokenType::lparen) {
                advance();
                TaqlExprNode node(ExprType::func_call);
                // Check if this is an aggregate function
                auto upper = to_upper(name);
                if (upper.size() > 1 && upper[0] == 'G' &&
                    (upper == "GCOUNT" || upper == "GFIRST" || upper == "GLAST" ||
                     upper == "GMIN" || upper == "GMAX" || upper == "GSUM" ||
                     upper == "GPRODUCT" || upper == "GSUMSQR" || upper == "GMEAN" ||
                     upper == "GVARIANCE" || upper == "GSTDDEV" || upper == "GRMS" ||
                     upper == "GANY" || upper == "GALL" || upper == "GNTRUE" ||
                     upper == "GNFALSE" || upper == "GHIST" || upper == "GAGGR" ||
                     upper == "GROWID" || upper == "GMEDIAN" || upper == "GFRACTILE" ||
                     upper == "GMINS" || upper == "GMAXS" || upper == "GSUMS" ||
                     upper == "GPRODUCTS" || upper == "GSUMSQRS" || upper == "GMEANS" ||
                     upper == "GVARIANCES" || upper == "GSTDDEVS" || upper == "GRMSS" ||
                     upper == "GANYS" || upper == "GALLS" || upper == "GNTRUES" ||
                     upper == "GNFALSES")) {
                    node.type = ExprType::aggregate_call;
                }
                node.name = name;
                // Arguments
                if (current_.type != TokenType::rparen) {
                    do {
                        node.children.push_back(parse_expression());
                    } while (match(TokenType::comma));
                }
                expect(TokenType::rparen);
                return node;
            }

            // Dotted name (table.column or UDF function like MEAS.EPOCH)
            if (current_.type == TokenType::dot) {
                std::string dotted = name;
                while (match(TokenType::dot)) {
                    dotted += ".";
                    dotted += std::string(current_.text);
                    advance();
                }
                // If followed by '(', it is a dotted function call (UDF)
                if (current_.type == TokenType::lparen) {
                    advance();
                    TaqlExprNode node(ExprType::func_call);
                    node.name = dotted;
                    if (current_.type != TokenType::rparen) {
                        do {
                            node.children.push_back(parse_expression());
                        } while (match(TokenType::comma));
                    }
                    expect(TokenType::rparen);
                    return node;
                }
                TaqlExprNode node(ExprType::column_ref);
                node.name = dotted;
                return node;
            }

            // Simple column reference
            TaqlExprNode node(ExprType::column_ref);
            node.name = name;
            return node;
        }

        // Star (wildcard projection)
        if (current_.type == TokenType::star) {
            advance();
            TaqlExprNode node(ExprType::wildcard);
            return node;
        }

        throw_error("expected expression");
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    bool peek_next_is(TokenType type) {
        // One-token lookahead for disambiguating & vs && and | vs ||.
        // We snapshot the lexer, grab one token, then restore.
        auto saved_pos = lexer_;
        auto tok = lexer_.next();
        lexer_ = saved_pos;
        return tok.type == type;
    }

    bool is_select_clause_keyword(std::string_view text) const {
        static const std::unordered_set<std::string> keywords = {
            "FROM",   "WHERE",  "GROUPBY", "GROUP",   "HAVING",    "ORDERBY",
            "ORDER",  "LIMIT",  "OFFSET",  "GIVING",  "INTO",      "DMINFO",
            "JOIN",   "ON",     "AS",      "ASC",     "DESC",      "DISTINCT",
            "NODUPL", "ALL",    "SET",     "VALUES",  "VALUE",     "SELECT",
            "UPDATE", "INSERT", "DELETE",  "COUNT",   "CALC",      "CREATE",
            "ALTER",  "DROP",   "SHOW",    "BETWEEN", "AND",       "OR",
            "NOT",    "IN",     "LIKE",    "ILIKE",   "EXISTS",    "BY",
            "ROLLUP", "ADDCOL", "DROPCOL", "COPYCOL", "RENAMECOL", "SETKEY",
            "COPYKEY","RENAMEKEY","DROPKEY","ADDROW",  "TIMING",    "STYLE",
            "WITH",   "TABLE",  "ADD",     "RENAME",    "COPY"};
        auto upper = to_upper(text);
        return keywords.count(upper) > 0;
    }

    bool is_unit_name(std::string_view text) const {
        // Common TaQL unit suffixes
        static const std::unordered_set<std::string> units = {
            "DEG", "RAD", "ARCSEC", "ARCMIN", "MAS", "HMS", "DMS",
            "M", "KM", "CM", "MM", "AU", "PC", "KPC", "MPC",
            "S", "MS", "US", "NS", "MIN", "H", "D",
            "HZ", "KHZ", "MHZ", "GHZ",
            "JY", "MJY", "KJY",
            "K", "MK",
            "M_S", "KM_S",
            "PA", "MBAR",
            "LAMBDA"};
        auto upper = to_upper(text);
        return units.count(upper) > 0;
    }

    std::string_view remaining_source() const {
        if (current_.loc.offset < source_.size()) {
            return source_.substr(current_.loc.offset);
        }
        return {};
    }
};

// ===========================================================================
// Public API implementations
// ===========================================================================

TaqlAst taql_parse(std::string_view query) {
    TaqlParser parser(query);
    return parser.parse();
}

std::string format_parse_error(const TaqlParseError& error, std::string_view source) {
    std::string result = "TaQL parse error at line " + std::to_string(error.location.line) +
                         " column " + std::to_string(error.location.column) + ": " +
                         error.message;
    if (!error.token.empty()) {
        result += " (token: '" + error.token + "')";
    }
    // Show source context
    if (error.location.offset < source.size()) {
        auto line_start = source.rfind('\n', error.location.offset);
        if (line_start == std::string_view::npos) line_start = 0;
        else ++line_start;
        auto line_end = source.find('\n', error.location.offset);
        if (line_end == std::string_view::npos) line_end = source.size();
        result += "\n  " + std::string(source.substr(line_start, line_end - line_start));
        result += "\n  " + std::string(error.location.offset - line_start, ' ') + "^";
    }
    return result;
}

std::string taql_show(std::string_view topic) {
    auto upper = to_upper(topic);

    if (upper.empty() || upper == "HELP") {
        return "TaQL - Table Query Language\n"
               "Commands: SELECT, UPDATE, INSERT, DELETE, COUNT, CALC,\n"
               "          CREATE TABLE, ALTER TABLE, DROP TABLE, SHOW\n"
               "Topics: SHOW commands | expressions | operators | functions |\n"
               "        sets | constants | datatypes | tableoptions | dminfo |\n"
               "        meastypes | units | all\n";
    }
    if (upper == "COMMANDS") {
        return "TaQL Commands:\n"
               "  SELECT [DISTINCT] expr_list FROM table [JOIN ...] [WHERE ...]\n"
               "         [GROUPBY ...] [HAVING ...] [ORDERBY ...] [LIMIT ...]\n"
               "  UPDATE table SET col=expr,... [WHERE ...]\n"
               "  INSERT INTO table [(cols)] VALUES (vals),...\n"
               "  DELETE FROM table [WHERE ...]\n"
               "  COUNT [cols] FROM table [WHERE ...]\n"
               "  CALC expr [FROM table]\n"
               "  CREATE TABLE name [AS options] [LIKE tbl] (col_defs)\n"
               "  ALTER TABLE name ADD|DROP|RENAME|COPY COLUMN ...\n"
               "  DROP TABLE name\n"
               "  SHOW [topic]\n";
    }
    if (upper == "EXPRESSIONS") {
        return "TaQL Expressions:\n"
               "  Arithmetic: + - * / // % **\n"
               "  Comparison: = == != <> < > <= >= ~= !~=\n"
               "  Logical: AND OR NOT && || !\n"
               "  Bitwise: & | ^ ~\n"
               "  Pattern: LIKE ILIKE ~ (regex) !~ (not regex)\n"
               "  Range: IN BETWEEN AROUND INCONE\n"
               "  Special: EXISTS (subquery)\n"
               "  Conditional: IIF(cond, true, false)\n";
    }
    if (upper == "OPERATORS") {
        return "TaQL Operators (precedence high to low):\n"
               "  ** (power, right-assoc)\n"
               "  - + ~ (unary)\n"
               "  * / // %\n"
               "  + - (binary)\n"
               "  & (bitwise AND)\n"
               "  ^ (bitwise XOR)\n"
               "  | (bitwise OR)\n"
               "  = != < > <= >= ~= !~= LIKE ILIKE IN BETWEEN\n"
               "  NOT\n"
               "  AND\n"
               "  OR\n";
    }
    if (upper == "FUNCTIONS" || upper == "ALL") {
        return "TaQL Functions:\n"
               "  Math: sin cos tan asin acos atan atan2 exp log log10 sqrt pow\n"
               "        abs sign round floor ceil fmod square cube min max\n"
               "  Complex: conj real imag complex norm arg\n"
               "  String: strlen upcase downcase capitalize trim ltrim rtrim\n"
               "          substr replace sreverse\n"
               "  Pattern: regex pattern sqlpattern\n"
               "  DateTime: datetime mjdtodate mjd date time year month day\n"
               "            cmonth weekday cdow week ctod cdate ctime\n"
               "  Angle: hms dms hdms normangle angdist angdistx\n"
               "  Test: isnan isinf isfinite isdef isnull iscolumn iskeyword\n"
               "  Array: ndim nelem shape array transpose areverse resize\n"
               "         diagonal arrflat\n"
               "  Reduction: arrsum arrmin arrmax arrmean arrmedian\n"
               "             arrvariance arrstddev arrrms arravdev arrfractile\n"
               "  Boolean: arrany arrall arrnTrue arrnFalse\n"
               "  Aggregate: gcount gmin gmax gsum gmean gvariance gstddev\n"
               "             gmedian gfractile gany gall gntrue gnfalse\n"
               "  Misc: iif rand rownr rowid near nearabs\n"
               "  Constants: pi() e() c()\n";
    }
    if (upper == "CONSTANTS") {
        return "TaQL Constants:\n"
               "  Boolean: TRUE FALSE T F\n"
               "  Integer: 123 0xFF\n"
               "  Double:  1.5 1.5e3 1.5deg 1.5rad\n"
               "  Complex: 1.5+2.3i 1.5-2.3j\n"
               "  String:  'text' \"text\"\n"
               "  DateTime: 2024/01/15 12:30:45.5\n"
               "  Regex:  p/pattern/ m/pattern/ f/pattern/\n"
               "  Array:  [1,2,3]\n";
    }
    if (upper == "DATATYPES") {
        return "TaQL Data Types:\n"
               "  BOOL INT FLOAT DOUBLE COMPLEX DCOMPLEX STRING\n";
    }
    if (upper == "SETS") {
        return "TaQL Sets and Ranges:\n"
               "  [a, b, c]          discrete set\n"
               "  [a =:= b]         closed interval\n"
               "  [a =:< b]         left-closed, right-open\n"
               "  [a <:= b]         left-open, right-closed\n"
               "  [a <:< b]         open interval\n"
               "  expr IN set        membership test\n"
               "  expr BETWEEN a AND b  closed interval\n";
    }
    if (upper == "TABLEOPTIONS") {
        return "TaQL Table Options (AS clause):\n"
               "  TYPE PLAIN|MEMORY|SCRATCH\n"
               "  ENDIAN BIG|LITTLE|LOCAL|AIPSRC\n"
               "  STORAGE SEPFILE|MULTIFILE|MULTIHDF5\n"
               "  OVERWRITE\n";
    }
    if (upper == "UNITS") {
        return "TaQL Units:\n"
               "  Angle: deg rad arcsec arcmin mas\n"
               "  Time: s ms us ns min h d\n"
               "  Length: m km cm mm au pc kpc mpc\n"
               "  Frequency: Hz kHz MHz GHz\n"
               "  Flux: Jy mJy kJy\n"
               "  Velocity: m/s km/s\n";
    }
    return "Unknown SHOW topic: '" + std::string(topic) + "'. Use SHOW for overview.\n";
}

// ===========================================================================
// TaQL expression evaluator
// ===========================================================================

namespace {

/// Convert a CellValue (from Table) to a TaqlValue.
TaqlValue cell_to_taql(const CellValue& cv) {
    return std::visit(
        [](auto&& v) -> TaqlValue {
            using T = std::decay_t<decltype(v)>; // NOLINT(readability-identifier-naming)
            if constexpr (std::is_same_v<T, bool> ||
                          std::is_same_v<T, std::int64_t> ||
                          std::is_same_v<T, double> ||
                          std::is_same_v<T, std::complex<double>> ||
                          std::is_same_v<T, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::int32_t> ||
                                 std::is_same_v<T, std::uint32_t>) {
                return static_cast<std::int64_t>(v);
            } else if constexpr (std::is_same_v<T, float>) {
                return static_cast<double>(v);
            } else if constexpr (std::is_same_v<T, std::complex<float>>) {
                return std::complex<double>(v.real(), v.imag());
            } else {
                return std::monostate{};
            }
        },
        cv);
}

/// Extract a double from a TaqlValue, promoting integers.
double as_double(const TaqlValue& v) {
    if (auto* d = std::get_if<double>(&v)) return *d;
    if (auto* i = std::get_if<std::int64_t>(&v)) return static_cast<double>(*i);
    if (auto* b = std::get_if<bool>(&v)) return *b ? 1.0 : 0.0;
    throw std::runtime_error("TaQL: cannot convert value to double");
}

/// Extract an int64 from a TaqlValue.
std::int64_t as_int(const TaqlValue& v) {
    if (auto* i = std::get_if<std::int64_t>(&v)) return *i;
    if (auto* b = std::get_if<bool>(&v)) return *b ? 1 : 0;
    if (auto* d = std::get_if<double>(&v)) return static_cast<std::int64_t>(*d);
    throw std::runtime_error("TaQL: cannot convert value to integer");
}

/// Extract a bool from a TaqlValue.
bool as_bool(const TaqlValue& v) {
    if (auto* b = std::get_if<bool>(&v)) return *b;
    if (auto* i = std::get_if<std::int64_t>(&v)) return *i != 0;
    if (auto* d = std::get_if<double>(&v)) return *d != 0.0;
    throw std::runtime_error("TaQL: cannot convert value to bool");
}

/// Extract a string from a TaqlValue.
std::string as_string(const TaqlValue& v) {
    if (auto* s = std::get_if<std::string>(&v)) return *s;
    if (auto* i = std::get_if<std::int64_t>(&v)) return std::to_string(*i);
    if (auto* d = std::get_if<double>(&v)) return std::to_string(*d);
    if (auto* b = std::get_if<bool>(&v)) return *b ? "true" : "false";
    throw std::runtime_error("TaQL: cannot convert value to string");
}

/// Extract a complex from a TaqlValue, promoting real types.
std::complex<double> as_complex(const TaqlValue& v) {
    if (auto* c = std::get_if<std::complex<double>>(&v)) return *c;
    if (auto* d = std::get_if<double>(&v)) return {*d, 0.0};
    if (auto* i = std::get_if<std::int64_t>(&v)) return {static_cast<double>(*i), 0.0};
    if (auto* b = std::get_if<bool>(&v)) return {*b ? 1.0 : 0.0, 0.0};
    throw std::runtime_error("TaQL: cannot convert value to complex");
}

/// Check if a TaqlValue is an array type.
bool is_array_value(const TaqlValue& v) {
    return std::holds_alternative<std::vector<bool>>(v) ||
           std::holds_alternative<std::vector<std::int64_t>>(v) ||
           std::holds_alternative<std::vector<double>>(v) ||
           std::holds_alternative<std::vector<std::complex<double>>>(v) ||
           std::holds_alternative<std::vector<std::string>>(v);
}

/// Get array element count.
std::int64_t array_nelem(const TaqlValue& v) {
    if (auto* a = std::get_if<std::vector<bool>>(&v))
        return static_cast<std::int64_t>(a->size());
    if (auto* a = std::get_if<std::vector<std::int64_t>>(&v))
        return static_cast<std::int64_t>(a->size());
    if (auto* a = std::get_if<std::vector<double>>(&v))
        return static_cast<std::int64_t>(a->size());
    if (auto* a = std::get_if<std::vector<std::complex<double>>>(&v))
        return static_cast<std::int64_t>(a->size());
    if (auto* a = std::get_if<std::vector<std::string>>(&v))
        return static_cast<std::int64_t>(a->size());
    return std::int64_t{0};
}

/// Extract array as vector<double>, promoting ints.
std::vector<double> as_double_array(const TaqlValue& v) {
    if (auto* a = std::get_if<std::vector<double>>(&v)) return *a;
    if (auto* a = std::get_if<std::vector<std::int64_t>>(&v)) {
        std::vector<double> r(a->size());
        for (std::size_t i = 0; i < a->size(); ++i)
            r[i] = static_cast<double>((*a)[i]);
        return r;
    }
    if (auto* a = std::get_if<std::vector<bool>>(&v)) {
        std::vector<double> r(a->size());
        for (std::size_t i = 0; i < a->size(); ++i)
            r[i] = (*a)[i] ? 1.0 : 0.0;
        return r;
    }
    // Single scalar -> 1-element array
    return {as_double(v)};
}

/// Read an array column cell and return as TaqlValue (vector<double> or vector<complex>).
TaqlValue read_array_cell_as_taql(Table& table, std::string_view col_name, std::uint64_t row) {
    const auto* cd = table.find_column_desc(col_name);
    if (cd == nullptr)
        throw std::runtime_error("TaQL: column '" + std::string(col_name) + "' not found");
    switch (cd->data_type) {
    case DataType::tp_double: {
        auto arr = table.read_array_double_cell(col_name, row);
        return TaqlValue{std::move(arr)};
    }
    case DataType::tp_float: {
        auto arr = table.read_array_float_cell(col_name, row);
        std::vector<double> res(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i)
            res[i] = static_cast<double>(arr[i]);
        return TaqlValue{std::move(res)};
    }
    case DataType::tp_int: case DataType::tp_short: case DataType::tp_int64: {
        auto arr = table.read_array_int_cell(col_name, row);
        std::vector<std::int64_t> res(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i)
            res[i] = static_cast<std::int64_t>(arr[i]);
        return TaqlValue{std::move(res)};
    }
    case DataType::tp_complex: {
        auto arr = table.read_array_complex_cell(col_name, row);
        std::vector<std::complex<double>> res(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i)
            res[i] = {static_cast<double>(arr[i].real()), static_cast<double>(arr[i].imag())};
        return TaqlValue{std::move(res)};
    }
    case DataType::tp_dcomplex: {
        auto arr = table.read_array_dcomplex_cell(col_name, row);
        return TaqlValue{std::move(arr)};
    }
    case DataType::tp_bool: {
        auto arr = table.read_array_bool_cell(col_name, row);
        return TaqlValue{std::move(arr)};
    }
    case DataType::tp_string: {
        auto arr = table.read_array_string_cell(col_name, row);
        return TaqlValue{std::move(arr)};
    }
    default:
        throw std::runtime_error("TaQL: unsupported array column data type for '" +
                                 std::string(col_name) + "'");
    }
}

/// Convert unit suffix value using unit.hpp.
double convert_unit(double val, const std::string& from_unit, const std::string& to_unit) {
    if (from_unit.empty() || to_unit.empty() || from_unit == to_unit)
        return val;
    // Use casacore_mini unit conversion
    Quantity q{val, from_unit};
    return q.get_value(to_unit);
}

/// Parse a datetime string to MJD seconds.
/// Supports: YYYY/MM/DD, YYYY/MM/DD/HH:MM:SS, YYYY-MM-DDTHH:MM:SS
double parse_datetime_to_mjd(const std::string& s) {
    // MJD epoch: 1858-11-17 00:00:00 UTC
    // Julian day 2400000.5
    int year = 0, month = 0, day = 0, hour = 0, min = 0;
    double sec = 0.0;

    // Try YYYY/MM/DD/HH:MM:SS.sss, then ISO, then date-only (date-only is
    // a subset of the first pattern, so the first sscanf already covers it).
    if (std::sscanf(s.c_str(), "%d/%d/%d/%d:%d:%lf", &year, &month, &day, &hour, &min, &sec) < 3 &&
        std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%lf", &year, &month, &day, &hour, &min, &sec) < 3) {
        throw std::runtime_error("TaQL: cannot parse datetime '" + s + "'");
    }

    // Convert to MJD using algorithm from Meeus (Astronomical Algorithms)
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    // Julian day number
    double jdn = static_cast<double>(day) +
                 std::floor((153.0 * m + 2.0) / 5.0) +
                 365.0 * static_cast<double>(y) +
                 std::floor(static_cast<double>(y) / 4.0) -
                 std::floor(static_cast<double>(y) / 100.0) +
                 std::floor(static_cast<double>(y) / 400.0) - 32045.0;
    double jd = jdn + (static_cast<double>(hour) - 12.0) / 24.0 +
                static_cast<double>(min) / 1440.0 + sec / 86400.0;
    return jd - 2400000.5; // MJD
}

/// Format MJD to datetime string.
std::string mjd_to_datetime(double mjd) {
    double jd = mjd + 2400000.5;
    // Convert JD to calendar date (Meeus algorithm)
    auto z = static_cast<std::int64_t>(std::lround(jd));
    double f = jd + 0.5 - static_cast<double>(z);
    std::int64_t a_val;
    if (z < 2299161) {
        a_val = z;
    } else {
        auto alpha = static_cast<std::int64_t>((static_cast<double>(z) - 1867216.25) / 36524.25);
        a_val = z + 1 + alpha - alpha / 4;
    }
    auto b = a_val + 1524;
    auto c = static_cast<std::int64_t>((static_cast<double>(b) - 122.1) / 365.25);
    auto d = static_cast<std::int64_t>(365.25 * static_cast<double>(c));
    auto e = static_cast<std::int64_t>(static_cast<double>(b - d) / 30.6001);

    int day_val = static_cast<int>(b - d - static_cast<std::int64_t>(30.6001 * static_cast<double>(e)));
    int month_val = (e < 14) ? static_cast<int>(e - 1) : static_cast<int>(e - 13);
    int year_val = (month_val > 2) ? static_cast<int>(c - 4716) : static_cast<int>(c - 4715);

    double day_frac = f * 24.0;
    int hour_val = static_cast<int>(day_frac);
    double min_frac = (day_frac - hour_val) * 60.0;
    int min_val = static_cast<int>(min_frac);
    double sec_val = (min_frac - min_val) * 60.0;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d/%02d/%02d/%02d:%02d:%06.3f",
                  year_val, month_val, day_val, hour_val, min_val, sec_val);
    return buf;
}

/// Compare two TaqlValues numerically.
int compare_values(const TaqlValue& a, const TaqlValue& b) {
    if (std::holds_alternative<std::string>(a) || std::holds_alternative<std::string>(b)) {
        auto sa = as_string(a);
        auto sb = as_string(b);
        return sa < sb ? -1 : (sa > sb ? 1 : 0);
    }
    double da = as_double(a);
    double db = as_double(b);
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/// Row evaluation context.
struct EvalContext {
    Table* table = nullptr;
    std::uint64_t row = 0;
    bool has_row = false;
    const std::vector<std::uint64_t>* group_rows = nullptr; // for aggregate eval
    // Multi-table support (JOIN)
    std::unordered_map<std::string, std::pair<Table*, std::uint64_t>>* joined_tables = nullptr;
};

/// Evaluate a single expression node within a row context.
TaqlValue eval_expr(const TaqlExprNode& node, const EvalContext& ctx);

/// Evaluate a math built-in function.
TaqlValue eval_math_func(const std::string& name, const std::vector<TaqlValue>& args,
                         const EvalContext& ctx) {
    auto upper = to_upper(name);

    // rownr() / rowid() — return current row number from context (0 or more args)
    if (upper == "ROWNR" || upper == "ROWID") {
        return static_cast<std::int64_t>(ctx.row);
    }

    // rand() — return random double in [0, 1)
    if (upper == "RAND") {
        static std::mt19937 gen{std::random_device{}()};  // NOLINT(cert-msc51-cpp)
        static std::uniform_real_distribution<double> dist(0.0, 1.0);
        return TaqlValue{dist(gen)};
    }

    if (args.empty()) {
        if (upper == "PI") return TaqlValue{static_cast<double>(3.14159265358979323846L)};
        if (upper == "E") return TaqlValue{static_cast<double>(2.71828182845904523536L)};
        if (upper == "C") return TaqlValue{299792458.0};
        throw std::runtime_error("TaQL: unknown zero-arg function '" + name + "'");
    }

    if (args.size() == 1) {
        // String functions (check before as_double to avoid type error)
        if (upper == "STRLEN") return static_cast<std::int64_t>(as_string(args[0]).size());
        if (upper == "UPCASE" || upper == "UPPER") {
            auto s = as_string(args[0]);
            for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }
        if (upper == "DOWNCASE" || upper == "LOWER") {
            auto s = as_string(args[0]);
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }
        if (upper == "TRIM") {
            auto s = as_string(args[0]);
            auto start = s.find_first_not_of(" \t\n\r");
            auto end = s.find_last_not_of(" \t\n\r");
            if (start == std::string::npos) return std::string{};
            return s.substr(start, end - start + 1);
        }
        if (upper == "LTRIM") {
            auto s = as_string(args[0]);
            auto start = s.find_first_not_of(" \t\n\r");
            if (start == std::string::npos) return std::string{};
            return s.substr(start);
        }
        if (upper == "RTRIM") {
            auto s = as_string(args[0]);
            auto end = s.find_last_not_of(" \t\n\r");
            if (end == std::string::npos) return std::string{};
            return s.substr(0, end + 1);
        }
        if (upper == "CAPITALIZE") {
            auto s = as_string(args[0]);
            bool next_upper = true;
            for (auto& c : s) {
                auto uc = static_cast<unsigned char>(c);
                if (std::isalpha(uc) != 0) {
                    c = next_upper ? static_cast<char>(std::toupper(uc))
                                   : static_cast<char>(std::tolower(uc));
                    next_upper = false;
                } else {
                    next_upper = (c == ' ' || c == '\t');
                }
            }
            return s;
        }
        if (upper == "SREVERSE") {
            auto s = as_string(args[0]);
            std::reverse(s.begin(), s.end());
            return s;
        }

        // Predicate functions that need table context
        if (upper == "ISCOLUMN") {
            if (ctx.table == nullptr) return TaqlValue{false};
            auto col_name = as_string(args[0]);
            for (auto& col : ctx.table->columns()) {
                if (col.name == col_name) return TaqlValue{true};
            }
            return TaqlValue{false};
        }
        if (upper == "ISKEYWORD") {
            if (ctx.table == nullptr) return TaqlValue{false};
            auto key_name = as_string(args[0]);
            return TaqlValue{ctx.table->keywords().find(key_name) != nullptr};
        }
        if (upper == "ISDEF") {
            // isdef checks if a column value is defined (non-null).
            // For scalar cells, they are always defined.
            return TaqlValue{true};
        }
        if (upper == "ISNULL") {
            return TaqlValue{std::holds_alternative<std::monostate>(args[0])};
        }

        // Array info functions (must check before as_double which rejects vectors)
        if (upper == "NDIM") {
            if (is_array_value(args[0])) return std::int64_t{1};
            return std::int64_t{0};
        }
        if (upper == "NELEM" || upper == "NELEMENTS") {
            return array_nelem(args[0]);
        }
        if (upper == "SHAPE") {
            if (is_array_value(args[0]))
                return std::vector<std::int64_t>{array_nelem(args[0])};
            return std::vector<std::int64_t>{};
        }
        // 1-arg array reductions (must also be before as_double)
        if (upper == "ARRSUM" || upper == "SUM") {
            auto arr = as_double_array(args[0]);
            double sum = 0.0;
            for (auto v : arr) sum += v;
            return TaqlValue{sum};
        }
        if (upper == "ARRMIN") {
            auto arr = as_double_array(args[0]);
            if (arr.empty()) return std::monostate{};
            return TaqlValue{*std::min_element(arr.begin(), arr.end())};
        }
        if (upper == "ARRMAX") {
            auto arr = as_double_array(args[0]);
            if (arr.empty()) return std::monostate{};
            return TaqlValue{*std::max_element(arr.begin(), arr.end())};
        }
        if (upper == "ARRMEAN" || upper == "MEAN") {
            auto arr = as_double_array(args[0]);
            if (arr.empty()) return TaqlValue{0.0};
            double sum = 0.0;
            for (auto v : arr) sum += v;
            return TaqlValue{sum / static_cast<double>(arr.size())};
        }
        if (upper == "ARRMEDIAN" || upper == "MEDIAN") {
            auto arr = as_double_array(args[0]);
            if (arr.empty()) return TaqlValue{0.0};
            std::sort(arr.begin(), arr.end());
            auto n = arr.size();
            if (n % 2 == 0)
                return TaqlValue{(arr[n / 2 - 1] + arr[n / 2]) / 2.0};
            return TaqlValue{arr[n / 2]};
        }
        if (upper == "ARRVARIANCE" || upper == "VARIANCE") {
            auto arr = as_double_array(args[0]);
            if (arr.size() < 2) return TaqlValue{0.0};
            double mean_val = 0.0;
            for (auto v : arr) mean_val += v;
            mean_val /= static_cast<double>(arr.size());
            double var = 0.0;
            for (auto v : arr) var += (v - mean_val) * (v - mean_val);
            return TaqlValue{var / static_cast<double>(arr.size() - 1)};
        }
        if (upper == "ARRSTDDEV" || upper == "STDDEV") {
            auto arr = as_double_array(args[0]);
            if (arr.size() < 2) return TaqlValue{0.0};
            double mean_val = 0.0;
            for (auto v : arr) mean_val += v;
            mean_val /= static_cast<double>(arr.size());
            double var = 0.0;
            for (auto v : arr) var += (v - mean_val) * (v - mean_val);
            return TaqlValue{std::sqrt(var / static_cast<double>(arr.size() - 1))};
        }
        if (upper == "ARRRMS" || upper == "RMS") {
            auto arr = as_double_array(args[0]);
            if (arr.empty()) return TaqlValue{0.0};
            double sum_sq = 0.0;
            for (auto v : arr) sum_sq += v * v;
            return TaqlValue{std::sqrt(sum_sq / static_cast<double>(arr.size()))};
        }
        if (upper == "ARRAVDEV") {
            auto arr = as_double_array(args[0]);
            if (arr.empty()) return TaqlValue{0.0};
            double mean_val = 0.0;
            for (auto v : arr) mean_val += v;
            mean_val /= static_cast<double>(arr.size());
            double avdev = 0.0;
            for (auto v : arr) avdev += std::abs(v - mean_val);
            return TaqlValue{avdev / static_cast<double>(arr.size())};
        }
        if (upper == "ARRANY" || upper == "ANY") {
            if (auto* a = std::get_if<std::vector<bool>>(&args[0])) {
                bool r = std::any_of(a->begin(), a->end(), [](bool b) { return b; });
                return TaqlValue{r};
            }
            auto arr = as_double_array(args[0]);
            bool r = std::any_of(arr.begin(), arr.end(), [](double v) { return v != 0.0; });
            return TaqlValue{r};
        }
        if (upper == "ARRALL" || upper == "ALL") {
            if (auto* a = std::get_if<std::vector<bool>>(&args[0])) {
                bool r = std::all_of(a->begin(), a->end(), [](bool b) { return b; });
                return TaqlValue{r};
            }
            auto arr = as_double_array(args[0]);
            bool r = std::all_of(arr.begin(), arr.end(), [](double v) { return v != 0.0; });
            return TaqlValue{r};
        }
        if (upper == "ARRNTRUE" || upper == "NTRUE") {
            if (auto* a = std::get_if<std::vector<bool>>(&args[0])) {
                auto cnt = std::count(a->begin(), a->end(), true);
                return static_cast<std::int64_t>(cnt);
            }
            auto arr = as_double_array(args[0]);
            auto cnt = std::count_if(arr.begin(), arr.end(), [](double v) { return v != 0.0; });
            return static_cast<std::int64_t>(cnt);
        }
        if (upper == "ARRNFALSE" || upper == "NFALSE") {
            if (auto* a = std::get_if<std::vector<bool>>(&args[0])) {
                auto cnt = std::count(a->begin(), a->end(), false);
                return static_cast<std::int64_t>(cnt);
            }
            auto arr = as_double_array(args[0]);
            auto cnt = std::count_if(arr.begin(), arr.end(), [](double v) { return v == 0.0; });
            return static_cast<std::int64_t>(cnt);
        }
        // 1-arg array manipulation
        if (upper == "TRANSPOSE" || upper == "DIAGONAL" || upper == "ARRFLAT" || upper == "FLATTEN") {
            return args[0]; // identity for 1-D
        }
        if (upper == "AREVERSE") {
            if (auto* a = std::get_if<std::vector<double>>(&args[0])) {
                auto result = *a;
                std::reverse(result.begin(), result.end());
                return TaqlValue{std::move(result)};
            }
            if (auto* a = std::get_if<std::vector<std::int64_t>>(&args[0])) {
                auto result = *a;
                std::reverse(result.begin(), result.end());
                return TaqlValue{std::move(result)};
            }
            return args[0];
        }
        // 1-arg complex functions
        if (upper == "CONJ") {
            auto c = as_complex(args[0]);
            return TaqlValue{std::conj(c)};
        }
        // 1-arg datetime functions
        if (upper == "DATETIME" || upper == "CTOD") {
            return TaqlValue{parse_datetime_to_mjd(as_string(args[0]))};
        }
        if (upper == "MJDTODATE" || upper == "MJD") {
            return TaqlValue{mjd_to_datetime(as_double(args[0]))};
        }
        if (upper == "NORMANGLE") {
            constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
            double rad = as_double(args[0]);
            rad = std::fmod(rad, kTwoPi);
            if (rad < 0) rad += kTwoPi;
            return TaqlValue{rad};
        }
        if (upper == "HMS") {
            double rad = as_double(args[0]);
            double hours = rad * 12.0 / 3.14159265358979323846;
            if (hours < 0) hours += 24.0;
            auto h = static_cast<int>(hours);
            double mf = (hours - h) * 60.0;
            auto m = static_cast<int>(mf);
            double s = (mf - m) * 60.0;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%06.3f", h, m, s);
            return TaqlValue{std::string(buf)};
        }
        if (upper == "DMS") {
            double rad = as_double(args[0]);
            double deg = rad * 180.0 / 3.14159265358979323846;
            char sign = '+';
            if (deg < 0) { sign = '-'; deg = -deg; }
            auto d = static_cast<int>(deg);
            double mf = (deg - d) * 60.0;
            auto m = static_cast<int>(mf);
            double s = (mf - m) * 60.0;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%c%02d.%02d.%06.3f", sign, d, m, s);
            return TaqlValue{std::string(buf)};
        }
        // 1-arg datetime functions that take MJD double
        if (upper == "DATE" || upper == "CDATE") {
            return TaqlValue{mjd_to_datetime(as_double(args[0])).substr(0, 10)};
        }
        if (upper == "TIME" || upper == "CTIME") {
            auto dt = mjd_to_datetime(as_double(args[0]));
            if (dt.size() > 11) return TaqlValue{dt.substr(11)};
            return TaqlValue{std::string("00:00:00.000")};
        }
        if (upper == "YEAR") {
            auto dt = mjd_to_datetime(as_double(args[0]));
            return static_cast<std::int64_t>(std::stoi(dt.substr(0, 4)));
        }
        if (upper == "MONTH") {
            auto dt = mjd_to_datetime(as_double(args[0]));
            return static_cast<std::int64_t>(std::stoi(dt.substr(5, 2)));
        }
        if (upper == "CMONTH") {
            auto dt = mjd_to_datetime(as_double(args[0]));
            auto m = std::stoi(dt.substr(5, 2));
            static const char* const months[] = {
                "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            return TaqlValue{std::string(months[m])};
        }
        if (upper == "DAY") {
            auto dt = mjd_to_datetime(as_double(args[0]));
            return static_cast<std::int64_t>(std::stoi(dt.substr(8, 2)));
        }
        if (upper == "WEEKDAY" || upper == "DOW") {
            auto mjd = as_double(args[0]);
            auto day_of_week = (static_cast<std::int64_t>(mjd) + 3) % 7;
            if (day_of_week < 0) day_of_week += 7;
            return static_cast<std::int64_t>(day_of_week);
        }
        if (upper == "CDOW") {
            auto mjd = as_double(args[0]);
            auto day_of_week = (static_cast<std::int64_t>(mjd) + 3) % 7;
            if (day_of_week < 0) day_of_week += 7;
            static const char* const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            return TaqlValue{std::string(days[day_of_week])};
        }
        if (upper == "WEEK") {
            auto dt = mjd_to_datetime(as_double(args[0]));
            auto y = std::stoi(dt.substr(0, 4));
            auto m_val = std::stoi(dt.substr(5, 2));
            auto d = std::stoi(dt.substr(8, 2));
            int doy = d;
            static const int days_before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
            if (m_val >= 1 && m_val <= 12) doy += days_before[m_val - 1];
            bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
            if (m_val > 2 && leap) ++doy;
            return static_cast<std::int64_t>((doy - 1) / 7 + 1);
        }

        // Pattern/regex passthrough
        if (upper == "REGEX" || upper == "PATTERN" || upper == "SQLPATTERN") {
            return args[0];
        }

        // Complex-aware functions (must be BEFORE as_double which rejects complex)
        if (upper == "REAL") {
            if (auto* c = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{c->real()};
            return TaqlValue{as_double(args[0])};
        }
        if (upper == "IMAG") {
            if (auto* c = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{c->imag()};
            return TaqlValue{0.0};
        }
        if (upper == "NORM") {
            if (auto* c = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{std::norm(*c)};
            auto xn = as_double(args[0]);
            return TaqlValue{xn * xn};
        }
        if (upper == "ARG") {
            if (auto* c = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{std::arg(*c)};
            return TaqlValue{0.0};
        }
        if (upper == "ABS") {
            if (auto* c = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{std::abs(*c)};
            return TaqlValue{std::abs(as_double(args[0]))};
        }
        if (upper == "STRING") return as_string(args[0]);

        // Numeric functions (require scalar double - must be AFTER array/complex/string checks)
        double x = as_double(args[0]);
        if (upper == "SIN") return TaqlValue{std::sin(x)};
        if (upper == "COS") return TaqlValue{std::cos(x)};
        if (upper == "TAN") return TaqlValue{std::tan(x)};
        if (upper == "ASIN") return TaqlValue{std::asin(x)};
        if (upper == "ACOS") return TaqlValue{std::acos(x)};
        if (upper == "ATAN") return TaqlValue{std::atan(x)};
        if (upper == "EXP") return TaqlValue{std::exp(x)};
        if (upper == "LOG" || upper == "LN") return TaqlValue{std::log(x)};
        if (upper == "LOG10") return TaqlValue{std::log10(x)};
        if (upper == "SQRT") return TaqlValue{std::sqrt(x)};
        if (upper == "SIGN") return TaqlValue{(x > 0.0) ? 1.0 : ((x < 0.0) ? -1.0 : 0.0)};
        if (upper == "ROUND") return TaqlValue{std::round(x)};
        if (upper == "FLOOR") return TaqlValue{std::floor(x)};
        if (upper == "CEIL") return TaqlValue{std::ceil(x)};
        if (upper == "SQUARE") return TaqlValue{x * x};
        if (upper == "CUBE") return TaqlValue{x * x * x};
        if (upper == "ISNAN") { bool r = std::isnan(x); return TaqlValue{r}; }
        if (upper == "ISINF") { bool r = std::isinf(x); return TaqlValue{r}; }
        if (upper == "ISFINITE") { bool r = std::isfinite(x); return TaqlValue{r}; }
        if (upper == "INT" || upper == "INTEGER") {
            constexpr auto kMinI64 = static_cast<double>(std::numeric_limits<std::int64_t>::min());
            constexpr auto kMaxI64 = static_cast<double>(std::numeric_limits<std::int64_t>::max());
            if (std::isnan(x) || x < kMinI64 || x > kMaxI64)
                throw std::runtime_error("TaQL: INT() overflow — value out of int64 range");
            return static_cast<std::int64_t>(x); // NOLINT(bugprone-narrowing-conversions)
        }
        if (upper == "BOOL") { bool r = (x != 0.0); return TaqlValue{r}; }
    }

    if (args.size() == 2) {
        // Complex construction
        if (upper == "COMPLEX") {
            return TaqlValue{std::complex<double>(as_double(args[0]), as_double(args[1]))};
        }
        // Array functions with 2 args
        if (upper == "ARRAY") {
            double fill_val = as_double(args[0]);
            auto n = static_cast<std::size_t>(as_int(args[1]));
            return TaqlValue{std::vector<double>(n, fill_val)};
        }
        if (upper == "RESIZE") {
            auto arr = as_double_array(args[0]);
            auto new_size = static_cast<std::size_t>(as_int(args[1]));
            arr.resize(new_size, 0.0);
            return TaqlValue{std::move(arr)};
        }
        if (upper == "ARRFRACTILE" || upper == "FRACTILE") {
            auto arr = as_double_array(args[0]);
            auto frac = as_double(args[1]);
            if (arr.empty()) return TaqlValue{0.0};
            std::sort(arr.begin(), arr.end());
            auto idx = static_cast<std::size_t>(frac * static_cast<double>(arr.size() - 1));
            if (idx >= arr.size()) idx = arr.size() - 1;
            return TaqlValue{arr[idx]};
        }
        // Running/boxed with 2 args
        if (upper == "RUNNINGSUM" || upper == "BOXEDSUM") {
            auto arr = as_double_array(args[0]);
            auto win = static_cast<std::size_t>(as_int(args[1]));
            if (win == 0) win = 1;
            std::vector<double> result(arr.size());
            for (std::size_t i = 0; i < arr.size(); ++i) {
                double sum = 0.0;
                auto start = (i >= win) ? i - win + 1 : std::size_t{0};
                for (std::size_t j = start; j <= i; ++j) sum += arr[j];
                result[i] = sum;
            }
            return TaqlValue{std::move(result)};
        }
        if (upper == "RUNNINGMEAN" || upper == "BOXEDMEAN") {
            auto arr = as_double_array(args[0]);
            auto win = static_cast<std::size_t>(as_int(args[1]));
            if (win == 0) win = 1;
            std::vector<double> result(arr.size());
            for (std::size_t i = 0; i < arr.size(); ++i) {
                double sum = 0.0;
                auto start = (i >= win) ? i - win + 1 : std::size_t{0};
                auto cnt = i - start + 1;
                for (std::size_t j = start; j <= i; ++j) sum += arr[j];
                result[i] = sum / static_cast<double>(cnt);
            }
            return TaqlValue{std::move(result)};
        }
        if (upper == "RUNNINGMIN" || upper == "BOXEDMIN") {
            auto arr = as_double_array(args[0]);
            auto win = static_cast<std::size_t>(as_int(args[1]));
            if (win == 0) win = 1;
            std::vector<double> result(arr.size());
            for (std::size_t i = 0; i < arr.size(); ++i) {
                auto start = (i >= win) ? i - win + 1 : std::size_t{0};
                double mn = arr[start];
                for (std::size_t j = start + 1; j <= i; ++j) if (arr[j] < mn) mn = arr[j];
                result[i] = mn;
            }
            return TaqlValue{std::move(result)};
        }
        if (upper == "RUNNINGMAX" || upper == "BOXEDMAX") {
            auto arr = as_double_array(args[0]);
            auto win = static_cast<std::size_t>(as_int(args[1]));
            if (win == 0) win = 1;
            std::vector<double> result(arr.size());
            for (std::size_t i = 0; i < arr.size(); ++i) {
                auto start = (i >= win) ? i - win + 1 : std::size_t{0};
                double mx = arr[start];
                for (std::size_t j = start + 1; j <= i; ++j) if (arr[j] > mx) mx = arr[j];
                result[i] = mx;
            }
            return TaqlValue{std::move(result)};
        }
        // HDMS(ra, dec)
        if (upper == "HDMS") {
            double ra_rad = as_double(args[0]);
            double dec_rad = as_double(args[1]);
            double hours = ra_rad * 12.0 / 3.14159265358979323846;
            if (hours < 0) hours += 24.0;
            auto h = static_cast<int>(hours);
            double mf_h = (hours - h) * 60.0;
            auto m_h = static_cast<int>(mf_h);
            double s_h = (mf_h - m_h) * 60.0;
            double deg = dec_rad * 180.0 / 3.14159265358979323846;
            char sign = '+';
            if (deg < 0) { sign = '-'; deg = -deg; }
            auto d = static_cast<int>(deg);
            double mf_d = (deg - d) * 60.0;
            auto m_d = static_cast<int>(mf_d);
            double s_d = (mf_d - m_d) * 60.0;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%06.3f %c%02d.%02d.%06.3f",
                          h, m_h, s_h, sign, d, m_d, s_d);
            return TaqlValue{std::string(buf)};
        }
        // UNIT(value, to_unit)
        if (upper == "UNIT") {
            return TaqlValue{convert_unit(as_double(args[0]), "", as_string(args[1]))};
        }
        if (upper == "ATAN2") return TaqlValue{std::atan2(as_double(args[0]), as_double(args[1]))};
        if (upper == "POW") return TaqlValue{std::pow(as_double(args[0]), as_double(args[1]))};
        if (upper == "FMOD") return TaqlValue{std::fmod(as_double(args[0]), as_double(args[1]))};
        if (upper == "MIN") return TaqlValue{std::min(as_double(args[0]), as_double(args[1]))};
        if (upper == "MAX") return TaqlValue{std::max(as_double(args[0]), as_double(args[1]))};
        if (upper == "NEAR" || upper == "NEARABS") {
            bool r = std::abs(as_double(args[0]) - as_double(args[1])) < 1e-13;
            return TaqlValue{r};
        }
        // SUBSTR(str, start) — return from position start to end
        if (upper == "SUBSTR") {
            auto s = as_string(args[0]);
            auto start = static_cast<std::size_t>(as_int(args[1]));
            if (start >= s.size()) return std::string{};
            return s.substr(start);
        }
    }

    if (args.size() == 3) {
        if (upper == "NEAR" || upper == "NEARABS") {
            bool r = std::abs(as_double(args[0]) - as_double(args[1])) < as_double(args[2]);
            return TaqlValue{r};
        }
        // SUBSTR(str, start, len)
        if (upper == "SUBSTR") {
            auto s = as_string(args[0]);
            auto start = static_cast<std::size_t>(as_int(args[1]));
            auto len = static_cast<std::size_t>(as_int(args[2]));
            if (start >= s.size()) return std::string{};
            return s.substr(start, len);
        }
        // REPLACE(str, old, new)
        if (upper == "REPLACE") {
            auto s = as_string(args[0]);
            auto old_str = as_string(args[1]);
            auto new_str = as_string(args[2]);
            if (old_str.empty()) return s;
            std::string result;
            std::size_t pos = 0;
            while (true) {
                auto found = s.find(old_str, pos);
                if (found == std::string::npos) { result += s.substr(pos); break; }
                result += s.substr(pos, found - pos);
                result += new_str;
                pos = found + old_str.size();
            }
            return result;
        }
    }

    // =====================================================================
    // Wave B: Array functions (any arg count)
    // =====================================================================

    // --- Array info functions ---
    if (upper == "NDIM") {
        if (args.empty()) throw std::runtime_error("TaQL: NDIM requires 1 argument");
        if (is_array_value(args[0])) {
            // For 1-D arrays in our model, ndim = 1
            return std::int64_t{1};
        }
        return std::int64_t{0}; // scalar
    }
    if (upper == "NELEM" || upper == "NELEMENTS") {
        if (args.empty()) throw std::runtime_error("TaQL: NELEM requires 1 argument");
        return array_nelem(args[0]);
    }
    if (upper == "SHAPE") {
        if (args.empty()) throw std::runtime_error("TaQL: SHAPE requires 1 argument");
        if (is_array_value(args[0])) {
            return std::vector<std::int64_t>{array_nelem(args[0])};
        }
        return std::vector<std::int64_t>{};
    }

    // --- Array reductions ---
    if (upper == "ARRSUM" || upper == "SUM") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRSUM requires 1 argument");
        auto arr = as_double_array(args[0]);
        double sum = 0.0;
        for (auto v : arr) sum += v;
        return TaqlValue{sum};
    }
    if (upper == "ARRMIN") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRMIN requires 1 argument");
        auto arr = as_double_array(args[0]);
        if (arr.empty()) return std::monostate{};
        return TaqlValue{*std::min_element(arr.begin(), arr.end())};
    }
    if (upper == "ARRMAX") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRMAX requires 1 argument");
        auto arr = as_double_array(args[0]);
        if (arr.empty()) return std::monostate{};
        return TaqlValue{*std::max_element(arr.begin(), arr.end())};
    }
    if (upper == "ARRMEAN" || upper == "MEAN") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRMEAN requires 1 argument");
        auto arr = as_double_array(args[0]);
        if (arr.empty()) return TaqlValue{0.0};
        double sum = 0.0;
        for (auto v : arr) sum += v;
        return TaqlValue{sum / static_cast<double>(arr.size())};
    }
    if (upper == "ARRMEDIAN" || upper == "MEDIAN") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRMEDIAN requires 1 argument");
        auto arr = as_double_array(args[0]);
        if (arr.empty()) return TaqlValue{0.0};
        std::sort(arr.begin(), arr.end());
        auto n = arr.size();
        if (n % 2 == 0)
            return TaqlValue{(arr[n / 2 - 1] + arr[n / 2]) / 2.0};
        return TaqlValue{arr[n / 2]};
    }
    if (upper == "ARRVARIANCE" || upper == "VARIANCE") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRVARIANCE requires 1 argument");
        auto arr = as_double_array(args[0]);
        if (arr.size() < 2) return TaqlValue{0.0};
        double mean = 0.0;
        for (auto v : arr) mean += v;
        mean /= static_cast<double>(arr.size());
        double var = 0.0;
        for (auto v : arr) var += (v - mean) * (v - mean);
        return TaqlValue{var / static_cast<double>(arr.size() - 1)};
    }
    if (upper == "ARRSTDDEV" || upper == "STDDEV") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRSTDDEV requires 1 argument");
        auto arr = as_double_array(args[0]);
        if (arr.size() < 2) return TaqlValue{0.0};
        double mean = 0.0;
        for (auto v : arr) mean += v;
        mean /= static_cast<double>(arr.size());
        double var = 0.0;
        for (auto v : arr) var += (v - mean) * (v - mean);
        return TaqlValue{std::sqrt(var / static_cast<double>(arr.size() - 1))};
    }
    if (upper == "ARRRMS" || upper == "RMS") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRRMS requires 1 argument");
        auto arr = as_double_array(args[0]);
        if (arr.empty()) return TaqlValue{0.0};
        double sum_sq = 0.0;
        for (auto v : arr) sum_sq += v * v;
        return TaqlValue{std::sqrt(sum_sq / static_cast<double>(arr.size()))};
    }
    if (upper == "ARRAVDEV") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRAVDEV requires 1 argument");
        auto arr = as_double_array(args[0]);
        if (arr.empty()) return TaqlValue{0.0};
        double mean = 0.0;
        for (auto v : arr) mean += v;
        mean /= static_cast<double>(arr.size());
        double avdev = 0.0;
        for (auto v : arr) avdev += std::abs(v - mean);
        return TaqlValue{avdev / static_cast<double>(arr.size())};
    }
    if (upper == "ARRFRACTILE" || upper == "FRACTILE") {
        if (args.size() < 2) throw std::runtime_error("TaQL: ARRFRACTILE requires 2 arguments");
        auto arr = as_double_array(args[0]);
        auto frac = as_double(args[1]);
        if (arr.empty()) return TaqlValue{0.0};
        std::sort(arr.begin(), arr.end());
        auto idx = static_cast<std::size_t>(frac * static_cast<double>(arr.size() - 1));
        if (idx >= arr.size()) idx = arr.size() - 1;
        return TaqlValue{arr[idx]};
    }

    // --- Array boolean reductions ---
    if (upper == "ARRANY" || upper == "ANY") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRANY requires 1 argument");
        if (auto* a = std::get_if<std::vector<bool>>(&args[0])) {
            bool r = std::any_of(a->begin(), a->end(), [](bool b) { return b; });
            return TaqlValue{r};
        }
        auto arr = as_double_array(args[0]);
        bool r = std::any_of(arr.begin(), arr.end(), [](double v) { return v != 0.0; });
        return TaqlValue{r};
    }
    if (upper == "ARRALL" || upper == "ALL") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRALL requires 1 argument");
        if (auto* a = std::get_if<std::vector<bool>>(&args[0])) {
            bool r = std::all_of(a->begin(), a->end(), [](bool b) { return b; });
            return TaqlValue{r};
        }
        auto arr = as_double_array(args[0]);
        bool r = std::all_of(arr.begin(), arr.end(), [](double v) { return v != 0.0; });
        return TaqlValue{r};
    }
    if (upper == "ARRNTRUE" || upper == "NTRUE") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRNTRUE requires 1 argument");
        if (auto* a = std::get_if<std::vector<bool>>(&args[0])) {
            auto cnt = std::count(a->begin(), a->end(), true);
            return static_cast<std::int64_t>(cnt);
        }
        auto arr = as_double_array(args[0]);
        auto cnt = std::count_if(arr.begin(), arr.end(), [](double v) { return v != 0.0; });
        return static_cast<std::int64_t>(cnt);
    }
    if (upper == "ARRNFALSE" || upper == "NFALSE") {
        if (args.empty()) throw std::runtime_error("TaQL: ARRNFALSE requires 1 argument");
        if (auto* a = std::get_if<std::vector<bool>>(&args[0])) {
            auto cnt = std::count(a->begin(), a->end(), false);
            return static_cast<std::int64_t>(cnt);
        }
        auto arr = as_double_array(args[0]);
        auto cnt = std::count_if(arr.begin(), arr.end(), [](double v) { return v == 0.0; });
        return static_cast<std::int64_t>(cnt);
    }

    // --- Array construction/manipulation ---
    if (upper == "ARRAY") {
        // ARRAY(value, shape) - create array filled with value
        if (args.size() < 2) throw std::runtime_error("TaQL: ARRAY requires 2+ arguments");
        double fill_val = as_double(args[0]);
        auto n = static_cast<std::size_t>(as_int(args[1]));
        return TaqlValue{std::vector<double>(n, fill_val)};
    }
    if (upper == "TRANSPOSE") {
        if (args.empty()) throw std::runtime_error("TaQL: TRANSPOSE requires 1 argument");
        // For 1-D arrays, transpose is identity
        return args[0];
    }
    if (upper == "AREVERSE") {
        if (args.empty()) throw std::runtime_error("TaQL: AREVERSE requires 1 argument");
        if (auto* a = std::get_if<std::vector<double>>(&args[0])) {
            auto result = *a;
            std::reverse(result.begin(), result.end());
            return TaqlValue{std::move(result)};
        }
        if (auto* a = std::get_if<std::vector<std::int64_t>>(&args[0])) {
            auto result = *a;
            std::reverse(result.begin(), result.end());
            return TaqlValue{std::move(result)};
        }
        return args[0];
    }
    if (upper == "DIAGONAL") {
        // For 1-D, return the array itself (diagonal of a 1-D is the same)
        if (args.empty()) throw std::runtime_error("TaQL: DIAGONAL requires 1 argument");
        return args[0];
    }
    if (upper == "ARRFLAT" || upper == "FLATTEN") {
        // Flatten is identity for 1-D
        if (args.empty()) throw std::runtime_error("TaQL: ARRFLAT requires 1 argument");
        return args[0];
    }
    if (upper == "RESIZE") {
        if (args.size() < 2) throw std::runtime_error("TaQL: RESIZE requires 2 arguments");
        auto arr = as_double_array(args[0]);
        auto new_size = static_cast<std::size_t>(as_int(args[1]));
        arr.resize(new_size, 0.0);
        return TaqlValue{std::move(arr)};
    }

    // --- Running/boxed reductions ---
    if (upper == "RUNNINGSUM" || upper == "BOXEDSUM") {
        if (args.size() < 2) throw std::runtime_error("TaQL: " + upper + " requires 2 arguments");
        auto arr = as_double_array(args[0]);
        auto win = static_cast<std::size_t>(as_int(args[1]));
        if (win == 0) win = 1;
        std::vector<double> result(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            double sum = 0.0;
            auto start = (i >= win) ? i - win + 1 : std::size_t{0};
            for (std::size_t j = start; j <= i; ++j) sum += arr[j];
            result[i] = sum;
        }
        return TaqlValue{std::move(result)};
    }
    if (upper == "RUNNINGMEAN" || upper == "BOXEDMEAN") {
        if (args.size() < 2) throw std::runtime_error("TaQL: " + upper + " requires 2 arguments");
        auto arr = as_double_array(args[0]);
        auto win = static_cast<std::size_t>(as_int(args[1]));
        if (win == 0) win = 1;
        std::vector<double> result(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            double sum = 0.0;
            auto start = (i >= win) ? i - win + 1 : std::size_t{0};
            auto count = i - start + 1;
            for (std::size_t j = start; j <= i; ++j) sum += arr[j];
            result[i] = sum / static_cast<double>(count);
        }
        return TaqlValue{std::move(result)};
    }
    if (upper == "RUNNINGMIN" || upper == "BOXEDMIN") {
        if (args.size() < 2) throw std::runtime_error("TaQL: " + upper + " requires 2 arguments");
        auto arr = as_double_array(args[0]);
        auto win = static_cast<std::size_t>(as_int(args[1]));
        if (win == 0) win = 1;
        std::vector<double> result(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            auto start = (i >= win) ? i - win + 1 : std::size_t{0};
            double mn = arr[start];
            for (std::size_t j = start + 1; j <= i; ++j)
                if (arr[j] < mn) mn = arr[j];
            result[i] = mn;
        }
        return TaqlValue{std::move(result)};
    }
    if (upper == "RUNNINGMAX" || upper == "BOXEDMAX") {
        if (args.size() < 2) throw std::runtime_error("TaQL: " + upper + " requires 2 arguments");
        auto arr = as_double_array(args[0]);
        auto win = static_cast<std::size_t>(as_int(args[1]));
        if (win == 0) win = 1;
        std::vector<double> result(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            auto start = (i >= win) ? i - win + 1 : std::size_t{0};
            double mx = arr[start];
            for (std::size_t j = start + 1; j <= i; ++j)
                if (arr[j] > mx) mx = arr[j];
            result[i] = mx;
        }
        return TaqlValue{std::move(result)};
    }

    // =====================================================================
    // Wave C: Complex number functions
    // =====================================================================
    if (upper == "CONJ") {
        if (args.empty()) throw std::runtime_error("TaQL: CONJ requires 1 argument");
        auto c = as_complex(args[0]);
        return TaqlValue{std::conj(c)};
    }
    if (upper == "COMPLEX") {
        if (args.size() < 2) throw std::runtime_error("TaQL: COMPLEX requires 2 arguments");
        return TaqlValue{std::complex<double>(as_double(args[0]), as_double(args[1]))};
    }

    // =====================================================================
    // Wave C: DateTime functions
    // =====================================================================
    if (upper == "DATETIME") {
        if (args.empty()) throw std::runtime_error("TaQL: DATETIME requires 1 argument");
        auto s = as_string(args[0]);
        return TaqlValue{parse_datetime_to_mjd(s)};
    }
    if (upper == "MJDTODATE" || upper == "MJD") {
        if (args.empty()) throw std::runtime_error("TaQL: MJDTODATE requires 1 argument");
        return TaqlValue{mjd_to_datetime(as_double(args[0]))};
    }
    if (upper == "DATE") {
        if (args.empty()) throw std::runtime_error("TaQL: DATE requires 1 argument");
        auto dt = mjd_to_datetime(as_double(args[0]));
        return TaqlValue{dt.substr(0, 10)}; // YYYY/MM/DD
    }
    if (upper == "TIME") {
        if (args.empty()) throw std::runtime_error("TaQL: TIME requires 1 argument");
        auto dt = mjd_to_datetime(as_double(args[0]));
        if (dt.size() > 11) return TaqlValue{dt.substr(11)};
        return TaqlValue{std::string("00:00:00.000")};
    }
    if (upper == "YEAR") {
        if (args.empty()) throw std::runtime_error("TaQL: YEAR requires 1 argument");
        auto dt = mjd_to_datetime(as_double(args[0]));
        return static_cast<std::int64_t>(std::stoi(dt.substr(0, 4)));
    }
    if (upper == "MONTH" || upper == "CMONTH") {
        if (args.empty()) throw std::runtime_error("TaQL: MONTH requires 1 argument");
        auto dt = mjd_to_datetime(as_double(args[0]));
        auto m = std::stoi(dt.substr(5, 2));
        if (upper == "CMONTH") {
            static const char* const months[] = {
                "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            return TaqlValue{std::string(months[m])};
        }
        return static_cast<std::int64_t>(m);
    }
    if (upper == "DAY") {
        if (args.empty()) throw std::runtime_error("TaQL: DAY requires 1 argument");
        auto dt = mjd_to_datetime(as_double(args[0]));
        return static_cast<std::int64_t>(std::stoi(dt.substr(8, 2)));
    }
    if (upper == "WEEKDAY" || upper == "CDOW" || upper == "DOW") {
        if (args.empty()) throw std::runtime_error("TaQL: WEEKDAY requires 1 argument");
        // MJD 0 = Wednesday (1858-11-17)
        auto mjd = as_double(args[0]);
        auto day_of_week = (static_cast<std::int64_t>(mjd) + 3) % 7; // 0=Sun
        if (day_of_week < 0) day_of_week += 7;
        if (upper == "CDOW") {
            static const char* const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            return TaqlValue{std::string(days[day_of_week])};
        }
        return static_cast<std::int64_t>(day_of_week);
    }
    if (upper == "WEEK") {
        if (args.empty()) throw std::runtime_error("TaQL: WEEK requires 1 argument");
        auto dt = mjd_to_datetime(as_double(args[0]));
        auto y = std::stoi(dt.substr(0, 4));
        auto m = std::stoi(dt.substr(5, 2));
        auto d = std::stoi(dt.substr(8, 2));
        // Approximate week of year
        int doy = d;
        static const int days_before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        if (m >= 1 && m <= 12) doy += days_before[m - 1];
        bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        if (m > 2 && leap) ++doy;
        return static_cast<std::int64_t>((doy - 1) / 7 + 1);
    }
    if (upper == "CTOD") {
        // Convert date string to MJD double
        if (args.empty()) throw std::runtime_error("TaQL: CTOD requires 1 argument");
        return TaqlValue{parse_datetime_to_mjd(as_string(args[0]))};
    }
    if (upper == "CDATE") {
        if (args.empty()) throw std::runtime_error("TaQL: CDATE requires 1 argument");
        return TaqlValue{mjd_to_datetime(as_double(args[0])).substr(0, 10)};
    }
    if (upper == "CTIME") {
        if (args.empty()) throw std::runtime_error("TaQL: CTIME requires 1 argument");
        auto dt = mjd_to_datetime(as_double(args[0]));
        if (dt.size() > 11) return TaqlValue{dt.substr(11)};
        return TaqlValue{std::string("00:00:00.000")};
    }

    // =====================================================================
    // Wave C: Angle functions
    // =====================================================================
    if (upper == "HMS") {
        // Convert radians to HH:MM:SS string
        if (args.empty()) throw std::runtime_error("TaQL: HMS requires 1 argument");
        double rad = as_double(args[0]);
        double hours = rad * 12.0 / 3.14159265358979323846;
        if (hours < 0) hours += 24.0;
        auto h = static_cast<int>(hours);
        double mf = (hours - h) * 60.0;
        auto m = static_cast<int>(mf);
        double s = (mf - m) * 60.0;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%06.3f", h, m, s);
        return TaqlValue{std::string(buf)};
    }
    if (upper == "DMS") {
        // Convert radians to DD.MM.SS string
        if (args.empty()) throw std::runtime_error("TaQL: DMS requires 1 argument");
        double rad = as_double(args[0]);
        double deg = rad * 180.0 / 3.14159265358979323846;
        char sign = '+';
        if (deg < 0) { sign = '-'; deg = -deg; }
        auto d = static_cast<int>(deg);
        double mf = (deg - d) * 60.0;
        auto m = static_cast<int>(mf);
        double s = (mf - m) * 60.0;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%c%02d.%02d.%06.3f", sign, d, m, s);
        return TaqlValue{std::string(buf)};
    }
    if (upper == "HDMS") {
        // Convert radians pair (ra, dec) to "HH:MM:SS DD.MM.SS"
        if (args.size() < 2) throw std::runtime_error("TaQL: HDMS requires 2 arguments");
        // Re-use HMS and DMS
        double ra_rad = as_double(args[0]);
        double dec_rad = as_double(args[1]);
        double hours = ra_rad * 12.0 / 3.14159265358979323846;
        if (hours < 0) hours += 24.0;
        auto h = static_cast<int>(hours);
        double mf_h = (hours - h) * 60.0;
        auto m_h = static_cast<int>(mf_h);
        double s_h = (mf_h - m_h) * 60.0;

        double deg = dec_rad * 180.0 / 3.14159265358979323846;
        char sign = '+';
        if (deg < 0) { sign = '-'; deg = -deg; }
        auto d = static_cast<int>(deg);
        double mf_d = (deg - d) * 60.0;
        auto m_d = static_cast<int>(mf_d);
        double s_d = (mf_d - m_d) * 60.0;

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%06.3f %c%02d.%02d.%06.3f",
                      h, m_h, s_h, sign, d, m_d, s_d);
        return TaqlValue{std::string(buf)};
    }
    if (upper == "NORMANGLE") {
        // Normalize angle to [0, 2*pi)
        if (args.empty()) throw std::runtime_error("TaQL: NORMANGLE requires 1 argument");
        constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
        double rad = as_double(args[0]);
        rad = std::fmod(rad, kTwoPi);
        if (rad < 0) rad += kTwoPi;
        return TaqlValue{rad};
    }
    if (upper == "ANGDIST" || upper == "ANGDISTX") {
        // Great-circle angular distance between two (lon,lat) pairs
        if (args.size() < 4) throw std::runtime_error("TaQL: ANGDIST requires 4 arguments");
        double lon1 = as_double(args[0]);
        double lat1 = as_double(args[1]);
        double lon2 = as_double(args[2]);
        double lat2 = as_double(args[3]);
        // Vincenty formula
        double dlon = lon2 - lon1;
        double cos_lat2 = std::cos(lat2);
        double sin_lat2 = std::sin(lat2);
        double cos_lat1 = std::cos(lat1);
        double sin_lat1 = std::sin(lat1);
        double a = cos_lat2 * std::sin(dlon);
        double b = cos_lat1 * sin_lat2 - sin_lat1 * cos_lat2 * std::cos(dlon);
        double c = sin_lat1 * sin_lat2 + cos_lat1 * cos_lat2 * std::cos(dlon);
        return TaqlValue{std::atan2(std::sqrt(a * a + b * b), c)};
    }

    // =====================================================================
    // Wave C: Unit conversion function
    // =====================================================================
    if (upper == "UNIT") {
        // UNIT(value, from_unit, to_unit) or UNIT(value, to_unit)
        if (args.size() == 2) {
            // Convert to given unit (assume SI input)
            return TaqlValue{convert_unit(as_double(args[0]), "", as_string(args[1]))};
        }
        if (args.size() >= 3) {
            return TaqlValue{convert_unit(as_double(args[0]), as_string(args[1]), as_string(args[2]))};
        }
        throw std::runtime_error("TaQL: UNIT requires 2-3 arguments");
    }

    // =====================================================================
    // Wave C: Pattern functions
    // =====================================================================
    if (upper == "REGEX" || upper == "PATTERN" || upper == "SQLPATTERN") {
        // Convert a pattern to regex or vice versa
        if (args.empty()) throw std::runtime_error("TaQL: " + upper + " requires 1 argument");
        return args[0]; // pass through for evaluation
    }

    // =====================================================================
    // Wave C: STYLE and TIMING (informational, no-op in evaluator)
    // =====================================================================

    // =====================================================================
    // Wave H: Cone search functions
    // =====================================================================

    // Helper lambda: Vincenty angular distance (radians)
    auto angdist_rad = [](double lon1, double lat1, double lon2, double lat2) -> double {
        double dlon = lon2 - lon1;
        double cos_lat2 = std::cos(lat2);
        double sin_lat2 = std::sin(lat2);
        double cos_lat1 = std::cos(lat1);
        double sin_lat1 = std::sin(lat1);
        double a = cos_lat2 * std::sin(dlon);
        double b = cos_lat1 * sin_lat2 - sin_lat1 * cos_lat2 * std::cos(dlon);
        double c = sin_lat1 * sin_lat2 + cos_lat1 * cos_lat2 * std::cos(dlon);
        return std::atan2(std::sqrt(a * a + b * b), c);
    };

    // INCONE(lon, lat, cLon, cLat, radius) - function-call form
    if (upper == "INCONE" || upper == "CONES") {
        if (args.size() < 5) throw std::runtime_error("TaQL: " + upper + " requires 5 arguments");
        double lon = as_double(args[0]);
        double lat = as_double(args[1]);
        double clon = as_double(args[2]);
        double clat = as_double(args[3]);
        double radius = as_double(args[4]);
        double dist = angdist_rad(lon, lat, clon, clat);
        bool result = dist <= std::abs(radius);
        return TaqlValue{result};
    }

    // ANYCONE(lon, lat, centers_array, radius) - check if point is in any cone
    if (upper == "ANYCONE") {
        if (args.size() < 3) throw std::runtime_error("TaQL: ANYCONE requires at least 3 arguments");
        double lon = as_double(args[0]);
        double lat = as_double(args[1]);
        // centers_array is a flat array [lon1,lat1,lon2,lat2,...] with radius as last arg
        // or centers_array has triples [lon1,lat1,r1,lon2,lat2,r2,...]
        if (auto* centers = std::get_if<std::vector<double>>(&args[2])) {
            if (args.size() >= 4) {
                // ANYCONE(lon, lat, centers_array, radius) - single radius for all cones
                double radius = as_double(args[3]);
                for (std::size_t i = 0; i + 1 < centers->size(); i += 2) {
                    double dist = angdist_rad(lon, lat, (*centers)[i], (*centers)[i + 1]);
                    if (dist <= std::abs(radius)) return TaqlValue{true};
                }
            } else {
                // ANYCONE(lon, lat, centers_with_radii) - triples [lon,lat,r,...]
                for (std::size_t i = 0; i + 2 < centers->size(); i += 3) {
                    double dist = angdist_rad(lon, lat, (*centers)[i], (*centers)[i + 1]);
                    if (dist <= std::abs((*centers)[i + 2])) return TaqlValue{true};
                }
            }
            return TaqlValue{false};
        }
        throw std::runtime_error("TaQL: ANYCONE 3rd argument must be an array");
    }

    // FINDCONE(lon, lat, centers_array, radius) - return index of matching cone
    if (upper == "FINDCONE") {
        if (args.size() < 3) throw std::runtime_error("TaQL: FINDCONE requires at least 3 arguments");
        double lon = as_double(args[0]);
        double lat = as_double(args[1]);
        if (auto* centers = std::get_if<std::vector<double>>(&args[2])) {
            if (args.size() >= 4) {
                // FINDCONE(lon, lat, centers_array, radius)
                double radius = as_double(args[3]);
                for (std::size_t i = 0; i + 1 < centers->size(); i += 2) {
                    double dist = angdist_rad(lon, lat, (*centers)[i], (*centers)[i + 1]);
                    if (dist <= std::abs(radius)) {
                        return TaqlValue{static_cast<std::int64_t>(i / 2)};
                    }
                }
            } else {
                // FINDCONE(lon, lat, centers_with_radii) - triples
                for (std::size_t i = 0; i + 2 < centers->size(); i += 3) {
                    double dist = angdist_rad(lon, lat, (*centers)[i], (*centers)[i + 1]);
                    if (dist <= std::abs((*centers)[i + 2])) {
                        return TaqlValue{static_cast<std::int64_t>(i / 3)};
                    }
                }
            }
            // Not found: return -1
            return TaqlValue{static_cast<std::int64_t>(-1)};
        }
        throw std::runtime_error("TaQL: FINDCONE 3rd argument must be an array");
    }

    // =====================================================================
    // Wave H: MeasUDF dispatch (MEAS.EPOCH, MEAS.DIRECTION, etc.)
    // =====================================================================
    if (upper.size() > 5 && upper.substr(0, 5) == "MEAS.") {
        auto meas_func = upper.substr(5);
        if (meas_func == "EPOCH") {
            // MEAS.EPOCH(value, fromRef, toRef)
            if (args.size() < 3) throw std::runtime_error("TaQL: MEAS.EPOCH requires 3 arguments");
            double mjd_val = as_double(args[0]);
            auto from_str = as_string(args[1]);
            auto to_str = as_string(args[2]);
            EpochRef from_ref = string_to_epoch_ref(from_str);
            EpochRef to_ref = string_to_epoch_ref(to_str);
            double day = std::floor(mjd_val);
            double frac = mjd_val - day;
            Measure m{MeasureType::epoch, MeasureRef{from_ref, std::nullopt},
                      EpochValue{day, frac}};
            auto result = convert_measure(m, to_ref);
            auto rv = std::get<EpochValue>(result.value);
            return TaqlValue{rv.day + rv.fraction};
        }
        if (meas_func == "DIRECTION") {
            // MEAS.DIRECTION(lon, lat, fromRef, toRef)
            if (args.size() < 4) throw std::runtime_error("TaQL: MEAS.DIRECTION requires 4 arguments");
            double lon_rad = as_double(args[0]);
            double lat_rad = as_double(args[1]);
            auto from_str = as_string(args[2]);
            auto to_str = as_string(args[3]);
            DirectionRef from_ref = string_to_direction_ref(from_str);
            DirectionRef to_ref = string_to_direction_ref(to_str);
            Measure m{MeasureType::direction, MeasureRef{from_ref, std::nullopt},
                      DirectionValue{lon_rad, lat_rad}};
            auto result = convert_measure(m, to_ref);
            auto rv = std::get<DirectionValue>(result.value);
            // Return as array [lon, lat]
            return TaqlValue{std::vector<double>{rv.lon_rad, rv.lat_rad}};
        }
        if (meas_func == "POSITION") {
            // MEAS.POSITION(x, y, z, fromRef, toRef)
            if (args.size() < 5) throw std::runtime_error("TaQL: MEAS.POSITION requires 5 arguments");
            double x = as_double(args[0]);
            double y = as_double(args[1]);
            double z = as_double(args[2]);
            auto from_str = as_string(args[3]);
            auto to_str = as_string(args[4]);
            PositionRef from_ref = string_to_position_ref(from_str);
            PositionRef to_ref = string_to_position_ref(to_str);
            Measure m{MeasureType::position, MeasureRef{from_ref, std::nullopt},
                      PositionValue{x, y, z}};
            auto result = convert_measure(m, to_ref);
            auto rv = std::get<PositionValue>(result.value);
            return TaqlValue{std::vector<double>{rv.x_m, rv.y_m, rv.z_m}};
        }
        if (meas_func == "FREQUENCY") {
            // MEAS.FREQUENCY(value, fromRef, toRef)
            if (args.size() < 3) throw std::runtime_error("TaQL: MEAS.FREQUENCY requires 3 arguments");
            double hz = as_double(args[0]);
            auto from_str = as_string(args[1]);
            auto to_str = as_string(args[2]);
            FrequencyRef from_ref = string_to_frequency_ref(from_str);
            FrequencyRef to_ref = string_to_frequency_ref(to_str);
            Measure m{MeasureType::frequency, MeasureRef{from_ref, std::nullopt},
                      FrequencyValue{hz}};
            auto result = convert_measure(m, to_ref);
            auto rv = std::get<FrequencyValue>(result.value);
            return TaqlValue{rv.hz};
        }
        throw std::runtime_error("TaQL: unknown MEAS function '" + name + "'");
    }

    throw std::runtime_error("TaQL: unknown function '" + name + "'");
}

/// Evaluate an expression node.
TaqlValue eval_expr(const TaqlExprNode& node, const EvalContext& ctx) {
    switch (node.type) {
    case ExprType::literal:
        return node.value;

    case ExprType::column_ref: {
        // Support qualified names: table.column or shorthand.column
        auto col_name = node.name;
        Table* target_table = ctx.table;
        std::uint64_t target_row = ctx.row;

        auto dot_pos = col_name.find('.');
        if (dot_pos != std::string::npos && ctx.joined_tables != nullptr) {
            auto table_alias = col_name.substr(0, dot_pos);
            col_name = col_name.substr(dot_pos + 1);
            auto it = ctx.joined_tables->find(table_alias);
            if (it != ctx.joined_tables->end()) {
                target_table = it->second.first;
                target_row = it->second.second;
            }
        }

        if (target_table == nullptr || !ctx.has_row)
            throw std::runtime_error("TaQL: column reference '" + node.name +
                                     "' without table context");

        // Try primary table first, then check joined tables for unqualified names
        const auto* cd = target_table->find_column_desc(col_name);
        if (cd == nullptr && dot_pos == std::string::npos && ctx.joined_tables != nullptr) {
            // Search joined tables for the column
            for (auto& [alias, pair] : *ctx.joined_tables) {
                auto* jcd = pair.first->find_column_desc(col_name);
                if (jcd != nullptr) {
                    target_table = pair.first;
                    target_row = pair.second;
                    cd = jcd;
                    break;
                }
            }
        }

        if (cd != nullptr && cd->kind == ColumnKind::array) {
            return read_array_cell_as_taql(*target_table, col_name, target_row);
        }
        auto cv = target_table->read_scalar_cell(col_name, target_row);
        return cell_to_taql(cv);
    }

    case ExprType::unary_op: {
        auto operand = eval_expr(node.children[0], ctx);
        switch (node.op) {
        case TaqlOp::negate:
            if (auto* i = std::get_if<std::int64_t>(&operand)) return -*i;
            return TaqlValue{-as_double(operand)};
        case TaqlOp::logical_not: {
            bool r = !as_bool(operand);
            return TaqlValue{r};
        }
        case TaqlOp::bit_not:
            return ~as_int(operand);
        case TaqlOp::unary_plus: // NOLINT(bugprone-branch-clone)
            return operand;
        default:
            return operand;
        }
    }

    case ExprType::binary_op: {
        auto lhs = eval_expr(node.children[0], ctx);
        auto rhs = eval_expr(node.children[1], ctx);

        // String concatenation for +
        if (node.op == TaqlOp::plus &&
            (std::holds_alternative<std::string>(lhs) || std::holds_alternative<std::string>(rhs))) {
            return as_string(lhs) + as_string(rhs);
        }

        // Integer operations where both are int
        if (std::holds_alternative<std::int64_t>(lhs) &&
            std::holds_alternative<std::int64_t>(rhs)) {
            auto a = std::get<std::int64_t>(lhs);
            auto b = std::get<std::int64_t>(rhs);
            switch (node.op) {
            case TaqlOp::plus: return a + b; // NOLINT(bugprone-branch-clone)
            case TaqlOp::minus: return a - b;
            case TaqlOp::multiply: return a * b;
            case TaqlOp::divide: [[fallthrough]];
            case TaqlOp::int_divide: return (b != 0) ? a / b : throw std::runtime_error("TaQL: division by zero"); // NOLINT(bugprone-branch-clone)
            case TaqlOp::modulo: return (b != 0) ? a % b : throw std::runtime_error("TaQL: modulo by zero");
            case TaqlOp::power: return static_cast<std::int64_t>(std::pow(a, b));
            case TaqlOp::bit_and: return a & b;
            case TaqlOp::bit_or: return a | b;
            case TaqlOp::bit_xor: return a ^ b;
            default: break;
            }
        }

        // Complex operations where either operand is complex
        if (std::holds_alternative<std::complex<double>>(lhs) ||
            std::holds_alternative<std::complex<double>>(rhs)) {
            auto ca = as_complex(lhs);
            auto cb = as_complex(rhs);
            switch (node.op) {
            case TaqlOp::plus: return TaqlValue{ca + cb};
            case TaqlOp::minus: return TaqlValue{ca - cb};
            case TaqlOp::multiply: return TaqlValue{ca * cb};
            case TaqlOp::divide:
                if (cb == std::complex<double>{0.0, 0.0})
                    throw std::runtime_error("TaQL: division by zero");
                return TaqlValue{ca / cb};
            case TaqlOp::power: return TaqlValue{std::pow(ca, cb)};
            default:
                throw std::runtime_error("TaQL: unsupported binary operator for complex");
            }
        }

        // Float operations
        double a = as_double(lhs);
        double b = as_double(rhs);
        switch (node.op) {
        case TaqlOp::plus: return TaqlValue{a + b}; // NOLINT(bugprone-branch-clone)
        case TaqlOp::minus: return TaqlValue{a - b};
        case TaqlOp::multiply: return TaqlValue{a * b};
        case TaqlOp::divide:
            return TaqlValue{(b != 0.0) ? a / b : throw std::runtime_error("TaQL: division by zero")};
        case TaqlOp::int_divide: return static_cast<std::int64_t>(a / b); // NOLINT(bugprone-narrowing-conversions)
        case TaqlOp::modulo: return TaqlValue{std::fmod(a, b)}; // NOLINT(bugprone-branch-clone)
        case TaqlOp::power: return TaqlValue{std::pow(a, b)};
        default:
            throw std::runtime_error("TaQL: unsupported binary operator");
        }
    }

    case ExprType::comparison: {
        auto lhs = eval_expr(node.children[0], ctx);
        auto rhs = eval_expr(node.children[1], ctx);
        int cmp = compare_values(lhs, rhs);
        bool result = false;
        switch (node.op) {
        case TaqlOp::eq: result = (cmp == 0); break; // NOLINT(bugprone-branch-clone)
        case TaqlOp::ne: result = (cmp != 0); break;
        case TaqlOp::lt: result = (cmp < 0); break;
        case TaqlOp::le: result = (cmp <= 0); break;
        case TaqlOp::gt: result = (cmp > 0); break;
        case TaqlOp::ge: result = (cmp >= 0); break;
        case TaqlOp::near: // NOLINT(bugprone-branch-clone)
            result = std::abs(as_double(lhs) - as_double(rhs)) < 1e-5;
            break;
        case TaqlOp::not_near:
            result = std::abs(as_double(lhs) - as_double(rhs)) >= 1e-5;
            break;
        default:
            throw std::runtime_error("TaQL: unsupported comparison operator");
        }
        return TaqlValue{result};
    }

    case ExprType::logical_op: {
        auto lhs = eval_expr(node.children[0], ctx);
        if (node.op == TaqlOp::logical_and) {
            bool r = as_bool(lhs) && as_bool(eval_expr(node.children[1], ctx));
            return TaqlValue{r};
        }
        if (node.op == TaqlOp::logical_or) {
            bool r = as_bool(lhs) || as_bool(eval_expr(node.children[1], ctx));
            return TaqlValue{r};
        }
        throw std::runtime_error("TaQL: unsupported logical operator");
    }

    case ExprType::func_call: {
        std::vector<TaqlValue> args;
        args.reserve(node.children.size());
        for (auto& child : node.children)
            args.push_back(eval_expr(child, ctx));
        return eval_math_func(node.name, args, ctx);
    }

    case ExprType::iif_expr: {
        auto cond = eval_expr(node.children[0], ctx);
        if (as_bool(cond))
            return eval_expr(node.children[1], ctx);
        return eval_expr(node.children[2], ctx);
    }

    case ExprType::in_expr: {
        auto lhs = eval_expr(node.children[0], ctx);
        bool found = false;
        auto& set_node = node.children[1];
        for (auto& elem : set_node.children) {
            auto elem_val = eval_expr(elem, ctx);
            if (compare_values(lhs, elem_val) == 0) {
                found = true;
                break;
            }
        }
        bool r = (node.op == TaqlOp::in_set) ? found : !found;
        return TaqlValue{r};
    }

    case ExprType::between_expr: {
        auto val = as_double(eval_expr(node.children[0], ctx));
        auto lo = as_double(eval_expr(node.children[1], ctx));
        auto hi = as_double(eval_expr(node.children[2], ctx));
        bool in_range = (val >= lo && val <= hi);
        bool r = (node.op == TaqlOp::between) ? in_range : !in_range;
        return TaqlValue{r};
    }

    case ExprType::like_expr: {
        auto val = as_string(eval_expr(node.children[0], ctx));
        auto pat = as_string(eval_expr(node.children[1], ctx));
        bool case_insensitive = (node.op == TaqlOp::ilike || node.op == TaqlOp::not_ilike);
        std::string target = val;
        std::string pattern = pat;
        if (case_insensitive) {
            for (auto& c : target) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (auto& c : pattern) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        // Convert SQL/glob pattern to regex: % and * -> .*, ? and _ -> .
        std::string re_str;
        re_str.reserve(pattern.size() * 2);
        for (auto c : pattern) {
            switch (c) {
            case '%': case '*': re_str += ".*"; break;
            case '?': case '_': re_str += '.'; break;
            case '.': case '(': case ')': case '[': case ']':
            case '{': case '}': case '^': case '$': case '|':
            case '+': case '\\':
                re_str += '\\'; re_str += c; break;
            default: re_str += c; break;
            }
        }
        bool match = false;
        try {
            std::regex re(re_str, std::regex::ECMAScript);
            match = std::regex_match(target, re);
        } catch (...) {
            match = (target == pattern);
        }
        bool r = (node.op == TaqlOp::like || node.op == TaqlOp::ilike) ? match : !match;
        return TaqlValue{r};
    }

    case ExprType::regex_expr: {
        auto val = as_string(eval_expr(node.children[0], ctx));
        auto pat = as_string(eval_expr(node.children[1], ctx));
        bool match = false;
        try {
            std::regex re(pat, std::regex::ECMAScript);
            match = std::regex_search(val, re);
        } catch (...) {
            match = false;
        }
        bool r = (node.op == TaqlOp::regex_match) ? match : !match;
        return TaqlValue{r};
    }

    case ExprType::set_expr: {
        // Return the first element for scalar context, or build a vector
        if (node.children.empty()) return std::monostate{};
        if (node.children.size() == 1) return eval_expr(node.children[0], ctx);
        // Build vector
        std::vector<double> vec;
        vec.reserve(node.children.size());
        for (auto& child : node.children) {
            vec.push_back(as_double(eval_expr(child, ctx)));
        }
        return vec;
    }

    case ExprType::wildcard:
        return std::monostate{}; // Handled specially in projection

    case ExprType::keyword_ref: {
        // table::keyword or col::keyword - look up in keywords
        if (ctx.table == nullptr)
            throw std::runtime_error("TaQL: keyword reference without table context");
        auto& kw = ctx.table->keywords();
        auto* found = kw.find(node.name);
        if (found == nullptr) return std::monostate{};
        // Extract scalar types from RecordValue storage variant.
        auto& st = found->storage();
        if (auto* v = std::get_if<double>(&st)) return *v;
        if (auto* v = std::get_if<float>(&st)) return static_cast<double>(*v);
        if (auto* v = std::get_if<std::int32_t>(&st)) return static_cast<std::int64_t>(*v);
        if (auto* v = std::get_if<std::int64_t>(&st)) return *v;
        if (auto* v = std::get_if<std::string>(&st)) return *v;
        if (auto* v = std::get_if<bool>(&st)) return *v;
        return std::monostate{};
    }

    case ExprType::subscript: {
        // Array subscript: arr[index] or arr[start:end]
        auto arr_val = eval_expr(node.children[0], ctx);
        if (node.children.size() < 2)
            throw std::runtime_error("TaQL: subscript requires index expression");
        auto idx_val = eval_expr(node.children[1], ctx);
        auto idx = as_int(idx_val);

        // Index into the appropriate array type
        if (auto* a = std::get_if<std::vector<double>>(&arr_val)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= a->size())
                throw std::runtime_error("TaQL: array index out of bounds");
            return TaqlValue{(*a)[static_cast<std::size_t>(idx)]};
        }
        if (auto* a = std::get_if<std::vector<std::int64_t>>(&arr_val)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= a->size())
                throw std::runtime_error("TaQL: array index out of bounds");
            return TaqlValue{(*a)[static_cast<std::size_t>(idx)]};
        }
        if (auto* a = std::get_if<std::vector<std::complex<double>>>(&arr_val)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= a->size())
                throw std::runtime_error("TaQL: array index out of bounds");
            return TaqlValue{(*a)[static_cast<std::size_t>(idx)]};
        }
        if (auto* a = std::get_if<std::vector<std::string>>(&arr_val)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= a->size())
                throw std::runtime_error("TaQL: array index out of bounds");
            return TaqlValue{(*a)[static_cast<std::size_t>(idx)]};
        }
        if (auto* a = std::get_if<std::vector<bool>>(&arr_val)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= a->size())
                throw std::runtime_error("TaQL: array index out of bounds");
            bool r = (*a)[static_cast<std::size_t>(idx)];
            return TaqlValue{r};
        }
        throw std::runtime_error("TaQL: subscript on non-array value");
    }

    case ExprType::unit_expr: {
        auto val = eval_expr(node.children[0], ctx);
        if (!node.unit.empty()) {
            // Apply unit suffix: convert the value using the unit system
            // For now, store the raw value (unit conversion happens on demand)
            return val;
        }
        return val;
    }

    case ExprType::aggregate_call: {
        // Evaluate aggregate function over group_rows (set by GROUPBY engine)
        if (ctx.group_rows == nullptr || ctx.table == nullptr)
            throw std::runtime_error("TaQL: aggregate function '" + node.name +
                                     "' used outside GROUPBY context");
        auto& grow = *ctx.group_rows;
        auto upper_agg = to_upper(node.name);
        if (upper_agg == "GCOUNT") {
            return static_cast<std::int64_t>(grow.size());
        }
        if (upper_agg == "GROWID") {
            std::vector<std::int64_t> ids;
            ids.reserve(grow.size());
            for (auto row : grow) ids.push_back(static_cast<std::int64_t>(row));
            return TaqlValue{std::move(ids)};
        }
        if (upper_agg == "GANY" || upper_agg == "GALL" ||
            upper_agg == "GNTRUE" || upper_agg == "GNFALSE") {
            std::int64_t ntrue = 0;
            for (auto row : grow) {
                EvalContext rc{ctx.table, row, true, nullptr};
                auto v = eval_expr(node.children[0], rc);
                if (as_bool(v)) ++ntrue;
            }
            auto nfalse = static_cast<std::int64_t>(grow.size()) - ntrue;
            if (upper_agg == "GANY") { bool b = (ntrue > 0); return TaqlValue{b}; }
            if (upper_agg == "GALL") { bool b = (nfalse == 0); return TaqlValue{b}; }
            if (upper_agg == "GNTRUE") return static_cast<std::int64_t>(ntrue);
            return static_cast<std::int64_t>(nfalse);
        }
        // Numeric aggregates: collect values
        std::vector<double> agg_vals;
        agg_vals.reserve(grow.size());
        for (auto row : grow) {
            EvalContext rc{ctx.table, row, true, nullptr};
            auto v = eval_expr(node.children[0], rc);
            agg_vals.push_back(as_double(v));
        }
        if (upper_agg == "GMIN") return TaqlValue{*std::min_element(agg_vals.begin(), agg_vals.end())};
        if (upper_agg == "GMAX") return TaqlValue{*std::max_element(agg_vals.begin(), agg_vals.end())};
        if (upper_agg == "GSUM") {
            double s = 0; for (auto v : agg_vals) s += v;
            return TaqlValue{s};
        }
        if (upper_agg == "GMEAN") {
            double s = 0; for (auto v : agg_vals) s += v;
            return TaqlValue{s / static_cast<double>(agg_vals.size())};
        }
        if (upper_agg == "GMEDIAN") {
            std::sort(agg_vals.begin(), agg_vals.end());
            auto n = agg_vals.size();
            double med = (n % 2 == 0) ? (agg_vals[n/2-1] + agg_vals[n/2]) / 2.0 : agg_vals[n/2];
            return TaqlValue{med};
        }
        if (upper_agg == "GVARIANCE") {
            double mean = 0; for (auto v : agg_vals) mean += v;
            mean /= static_cast<double>(agg_vals.size());
            double var = 0; for (auto v : agg_vals) var += (v - mean) * (v - mean);
            return TaqlValue{var / static_cast<double>(agg_vals.size() - 1)};
        }
        if (upper_agg == "GSTDDEV") {
            double mean = 0; for (auto v : agg_vals) mean += v;
            mean /= static_cast<double>(agg_vals.size());
            double var = 0; for (auto v : agg_vals) var += (v - mean) * (v - mean);
            return TaqlValue{std::sqrt(var / static_cast<double>(agg_vals.size() - 1))};
        }
        if (upper_agg == "GRMS") {
            double sq = 0; for (auto v : agg_vals) sq += v * v;
            return TaqlValue{std::sqrt(sq / static_cast<double>(agg_vals.size()))};
        }
        if (upper_agg == "GFIRST") return TaqlValue{agg_vals.front()};
        if (upper_agg == "GLAST") return TaqlValue{agg_vals.back()};
        if (upper_agg == "GFRACTILE") {
            double frac = as_double(eval_expr(node.children[1], ctx));
            std::sort(agg_vals.begin(), agg_vals.end());
            auto idx = static_cast<std::size_t>(frac * static_cast<double>(agg_vals.size() - 1));
            if (idx >= agg_vals.size()) idx = agg_vals.size() - 1;
            return TaqlValue{agg_vals[idx]};
        }
        throw std::runtime_error("TaQL: unsupported aggregate '" + node.name + "'");
    }

    case ExprType::cone_expr: {
        // INCONE operator: [lon,lat] INCONE [cLon,cLat,radius]
        // Evaluate left side (point) and right side (cone center + radius)
        auto lhs = eval_expr(node.children[0], ctx);
        auto rhs = eval_expr(node.children[1], ctx);
        // Extract point coordinates from lhs (set/array)
        double lon1 = 0.0;
        double lat1 = 0.0;
        if (auto* dv = std::get_if<std::vector<double>>(&lhs)) {
            if (dv->size() < 2) throw std::runtime_error("TaQL: INCONE lhs needs 2 values");
            lon1 = (*dv)[0];
            lat1 = (*dv)[1];
        } else {
            lon1 = as_double(lhs);
            lat1 = (node.children[0].children.size() >= 2)
                       ? as_double(eval_expr(node.children[0].children[1], ctx))
                       : 0.0;
        }
        // Extract cone center + radius from rhs
        double clon = 0.0;
        double clat = 0.0;
        double rad = 0.0;
        if (auto* dv = std::get_if<std::vector<double>>(&rhs)) {
            if (dv->size() < 3) throw std::runtime_error("TaQL: INCONE rhs needs 3 values");
            clon = (*dv)[0];
            clat = (*dv)[1];
            rad = (*dv)[2];
        } else {
            throw std::runtime_error("TaQL: INCONE rhs must be [cLon, cLat, radius]");
        }
        // Vincenty angular distance
        double dlon = clon - lon1;
        double cos_clat = std::cos(clat);
        double sin_clat = std::sin(clat);
        double cos_lat1 = std::cos(lat1);
        double sin_lat1 = std::sin(lat1);
        double a = cos_clat * std::sin(dlon);
        double b = cos_lat1 * sin_clat - sin_lat1 * cos_clat * std::cos(dlon);
        double c = sin_lat1 * sin_clat + cos_lat1 * cos_clat * std::cos(dlon);
        double dist = std::atan2(std::sqrt(a * a + b * b), c);
        bool in_cone = dist <= std::abs(rad);
        return TaqlValue{in_cone};
    }

    case ExprType::around_expr: {
        // AROUND value IN range: |value - center| <= halfwidth
        auto val = as_double(eval_expr(node.children[0], ctx));
        auto center = as_double(eval_expr(node.children[1], ctx));
        auto halfwidth = (node.children.size() > 2)
                             ? as_double(eval_expr(node.children[2], ctx))
                             : 0.0;
        bool r = std::abs(val - center) <= halfwidth;
        return TaqlValue{r};
    }

    case ExprType::exists_expr:
        // EXISTS subquery - requires multi-table context (Wave G)
        throw std::runtime_error("TaQL: EXISTS requires multi-table context");

    case ExprType::subquery:
        throw std::runtime_error("TaQL: subqueries require multi-table context");

    case ExprType::record_literal: {
        // Record literals evaluate to their first value for simplicity
        if (node.children.empty()) return std::monostate{};
        return eval_expr(node.children[0], ctx);
    }

    default:
        throw std::runtime_error("TaQL: unsupported expression type in evaluator");
    }
}

/// Convert a TaQL column definition data-type string to DataType enum.
DataType taql_dtype_to_data_type(const std::string& dt) {
    auto u = to_upper(dt);
    if (u == "BOOL" || u == "B") return DataType::tp_bool;
    if (u == "UCHAR" || u == "UC") return DataType::tp_uchar;
    if (u == "SHORT" || u == "S") return DataType::tp_short;
    if (u == "USHORT" || u == "US") return DataType::tp_ushort;
    if (u == "INT" || u == "I" || u == "I4") return DataType::tp_int;
    if (u == "UINT" || u == "U" || u == "UI") return DataType::tp_uint;
    if (u == "INT64" || u == "I8") return DataType::tp_int64;
    if (u == "FLOAT" || u == "R4" || u == "FLT") return DataType::tp_float;
    if (u == "DOUBLE" || u == "R8" || u == "D" || u == "DBL") return DataType::tp_double;
    if (u == "COMPLEX" || u == "C4") return DataType::tp_complex;
    if (u == "DCOMPLEX" || u == "C8" || u == "DC") return DataType::tp_dcomplex;
    if (u == "STRING" || u == "A") return DataType::tp_string;
    throw std::runtime_error("TaQL: unknown column data type '" + dt + "'");
}

/// Convert TaqlColumnDef to TableColumnSpec.
TableColumnSpec taql_coldef_to_spec(const TaqlColumnDef& cd) {
    TableColumnSpec spec;
    spec.name = cd.name;
    spec.data_type = cd.data_type.empty() ? DataType::tp_double
                                          : taql_dtype_to_data_type(cd.data_type);
    if (!cd.shape.empty()) {
        spec.kind = ColumnKind::array;
        spec.shape = cd.shape;
    } else if (cd.ndim.has_value() && *cd.ndim > 0) {
        spec.kind = ColumnKind::array;
        // No fixed shape known; leave shape empty (variable-shape array)
    } else {
        spec.kind = ColumnKind::scalar;
    }
    spec.comment = cd.comment;
    return spec;
}

/// Convert a TaqlValue to RecordValue for keyword setting.
RecordValue taql_value_to_record_value(const TaqlValue& val) {
    if (auto* b = std::get_if<bool>(&val)) return RecordValue(*b);
    if (auto* i = std::get_if<std::int64_t>(&val)) return RecordValue(static_cast<std::int32_t>(*i));
    if (auto* d = std::get_if<double>(&val)) return RecordValue(*d);
    if (auto* s = std::get_if<std::string>(&val)) return RecordValue(*s);
    throw std::runtime_error("TaQL: unsupported value type for keyword");
}

} // anonymous namespace

// ===========================================================================
// TaQL command execution
// ===========================================================================

TaqlResult taql_execute(std::string_view query, Table& table) {
    auto ast = taql_parse(query);
    TaqlResult result;

    switch (ast.command) {
    case TaqlCommand::show_cmd:
        result.show_text = taql_show(ast.show_topic);
        break;

    case TaqlCommand::select_cmd: {
        // Open joined tables if present
        std::vector<Table> joined_table_storage;
        std::unordered_map<std::string, std::pair<Table*, std::uint64_t>> joined_map;
        for (auto& join : ast.joins) {
            joined_table_storage.push_back(Table::open(join.table.name));
            auto alias = join.table.shorthand.empty() ? join.table.name : join.table.shorthand;
            joined_map[alias] = {&joined_table_storage.back(), 0};
        }
        // Also register the primary table by its shorthand
        if (!ast.tables.empty() && !ast.tables[0].shorthand.empty())
            joined_map[ast.tables[0].shorthand] = {&table, 0};

        EvalContext ctx;
        ctx.table = &table;
        ctx.has_row = true;
        if (!joined_map.empty())
            ctx.joined_tables = &joined_map;

        // ---- JOIN execution (nested-loop) ----
        std::vector<std::uint64_t> filtered_rows;
        if (!ast.joins.empty()) {
            struct JoinRow {
                std::uint64_t main_row;
                std::vector<std::uint64_t> join_rows; // one per join table
            };
            std::vector<JoinRow> join_results;

            auto nrow = table.nrow();
            for (std::uint64_t r = 0; r < nrow; ++r) {
                // For simplicity, support single JOIN
                if (joined_table_storage.size() >= 1) {
                    auto& jtable = joined_table_storage[0];
                    auto& join = ast.joins[0];
                    auto alias = join.table.shorthand.empty() ? join.table.name : join.table.shorthand;
                    auto jnrow = jtable.nrow();

                    for (std::uint64_t jr = 0; jr < jnrow; ++jr) {
                        // Update joined table row in context
                        joined_map[alias] = {&jtable, jr};
                        ctx.row = r;

                        // Evaluate ON predicate
                        bool passes = true;
                        if (join.on_expr.type != ExprType::literal ||
                            !std::holds_alternative<std::monostate>(join.on_expr.value)) {
                            auto on_val = eval_expr(join.on_expr, ctx);
                            passes = as_bool(on_val);
                        }

                        if (passes) {
                            // Apply WHERE filter
                            if (ast.where_expr.has_value()) {
                                auto where_val = eval_expr(*ast.where_expr, ctx);
                                if (!as_bool(where_val)) continue;
                            }
                            join_results.push_back({r, {jr}});
                        }
                    }
                }
            }

            // Helper to set up context for a join result
            auto setup_join_ctx = [&](const JoinRow& jrow) {
                ctx.row = jrow.main_row;
                if (!jrow.join_rows.empty()) {
                    auto& join0 = ast.joins[0];
                    auto a = join0.table.shorthand.empty() ? join0.table.name : join0.table.shorthand;
                    joined_map[a] = {&joined_table_storage[0], jrow.join_rows[0]};
                }
            };

            // ---- JOIN + GROUPBY ----
            if (!ast.group_by.empty()) {
                // Group join results by key expressions
                std::vector<std::pair<std::string, std::vector<std::size_t>>> groups;
                std::unordered_map<std::string, std::size_t> group_index;

                for (std::size_t ji = 0; ji < join_results.size(); ++ji) {
                    setup_join_ctx(join_results[ji]);
                    std::string key;
                    for (auto& gexpr : ast.group_by) {
                        auto v = eval_expr(gexpr, ctx);
                        key += as_string(v) + "|";
                    }
                    auto it = group_index.find(key);
                    if (it == group_index.end()) {
                        group_index[key] = groups.size();
                        groups.emplace_back(key, std::vector<std::size_t>{ji});
                    } else {
                        groups[it->second].second.push_back(ji);
                    }
                }

                // For each group, evaluate projections
                for (auto& [gkey, gindices] : groups) {
                    // Collect main-table row indices for this group (for aggregate functions)
                    std::vector<std::uint64_t> group_main_rows;
                    group_main_rows.reserve(gindices.size());
                    for (auto gi : gindices)
                        group_main_rows.push_back(join_results[gi].main_row);

                    // Set up context with first row of group + group_rows for aggregates
                    setup_join_ctx(join_results[gindices[0]]);
                    ctx.group_rows = &group_main_rows;

                    result.rows.push_back(join_results[gindices[0]].main_row);
                    for (auto& proj : ast.projections) {
                        result.values.push_back(eval_expr(proj.expr, ctx));
                    }
                }
                ctx.group_rows = nullptr;

                // HAVING
                if (ast.having_expr.has_value()) {
                    // Re-filter groups (rebuild groups to get HAVING context)
                    // For simplicity, since results are already flattened, skip HAVING for now
                }

                result.affected_rows = groups.size();
                break;
            }

            // ---- JOIN without GROUPBY: direct projection ----
            for (auto& jr : join_results) {
                result.rows.push_back(jr.main_row);
                setup_join_ctx(jr);
                for (auto& proj : ast.projections) {
                    if (proj.expr.type == ExprType::wildcard) {
                        for (auto& col : table.columns()) {
                            auto cv = table.read_scalar_cell(col.name, jr.main_row);
                            result.values.push_back(cell_to_taql(cv));
                        }
                    } else {
                        result.values.push_back(eval_expr(proj.expr, ctx));
                    }
                }
            }
            break;
        }

        // ---- Non-JOIN SELECT ----
        auto nrow = table.nrow();
        for (std::uint64_t r = 0; r < nrow; ++r) {
            ctx.row = r;
            if (ast.where_expr.has_value()) {
                auto where_val = eval_expr(*ast.where_expr, ctx);
                if (!as_bool(where_val)) continue;
            }
            filtered_rows.push_back(r);
        }

        // ---- GROUPBY engine ----
        if (!ast.group_by.empty()) {
            // Group filtered rows by key expressions
            // Use string serialization of values as group key
            std::vector<std::pair<std::string, std::vector<std::uint64_t>>> groups;
            std::unordered_map<std::string, std::size_t> group_index;

            for (auto r : filtered_rows) {
                EvalContext rc{&table, r, true};
                std::string key;
                for (auto& gexpr : ast.group_by) {
                    auto v = eval_expr(gexpr, rc);
                    key += as_string(v) + "|";
                }
                auto it = group_index.find(key);
                if (it == group_index.end()) {
                    group_index[key] = groups.size();
                    groups.emplace_back(key, std::vector<std::uint64_t>{r});
                } else {
                    groups[it->second].second.push_back(r);
                }
            }

            // For each group, evaluate projections using eval_expr with group context
            struct GroupResult {
                std::vector<TaqlValue> values;
                std::uint64_t first_row;
            };
            std::vector<GroupResult> group_results;
            group_results.reserve(groups.size());

            for (auto& [gkey, grow] : groups) {
                GroupResult gr;
                gr.first_row = grow[0];

                // Context with group_rows set for aggregate evaluation
                EvalContext gc{&table, grow[0], true, &grow};

                for (auto& proj : ast.projections) {
                    if (proj.expr.type == ExprType::wildcard) {
                        for (auto& col : table.columns()) {
                            auto cv = table.read_scalar_cell(col.name, grow[0]);
                            gr.values.push_back(cell_to_taql(cv));
                        }
                    } else {
                        // eval_expr handles aggregate_call via group_rows
                        gr.values.push_back(eval_expr(proj.expr, gc));
                    }
                }
                group_results.push_back(std::move(gr));
            }

            // Apply HAVING filter
            if (ast.having_expr.has_value()) {
                std::vector<GroupResult> having_passed;
                for (std::size_t gi = 0; gi < group_results.size(); ++gi) {
                    auto& gr = group_results[gi];
                    auto& grow = groups[gi].second;
                    EvalContext hc{&table, gr.first_row, true, &grow};
                    auto hv = eval_expr(*ast.having_expr, hc);
                    if (as_bool(hv))
                        having_passed.push_back(std::move(gr));
                }
                group_results = std::move(having_passed);
            }

            // Flatten group results into result.values
            for (auto& gr : group_results) {
                result.rows.push_back(gr.first_row);
                for (auto& v : gr.values)
                    result.values.push_back(std::move(v));
            }
            result.affected_rows = group_results.size();
            break;
        }

        // ---- Non-grouped SELECT (original path) ----
        result.rows = std::move(filtered_rows);

        // ORDERBY: sort result rows
        if (!ast.order_by.empty()) {
            auto& sort_keys = ast.order_by;
            std::sort(result.rows.begin(), result.rows.end(),
                      [&](std::uint64_t a, std::uint64_t b) {
                          EvalContext ca{&table, a, true};
                          EvalContext cb{&table, b, true};
                          for (auto& key : sort_keys) {
                              auto va = eval_expr(key.expr, ca);
                              auto vb = eval_expr(key.expr, cb);
                              int cmp = compare_values(va, vb);
                              if (key.order == SortOrder::descending) cmp = -cmp;
                              if (cmp != 0) return cmp < 0;
                          }
                          return false;
                      });
        }

        // DISTINCT: remove duplicate rows based on projection values
        if (ast.select_distinct && !ast.projections.empty()) {
            std::vector<std::uint64_t> unique_rows;
            std::unordered_set<std::string> seen;
            for (auto r : result.rows) {
                EvalContext rc{&table, r, true};
                std::string key;
                for (auto& proj : ast.projections) {
                    auto v = eval_expr(proj.expr, rc);
                    key += as_string(v) + "|";
                }
                if (seen.insert(key).second) {
                    unique_rows.push_back(r);
                }
            }
            result.rows = std::move(unique_rows);
        }

        // LIMIT / OFFSET
        if (ast.offset_expr.has_value()) {
            EvalContext empty_ctx{};
            auto offset = static_cast<std::size_t>(as_int(eval_expr(*ast.offset_expr, empty_ctx)));
            if (offset < result.rows.size()) {
                result.rows.erase(result.rows.begin(),
                                  result.rows.begin() + static_cast<std::ptrdiff_t>(offset));
            } else {
                result.rows.clear();
            }
        }
        if (ast.limit_expr.has_value()) {
            EvalContext empty_ctx{};
            auto limit = static_cast<std::size_t>(as_int(eval_expr(*ast.limit_expr, empty_ctx)));
            if (limit < result.rows.size()) {
                result.rows.resize(limit);
            }
        }

        // Build column names from projections
        for (auto& proj : ast.projections) {
            if (!proj.alias.empty()) {
                result.column_names.push_back(proj.alias);
            } else if (proj.expr.type == ExprType::column_ref) {
                result.column_names.push_back(proj.expr.name);
            } else if (proj.expr.type == ExprType::wildcard) {
                for (auto& col : table.columns()) {
                    result.column_names.push_back(col.name);
                }
            } else {
                result.column_names.push_back("expr");
            }
        }

        // Evaluate projections for each result row and store values
        for (auto r : result.rows) {
            EvalContext rc{&table, r, true};
            for (auto& proj : ast.projections) {
                if (proj.expr.type == ExprType::wildcard) {
                    // All columns
                    for (auto& col : table.columns()) {
                        auto cv = table.read_scalar_cell(col.name, r);
                        result.values.push_back(cell_to_taql(cv));
                    }
                } else {
                    result.values.push_back(eval_expr(proj.expr, rc));
                }
            }
        }
        break;
    }

    case TaqlCommand::update_cmd: {
        if (!table.is_writable())
            throw std::runtime_error("TaQL: UPDATE requires writable table");

        EvalContext ctx;
        ctx.table = &table;
        ctx.has_row = true;

        auto nrow = table.nrow();
        std::uint64_t affected = 0;
        for (std::uint64_t r = 0; r < nrow; ++r) {
            ctx.row = r;
            if (ast.where_expr.has_value()) {
                auto where_val = eval_expr(*ast.where_expr, ctx);
                if (!as_bool(where_val)) continue;
            }
            // Apply assignments
            for (auto& asgn : ast.assignments) {
                auto val = eval_expr(asgn.value, ctx);
                // Convert TaqlValue to CellValue
                CellValue cv;
                if (auto* b = std::get_if<bool>(&val)) cv = *b;
                else if (auto* i = std::get_if<std::int64_t>(&val)) cv = static_cast<std::int32_t>(*i);
                else if (auto* d = std::get_if<double>(&val)) cv = *d;
                else if (auto* s = std::get_if<std::string>(&val)) cv = *s;
                else if (auto* c = std::get_if<std::complex<double>>(&val)) cv = *c;
                else throw std::runtime_error("TaQL: unsupported value type in UPDATE");
                table.write_scalar_cell(asgn.column, r, cv);
            }
            result.rows.push_back(r);
            ++affected;
        }
        result.affected_rows = affected;
        if (affected > 0) table.flush();
        break;
    }

    case TaqlCommand::delete_cmd: {
        if (!table.is_writable())
            throw std::runtime_error("TaQL: DELETE requires writable table");

        EvalContext ctx;
        ctx.table = &table;
        ctx.has_row = true;

        // Collect rows to delete
        std::vector<std::uint64_t> rows_to_delete;
        auto nrow = table.nrow();
        for (std::uint64_t r = 0; r < nrow; ++r) {
            ctx.row = r;
            if (ast.where_expr.has_value()) {
                auto where_val = eval_expr(*ast.where_expr, ctx);
                if (!as_bool(where_val)) continue;
            }
            rows_to_delete.push_back(r);
        }

        // Apply ORDERBY + LIMIT if present
        // (simplified: just apply LIMIT to the collected set)
        if (ast.limit_expr.has_value()) {
            EvalContext limit_ctx;
            auto limit_val = eval_expr(*ast.limit_expr, limit_ctx);
            auto limit = static_cast<std::size_t>(as_int(limit_val));
            if (rows_to_delete.size() > limit)
                rows_to_delete.resize(limit);
        }

        result.affected_rows = rows_to_delete.size();
        result.rows = rows_to_delete;
        if (!rows_to_delete.empty()) {
            table.remove_rows(std::move(rows_to_delete));
            table.flush();
        }
        break;
    }


    case TaqlCommand::count_cmd: {
        EvalContext ctx;
        ctx.table = &table;
        ctx.has_row = true;
        auto nrow = table.nrow();
        std::uint64_t count = 0;
        for (std::uint64_t r = 0; r < nrow; ++r) {
            ctx.row = r;
            if (ast.where_expr.has_value()) {
                auto where_val = eval_expr(*ast.where_expr, ctx);
                if (!as_bool(where_val)) continue;
            }
            ++count;
        }
        result.affected_rows = count;
        result.values.push_back(static_cast<std::int64_t>(count));
        break;
    }

    case TaqlCommand::calc_cmd: {
        if (ast.calc_expr.has_value()) {
            EvalContext ctx;
            ctx.table = &table;
            ctx.has_row = false;
            auto val = eval_expr(*ast.calc_expr, ctx);
            result.values.push_back(val);
        }
        break;
    }

    case TaqlCommand::insert_cmd: {
        if (!table.is_writable())
            throw std::runtime_error("TaQL: INSERT requires writable table");

        EvalContext ctx;
        ctx.table = &table;
        ctx.has_row = false;

        // Determine target columns
        std::vector<std::string> cols = ast.insert_columns;
        if (cols.empty()) {
            for (auto& c : table.columns())
                cols.push_back(c.name);
        }

        // INSERT ... VALUES (row1), (row2), ...
        std::uint64_t added = 0;
        for (auto& row_exprs : ast.insert_values) {
            auto start_row = table.nrow();
            table.add_rows(1);
            ctx.row = start_row;
            ctx.has_row = true;
            for (std::size_t i = 0; i < row_exprs.size() && i < cols.size(); ++i) {
                auto val = eval_expr(row_exprs[i], ctx);
                CellValue cv;
                if (auto* b = std::get_if<bool>(&val)) cv = *b;
                else if (auto* ii = std::get_if<std::int64_t>(&val)) cv = static_cast<std::int32_t>(*ii);
                else if (auto* d = std::get_if<double>(&val)) cv = *d;
                else if (auto* s = std::get_if<std::string>(&val)) cv = *s;
                else if (auto* c = std::get_if<std::complex<double>>(&val)) cv = *c;
                else throw std::runtime_error("TaQL: unsupported value type in INSERT");
                table.write_scalar_cell(cols[i], start_row, cv);
            }
            result.rows.push_back(start_row);
            ++added;
        }
        result.affected_rows = added;
        if (added > 0) table.flush();
        break;
    }


    case TaqlCommand::create_table_cmd: {
        // CREATE TABLE creates a new table at the path specified in the AST
        std::string table_path;
        if (!ast.tables.empty())
            table_path = ast.tables[0].name;
        if (table_path.empty())
            throw std::runtime_error("TaQL: CREATE TABLE requires a table name/path");

        std::vector<TableColumnSpec> specs;
        specs.reserve(ast.create_columns.size());
        for (auto& cd : ast.create_columns)
            specs.push_back(taql_coldef_to_spec(cd));

        // Determine initial row count from LIMIT clause
        std::uint64_t init_rows = 0;
        if (ast.limit_expr.has_value()) {
            EvalContext limit_ctx;
            auto lv = eval_expr(*ast.limit_expr, limit_ctx);
            init_rows = static_cast<std::uint64_t>(as_int(lv));
        }

        auto new_table = Table::create(table_path, specs, init_rows);
        new_table.flush();
        result.affected_rows = init_rows;
        break;
    }

    case TaqlCommand::alter_table_cmd: {
        if (!table.is_writable())
            throw std::runtime_error("TaQL: ALTER TABLE requires writable table");

        EvalContext ctx;
        ctx.table = &table;
        ctx.has_row = false;

        for (auto& step : ast.alter_steps) {
            switch (step.action) {
            case AlterAction::add_column:
                for (auto& cd : step.columns) {
                    auto spec = taql_coldef_to_spec(cd);
                    table.add_column(spec);
                }
                break;
            case AlterAction::copy_column:
                for (auto& [from, to] : step.renames)
                    table.copy_column(from, to);
                break;
            case AlterAction::rename_column:
                for (auto& [from, to] : step.renames)
                    table.rename_column(from, to);
                break;
            case AlterAction::drop_column:
                for (auto& name : step.names)
                    table.remove_column(name);
                break;
            case AlterAction::set_keyword:
                for (auto& [key, expr] : step.keywords) {
                    auto val = eval_expr(expr, ctx);
                    table.set_keyword(key, taql_value_to_record_value(val));
                }
                break;
            case AlterAction::copy_keyword:
                // Copy keyword: read value of source keyword, set on dest
                for (auto& [from, to] : step.renames) {
                    auto* rv = table.keywords().find(from);
                    if (rv != nullptr) table.set_keyword(to, *rv);
                }
                break;
            case AlterAction::rename_keyword:
                for (auto& [from, to] : step.renames) {
                    auto* rv = table.keywords().find(from);
                    if (rv != nullptr) {
                        table.set_keyword(to, *rv);
                        table.remove_keyword(from);
                    }
                }
                break;
            case AlterAction::drop_keyword:
                for (auto& name : step.names)
                    table.remove_keyword(name);
                break;
            case AlterAction::add_row: {
                auto rv = eval_expr(step.row_count, ctx);
                auto n = static_cast<std::uint64_t>(as_int(rv));
                table.add_rows(n);
                result.affected_rows += n;
                break;
            }
            }
        }
        table.flush();
        break;
    }

    case TaqlCommand::drop_table_cmd: {
        for (auto& path : ast.drop_tables)
            Table::drop(path);
        break;
    }
    }

    return result;
}

TaqlResult taql_calc(std::string_view query) {
    auto ast = taql_parse(query);
    TaqlResult result;

    if (ast.command == TaqlCommand::show_cmd) {
        result.show_text = taql_show(ast.show_topic);
        return result;
    }

    // For CALC, evaluate the expression without table context
    if (ast.calc_expr.has_value()) {
        EvalContext ctx;
        auto val = eval_expr(*ast.calc_expr, ctx);
        result.values.push_back(val);
    }

    // CREATE TABLE can be executed without a table reference
    if (ast.command == TaqlCommand::create_table_cmd) {
        std::string table_path;
        if (!ast.tables.empty())
            table_path = ast.tables[0].name;
        if (table_path.empty())
            throw std::runtime_error("TaQL: CREATE TABLE requires a table name/path");

        std::vector<TableColumnSpec> specs;
        specs.reserve(ast.create_columns.size());
        for (auto& cd : ast.create_columns)
            specs.push_back(taql_coldef_to_spec(cd));

        std::uint64_t init_rows = 0;
        if (ast.limit_expr.has_value()) {
            EvalContext ctx;
            auto lv = eval_expr(*ast.limit_expr, ctx);
            init_rows = static_cast<std::uint64_t>(as_int(lv));
        }

        auto new_table = Table::create(table_path, specs, init_rows);
        new_table.flush();
        result.affected_rows = init_rows;
    }

    // DROP TABLE can be executed without a table reference
    if (ast.command == TaqlCommand::drop_table_cmd) {
        for (auto& path : ast.drop_tables)
            Table::drop(path);
    }

    return result;
}

} // namespace casacore_mini
