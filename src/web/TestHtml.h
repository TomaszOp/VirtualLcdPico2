#pragma once

// Strona testowa (/test): podglad, test petli zwrotnej, demo i diagnostyka.
// Wspolny naglowek i styl z Pages.h.
#include "Pages.h"

static const char kTestHtml[] = VLCD_HEAD("Virtual LCD – test") R"HTML(
  label { color:var(--muted); font-size:13px; font-weight:600; }
  select { font:inherit; font-weight:550; padding:7px 10px; border-radius:10px; border:1px solid var(--line);
           background:var(--bg2); color:var(--fg); }
  select:focus { outline:0; border-color:var(--accent); box-shadow:var(--ring); }
  .tw { max-width:760px; margin:0 auto; padding:16px 16px 0; display:grid; gap:14px; }
  .tools { display:flex; flex-wrap:wrap; gap:10px 16px; align-items:center; padding:12px 16px; }
  .tools .btn, #loop .btn { padding:8px 14px; }
  #stage { display:grid; padding:16px; overflow:auto; border-radius:14px;
           background:var(--stage); box-shadow:inset 0 0 0 1px var(--line); }
  #screen { margin:auto; image-rendering:pixelated; background:#000; border-radius:4px;
            box-shadow:0 0 0 6px #05060a, 0 0 0 7px rgba(255,255,255,.08), 0 18px 40px rgba(0,0,0,.55); }
  #screen.inv { filter:invert(1); }
  #screen.off { filter:brightness(0); }
  #loop .row { display:flex; flex-wrap:wrap; gap:8px 10px; align-items:center; margin:0 0 12px; }
  #loop .row .lbl { margin:0 6px 0 0; min-width:150px; }
  #loop small { color:var(--muted); }
  #loop pre { background:#0b0d16; color:#c7d2fe; border-radius:12px; padding:12px 14px; white-space:pre-wrap;
              font:12px/1.55 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace; min-height:3em; margin:8px 0 0;
              border:1px solid rgba(255,255,255,.06); }
  footer { padding:14px 16px 24px; color:var(--muted); font-size:12px; text-align:center; }
</style>
</head>
<body>
)HTML" VLCD_NAV("← ", "", "", "", " class=\"on\"") R"HTML(
<div class="tw">
<div class="card tools">
  <span class="pill"><span id="dot" class="dot"></span><span id="conn">rozłączony</span></span>
  <label>Emulowany LCD <select id="driver"></select></label>
  <label>Skala <select id="scale"><option>1</option><option selected>2</option><option>3</option><option>4</option></select></label>
  <label>Obrót <select id="rot"><option value="0">0°</option><option value="90">90°</option><option value="180">180°</option><option value="270">270°</option></select></label>
  <button class="btn" id="test">Obraz testowy</button>
</div>
<main class="card" style="padding:12px"><div id="stage"><canvas id="screen" width="240" height="320"></canvas></div></main>
<section class="card" id="loop">
  <h2>Testy przez SPI</h2>
  <div class="row">
    <span class="lbl">Wyświetlacz i zegar</span>
    <select id="demoDrv"></select>
    <select id="demoClk"><option value="10">10 MHz</option><option value="20" selected>20 MHz</option><option value="40">40 MHz</option></select>
  </div>
  <div class="row">
    <span class="lbl">Demo</span>
    <button class="btn primary" id="demoStart" data-test>Start demo</button>
  </div>
  <div class="row">
    <span class="lbl">Test pętli zwrotnej</span>
    <button class="btn" id="loopOne" data-test>Wybrany zegar</button>
    <button class="btn" id="loopClocks" data-test>10, 20 i 40 MHz</button>
    <button class="btn" id="loopAll" data-test>Wszystkie modele</button>
  </div>
  <div class="row"><small>„Wszystkie modele” – każdy sterownik w pierwszej rozdzielczości, przy 10, 20 i 40 MHz.
    Testy wymagają przewodów GP10→GP4 (SCK), GP11→GP2 (MOSI), GP12→GP3 (D/C), GP13→GP5 (CS), GP14→GP6 (RST).</small></div>
  <pre id="looplog">—</pre>
