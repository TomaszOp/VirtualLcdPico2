#pragma once

// Strony uzytkownika: "/" (podglad), "/lcd" (ustawienia wyswietlacza) oraz "/wifi" (ustawienia sieci).
// Wspolny styl jest doklejany do obu stron (sasiednie literaly lacza sie w jeden tekst).

#define VLCD_HEAD(title) R"HTML(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#2563eb">
<title>)HTML" title R"HTML(</title>
<style>
:root { color-scheme: light; --bg:#eef1f7; --bg2:#e6eaf3; --card:rgba(255,255,255,.82); --solid:#fff; --fg:#0f172a;
        --muted:#64748b; --line:rgba(15,23,42,.09); --accent:#4f46e5; --accent2:#06b6d4; --soft:rgba(79,70,229,.10);
        --ok:#10b981; --bad:#ef4444; --warn:#f59e0b; --shadow:0 1px 2px rgba(15,23,42,.06), 0 8px 24px rgba(15,23,42,.07);
        --ring:0 0 0 3px rgba(79,70,229,.28); --r:18px;
        --stage:radial-gradient(circle at 50% 30%, #e7e9ee, #c8ccd5 75%); }
@media (prefers-color-scheme: dark) {
  :root { color-scheme: dark; --bg:#090b12; --bg2:#0f1220; --card:rgba(23,27,40,.78); --solid:#161a27; --fg:#e7eaf3;
          --muted:#8b93a7; --line:rgba(255,255,255,.08); --accent:#818cf8; --accent2:#22d3ee; --soft:rgba(129,140,248,.14);
          --ok:#34d399; --bad:#f87171; --warn:#fbbf24; --shadow:0 1px 2px rgba(0,0,0,.4), 0 10px 30px rgba(0,0,0,.35);
          --ring:0 0 0 3px rgba(129,140,248,.35);
          --stage:radial-gradient(circle at 50% 30%, #3a3f4b, #262a33 75%); }
}
* { box-sizing:border-box; }
html { min-height:100%; background:var(--bg); }
body { margin:0; min-height:100vh; font:15px/1.5 ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",sans-serif;
       color:var(--fg); -webkit-text-size-adjust:100%; -webkit-font-smoothing:antialiased;
       background:radial-gradient(1200px 600px at 10% -10%, var(--soft), transparent 60%),
                  radial-gradient(900px 500px at 110% 0%, rgba(6,182,212,.10), transparent 60%), var(--bg); }
.top { position:sticky; top:0; z-index:5; display:flex; align-items:center; gap:12px; padding:10px 18px;
       padding-top:max(10px, env(safe-area-inset-top)); background:var(--card); border-bottom:1px solid var(--line);
       -webkit-backdrop-filter:saturate(180%) blur(14px); backdrop-filter:saturate(180%) blur(14px); }
.brand { display:flex; align-items:center; gap:10px; font-weight:700; font-size:16px; letter-spacing:-.01em; margin-right:auto;
         white-space:nowrap; color:inherit; text-decoration:none; }
.logo { width:26px; height:20px; border-radius:6px; flex:none; position:relative;
        background:linear-gradient(135deg, var(--accent), var(--accent2)); box-shadow:0 4px 12px rgba(79,70,229,.35); }
.logo::after { content:""; position:absolute; inset:4px; border-radius:2px; background:rgba(255,255,255,.85); opacity:.9; }
nav { display:flex; gap:2px; padding:3px; border-radius:12px; background:var(--soft); }
nav a { color:var(--muted); text-decoration:none; padding:6px 12px; border-radius:9px; font-weight:600; font-size:14px;
        transition:color .15s, background .15s; }
nav a:hover { color:var(--fg); }
nav a.on { background:var(--solid); color:var(--accent); box-shadow:0 1px 3px rgba(15,23,42,.12); }
.wrap { max-width:1100px; margin:0 auto; padding:20px 16px 32px; }
.card { background:var(--card); border:1px solid var(--line); border-radius:var(--r); padding:20px; box-shadow:var(--shadow);
        -webkit-backdrop-filter:blur(10px); backdrop-filter:blur(10px); }
h2 { font-size:12px; font-weight:700; text-transform:uppercase; letter-spacing:.08em; color:var(--muted); margin:0 0 16px; }
.field { margin-bottom:20px; }
.lbl { display:block; font-weight:650; margin-bottom:8px; }
.hint { font-size:13px; color:var(--muted); margin-top:7px; }
.seg { display:flex; flex-wrap:wrap; gap:4px; padding:4px; background:var(--bg2); border:1px solid var(--line); border-radius:12px; }
.seg button { flex:1 1 0; min-width:54px; border:0; background:transparent; color:var(--muted); font:inherit; font-weight:600;
              padding:9px 10px; border-radius:9px; cursor:pointer; transition:color .15s, background .15s, box-shadow .15s; }
.seg button:hover { color:var(--fg); }
.seg button[aria-pressed=true] { background:var(--solid); color:var(--accent); box-shadow:0 1px 3px rgba(15,23,42,.14); }
.btn { font:inherit; font-weight:650; padding:11px 18px; border-radius:12px; border:1px solid var(--line);
       background:var(--solid); color:var(--fg); cursor:pointer; transition:transform .08s, box-shadow .15s, filter .15s; }
.btn:hover:not(:disabled) { box-shadow:var(--shadow); }
.btn:active:not(:disabled) { transform:translateY(1px); }
.btn.primary { background:linear-gradient(135deg, var(--accent), #7c3aed); border-color:transparent; color:#fff;
               box-shadow:0 6px 16px rgba(79,70,229,.35); }
.btn.primary:hover:not(:disabled) { filter:brightness(1.07); }
.btn.danger { color:var(--bad); }
.btn:disabled { opacity:.45; cursor:default; box-shadow:none; }
.actions { display:flex; flex-wrap:wrap; gap:10px; }
.actions .btn { flex:1 1 auto; }
.pill { display:inline-flex; align-items:center; gap:7px; padding:5px 12px; border-radius:999px; background:var(--card);
        border:1px solid var(--line); font-size:13px; font-weight:550; color:var(--muted); white-space:nowrap; }
.dot { width:8px; height:8px; border-radius:50%; background:var(--bad); flex:none; box-shadow:0 0 0 3px rgba(239,68,68,.18); }
.dot.ok { background:var(--ok); box-shadow:0 0 0 3px rgba(16,185,129,.2); }
.dot.warn { background:var(--warn); box-shadow:0 0 0 3px rgba(245,158,11,.2); }
.note { font-size:13px; color:var(--muted); margin:14px 0 0; min-height:1.3em; }
.note.ok { color:var(--ok); } .note.err { color:var(--bad); }
input[type=text], input[type=password] { width:100%; font:inherit; padding:11px 13px; border-radius:12px;
       border:1px solid var(--line); background:var(--bg2); color:var(--fg); transition:box-shadow .15s, border-color .15s; }
input:focus { outline:0; border-color:var(--accent); box-shadow:var(--ring); }
.check { display:flex; align-items:flex-start; gap:10px; font-weight:600; cursor:pointer; }
.check input { width:20px; height:20px; margin:1px 0 0; accent-color:var(--accent); flex:none; cursor:pointer; }
select.combo { width:100%; font:inherit; font-weight:600; padding:11px 40px 11px 13px; border-radius:12px; border:1px solid var(--line);
               color:var(--fg); cursor:pointer; -webkit-appearance:none; appearance:none;
               background:var(--bg2) url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='8'%3E%3Cpath d='M1 1l5 5 5-5' fill='none' stroke='%2364748b' stroke-width='2'/%3E%3C/svg%3E") no-repeat right 14px center; }
select.combo:disabled { cursor:default; opacity:.7; }
select.combo:focus { outline:0; border-color:var(--accent); box-shadow:var(--ring); }
.btn:focus-visible, .seg button:focus-visible, nav a:focus-visible, .brand:focus-visible { outline:0; box-shadow:var(--ring); }
@media (max-width:560px) {
  .wrap { padding:12px 10px 24px; } .card { padding:16px; border-radius:16px; }
  .top { padding-left:12px; padding-right:12px; gap:8px; } nav a { padding:6px 9px; }
}
@media (max-width:420px) { .brand .name { display:none; } nav a { padding:6px 7px; font-size:13px; } }
)HTML"

// back: "← " na podstronach - logo z nazwa prowadzi zawsze do podgladu.
#define VLCD_NAV(back, main, lcd, wifi, test) R"HTML(
<header class="top">
  <a class="brand" href="/">)HTML" back R"HTML(<span class="logo"></span><span class="name">Virtual LCD</span></a>
  <nav><a href="/")HTML" main R"HTML(>Podgląd</a><a href="/lcd")HTML" lcd R"HTML(>Wyświetlacz</a><a href="/wifi")HTML" wifi R"HTML(>WiFi</a><a href="/test")HTML" test R"HTML(>Test</a></nav>
</header>
)HTML"

#define VLCD_JS_COMMON R"HTML(
const $ = id => document.getElementById(id);
function seg(el, items, pick) {
  el.innerHTML = '';
  for (const [v, t] of items) {
    const b = document.createElement('button');
    b.type = 'button'; b.textContent = t; b.dataset.v = v;
    b.onclick = () => pick(v);
    el.append(b);
  }
}
function mark(el, v) { for (const b of el.children) b.setAttribute('aria-pressed', b.dataset.v === String(v)); }
function note(t, cls) { const n = $('msg'); n.textContent = t; n.className = 'note ' + (cls || ''); }
async function post(url, data) {
  const r = await fetch(url, { method: 'POST', body: new URLSearchParams(data || {}) });
  const d = await r.json().catch(() => ({}));
  if (!r.ok || !d.ok) throw new Error(d.error || ('HTTP ' + r.status));
  return d;
}
)HTML"

static const char kMainHtml[] = VLCD_HEAD("Virtual LCD") R"HTML(
.stage { display:flex; flex-direction:column; gap:14px; padding:14px; }
#view { display:grid; min-height:220px; padding:16px; overflow:auto; border-radius:14px;
        background:var(--stage); box-shadow:inset 0 0 0 1px var(--line); }
#screen { margin:auto; display:block; background:#000; image-rendering:crisp-edges; image-rendering:pixelated;
          border-radius:4px; box-shadow:0 0 0 6px #05060a, 0 0 0 7px rgba(255,255,255,.08), 0 18px 40px rgba(0,0,0,.55); }
#screen.inv { filter:invert(1); }
#screen.off { filter:brightness(0); }
#screen.round { border-radius:50%; }
.meta { display:flex; flex-wrap:wrap; gap:6px; justify-content:center; }
</style>
</head>
<body>
)HTML" VLCD_NAV("", " class=\"on\"", "", "", "") R"HTML(
<main class="wrap">
  <section class="card stage">
    <div id="view"><canvas id="screen" width="240" height="320"></canvas></div>
    <div class="meta">
      <span class="pill"><span id="dot" class="dot"></span><span id="conn">Łączenie…</span></span>
      <span class="pill" id="res">—</span>
      <span class="pill" id="disp">—</span>
    </div>
    <p class="note" id="msg"></p>
  </section>
</main>
<script>
)HTML" VLCD_JS_COMMON R"HTML(
const cv = $('screen'), ctx = cv.getContext('2d');
const src = document.createElement('canvas'), sctx = src.getContext('2d');
// Obrot i skalowanie podgladu - ustawiane na stronie Wyswietlacz.
const st = { rot: 0, scale: 0 };
let img = null, ws = null, need = false;

function layout() {
  if (!img) return;
  const side = st.rot % 180 !== 0;
  const w = side ? src.height : src.width, h = side ? src.width : src.height;
  if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
  let s = st.scale;
  if (!s) {
    const v = $('view'), top = v.getBoundingClientRect().top + scrollY;
    const aw = v.clientWidth - 24;
    const ah = Math.max(220, innerHeight - top - 110);
    s = Math.min(aw / w, ah / h);
    s = s >= 2 ? Math.floor(s) : Math.max(0.25, s);
  }
  cv.style.width = w * s + 'px';
  cv.style.height = h * s + 'px';
  need = true;
}
addEventListener('resize', () => { if (!st.scale) layout(); });

function draw() {
  if (need && img) {
    need = false;
    sctx.putImageData(img, 0, 0);
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.translate(cv.width / 2, cv.height / 2);
    ctx.rotate(st.rot * Math.PI / 180);
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
      const c = px[i], r5 = c >> 11, g6 = (c >> 5) & 63, b5 = c & 31;
      d[o] = (r5 << 3) | (r5 >> 2);
      d[o + 1] = (g6 << 2) | (g6 >> 4);
      d[o + 2] = (b5 << 3) | (b5 >> 2);
    }
  }
  need = true;
}

