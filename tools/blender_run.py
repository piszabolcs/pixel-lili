#!/usr/bin/env python3
"""Pixel Lili: run a headless Blender script from Windows (the GPU and Blender live here, not in WSL).

    py -3.12 tools/blender_run.py tools/blender/scene_setup.py -- --out assets/blender/scene.blend
    py -3.12 tools/blender_run.py --blend assets/blender/lili.blend tools/blender/render_all.py -- --anim walk

Blender is found in $BLENDER or the newest "C:/Program Files/Blender Foundation/Blender */blender.exe".
It runs from the repo root with --factory-startup (no user add-ons or preferences leak into a
build) and --python-exit-code 1 (a failing script fails the run). The script's own "lili:" lines
are printed; on failure the last 25 lines of Blender's output too. The full log goes to
assets/raw_renders/_logs/<script>.log.
"""
import glob, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def find_blender():
    if os.environ.get("BLENDER"):
        return os.environ["BLENDER"]
    found = sorted(glob.glob(r"C:\Program Files\Blender Foundation\Blender *\blender.exe"))
    if not found:
        sys.exit("no Blender: install it or set BLENDER=<path to blender.exe>")
    return found[-1]

def main():
    args = sys.argv[1:]
    blend = None
    if args[:1] == ["--blend"]:
        blend, args = args[1], args[2:]
    if not args:
        sys.exit(__doc__)
    script, rest = args[0], args[1:]
    cmd = [find_blender(), "-b"] + ([blend] if blend else []) + ["--factory-startup", "--python-exit-code", "1", "-P", script] + rest
    logdir = os.path.join(ROOT, "assets", "raw_renders", "_logs")
    os.makedirs(logdir, exist_ok=True)
    log = os.path.join(logdir, os.path.splitext(os.path.basename(script))[0] + ".log")
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = (p.stdout or "") + (p.stderr or "")
    open(log, "w", encoding="utf-8").write(out)
    for line in out.splitlines():
        if line.startswith("lili:"):
            print(line)
    if p.returncode:
        print("\n".join(out.splitlines()[-25:]))
        print(f"blender exit {p.returncode} (full log: {os.path.relpath(log, ROOT)})")
    sys.exit(p.returncode)

if __name__ == "__main__":
    main()