</section>
</div>
<footer id="stats"></footer>
<script>
const $ = id => document.getElementById(id);
const screen = $('screen'), ctx = screen.getContext('2d');
const src = document.createElement('canvas'), sctx = src.getContext('2d');
let img = null, ws = null, info = {}, needDraw = false;

function load(k, d) { try { return localStorage.getItem(k) || d; } catch (e) { return d; } }
function save(k, v) { try { localStorage.setItem(k, v); } catch (e) {} }
$('scale').value = load('scale', '2');
$('rot').value = load('rot', '0');

function resize(w, h) {
  src.width = w; src.height = h;
  img = sctx.createImageData(w, h);
  for (let i = 3; i < img.data.length; i += 4) img.data[i] = 255;
  layout();
}

function layout() {
  if (!img) return;
  const s = +$('scale').value, r = +$('rot').value;
  const w = src.width, h = src.height, side = r % 180 !== 0;
  screen.width = (side ? h : w) * s;
  screen.height = (side ? w : h) * s;
  needDraw = true;
}

function draw() {
  if (needDraw && img) {
    needDraw = false;
    sctx.putImageData(img, 0, 0);
    const s = +$('scale').value, r = +$('rot').value;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.imageSmoothingEnabled = false;
    ctx.translate(screen.width / 2, screen.height / 2);
    ctx.rotate(r * Math.PI / 180);
    ctx.scale(s, s);
    ctx.drawImage(src, -src.width / 2, -src.height / 2);
  }
  requestAnimationFrame(draw);
}
requestAnimationFrame(draw);

function onRegion(buf) {
  if (!img) return;
  let [x, y, w, h] = new Uint16Array(buf, 0, 4);
  let px;
  if (x & 0x4000) {  // RLE: 0x8000|(n-1), kolor = powtorzenie; (n-1), n pikseli = kopia
    x &= 0x3FFF;
    const src = new Uint16Array(buf, 8);
    px = new Uint16Array(w * h);
    for (let i = 0, j = 0; j < src.length && i < px.length;) {
      const t = src[j++];
      if (t & 0x8000) { const n = (t & 0x7FFF) + 1; px.fill(src[j++], i, i + n); i += n; }
      else { const n = t + 1; px.set(src.subarray(j, j + n), i); i += n; j += n; }
    }
  } else px = new Uint16Array(buf, 8, w * h);

  const d = img.data, W = img.width;
  let i = 0;
  for (let row = 0; row < h; row++) {
    let o = ((y + row) * W + x) * 4;
    for (let col = 0; col < w; col++, i++, o += 4) {
      const c = px[i];
      const r5 = c >> 11, g6 = (c >> 5) & 63, b5 = c & 31;
      d[o] = (r5 << 3) | (r5 >> 2);
      d[o + 1] = (g6 << 2) | (g6 >> 4);
      d[o + 2] = (b5 << 3) | (b5 >> 2);
    }
  }
  needDraw = true;
}

// Lista "model rozdzielczosc" -> wartosc "MODEL/ROZDZIELCZOSC" (dla wyboru emulatora i testow).
// Serwer wysyla ja raz, po "hello".
function onDrivers(m) {
  const opts = m.drivers.flatMap(d => d.res.map(r => [d.name + '/' + r, d.name + ' ' + r]));
  for (const id of ['driver', 'demoDrv']) {
    const sel = $(id);
    const keep = sel.value || load(id, '');
    sel.innerHTML = '';
    for (const [v, t] of opts) sel.add(new Option(t, v));
    if (keep && opts.some(o => o[0] === keep)) sel.value = keep;
  }
  if (info.driver) $('driver').value = info.driver + '/' + info.res;
}

