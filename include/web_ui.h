#pragma once

// Single-file web page (HTML+CSS+JS) served directly from flash, no
// filesystem upload and no internet connection required (no external
// resources). Realtime channel control goes over a WebSocket for minimum
// latency; fixture/Wi-Fi/OTA management uses plain REST calls.
static const char INDEX_HTML[] PROGMEM = R"HTMLDOC(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>ESP32 DMX Controller</title>
<style>
  :root{
    --bg:#14161a; --panel:#1e2127; --panel2:#262a32; --border:#33383f;
    --text:#e8e9ec; --muted:#9aa1ab; --accent:#ff9d1f; --accent2:#3fa9f5;
    --danger:#e05a5a; --ok:#4caf6a;
  }
  *{box-sizing:border-box;}
  body{
    margin:0; background:var(--bg); color:var(--text);
    font-family:-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;
    padding-bottom:60px;
  }
  header{
    background:var(--panel); border-bottom:1px solid var(--border);
    padding:14px 16px; position:sticky; top:0; z-index:10;
    display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:8px;
  }
  header h1{font-size:18px; margin:0; letter-spacing:.5px;}
  header h1 span{color:var(--accent);}
  #status{font-size:12px; color:var(--muted); text-align:right; line-height:1.5;}
  #status b{color:var(--text);}
  .dot{display:inline-block; width:8px; height:8px; border-radius:50%; margin-right:5px;}
  .dot.on{background:var(--ok);} .dot.off{background:var(--danger);}

  nav{display:flex; gap:4px; padding:10px 16px 0; flex-wrap:wrap;}
  nav button{
    background:var(--panel); color:var(--muted); border:1px solid var(--border);
    padding:9px 16px; border-radius:8px 8px 0 0; cursor:pointer; font-size:14px;
    border-bottom:none;
  }
  nav button.active{background:var(--panel2); color:var(--text); font-weight:600;}

  main{padding:16px; max-width:960px; margin:0 auto;}
  section{display:none;}
  section.active{display:block;}

  .card{
    background:var(--panel2); border:1px solid var(--border); border-radius:10px;
    padding:16px; margin-bottom:16px;
  }
  .card h2{margin:0 0 12px; font-size:15px; color:var(--muted); text-transform:uppercase; letter-spacing:.5px;}

  .row{display:flex; gap:10px; flex-wrap:wrap; align-items:flex-end;}
  .field{display:flex; flex-direction:column; gap:4px;}
  .field label{font-size:12px; color:var(--muted);}
  input[type=text], input[type=number], input[type=password], input[type=file], select{
    background:var(--bg); border:1px solid var(--border); color:var(--text);
    padding:9px 10px; border-radius:6px; font-size:14px; width:100%;
  }
  input[type=number]{width:100px;}

  .channel-row{display:flex; gap:8px; align-items:center; margin-bottom:6px;}
  .channel-row input[type=text]{flex:1;}
  .channel-row .chnum{font-size:12px; color:var(--muted); font-family:monospace; width:26px; flex-shrink:0;}
  .channel-row button{
    background:transparent; border:1px solid var(--border); color:var(--muted);
    border-radius:6px; padding:6px 10px; cursor:pointer; font-size:12px; flex-shrink:0;
  }

  button.btn{
    background:var(--accent2); color:#0a0a0a; border:none; border-radius:6px;
    padding:10px 16px; font-size:14px; font-weight:600; cursor:pointer;
  }
  button.btn.secondary{background:var(--border); color:var(--text);}
  button.btn.danger{background:var(--danger); color:#fff;}
  button.btn.warn{background:var(--accent); color:#0a0a0a;}
  button.btn:active{filter:brightness(0.9);}
  button.btn:disabled{opacity:.5; cursor:default;}

  .global-actions{display:flex; gap:10px; margin-bottom:16px; flex-wrap:wrap; align-items:center;}

  .fixture{border:1px solid var(--border); border-radius:8px; padding:12px; margin-bottom:12px; background:var(--bg);}
  .fixture-head{display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px; margin-bottom:10px;}
  .fixture-head .name{font-weight:600;}
  .fixture-head .addr{font-size:12px; color:var(--muted); font-family:monospace;}
  .fixture-actions{display:flex; gap:6px;}
  .fixture-actions button{
    background:transparent; border:1px solid var(--border); color:var(--muted);
    border-radius:6px; padding:5px 9px; cursor:pointer; font-size:12px;
  }
  .channels{display:grid; grid-template-columns:repeat(auto-fill,minmax(110px,1fr)); gap:10px;}
  .channel{background:var(--panel2); border-radius:6px; padding:8px; text-align:center;}
  .channel .lbl{font-size:11px; color:var(--muted); margin-bottom:4px;}
  .channel .val{font-size:13px; font-weight:600; margin-top:4px; color:var(--accent2);}
  input[type=range]{width:100%; accent-color: var(--accent2);}

  .empty{color:var(--muted); font-size:13px; text-align:center; padding:20px 0;}
  .msg{font-size:13px; margin-top:8px;}
  .msg.error{color:var(--danger);}
  .msg.ok{color:var(--ok);}

  table.raw{width:100%; border-collapse:collapse; font-size:13px;}
  table.raw th, table.raw td{text-align:left; padding:4px; border-bottom:1px solid var(--border);}

  progress{width:100%; height:18px; accent-color:var(--accent2);}
</style>
</head>
<body>

<header>
  <h1>ESP32 <span>DMX</span> Controller</h1>
  <div id="status">loading status...</div>
</header>

<nav>
  <button class="tab active" data-tab="profiles">Profiles</button>
  <button class="tab" data-tab="fixtures">Fixtures</button>
  <button class="tab" data-tab="raw">Raw channel</button>
  <button class="tab" data-tab="presets">Presets</button>
  <button class="tab" data-tab="wifi">Wi-Fi</button>
  <button class="tab" data-tab="artnet">Art-Net</button>
  <button class="tab" data-tab="ota">Firmware</button>
</nav>

<main>

  <div class="global-actions">
    <span class="dot off" id="ws-dot"></span><span id="ws-label" style="font-size:12px;color:var(--muted);">connecting...</span>
    <button class="btn danger" onclick="sendWs({a:'bo'})">Blackout</button>
    <button class="btn warn" onclick="sendWs({a:'fo'})">Full ON</button>
  </div>

  <section id="tab-profiles" class="active">
    <div class="card">
      <h2 id="profile-form-title">New fixture profile</h2>
      <p style="color:var(--muted); font-size:13px;">
        A profile is a reusable template — like an entry in a real console's
        fixture library. Give each channel a name (Red, Green, Dimmer,
        Pan...); you'll pick this profile when patching a fixture below.
      </p>
      <div class="row">
        <div class="field" style="flex:1; min-width:200px;">
          <label>Profile name</label><input type="text" id="profile-name" placeholder="e.g. Generic RGBW">
        </div>
      </div>
      <div class="field" style="margin-top:10px;">
        <label>Channels (in order)</label>
        <div id="profile-channels"></div>
        <button class="btn secondary" style="margin-top:8px; align-self:flex-start;" onclick="addChannelRow()">+ Add channel</button>
      </div>
      <div class="row" style="margin-top:12px;">
        <button class="btn" id="profile-save-btn" onclick="saveProfile()">Save profile</button>
        <button class="btn secondary" id="profile-cancel-btn" onclick="cancelProfileEdit()" style="display:none;">Cancel edit</button>
      </div>
      <div id="profile-msg" class="msg"></div>
    </div>

    <div id="profiles-list"></div>
  </section>

  <section id="tab-fixtures">
    <div class="card">
      <h2>New fixture</h2>
      <div class="row">
        <div class="field"><label>Name</label><input type="text" id="fx-name" placeholder="e.g. Stage left RGB PAR"></div>
        <div class="field"><label>Address (1-512)</label><input type="number" id="fx-address" min="1" max="512" value="1"></div>
        <div class="field" style="flex:1; min-width:200px;">
          <label>Profile</label>
          <select id="fx-profile"></select>
        </div>
        <button class="btn" onclick="addFixture()">Add</button>
      </div>
      <div id="fx-msg" class="msg"></div>
    </div>

    <div id="fixtures-list"></div>
  </section>

  <section id="tab-raw">
    <div class="card">
      <h2>Direct channel control</h2>
      <div class="row">
        <div class="field"><label>Channel (1-512)</label><input type="number" id="raw-channel" min="1" max="512" value="1"></div>
        <div class="field" style="flex:1; min-width:200px;">
          <label>Value: <span id="raw-value-lbl">0</span></label>
          <input type="range" id="raw-value" min="0" max="255" value="0"
            oninput="document.getElementById('raw-value-lbl').textContent=this.value; liveSetRaw();">
        </div>
      </div>
    </div>

    <div class="card">
      <h2>Active channels (value &gt; 0)</h2>
      <table class="raw" id="raw-table">
        <thead><tr><th>Channel</th><th>Value</th></tr></thead>
        <tbody></tbody>
      </table>
      <div class="empty" id="raw-empty">No active channels</div>
    </div>
  </section>

  <section id="tab-presets">
    <div class="card">
      <h2>Save current state as preset</h2>
      <p style="color:var(--muted); font-size:13px;">
        Captures all 512 channels as they are right now (like a scene/cue on
        a real console). Recall it later to instantly restore this look.
      </p>
      <div class="row">
        <div class="field" style="flex:1; min-width:200px;">
          <label>Name</label><input type="text" id="preset-name" placeholder="e.g. Warm wash">
        </div>
        <button class="btn" onclick="savePreset()">Save current as new preset</button>
      </div>
      <div id="preset-msg" class="msg"></div>
    </div>

    <div id="presets-list"></div>
  </section>

  <section id="tab-wifi">
    <div class="card">
      <h2>Wi-Fi network (station mode)</h2>
      <p style="color:var(--muted); font-size:13px;">
        The access point <b id="wifi-ap-name">-</b> is always on for local control.
        Configure an existing network here to also reach the controller from
        your home/venue network.
      </p>
      <div class="row">
        <div class="field"><label>SSID</label><input type="text" id="wifi-ssid"></div>
        <div class="field"><label>Password</label><input type="password" id="wifi-pass"></div>
        <button class="btn" onclick="saveWifi()">Save &amp; reboot</button>
      </div>
      <div id="wifi-msg" class="msg"></div>
    </div>
  </section>

  <section id="tab-artnet">
    <div class="card">
      <h2>Art-Net input</h2>
      <p style="color:var(--muted); font-size:13px;">
        When enabled, incoming Art-Net packets for the selected universe
        drive all 512 channels directly, overriding manual/web control while
        packets keep arriving. Disable it to go back to local control only.
        Setting is remembered across reboots.
      </p>
      <div class="row">
        <div class="field">
          <label><input type="checkbox" id="artnet-enabled" style="width:auto; margin-right:6px;">Enable Art-Net input</label>
        </div>
        <div class="field"><label>Universe (0-based)</label><input type="number" id="artnet-universe" min="0" max="32767" value="0"></div>
        <button class="btn" onclick="saveArtnet()">Save</button>
      </div>
      <div id="artnet-msg" class="msg"></div>
      <p id="artnet-status" style="color:var(--muted); font-size:13px; margin-top:10px;"></p>
    </div>
  </section>

  <section id="tab-ota">
    <div class="card">
      <h2>Firmware update (OTA)</h2>
      <p style="color:var(--muted); font-size:13px;">
        Upload a <code>firmware.bin</code> built with <code>pio run</code>
        (found in <code>.pio/build/&lt;env&gt;/firmware.bin</code>). The
        device reboots automatically when the upload completes.
        Alternatively, use PlatformIO's network upload
        (<code>pio run -e esp32-c3-supermini-ota -t upload</code>) which uses
        the same OTA password.
      </p>
      <div class="row">
        <div class="field"><label>OTA password</label><input type="password" id="ota-pass"></div>
      </div>
      <div class="row" style="margin-top:10px;">
        <input type="file" id="ota-file" accept=".bin">
        <button class="btn" onclick="uploadFirmware()">Upload &amp; flash</button>
      </div>
      <progress id="ota-progress" value="0" max="100" style="display:none; margin-top:10px;"></progress>
      <div id="ota-msg" class="msg"></div>
    </div>
  </section>

</main>

<script>
let fixturesCache = [];
let profilesCache = [];
let profileEditId = null;
let dmxValues = new Array(512).fill(0);
let uiUpdateScheduled = false;
let ws = null;

function $(id){ return document.getElementById(id); }

document.querySelectorAll('nav .tab').forEach(btn=>{
  btn.addEventListener('click', ()=>{
    document.querySelectorAll('nav .tab').forEach(b=>b.classList.remove('active'));
    document.querySelectorAll('main section').forEach(s=>s.classList.remove('active'));
    btn.classList.add('active');
    $('tab-'+btn.dataset.tab).classList.add('active');
    history.replaceState(null, '', '#' + btn.dataset.tab);
  });
});
// Deep-link support: e.g. http://<device>/#fixtures opens straight to that tab.
(() => {
  const initial = location.hash.slice(1);
  const btn = initial && document.querySelector(`nav [data-tab="${initial}"]`);
  if(btn) btn.click();
})();

// Prevents the periodic status poll from wiping out the Wi-Fi form while
// the user is filling it in (it only reflects the currently *saved*/active
// network, which is blank until the form is actually submitted).
let wifiFormDirty = false;
$('wifi-ssid').addEventListener('input', () => wifiFormDirty = true);
$('wifi-pass').addEventListener('input', () => wifiFormDirty = true);

// ---------------------------------------------------------------------------
// WebSocket — realtime DMX channel control (low latency path)
// ---------------------------------------------------------------------------
function connectWs(){
  const proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
  ws = new WebSocket(proto + location.host + '/ws');

  ws.onopen = () => {
    $('ws-dot').className = 'dot on';
    $('ws-label').textContent = 'live';
  };
  ws.onclose = () => {
    $('ws-dot').className = 'dot off';
    $('ws-label').textContent = 'reconnecting...';
    setTimeout(connectWs, 1000);
  };
  ws.onerror = () => ws.close();
  ws.onmessage = (evt) => {
    let msg;
    try { msg = JSON.parse(evt.data); } catch(e){ return; }

    if(msg.d){
      dmxValues = msg.d;
    } else if(msg.c !== undefined){
      dmxValues[msg.c - 1] = msg.v;
    } else if(msg.a === 'bo'){
      dmxValues.fill(0);
    } else if(msg.a === 'fo'){
      dmxValues.fill(255);
    }
    scheduleUiUpdate();
  };
}

function sendWs(obj){
  if(ws && ws.readyState === WebSocket.OPEN){
    ws.send(JSON.stringify(obj));
  }
}

function scheduleUiUpdate(){
  if(uiUpdateScheduled) return;
  uiUpdateScheduled = true;
  requestAnimationFrame(()=>{ uiUpdateScheduled = false; renderDmxState(); });
}

function renderDmxState(){
  document.querySelectorAll('.fixture').forEach(card=>{
    const addr = parseInt(card.dataset.address, 10);
    const ch = parseInt(card.dataset.channels, 10);
    for(let i=0;i<ch;i++){
      const slider = card.querySelector(`input[data-ch="${i}"]`);
      const label = card.querySelector(`span[data-ch="${i}"]`);
      const v = dmxValues[addr - 1 + i];
      if(slider && document.activeElement !== slider) slider.value = v;
      if(label) label.textContent = v;
    }
  });

  if($('tab-raw').classList.contains('active')){
    const tbody = document.querySelector('#raw-table tbody');
    tbody.innerHTML = '';
    let count = 0;
    dmxValues.forEach((v, idx)=>{
      if(v > 0){
        count++;
        const tr = document.createElement('tr');
        tr.innerHTML = `<td>${idx+1}</td><td>${v}</td>`;
        tbody.appendChild(tr);
      }
    });
    $('raw-empty').style.display = count ? 'none' : 'block';
  }
}

function liveSetRaw(){
  const channel = parseInt($('raw-channel').value, 10);
  const value = parseInt($('raw-value').value, 10);
  dmxValues[channel - 1] = value;
  sendWs({c: channel, v: value});
}

// ---------------------------------------------------------------------------
// REST — status, fixtures (CRUD), Wi-Fi config, OTA (low-frequency actions)
// ---------------------------------------------------------------------------
async function api(path, opts){
  const res = await fetch(path, opts);
  let data = null;
  try { data = await res.json(); } catch(e){}
  if(!res.ok || (data && data.ok === false)){
    throw new Error((data && data.error) || ('HTTP error ' + res.status));
  }
  return data;
}

async function refreshStatus(){
  try{
    const s = await api('/api/status');
    $('wifi-ap-name').textContent = s.apSsid;
    let html = `AP: <b>${s.apSsid}</b> (${s.apIp})<br>`;
    if(s.staConnected){
      html += `<span class="dot on"></span>WiFi: <b>${s.staSsid}</b> — ${s.staIp}`;
    } else if(s.staSsid){
      html += `<span class="dot off"></span>WiFi: connecting to <b>${s.staSsid}</b>...`;
    } else {
      html += `<span class="dot off"></span>WiFi not configured`;
    }
    $('status').innerHTML = html;
    if(!wifiFormDirty) $('wifi-ssid').value = s.staSsid || '';
  } catch(e){ /* ignore */ }
}

function channelRowsCount(){
  return document.querySelectorAll('#profile-channels .channel-row').length;
}

function renumberChannelRows(){
  document.querySelectorAll('#profile-channels .channel-row').forEach((row, i) => {
    row.querySelector('.chnum').textContent = 'Ch ' + (i + 1);
  });
}

function addChannelRow(value){
  const wrap = $('profile-channels');
  const row = document.createElement('div');
  row.className = 'channel-row';
  row.innerHTML = `
    <span class="chnum"></span>
    <input type="text" placeholder="e.g. Red" value="${value ? value.replace(/"/g,'&quot;') : ''}">
    <button type="button" onclick="removeChannelRow(this)">&times;</button>
  `;
  wrap.appendChild(row);
  renumberChannelRows();
}

function removeChannelRow(btn){
  if(channelRowsCount() <= 1) return; // a profile needs at least one channel
  btn.closest('.channel-row').remove();
  renumberChannelRows();
}

function resetProfileForm(){
  profileEditId = null;
  $('profile-name').value = '';
  $('profile-channels').innerHTML = '';
  addChannelRow();
  $('profile-form-title').textContent = 'New fixture profile';
  $('profile-save-btn').textContent = 'Save profile';
  $('profile-cancel-btn').style.display = 'none';
}

function cancelProfileEdit(){
  resetProfileForm();
}

function editProfile(id){
  const p = profilesCache.find(x => x.id === id);
  if(!p) return;
  profileEditId = id;
  $('profile-name').value = p.name;
  $('profile-channels').innerHTML = '';
  p.channels.forEach(ch => addChannelRow(ch));
  $('profile-form-title').textContent = 'Edit "' + p.name + '"';
  $('profile-save-btn').textContent = 'Update profile';
  $('profile-cancel-btn').style.display = 'inline-block';
  document.querySelector('nav [data-tab="profiles"]').click();
}

async function saveProfile(){
  const name = $('profile-name').value.trim();
  const channels = Array.from(document.querySelectorAll('#profile-channels input[type=text]')).map(i => i.value.trim());
  const msg = $('profile-msg');
  msg.textContent = '';
  try{
    if(profileEditId){
      await api('/api/profiles/update', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({id: profileEditId, name, channels})
      });
      msg.className = 'msg ok'; msg.textContent = 'Profile updated.';
    } else {
      await api('/api/profiles', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({name, channels})
      });
      msg.className = 'msg ok'; msg.textContent = 'Profile saved.';
    }
    resetProfileForm();
    await refreshProfiles();
  }catch(e){
    msg.className = 'msg error'; msg.textContent = e.message;
  }
}

