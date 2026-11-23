#pragma once

// Angielskie wersje stron z Pages.h (te same identyfikatory elementow i ta sama logika - inne tylko teksty).
// Dolaczany na koncu Pages.h, korzysta z jego makr.

static const char kMainHtmlEn[] = VLCD_HEAD("en", "Virtual LCD") VLCD_CSS_MAIN R"HTML(
</style>
</head>
<body>
)HTML" VLCD_NAV_EN("", " class=\"on\"", "", "", "") R"HTML(
<main class="wrap">
  <section class="card stage">
    <div id="view"><canvas id="screen" width="240" height="320"></canvas></div>
    <div class="meta">
      <span class="pill"><span id="dot" class="dot"></span><span id="conn">Connecting…</span></span>
      <span class="pill" id="res">—</span>
      <span class="pill" id="disp">—</span>
    </div>
    <p class="note" id="msg"></p>
  </section>
</main>
<script>
)HTML" VLCD_JS_COMMON VLCD_JS_MAIN_VIEW R"HTML(
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
  $('disp').textContent = (m.on ? 'Display on' : 'Display off') + (m.inv ? ' · inverted' : '') +
    (m.on && m.bl < 100 ? ` · backlight ${m.bl}%` : '');
}

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => { $('dot').className = 'dot ok'; $('conn').textContent = 'Connected'; ws.send('hello'); };
  ws.onclose = () => { $('dot').className = 'dot'; $('conn').textContent = 'Not connected'; setTimeout(connect, 1500); };
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
  $('dot').className = 'dot'; $('conn').textContent = 'Open in another tab – click to connect';
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
  }).catch(() => note('Could not load the settings from the device.', 'err'));
}
addEventListener('pageshow', e => { if (e.persisted) loadSettings(); });  // powrot ze strony Wyswietlacz
loadSettings();
connect();
</script>
</body>
</html>
)HTML";

static const char kLcdHtmlEn[] = VLCD_HEAD("en", "Virtual LCD – display") VLCD_CSS_LCD R"HTML(
</style>
</head>
<body>
)HTML" VLCD_NAV_EN("← ", "", " class=\"on\"", "", "") R"HTML(
<main class="wrap">
  <section class="card">
    <h2>Display</h2>
    <div class="field"><label class="lbl" for="driver">LCD model</label><select class="combo" id="driver"></select>
      <div class="hint">Emulated controller – must match the library used by the microcontroller program.</div></div>
    <div class="field"><label class="lbl" for="res">Resolution</label><select class="combo" id="res"></select>
      <div class="hint">Visible part of the controller memory – as set in the library (width×height in portrait).</div></div>
    <div class="field"><label class="check"><input type="checkbox" id="ignoreLed">
      <span>Ignore the LED pin – backlight always 100%</span></label>
      <div class="hint" id="ledHint">Connect the display LED (backlight) pin to GP7. Both a steady level and PWM are measured,
        and the preview is dimmed according to the duty cycle.</div></div>
    <div class="field"><span class="lbl">Rotation</span><div class="seg" id="rot"></div>
      <div class="hint">Rotation and scaling apply to the preview on the Preview page.</div></div>
    <div class="field"><span class="lbl">Scaling</span><div class="seg" id="scale"></div></div>
    <div class="actions">
      <button class="btn" id="test" type="button">Test pattern</button>
      <button class="btn primary" id="save" type="button" disabled>Save</button>
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
seg($('scale'), [[0, 'Fit'], [1, '1×'], [2, '2×'], [3, '3×'], [4, '4×']], v => { st.scale = v; sync(); });

function sync() {
  $('driver').value = st.driver; $('res').value = st.res; mark($('rot'), st.rot); mark($('scale'), st.scale);
  $('ignoreLed').checked = st.ignoreLed;
  const changed = saved && (saved.driver !== st.driver || saved.res !== st.res || saved.rot !== st.rot ||
                            saved.scale !== st.scale || saved.ignoreLed !== st.ignoreLed);
  $('save').disabled = !changed;
  if (changed) note('Unsaved changes – click Save.');
  else if ($('msg').textContent.startsWith('Unsaved')) note('');
}

$('driver').onchange = e => pickDriver(e.target.value);
$('res').onchange = e => { st.res = e.target.value; sync(); };
$('ignoreLed').onchange = e => { st.ignoreLed = e.target.checked; sync(); };
$('test').onclick = async () => {
  try { await post('/api/test'); note('Test pattern sent – you can see it on the Preview page.', 'ok'); }
  catch (e) { note('Not sent: ' + e.message, 'err'); }
};
$('save').onclick = async () => {
  $('save').disabled = true;
  try {
    await post('/api/settings', st);
    saved = { ...st };
    note('Saved. The model and resolution apply immediately, and the settings will also be loaded at startup.', 'ok');
  } catch (e) { note('Not saved: ' + e.message, 'err'); }
  sync();
};

