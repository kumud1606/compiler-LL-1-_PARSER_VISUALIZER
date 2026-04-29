from collections import deque


EPS = "eps"
END_MARK = "$"


def build_grammar():
    raw = {
        "Program": [["IncludeList", "ProgramCore"]],
        "IncludeList": [["IncludeDirective", "IncludeList"], [EPS]],
        "IncludeDirective": [["#", "include", "header"]],
        "ProgramCore": [["Type", "ProgramAfterType"], ["NonTypeStmt", "StmtList"], [EPS]],
        "ProgramAfterType": [["main", "(", ")", "Block"], ["id", "DeclInit", ";", "StmtList"]],
        "StmtList": [["Stmt", "StmtList"], [EPS]],
        "Stmt": [["Decl", ";"], ["Assign", ";"], ["IfStmt"], ["WhileStmt"], ["ForStmt"], ["Block"]],
        "NonTypeStmt": [["Assign", ";"], ["IfStmt"], ["WhileStmt"], ["ForStmt"], ["Block"]],
        "Block": [["{", "StmtList", "}"]],
        "Decl": [["Type", "id", "DeclInit"]],
        "Type": [["int"], ["float"], ["char"]],
        "DeclInit": [["=", "Expr"], [EPS]],
        "Assign": [["id", "=", "Expr"]],
        "IfStmt": [["if", "(", "BoolExpr", ")", "Block", "ElsePart"]],
        "WhileStmt": [["while", "(", "BoolExpr", ")", "Block"]],
        "ForStmt": [["for", "(", "ForInit", ";", "ForCond", ";", "ForUpdate", ")", "Block"]],
        "ForInit": [["Decl"], ["Assign"], [EPS]],
        "ForCond": [["BoolExpr"], [EPS]],
        "ForUpdate": [["Assign"], [EPS]],
        "ElsePart": [["else", "Block"], [EPS]],
        "BoolExpr": [["Expr", "BoolExprTail"]],
        "BoolExprTail": [["RelOp", "Expr"], [EPS]],
        "RelOp": [["<"], [">"], ["<="], [">="], ["=="], ["!="]],
        "Expr": [["Term", "ExprTail"]],
        "ExprTail": [["+", "Term", "ExprTail"], ["-", "Term", "ExprTail"], [EPS]],
        "Term": [["Factor", "TermTail"]],
        "TermTail": [["*", "Factor", "TermTail"], ["/", "Factor", "TermTail"], [EPS]],
        "Factor": [["id"], ["num"], ["(", "Expr", ")"]],
    }

    productions = {}
    for lhs, rules in raw.items():
        productions[lhs] = [tuple() if rule == [EPS] else tuple(rule) for rule in rules]

    nonterminals = list(productions.keys())
    terminals = set()
    for rules in productions.values():
      for rule in rules:
        for sym in rule:
          if sym not in productions:
            terminals.add(sym)
    terminals.add(END_MARK)

    return {
        "start": "Program",
        "productions": productions,
        "nonterminals": nonterminals,
        "terminals": terminals,
    }


def tokenize(code):
    tokens = []
    i = 0

    def is_id_start(ch):
        return ch.isalpha() or ch == "_"

    def is_id_body(ch):
        return ch.isalnum() or ch == "_"

    while i < len(code):
        ch = code[i]
        if ch.isspace():
            i += 1
            continue

        if ch == "#":
            tokens.append({"kind": "#", "lexeme": "#"})
            i += 1
            continue

        if tokens and tokens[-1]["kind"] == "include" and ch in '<"':
            closing = ">" if ch == "<" else '"'
            j = i + 1
            while j < len(code) and code[j] != closing:
                j += 1
            if j >= len(code):
                raise ValueError("Unterminated header after include.")
            tokens.append({"kind": "header", "lexeme": code[i : j + 1]})
            i = j + 1
            continue

        if is_id_start(ch):
            j = i + 1
            while j < len(code) and is_id_body(code[j]):
                j += 1
            word = code[i:j]
            if word in {"if", "else", "int", "float", "char", "while", "for", "include", "main"}:
                tokens.append({"kind": word, "lexeme": word})
            else:
                tokens.append({"kind": "id", "lexeme": word})
            i = j
            continue

        if ch.isdigit():
            j = i + 1
            while j < len(code) and (code[j].isdigit() or code[j] == "."):
                j += 1
            tokens.append({"kind": "num", "lexeme": code[i:j]})
            i = j
            continue

        if i + 1 < len(code):
            two = code[i : i + 2]
            if two in {"<=", ">=", "==", "!="}:
                tokens.append({"kind": two, "lexeme": two})
                i += 2
                continue

        if ch in {"(", ")", "{", "}", ";", "=", "<", ">", "+", "-", "*", "/"}:
            tokens.append({"kind": ch, "lexeme": ch})
            i += 1
            continue

        raise ValueError(f"Unsupported token: '{ch}'")

    tokens.append({"kind": END_MARK, "lexeme": END_MARK})
    return tokens