async function deleteProfile(id){
  if(!confirm('Delete this profile?')) return;
  try{
    await api('/api/profiles/delete', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({id})
    });
    refreshProfiles();
  }catch(e){ alert(e.message); }
}

function profileRow(p){
  const div = document.createElement('div');
  div.className = 'fixture';
  div.innerHTML = `
    <div class="fixture-head">
      <div>
        <div class="name">${p.name}</div>
        <div class="addr">${p.channels.length} ch: ${p.channels.join(', ')}</div>
      </div>
      <div class="fixture-actions">
        <button onclick="editProfile(${p.id})">Edit</button>
        <button onclick="deleteProfile(${p.id})">Delete</button>
      </div>
    </div>
  `;
  return div;
}

async function refreshProfiles(){
  const data = await api('/api/profiles');
  profilesCache = data.profiles;

  const list = $('profiles-list');
  list.innerHTML = '';
  if(profilesCache.length === 0){
    list.innerHTML = '<div class="empty">No profiles yet. Create one above.</div>';
  } else {
    profilesCache.forEach(p => list.appendChild(profileRow(p)));
  }

  const select = $('fx-profile');
  const prevValue = select.value;
  select.innerHTML = profilesCache.map(p =>
    `<option value="${p.id}">${p.name} (${p.channels.length}ch)</option>`
  ).join('');
  if(prevValue && profilesCache.some(p => String(p.id) === prevValue)) select.value = prevValue;
}

