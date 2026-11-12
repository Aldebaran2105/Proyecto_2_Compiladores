"""Servidor del entorno web de RobotLang.

Solo usa la librería estándar de Python. Recibe el código desde la página,
ejecuta el compilador `robotc` en cada modo y devuelve todo en JSON:
tokens, AST, IR (antes y después de optimizar), ensamblador x86-64,
traza de la simulación y la salida del programa ya compilado.

Uso (desde la raíz del repo, después de compilar con CMake):
    python3 web/server.py
y abrir http://localhost:8000
"""

import json
import os
import re
import shutil
import subprocess
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

WEB_DIR = Path(__file__).resolve().parent
ROOT_DIR = WEB_DIR.parent
ROBOTC = Path(os.environ.get("ROBOTC", ROOT_DIR / "build" / "robotc"))
EXAMPLES_DIR = ROOT_DIR / "examples"
PORT = int(os.environ.get("PORT", "8000"))
TIMEOUT = 5  # segundos máximos por cada llamada

# Formato de error de robotc:  archivo:linea:columna: mensaje
ERROR_RE = re.compile(r"^.*?:(\d+):(\d+):\s*(.*)$")


def run(args):
    """Ejecuta un comando y devuelve (codigo_de_salida, stdout, stderr)."""
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=TIMEOUT)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return 1, "", "tiempo de ejecucion excedido"


def parse_errors(stderr, phase):
    """Convierte la salida de error de robotc en una lista de errores."""
    errors = []
    for line in stderr.splitlines():
        m = ERROR_RE.match(line)
        if m:
            errors.append({"phase": phase, "line": int(m.group(1)),
                           "col": int(m.group(2)), "message": m.group(3)})
        elif line.strip():
            errors.append({"phase": phase, "line": 0, "col": 0, "message": line.strip()})
    return errors


def compile_program(source, optimize):
    """Pasa el programa por todas las fases del compilador."""
    out = {"ok": False, "errors": []}
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / "program.robot"
        src.write_text(source, encoding="utf-8")
        robotc = str(ROBOTC)

        # 1. Scanner: los tokens siempre se muestran, aunque haya errores
        code, stdout, stderr = run([robotc, str(src), "--tokens"])
        out["tokens"] = stdout
        if code != 0:
            lexical = []
            for line in stdout.splitlines():
                parts = line.split("\t")
                if len(parts) == 3 and parts[1] == "ERR":
                    ln, col = parts[0].split(":")
                    lexical.append({"phase": "lexico", "line": int(ln), "col": int(col),
                                    "message": f"símbolo no válido {parts[2]}"})
            out["errors"] = lexical or parse_errors(stderr, "lexico")
            return out

        # 2. Parser: AST
        code, stdout, stderr = run([robotc, str(src), "--ast"])
        if code != 0:
            out["errors"] = parse_errors(stderr, "sintactico")
            return out
        out["ast"] = stdout

        # 3. Semántica: IR original
        code, stdout, stderr = run([robotc, str(src), "--ir"])
        if code != 0:
            out["errors"] = parse_errors(stderr, "semantico")
            return out
        out["ir"] = stdout

        # 4. Optimización: IR optimizada
        _, out["irOptimized"], _ = run([robotc, str(src), "--ir", "--optimize"])

        # 5. Simulación: traza por ticks y estado final
        _, out["trace"], _ = run([robotc, str(src), "--trace"])
        _, out["result"], _ = run([robotc, str(src), "--run"])

        # 6. Generación de código x86-64 (con o sin optimizar)
        asm = Path(tmp) / "program.s"
        args = [robotc, str(src), "--emit", str(asm)] + (["--optimize"] if optimize else [])
        code, _, stderr = run(args)
        out["asm"] = asm.read_text(encoding="utf-8") if code == 0 and asm.exists() else stderr

        # 7. Ejecución real: ensamblar con gcc y correr el binario
        gcc = shutil.which("gcc")
        if gcc and code == 0:
            binary = Path(tmp) / "program"
            gcode, _, gerr = run([gcc, str(asm), "-o", str(binary)])
            if gcode == 0:
                _, eout, eerr = run([str(binary)])
                out["execution"] = eout + eerr
            else:
                out["execution"] = "gcc no pudo ensamblar el programa:\n" + gerr
        else:
            out["execution"] = "gcc no esta disponible en este equipo."

        out["ok"] = True
    return out


def list_examples():
    return {p.name: p.read_text(encoding="utf-8")
            for p in sorted(EXAMPLES_DIR.glob("*.robot"))}


class Handler(BaseHTTPRequestHandler):
    def send_json(self, data, status=200):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            body = (WEB_DIR / "index.html").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/api/examples":
            self.send_json(list_examples())
        else:
            self.send_error(404)

    def do_POST(self):
        if self.path != "/api/compile":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", 0))
        try:
            data = json.loads(self.rfile.read(length) or b"{}")
        except json.JSONDecodeError:
            self.send_json({"ok": False, "errors": [{"message": "JSON invalido"}]}, 400)
            return
        self.send_json(compile_program(str(data.get("source", "")),
                                       bool(data.get("optimize", False))))

    def log_message(self, fmt, *args):
        pass  # sin ruido en la terminal


if __name__ == "__main__":
    if not ROBOTC.exists():
        raise SystemExit(f"No se encontro el compilador en {ROBOTC}. Compila primero con CMake.")
    print(f"RobotLang IDE en http://localhost:{PORT}  (Ctrl+C para salir)")
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
