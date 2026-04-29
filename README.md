# C Snippet Syntax Checker

This project is a from-scratch LL(1)-based syntax checker for small C snippets.

## What It Does

- Accepts C snippet text from a browser UI
- Tokenizes and parses using a C++ LL(1)-style parser engine
- Displays:
  - FIRST sets
  - FOLLOW sets
  - LL(1) table
  - Grammar-to-token correlation table
  - Separate grammar and token Stack/Input/Action traces
  - Final accepted/rejected result

## Project Files

- `backend/parser.cpp` - grammar, tokenizer, FIRST/FOLLOW, table, and parser trace output
- `backend/server.py` - HTTP server and `/api/parse` endpoint
- `frontend/index.html` - landing page
- `frontend/parser.html` - analyzer page
- `frontend/app.js` - UI logic
- `frontend/styles.css` - styling

## Run Locally

1. Install Python 3
2. Install `g++` (for example via MinGW on Windows) and add it to PATH
3. Run:

```powershell
python backend/server.py
```

4. Open [http://127.0.0.1:8080](http://127.0.0.1:8080)

The backend auto-compiles `backend/parser.cpp` into `backend/parser_engine.exe` when needed.

## Supported Grammar Scope

- declarations: `int x;`, `float y = 3;`
- assignments: `x = x + 1;`
- blocks: `{ ... }`
- `if/else`, `while`, and `for` statements
- optional preprocessor include lines such as `#include<stdio.h>` or `# include "math.h"`
- `main` function forms such as `int main() { ... }`
- arithmetic expressions with `+ - * /`
- relational operators `< > <= >= == !=`

This is intentionally a subset parser for learning and demonstration, not a full C compiler.
