#include "casacore_mini/taql.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
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
        if (std::isdigit(c) || (c == '.' && pos_ + 1 < source_.size() &&
                                 std::isdigit(source_[pos_ + 1]))) {
            return lex_number();
        }

        // Identifiers and keywords
        if (std::isalpha(c) || c == '_' || c == '$') {
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
        while (pos_ < source_.size() && std::isalpha(source_[pos_])) {
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
        while (pos_ < source_.size() && std::isdigit(source_[pos_])) {
            advance(1);
        }

        // Decimal point
        if (pos_ < source_.size() && source_[pos_] == '.' &&
            (pos_ + 1 >= source_.size() || source_[pos_ + 1] != '.')) {
            is_float = true;
            advance(1);
            while (pos_ < source_.size() && std::isdigit(source_[pos_])) {
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
            while (pos_ < source_.size() && std::isdigit(source_[pos_])) {
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
        while (pos_ < source_.size() && std::isxdigit(source_[pos_])) {
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
               (std::isalnum(source_[pos_]) || source_[pos_] == '_' || source_[pos_] == '$')) {
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
// Evaluator stubs (populated in W4)
// ===========================================================================

TaqlResult taql_execute(std::string_view query, Table& /*table*/) {
    auto ast = taql_parse(query);
    TaqlResult result;
    if (ast.command == TaqlCommand::show_cmd) {
        result.show_text = taql_show(ast.show_topic);
    }
    // Full evaluation implemented in W4
    return result;
}

TaqlResult taql_calc(std::string_view query) {
    auto ast = taql_parse(query);
    TaqlResult result;
    // Full CALC evaluation implemented in W4
    return result;
}

} // namespace casacore_mini
