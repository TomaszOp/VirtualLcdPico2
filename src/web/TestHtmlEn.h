#pragma once

// Angielska wersja strony testowej z TestHtml.h (ta sama logika, inne tylko teksty).
// Dolaczany z TestHtml.h, korzysta z makr Pages.h. Log testu przychodzi z firmware i jest po polsku.

static const char kTestHtmlEn[] = VLCD_HEAD("en", "Virtual LCD – test") VLCD_CSS_TEST R"HTML(
</style>
</head>
<body>
)HTML" VLCD_NAV_EN("← ", "", "", "", " class=\"on\"") R"HTML(
<div class="tw">
<div class="card tools">
  <span class="pill"><span id="dot" class="dot"></span><span id="conn">disconnected</span></span>
  <label>Emulated LCD <select id="driver"></select></label>
  <label>Scale <select id="scale"><option>1</option><option selected>2</option><option>3</option><option>4</option></select></label>
  <label>Rotation <select id="rot"><option value="0">0°</option><option value="90">90°</option><option value="180">180°</option><option value="270">270°</option></select></label>
  <button class="btn" id="test">Test pattern</button>
</div>
<main class="card" style="padding:12px"><div id="stage"><canvas id="screen" width="240" height="320"></canvas></div></main>
<section class="card" id="loop">
  <h2>SPI tests</h2>
  <div class="row">
    <span class="lbl">Display and clock</span>
    <select id="demoDrv"></select>
    <select id="demoClk"><option value="10">10 MHz</option><option value="20" selected>20 MHz</option><option value="40">40 MHz</option></select>
  </div>
  <div class="row">
    <span class="lbl">Demo</span>
    <button class="btn primary" id="demoStart" data-test>Start demo</button>
  </div>
  <div class="row">
    <span class="lbl">Loopback test</span>
    <button class="btn" id="loopOne" data-test>Selected clock</button>
    <button class="btn" id="loopClocks" data-test>10, 20 and 40 MHz</button>
    <button class="btn" id="loopAll" data-test>All models</button>
  </div>
  <div class="row"><small>“All models” – every controller in its first resolution, at 10, 20 and 40 MHz.
    The tests need wires GP10→GP4 (SCK), GP11→GP2 (MOSI), GP12→GP3 (D/C), GP13→GP5 (CS), GP14→GP6 (RST).</small></div>
  <pre id="looplog">—</pre>
</section>
</div>
<footer id="stats"></footer>
<script>
)HTML" VLCD_JS_TEST_VIEW R"HTML(
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
  $('stats').textContent = `${m.driver} ${m.res} · display ${m.on ? 'on' : 'off'}` +
    ` · commands ${m.cmds} · SPI bytes ${m.bytes}` +
    (m.bl < 100 ? ` · LED ${m.bl}%` : '') + (m.ovr ? ` · OVERRUNS ${m.ovr}` : '') + (m.resync ? ` · broken bytes ${m.resync}` : '') + ` · resets ${m.resets}`;
  info = m;
}

function onTest(m) {
  $('looplog').textContent = (m.run ? 'Test running...\n' : '') + (m.log || (m.en ? '—' : 'Test disabled in firmware'));
  for (const b of document.querySelectorAll('[data-test]')) b.disabled = m.run || !m.en;
}

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => { $('dot').className = 'dot ok'; $('conn').textContent = 'Connected'; ws.send('hello'); };
  ws.onclose = () => { $('dot').className = 'dot'; $('conn').textContent = 'Not connected'; setTimeout(connect, 1500); };
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
  $('dot').className = 'dot'; $('conn').textContent = 'Open in another tab – click to connect';
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