function onInfo(m) {
  if (!img || m.w !== src.width || m.h !== src.height) resize(m.w, m.h);
  $('driver').value = m.driver + '/' + m.res;
  screen.style.borderRadius = m.round ? '50%' : '';
  const bl = m.on ? (m.bl ?? 100) / 100 : 0;
  screen.style.filter = (m.inv ? 'invert(1) ' : '') + `brightness(${bl})`;
  $('stats').textContent = `${m.driver} ${m.res} · ekran ${m.on ? 'wl.' : 'wyl.'}` +
    ` · komendy ${m.cmds} · bajty SPI ${m.bytes}` +
    (m.bl < 100 ? ` · LED ${m.bl}%` : '') + (m.ovr ? ` · PRZEPELNIENIA ${m.ovr}` : '') + (m.resync ? ` · urwane bajty ${m.resync}` : '') + ` · resety ${m.resets}`;
  info = m;
}

function onTest(m) {
  $('looplog').textContent = (m.run ? 'Test trwa...\n' : '') + (m.log || (m.en ? '—' : 'Test wyłączony w firmware'));
  for (const b of document.querySelectorAll('[data-test]')) b.disabled = m.run || !m.en;
}

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => { $('dot').className = 'dot ok'; $('conn').textContent = 'Połączono'; ws.send('hello'); };
  ws.onclose = () => { $('dot').className = 'dot'; $('conn').textContent = 'Brak połączenia'; setTimeout(connect, 1500); };
  ws.onmessage = e => {
    if (typeof e.data === 'string') {
      try {
        const m = JSON.parse(e.data);
        if (m.t === 'info') onInfo(m);
        else if (m.t === 'drivers') onDrivers(m);
        else if (m.t === 'test') onTest(m);
        else if (m.t === 'bye') bye();
      } catch (x) {}
    }
    else onRegion(e.data);
  };
}
connect();

function send(m) { if (ws && ws.readyState === 1) ws.send(m); }
// Serwer zamknal polaczenie, bo ta przegladarka otworzyla strone w innej karcie - bez automatycznego powrotu.
function bye() {
  if (!ws) return;
  ws.onclose = null; ws.close(); ws = null;
  $('dot').className = 'dot'; $('conn').textContent = 'Otwarte w innej karcie – kliknij, aby połączyć';
  $('conn').style.cursor = 'pointer';
  $('conn').onclick = () => { $('conn').onclick = null; $('conn').style.cursor = ''; if (!ws) connect(); };
}
setInterval(() => send('ping'), 2000);  // znak zycia dla serwera
// Wyjscie ze strony (takze do pamieci podrecznej przegladarki): zamknij polaczenie jawnie, po powrocie polacz ponownie.
addEventListener('pagehide', () => { if (ws) { ws.onclose = null; ws.close(); ws = null; } });
addEventListener('pageshow', e => { if (e.persisted && !ws) connect(); });
$('test').onclick = () => send('test');
$('demoStart').onclick = () => send('demo:' + $('demoDrv').value + '@' + $('demoClk').value);
$('loopOne').onclick = () => send('loop:' + $('demoDrv').value + '@' + $('demoClk').value);
$('loopClocks').onclick = () => send('loop:' + $('demoDrv').value);
$('loopAll').onclick = () => send('loop:all');
$('demoClk').value = load('demoClk', '20');
$('demoDrv').onchange = e => save('demoDrv', e.target.value);
$('demoClk').onchange = e => save('demoClk', e.target.value);
$('driver').onchange = e => send('driver:' + e.target.value);
$('scale').onchange = e => { save('scale', e.target.value); layout(); };
$('rot').onchange = e => { save('rot', e.target.value); layout(); };
</script>
</body>
</html>
)HTML";

#undef VLCD_HEAD
#undef VLCD_NAV
#undef VLCD_JS_COMMON
