"""Evaluación experimental de RobotLang.

Reproduce todas las tablas del informe. Desde la raíz del repo:
    cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release && cmake --build build-rel
    python3 bench/run_bench.py            # 30 repeticiones (por defecto)
    python3 bench/run_bench.py --runs 5   # prueba rápida
Escribe los resultados en bench/results.json y los muestra en pantalla.
"""
import argparse
import json
import platform
import re
import statistics
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BENCH = ROOT / "bench"
OUT = BENCH / "out"
ROBOTC = ROOT / "build-rel" / "robotc"
MAX_TICKS = "100000000"
PROGRAMS = ["square_big", "redundant", "multi", "interaction"]
MANUAL = {"square_big": "square_big", "redundant": "redundant"}
LOC_PAIRS = [("examples/patrol.robot", "bench/manual/patrol.cpp"),
             ("bench/programs/square_big.robot", "bench/manual/square_big.cpp"),
             ("bench/programs/redundant.robot", "bench/manual/redundant.cpp")]


def sh(args, **kw):
    return subprocess.run(args, capture_output=True, text=True, check=True, **kw)


def loc(path):
    """Líneas de código sin contar líneas vacías ni comentarios."""
    n = 0
    for line in (ROOT / path).read_text().splitlines():
        s = line.strip()
        if s and not s.startswith(("#", "//")):
            n += 1
    return n


def timeit(cmd, runs):
    """Ejecuta el comando `runs` veces y devuelve la media y la desviación en ms."""
    sh(cmd)  # calentamiento
    samples = []
    for _ in range(runs):
        t0 = time.perf_counter()
        sh(cmd)
        samples.append((time.perf_counter() - t0) * 1000)
    return {"mean_ms": statistics.mean(samples), "sd_ms": statistics.stdev(samples) if runs > 1 else 0.0}


def build_tools():
    OUT.mkdir(exist_ok=True)
    sources = [str(p) for p in (ROOT / "src").glob("*.cpp") if p.name != "main.cpp"]
    sh(["g++", "-std=c++17", "-O2", f"-I{ROOT / 'src'}", str(BENCH / "opt_stats.cpp"), *sources,
        "-o", str(OUT / "opt_stats")])
    for name in MANUAL.values():
        sh(["g++", "-std=c++17", "-O2", str(BENCH / "manual" / f"{name}.cpp"), "-o", str(OUT / f"manual_{name}")])


def environment():
    def first(cmd):
        try:
            return sh(cmd).stdout.splitlines()[0]
        except Exception:
            return "?"
    cpu = "?"
    try:
        cpu = next(l.split(":", 1)[1].strip() for l in Path("/proc/cpuinfo").read_text().splitlines()
                   if l.startswith("model name"))
    except Exception:
        pass
    return {"cpu": cpu, "os": platform.platform(), "gcc": first(["gcc", "--version"]),
            "python": platform.python_version()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=30)
    args = ap.parse_args()
    build_tools()
    results = {"environment": environment(), "runs": args.runs,
               "expressiveness": [], "optimizations": {}, "runtime": {}, "errors": []}

    # 1. Expresividad
    for dsl, manual in LOC_PAIRS:
        results["expressiveness"].append({"program": Path(dsl).name, "dsl_loc": loc(dsl), "cpp_loc": loc(manual)})

    # 2. Optimizaciones y 3. rendimiento
    for prog in PROGRAMS:
        src = BENCH / "programs" / f"{prog}.robot"
        results["optimizations"][prog] = {}
        for mode in ["none", "merge", "turns", "all"]:
            asm, binary = OUT / f"{prog}_{mode}.s", OUT / f"{prog}_{mode}"
            stats = sh([str(OUT / "opt_stats"), str(src), mode, str(asm), MAX_TICKS]).stdout
            row = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", stats)}
            row["asm_lines"] = len(asm.read_text().splitlines())
            sh(["gcc", str(asm), "-o", str(binary)])
            row["output"] = sh([str(binary)]).stdout
            row["time"] = timeit([str(binary)], args.runs)
            results["optimizations"][prog][mode] = row
            print(f"{prog:12} {mode:6} instr {row['instr_before']:>3}->{row['instr_after']:<3} "
                  f"ticks {row['ticks_after']:>9}  asm {row['asm_lines']:>4}  "
                  f"{row['time']['mean_ms']:8.2f} ± {row['time']['sd_ms']:.2f} ms")
        rt = {"simulator": timeit([str(ROBOTC), str(src), "--run", "--max-ticks", MAX_TICKS], args.runs)}
        sim_out = sh([str(ROBOTC), str(src), "--run", "--max-ticks", MAX_TICKS]).stdout
        rt["same_output"] = sim_out == results["optimizations"][prog]["none"]["output"] \
            == results["optimizations"][prog]["all"]["output"]
        if prog in MANUAL:
            manual = OUT / f"manual_{MANUAL[prog]}"
            rt["manual_cpp_O2"] = timeit([str(manual)], args.runs)
            rt["manual_same_output"] = sh([str(manual)]).stdout == sim_out
        results["runtime"][prog] = rt
        print(f"{prog:12} simulador {rt['simulator']['mean_ms']:8.2f} ms  misma salida: {rt['same_output']}"
              + (f"  manual {rt['manual_cpp_O2']['mean_ms']:.2f} ms" if prog in MANUAL else ""))

    # 4. Detección de errores
    for f in sorted((BENCH / "errors").glob("*.robot")):
        p = subprocess.run([str(ROBOTC), str(f), "--ir"], capture_output=True, text=True)
        msg = p.stderr.strip().splitlines()[0].split(": ", 1)[1] if p.returncode else ""
        results["errors"].append({"program": f.name, "detected": p.returncode != 0, "message": msg})
        print(f"{f.name:32} {'detectado' if p.returncode else 'NO detectado':12} {msg}")

    (BENCH / "results.json").write_text(json.dumps(results, indent=2, ensure_ascii=False))
    print(f"\nResultados en {BENCH / 'results.json'}")


if __name__ == "__main__":
    main()