def lex_display(token):
    if token["kind"] == "id":
        return f'id({token["lexeme"]})'
    if token["kind"] == "num":
        return f'num({token["lexeme"]})'
    if token["kind"] == "header":
        return f'header({token["lexeme"]})'
    return token["kind"]


def compute_first(grammar):
    first = {nt: set() for nt in grammar["nonterminals"]}

    changed = True
    while changed:
        changed = False
        for lhs, rules in grammar["productions"].items():
            for rule in rules:
                seq_first = first_of_sequence(rule, first, grammar["nonterminals"])
                before = len(first[lhs])
                first[lhs].update(seq_first)
                if len(first[lhs]) != before:
                    changed = True
    return first


def first_of_sequence(sequence, first, nonterminals):
    if not sequence:
        return {EPS}

    out = set()
    nullable = True
    for sym in sequence:
        if sym not in nonterminals:
            out.add(sym)
            nullable = False
            break
        out.update(first[sym] - {EPS})
        if EPS not in first[sym]:
            nullable = False
            break

    if nullable:
        out.add(EPS)
    return out


def closure(items, grammar, first):
    nonterminals = set(grammar["nonterminals"])
    productions = grammar["productions"]
    closed = set(items)

    changed = True
    while changed:
        changed = False
        additions = set()
        for lhs, rhs, dot, lookahead in closed:
            if dot >= len(rhs):
                continue
            symbol = rhs[dot]
            if symbol not in nonterminals:
                continue
            beta = rhs[dot + 1 :] + (lookahead,)
            lookaheads = first_of_sequence(beta, first, grammar["nonterminals"]) - {EPS}
            for prod in productions[symbol]:
                for la in lookaheads:
                    item = (symbol, prod, 0, la)
                    if item not in closed:
                        additions.add(item)
        if additions:
            closed.update(additions)
            changed = True
    return frozenset(closed)


def goto(items, symbol, grammar, first):
    moved = {
        (lhs, rhs, dot + 1, lookahead)
        for lhs, rhs, dot, lookahead in items
        if dot < len(rhs) and rhs[dot] == symbol
    }
    if not moved:
        return frozenset()
    return closure(moved, grammar, first)


def item_display(item):
    lhs, rhs, dot, lookahead = item
    pieces = list(rhs)
    pieces.insert(dot, ".")
    body = " ".join(pieces) if pieces else "."
    return f"{lhs} -> {body}, {lookahead}"


def production_display(lhs, rhs):
    return f"{lhs} -> {' '.join(rhs) if rhs else EPS}"


def build_clr_tables(grammar):
    nonterminals = set(grammar["nonterminals"])
    terminals = set(grammar["terminals"])
    first = compute_first(grammar)
    augmented_start = "Program'"
    augmented_prod = (grammar["start"],)

    start_state = closure({(augmented_start, augmented_prod, 0, END_MARK)}, grammar, first)
    states = [start_state]
    state_ids = {start_state: 0}
    transitions = {}
    queue = deque([start_state])
    symbols = [s for s in list(terminals - {END_MARK}) + grammar["nonterminals"]]

    while queue:
        state = queue.popleft()
        sid = state_ids[state]
        for symbol in symbols:
            nxt = goto(state, symbol, grammar, first)
            if not nxt:
                continue
            if nxt not in state_ids:
                state_ids[nxt] = len(states)
                states.append(nxt)
                queue.append(nxt)
            transitions[(sid, symbol)] = state_ids[nxt]

    action = {}
    goto_table = {}
    conflicts = []

    def place_action(state_id, terminal, value):
        row = action.setdefault(state_id, {})
        current = row.get(terminal)
        if current is not None and current != value:
            conflicts.append(f"ACTION[{state_id}, {terminal}] has both {current} and {value}")
            return
        row[terminal] = value

    for state_id, state in enumerate(states):
        for item in state:
            lhs, rhs, dot, lookahead = item
            if dot < len(rhs):
                symbol = rhs[dot]
                target = transitions.get((state_id, symbol))
                if symbol in terminals and symbol != END_MARK and target is not None:
                    place_action(state_id, symbol, f"s{target}")
                elif symbol in nonterminals and target is not None:
                    goto_table.setdefault(state_id, {})[symbol] = str(target)
            else:
                if lhs == augmented_start and lookahead == END_MARK:
                    place_action(state_id, END_MARK, "acc")
                else:
                    place_action(state_id, lookahead, f"r({production_display(lhs, rhs)})")

    state_payload = []
    for state_id, state in enumerate(states):
        ordered = sorted(state, key=lambda item: (item[0], item[1], item[2], item[3]))
        state_payload.append({
            "state": state_id,
            "items": [item_display(item) for item in ordered],
        })

    return {
        "states": state_payload,
        "action": {str(k): v for k, v in action.items()},
        "goto": {str(k): v for k, v in goto_table.items()},
        "conflicts": conflicts,
    }