function onInfo(m) {
  if (!img || m.w !== src.width || m.h !== src.height) {
    src.width = m.w; src.height = m.h;
    img = sctx.createImageData(m.w, m.h);
    for (let i = 3; i < img.data.length; i += 4) img.data[i] = 255;
    layout();
  }
  // Inwersja, wylaczony ekran i podswietlenie (pin LED) jako jeden filtr.
  cv.classList.toggle('round', !!m.round);
  const bl = m.on ? (m.bl ?? 100) / 100 : 0;
  cv.style.filter = (m.inv ? 'invert(1) ' : '') + `brightness(${bl})`;
  $('res').textContent = `${m.driver} · ${m.w}×${m.h}`;
  $('disp').textContent = (m.on ? 'Ekran włączony' : 'Ekran wyłączony') + (m.inv ? ' · inwersja' : '') +
    (m.on && m.bl < 100 ? ` · podświetlenie ${m.bl}%` : '');
}

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => { $('dot').className = 'dot ok'; $('conn').textContent = 'Połączono'; ws.send('hello'); };
  ws.onclose = () => { $('dot').className = 'dot'; $('conn').textContent = 'Brak połączenia'; setTimeout(connect, 1500); };
  ws.onmessage = e => {
    if (typeof e.data !== 'string') return onRegion(e.data);
    try { const m = JSON.parse(e.data); if (m.t === 'info') onInfo(m); else if (m.t === 'bye') bye(); } catch (x) {}
  };
}
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

