// Pixel Lili: hooks for driving the PixelLab editor (pixellab.ai/editor, Pixelorama in a same-origin iframe)
// from Claude in Chrome without any native file dialog (2026-10-23). Run once per page load in the editor tab
// (javascript_tool), with tools/pixellab_recv.py running. Then:
//   open an image:  File > Open (the picker is captured) -> __prepUpload() -> find "claude file input" ->
//                   file_upload the PNG -> __kick() -> confirm "Importálás másként: Új projekt"
//   save a sheet:   click the "lili recv" button once (a real click, so the popup is allowed), then
//                   File > Export as > Képsorozat > Exportálás; the PNG goes to the receiver, not to Downloads.
(() => {
  const top_ = window, w = document.querySelector('iframe').contentWindow;
  // 1. Pixelorama opens files with a hidden <input type=file>.click(): keep the input, open no dialog
  w.__fileInputs = w.__fileInputs || [];
  if (!w.__clickPatched) {
    const orig = w.HTMLInputElement.prototype.click;
    w.HTMLInputElement.prototype.click = function () {
      if (this.type === 'file') { w.document.body.appendChild(this); w.__fileInputs.push(this); return; }
      return orig.call(this);
    };
    w.__clickPatched = true;
  }
  // the accessibility tree (find) does not reach into the iframe: move the input into the top document
  top_.__prepUpload = () => {
    const inp = w.__fileInputs[w.__fileInputs.length - 1];
    if (!inp) return 'none';
    const old = document.getElementById('claude-file-input'); if (old && old !== inp) old.remove();
    inp.id = 'claude-file-input'; inp.setAttribute('aria-label', 'claude file input');
    document.body.appendChild(inp);
    inp.style.cssText = 'position:fixed;left:0;bottom:0;width:120px;height:24px;z-index:99999;opacity:0.01';
    return 'ready ' + w.__fileInputs.length;
  };
  // Pixelorama reads the file (global fileData) after the window gets focus back from the dialog
  top_.__kick = () => {
    for (const t of [w, w.document.querySelector('canvas')]) { t.dispatchEvent(new w.FocusEvent('blur')); t.dispatchEvent(new w.FocusEvent('focus')); }
    const i = document.getElementById('claude-file-input'); if (i) i.remove();
    return 'kicked ' + w.fileName;
  };
  // 2. exports end in <a download>.click(): take the blob and post it to the receiver popup instead
  top_.__saved = top_.__saved || [];
  if (!top_.__recvListener) {
    top_.addEventListener('message', ev => { if (ev.origin === 'http://127.0.0.1:8765') top_.__saved.push(ev.data); });
    top_.__recvListener = true;
  }
  top_.__send = (name, buf) => {
    if (top_.__recv && !top_.__recv.closed) top_.__recv.postMessage({name, buf}, 'http://127.0.0.1:8765');
    else top_.__pending = (top_.__pending || []).concat([{name, buf}]);
  };
  for (const win of [w, top_]) {
    if (win.__exportPatched) continue;
    const origA = win.HTMLAnchorElement.prototype.click;
    win.HTMLAnchorElement.prototype.click = function () {
      if (this.hasAttribute('download')) {
        const name = this.download || 'export.png';
        fetch(this.href).then(r => r.arrayBuffer()).then(buf => top_.__send(name, buf));
        return;
      }
      return origA.call(this);
    };
    win.__exportPatched = true;
  }
  if (!document.getElementById('lili-recv-btn')) {
    const btn = document.createElement('button');
    btn.id = 'lili-recv-btn'; btn.textContent = 'lili recv';
    btn.style.cssText = 'position:fixed;right:4px;bottom:4px;z-index:99999;font:11px sans-serif;padding:2px 6px;opacity:.8';
    btn.onclick = () => {
      top_.__recv = window.open('http://127.0.0.1:8765/recv.html', 'lili_recv', 'width=320,height=160');
      setTimeout(() => { for (const p of (top_.__pending || [])) top_.__recv.postMessage(p, 'http://127.0.0.1:8765'); top_.__pending = []; }, 1500);
    };
    document.body.appendChild(btn);
  }
  return 'hooks ready';
})();
