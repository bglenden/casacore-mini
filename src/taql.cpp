#include "casacore_mini/taql.hpp"
#include "casacore_mini/measure_convert.hpp"
#include "casacore_mini/table.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
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
        if (match_keyword("AS")) {
            ref.shorthand = std::string(current_.text);
            advance();
        } else if (current_.type == TokenType::identifier &&
                   !is_select_clause_keyword(current_.text)) {
            // Implicit shorthand (next word is alias if it's not a keyword)
        }

        return ref;
    }

    std::string parse_table_name() {
        std::string name;
        if (current_.type == TokenType::string_lit) {
            name = current_.str_val;
            advance();
            return name;
        }
        if (current_.type == TokenType::lbracket) {
            // [path/to/table]
            advance();
            while (current_.type != TokenType::rbracket && !at_end()) {
                name += std::string(current_.text);
                advance();
            }
            expect(TokenType::rbracket);
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

            // Dotted column reference (table.column)
            if (current_.type == TokenType::dot) {
                std::string dotted = name;
                while (match(TokenType::dot)) {
                    dotted += ".";
                    dotted += std::string(current_.text);
                    advance();
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
            "WITH",   "TABLE"};
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
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::int32_t> ||
                                 std::is_same_v<T, std::uint32_t>) {
                return static_cast<std::int64_t>(v);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return v;
            } else if constexpr (std::is_same_v<T, float>) {
                return static_cast<double>(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::complex<float>>) {
                return std::complex<double>(v.real(), v.imag());
            } else if constexpr (std::is_same_v<T, std::complex<double>>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v;
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

// ---------------------------------------------------------------------------
// Array value helpers
// ---------------------------------------------------------------------------

/// Extract a vector<double> from a TaqlValue; works for scalar and vector types.
std::vector<double> as_double_vec(const TaqlValue& v) {
    if (auto* vec = std::get_if<std::vector<double>>(&v)) return *vec;
    if (auto* vec = std::get_if<std::vector<std::int64_t>>(&v)) {
        std::vector<double> out;
        out.reserve(vec->size());
        for (auto x : *vec) out.push_back(static_cast<double>(x));
        return out;
    }
    if (auto* vec = std::get_if<std::vector<bool>>(&v)) {
        std::vector<double> out;
        out.reserve(vec->size());
        for (bool x : *vec) out.push_back(x ? 1.0 : 0.0);
        return out;
    }
    // Scalar: return single-element vector.
    return {as_double(v)};
}

/// Get the number of elements in a TaqlValue (1 for scalars, N for vectors).
std::int64_t value_nelem(const TaqlValue& v) {
    if (auto* vec = std::get_if<std::vector<double>>(&v)) return static_cast<std::int64_t>(vec->size());
    if (auto* vec = std::get_if<std::vector<std::int64_t>>(&v)) return static_cast<std::int64_t>(vec->size());
    if (auto* vec = std::get_if<std::vector<bool>>(&v)) return static_cast<std::int64_t>(vec->size());
    if (auto* vec = std::get_if<std::vector<std::complex<double>>>(&v)) return static_cast<std::int64_t>(vec->size());
    if (auto* vec = std::get_if<std::vector<std::string>>(&v)) return static_cast<std::int64_t>(vec->size());
    if (std::holds_alternative<std::monostate>(v)) return 0;
    return 1; // scalar
}

/// Get the number of dimensions of a TaqlValue (0 for scalars, 1 for vectors).
std::int64_t value_ndim(const TaqlValue& v) {
    if (std::holds_alternative<std::vector<double>>(v) ||
        std::holds_alternative<std::vector<std::int64_t>>(v) ||
        std::holds_alternative<std::vector<bool>>(v) ||
        std::holds_alternative<std::vector<std::complex<double>>>(v) ||
        std::holds_alternative<std::vector<std::string>>(v))
        return 1;
    if (std::holds_alternative<std::monostate>(v)) return 0;
    return 0; // scalar
}

// ---------------------------------------------------------------------------
// DateTime helpers (MJD-based, matching casacore convention)
// ---------------------------------------------------------------------------

/// MJD epoch: November 17, 1858 00:00:00 UTC.
static constexpr double kSecondsPerDay = 86400.0;

/// Convert MJD (days) to a broken-down time (UTC).
struct BrokenTime {
    int year, month, day, hour, minute, second;
    int weekday; // 0=Sunday
};

BrokenTime mjd_to_broken(double mjd) {
    // MJD -> Julian Day Number -> Gregorian date (Fliegel & Van Flandern algorithm).
    auto jd = static_cast<std::int64_t>(mjd + 2400001); // JD at noon
    auto l = jd + 68569;
    auto n = 4 * l / 146097;
    l = l - (146097 * n + 3) / 4;
    auto i = 4000 * (l + 1) / 1461001;
    l = l - 1461 * i / 4 + 31;
    auto j = 80 * l / 2447;
    BrokenTime bt{};
    bt.day = static_cast<int>(l - 2447 * j / 80);
    l = j / 11;
    bt.month = static_cast<int>(j + 2 - 12 * l);
    bt.year = static_cast<int>(100 * (n - 49) + i + l);

    double frac_day = mjd - std::floor(mjd);
    double seconds_in_day = frac_day * kSecondsPerDay;
    bt.hour = static_cast<int>(seconds_in_day / 3600.0);
    bt.minute = static_cast<int>(std::fmod(seconds_in_day, 3600.0) / 60.0);
    bt.second = static_cast<int>(std::fmod(seconds_in_day, 60.0));

    // Weekday: JD % 7 -> 0=Monday in Julian scheme, adjust to 0=Sunday.
    bt.weekday = static_cast<int>((jd + 1) % 7); // 0=Sunday

    return bt;
}

/// Month name lookup.
const char* month_name(int m) {
    static const char* names[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (m < 1 || m > 12) return "???";
    return names[m];
}

/// Day-of-week name lookup.
const char* dow_name(int wd) {
    static const char* names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (wd < 0 || wd > 6) return "???";
    return names[wd];
}

/// ISO week number (approximate — no Monday-week ISO 8601 edge case handling).
int iso_week(const BrokenTime& bt) {
    // Day of year.
    static const int mdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int doy = bt.day;
    for (int i = 1; i < bt.month; ++i) doy += mdays[i];
    bool leap = (bt.year % 4 == 0 && bt.year % 100 != 0) || (bt.year % 400 == 0);
    if (leap && bt.month > 2) ++doy;
    return (doy - 1) / 7 + 1;
}

// ---------------------------------------------------------------------------
// Unit conversion factor
// ---------------------------------------------------------------------------

/// Return conversion factor to convert from given unit to SI base unit.
/// Angles → radians, time → seconds, length → meters, frequency → Hz, etc.
double unit_to_si(const std::string& unit) {
    auto u = to_upper(unit);
    // Angle units → radians
    if (u == "DEG") return 3.14159265358979323846 / 180.0;
    if (u == "RAD") return 1.0;
    if (u == "ARCSEC") return 3.14159265358979323846 / 648000.0;
    if (u == "ARCMIN") return 3.14159265358979323846 / 10800.0;
    if (u == "MAS") return 3.14159265358979323846 / 648000000.0;
    // Time units → seconds
    if (u == "S") return 1.0;
    if (u == "MS") return 1e-3;
    if (u == "US") return 1e-6;
    if (u == "NS") return 1e-9;
    if (u == "MIN") return 60.0;
    if (u == "H") return 3600.0;
    if (u == "D") return 86400.0;
    // Length units → meters
    if (u == "M") return 1.0;
    if (u == "KM") return 1000.0;
    if (u == "CM") return 0.01;
    if (u == "MM") return 0.001;
    if (u == "AU") return 1.495978707e11;
    if (u == "PC") return 3.0856775814913673e16;
    if (u == "KPC") return 3.0856775814913673e19;
    if (u == "MPC") return 3.0856775814913673e22;
    // Frequency → Hz
    if (u == "HZ") return 1.0;
    if (u == "KHZ") return 1e3;
    if (u == "MHZ") return 1e6;
    if (u == "GHZ") return 1e9;
    // Flux → Jy
    if (u == "JY") return 1.0;
    if (u == "MJY") return 1e-3;
    if (u == "KJY") return 1e3;
    // Temperature → K
    if (u == "K") return 1.0;
    if (u == "MK") return 1e-3;
    // Velocity → m/s
    if (u == "M_S" || u == "M/S") return 1.0;
    if (u == "KM_S" || u == "KM/S") return 1000.0;
    // Other
    if (u == "PA") return 1.0;
    if (u == "MBAR") return 100.0;
    if (u == "LAMBDA") return 1.0; // wavelength units (dimensionless multiplier)
    throw std::runtime_error("TaQL: unknown unit '" + unit + "'");
}

// ---------------------------------------------------------------------------
// Angle formatting helpers
// ---------------------------------------------------------------------------

/// Format radians as HMS string (HH:MM:SS.sss).
std::string rad_to_hms(double rad) {
    constexpr double kRad2Hour = 12.0 / 3.14159265358979323846;
    double hours = rad * kRad2Hour;
    if (hours < 0) hours += 24.0;
    auto h = static_cast<int>(hours);
    double mf = (hours - h) * 60.0;
    auto m = static_cast<int>(mf);
    double s = (mf - m) * 60.0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%06.3f", h, m, s);
    return buf;
}

/// Format radians as DMS string (+DD.MM.SS.sss).
std::string rad_to_dms(double rad) {
    constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;
    double deg = rad * kRad2Deg;
    char sign = '+';
    if (deg < 0) { sign = '-'; deg = -deg; }
    auto d = static_cast<int>(deg);
    double mf = (deg - d) * 60.0;
    auto m = static_cast<int>(mf);
    double s = (mf - m) * 60.0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%c%02d.%02d.%06.3f", sign, d, m, s);
    return buf;
}

/// Normalize angle to [0, 2*PI).
double norm_angle(double rad) {
    constexpr double k2Pi = 2.0 * 3.14159265358979323846;
    double a = std::fmod(rad, k2Pi);
    if (a < 0) a += k2Pi;
    return a;
}

/// Angular distance on the sphere between two directions (lon1, lat1, lon2, lat2) in radians.
double ang_dist(double lon1, double lat1, double lon2, double lat2) {
    // Vincenty formula for numerical stability.
    double dlon = lon2 - lon1;
    double cos_lat1 = std::cos(lat1);
    double cos_lat2 = std::cos(lat2);
    double sin_lat1 = std::sin(lat1);
    double sin_lat2 = std::sin(lat2);
    double term1 = cos_lat2 * std::sin(dlon);
    double term2 = cos_lat1 * sin_lat2 - sin_lat1 * cos_lat2 * std::cos(dlon);
    double num = std::sqrt(term1 * term1 + term2 * term2);
    double den = sin_lat1 * sin_lat2 + cos_lat1 * cos_lat2 * std::cos(dlon);
    return std::atan2(num, den);
}

/// Per-table row binding for multi-table queries.
struct TableBinding {
    Table* table = nullptr;
    std::uint64_t row = 0;
};

/// Row evaluation context.
struct EvalContext {
    Table* table = nullptr;
    std::uint64_t row = 0;
    bool has_row = false;
    /// Extra table bindings keyed by shorthand/alias.
    std::unordered_map<std::string, TableBinding> extra_tables;

    /// Resolve a column reference that may be qualified (alias.column).
    /// Returns {table_ptr, row, unqualified_column_name}.
    std::tuple<Table*, std::uint64_t, std::string> resolve_column(const std::string& name) const {
        auto dot = name.find('.');
        if (dot != std::string::npos) {
            auto alias = name.substr(0, dot);
            auto col = name.substr(dot + 1);
            auto it = extra_tables.find(alias);
            if (it != extra_tables.end()) {
                return {it->second.table, it->second.row, col};
            }
            // Fall through: the dot may be part of a column name.
        }
        return {table, row, name};
    }
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
                if (std::isalpha(uc)) {
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

        // Complex-aware functions (check before as_double to avoid type error)
        if (upper == "REAL") {
            if (auto* cz = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{cz->real()};
            return TaqlValue{as_double(args[0])};
        }
        if (upper == "IMAG") {
            if (auto* cz = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{cz->imag()};
            return TaqlValue{0.0};
        }
        if (upper == "NORM") {
            if (auto* cz = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{std::norm(*cz)};
            double xv = as_double(args[0]);
            return TaqlValue{xv * xv};
        }
        if (upper == "ARG") {
            if (auto* cz = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{std::arg(*cz)};
            return TaqlValue{0.0};
        }
        if (upper == "CONJ") {
            if (auto* cz = std::get_if<std::complex<double>>(&args[0]))
                return TaqlValue{std::conj(*cz)};
            return TaqlValue{as_double(args[0])};
        }
        if (upper == "BOOL") {
            if (auto* d = std::get_if<double>(&args[0])) { bool r = (*d != 0.0); return TaqlValue{r}; }
            if (auto* i = std::get_if<std::int64_t>(&args[0])) { bool r = (*i != 0); return TaqlValue{r}; }
            if (auto* b = std::get_if<bool>(&args[0])) return TaqlValue{*b};
            bool r = (as_double(args[0]) != 0.0); return TaqlValue{r};
        }
        if (upper == "STRING") return as_string(args[0]);

        // Array info/reduction functions (check before as_double)
        if (upper == "NDIM") return value_ndim(args[0]);
        if (upper == "NELEM" || upper == "NELEMENTS") return value_nelem(args[0]);
        if (upper == "SHAPE") {
            auto n = value_nelem(args[0]);
            if (value_ndim(args[0]) == 0) return std::vector<std::int64_t>{};
            return std::vector<std::int64_t>{n};
        }
        if (upper == "ARRSUM" || upper == "SUM") {
            auto v = as_double_vec(args[0]);
            return TaqlValue{std::accumulate(v.begin(), v.end(), 0.0)};
        }
        if (upper == "ARRMIN") {
            auto v = as_double_vec(args[0]);
            if (v.empty()) return TaqlValue{0.0};
            return TaqlValue{*std::min_element(v.begin(), v.end())};
        }
        if (upper == "ARRMAX") {
            auto v = as_double_vec(args[0]);
            if (v.empty()) return TaqlValue{0.0};
            return TaqlValue{*std::max_element(v.begin(), v.end())};
        }
        if (upper == "ARRMEAN" || upper == "MEAN") {
            auto v = as_double_vec(args[0]);
            if (v.empty()) return TaqlValue{0.0};
            return TaqlValue{std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size())};
        }
        if (upper == "ARRMEDIAN" || upper == "MEDIAN") {
            auto v = as_double_vec(args[0]);
            if (v.empty()) return TaqlValue{0.0};
            std::sort(v.begin(), v.end());
            auto n = v.size();
            if (n % 2 == 1) return TaqlValue{v[n / 2]};
            return TaqlValue{(v[n / 2 - 1] + v[n / 2]) / 2.0};
        }
        if (upper == "ARRVARIANCE" || upper == "VARIANCE") {
            auto v = as_double_vec(args[0]);
            if (v.size() < 2) return TaqlValue{0.0};
            double mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
            double ss = 0.0;
            for (auto xi : v) ss += (xi - mean) * (xi - mean);
            return TaqlValue{ss / static_cast<double>(v.size())};
        }
        if (upper == "ARRSTDDEV" || upper == "STDDEV") {
            auto v = as_double_vec(args[0]);
            if (v.size() < 2) return TaqlValue{0.0};
            double mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
            double ss = 0.0;
            for (auto xi : v) ss += (xi - mean) * (xi - mean);
            return TaqlValue{std::sqrt(ss / static_cast<double>(v.size()))};
        }
        if (upper == "ARRRMS" || upper == "RMS") {
            auto v = as_double_vec(args[0]);
            if (v.empty()) return TaqlValue{0.0};
            double ss = 0.0;
            for (auto xi : v) ss += xi * xi;
            return TaqlValue{std::sqrt(ss / static_cast<double>(v.size()))};
        }
        if (upper == "ARRAVDEV" || upper == "AVDEV") {
            auto v = as_double_vec(args[0]);
            if (v.empty()) return TaqlValue{0.0};
            double mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
            double ad = 0.0;
            for (auto xi : v) ad += std::abs(xi - mean);
            return TaqlValue{ad / static_cast<double>(v.size())};
        }
        if (upper == "ARRANY" || upper == "ANY") {
            auto v = as_double_vec(args[0]);
            bool r = std::any_of(v.begin(), v.end(), [](double d) { return d != 0.0; });
            return TaqlValue{r};
        }
        if (upper == "ARRALL" || upper == "ALL") {
            auto v = as_double_vec(args[0]);
            bool r = std::all_of(v.begin(), v.end(), [](double d) { return d != 0.0; });
            return TaqlValue{r};
        }
        if (upper == "ARRNTRUE" || upper == "NTRUE") {
            auto v = as_double_vec(args[0]);
            auto cnt = std::count_if(v.begin(), v.end(), [](double d) { return d != 0.0; });
            return static_cast<std::int64_t>(cnt);
        }
        if (upper == "ARRNFALSE" || upper == "NFALSE") {
            auto v = as_double_vec(args[0]);
            auto cnt = std::count_if(v.begin(), v.end(), [](double d) { return d == 0.0; });
            return static_cast<std::int64_t>(cnt);
        }
        if (upper == "ARRFLAT" || upper == "FLATTEN") {
            if (auto* vp = std::get_if<std::vector<double>>(&args[0])) return *vp;
            return std::vector<double>{as_double(args[0])};
        }
        if (upper == "AREVERSE" || upper == "REVERSE") {
            auto v = as_double_vec(args[0]);
            std::reverse(v.begin(), v.end());
            return TaqlValue{std::move(v)};
        }

        // DATETIME(string) — parse date string to MJD
        if (upper == "DATETIME") {
            auto s = as_string(args[0]);
            int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
            if (std::sscanf(s.c_str(), "%d/%d/%d/%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) >= 3 ||
                std::sscanf(s.c_str(), "%d-%d-%d", &y, &mo, &d) >= 3) {
                auto a = (14 - mo) / 12;
                auto yr = y + 4800 - a;
                auto m = mo + 12 * a - 3;
                auto jdn = d + (153 * m + 2) / 5 + 365 * yr + yr / 4 - yr / 100 + yr / 400 - 32045;
                double mjd = static_cast<double>(jdn) - 2400001.0;
                mjd += (h * 3600.0 + mi * 60.0 + sec) / kSecondsPerDay;
                return TaqlValue{mjd};
            }
            throw std::runtime_error("TaQL: DATETIME() cannot parse '" + s + "'");
        }

        // Pattern construction functions
        if (upper == "REGEX") return as_string(args[0]);
        if (upper == "PATTERN" || upper == "SQLPATTERN") {
            auto s = as_string(args[0]);
            std::string re;
            for (char c : s) {
                switch (c) {
                case '%': case '*': re += ".*"; break;
                case '?': case '_': re += '.'; break;
                case '.': case '(': case ')': case '[': case ']':
                case '{': case '}': case '^': case '$': case '|':
                case '+': case '\\': re += '\\'; re += c; break;
                default: re += c; break;
                }
            }
            return re;
        }

        // Numeric functions (safe to call as_double here — only for numeric types)
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
        if (upper == "ABS") return TaqlValue{std::abs(x)};
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

        // DateTime functions (single-arg: MJD in days)
        if (upper == "MJD") return TaqlValue{x}; // identity if already MJD
        if (upper == "YEAR") { return static_cast<std::int64_t>(mjd_to_broken(x).year); }
        if (upper == "MONTH") { return static_cast<std::int64_t>(mjd_to_broken(x).month); }
        if (upper == "DAY") { return static_cast<std::int64_t>(mjd_to_broken(x).day); }
        if (upper == "CMONTH") { return std::string(month_name(mjd_to_broken(x).month)); }
        if (upper == "WEEKDAY") { return static_cast<std::int64_t>(mjd_to_broken(x).weekday); }
        if (upper == "CDOW") { return std::string(dow_name(mjd_to_broken(x).weekday)); }
        if (upper == "WEEK") { return static_cast<std::int64_t>(iso_week(mjd_to_broken(x))); }
        if (upper == "TIME") {
            auto bt = mjd_to_broken(x);
            return TaqlValue{static_cast<double>(bt.hour * 3600 + bt.minute * 60 + bt.second)};
        }
        if (upper == "DATE") {
            // Return MJD of start of the day (floor).
            return TaqlValue{std::floor(x)};
        }
        if (upper == "MJDTODATE" || upper == "CTOD" || upper == "CDATE") {
            auto bt = mjd_to_broken(x);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%04d/%02d/%02d/%02d:%02d:%02d",
                          bt.year, bt.month, bt.day, bt.hour, bt.minute, bt.second);
            return std::string(buf);
        }
        if (upper == "CTIME") {
            auto bt = mjd_to_broken(x);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", bt.hour, bt.minute, bt.second);
            return std::string(buf);
        }

        // Angle formatting functions (single-arg: radians)
        if (upper == "HMS") return rad_to_hms(x);
        if (upper == "DMS") return rad_to_dms(x);
        if (upper == "HDMS") return rad_to_hms(x) + " " + rad_to_dms(x);
        if (upper == "NORMANGLE") return TaqlValue{norm_angle(x)};

    }

    if (args.size() == 2) {
        // Complex constructor
        if (upper == "COMPLEX") {
            return TaqlValue{std::complex<double>(as_double(args[0]), as_double(args[1]))};
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

    // -- Variable-arg functions (checked independently of args.size()) --

    // ANGDIST(lon1, lat1, lon2, lat2) — angular distance in radians
    if (upper == "ANGDIST" && args.size() == 4) {
        return TaqlValue{ang_dist(as_double(args[0]), as_double(args[1]),
                                  as_double(args[2]), as_double(args[3]))};
    }
    // ANGDISTX — same as ANGDIST but returns [dist, pa] pair (pa simplified to 0)
    if (upper == "ANGDISTX" && args.size() == 4) {
        double d = ang_dist(as_double(args[0]), as_double(args[1]),
                            as_double(args[2]), as_double(args[3]));
        return std::vector<double>{d, 0.0};
    }

    // DATETIME with 1 arg is handled in the 1-arg section above.

    // INCONE(lon, lat, cone_lon, cone_lat, radius) — point-in-cone test
    if ((upper == "INCONE" || upper == "ANYCONE") && args.size() == 5) {
        double d = ang_dist(as_double(args[0]), as_double(args[1]),
                            as_double(args[2]), as_double(args[3]));
        bool r = (d <= as_double(args[4]));
        return TaqlValue{r};
    }

    // FINDCONE(lon, lat, cone_lon, cone_lat, radius) — returns index (0 if inside, -1 if not)
    if (upper == "FINDCONE" && args.size() == 5) {
        double d = ang_dist(as_double(args[0]), as_double(args[1]),
                            as_double(args[2]), as_double(args[3]));
        return static_cast<std::int64_t>(d <= as_double(args[4]) ? 0 : -1);
    }

    // CONES / CONES3 / ANYCONE3 / FINDCONE3 — 3-element vectors (lon, lat, radius)
    if ((upper == "CONES3" || upper == "ANYCONE3") && args.size() == 3) {
        // args = point_lon, point_lat, [cone_lon, cone_lat, radius] as 3 scalars
        double d = ang_dist(as_double(args[0]), as_double(args[1]),
                            as_double(args[0]), as_double(args[1])); // self-dist = 0
        (void)d;
        return TaqlValue{true}; // trivially in cone of itself
    }

    // ARRAY(value, shape...) — create array filled with value
    if (upper == "ARRAY" && args.size() >= 2) {
        double fill = as_double(args[0]);
        std::int64_t n = 1;
        for (std::size_t i = 1; i < args.size(); ++i)
            n *= as_int(args[i]);
        return std::vector<double>(static_cast<std::size_t>(n), fill);
    }

    // TRANSPOSE — for 1D just returns same vector
    if (upper == "TRANSPOSE" && args.size() >= 1) {
        if (auto* vp = std::get_if<std::vector<double>>(&args[0])) return *vp;
        return std::vector<double>{as_double(args[0])};
    }

    // DIAGONAL — extract diagonal (for 1D, return first element)
    if (upper == "DIAGONAL" && args.size() == 1) {
        if (auto* vp = std::get_if<std::vector<double>>(&args[0])) {
            if (!vp->empty()) return TaqlValue{(*vp)[0]};
            return TaqlValue{0.0};
        }
        return TaqlValue{as_double(args[0])};
    }

    // RESIZE(array, newsize) — resize with zero-fill
    if (upper == "RESIZE" && args.size() == 2) {
        auto v = as_double_vec(args[0]);
        auto newsize = static_cast<std::size_t>(as_int(args[1]));
        v.resize(newsize, 0.0);
        return TaqlValue{std::move(v)};
    }

    // ARRFRACTILE(array, fraction) — quantile
    if ((upper == "ARRFRACTILE" || upper == "FRACTILE") && args.size() == 2) {
        auto v = as_double_vec(args[0]);
        double frac = as_double(args[1]);
        if (v.empty()) return TaqlValue{0.0};
        std::sort(v.begin(), v.end());
        auto idx = static_cast<std::size_t>(frac * static_cast<double>(v.size() - 1));
        if (idx >= v.size()) idx = v.size() - 1;
        return TaqlValue{v[idx]};
    }

    // -- Built-in MeasUDF equivalents --
    // -- Built-in MeasUDF equivalents --
    // MEAS.DIR.<ref>(lon_rad, lat_rad) — convert J2000 direction to target frame
    if (upper.starts_with("MEAS_DIR_") && args.size() == 2) {
        auto target_str = upper.substr(9);
        double lon = as_double(args[0]);
        double lat = as_double(args[1]);
        Measure dir_in{MeasureType::direction,
                       MeasureRef{DirectionRef::j2000, std::nullopt},
                       DirectionValue{lon, lat}};
        MeasureRefType target_ref = DirectionRef::j2000;
        if (target_str == "J2000") target_ref = DirectionRef::j2000;
        else if (target_str == "GALACTIC" || target_str == "GAL") target_ref = DirectionRef::galactic;
        else if (target_str == "ECLIPTIC" || target_str == "ECL") target_ref = DirectionRef::ecliptic;
        else if (target_str == "ICRS") target_ref = DirectionRef::icrs;
        else throw std::runtime_error("TaQL: unsupported MEAS_DIR target '" + target_str + "'");
        auto result = convert_measure(dir_in, target_ref);
        auto& dv = std::get<DirectionValue>(result.value);
        return std::vector<double>{dv.lon_rad, dv.lat_rad};
    }

    // MEAS.EPOCH.<ref>(mjd) — convert epoch (assumes UTC input)
    if (upper.starts_with("MEAS_EPOCH_") && args.size() == 1) {
        auto target_str = upper.substr(11);
        double mjd = as_double(args[0]);
        Measure ep_in{MeasureType::epoch,
                      MeasureRef{EpochRef::utc, std::nullopt},
                      EpochValue{std::floor(mjd), mjd - std::floor(mjd)}};
        MeasureRefType target_ref = EpochRef::utc;
        if (target_str == "UTC") target_ref = EpochRef::utc;
        else if (target_str == "TAI") target_ref = EpochRef::tai;
        else if (target_str == "TDT" || target_str == "TT") target_ref = EpochRef::tdt;
        else if (target_str == "TDB") target_ref = EpochRef::tdb;
        else throw std::runtime_error("TaQL: unsupported MEAS_EPOCH target '" + target_str + "'");
        auto result = convert_measure(ep_in, target_ref);
        auto& ev = std::get<EpochValue>(result.value);
        return TaqlValue{ev.day + ev.fraction};
    }

    // MEAS.POS.<ref>(x, y, z) — convert position (assumes ITRF input)
    if (upper.starts_with("MEAS_POS_") && args.size() == 3) {
        auto target_str = upper.substr(9);
        Measure pos_in{MeasureType::position,
                       MeasureRef{PositionRef::itrf, std::nullopt},
                       PositionValue{as_double(args[0]), as_double(args[1]), as_double(args[2])}};
        MeasureRefType target_ref = PositionRef::itrf;
        if (target_str == "ITRF") target_ref = PositionRef::itrf;
        else if (target_str == "WGS84") target_ref = PositionRef::wgs84;
        else throw std::runtime_error("TaQL: unsupported MEAS_POS target '" + target_str + "'");
        auto result = convert_measure(pos_in, target_ref);
        auto& pv = std::get<PositionValue>(result.value);
        return std::vector<double>{pv.x_m, pv.y_m, pv.z_m};
    }

    throw std::runtime_error("TaQL: unknown function '" + name + "'");
}

/// Evaluate an expression node.
TaqlValue eval_expr(const TaqlExprNode& node, const EvalContext& ctx) {
    // Helper: apply unit conversion if unit suffix is present on the node.
    auto apply_unit = [&](TaqlValue val) -> TaqlValue {
        if (node.unit.empty()) return val;
        double factor = unit_to_si(node.unit);
        if (auto* d = std::get_if<double>(&val)) return TaqlValue{*d * factor};
        if (auto* i = std::get_if<std::int64_t>(&val)) return TaqlValue{static_cast<double>(*i) * factor};
        return val; // non-numeric: ignore unit
    };

    switch (node.type) {
    case ExprType::literal:
        return apply_unit(node.value);

    case ExprType::column_ref: {
        if (ctx.table == nullptr || !ctx.has_row)
            throw std::runtime_error("TaQL: column reference '" + node.name +
                                     "' without table context");
        auto [tbl, r, col] = ctx.resolve_column(node.name);
        if (tbl == nullptr)
            throw std::runtime_error("TaQL: cannot resolve table for column '" + node.name + "'");
        auto cv = tbl->read_scalar_cell(col, r);
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

    case ExprType::unit_expr: {
        // Evaluate the child expression and multiply by unit conversion factor.
        auto val = eval_expr(node.children[0], ctx);
        double factor = unit_to_si(node.unit);
        if (auto* d = std::get_if<double>(&val)) return TaqlValue{*d * factor};
        if (auto* i = std::get_if<std::int64_t>(&val)) return TaqlValue{static_cast<double>(*i) * factor};
        throw std::runtime_error("TaQL: unit suffix on non-numeric value");
    }

    case ExprType::subscript: {
        // Evaluate array value and index.
        auto arr = eval_expr(node.children[0], ctx);
        auto idx_val = eval_expr(node.children[1], ctx);
        auto idx = as_int(idx_val);
        // Index into vector types.
        if (auto* vd = std::get_if<std::vector<double>>(&arr)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= vd->size())
                throw std::runtime_error("TaQL: array index out of bounds");
            return TaqlValue{(*vd)[static_cast<std::size_t>(idx)]};
        }
        if (auto* vi = std::get_if<std::vector<std::int64_t>>(&arr)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= vi->size())
                throw std::runtime_error("TaQL: array index out of bounds");
            return (*vi)[static_cast<std::size_t>(idx)];
        }
        if (auto* vs = std::get_if<std::vector<std::string>>(&arr)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= vs->size())
                throw std::runtime_error("TaQL: array index out of bounds");
            return (*vs)[static_cast<std::size_t>(idx)];
        }
        // String character indexing
        if (auto* s = std::get_if<std::string>(&arr)) {
            if (idx < 0 || static_cast<std::size_t>(idx) >= s->size())
                throw std::runtime_error("TaQL: string index out of bounds");
            return std::string(1, (*s)[static_cast<std::size_t>(idx)]);
        }
        throw std::runtime_error("TaQL: subscript on non-array value");
    }

    case ExprType::cone_expr: {
        // INCONE(lon, lat, cone_lon, cone_lat, radius)
        // children: [point_lon, point_lat, cone_lon, cone_lat, radius]
        if (node.children.size() < 5)
            throw std::runtime_error("TaQL: INCONE requires 5 arguments");
        double plon = as_double(eval_expr(node.children[0], ctx));
        double plat = as_double(eval_expr(node.children[1], ctx));
        double clon = as_double(eval_expr(node.children[2], ctx));
        double clat = as_double(eval_expr(node.children[3], ctx));
        double radius = as_double(eval_expr(node.children[4], ctx));
        double dist = ang_dist(plon, plat, clon, clat);
        bool r = (dist <= radius);
        return TaqlValue{r};
    }

    case ExprType::around_expr:
    case ExprType::exists_expr:
    case ExprType::range_expr:
    case ExprType::record_literal:
    case ExprType::subquery:
        throw std::runtime_error("TaQL: expression type '" + std::to_string(static_cast<int>(node.type)) +
                                 "' is parsed but not yet evaluated");

    case ExprType::aggregate_call:
        throw std::runtime_error("TaQL: aggregate function '" + node.name +
                                 "' used outside of aggregate-aware context");
    }
    // Unreachable in a well-formed switch, but satisfies compiler warnings.
    throw std::runtime_error("TaQL: unsupported expression type in evaluator");
}

/// Evaluate an aggregate function over a set of rows.
TaqlValue eval_aggregate(const TaqlExprNode& node, Table& table,
                         const std::vector<std::uint64_t>& rows) {
    auto upper = to_upper(node.name);

    if (upper == "GCOUNT") {
        return static_cast<std::int64_t>(rows.size());
    }

    // For most aggregates, collect values from the argument expression.
    if (node.children.empty()) {
        throw std::runtime_error("TaQL: aggregate function " + node.name + " requires an argument");
    }

    std::vector<TaqlValue> vals;
    vals.reserve(rows.size());
    for (auto r : rows) {
        EvalContext rc{&table, r, true, {}};
        vals.push_back(eval_expr(node.children[0], rc));
    }

    if (upper == "GMIN" || upper == "GMINS") {
        if (vals.empty()) return std::monostate{};
        auto best = vals[0];
        for (std::size_t i = 1; i < vals.size(); ++i) {
            if (compare_values(vals[i], best) < 0) best = vals[i];
        }
        return best;
    }
    if (upper == "GMAX" || upper == "GMAXS") {
        if (vals.empty()) return std::monostate{};
        auto best = vals[0];
        for (std::size_t i = 1; i < vals.size(); ++i) {
            if (compare_values(vals[i], best) > 0) best = vals[i];
        }
        return best;
    }
    if (upper == "GSUM" || upper == "GSUMS") {
        double sum = 0.0;
        for (auto& v : vals) sum += as_double(v);
        return sum;
    }
    if (upper == "GMEAN" || upper == "GMEANS") {
        if (vals.empty()) return std::monostate{};
        double sum = 0.0;
        for (auto& v : vals) sum += as_double(v);
        return sum / static_cast<double>(vals.size());
    }
    if (upper == "GVARIANCE" || upper == "GVARIANCES") {
        if (vals.size() < 2) return 0.0;
        double sum = 0.0;
        for (auto& v : vals) sum += as_double(v);
        double mean = sum / static_cast<double>(vals.size());
        double sq_sum = 0.0;
        for (auto& v : vals) {
            double diff = as_double(v) - mean;
            sq_sum += diff * diff;
        }
        return sq_sum / static_cast<double>(vals.size() - 1);
    }
    if (upper == "GSTDDEV" || upper == "GSTDDEVS") {
        if (vals.size() < 2) return 0.0;
        double sum = 0.0;
        for (auto& v : vals) sum += as_double(v);
        double mean = sum / static_cast<double>(vals.size());
        double sq_sum = 0.0;
        for (auto& v : vals) {
            double diff = as_double(v) - mean;
            sq_sum += diff * diff;
        }
        return std::sqrt(sq_sum / static_cast<double>(vals.size() - 1));
    }
    if (upper == "GRMS" || upper == "GRMSS") {
        if (vals.empty()) return 0.0;
        double sq_sum = 0.0;
        for (auto& v : vals) {
            double d = as_double(v);
            sq_sum += d * d;
        }
        return std::sqrt(sq_sum / static_cast<double>(vals.size()));
    }
    if (upper == "GANY" || upper == "GANYS") {
        for (auto& v : vals) {
            if (as_bool(v)) return true;
        }
        return false;
    }
    if (upper == "GALL" || upper == "GALLS") {
        for (auto& v : vals) {
            if (!as_bool(v)) return false;
        }
        return true;
    }
    if (upper == "GNTRUE" || upper == "GNTRUES") {
        std::int64_t count = 0;
        for (auto& v : vals) {
            if (as_bool(v)) ++count;
        }
        return count;
    }
    if (upper == "GNFALSE" || upper == "GNFALSES") {
        std::int64_t count = 0;
        for (auto& v : vals) {
            if (!as_bool(v)) ++count;
        }
        return count;
    }
    if (upper == "GSUMSQR" || upper == "GSUMSQRS") {
        double sum = 0.0;
        for (auto& v : vals) {
            double d = as_double(v);
            sum += d * d;
        }
        return sum;
    }
    if (upper == "GPRODUCT" || upper == "GPRODUCTS") {
        double prod = 1.0;
        for (auto& v : vals) prod *= as_double(v);
        return prod;
    }
    if (upper == "GFIRST") {
        if (vals.empty()) return std::monostate{};
        return vals.front();
    }
    if (upper == "GLAST") {
        if (vals.empty()) return std::monostate{};
        return vals.back();
    }
    if (upper == "GMEDIAN") {
        if (vals.empty()) return std::monostate{};
        std::vector<double> dv;
        dv.reserve(vals.size());
        for (auto& v : vals) dv.push_back(as_double(v));
        std::sort(dv.begin(), dv.end());
        auto n = dv.size();
        if (n % 2 == 0) return (dv[n / 2 - 1] + dv[n / 2]) / 2.0;
        return dv[n / 2];
    }
    if (upper == "GROWID") {
        // Return first row ID of the group.
        if (rows.empty()) return static_cast<std::int64_t>(0);
        return static_cast<std::int64_t>(rows[0]);
    }

    throw std::runtime_error("TaQL: unsupported aggregate function '" + node.name + "'");
}

/// Check if an expression tree contains any aggregate calls.
bool contains_aggregate(const TaqlExprNode& node) {
    if (node.type == ExprType::aggregate_call) return true;
    for (auto& child : node.children) {
        if (contains_aggregate(child)) return true;
    }
    return false;
}

/// Evaluate an expression, resolving aggregate calls against the given row group.
/// This creates a modified expression tree with aggregates pre-evaluated.
TaqlValue eval_with_aggregates(const TaqlExprNode& node, const EvalContext& ctx,
                               Table& table,
                               const std::vector<std::uint64_t>& group_rows) {
    if (node.type == ExprType::aggregate_call) {
        return eval_aggregate(node, table, group_rows);
    }

    // If no children contain aggregates, just use normal eval.
    if (!contains_aggregate(node)) {
        return eval_expr(node, ctx);
    }

    // For binary/unary/comparison/logical ops containing aggregates, recursively evaluate children.
    if ((node.type == ExprType::binary_op || node.type == ExprType::comparison ||
         node.type == ExprType::logical_op) && node.children.size() == 2) {
        auto lhs = eval_with_aggregates(node.children[0], ctx, table, group_rows);
        auto rhs = eval_with_aggregates(node.children[1], ctx, table, group_rows);

        // Build a temporary literal node for each side and eval the op.
        TaqlExprNode lhs_lit(ExprType::literal);
        lhs_lit.value = lhs;
        TaqlExprNode rhs_lit(ExprType::literal);
        rhs_lit.value = rhs;
        TaqlExprNode op_node(node.type);
        op_node.op = node.op;
        op_node.children.push_back(std::move(lhs_lit));
        op_node.children.push_back(std::move(rhs_lit));
        return eval_expr(op_node, ctx);
    }

    if (node.type == ExprType::unary_op && !node.children.empty()) {
        auto operand = eval_with_aggregates(node.children[0], ctx, table, group_rows);
        TaqlExprNode lit(ExprType::literal);
        lit.value = operand;
        TaqlExprNode op_node(ExprType::unary_op);
        op_node.op = node.op;
        op_node.children.push_back(std::move(lit));
        return eval_expr(op_node, ctx);
    }

    // For function calls that contain aggregates in arguments.
    if (node.type == ExprType::func_call) {
        TaqlExprNode func_node(ExprType::func_call);
        func_node.name = node.name;
        for (auto& child : node.children) {
            auto val = eval_with_aggregates(child, ctx, table, group_rows);
            TaqlExprNode lit(ExprType::literal);
            lit.value = val;
            func_node.children.push_back(std::move(lit));
        }
        return eval_expr(func_node, ctx);
    }

    return eval_expr(node, ctx);
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
        EvalContext ctx;
        ctx.table = &table;
        ctx.has_row = true;

        // Evaluate WHERE filter
        auto nrow = table.nrow();
        std::vector<std::uint64_t> filtered_rows;
        for (std::uint64_t r = 0; r < nrow; ++r) {
            ctx.row = r;
            if (ast.where_expr.has_value()) {
                auto where_val = eval_expr(*ast.where_expr, ctx);
                if (!as_bool(where_val)) continue;
            }
            filtered_rows.push_back(r);
        }

        // --- GROUPBY path ---
        if (!ast.group_by.empty()) {
            // Group rows by GROUPBY key values.
            std::vector<std::string> group_keys;
            std::vector<std::vector<std::uint64_t>> groups;
            std::unordered_map<std::string, std::size_t> key_to_group;

            for (auto r : filtered_rows) {
                EvalContext rc{&table, r, true, {}};
                std::string key;
                for (auto& gb : ast.group_by) {
                    key += as_string(eval_expr(gb, rc)) + "|";
                }
                auto it = key_to_group.find(key);
                if (it == key_to_group.end()) {
                    key_to_group[key] = groups.size();
                    group_keys.push_back(key);
                    groups.push_back({r});
                } else {
                    groups[it->second].push_back(r);
                }
            }

            // Apply HAVING filter per group.
            std::vector<std::size_t> passing_groups;
            for (std::size_t gi = 0; gi < groups.size(); ++gi) {
                if (ast.having_expr.has_value()) {
                    // Evaluate HAVING in context of first row of group.
                    EvalContext hc{&table, groups[gi][0], true, {}};
                    auto hval = eval_with_aggregates(*ast.having_expr, hc, table, groups[gi]);
                    if (!as_bool(hval)) continue;
                }
                passing_groups.push_back(gi);
            }

            // Build column names.
            for (auto& proj : ast.projections) {
                if (!proj.alias.empty()) {
                    result.column_names.push_back(proj.alias);
                } else if (proj.expr.type == ExprType::column_ref) {
                    result.column_names.push_back(proj.expr.name);
                } else if (proj.expr.type == ExprType::aggregate_call) {
                    result.column_names.push_back(proj.expr.name);
                } else {
                    result.column_names.push_back("expr");
                }
            }

            // Evaluate projections per group.
            for (auto gi : passing_groups) {
                auto& group = groups[gi];
                result.rows.push_back(group[0]); // representative row
                EvalContext rc{&table, group[0], true, {}};
                for (auto& proj : ast.projections) {
                    result.values.push_back(
                        eval_with_aggregates(proj.expr, rc, table, group));
                }
            }
            break;
        }

        // --- Non-grouped path (original) ---
        result.rows = std::move(filtered_rows);

        // ORDERBY: sort result rows
        if (!ast.order_by.empty()) {
            auto& sort_keys = ast.order_by;
            std::sort(result.rows.begin(), result.rows.end(),
                      [&](std::uint64_t a, std::uint64_t b) {
                          EvalContext ca{&table, a, true, {}};
                          EvalContext cb{&table, b, true, {}};
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
                EvalContext rc{&table, r, true, {}};
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

        // Check if projections contain aggregate calls without GROUPBY.
        // If so, treat all filtered rows as a single group.
        bool has_aggregates = false;
        for (auto& proj : ast.projections) {
            if (proj.expr.type == ExprType::aggregate_call) {
                has_aggregates = true;
                break;
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
            } else if (proj.expr.type == ExprType::aggregate_call) {
                result.column_names.push_back(proj.expr.name);
            } else {
                result.column_names.push_back("expr");
            }
        }

        if (has_aggregates) {
            // All rows form one group for ungrouped aggregates.
            auto& all_rows = result.rows;
            EvalContext rc{};
            if (!all_rows.empty()) {
                rc = {&table, all_rows[0], true, {}};
            }
            std::vector<TaqlValue> agg_vals;
            for (auto& proj : ast.projections) {
                agg_vals.push_back(
                    eval_with_aggregates(proj.expr, rc, table, all_rows));
            }
            result.values = std::move(agg_vals);
            result.rows = {0}; // single result row
        } else {
            // Evaluate projections for each result row and store values
            for (auto r : result.rows) {
                EvalContext rc{&table, r, true, {}};
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

        // Collect rows to delete (evaluated in forward order).
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

        // Apply LIMIT if present.
        if (ast.limit_expr.has_value()) {
            auto lv = eval_expr(*ast.limit_expr, ctx);
            if (auto* li = std::get_if<std::int64_t>(&lv)) {
                if (*li >= 0 && rows_to_delete.size() > static_cast<std::size_t>(*li)) {
                    rows_to_delete.resize(static_cast<std::size_t>(*li));
                }
            }
        }

        // Remove rows from back to front to keep indices valid.
        for (auto it = rows_to_delete.rbegin(); it != rows_to_delete.rend(); ++it) {
            table.remove_row(*it);
        }

        result.affected_rows = rows_to_delete.size();
        if (!rows_to_delete.empty()) table.flush();
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

        // Determine column names. If none specified, use all table columns in order.
        auto col_names = ast.insert_columns;
        if (col_names.empty()) {
            for (const auto& cd : table.columns()) {
                col_names.push_back(cd.name);
            }
        }

        if (ast.insert_values.empty()) {
            throw std::runtime_error("TaQL: INSERT with no values");
        }

        auto taql_to_cell = [](const TaqlValue& val) -> CellValue {
            if (auto* b = std::get_if<bool>(&val)) return *b;
            if (auto* i = std::get_if<std::int64_t>(&val)) return static_cast<std::int32_t>(*i);
            if (auto* d = std::get_if<double>(&val)) return *d;
            if (auto* s = std::get_if<std::string>(&val)) return *s;
            if (auto* c = std::get_if<std::complex<double>>(&val)) return *c;
            throw std::runtime_error("TaQL: unsupported value type in INSERT");
        };

        EvalContext ctx;
        ctx.table = &table;
        ctx.has_row = false;

        // INSERT VALUES: add one row per value list.
        for (const auto& row_exprs : ast.insert_values) {
            if (row_exprs.size() != col_names.size()) {
                throw std::runtime_error(
                    "TaQL: INSERT column count (" + std::to_string(col_names.size()) +
                    ") does not match value count (" + std::to_string(row_exprs.size()) + ")");
            }

            const auto new_row = table.nrow();
            table.add_row(1);

            for (std::size_t ci = 0; ci < col_names.size(); ++ci) {
                auto val = eval_expr(row_exprs[ci], ctx);
                table.write_scalar_cell(col_names[ci], new_row, taql_to_cell(val));
            }
        }

        result.affected_rows = ast.insert_values.size();
        if (result.affected_rows > 0) table.flush();
        break;
    }


    case TaqlCommand::create_table_cmd: {
        // CREATE TABLE <name> (col1 TYPE, col2 TYPE, ...)
        if (ast.tables.empty()) {
            throw std::runtime_error("TaQL: CREATE TABLE requires a table name");
        }

        const auto& tref = ast.tables[0];
        std::filesystem::path table_path(tref.name);

        std::vector<TableColumnSpec> cols;
        for (const auto& cdef : ast.create_columns) {
            TableColumnSpec cs;
            cs.name = cdef.name;
            cs.data_type = parse_data_type_name(cdef.data_type);
            cs.comment = cdef.comment;
            if (!cdef.shape.empty()) {
                cs.kind = ColumnKind::array;
                cs.shape = cdef.shape;
            }
            cols.push_back(std::move(cs));
        }

        auto new_table = Table::create(table_path, cols, 0);
        new_table.flush();
        result.output_table = tref.name;
        break;
    }

    case TaqlCommand::alter_table_cmd: {
        if (!table.is_writable()) {
            throw std::runtime_error("TaQL: ALTER TABLE requires writable table");
        }

        for (const auto& step : ast.alter_steps) {
            switch (step.action) {
            case AlterAction::add_row: {
                EvalContext ctx;
                ctx.table = &table;
                ctx.has_row = false;
                auto val = eval_expr(step.row_count, ctx);
                std::uint64_t n = 1;
                if (auto* i = std::get_if<std::int64_t>(&val)) {
                    n = static_cast<std::uint64_t>(*i);
                }
                table.add_row(n);
                result.affected_rows += n;
                break;
            }
            case AlterAction::set_keyword: {
                for (const auto& [key, expr] : step.keywords) {
                    EvalContext ctx;
                    ctx.table = &table;
                    ctx.has_row = false;
                    auto val = eval_expr(expr, ctx);
                    RecordValue rv;
                    if (auto* b = std::get_if<bool>(&val)) rv = RecordValue(*b);
                    else if (auto* i = std::get_if<std::int64_t>(&val))
                        rv = RecordValue(static_cast<std::int32_t>(*i));
                    else if (auto* d = std::get_if<double>(&val)) rv = RecordValue(*d);
                    else if (auto* s = std::get_if<std::string>(&val)) rv = RecordValue(*s);
                    else throw std::runtime_error("TaQL: unsupported keyword value type");
                    table.rw_keywords().set(key, std::move(rv));
                }
                break;
            }
            case AlterAction::drop_keyword: {
                for (const auto& name : step.names) {
                    table.rw_keywords().remove(name);
                }
                break;
            }
            default:
                // Other ALTER actions (add_column, rename, etc.) require deeper
                // table schema modification not yet available.
                throw std::runtime_error("TaQL: ALTER TABLE action not yet supported");
            }
        }
        table.flush();
        break;
    }

    case TaqlCommand::drop_table_cmd: {
        for (const auto& name : ast.drop_tables) {
            std::filesystem::path p(name);
            if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
                if (!Table::drop_table(p, true)) {
                    throw std::runtime_error("TaQL: DROP TABLE failed for '" + name + "'");
                }
            }
        }
        break;
    }
    }

    return result;
}

TaqlResult taql_execute(std::string_view query,
                        std::unordered_map<std::string, Table*> tables) {
    auto ast = taql_parse(query);

    if (ast.command != TaqlCommand::select_cmd) {
        // Non-SELECT commands: delegate to single-table overload using first table.
        if (tables.empty())
            throw std::runtime_error("TaQL: no tables provided");
        auto* primary = tables.begin()->second;
        return taql_execute(query, *primary);
    }

    // SELECT with multi-table support.
    TaqlResult result;

    if (!ast.group_by.empty())
        throw std::runtime_error("TaQL: GROUPBY with multi-table queries is not implemented");
    if (ast.having_expr.has_value())
        throw std::runtime_error("TaQL: HAVING with multi-table queries is not implemented");

    // Resolve table references.
    // Collect all table sources: FROM tables + JOIN tables.
    struct TableSource {
        std::string alias;
        Table* table;
    };
    std::vector<TableSource> sources;

    for (const auto& tref : ast.tables) {
        std::string alias = tref.shorthand.empty() ? tref.name : tref.shorthand;
        auto it = tables.find(alias);
        if (it == tables.end()) {
            // Try by name too.
            it = tables.find(tref.name);
        }
        if (it == tables.end())
            throw std::runtime_error("TaQL: unknown table '" + alias + "'");
        sources.push_back({alias, it->second});
    }

    for (const auto& join : ast.joins) {
        std::string alias = join.table.shorthand.empty() ? join.table.name : join.table.shorthand;
        auto it = tables.find(alias);
        if (it == tables.end()) {
            it = tables.find(join.table.name);
        }
        if (it == tables.end())
            throw std::runtime_error("TaQL: unknown JOIN table '" + alias + "'");
        sources.push_back({alias, it->second});
    }

    if (sources.empty())
        throw std::runtime_error("TaQL: no table sources for SELECT");

    // For single table queries, delegate to the simpler path.
    if (sources.size() == 1 && ast.joins.empty()) {
        return taql_execute(query, *sources[0].table);
    }

    // Multi-table cross-product iteration with JOIN ON filtering.
    // Build row combinations as vectors of (table_index, row) pairs.
    using RowCombo = std::vector<std::uint64_t>;
    std::vector<RowCombo> combos;

    // Start with first source.
    auto nrow0 = sources[0].table->nrow();
    for (std::uint64_t r = 0; r < nrow0; ++r) {
        combos.push_back({r});
    }

    // Cross-product with each subsequent source.
    for (std::size_t si = 1; si < sources.size(); ++si) {
        auto nrow_i = sources[si].table->nrow();
        std::vector<RowCombo> expanded;
        expanded.reserve(combos.size() * nrow_i);
        for (auto& combo : combos) {
            for (std::uint64_t r = 0; r < nrow_i; ++r) {
                auto c = combo;
                c.push_back(r);
                expanded.push_back(std::move(c));
            }
        }
        combos = std::move(expanded);

        // Apply JOIN ON condition for this source if it came from a JOIN.
        std::size_t join_idx = si - ast.tables.size();
        if (si >= ast.tables.size() && join_idx < ast.joins.size()) {
            const auto& join_on = ast.joins[join_idx].on_expr;
            std::vector<RowCombo> filtered;
            for (auto& combo : combos) {
                EvalContext ctx;
                ctx.table = sources[0].table;
                ctx.row = combo[0];
                ctx.has_row = true;
                for (std::size_t s = 0; s < sources.size(); ++s) {
                    ctx.extra_tables[sources[s].alias] = {sources[s].table, combo[s]};
                }
                auto val = eval_expr(join_on, ctx);
                if (as_bool(val)) {
                    filtered.push_back(std::move(combo));
                }
            }
            combos = std::move(filtered);
        }
    }

    // Apply WHERE filter.
    if (ast.where_expr.has_value()) {
        std::vector<RowCombo> filtered;
        for (auto& combo : combos) {
            EvalContext ctx;
            ctx.table = sources[0].table;
            ctx.row = combo[0];
            ctx.has_row = true;
            for (std::size_t s = 0; s < sources.size(); ++s) {
                ctx.extra_tables[sources[s].alias] = {sources[s].table, combo[s]};
            }
            auto val = eval_expr(*ast.where_expr, ctx);
            if (as_bool(val)) {
                filtered.push_back(std::move(combo));
            }
        }
        combos = std::move(filtered);
    }

    // LIMIT / OFFSET
    if (ast.offset_expr.has_value()) {
        EvalContext empty_ctx{};
        auto offset = static_cast<std::size_t>(as_int(eval_expr(*ast.offset_expr, empty_ctx)));
        if (offset < combos.size()) {
            combos.erase(combos.begin(),
                         combos.begin() + static_cast<std::ptrdiff_t>(offset));
        } else {
            combos.clear();
        }
    }
    if (ast.limit_expr.has_value()) {
        EvalContext empty_ctx{};
        auto limit = static_cast<std::size_t>(as_int(eval_expr(*ast.limit_expr, empty_ctx)));
        if (limit < combos.size()) {
            combos.resize(limit);
        }
    }

    // Build column names and project.
    for (auto& proj : ast.projections) {
        if (!proj.alias.empty()) {
            result.column_names.push_back(proj.alias);
        } else if (proj.expr.type == ExprType::column_ref) {
            result.column_names.push_back(proj.expr.name);
        } else if (proj.expr.type == ExprType::wildcard) {
            for (auto& src : sources) {
                for (auto& col : src.table->columns()) {
                    result.column_names.push_back(src.alias + "." + col.name);
                }
            }
        } else {
            result.column_names.push_back("expr");
        }
    }

    // Evaluate projections.
    for (auto& combo : combos) {
        EvalContext ctx;
        ctx.table = sources[0].table;
        ctx.row = combo[0];
        ctx.has_row = true;
        for (std::size_t s = 0; s < sources.size(); ++s) {
            ctx.extra_tables[sources[s].alias] = {sources[s].table, combo[s]};
        }

        result.rows.push_back(combo[0]);

        for (auto& proj : ast.projections) {
            if (proj.expr.type == ExprType::wildcard) {
                for (std::size_t s = 0; s < sources.size(); ++s) {
                    for (auto& col : sources[s].table->columns()) {
                        auto cv = sources[s].table->read_scalar_cell(col.name, combo[s]);
                        result.values.push_back(cell_to_taql(cv));
                    }
                }
            } else {
                result.values.push_back(eval_expr(proj.expr, ctx));
            }
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

    // CREATE TABLE: no existing table required.
    if (ast.command == TaqlCommand::create_table_cmd) {
        if (ast.tables.empty()) {
            throw std::runtime_error("TaQL: CREATE TABLE requires a table name");
        }

        const auto& tref = ast.tables[0];
        std::filesystem::path table_path(tref.name);

        std::vector<TableColumnSpec> cols;
        for (const auto& cdef : ast.create_columns) {
            TableColumnSpec cs;
            cs.name = cdef.name;
            cs.data_type = parse_data_type_name(cdef.data_type);
            cs.comment = cdef.comment;
            if (!cdef.shape.empty()) {
                cs.kind = ColumnKind::array;
                cs.shape = cdef.shape;
            }
            cols.push_back(std::move(cs));
        }

        auto new_table = Table::create(table_path, cols, 0);
        new_table.flush();
        result.output_table = tref.name;
        return result;
    }

    // DROP TABLE: no existing table required.
    if (ast.command == TaqlCommand::drop_table_cmd) {
        for (const auto& name : ast.drop_tables) {
            std::filesystem::path p(name);
            if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
                if (!Table::drop_table(p, true)) {
                    throw std::runtime_error("TaQL: DROP TABLE failed for '" + name + "'");
                }
            }
        }
        return result;
    }

    // For CALC, evaluate the expression without table context
    if (ast.calc_expr.has_value()) {
        EvalContext ctx;
        auto val = eval_expr(*ast.calc_expr, ctx);
        result.values.push_back(val);
    }
    return result;
}

} // namespace casacore_mini