function loadSettings() {
  fetch('/api/settings', { cache: 'no-store' }).then(r => r.json()).then(d => {
    st.rot = d.rot; st.scale = d.scale; layout();
  }).catch(() => note('Nie udało się wczytać ustawień z urządzenia.', 'err'));
}
addEventListener('pageshow', e => { if (e.persisted) loadSettings(); });  // powrot ze strony Wyswietlacz
loadSettings();
connect();
</script>
</body>
</html>
)HTML";

static const char kLcdHtml[] = VLCD_HEAD("Virtual LCD – wyświetlacz") R"HTML(
.wrap { max-width:640px; }
.actions { margin-top:4px; }
</style>
</head>
<body>
)HTML" VLCD_NAV("← ", "", " class=\"on\"", "", "") R"HTML(
<main class="wrap">
  <section class="card">
    <h2>Wyświetlacz</h2>
    <div class="field"><label class="lbl" for="driver">Model LCD</label><select class="combo" id="driver"></select>
      <div class="hint">Emulowany sterownik – musi zgadzać się z biblioteką w programie mikrokontrolera.</div></div>
    <div class="field"><label class="lbl" for="res">Rozdzielczość</label><select class="combo" id="res"></select>
      <div class="hint">Widoczna część pamięci sterownika – taka jak ustawiona w bibliotece (szerokość×wysokość w pionie).</div></div>
    <div class="field"><label class="check"><input type="checkbox" id="ignoreLed">
      <span>Pomiń pin LED – podświetlenie zawsze 100%</span></label>
      <div class="hint" id="ledHint">Pin LED (podświetlenie) wyświetlacza podłącz do GP7. Stan stały i PWM są mierzone,
        a podgląd jest przyciemniany według wypełnienia.</div></div>
    <div class="field"><span class="lbl">Obrót</span><div class="seg" id="rot"></div>
      <div class="hint">Obrót i skalowanie dotyczą podglądu na stronie Podgląd.</div></div>
    <div class="field"><span class="lbl">Skalowanie</span><div class="seg" id="scale"></div></div>
    <div class="actions">
      <button class="btn" id="test" type="button">Obraz testowy</button>
      <button class="btn primary" id="save" type="button" disabled>Zapisz</button>
    </div>
    <p class="note" id="msg"></p>
  </section>