def format_lr_stack(states, symbols):
    parts = [str(states[0])]
    for index, symbol in enumerate(symbols):
        parts.append(symbol)
        parts.append(str(states[index + 1]))
    return " ".join(parts)


def parse_clr(tokens, tables, grammar):
    action_table = {int(k): v for k, v in tables["action"].items()}
    goto_table = {int(k): v for k, v in tables["goto"].items()}
    productions = grammar["productions"]

    states = [0]
    symbols = []
    trace = []
    grammar_used = []
    seen_rules = set()
    ip = 0

    while True:
        state = states[-1]
        token = tokens[ip]["kind"]
        input_view = " ".join(lex_display(t) for t in tokens[ip:])
        row = {
            "stack": format_lr_stack(states, symbols),
            "input": input_view,
            "action": "",
        }

        action = action_table.get(state, {}).get(token)
        if action is None:
            row["action"] = f"Error: no ACTION[{state}, {lex_display(tokens[ip])}]"
            trace.append(row)
            return {
                "accepted": False,
                "message": "Snippet rejected by CLR parser.",
                "trace": trace,
                "grammar_used": grammar_used,
            }

        if action == "acc":
            row["action"] = "Accept"
            trace.append(row)
            return {
                "accepted": True,
                "message": "Snippet accepted by CLR parser.",
                "trace": trace,
                "grammar_used": grammar_used,
            }

        if action.startswith("s"):
            next_state = int(action[1:])
            row["action"] = f"Shift {lex_display(tokens[ip])}, goto {next_state}"
            symbols.append(lex_display(tokens[ip]))
            states.append(next_state)
            ip += 1
            trace.append(row)
            continue

        rule_text = action[2:-1]
        lhs, rhs_text = rule_text.split(" -> ", 1)
        rhs = tuple() if rhs_text == EPS else tuple(rhs_text.split())
        pop_count = len(rhs)
        for _ in range(pop_count):
            states.pop()
            symbols.pop()

        goto_state = goto_table.get(states[-1], {}).get(lhs)
        if goto_state is None:
            row["action"] = f"Error: missing GOTO[{states[-1]}, {lhs}]"
            trace.append(row)
            return {
                "accepted": False,
                "message": "Snippet rejected by CLR parser.",
                "trace": trace,
                "grammar_used": grammar_used,
            }

        symbols.append(lhs)
        states.append(int(goto_state))
        if rule_text not in seen_rules:
            grammar_used.append(rule_text)
            seen_rules.add(rule_text)
        row["action"] = f"Reduce by {rule_text}; goto {goto_state}"
        trace.append(row)


def run_clr_analysis(code):
    grammar = build_grammar()
    tokens = tokenize(code.strip())
    tables = build_clr_tables(grammar)

    if tables["conflicts"]:
        return {
            "accepted": False,
            "message": "CLR conflict(s) detected; parse was not run.",
            "mode": "clr",
            "clr_ok": False,
            "clr_conflicts": tables["conflicts"],
            "grammar_used": [],
            "states": tables["states"],
            "action_table": tables["action"],
            "goto_table": tables["goto"],
            "trace": [],
        }

    parse_result = parse_clr(tokens, tables, grammar)
    return {
        "accepted": parse_result["accepted"],
        "message": parse_result["message"],
        "mode": "clr",
        "clr_ok": True,
        "clr_conflicts": [],
        "grammar_used": parse_result["grammar_used"],
        "states": tables["states"],
        "action_table": tables["action"],
        "goto_table": tables["goto"],
        "trace": parse_result["trace"],
    }