function fixtureCard(fx){
  const div = document.createElement('div');
  div.className = 'fixture';
  div.dataset.address = fx.address;
  div.dataset.channels = fx.channels.length;

  let channelsHtml = '';
  fx.channels.forEach((chName, i) => {
    const v = dmxValues[fx.address - 1 + i] || 0;
    channelsHtml += `
      <div class="channel">
        <div class="lbl">${chName} (addr ${fx.address + i})</div>
        <input type="range" min="0" max="255" value="${v}" data-ch="${i}"
          oninput="liveSetFixtureChannel(${fx.id}, ${i}, this.value)">
        <span class="val" data-ch="${i}">${v}</span>
      </div>`;
  });

  div.innerHTML = `
    <div class="fixture-head">
      <div>
        <div class="name">${fx.name}</div>
        <div class="addr">${fx.profileName} — address ${fx.address} - ${fx.address + fx.channels.length - 1} (${fx.channels.length} ch)</div>
      </div>
      <div class="fixture-actions">
        <button onclick="deleteFixture(${fx.id})">Delete</button>
      </div>
    </div>
    <div class="channels">${channelsHtml}</div>
  `;
  return div;
}

async function refreshFixtures(){
  const data = await api('/api/fixtures');
  fixturesCache = data.fixtures;
  const list = $('fixtures-list');
  list.innerHTML = '';
  if(fixturesCache.length === 0){
    list.innerHTML = '<div class="empty">No fixtures configured yet. Add one above.</div>';
    return;
  }
  fixturesCache.sort((a,b)=>a.address-b.address).forEach(fx=>{
    list.appendChild(fixtureCard(fx));
  });
}