</main>
<script>
)HTML" VLCD_JS_COMMON R"HTML(
const st = { driver: '', res: '', rot: 0, scale: 0, ignoreLed: true };
let saved = null, drivers = [];

function resList(name) { const d = drivers.find(x => x.name === name); return d ? d.res : []; }
function pickDriver(v) {
  st.driver = v;
  const list = resList(v);
  if (!list.includes(st.res)) st.res = list[0] || '';
  const sel = $('res');
  sel.innerHTML = '';
  for (const r of list) sel.add(new Option(r.replace('x', ' × '), r));
  sel.disabled = list.length < 2;  // np. GC9A01 ma jedna rozdzielczosc
  sync();
}

seg($('rot'), [[0, '0°'], [90, '90°'], [180, '180°'], [270, '270°']], v => { st.rot = v; sync(); });
seg($('scale'), [[0, 'Dopasuj'], [1, '1×'], [2, '2×'], [3, '3×'], [4, '4×']], v => { st.scale = v; sync(); });

function sync() {
  $('driver').value = st.driver; $('res').value = st.res; mark($('rot'), st.rot); mark($('scale'), st.scale);
  $('ignoreLed').checked = st.ignoreLed;
  const changed = saved && (saved.driver !== st.driver || saved.res !== st.res || saved.rot !== st.rot ||
                            saved.scale !== st.scale || saved.ignoreLed !== st.ignoreLed);
  $('save').disabled = !changed;
  if (changed) note('Niezapisane zmiany – kliknij Zapisz.');
  else if ($('msg').textContent.startsWith('Niezapisane')) note('');
}

$('driver').onchange = e => pickDriver(e.target.value);
$('res').onchange = e => { st.res = e.target.value; sync(); };
$('ignoreLed').onchange = e => { st.ignoreLed = e.target.checked; sync(); };
$('test').onclick = async () => {
  try { await post('/api/test'); note('Obraz testowy wysłany – widać go na stronie Podgląd.', 'ok'); }
  catch (e) { note('Nie wysłano: ' + e.message, 'err'); }
};
$('save').onclick = async () => {
  $('save').disabled = true;
  try {
    await post('/api/settings', st);
    saved = { ...st };
    note('Zapisano. Model i rozdzielczość działają od razu, ustawienia zostaną też wczytane przy starcie.', 'ok');
  } catch (e) { note('Nie zapisano: ' + e.message, 'err'); }
  sync();
};

fetch('/api/settings', { cache: 'no-store' }).then(r => r.json()).then(d => {
  drivers = d.drivers;
  for (const x of drivers) $('driver').add(new Option(x.name, x.name));
  saved = { driver: d.driver, res: d.res, rot: d.rot, scale: d.scale, ignoreLed: d.ignoreLed };
  $('ledHint').textContent = `Pin LED (podświetlenie) wyświetlacza podłącz do GP${d.ledPin}. Teraz zmierzone: ${d.led}%. ` +
    'Stan stały i PWM są mierzone, a podgląd jest przyciemniany według wypełnienia.';
  Object.assign(st, saved);
  pickDriver(st.driver);
  if (d.current && (d.current !== d.driver || d.currentRes !== d.res))
    note(`Teraz działa ${d.current} ${d.currentRes} (wybrany w teście) – zapisany jest ${d.driver} ${d.res}.`);
}).catch(() => note('Nie udało się wczytać ustawień z urządzenia.', 'err'));
</script>
</body>
</html>
)HTML";

