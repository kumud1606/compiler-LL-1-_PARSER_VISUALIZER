#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

static const string EPS = "eps";
static const string END_MARK = "$";

struct Grammar {
    map<string, vector<vector<string>>> productions;
    vector<string> nonTerminals;
    set<string> terminals;
    string start;
};

struct LexToken {
    string kind;
    string lexeme;
};

struct TraceRow {
    string stack;
    string input;
    string action;
};

struct Result {
    map<string, set<string>> first;
    map<string, set<string>> follow;
    map<string, map<string, vector<string>>> table;
    vector<string> grammarUsed;
    vector<TraceRow> trace;
    bool accepted = false;
    string message;
    bool ll1_ok = true;
    vector<string> ll1_conflicts;
    vector<LexToken> tokens;
};

static bool isNonTerminal(const Grammar& g, const string& sym) {
    return g.productions.find(sym) != g.productions.end();
}

static string trim(const string& s) {
    size_t i = 0;
    while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) i++;
    size_t j = s.size();
    while (j > i && isspace(static_cast<unsigned char>(s[j - 1]))) j--;
    return s.substr(i, j - i);
}

static string join(const vector<string>& parts, const string& sep = " ") {
    ostringstream out;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) out << sep;
        out << parts[i];
    }
    return out.str();
}

static string lexDisplay(const LexToken& t) {
    if (t.kind == "id") return string("id(") + t.lexeme + ")";
    if (t.kind == "num") return string("num(") + t.lexeme + ")";
    if (t.kind == "header") return string("header(") + t.lexeme + ")";
    return t.kind;
}

static string formatProductionRhs(
    const vector<string>& prod,
    const vector<LexToken>& input,
    size_t ip
) {
    vector<string> idMarks;
    vector<string> numMarks;
    vector<string> headerMarks;
    for (size_t i = ip; i < input.size(); i++) {
        if (input[i].kind == "id") idMarks.push_back(lexDisplay(input[i]));
        else if (input[i].kind == "num") numMarks.push_back(lexDisplay(input[i]));
        else if (input[i].kind == "header") headerMarks.push_back(lexDisplay(input[i]));
    }
    size_t ii = 0;
    size_t ni = 0;
    size_t hi = 0;
    vector<string> parts;
    for (const string& sym : prod) {
        if (sym == EPS) {
            parts.push_back(sym);
            continue;
        }
        if (sym == "id") {
            parts.push_back(ii < idMarks.size() ? idMarks[ii++] : "id");
            continue;
        }
        if (sym == "num") {
            parts.push_back(ni < numMarks.size() ? numMarks[ni++] : "num");
            continue;
        }
        if (sym == "header") {
            parts.push_back(hi < headerMarks.size() ? headerMarks[hi++] : "header");
            continue;
        }
        parts.push_back(sym);
    }
    return join(parts);
}

static string toJsonString(const string& s) {
    string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out.push_back(c);
    }
    return out;
}