async function addFixture(){
  const name = $('fx-name').value.trim();
  const address = parseInt($('fx-address').value, 10);
  const profileId = parseInt($('fx-profile').value, 10);
  const msg = $('fx-msg');
  msg.textContent = '';
  try{
    await api('/api/fixtures', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({name, address, profileId})
    });
    $('fx-name').value = '';
    msg.className = 'msg ok'; msg.textContent = 'Fixture added.';
    refreshFixtures();
  }catch(e){
    msg.className = 'msg error'; msg.textContent = e.message;
  }
}

async function deleteFixture(id){
  if(!confirm('Delete this fixture?')) return;
  await api('/api/fixtures/delete', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({id})
  });
  refreshFixtures();
}

function liveSetFixtureChannel(fixtureId, chIndex, value){
  const fx = fixturesCache.find(f=>f.id===fixtureId);
  if(!fx) return;
  const channel = fx.address + chIndex;
  value = parseInt(value, 10);
  dmxValues[channel - 1] = value;
  const card = document.querySelector(`.fixture[data-address="${fx.address}"]`);
  if(card){
    const label = card.querySelector(`span[data-ch="${chIndex}"]`);
    if(label) label.textContent = value;
  }
  sendWs({c: channel, v: value});
}

async function saveWifi(){
  const ssid = $('wifi-ssid').value.trim();
  const password = $('wifi-pass').value;
  const msg = $('wifi-msg');
  try{
    await api('/api/wifi', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({ssid, password})
    });
    wifiFormDirty = false;
    msg.className = 'msg ok';
    msg.textContent = 'Saved. The device is rebooting...';
  }catch(e){
    msg.className = 'msg error'; msg.textContent = e.message;
  }
}

