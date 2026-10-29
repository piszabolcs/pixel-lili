// Pixel Lili: the ElevenLabs sound-effects page without a Save dialog (2026-10-29; tools/pixellab_editor_hooks.js
// does the same for PixelLab). Run it in the page (Claude in Chrome's javascript_tool), with tools/sound_recv.py
// listening on 127.0.0.1:8766:
//   1. a download the page starts (an <a download> clicked, or dispatched) is stopped before the browser sees it;
//      its blob (kept from URL.createObjectURL) or its URL (fetched with the page's cookies) goes to window.__dl
//   2. __send(i, name) posts download i to the receiver popup as `name` (assets/sounds/elevenlabs/<name>)
//   3. the "lili recv" button (top left) opens the popup: click it once for real, a popup needs a user's click
(() => {
    const w = window;
    if (w.__liliHooks) return 'hooks already in';
    w.__liliHooks = true; w.__objs = {}; w.__dl = []; w.__saved = []; w.__pending = [];
    const mk = URL.createObjectURL.bind(URL);
    URL.createObjectURL = (o) => { const u = mk(o); if (o instanceof Blob) w.__objs[u] = o; return u; };
    const grab = (a) => {
        const href = a.href || '';
        if (!(a.download || href.startsWith('blob:') || /\.(mp3|wav|ogg|opus)(\?|$)/i.test(href))) return false;
        w.__dl.push({ href: href.slice(0, 64), name: a.download || '', blob: w.__objs[href] || null, url: w.__objs[href] ? null : href });
        return true;
    };
    const click = HTMLAnchorElement.prototype.click;
    HTMLAnchorElement.prototype.click = function () { if (!grab(this)) return click.call(this); };
    const disp = HTMLAnchorElement.prototype.dispatchEvent;
    HTMLAnchorElement.prototype.dispatchEvent = function (e) { if (e.type === 'click' && grab(this)) return false; return disp.call(this, e); };
    document.addEventListener('click', (e) => { const a = e.target.closest && e.target.closest('a'); if (a && grab(a)) e.preventDefault(); }, true);
    w.addEventListener('message', (ev) => { if (ev.origin === 'http://127.0.0.1:8766') w.__saved.push(ev.data); });
    w.__send = async (i, name) => {
        const d = w.__dl[i]; if (!d) return 'no download ' + i;
        const buf = d.blob ? await d.blob.arrayBuffer() : await (await fetch(d.url, { credentials: 'include' })).arrayBuffer();
        if (w.__recv && !w.__recv.closed) w.__recv.postMessage({ name, buf }, 'http://127.0.0.1:8766');
        else w.__pending.push({ name, buf });
        return name + ' ' + buf.byteLength + ' B';
    };
    const btn = document.createElement('button');
    btn.id = 'lili-recv-btn'; btn.textContent = 'lili recv';
    btn.style.cssText = 'position:fixed;left:4px;top:4px;z-index:2147483647;padding:4px 8px;background:#c33;color:#fff;border:0;border-radius:4px';
    btn.addEventListener('click', () => {
        w.__recv = w.open('http://127.0.0.1:8766/recv.html', 'lili_recv_snd', 'width=320,height=160');
        setTimeout(() => { for (const p of w.__pending) w.__recv.postMessage(p, 'http://127.0.0.1:8766'); w.__pending = []; }, 1500);
    });
    document.body.appendChild(btn);
    return 'hooks in';
})();