static Grammar buildGrammar() {
    Grammar g;
    g.start = "Program";
    g.productions = {
        {"Program", {{"IncludeList", "ProgramCore"}}},
        {"IncludeList", {{"IncludeDirective", "IncludeList"}, {EPS}}},
        {"IncludeDirective", {{"#", "include", "header"}}},
        {"ProgramCore", {{"Type", "ProgramAfterType"}, {"NonTypeStmt", "StmtList"}, {EPS}}},
        {"ProgramAfterType", {{"main", "(", ")", "Block"}, {"id", "DeclInit", ";", "StmtList"}}},
        {"StmtList", {{"Stmt", "StmtList"}, {EPS}}},
        {"Stmt", {{"Decl", ";"}, {"Assign", ";"}, {"IfStmt"}, {"WhileStmt"}, {"ForStmt"}, {"Block"}}},
        {"NonTypeStmt", {{"Assign", ";"}, {"IfStmt"}, {"WhileStmt"}, {"ForStmt"}, {"Block"}}},
        {"Block", {{"{", "StmtList", "}"}}},
        {"Decl", {{"Type", "id", "DeclInit"}}},
        {"Type", {{"int"}, {"float"}, {"char"}}},
        {"DeclInit", {{"=", "Expr"}, {EPS}}},
        {"Assign", {{"id", "=", "Expr"}}},
        {"IfStmt", {{"if", "(", "BoolExpr", ")", "Block", "ElsePart"}}},
        {"WhileStmt", {{"while", "(", "BoolExpr", ")", "Block"}}},
        {"ForStmt", {{"for", "(", "ForInit", ";", "ForCond", ";", "ForUpdate", ")", "Block"}}},
        {"ForInit", {{"Decl"}, {"Assign"}, {EPS}}},
        {"ForCond", {{"BoolExpr"}, {EPS}}},
        {"ForUpdate", {{"Assign"}, {EPS}}},
        {"ElsePart", {{"else", "Block"}, {EPS}}},
        {"BoolExpr", {{"Expr", "BoolExprTail"}}},
        {"BoolExprTail", {{"RelOp", "Expr"}, {EPS}}},
        {"RelOp", {{"<"}, {">"}, {"<="}, {">="}, {"=="}, {"!="}}},
        {"Expr", {{"Term", "ExprTail"}}},
        {"ExprTail", {{"+", "Term", "ExprTail"}, {"-", "Term", "ExprTail"}, {EPS}}},
        {"Term", {{"Factor", "TermTail"}}},
        {"TermTail", {{"*", "Factor", "TermTail"}, {"/", "Factor", "TermTail"}, {EPS}}},
        {"Factor", {{"id"}, {"num"}, {"(", "Expr", ")"}}}
    };

    for (const auto& p : g.productions) g.nonTerminals.push_back(p.first);

    for (const auto& p : g.productions) {
        for (const auto& rhs : p.second) {
            for (const auto& sym : rhs) {
                if (sym != EPS && !isNonTerminal(g, sym)) g.terminals.insert(sym);
            }
        }
    }
    g.terminals.insert(END_MARK);
    return g;
}

static set<string> firstOfSequence(
    const Grammar& g,
    const vector<string>& sequence,
    const map<string, set<string>>& first
) {
    set<string> out;
    bool nullable = true;
    if (sequence.empty()) {
        out.insert(EPS);
        return out;
    }

    for (const string& sym : sequence) {
        if (sym == EPS) {
            out.insert(EPS);
            continue;
        }
        if (!isNonTerminal(g, sym)) {
            out.insert(sym);
            nullable = false;
            break;
        }

        const auto it = first.find(sym);
        if (it == first.end()) {
            nullable = false;
            break;
        }

        for (const auto& token : it->second) {
            if (token != EPS) out.insert(token);
        }
        if (it->second.find(EPS) == it->second.end()) {
            nullable = false;
            break;
        }
    }

    if (nullable) out.insert(EPS);
    return out;
}

static map<string, set<string>> computeFirst(const Grammar& g) {
    map<string, set<string>> first;
    for (const auto& nt : g.nonTerminals) first[nt] = {};

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& nt : g.nonTerminals) {
            for (const auto& rhs : g.productions.at(nt)) {
                const auto f = firstOfSequence(g, rhs, first);
                const size_t before = first[nt].size();
                first[nt].insert(f.begin(), f.end());
                if (first[nt].size() != before) changed = true;
            }
        }
    }
    return first;
}

static map<string, set<string>> computeFollow(const Grammar& g, const map<string, set<string>>& first) {
    map<string, set<string>> follow;
    for (const auto& nt : g.nonTerminals) follow[nt] = {};
    follow[g.start].insert(END_MARK);

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& lhs : g.nonTerminals) {
            for (const auto& rhs : g.productions.at(lhs)) {
                for (size_t i = 0; i < rhs.size(); i++) {
                    const string& B = rhs[i];
                    if (!isNonTerminal(g, B)) continue;

                    vector<string> beta;
                    for (size_t j = i + 1; j < rhs.size(); j++) beta.push_back(rhs[j]);

                    const auto firstBeta = firstOfSequence(g, beta, first);
                    const size_t before = follow[B].size();
                    for (const auto& t : firstBeta) {
                        if (t != EPS) follow[B].insert(t);
                    }
                    if (beta.empty() || firstBeta.find(EPS) != firstBeta.end()) {
                        follow[B].insert(follow[lhs].begin(), follow[lhs].end());
                    }
                    if (follow[B].size() != before) changed = true;
                }
            }
        }
    }
    return follow;
}