static const char kWifiHtml[] = VLCD_HEAD("Virtual LCD – WiFi") R"HTML(
.wrap { max-width:640px; }
.card + .card { margin-top:14px; }
dl { display:grid; grid-template-columns:auto 1fr; gap:8px 16px; margin:0; }
dt { color:var(--muted); } dd { margin:0; font-weight:550; overflow-wrap:anywhere; }
.row { display:flex; gap:8px; }
.row input { flex:1; min-width:0; }
.nets { display:flex; flex-direction:column; gap:4px; margin-top:8px; }
.nets button { display:flex; align-items:center; gap:10px; width:100%; text-align:left; font:inherit; color:var(--fg);
               background:var(--bg); border:1px solid var(--line); border-radius:9px; padding:9px 12px; cursor:pointer; }
.nets button:hover { border-color:var(--accent); }
.nets .n { flex:1; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; font-weight:550; }
.nets .s { font-size:12px; color:var(--muted); white-space:nowrap; }
.bars { display:inline-flex; align-items:flex-end; gap:2px; height:14px; }
.bars i { width:4px; background:var(--line); border-radius:1px; }
.bars i.on { background:var(--accent); }
.order { margin:0; padding-left:20px; color:var(--muted); font-size:14px; }
.order li { margin:3px 0; }
#done { display:none; }
#done.show { display:block; }
</style>
</head>
<body>
)HTML" VLCD_NAV("← ", "", "", " class=\"on\"", "") R"HTML(
<main class="wrap">
  <section class="card">
    <h2>Stan połączenia</h2>
    <dl id="now"><dt>Ładowanie…</dt><dd></dd></dl>
  </section>

  <section class="card" id="form">
    <h2>Ustawienia WiFi</h2>
    <div class="field"><span class="lbl">Tryb</span><div class="seg" id="mode"></div>
      <div class="hint" id="modeHint"></div></div>
    <div class="field">
      <label class="lbl" for="ssid" id="ssidLbl">Nazwa sieci (SSID)</label>
      <div class="row"><input type="text" id="ssid" maxlength="32" autocomplete="off" autocapitalize="none" spellcheck="false">
        <button class="btn" id="scan" type="button">Szukaj</button></div>
      <div class="nets" id="nets"></div>
    </div>
    <div class="field">
      <label class="lbl" for="pass">Hasło</label>
      <div class="row"><input type="password" id="pass" maxlength="63" autocomplete="new-password">
        <button class="btn" id="show" type="button">Pokaż</button></div>
      <div class="hint">Puste = sieć otwarta (bez hasła). W przeciwnym razie co najmniej 8 znaków.</div>
    </div>
    <div class="actions">
      <button class="btn primary" id="save" type="button">Zapisz i uruchom ponownie</button>
    </div>
    <p class="note" id="msg"></p>
  </section>

  <section class="card" id="done">
    <h2>Uruchamianie ponowne</h2>
    <p id="doneText"></p>
    <p class="note" id="wait">Czekam na urządzenie…</p>
  </section>

  <section class="card">
    <h2>Kolejność przy starcie</h2>
    <ol class="order">
      <li>Ustawienia zapisane na tej stronie (pamięć flash Pico).</li>
      <li>Plik <code>secrets.h</code> w firmware (WIFI_SSID, WIFI_PASSWORD, WIFI_AP).</li>
      <li>Otwarty punkt dostępu <b>VirtualLCD</b>, adres http://192.168.4.1/.</li>
    </ol>
    <p class="hint">Gdy sieć z danego kroku nie wystartuje (połączenie najwyżej 15 s), urządzenie przechodzi do
      następnego. Przy punkcie dostępu VirtualLCD nadal co 30 s próbuje połączyć się z siecią i wyłącza go po połączeniu.</p>
    <div class="actions" style="margin-top:14px">
      <button class="btn danger" id="clear" type="button">Usuń zapisane ustawienia</button>
      <button class="btn" id="reboot" type="button">Uruchom ponownie</button>
    </div>
  </section>