fetch('/api/settings', { cache: 'no-store' }).then(r => r.json()).then(d => {
  drivers = d.drivers;
  for (const x of drivers) $('driver').add(new Option(x.name, x.name));
  saved = { driver: d.driver, res: d.res, rot: d.rot, scale: d.scale, ignoreLed: d.ignoreLed };
  $('ledHint').textContent = `Connect the display LED (backlight) pin to GP${d.ledPin}. Measured now: ${d.led}%. ` +
    'Both a steady level and PWM are measured, and the preview is dimmed according to the duty cycle.';
  Object.assign(st, saved);
  pickDriver(st.driver);
  if (d.current && (d.current !== d.driver || d.currentRes !== d.res))
    note(`Now running ${d.current} ${d.currentRes} (selected in the test) – saved is ${d.driver} ${d.res}.`);
}).catch(() => note('Could not load the settings from the device.', 'err'));
</script>
</body>
</html>
)HTML";

static const char kWifiHtmlEn[] = VLCD_HEAD("en", "Virtual LCD – WiFi") VLCD_CSS_WIFI R"HTML(
</style>
</head>
<body>
)HTML" VLCD_NAV_EN("← ", "", "", " class=\"on\"", "") R"HTML(
<main class="wrap">
  <section class="card">
    <h2>Connection status</h2>
    <dl id="now"><dt>Loading…</dt><dd></dd></dl>
  </section>

  <section class="card" id="form">
    <h2>WiFi settings</h2>
    <div class="field"><span class="lbl">Mode</span><div class="seg" id="mode"></div>
      <div class="hint" id="modeHint"></div></div>
    <div class="field">
      <label class="lbl" for="ssid" id="ssidLbl">Network name (SSID)</label>
      <div class="row"><input type="text" id="ssid" maxlength="32" autocomplete="off" autocapitalize="none" spellcheck="false">
        <button class="btn" id="scan" type="button">Scan</button></div>
      <div class="nets" id="nets"></div>
    </div>
    <div class="field">
      <label class="lbl" for="pass">Password</label>
      <div class="row"><input type="password" id="pass" maxlength="63" autocomplete="new-password">
        <button class="btn" id="show" type="button">Show</button></div>
      <div class="hint">Empty = open network (no password). Otherwise at least 8 characters.</div>
    </div>
    <div class="actions">
      <button class="btn primary" id="save" type="button">Save and restart</button>
    </div>
    <p class="note" id="msg"></p>
  </section>

  <section class="card" id="done">
    <h2>Restarting</h2>
    <p id="doneText"></p>
    <p class="note" id="wait">Waiting for the device…</p>
  </section>

  <section class="card">
    <h2>Startup order</h2>
    <ol class="order">
      <li>Settings saved on this page (Pico flash memory).</li>
      <li>The <code>secrets.h</code> file in the firmware (WIFI_SSID, WIFI_PASSWORD, WIFI_AP).</li>
      <li>Open access point <b>VirtualLCD</b>, address http://192.168.4.1/.</li>
    </ol>
    <p class="hint">If the network from a step does not start (connecting takes at most 15 s), the device moves on to the
      next one. While running the VirtualLCD access point it still tries to join the network every 30 s and turns the access point off once connected.</p>
    <div class="actions" style="margin-top:14px">
      <button class="btn danger" id="clear" type="button">Delete saved settings</button>
      <button class="btn" id="reboot" type="button">Restart</button>
    </div>
  </section>
</main>
<script>
)HTML" VLCD_JS_COMMON R"HTML(
let mode = 'sta', info = null;
const SRC = { nvs: 'saved on the page (flash)', secrets: 'secrets.h file', default: 'default (no settings)' };

seg($('mode'), [['sta', 'WiFi network (STA)'], ['ap', 'Access point (AP)']], setMode);
function setMode(m) {
  mode = m; mark($('mode'), m);
  const sta = m === 'sta';
  $('ssidLbl').textContent = sta ? 'Network name (SSID)' : 'Access point name';
  $('modeHint').textContent = sta ? 'The device will join an existing WiFi network (the router assigns the address).'
                                  : 'The device will create its own network, page address: http://192.168.4.1/';
  $('scan').style.display = sta ? '' : 'none';
  if (!sta) $('nets').innerHTML = '';
}

function row(k, v) { return `<dt>${k}</dt><dd>${v}</dd>`; }
function esc(s) { return String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]); }
function showNow(d) {
  const n = d.now;
  let h = row('Settings source', SRC[n.source] || n.source);
  if (n.sta) h += row('Network', `${esc(n.staSsid)} – ` + (n.connected
      ? `<span style="color:var(--ok)">connected</span>, ${n.ip}, ${n.rssi} dBm`
      : `<span style="color:var(--warn)">connecting…</span>`));
  if (n.ap) h += row(n.sta ? 'Fallback access point' : 'Access point', `${esc(n.apSsid)}, ${n.apIp}`);
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
  if (d.saved.set && d.saved.hasPass) $('pass').placeholder = 'Enter the password again';
}
load().catch(() => { $('now').innerHTML = row('Error', 'could not read the status'); setMode('sta'); });
setInterval(() => fetch('/api/wifi').then(r => r.json()).then(showNow).catch(() => {}), 5000);