async function refreshArtnet(){
  try{
    const a = await api('/api/artnet');
    if(document.activeElement !== $('artnet-enabled')) $('artnet-enabled').checked = a.enabled;
    if(document.activeElement !== $('artnet-universe')) $('artnet-universe').value = a.universe;

    let status;
    if(a.msSinceLastPacket < 0){
      status = 'No Art-Net packets received yet.';
    } else {
      status = `Last packet ${(a.msSinceLastPacket/1000).toFixed(1)}s ago from ${a.senderIp}.`;
    }
    $('artnet-status').textContent = status;
  }catch(e){ /* ignore */ }
}

async function saveArtnet(){
  const enabled = $('artnet-enabled').checked;
  const universe = parseInt($('artnet-universe').value, 10) || 0;
  const msg = $('artnet-msg');
  try{
    await api('/api/artnet', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({enabled, universe})
    });
    msg.className = 'msg ok'; msg.textContent = 'Saved.';
  }catch(e){
    msg.className = 'msg error'; msg.textContent = e.message;
  }
}

function uploadFirmware(){
  const fileInput = $('ota-file');
  const msg = $('ota-msg');
  const progress = $('ota-progress');
  msg.textContent = '';
  if(!fileInput.files.length){
    msg.className = 'msg error'; msg.textContent = 'Choose a firmware.bin file first.';
    return;
  }

  const form = new FormData();
  form.append('password', $('ota-pass').value);
  form.append('firmware', fileInput.files[0]);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');
  progress.style.display = 'block';
  xhr.upload.onprogress = (e) => {
    if(e.lengthComputable) progress.value = (e.loaded / e.total) * 100;
  };
  xhr.onload = () => {
    if(xhr.status === 200){
      msg.className = 'msg ok';
      msg.textContent = 'Update successful, the device is rebooting...';
    } else {
      msg.className = 'msg error';
      msg.textContent = 'Update failed: ' + xhr.responseText;
    }
  };
  xhr.onerror = () => {
    msg.className = 'msg error';
    msg.textContent = 'Upload error.';
  };
  xhr.send(form);
}

