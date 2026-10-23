#!/usr/bin/env python3
"""Pixel Lili: local receiver for PixelLab editor exports, so no Save dialog opens (2026-10-23).

    py -3.12 tools/pixellab_recv.py [out_dir]        (default: assets/pixellab), listens on 127.0.0.1:8765

GET  /recv.html          a page the editor tab opens as a popup (tools/pixellab_editor_hooks.js); it takes
                         postMessage({name, buf}) from https://(www.)pixellab.ai and POSTs it back here
POST /save?name=x.png    writes the body into out_dir (name: letters, digits, _ . -, PNG signature checked)
Only the loopback interface, only PNG, only into out_dir.
"""
import http.server, os, re, sys, urllib.parse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "assets", "pixellab")
os.makedirs(OUT, exist_ok=True)
PAGE = b"""<!doctype html><meta charset="utf-8"><title>lili recv</title>
<body style="background:#222;color:#ccc;font:14px sans-serif">lili recv: ready<pre id="log"></pre>
<script>
const log = document.getElementById('log');
window.addEventListener('message', async (ev) => {
  if (!/^https:\\/\\/(www\\.)?pixellab\\.ai$/.test(ev.origin)) return;
  const {name, buf} = ev.data || {};
  if (!name || !buf) return;
  const r = await fetch('/save?name=' + encodeURIComponent(name), {method: 'POST', body: buf});
  const t = await r.text();
  log.textContent += t + '\\n';
  ev.source.postMessage({saved: name, status: r.status, text: t}, ev.origin);
});
if (window.opener) window.opener.postMessage({recvReady: true}, '*');
</script>"""

class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.split('?')[0] != '/recv.html':
            self.send_error(404); return
        self.send_response(200); self.send_header('Content-Type', 'text/html'); self.end_headers()
        self.wfile.write(PAGE)

    def do_POST(self):
        u = urllib.parse.urlparse(self.path)
        name = urllib.parse.parse_qs(u.query).get('name', [''])[0]
        if u.path != '/save' or not re.fullmatch(r'[A-Za-z0-9_.-]{1,80}\.png', name):
            self.send_error(400); return
        data = self.rfile.read(int(self.headers.get('Content-Length', 0)))
        if not data.startswith(b'\x89PNG'):
            self.send_error(415); return
        with open(os.path.join(OUT, name), 'wb') as f:
            f.write(data)
        msg = f'saved {name} {len(data)} B'
        print(msg, flush=True)
        self.send_response(200); self.send_header('Content-Type', 'text/plain'); self.end_headers()
        self.wfile.write(msg.encode())

    def log_message(self, *a):
        pass

print(f"lili recv on 127.0.0.1:8765 -> {OUT}", flush=True)
http.server.ThreadingHTTPServer(('127.0.0.1', 8765), H).serve_forever()