function bars(rssi) {
  const n = rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1;
  return '<span class="bars">' + [6, 9, 12, 14].map((h, i) => `<i style="height:${h}px" class="${i < n ? 'on' : ''}"></i>`).join('') + '</span>';
}
$('scan').onclick = async () => {
  const b = $('scan'); b.disabled = true; b.textContent = 'Scanning…';
  try {
    const list = await (await fetch('/api/scan')).json();
    if (!Array.isArray(list)) throw new Error(list.error || 'error');
    $('nets').innerHTML = '';
    if (!list.length) $('nets').innerHTML = '<div class="hint">No networks found.</div>';
    for (const n of list) {
      const e = document.createElement('button');
      e.type = 'button';
      e.innerHTML = `${bars(n.rssi)}<span class="n">${esc(n.ssid)}</span><span class="s">${n.open ? 'open' : 'secured'}</span>`;
      e.onclick = () => { $('ssid').value = n.ssid; $('nets').innerHTML = ''; $('pass').value = ''; if (!n.open) $('pass').focus(); };
      $('nets').append(e);
    }
  } catch (e) { $('nets').innerHTML = '<div class="hint">Could not scan for networks (try again in a moment).</div>'; }
  b.disabled = false; b.textContent = 'Scan';
};
$('show').onclick = () => {
  const p = $('pass'), hidden = p.type === 'password';
  p.type = hidden ? 'text' : 'password'; $('show').textContent = hidden ? 'Hide' : 'Show';
};

function waitForDevice(text) {
  $('form').style.display = 'none';
  $('done').classList.add('show');
  $('doneText').innerHTML = text;
  let tries = 0;
  setTimeout(function poll() {
    fetch('/api/wifi', { cache: 'no-store' }).then(r => { if (r.ok) location.reload(); else throw 0; })
      .catch(() => { if (++tries < 30) setTimeout(poll, 2000); else $('wait').textContent = 'The device does not respond at this address – connect to it as described above.'; });
  }, 5000);
}
function afterText(ap, ssid) {
  return ap ? `Connect your phone or computer to the <b>${esc(ssid)}</b> network and open <b>http://192.168.4.1/</b>.`
            : `The device will join the <b>${esc(ssid)}</b> network. The router assigns the address (shown in the serial console
               and in the router's device list). If the connection fails, after 15–30 s the <b>VirtualLCD</b> network
               appears with the address http://192.168.4.1/.`;
}

$('save').onclick = async () => {
  const ssid = $('ssid').value, pass = $('pass').value;
  const bytes = new TextEncoder().encode(ssid).length;
  if (!bytes || bytes > 32) return note('The network name must be 1 to 32 characters long.', 'err');
  if (pass && (pass.length < 8 || pass.length > 63)) return note('The password must be 8 to 63 characters long or empty.', 'err');
  if (!pass && !confirm(mode === 'ap' ? 'The access point will be open (no password). Continue?'
                                      : 'No password – connecting to an open network. Continue?')) return;
  $('save').disabled = true;
  try {
    await post('/api/wifi', { mode, ssid, pass });
    waitForDevice('Settings saved. ' + afterText(mode === 'ap', ssid));
  } catch (e) { note('Not saved: ' + e.message, 'err'); $('save').disabled = false; }
};
$('clear').onclick = async () => {
  if (!confirm('Delete the WiFi settings saved on this page? After the restart the data from secrets.h or the VirtualLCD access point will be used.')) return;
  try {
    await post('/api/wifi/clear');
    waitForDevice('Saved settings deleted. After the restart the device will use the data from secrets.h, and without it will create the <b>VirtualLCD</b> network (http://192.168.4.1/).');
  } catch (e) { note('Not deleted: ' + e.message, 'err'); }
};
$('reboot').onclick = async () => {
  if (!confirm('Restart the device?')) return;
  try { await post('/api/reboot'); waitForDevice('The device is restarting with the current settings.'); }
  catch (e) { note('Error: ' + e.message, 'err'); }
};
</script>
</body>
</html>
)HTML";