</main>
<script>
)HTML" VLCD_JS_COMMON R"HTML(
let mode = 'sta', info = null;
const SRC = { nvs: 'zapisane na stronie (flash)', secrets: 'plik secrets.h', default: 'domyślne (brak ustawień)' };

seg($('mode'), [['sta', 'Sieć WiFi (STA)'], ['ap', 'Punkt dostępu (AP)']], setMode);
function setMode(m) {
  mode = m; mark($('mode'), m);
  const sta = m === 'sta';
  $('ssidLbl').textContent = sta ? 'Nazwa sieci (SSID)' : 'Nazwa punktu dostępu';
  $('modeHint').textContent = sta ? 'Urządzenie dołączy do istniejącej sieci WiFi (adres nada router).'
                                  : 'Urządzenie utworzy własną sieć, adres strony: http://192.168.4.1/';
  $('scan').style.display = sta ? '' : 'none';
  if (!sta) $('nets').innerHTML = '';
}

function row(k, v) { return `<dt>${k}</dt><dd>${v}</dd>`; }
function esc(s) { return String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]); }
function showNow(d) {
  const n = d.now;
  let h = row('Źródło ustawień', SRC[n.source] || n.source);
  if (n.sta) h += row('Sieć', `${esc(n.staSsid)} – ` + (n.connected
      ? `<span style="color:var(--ok)">połączono</span>, ${n.ip}, ${n.rssi} dBm`
      : `<span style="color:var(--warn)">łączenie…</span>`));
  if (n.ap) h += row(n.sta ? 'Awaryjny punkt dostępu' : 'Punkt dostępu', `${esc(n.apSsid)}, ${n.apIp}`);
  $('now').innerHTML = h;
  $('clear').disabled = !d.saved.set;
}

