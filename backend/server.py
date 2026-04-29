import json
import os
import subprocess
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlparse

from clr_engine import run_clr_analysis


BASE_DIR = Path(__file__).resolve().parent.parent
FRONTEND_DIR = BASE_DIR / "frontend"
BACKEND_DIR = BASE_DIR / "backend"
ENGINE_PATH = BACKEND_DIR / "parser_engine.exe"
SOURCE_PATH = BACKEND_DIR / "parser.cpp"


def compile_engine():
    if ENGINE_PATH.exists() and ENGINE_PATH.stat().st_mtime >= SOURCE_PATH.stat().st_mtime:
        return None
    try:
        subprocess.run(
            ["g++", "-std=c++17", str(SOURCE_PATH), "-o", str(ENGINE_PATH)],
            check=True,
            capture_output=True,
            text=True,
        )
        return None
    except FileNotFoundError:
        return "g++ not found. Install MinGW or another C++ compiler and add it to PATH."
    except subprocess.CalledProcessError as ex:
        return f"C++ compile error: {ex.stderr.strip()}"


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        # Keep terminal output clean and focused on startup/errors.
        return

    def _send_json(self, status, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_file(self, path: Path, content_type: str):
        if not path.exists():
            self.send_error(404, "Not found")
            return
        data = path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/" or path == "/index.html":
            file_path = FRONTEND_DIR / "index.html"
            content_type = "text/html; charset=utf-8"
        elif path == "/parser.html":
            file_path = FRONTEND_DIR / "parser.html"
            content_type = "text/html; charset=utf-8"
        elif path == "/styles.css":
            file_path = FRONTEND_DIR / "styles.css"
            content_type = "text/css; charset=utf-8"
        elif path == "/app.js":
            file_path = FRONTEND_DIR / "app.js"
            content_type = "application/javascript; charset=utf-8"
        else:
            self.send_error(404, "Not found")
            return
        self._send_file(file_path, content_type)

    def do_POST(self):
        if urlparse(self.path).path != "/api/parse":
            self.send_error(404, "Not found")
            return

        content_length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(content_length).decode("utf-8")

        try:
            body = json.loads(raw_body) if raw_body else {}
        except json.JSONDecodeError:
            self._send_json(400, {"error": "Invalid JSON payload."})
            return

        code = body.get("code", "")
        mode = body.get("mode", "ll1")
        if not isinstance(code, str) or not code.strip():
            self._send_json(400, {"error": "Please enter a C snippet."})
            return
        if mode not in {"ll1", "clr"}:
            self._send_json(400, {"error": "Unsupported parser mode."})
            return

        if mode == "clr":
            try:
                response_json = run_clr_analysis(code)
                self._send_json(200, response_json)
            except ValueError as ex:
                self._send_json(400, {"error": str(ex)})
            except Exception as ex:
                self._send_json(500, {"error": f"CLR parser failed: {ex}"})
            return

        compile_error = compile_engine()
        if compile_error:
            self._send_json(500, {"error": compile_error})
            return

        try:
            run = subprocess.run(
                [str(ENGINE_PATH)],
                input=code,
                capture_output=True,
                text=True,
                check=True,
            )
            response_json = json.loads(run.stdout)
            self._send_json(200, response_json)
        except subprocess.CalledProcessError as ex:
            details = ex.stderr.strip() or "Unknown parser error."
            self._send_json(500, {"error": f"Parser engine failed: {details}"})
        except json.JSONDecodeError:
            self._send_json(500, {"error": "Parser engine returned malformed JSON."})


def main():
    port = int(os.environ.get("PORT", "8080"))
    server = HTTPServer(("127.0.0.1", port), Handler)
    print(f"Server running at http://127.0.0.1:{port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