static bool sameProduction(const vector<string>& a, const vector<string>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static map<string, map<string, vector<string>>> buildTable(
    const Grammar& g,
    const map<string, set<string>>& first,
    const map<string, set<string>>& follow,
    vector<string>& conflicts
) {
    map<string, map<string, vector<string>>> table;

    auto place = [&](const string& nt, const string& term, const vector<string>& alpha) {
        auto& cell = table[nt][term];
        const bool occupied = !cell.empty();
        if (occupied && !sameProduction(cell, alpha)) {
            conflicts.push_back(
                "M[" + nt + ", " + term + "] has both \"" + join(cell) + "\" and \"" + join(alpha) + "\""
            );
            return;
        }
        if (!occupied) cell = alpha;
    };

    for (const auto& A : g.nonTerminals) {
        for (const auto& alpha : g.productions.at(A)) {
            const auto firstAlpha = firstOfSequence(g, alpha, first);
            for (const auto& t : firstAlpha) {
                if (t != EPS) place(A, t, alpha);
            }
            if (firstAlpha.find(EPS) != firstAlpha.end()) {
                for (const auto& b : follow.at(A)) place(A, b, alpha);
            }
        }
    }
    return table;
}

static vector<LexToken> tokenize(const string& code, string& error) {
    vector<LexToken> tokens;
    size_t i = 0;
    auto isIdStart = [](char c) { return isalpha(static_cast<unsigned char>(c)) || c == '_'; };
    auto isIdBody = [](char c) { return isalnum(static_cast<unsigned char>(c)) || c == '_'; };

    while (i < code.size()) {
        const char c = code[i];
        if (isspace(static_cast<unsigned char>(c))) {
            i++;
            continue;
        }

        if (c == '#') {
            tokens.push_back({"#", "#"});
            i++;
            continue;
        }

        if (!tokens.empty() && tokens.back().kind == "include" && (c == '<' || c == '"')) {
            const char closing = (c == '<') ? '>' : '"';
            size_t j = i + 1;
            while (j < code.size() && code[j] != closing) j++;
            if (j >= code.size()) {
                error = "Unterminated header after include.";
                return {};
            }
            tokens.push_back({"header", code.substr(i, j - i + 1)});
            i = j + 1;
            continue;
        }

        if (isIdStart(c)) {
            size_t j = i + 1;
            while (j < code.size() && isIdBody(code[j])) j++;
            const string word = code.substr(i, j - i);
            if (
                word == "if" || word == "else" || word == "int" || word == "float" || word == "char" ||
                word == "while" || word == "for" || word == "include" || word == "main"
            ) {
                tokens.push_back({word, word});
            } else {
                tokens.push_back({"id", word});
            }
            i = j;
            continue;
        }

        if (isdigit(static_cast<unsigned char>(c))) {
            size_t j = i + 1;
            while (j < code.size() && (isdigit(static_cast<unsigned char>(code[j])) || code[j] == '.')) j++;
            tokens.push_back({"num", code.substr(i, j - i)});
            i = j;
            continue;
        }

        if (i + 1 < code.size()) {
            const string two = code.substr(i, 2);
            if (two == "<=" || two == ">=" || two == "==" || two == "!=") {
                tokens.push_back({two, two});
                i += 2;
                continue;
            }
        }

        const string one(1, c);
        static const set<string> singles = {"(", ")", "{", "}", ";", "=", "<", ">", "+", "-", "*", "/"};
        if (singles.find(one) != singles.end()) {
            tokens.push_back({one, one});
            i++;
            continue;
        }

        error = string("Unsupported token: '") + c + "'";
        return {};
    }

    tokens.push_back({END_MARK, END_MARK});
    return tokens;
}

static vector<string> sortedVector(const set<string>& values) {
    return vector<string>(values.begin(), values.end());
}

static void filterResultForSnippet(
    Result& r,
    const set<string>& usedNonTerminals,
    const set<string>& usedTerminals
) {
    map<string, set<string>> filteredFirst;
    map<string, set<string>> filteredFollow;
    map<string, map<string, vector<string>>> filteredTable;

    for (const string& nt : sortedVector(usedNonTerminals)) {
        if (r.first.find(nt) != r.first.end()) {
            set<string> filteredSet;
            for (const string& sym : r.first[nt]) {
                if (sym == EPS || usedTerminals.find(sym) != usedTerminals.end()) filteredSet.insert(sym);
            }
            filteredFirst[nt] = filteredSet;
        }
        if (r.follow.find(nt) != r.follow.end()) {
            set<string> filteredSet;
            for (const string& sym : r.follow[nt]) {
                if (usedTerminals.find(sym) != usedTerminals.end()) filteredSet.insert(sym);
            }
            filteredFollow[nt] = filteredSet;
        }

        if (r.table.find(nt) == r.table.end()) continue;
        for (const string& term : sortedVector(usedTerminals)) {
            const auto cell = r.table[nt].find(term);
            if (cell != r.table[nt].end()) filteredTable[nt][term] = cell->second;
        }
    }

    r.first = filteredFirst;
    r.follow = filteredFollow;
    r.table = filteredTable;
}

static Result parseCode(const string& code) {
    const Grammar g = buildGrammar();
    Result r;
    r.first = computeFirst(g);
    r.follow = computeFollow(g, r.first);
    r.table = buildTable(g, r.first, r.follow, r.ll1_conflicts);
    r.ll1_ok = r.ll1_conflicts.empty();
    if (!r.ll1_ok) {
        r.accepted = false;
        r.message = "LL(1) conflict(s) in parsing table; predictive parse was not run.";
        return r;
    }

    string tokenizerError;
    const vector<LexToken> input = tokenize(code, tokenizerError);
    if (!tokenizerError.empty()) {
        r.accepted = false;
        r.message = tokenizerError;
        return r;
    }
    r.tokens = input;

    vector<string> stack = {END_MARK, g.start};
    size_t ip = 0;
    set<string> usedNonTerminals;
    set<string> usedTerminals;
    set<string> seenGrammarRules;
    for (const auto& token : input) usedTerminals.insert(token.kind);

    while (!stack.empty()) {
        const string X = stack.back();
        const string a = (ip < input.size() ? input[ip].kind : END_MARK);

        vector<string> stackView = stack;
        reverse(stackView.begin(), stackView.end());
        if (!stackView.empty() && ip < input.size()) {
            if (!isNonTerminal(g, stackView[0]) && stackView[0] == input[ip].kind) {
                stackView[0] = lexDisplay(input[ip]);
            }
        }

        vector<string> tokenInputParts;
        for (size_t j = ip; j < input.size(); j++) {
            tokenInputParts.push_back(lexDisplay(input[j]));
        }

        TraceRow row;
        row.stack = join(stackView);
        row.input = join(tokenInputParts);

        if (X == END_MARK && a == END_MARK) {
            row.action = "Accept";
            r.trace.push_back(row);
            r.accepted = true;
            r.message = "Snippet accepted by parser.";
            break;
        }

        if (!isNonTerminal(g, X)) {
            if (X == a) {
                row.action = string("Match ") + lexDisplay(input[ip]);
                stack.pop_back();
                ip++;
            } else {
                const string foundTok = (ip < input.size() ? lexDisplay(input[ip]) : string("$"));
                row.action = string("Error: expected ") + X + string(", found ") + foundTok;
                r.accepted = false;
                r.message = "Snippet rejected (terminal mismatch).";
                r.trace.push_back(row);
                break;
            }
            r.trace.push_back(row);
            continue;
        }

        usedNonTerminals.insert(X);
        const auto tableRow = r.table.find(X);
        if (tableRow == r.table.end() || tableRow->second.find(a) == tableRow->second.end()) {
            const string la = (ip < input.size() ? lexDisplay(input[ip]) : string("$"));
            row.action = string("Error: no table entry M[") + X + string(", ") + la + string("]");
            r.accepted = false;
            r.message = "Snippet rejected (no LL(1) table entry).";
            r.trace.push_back(row);
            break;
        }

        const vector<string> production = tableRow->second.at(a);
        stack.pop_back();
        if (!(production.size() == 1 && production[0] == EPS)) {
            for (auto it = production.rbegin(); it != production.rend(); ++it) stack.push_back(*it);
        }
        const string grammarRule = X + " -> " + join(production);
        if (seenGrammarRules.insert(grammarRule).second) r.grammarUsed.push_back(grammarRule);
        row.action = X + " -> " + formatProductionRhs(production, input, ip);
        r.trace.push_back(row);
    }

    if (!r.accepted && r.message.empty()) r.message = "Snippet rejected.";
    filterResultForSnippet(r, usedNonTerminals, usedTerminals);
    return r;
}

static void printJson(const Result& r) {
    cout << "{";
    cout << "\"accepted\":" << (r.accepted ? "true" : "false") << ",";
    cout << "\"message\":\"" << toJsonString(r.message) << "\",";
    cout << "\"ll1_ok\":" << (r.ll1_ok ? "true" : "false") << ",";

    cout << "\"ll1_conflicts\":[";
    for (size_t i = 0; i < r.ll1_conflicts.size(); i++) {
        if (i) cout << ",";
        cout << "\"" << toJsonString(r.ll1_conflicts[i]) << "\"";
    }
    cout << "],";

    cout << "\"tokens\":[";
    for (size_t i = 0; i < r.tokens.size(); i++) {
        if (i) cout << ",";
        cout << "{\"kind\":\"" << toJsonString(r.tokens[i].kind) << "\",";
        cout << "\"lexeme\":\"" << toJsonString(r.tokens[i].lexeme) << "\"}";
    }
    cout << "],";

    cout << "\"grammar_used\":[";
    for (size_t i = 0; i < r.grammarUsed.size(); i++) {
        if (i) cout << ",";
        cout << "\"" << toJsonString(r.grammarUsed[i]) << "\"";
    }
    cout << "],";

    cout << "\"first\":{";
    bool firstNt = true;
    for (const auto& nt : r.first) {
        if (!firstNt) cout << ",";
        firstNt = false;
        cout << "\"" << toJsonString(nt.first) << "\":[";
        bool firstItem = true;
        for (const auto& token : nt.second) {
            if (!firstItem) cout << ",";
            firstItem = false;
            cout << "\"" << toJsonString(token) << "\"";
        }
        cout << "]";
    }
    cout << "},";

    cout << "\"follow\":{";
    bool followNt = true;
    for (const auto& nt : r.follow) {
        if (!followNt) cout << ",";
        followNt = false;
        cout << "\"" << toJsonString(nt.first) << "\":[";
        bool firstItem = true;
        for (const auto& token : nt.second) {
            if (!firstItem) cout << ",";
            firstItem = false;
            cout << "\"" << toJsonString(token) << "\"";
        }
        cout << "]";
    }
    cout << "},";

    cout << "\"table\":{";
    bool rowFirst = true;
    for (const auto& row : r.table) {
        if (!rowFirst) cout << ",";
        rowFirst = false;
        cout << "\"" << toJsonString(row.first) << "\":{";
        bool cellFirst = true;
        for (const auto& cell : row.second) {
            if (!cellFirst) cout << ",";
            cellFirst = false;
            cout << "\"" << toJsonString(cell.first) << "\":\"" << toJsonString(join(cell.second)) << "\"";
        }
        cout << "}";
    }
    cout << "},";

    cout << "\"trace\":[";
    for (size_t i = 0; i < r.trace.size(); i++) {
        if (i) cout << ",";
        cout << "{";
        cout << "\"stack\":\"" << toJsonString(r.trace[i].stack) << "\",";
        cout << "\"input\":\"" << toJsonString(r.trace[i].input) << "\",";
        cout << "\"action\":\"" << toJsonString(r.trace[i].action) << "\"";
        cout << "}";
    }
    cout << "]";
    cout << "}";
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ostringstream buffer;
    buffer << cin.rdbuf();
    const string code = trim(buffer.str());

    const Result result = parseCode(code);
    printJson(result);
    return 0;
}
