#!/usr/bin/env python3
"""Pixel Lili: talk to the Blender that is open on the desk (the MCP for Blender add-on, localhost:9876).

    py -3.12 tools/blender_live.py scene                       objects in the open scene
    py -3.12 tools/blender_live.py shot docs/img/live.png      the 3D viewport as a PNG
    py -3.12 tools/blender_live.py exec tools/blender/x.py     run a script inside the open Blender
    py -3.12 tools/blender_live.py exec -c "import bpy; ..."   ... or a line of code

The same socket and JSON commands the MCP server (uvx mcp-for-blender) uses, so the work is
visible live in the Blender window even before Claude Code has loaded the MCP tools. Blender
must be running with a window (the add-on cannot serve in `blender -b`); it starts its server
by itself when a file is loaded. Only localhost; nothing leaves the machine.
"""
import json, os, socket, sys

def send(cmd, params=None, timeout=120):
    s = socket.create_connection(("127.0.0.1", 9876), timeout=timeout)
    s.sendall(json.dumps({"type": cmd, "params": params or {}}).encode())
    buf = b""
    while True:
        chunk = s.recv(65536)
        if not chunk:
            break
        buf += chunk
        try:
            return json.loads(buf)
        except json.JSONDecodeError:
            continue
    return json.loads(buf)

def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    try:
        if a[0] == "scene":
            r = send("get_scene_info")
            res = r.get("result", r)
            print(f"{res.get('name')}: {res.get('object_count')} objects")
            for o in res.get("objects", []):
                print(f"  {o.get('name'):<20} {o.get('type'):<8} {o.get('location')}")
        elif a[0] == "shot":
            out = os.path.abspath(a[1])
            os.makedirs(os.path.dirname(out), exist_ok=True)
            r = send("get_viewport_screenshot", {"filepath": out, "max_size": int(a[2]) if len(a) > 2 else 800})
            print(r.get("status"), r.get("result", r.get("message")))
        elif a[0] == "exec":
            code = a[2] if a[1] == "-c" else open(a[1], encoding="utf-8").read()
            r = send("execute_code", {"code": code})
            res = r.get("result", r)
            print(r.get("status"), res.get("result", res) if isinstance(res, dict) else res)
        else:
            sys.exit(__doc__)
    except ConnectionRefusedError:
        sys.exit("no Blender on localhost:9876: open Blender (with a window) and load a .blend; the add-on starts its server then")

if __name__ == "__main__":
    main()