function presetRow(p){
  const div = document.createElement('div');
  div.className = 'fixture';
  div.innerHTML = `
    <div class="fixture-head">
      <div class="field" style="flex:1; min-width:160px;">
        <input type="text" value="${p.name}" data-preset-name="${p.id}">
      </div>
      <div class="fixture-actions">
        <button onclick="recallPreset(${p.id})">Recall</button>
        <button onclick="updatePreset(${p.id})">Update</button>
        <button onclick="deletePreset(${p.id})">Delete</button>
      </div>
    </div>
  `;
  return div;
}

async function refreshPresets(){
  const data = await api('/api/presets');
  const list = $('presets-list');
  list.innerHTML = '';
  if(data.presets.length === 0){
    list.innerHTML = '<div class="empty">No presets saved yet.</div>';
    return;
  }
  data.presets.forEach(p => list.appendChild(presetRow(p)));
}

async function savePreset(){
  const name = $('preset-name').value.trim();
  const msg = $('preset-msg');
  try{
    await api('/api/presets', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({name})
    });
    $('preset-name').value = '';
    msg.className = 'msg ok'; msg.textContent = 'Preset saved.';
    refreshPresets();
  }catch(e){
    msg.className = 'msg error'; msg.textContent = e.message;
  }
}

async function recallPreset(id){
  try{
    await api('/api/presets/recall', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({id})
    });
  }catch(e){ alert(e.message); }
}

async function updatePreset(id){
  const input = document.querySelector(`input[data-preset-name="${id}"]`);
  const name = input ? input.value.trim() : '';
  try{
    await api('/api/presets/update', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({id, name})
    });
  }catch(e){ alert(e.message); }
}

async function deletePreset(id){
  if(!confirm('Delete this preset?')) return;
  await api('/api/presets/delete', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({id})
  });
  refreshPresets();
}

async function refreshAll(){
  await refreshStatus();
  await refreshProfiles();
  await refreshFixtures();
  await refreshPresets();
  await refreshArtnet();
  renderDmxState();
}

resetProfileForm();
connectWs();
refreshAll();
setInterval(refreshStatus, 5000);
setInterval(refreshArtnet, 3000);
</script>
</body>
</html>
)HTMLDOC";