async function load() {
  const d = await (await fetch('/api/wifi')).json();
  info = d;
  showNow(d);
  if (d.saved.set) { setMode(d.saved.ap ? 'ap' : 'sta'); $('ssid').value = d.saved.ssid; }
  else if (d.now.source === 'secrets') { setMode(d.now.sta ? 'sta' : 'ap'); $('ssid').value = d.now.sta ? d.now.staSsid : d.now.apSsid; }
  else setMode('sta');
  if (d.saved.set && d.saved.hasPass) $('pass').placeholder = 'Wpisz hasło ponownie';
}
load().catch(() => { $('now').innerHTML = row('Błąd', 'nie udało się odczytać stanu'); setMode('sta'); });
setInterval(() => fetch('/api/wifi').then(r => r.json()).then(showNow).catch(() => {}), 5000);

function bars(rssi) {
  const n = rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1;
  return '<span class="bars">' + [6, 9, 12, 14].map((h, i) => `<i style="height:${h}px" class="${i < n ? 'on' : ''}"></i>`).join('') + '</span>';
}
$('scan').onclick = async () => {
  const b = $('scan'); b.disabled = true; b.textContent = 'Szukam…';
  try {
    const list = await (await fetch('/api/scan')).json();
    if (!Array.isArray(list)) throw new Error(list.error || 'błąd');
    $('nets').innerHTML = '';
    if (!list.length) $('nets').innerHTML = '<div class="hint">Nie znaleziono sieci.</div>';
    for (const n of list) {
      const e = document.createElement('button');
      e.type = 'button';
      e.innerHTML = `${bars(n.rssi)}<span class="n">${esc(n.ssid)}</span><span class="s">${n.open ? 'otwarta' : 'zabezpieczona'}</span>`;
      e.onclick = () => { $('ssid').value = n.ssid; $('nets').innerHTML = ''; $('pass').value = ''; if (!n.open) $('pass').focus(); };
      $('nets').append(e);
    }
  } catch (e) { $('nets').innerHTML = '<div class="hint">Nie udało się wyszukać sieci (spróbuj ponownie za chwilę).</div>'; }
  b.disabled = false; b.textContent = 'Szukaj';
};
$('show').onclick = () => {
  const p = $('pass'), hidden = p.type === 'password';
  p.type = hidden ? 'text' : 'password'; $('show').textContent = hidden ? 'Ukryj' : 'Pokaż';
};

function waitForDevice(text) {
  $('form').style.display = 'none';
  $('done').classList.add('show');
  $('doneText').innerHTML = text;
  let tries = 0;
  setTimeout(function poll() {
    fetch('/api/wifi', { cache: 'no-store' }).then(r => { if (r.ok) location.reload(); else throw 0; })
      .catch(() => { if (++tries < 30) setTimeout(poll, 2000); else $('wait').textContent = 'Urządzenie nie odpowiada pod tym adresem – połącz się z nim według opisu powyżej.'; });
  }, 5000);
}
function afterText(ap, ssid) {
  return ap ? `Połącz telefon lub komputer z siecią <b>${esc(ssid)}</b> i otwórz <b>http://192.168.4.1/</b>.`
            : `Urządzenie połączy się z siecią <b>${esc(ssid)}</b>. Adres nada router (widać go w konsoli szeregowej
               i na liście urządzeń routera). Jeśli połączenie się nie uda, po 15–30 s pojawi się sieć <b>VirtualLCD</b>
               z adresem http://192.168.4.1/.`;
}

$('save').onclick = async () => {
  const ssid = $('ssid').value, pass = $('pass').value;
  const bytes = new TextEncoder().encode(ssid).length;
  if (!bytes || bytes > 32) return note('Nazwa sieci musi mieć od 1 do 32 znaków.', 'err');
  if (pass && (pass.length < 8 || pass.length > 63)) return note('Hasło musi mieć od 8 do 63 znaków albo być puste.', 'err');
  if (!pass && !confirm(mode === 'ap' ? 'Punkt dostępu będzie otwarty (bez hasła). Kontynuować?'
                                      : 'Bez hasła – łączenie z siecią otwartą. Kontynuować?')) return;
  $('save').disabled = true;
  try {
    await post('/api/wifi', { mode, ssid, pass });
    waitForDevice('Ustawienia zapisane. ' + afterText(mode === 'ap', ssid));
  } catch (e) { note('Nie zapisano: ' + e.message, 'err'); $('save').disabled = false; }
};
$('clear').onclick = async () => {
  if (!confirm('Usunąć ustawienia WiFi zapisane na stronie? Po restarcie zostaną użyte dane z secrets.h albo punkt dostępu VirtualLCD.')) return;
  try {
    await post('/api/wifi/clear');
    waitForDevice('Zapisane ustawienia usunięte. Po restarcie urządzenie użyje danych z secrets.h, a bez nich utworzy sieć <b>VirtualLCD</b> (http://192.168.4.1/).');
  } catch (e) { note('Nie usunięto: ' + e.message, 'err'); }
};
$('reboot').onclick = async () => {
  if (!confirm('Uruchomić urządzenie ponownie?')) return;
  try { await post('/api/reboot'); waitForDevice('Urządzenie uruchamia się ponownie z obecnymi ustawieniami.'); }
  catch (e) { note('Błąd: ' + e.message, 'err'); }
};
</script>
</body>
</html>
)HTML";

// Makra VLCD_HEAD / VLCD_NAV / VLCD_JS_COMMON uzywa tez TestHtml.h (tam sa #undef).
