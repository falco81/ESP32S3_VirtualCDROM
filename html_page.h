// Auto-generated - do not edit manually
// HTML page for ESP32-S3 Virtual CD-ROM web interface
#pragma once

static const char HTML_PAGE[] PROGMEM = R"RAWHTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 Virtual CD-ROM</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#0d1117;--surface:#161b22;--border:#30363d;--border2:#21262d;
  --tx:#c9d1d9;--tx2:#8b949e;--tx3:#484f58;
  --blue:#58a6ff;--green:#3fb950;--red:#f85149;
  --gbg:#238636;--rbg:#b62324;--bbg:#1f6feb;--gray:#21262d;
  --ghi:#2ea043;--rhi:#da3633;--bhi:#388bfd;--grhi:#30363d;
  --r:8px;--rs:5px;
}
html,body{height:100%;font-family:'Courier New',monospace;background:var(--bg);color:var(--tx)}
body{display:flex;flex-direction:column;padding:clamp(8px,2vw,20px);gap:clamp(6px,1.2vw,14px);min-height:100vh}
h1{color:var(--blue);font-size:clamp(1rem,2.5vw,1.4rem)}
.sub{color:var(--tx2);font-size:clamp(.7rem,1.5vw,.85rem)}
.tabs{display:flex;gap:4px;border-bottom:1px solid var(--border)}
.tab{padding:clamp(5px,1vw,9px) clamp(10px,2vw,20px);cursor:pointer;color:var(--tx2);border:1px solid transparent;border-bottom:none;border-radius:6px 6px 0 0;font-size:clamp(.75rem,1.5vw,.9rem);user-select:none;position:relative;bottom:-1px}
.tab.active{color:var(--tx);background:var(--surface);border-color:var(--border)}
.tab.disabled{opacity:.4;cursor:default;pointer-events:none}
.panel{display:none;flex:1;flex-direction:column;gap:clamp(6px,1vw,12px);min-height:0}
.panel.active{display:flex}
.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);padding:clamp(8px,1.5vw,16px)}
.ct{font-size:clamp(.68rem,1.3vw,.8rem);color:var(--tx2);text-transform:uppercase;letter-spacing:.06em;margin-bottom:clamp(6px,1vw,10px)}
.sbar{display:flex;align-items:center;gap:clamp(6px,1vw,10px);flex-wrap:wrap}
.badge{padding:2px clamp(6px,1vw,12px);border-radius:20px;font-size:clamp(.68rem,1.3vw,.8rem);font-weight:bold}
.badge.on{background:#1f4b2e;color:var(--green);border:1px solid var(--green)}
.badge.off{background:#3b1f1f;color:var(--red);border:1px solid var(--red)}
.badge.def{background:#1f3a5f;color:var(--blue);border:1px solid var(--bbg)}
.mname{color:#79c0ff;font-size:clamp(.75rem,1.5vw,.95rem);flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.btn{display:inline-flex;align-items:center;gap:4px;padding:clamp(4px,.8vw,6px) clamp(8px,1.5vw,14px);border:none;border-radius:var(--rs);cursor:pointer;font-size:clamp(.7rem,1.3vw,.82rem);font-weight:bold;font-family:inherit;white-space:nowrap;transition:.12s}
.g{background:var(--gbg);color:#fff}.g:hover{background:var(--ghi)}
.r{background:var(--rbg);color:#fff}.r:hover{background:var(--rhi)}
.b{background:var(--bbg);color:#fff}.b:hover{background:var(--bhi)}
.gr{background:var(--gray);color:var(--tx);border:1px solid var(--border)}.gr:hover{background:var(--grhi)}
.yw{background:#7d4e17;color:#e3b341;border:1px solid #9e6a03}.yw:hover{background:#9e6a03}
.tbl-wrap{overflow-y:auto;overflow-x:hidden;flex:1;min-height:0}
table{width:100%;table-layout:auto;border-collapse:collapse;font-size:clamp(.72rem,1.4vw,.88rem)}
th,td{vertical-align:middle;padding:clamp(4px,.8vw,8px) clamp(6px,1vw,10px);vertical-align:middle}
th{text-align:left;color:var(--tx2);border-bottom:1px solid var(--border2);font-weight:normal;position:sticky;top:0;background:var(--surface);z-index:1}
td{border-bottom:1px solid var(--bg)}
tr:last-child td{border-bottom:none}
tr:hover td{background:var(--border2)}
td.ic,th.ic{width:28px;min-width:28px;text-align:center;font-size:1rem}
td.nm{color:var(--tx);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;min-width:0;}
td.nm.d{color:#79c0ff;cursor:pointer}td.nm.d:hover{text-decoration:underline}
td.sz,th.sz{color:var(--tx2);text-align:right;white-space:nowrap;width:100px}
td.ac,th.ac{text-align:right;white-space:nowrap;min-width:100px;vertical-align:middle}
td.ac .btn{margin-left:4px;white-space:nowrap;min-width:28px;padding:4px 8px}
#mkModal.show{display:flex!important}
.bc{display:flex;align-items:center;gap:4px;flex-wrap:wrap;margin-bottom:clamp(6px,1vw,10px);font-size:clamp(.7rem,1.3vw,.85rem)}
.bcp{color:var(--blue);cursor:pointer}.bcp:hover{text-decoration:underline}
.bcs{color:var(--tx3)}
.tb{display:flex;gap:clamp(4px,.8vw,8px);margin-bottom:clamp(6px,1vw,10px);flex-wrap:wrap;align-items:center}
.pw{display:none;margin:8px 0;padding:10px;background:var(--bg);border-radius:6px;border:1px solid var(--border2)}
.pf{font-size:clamp(.68rem,1.2vw,.8rem);color:var(--tx2);margin-bottom:5px}
.pb{height:6px;background:var(--border2);border-radius:4px;overflow:hidden;margin-bottom:4px}
.pbi{height:100%;background:var(--bbg);width:0%;border-radius:4px;transition:width .15s}
.pp{font-size:clamp(.65rem,1.2vw,.78rem);color:var(--blue)}
.ni{color:var(--tx2);padding:clamp(8px,1.5vw,16px) 0;text-align:center;font-size:clamp(.72rem,1.4vw,.86rem)}
.dz{border:2px dashed var(--border);border-radius:6px;padding:10px;text-align:center;color:var(--tx3);font-size:clamp(.7rem,1.3vw,.83rem);margin-bottom:8px;display:none}
.dz.ov{border-color:var(--bbg);color:var(--blue)}
#logcard{display:none}
.sp{display:none;width:13px;height:13px;border:2px solid var(--border);border-top-color:var(--blue);border-radius:50%;animation:spin .7s linear infinite;vertical-align:middle;margin-left:5px}
#log{background:#010409;border:1px solid var(--border2);border-radius:var(--rs);padding:clamp(8px,1.5vw,14px);font-size:clamp(.72rem,1.4vw,.85rem);color:var(--green);flex:1;overflow-y:auto;font-family:monospace;white-space:pre-wrap;word-break:break-all}
.si-tbl{width:100%;table-layout:auto;border-collapse:collapse;font-size:clamp(.72rem,1.4vw,.88rem)}
.si-tbl td{padding:clamp(3px,.6vw,6px) clamp(4px,.8vw,8px);border-bottom:1px solid var(--bg);vertical-align:top}
.si-tbl tr:last-child td{border-bottom:none}
.si-tbl td:first-child{color:var(--tx2);white-space:nowrap;width:1%;padding-right:clamp(12px,2vw,24px)}
.si-tbl td:last-child{color:var(--tx)}
.cfg-row{display:flex;align-items:center;gap:10px;margin-bottom:8px}.cfg-lbl{color:var(--tx2);font-size:clamp(.7rem,1.3vw,.82rem);white-space:nowrap;width:140px;min-width:140px}.cfg-inp{flex:1;background:var(--bg);border:1px solid var(--border);border-radius:var(--rs);color:var(--tx);padding:clamp(4px,.7vw,7px) 10px;font-family:inherit;font-size:clamp(.75rem,1.4vw,.88rem)}.cfg-inp:focus{outline:none;border-color:var(--blue)}.si-ok{color:var(--green)!important}.si-err{color:var(--red)!important}.si-warn{color:#e3b341!important}
.si-bar{height:6px;background:var(--border2);border-radius:3px;overflow:hidden;margin-top:3px}
.si-bar-i{height:100%;border-radius:3px;background:var(--bbg)}
@keyframes spin{to{transform:rotate(360deg)}}
.mbg{display:none;position:fixed;inset:0;background:rgba(0,0,0,.75);z-index:100;align-items:center;justify-content:center}
.mbg.show{display:flex}
.md{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);padding:clamp(14px,2vw,22px);min-width:280px;max-width:440px;width:90%}
.md h3{margin-bottom:10px;font-size:clamp(.85rem,1.8vw,1rem)}
.md input{width:100%;background:var(--bg);border:1px solid var(--border);border-radius:var(--rs);color:var(--tx);padding:clamp(5px,.8vw,8px) 10px;font-family:inherit;font-size:clamp(.75rem,1.4vw,.9rem);margin-bottom:10px}
.md input:focus{outline:none;border-color:var(--blue)}
.mdb{display:flex;gap:8px;justify-content:flex-end}
.cue-badge{font-size:.68rem;padding:1px 5px;border-radius:3px;background:#1f3a5f;color:#79c0ff;border:1px solid var(--bbg);margin-left:5px;vertical-align:middle}
.def-badge{font-size:.68rem;padding:1px 5px;border-radius:3px;background:#2a1f00;color:#e3b341;border:1px solid #9e6a03;margin-left:5px;vertical-align:middle}
.tog{width:32px;height:17px;background:var(--gray);border-radius:9px;cursor:pointer;position:relative;border:1px solid var(--border);transition:.2s;flex-shrink:0;display:inline-block}
.tog.on{background:var(--gbg);border-color:var(--ghi)}
.tknob{width:11px;height:11px;background:var(--tx);border-radius:50%;position:absolute;top:2px;left:2px;transition:.2s}
.tog.on .tknob{left:17px}
@media(max-width:600px){td.sz{display:none}.mname{font-size:.8rem}}

.ap{background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);border:1px solid #e94560;border-radius:10px;padding:14px 16px;margin-top:8px;display:none}
.ap.show{display:block}
.ap-title{font-size:.78rem;color:var(--muted);margin-bottom:10px;letter-spacing:.05em;text-transform:uppercase}
.ap-track{font-size:1rem;font-weight:600;color:#e94560;min-height:1.4em;margin-bottom:4px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ap-time{font-size:.78rem;color:var(--tx2);margin-bottom:10px;display:flex;justify-content:space-between}
.ap-prog{width:100%;height:6px;background:#0f3460;border-radius:3px;cursor:pointer;margin-bottom:12px;position:relative}
.ap-prog-fill{height:100%;background:linear-gradient(90deg,#e94560,#f5a623);border-radius:3px;pointer-events:none;transition:width .5s linear}
.ap-btns{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-bottom:10px}
.ap-btn{background:#0f3460;border:1px solid #1e4080;border-radius:6px;color:#e0e0e0;padding:6px 12px;cursor:pointer;font-size:1rem;line-height:1;transition:background .15s,transform .1s}
.ap-btn:hover{background:#1e4080}
.ap-btn:active{transform:scale(.93)}
.ap-btn.active{background:#e94560;border-color:#e94560;color:#fff}
.ap-vol{display:flex;align-items:center;gap:8px;margin-top:6px}
.ap-vol-slider{flex:1;-webkit-appearance:none;height:4px;border-radius:2px;background:#0f3460;outline:none;cursor:pointer}
.ap-vol-slider::-webkit-slider-thumb{-webkit-appearance:none;width:14px;height:14px;border-radius:50%;background:#e94560;cursor:pointer}
.ap-tracklist{margin-top:10px;max-height:140px;overflow-y:auto;border-top:1px solid #1e4080;padding-top:8px}
.ap-trow{display:flex;align-items:center;gap:8px;padding:5px 6px;border-radius:5px;cursor:pointer;font-size:.8rem;transition:background .1s}
.ap-trow:hover{background:#1e4080}
.ap-trow.playing{background:#e9456022;border-left:3px solid #e94560}
.ap-tnum{color:var(--muted);min-width:22px;text-align:right}
.ap-tname{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ap-tdur{color:var(--muted);font-size:.75rem}
</style>
</head>
<body>
<div>
  <h1>&#128191; ESP32-S3 Virtual CD-ROM</h1>
  <p class="sub">MicroSD &#8594; USB CD-ROM for PC</p>
</div>
<div class="tabs">
  <div class="tab active" id="t0" onclick="showTab(0)">&#128249; CD-ROM</div>
  <div class="tab" id="t1" onclick="showTab(1)">&#127925; Audio</div>
  <div class="tab" id="t2" onclick="showTab(2)">&#128193; File Manager</div>
  <div class="tab" id="t3" onclick="showTab(3)">&#128203; Log</div>
  <div class="tab" id="t4" onclick="showTab(4)">&#128268; Status</div>
  <div class="tab" id="t5" onclick="showTab(5)">&#9881; Config</div>
</div>

<div class="panel active" id="p0">
  <div class="card">
    <div class="ct">&#127908; Current medium</div>
    <div class="sbar">
      <span class="badge off" id="badge">NONE</span>
      <span class="mname" id="mname">&#8212; nothing mounted &#8212;</span>
      <button class="btn r" onclick="doEject()">&#9167; Eject</button>
    </div>
    <div id="defbar" style="margin-top:8px;display:none;align-items:center;gap:8px;flex-wrap:wrap">
      <span style="font-size:clamp(.68rem,1.2vw,.78rem);color:var(--tx2)">Default on boot:</span>
      <span class="badge def" id="defbadge" style="font-size:.72rem"></span>
      <button class="btn yw" onclick="doSetDefault()" id="btnSetDef" title="Set current as default">&#11088; Set default</button>
      <button class="btn gr" onclick="doClearDefault()" id="btnClrDef" title="Clear default">&#10005; Clear</button>
    </div>
  </div>
  <div class="card" style="flex:1;display:flex;flex-direction:column;min-height:0;overflow:hidden">
    <div class="ct" style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:6px">
      <span>&#128190; Browse SD for disc images <span class="sp" id="cdSp"></span></span>
      <label style="display:flex;align-items:center;gap:6px;cursor:pointer;font-size:clamp(.68rem,1.3vw,.8rem);color:var(--tx2);text-transform:none;letter-spacing:0">
        <span>Show BIN tracks</span>
        <div class="tog" onclick="toggleBin()"><div class="tknob"></div></div>
      </label>
    </div>
    <div class="bc" id="cdBc"></div>
    <div id="cdNone" class="ni" style="display:none">No disc images found here.</div>
    <div class="tbl-wrap">
      <table id="cdTbl" style="table-layout:fixed;width:100%">
        <colgroup><col style="width:32px"><col><col style="width:100px"><col style="width:130px"></colgroup>
        <thead><tr><th class="ic"></th><th>Name</th><th class="sz">Size</th><th class="ac"></th></tr></thead>
        <tbody id="cdBody"></tbody>
      </table>
    </div>
  </div>
</div>

<div class="panel" id="p1">
  <div class="card" style="flex:1">
    <div class="ap" id="audioPlayer" style="display:block;background:transparent;border:none;padding:0">
      <div class="ct" style="margin-bottom:12px">&#127925; Audio CD Player</div>
      <div class="ap-track" id="apTrackName">&#8212;</div>
      <div class="ap-time">
        <span id="apTimeRel">0:00</span>
        <span id="apTimeAbs">0:00</span>
        <span id="apTimeTotal">&#8212;</span>
      </div>
      <div class="ap-prog" id="apProg" onclick="apSeek(event)">
        <div class="ap-prog-fill" id="apProgFill" style="width:0%"></div>
      </div>
      <div class="ap-btns">
        <button class="ap-btn" onclick="apPrev()" title="Previous track">&#9198;</button>
        <button class="ap-btn" id="apBtnPlay" onclick="apPlayPause()" title="Play/Pause">&#9654;</button>
        <button class="ap-btn" onclick="apStop()" title="Stop">&#9632;</button>
        <button class="ap-btn" onclick="apNext()" title="Next track">&#9197;</button>
        <button class="ap-btn" id="apBtnMute" onclick="apToggleMute()" title="Mute">&#128266;</button>
        <span style="flex:1"></span>
        <span id="apStateBadge" style="font-size:.72rem;padding:3px 8px;border-radius:4px;background:#0f3460;color:var(--muted)">STOPPED</span>
      </div>
      <div class="ap-vol">
        <span style="font-size:.85rem">&#128265;</span>
        <input class="ap-vol-slider" id="apVol" type="range" min="0" max="100" value="80"
               oninput="apSetVolume(this.value)" title="Volume">
        <span style="font-size:.78rem;color:var(--tx2);min-width:28px;text-align:right" id="apVolLabel">80</span>
      </div>
      <div class="ap-tracklist" id="apTrackList" style="max-height:300px"></div>
    </div>
  </div>
</div>

<div class="panel" id="p2">
  <div class="card" style="flex:1;display:flex;flex-direction:column;min-height:0;overflow:hidden">
    <div class="bc" id="fmBc"></div>
    <div class="tb">
      <button class="btn b" onclick="openDz()">&#8679; Upload</button>
      <button class="btn gr" onclick="openMkdir()">&#128193; New folder</button>
      <span class="sp" id="fmSp"></span>
    </div>
    <div class="dz" id="dz">
      Drop files here or
      <button class="btn b" style="margin:0 6px" onclick="document.getElementById('fi').click()">Browse&hellip;</button>
      <button class="btn gr" onclick="closeDz()">&#10005;</button>
      <input type="file" id="fi" multiple style="display:none" onchange="queueFiles(this.files)">
    </div>
    <div class="pw" id="pw"><div class="pf" id="pf"></div><div class="pb"><div class="pbi" id="pbi"></div></div><div class="pp" id="pp"></div></div>
    <div id="fmNone" class="ni" style="display:none">(empty folder)</div>
    <div class="tbl-wrap">
      <table id="fmTbl" style="table-layout:fixed;width:100%">
        <colgroup><col style="width:32px"><col><col style="width:100px"><col style="width:130px"></colgroup>
        <thead><tr><th class="ic"></th><th>Name</th><th class="sz">Size</th><th class="ac"></th></tr></thead>
        <tbody id="fmBody"></tbody>
      </table>
    </div>
  </div>
</div>

<div class="panel" id="p3">
  <div class="card" style="flex:1;display:flex;flex-direction:column;min-height:0">
    <div class="ct" style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:6px">
      <span>&#128203; System Log</span>
      <div style="display:flex;gap:6px;align-items:center">
        <label style="font-size:.75rem;color:var(--muted);display:flex;align-items:center;gap:4px">
          <input type="checkbox" id="logAutoScroll" checked> Auto-scroll
        </label>
        <label style="font-size:.75rem;color:var(--muted);display:flex;align-items:center;gap:4px">
          <input type="checkbox" id="logLiveUpdate" checked onchange="logLiveToggle()"> Live
        </label>
        <button class="btn gr" style="font-size:.7rem;padding:2px 8px" onclick="clearLog()">Clear</button>
      </div>
    </div>
    <div id="log" style="flex:1;overflow-y:auto;font-size:.75rem;white-space:pre-wrap;word-break:break-all;min-height:0;background:var(--bg);padding:6px;border-radius:4px;border:1px solid var(--border)">Ready.</div>
  </div>
</div>

<div class="panel" id="p4">
  <div style="display:flex;align-items:center;gap:8px;margin-bottom:2px">
    <button class="btn b" onclick="loadSysinfo()" id="siBtn">&#8635; Refresh</button>
    <span class="sp" id="siSp"></span>
    <span id="siTime" style="font-size:.75rem;color:var(--tx3)"></span>
  </div>
  <div style="display:flex;flex-direction:column;gap:clamp(6px,1vw,12px);overflow-y:auto;flex:1;min-height:0">

    <!-- WiFi -->
    <div class="card">
      <div class="ct">&#128246; WiFi</div>
      <table class="si-tbl" id="siWifi"></table>
    </div>

    <!-- SD Card -->
    <div class="card">
      <div class="ct">&#128190; SD Card</div>
      <table class="si-tbl" id="siSd"></table>
    </div>

    <!-- Disc Image -->
    <div class="card">
      <div class="ct">&#128191; Disc Image</div>
      <table class="si-tbl" id="siImg"></table>
    </div>

    <!-- Audio -->
    <div class="card">
      <div class="ct">&#127925; Audio</div>
      <table class="si-tbl" id="siAudio"><tbody></tbody></table>
    </div>

    <!-- System -->
    <div class="card">
      <div class="ct">&#9881; System</div>
      <table class="si-tbl" id="siSys"></table>
    </div>

  </div>
</div>

<div class="panel" id="p5">
  <div style="display:flex;flex-direction:column;gap:clamp(6px,1vw,12px);overflow-y:auto;flex:1;min-height:0">

    <!-- WiFi Configuration -->
    <div class="card">
      <div class="ct">&#128246; WiFi</div>
      <div style="display:flex;gap:8px;margin-bottom:10px;align-items:center;flex-wrap:wrap">
        <button class="btn b" onclick="cfgScanWifi()" id="scanBtn">&#128269; Scan networks</button>
        <span class="sp" id="scanSp"></span>
      </div>
      <div id="scanResults" style="margin-bottom:10px;display:none">
        <select id="ssidSelect" style="width:100%;background:var(--bg);color:var(--tx);border:1px solid var(--border);border-radius:var(--rs);padding:6px 8px;font-family:inherit;font-size:clamp(.75rem,1.4vw,.88rem);margin-bottom:8px" onchange="cfgPickSsid()">
          <option value="">-- select network --</option>
        </select>
      </div>
      <div class="cfg-row"><label class="cfg-lbl">SSID</label><input class="cfg-inp" id="cfgSsid" type="text" placeholder="Network name" autocomplete="off"><span id="cfgSsidHint" style="font-size:.78rem;color:var(--accent);margin-left:4px"></span></div>
      <div class="cfg-row"><label class="cfg-lbl">Password</label><input class="cfg-inp" id="cfgPass" type="password" placeholder="Leave empty to keep current" autocomplete="new-password"></div>
    </div>

    <!-- Network: DHCP / Static IP -->
    <div class="card">
      <div class="ct">&#127760; Network</div>
      <div class="cfg-row">
        <label class="cfg-lbl">IP mode</label>
        <label style="margin-right:16px"><input type="radio" name="ipmode" id="rDhcp" value="dhcp" checked onchange="cfgDhcpToggle()"> DHCP</label>
        <label><input type="radio" name="ipmode" id="rStatic" value="static" onchange="cfgDhcpToggle()"> Static IP</label>
      </div>
      <div id="staticFields" style="display:none">
        <div class="cfg-row"><label class="cfg-lbl">IP address</label><input class="cfg-inp" id="cfgIp" type="text" placeholder="192.168.1.100"></div>
        <div class="cfg-row"><label class="cfg-lbl">Subnet mask</label><input class="cfg-inp" id="cfgMask" type="text" placeholder="255.255.255.0"></div>
        <div class="cfg-row"><label class="cfg-lbl">Gateway</label><input class="cfg-inp" id="cfgGw" type="text" placeholder="192.168.1.1"></div>
        <div class="cfg-row"><label class="cfg-lbl">DNS</label><input class="cfg-inp" id="cfgDns" type="text" placeholder="8.8.8.8"></div>
      </div>
      <div class="cfg-row"><label class="cfg-lbl">Hostname / FQDN</label>
        <input class="cfg-inp" id="cfgHostname" type="text" placeholder="espcd or espcd.corp.net" style="flex:1" oninput="cfgMdnsUpdate(this.value)">
      </div>
      <div style="padding-left:148px;font-size:.73rem;color:var(--muted);margin-top:-4px;margin-bottom:6px;line-height:1.8">
        FQDN:&nbsp;<span id="cfgFqdnPreview" style="color:var(--tx)"></span>&emsp;·&emsp;mDNS:&nbsp;<a id="cfgMdnsLink" href="#" target="_blank" id2="cfgMdnsPreview"><span id="cfgMdnsPreview" style="color:var(--accent)"></span></a>
      </div>
    </div>

    <!-- 802.1x Enterprise WiFi -->
    <div class="card">
      <div class="ct">&#128275; 802.1x Enterprise WiFi</div>
      <div class="cfg-row">
        <label class="cfg-lbl">EAP Mode</label>
        <select class="cfg-inp" id="cfgEapMode" onchange="cfgEapModeToggle()">
          <option value="0">Disabled (WPA2-Personal)</option>
          <option value="1">PEAP (username + password)</option>
          <option value="2">EAP-TLS (certificate)</option>
        </select>
      </div>
      <div id="eapFields" style="display:none">
        <div class="cfg-row"><label class="cfg-lbl">EAP Identity</label>
          <input class="cfg-inp" id="cfgEapId" type="text" placeholder="user@corp.net (optional for TLS)">
        </div>
        <div id="eapPeapFields">
          <div class="cfg-row"><label class="cfg-lbl">Username</label><input class="cfg-inp" id="cfgEapUser" type="text"></div>
          <div class="cfg-row"><label class="cfg-lbl">Password</label><input class="cfg-inp" id="cfgEapPass" type="password" placeholder="(leave empty to keep current)"></div>
        </div>
        <div id="eapTlsFields">
          <div class="cfg-row">
            <label class="cfg-lbl">Scan SD for certs</label>
            <button class="btn b" onclick="cfgScanCerts()" id="scanCertBtn">&#128269; Scan SD</button>
            <span class="sp" id="certSp"></span>
          </div>
          <div class="cfg-row"><label class="cfg-lbl">CA cert (optional)</label>
            <select class="cfg-inp" id="cfgEapCa"><option value="">(none &#8212; skip server verify)</option></select>
          </div>
          <div class="cfg-row"><label class="cfg-lbl">Client cert</label>
            <select class="cfg-inp" id="cfgEapCert"><option value="">(not set)</option></select>
          </div>
          <div class="cfg-row"><label class="cfg-lbl">Client key</label>
            <select class="cfg-inp" id="cfgEapKey"><option value="">(not set)</option></select>
          </div>
          <div class="cfg-row"><label class="cfg-lbl">Key passphrase</label>
            <input class="cfg-inp" id="cfgEapKPass" type="password"
                   placeholder="(only for encrypted keys)" autocomplete="new-password">
          <div id="eapKeyWarning" style="display:none;font-size:.75rem;color:#f5a623;margin-top:4px">
            &#9888; This appears to be a private key — set Key passphrase if it is encrypted.
          </div>
          </div>
          <div style="font-size:.75rem;color:var(--muted);margin-top:6px">
            &#9432; Click <b>Scan SD</b> to detect .pem/.crt/.key files on the card.
          </div>
        </div>
      </div>
    </div>

    <!-- Audio Module -->
    <div class="card">
      <div class="ct">&#127925; Audio Module</div>
      <div class="cfg-row">
        <label class="cfg-lbl">PCM5102 I2S module</label>
        <select class="cfg-inp" id="cfgAudioModule" style="width:100%">
          <option value="0">Disabled (no hardware connected)</option>
          <option value="1">GY-PCM5102 I2S &#8212; GPIO 14/15/16</option>
        </select>
      </div>
      <div style="font-size:.75rem;color:var(--muted);margin-top:6px">
        &#9432; Requires reboot. BCK&#8594;GPIO14 &nbsp;WS&#8594;GPIO15 &nbsp;DIN&#8594;GPIO16 &nbsp;VCC&#8594;3V3.
      </div>
    </div>

    <!-- Web UI Authentication -->
    <div class="card">
      <div class="ct">&#128272; Web UI Authentication</div>
      <div class="cfg-row">
        <label class="cfg-lbl">HTTP Basic Auth</label>
        <select class="cfg-inp" id="cfgWebAuth" onchange="webAuthToggle()">
          <option value="0">Disabled (no login required)</option>
          <option value="1">Enabled &#8212; require username &amp; password</option>
        </select>
      </div>
      <div id="webAuthFields" style="display:none">
        <div class="cfg-row" style="margin-top:8px">
          <label class="cfg-lbl">Username</label>
          <input class="cfg-inp" id="cfgWebUser" type="text" maxlength="31"
                 autocomplete="username" placeholder="admin">
        </div>
        <div class="cfg-row" style="margin-top:8px">
          <label class="cfg-lbl">New password</label>
          <input class="cfg-inp" id="cfgWebPass" type="password" maxlength="63"
                 autocomplete="new-password" placeholder="(leave empty to keep current)">
        </div>
      </div>
      <div style="font-size:.75rem;color:var(--muted);margin-top:6px">
        &#9432; Default: <b>admin / admin</b>. Changes take effect immediately. Password is write-only.
      </div>
    </div>

    <!-- DOS Compatibility Mode -->
    <div class="card">
      <div class="ct">&#128190; DOS Compatibility Mode</div>
      <div class="cfg-row">
        <label class="cfg-lbl">Mount behavior</label>
        <select class="cfg-inp" id="cfgDosCompat" style="width:100%">
          <option value="0">Normal (USB re-enumeration on mount/eject)</option>
          <option value="1">DOS compatible (UNIT ATTENTION only, no re-enum)</option>
        </select>
      </div>
      <div style="font-size:.75rem;color:var(--muted);margin-top:6px">
        &#9432; Enable when using with DOS (MSCDEX / USBASPI). Prevents the device from
        disappearing after disc swap. Uses UNIT&nbsp;ATTENTION&nbsp;0x28 instead of USB
        re-enumeration so DOS drivers can reload the TOC without losing the device.
      </div>
    </div>

    <!-- HTTPS -->
    <div class="card">
      <div class="ct">&#128274; HTTPS / TLS</div>
      <div class="cfg-row">
        <label class="cfg-lbl">HTTPS on port 443</label>
        <select class="cfg-inp" id="cfgHttpsEnabled" style="width:100%" onchange="httpsToggle()">
          <option value="0">Disabled (HTTP only)</option>
          <option value="1">Enabled (HTTP redirects to HTTPS)</option>
        </select>
      </div>
      <div id="httpsOptions" style="display:none">
        <div class="cfg-row">
          <label class="cfg-lbl">Scan SD for certs</label>
          <button class="btn b" id="httpsScanBtn" onclick="httpsScanCerts()">&#128269; Scan SD</button>
          <span class="sp" id="httpsScanSp"></span>
        </div>
        <div class="cfg-row">
          <label class="cfg-lbl">Server certificate</label>
          <select class="cfg-inp" id="cfgHttpsCert" style="flex:1">
            <option value="">(not set)</option>
          </select>
        </div>
        <div class="cfg-row">
          <label class="cfg-lbl">Server private key</label>
          <select class="cfg-inp" id="cfgHttpsKey" style="flex:1">
            <option value="">(not set)</option>
          </select>
        </div>
        <div class="cfg-row">
          <label class="cfg-lbl">Key passphrase</label>
          <input class="cfg-inp" id="cfgHttpsKPass" type="password"
                 placeholder="(only for encrypted keys)" autocomplete="new-password">
        </div>
        <div class="cfg-row">
          <label class="cfg-lbl">HTTP port</label>
          <input class="cfg-inp" id="cfgHttpPort" type="number" min="1" max="65535" value="80"
                 style="width:90px" title="HTTP server port (reboot required)">
        </div>
        <div id="httpsPortRow" class="cfg-row">
          <label class="cfg-lbl">HTTPS port</label>
          <input class="cfg-inp" id="cfgHttpsPort" type="number" min="1" max="65535" value="443"
                 style="width:90px">
        </div>
        <div id="tlsOptions" style="display:none">
          <div class="cfg-row">
            <label class="cfg-lbl">TLS protocol</label>
            <select class="cfg-inp" id="cfgTlsMinVer" style="flex:1">
              <option value="0">TLS 1.2 only (maximum compat)</option>
              <option value="1">TLS 1.2 + 1.3 (recommended)</option>
            </select>
          </div>
          <div class="cfg-row">
            <label class="cfg-lbl">Cipher suites</label>
            <select class="cfg-inp" id="cfgTlsCiphers" style="flex:1">
              <option value="0">Auto (all supported)</option>
              <option value="1">Strong only — ECDHE + AES-GCM</option>
              <option value="2">Medium — ECDHE + AES-GCM/CBC</option>
              <option value="3">All incl. legacy RSA key exchange</option>
            </select>
          </div>
        </div>
        <div style="font-size:.73rem;color:var(--muted);margin-top:4px">
          &#9432; PEM format on SD. Port/TLS changes require reboot.
          Generate: <code>openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 3650 -nodes -addext "subjectAltName=IP:x.x.x.x"</code>
        </div>
        <div style="font-size:.73rem;color:#e8a020;margin-top:6px;padding:5px 8px;background:rgba(232,160,32,.1);border-radius:4px;border-left:3px solid #e8a020">
          &#9888; <strong>HTTPS reduces transfer speed 2&ndash;4&times;</strong> compared to HTTP (TLS + proxy overhead). Upload/download over HTTPS: ~2&ndash;4 Mbps vs HTTP ~10 Mbps. For large files (ISO, BIN) use HTTP or copy SD card directly to PC.
        </div>
      </div>
    </div>

    <!-- Actions -->
    <div class="card">
      <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:center">
        <button class="btn b" id="cfgSaveBtn" onclick="cfgSave()">&#10003; Save &amp; apply</button>
        <button class="btn yw" onclick="cfgReboot()">&#8635; Reboot</button>
        <button class="btn gr" onclick="cfgFactory()" style="margin-left:auto">&#9888; Factory reset</button>
        <span class="sp" id="cfgActSp"></span>
      </div>
      <div style="margin-top:6px;min-height:1.4rem;text-align:right">
        <span id="cfgActMsg" style="font-size:.8rem"></span>
      </div>
      <div style="margin-top:10px;display:flex;gap:8px;flex-wrap:wrap;align-items:center">
        <button class="btn gr" onclick="sdSafeUnmount()" id="sdUmBtn">&#9014; Unmount SD</button>
        <button class="btn b" onclick="sdRemount()" id="sdMtBtn">&#8679; Mount SD</button>
        <span id="sdCfgMsg" style="font-size:.8rem;color:var(--muted)"></span>
        <span class="sp" id="sdCfgSp"></span>
      </div>
    </div>

  </div>
</div>

<script>
function $(id){return document.getElementById(id);}
var logSeq=0, logTimer=null;

function log(m){
  // Client-side log — for UI action feedback
  var el=$('log');
  if(!el) return;
  var t=new Date().toTimeString().slice(0,8);
  el.textContent+='['+t+'] '+m+'\n';
  if($('logAutoScroll')&&$('logAutoScroll').checked) el.scrollTop=el.scrollHeight;
}

function logFetch(){
  fetch('/api/log?seq='+logSeq).then(function(r){return r.json();}).then(function(d){
    if(!d||d.seq===undefined) return;
    if(d.seq!==logSeq){
      // New content available — replace entire log content
      logSeq=d.seq;
      var el=$('log');
      if(!el) return;
      el.textContent=d.log;
      if($('logAutoScroll')&&$('logAutoScroll').checked) el.scrollTop=el.scrollHeight;
    }
  }).catch(function(){});
}

function logLiveToggle(){
  var live=$('logLiveUpdate')&&$('logLiveUpdate').checked;
  if(live){
    if(!logTimer) logTimer=setInterval(logFetch,1000);
    logFetch();
  // Pre-fetch dosCompatOn so doMount knows mode before Status tab is opened
  fetch('/api/sysinfo').then(function(r){return r.json();}).then(function(s){dosCompatOn=s.dos_compat||false;}).catch(function(){});
  } else {
    if(logTimer){clearInterval(logTimer);logTimer=null;}
  }
}

function clearLog(){
  var el=$('log');
  if(el) el.textContent='Log cleared.\n';
}
function fmtSz(b){if(!b||b<0)return '';var u=['B','KB','MB','GB'],i=0;while(b>=1024&&i<3){b/=1024;i++;}return(i?b.toFixed(1):b)+'\u00a0'+u[i];}
function spin(id,on){$(id).style.display=on?'inline-block':'none';}
function jp(b,n){return b==='/'?'/'+n:b+'/'+n;}
function mkTr(){return document.createElement('tr');}
function mkTd(cls,t){var td=document.createElement('td');td.className=cls;if(t)td.textContent=t;return td;}
function cmp(a,b){return a.name.localeCompare(b.name);}

var curTab=0;
// ══════════════════════════════════════════════════════
//  AUDIO PLAYER
// ══════════════════════════════════════════════════════
var apState=0, apTrack=0, apTracks=[], apPollTimer=null, apVolume=80, apMuted=false;

function apFmt(m,s,f){return m+':'+(s<10?'0':'')+s;}
function apSectToMs(sects){var t=Math.floor(sects/75);return apFmt(Math.floor(t/60),t%60,0);}

function apLoad(status){
  if(!status||!status.tracks) return;
  apTracks = status.tracks;
  apState  = status.state;
  apVolume = status.volume;
  apMuted  = status.muted;

  // Enable/disable Audio tab based on whether there are audio tracks
  var hasAudio = apTracks.length > 0;
  var t5 = $('t5');
  if(t5){
    t5.classList.toggle('disabled', !hasAudio);
    // If tab was active but disc changed to no audio, go back to tab 0
    if(!hasAudio && curTab===1) showTab(0);
    // If audio appeared and we're on tab 0, don't auto-switch — user decides
  }

  // Update volume UI
  $('apVol').value     = apVolume;
  $('apVolLabel').textContent = apVolume;
  $('apBtnMute').classList.toggle('active', apMuted);
  $('apBtnMute').textContent  = apMuted ? '\u{1F507}' : '\u{1F50A}';

  // Update play button
  $('apBtnPlay').textContent = (apState===1)?'\u23F8':'\u25B6';
  $('apBtnPlay').classList.toggle('active', apState===1);

  // State badge
  var badges={0:'STOPPED',1:'PLAYING',2:'PAUSED'};
  var badgeCols={0:'#333',1:'#e94560',2:'#f5a623'};
  var sb=$('apStateBadge');
  sb.textContent=badges[apState]||'?';
  sb.style.background=badgeCols[apState]||'#333';
  sb.style.color=apState?'#fff':'var(--muted)';

  // Current track info
  var sub=status.sub||{};
  var curTrk=apTracks.find(function(t){return t.num===sub.track;});
  if(curTrk){
    apTrack=curTrk.num;
    $('apTrackName').textContent='Track '+curTrk.num+'  '+curTrk.title;
    $('apTimeRel').textContent=apFmt(sub.relM,sub.relS,sub.relF);
    $('apTimeAbs').textContent=apFmt(sub.absM,sub.absS,sub.absF);
    $('apTimeTotal').textContent=apSectToMs(curTrk.len);
    var prog=curTrk.len>0?(sub.relM*60*75+sub.relS*75+sub.relF)/curTrk.len*100:0;
    $('apProgFill').style.width=Math.min(100,prog)+'%';
  } else if(apTracks.length>0){
    $('apTrackName').textContent='Track '+apTracks[0].num+'  '+apTracks[0].title;
    $('apTimeRel').textContent='0:00';
    $('apTimeAbs').textContent='0:00';
    $('apTimeTotal').textContent=apSectToMs(apTracks[0].len);
    $('apProgFill').style.width='0%';
  }

  // Track list
  var tl=$('apTrackList');
  if(tl.children.length!==apTracks.length){
    tl.innerHTML='';
    apTracks.forEach(function(t){
      var row=document.createElement('div');
      row.className='ap-trow';
      row.id='aptr'+t.num;
      row.innerHTML='<span class="ap-tnum">'+t.num+'</span>'+
        '<span class="ap-tname">'+t.title+'</span>'+
        '<span class="ap-tdur">'+apSectToMs(t.len)+'</span>';
      row.onclick=(function(n){return function(){apPlayTrack(n);};})(t.num);
      tl.appendChild(row);
    });
  }
  // Highlight current track row
  apTracks.forEach(function(t){
    var row=$('aptr'+t.num);
    if(row) row.classList.toggle('playing', t.num===sub.track&&apState>0);
  });
}

function apPoll(){
  fetch('/api/audio/status').then(function(r){return r.json();}).then(apLoad).catch(function(){});
}

function apStartPoll(){
  if(!apPollTimer) apPollTimer=setInterval(apPoll,400);
  apPoll();
}
function apStopPoll(){
  // Keep a slow background poll (3s) so PC-driven changes are still detected
  // and the tab state badge / enable stays current
  if(apPollTimer){ clearInterval(apPollTimer); apPollTimer=null; }
  if(!apBgTimer)  apBgTimer=setInterval(apBgPoll,2000);logFetch();
}
var apBgTimer=null;
function apBgPoll(){
  // Lightweight background poll — just update tab state, don't repaint tracklist
  fetch('/api/audio/status').then(function(r){return r.json();}).then(function(s){
    if(!s) return;
    // Update tab enable/disable
    var hasAudio=s.track_count>0;
    var t5=$('t1');
    if(t5) t5.classList.toggle('disabled',!hasAudio);
    // If on audio tab, do full repaint
    if(hasAudio){
      if(apTracks.length===0) apLoad(s);
      else if(curTab===1) apLoad(s);
    }
  }).catch(function(){});
}

function apPlayTrack(n){
  fetch('/api/audio/play?track='+n).then(function(){apPoll();});
}
function apPlayPause(){
  if(apState===1) fetch('/api/audio/pause').then(function(){apPoll();});
  else if(apState===2) fetch('/api/audio/resume').then(function(){apPoll();});
  else {
    var t=apTracks.find(function(t){return t.num===apTrack;})||apTracks[0];
    if(t) apPlayTrack(t.num);
  }
}
function apStop(){ fetch('/api/audio/stop').then(function(){apPoll();}); }
function apNext(){
  var idx=apTracks.findIndex(function(t){return t.num===apTrack;});
  if(idx<apTracks.length-1) apPlayTrack(apTracks[idx+1].num);
}
function apPrev(){
  var idx=apTracks.findIndex(function(t){return t.num===apTrack;});
  if(idx>0) apPlayTrack(apTracks[idx-1].num);
  else if(idx===0) apSeekRel(0);
}
function apToggleMute(){
  fetch('/api/audio/mute').then(function(){apPoll();});
}
function apSetVolume(v){
  apVolume=parseInt(v);
  $('apVolLabel').textContent=v;
  fetch('/api/audio/volume?v='+v);
}
function apSeek(evt){
  var rect=$('apProg').getBoundingClientRect();
  var rel=(evt.clientX-rect.left)/rect.width;
  if(rel<0)rel=0;if(rel>1)rel=1;
  var t=apTracks.find(function(t){return t.num===apTrack;})||apTracks[0];
  if(!t) return;
  fetch('/api/audio/seek?track='+t.num+'&rel='+rel.toFixed(4));
}
function apSeekRel(rel){
  var t=apTracks.find(function(t){return t.num===apTrack;})||apTracks[0];
  if(t) fetch('/api/audio/seek?track='+t.num+'&rel='+rel);
}

function webAuthToggle(){
  $('webAuthFields').style.display=$('cfgWebAuth').value==='1'?'block':'none';
}
function showTab(n){curTab=n;for(var i=0;i<6;i++){var tEl=$('t'+i);if(tEl&&!tEl.classList.contains('disabled'))tEl.classList.toggle('active',i===n);var pEl=$('p'+i);if(pEl)pEl.classList.toggle('active',i===n);}if(n===1){apStartPoll();}else{apStopPoll();}if(n===0)cdLoadDir(cdPath);if(n===2)fmLoadDir(fmPath);if(n===3){if($("logLiveUpdate")&&$("logLiveUpdate").checked){if(!logTimer)logTimer=setInterval(logFetch,1000);logFetch();}}else{if(n!==3&&logTimer&&!($("logLiveUpdate")&&$("logLiveUpdate").checked)){clearInterval(logTimer);logTimer=null;}}if(n===3){if(!logTimer&&$("logLiveUpdate")&&$("logLiveUpdate").checked){logTimer=setInterval(logFetch,1000);}logFetch();}else if(n!==3){if(logTimer&&!($("logLiveUpdate")&&$("logLiveUpdate").checked)){clearInterval(logTimer);logTimer=null;}}if(n===4)loadSysinfo();if(n===5)cfgLoad();}

function cfgMsg(id,txt,isOk){var el=$(id);el.style.display='block';el.style.color=isOk?'var(--green)':'var(--red)';el.textContent=txt;}

function cfgDhcpToggle(){
  var dhcp=$('rDhcp').checked;
  $('staticFields').style.display=dhcp?'none':'block';
  // Pri prepnuti na Static: vzdy predvypln aktualne bezicimi DHCP hodnotami
  if(!dhcp){
    fetch('/api/sysinfo').then(function(r){return r.json();}).then(function(s){
      if(s.wifi_connected && s.wifi_ip){
        $('cfgIp').value  =s.wifi_ip  ||'';
        $('cfgMask').value=s.wifi_mask||'';
        $('cfgGw').value  =s.wifi_gw  ||'';
        $('cfgDns').value =s.wifi_dns ||'';
      }
    }).catch(function(){});
  }
}

function cfgScanWifi(){
  spin('scanSp',true);$('scanBtn').disabled=true;$('scanResults').style.display='none';
  fetch('/api/wifi/scan').then(function(r){return r.json();}).then(function(nets){
    spin('scanSp',false);$('scanBtn').disabled=false;
    var sel=$('ssidSelect');
    sel.innerHTML='<option value="">-- select network --</option>';
    // Sort by SSID then by RSSI descending
    nets.sort(function(a,b){
      if(a.ssid===b.ssid) return b.rssi-a.rssi;
      return a.ssid.localeCompare(b.ssid);
    });
        nets.forEach(function(n){
      var opt=document.createElement('option');
      opt.value=n.ssid;
      opt.dataset.band=n.band||'';
      opt.dataset.ch=n.ch||'';
      var bars=n.rssi>-50?'▂2▄▆█':n.rssi>-65?'▂▄▆_':n.rssi>-75?'▂▄__':'▂___';
      opt.textContent=n.ssid+' ['+n.band+' ch'+n.ch+'] '+bars+' '+n.rssi+'dBm'+(n.enc?' 🔒':'');
      sel.appendChild(opt);
    });
    $('scanResults').style.display='block';
  }).catch(function(){spin('scanSp',false);$('scanBtn').disabled=false;});
}

function cfgPickSsid(){
  var sel=$('ssidSelect');
  var v=sel.value;
  if(!v) return;
  $('cfgSsid').value=v;
  var opt=sel.options[sel.selectedIndex];
  var band=opt.dataset.band||'';
  var ch=opt.dataset.ch||'';
  if(band){
    var hint=$('cfgSsidHint');
    if(hint) hint.textContent='Selected: '+band+(ch?' Ch'+ch:'');
  }
}

function cfgScanCerts(){
  spin('certSp',true);
  var certBtn=$('scanCertBtn');
  if(certBtn) certBtn.disabled=true;
  var found=[];

  function scanDir(path, done){
    fetch('/api/ls?path='+encodeURIComponent(path)).then(function(r){return r.json();}).then(function(items){
      var subdirs=[];
      items.forEach(function(it){
        if(it.dir){
          subdirs.push(path==='/'?'/'+it.name:path+'/'+it.name);
        } else {
          var nm=it.name.toLowerCase();
          if(nm.endsWith('.pem')||nm.endsWith('.crt')||nm.endsWith('.cer')||nm.endsWith('.key')){
            var full=(path==='/'?'/'+it.name:path+'/'+it.name);
            found.push(full);
          }
        }
      });
      // Recurse into subdirs sequentially
      function nextDir(i){
        if(i>=subdirs.length){done();return;}
        scanDir(subdirs[i],function(){nextDir(i+1);});
      }
      nextDir(0);
    }).catch(function(){done();});
  }

  scanDir('/',function(){
    spin('certSp',false);
    if(certBtn) certBtn.disabled=false;
    found.sort();

    // Populate CA cert select (includes "none" option)
    var ca=$('cfgEapCa');
    ca.innerHTML='<option value="">(none &#8212; skip server verify)</option>';
    found.forEach(function(f){
      var o=document.createElement('option');
      o.value=f; o.textContent=f;
      ca.appendChild(o);
    });

    // Populate client cert select
    var cert=$('cfgEapCert');
    cert.innerHTML='<option value="">(not set)</option>';
    found.forEach(function(f){
      var o=document.createElement('option');
      o.value=f; o.textContent=f;
      cert.appendChild(o);
    });

    // Populate key select
    var key=$('cfgEapKey');
    key.innerHTML='<option value="">(not set)</option>';
    found.forEach(function(f){
      var o=document.createElement('option');
      o.value=f; o.textContent=f;
      key.appendChild(o);
    });

    // Restore previously saved paths
    eapRestoreSaved();

    // Show count feedback
    var sp=$('certSp');
    if(sp) sp.textContent=found.length?('Found '+found.length+' file(s)'):'No cert files found';
    setTimeout(function(){if(sp)sp.textContent='';},4000);
  });
}

function eapRestoreSaved(){
  // After scan, try to restore previously saved paths into selects
  ['cfgEapCa','cfgEapCert','cfgEapKey'].forEach(function(id){
    var sel=$(id); if(!sel) return;
    var saved=sel._savedVal||'';
    if(!saved) return;
    // Check if option exists
    for(var i=0;i<sel.options.length;i++){
      if(sel.options[i].value===saved){ sel.selectedIndex=i; return; }
    }
    // Not in list — add as manual entry
    var opt=document.createElement('option');
    opt.value=saved; opt.textContent=saved+' (saved)';
    sel.appendChild(opt); sel.value=saved;
  });
}

function eapPopulateSelects(allFiles){
  // CA: none + all cert/pem files
  var caExt=['.pem','.crt','.cer'];
  var keyExt=['.key','.pem'];
  var certExt=['.crt','.pem','.cer'];

  function fillSel(id,exts,noneLabel){
    var sel=$(id); if(!sel) return;
    var saved=sel._savedVal||sel.value||'';
    sel.innerHTML='<option value="">'+(noneLabel||'(none)')+'</option>';
    allFiles.filter(function(f){
      return exts.some(function(e){return f.toLowerCase().endsWith(e);});
    }).forEach(function(f){
      var opt=document.createElement('option');
      opt.value=f; opt.textContent=f; sel.appendChild(opt);
    });
    if(saved){
      for(var i=0;i<sel.options.length;i++){
        if(sel.options[i].value===saved){ sel.selectedIndex=i; return; }
      }
      var opt=document.createElement('option');
      opt.value=saved; opt.textContent=saved+' (saved)';
      sel.appendChild(opt); sel.value=saved;
    }
  }

  fillSel('cfgEapCa',   caExt,   '(none — disable server verification)');
  fillSel('cfgEapCert', certExt, '(select client certificate)');
  fillSel('cfgEapKey',  keyExt,  '(select private key)');
}

function eapScanSD(){
  spin('certSp',true); var _b=$('scanCertBtn');if(_b)_b.disabled=true;
  var allFiles=[];

  function scanDir(path, done){
    fetch('/api/ls?path='+encodeURIComponent(path))
      .then(function(r){return r.json();})
      .then(function(items){
        var subdirs=[];
        items.forEach(function(item){
          if(item.dir){ subdirs.push(item.name); }
          else { allFiles.push(path==='/'?'/'+item.name:path+'/'+item.name); }
        });
        var pending=subdirs.length;
        if(pending===0){ done(); return; }
        subdirs.forEach(function(d){
          var sub=path==='/'?'/'+d:path+'/'+d;
          // Skip system dirs
          if(sub.toLowerCase()==='/system volume information'){ pending--; if(pending===0) done(); return; }
          scanDir(sub,function(){ pending--; if(pending===0) done(); });
        });
      }).catch(function(){ done(); });
  }

  scanDir('/',function(){
    spin('certSp',false); var _b2=$('scanCertBtn');if(_b2)_b2.disabled=false;
    eapPopulateSelects(allFiles);
    var certFiles=allFiles.filter(function(f){
      return ['.pem','.crt','.cer','.key'].some(function(e){return f.toLowerCase().endsWith(e);});
    });
    if(certFiles.length===0){
      alert('No certificate files (.pem .crt .key) found on SD card.');
    }
  });
}

function cfgEapModeToggle(){
  var m=parseInt($('cfgEapMode').value||0);
  $('eapFields').style.display=m?'block':'none';
  $('eapPeapFields').style.display=(m===1)?'block':'none';
  $('eapTlsFields').style.display=(m===2)?'block':'none';
}

function eapCheckKeyFormat(){
  var sel=$('cfgEapKey');
  var val=sel?sel.value:'';
  var warn=$('eapKeyWarning'); if(!warn) return;
  // Show warning if .key file is selected (we can't check format here, just remind)
  warn.style.display=(val&&val.toLowerCase().endsWith('.key'))?'block':'none';
}

function cfgMdnsUpdate(v){
  var fqdn=(v||'espcd').toLowerCase().replace(/[^a-z0-9.-]/g,'').substring(0,63)||'espcd';
  // mDNS label is first part before dot
  var label=fqdn.split('.')[0]||'espcd';
  var fqdnEl=$('cfgFqdnPreview');
  var mdnsEl=$('cfgMdnsPreview');
  if(fqdnEl) fqdnEl.textContent=fqdn;
  if(mdnsEl) mdnsEl.textContent=label+'.local';
}

function httpsToggle(){
  var on=$("cfgHttpsEnabled")&&$("cfgHttpsEnabled").value==="1";
  var opts=$("httpsOptions"); if(opts) opts.style.display=on?"block":"none";
  var tls=$("tlsOptions"); if(tls) tls.style.display=on?"block":"none";
  var pr=$("httpsPortRow"); if(pr) pr.style.display=on?"flex":"none";
}

function httpsScanCerts(){
  spin("httpsScanSp",true);
  var btn=$("httpsScanBtn"); if(btn) btn.disabled=true;
  var found=[];
  function scanDir(path,done){
    fetch("/api/ls?path="+encodeURIComponent(path)).then(function(r){return r.json();}).then(function(items){
      var subs=[];
      items.forEach(function(it){
        if(it.dir){ subs.push(path==="/"?"/"+it.name:path+"/"+it.name); }
        else{ var nm=it.name.toLowerCase();
          if(nm.endsWith(".pem")||nm.endsWith(".crt")||nm.endsWith(".cer")||nm.endsWith(".key")){
            found.push(path==="/"?"/"+it.name:path+"/"+it.name); }}
      });
      function nd(i){ if(i>=subs.length){done();return;} scanDir(subs[i],function(){nd(i+1);}); }
      nd(0);
    }).catch(function(){done();});
  }
  scanDir("/",function(){
    spin("httpsScanSp",false); if(btn) btn.disabled=false;
    found.sort();
    ["cfgHttpsCert","cfgHttpsKey"].forEach(function(id){
      var sel=$(id); if(!sel) return;
      var saved=sel._savedVal||"";
      sel.innerHTML="<option value=''>(not set)</option>";
      found.forEach(function(f){ var o=document.createElement("option"); o.value=f; o.textContent=f; sel.appendChild(o); });
      if(saved){ for(var i=0;i<sel.options.length;i++){ if(sel.options[i].value===saved){sel.selectedIndex=i;break;} } }
    });
    var sp=$("httpsScanSp"); if(sp) sp.textContent=found.length?found.length+" file(s)":" No certs found";
    setTimeout(function(){if(sp)sp.textContent="";},3000);
  });
}

function cfgLoad(){
  fetch('/api/config/get').then(function(r){return r.json();}).then(function(c){
    $('cfgSsid').value=c.ssid||'';
    $('cfgPass').value='';
    $('cfgIp').value=c.ip||'';
    $('cfgMask').value=c.mask||'';
    $('cfgGw').value=c.gw||'';
    $('cfgDns').value=c.dns||'';
    var hn=c.hostname||'espcd'; $('cfgHostname').value=hn; cfgMdnsUpdate(hn);
    var em=parseInt(c.eapMode||0); $('cfgEapMode').value=em; cfgEapModeToggle();
    $('cfgEapId').value=c.eapIdentity||'';
    $('cfgEapUser').value=c.eapUsername||'';
    $('cfgEapPass').value='';
    // For selects: set value after scan, or store raw path for now
    $('cfgEapCa')._savedVal   = c.eapCaPath   || '';
    $('cfgEapCert')._savedVal = c.eapCertPath || '';
    $('cfgEapKey')._savedVal  = c.eapKeyPath  || '';
    $('cfgEapKPass').value = '';  // never pre-fill password
    $('cfgAudioModule').value = c.audioModule ? '1' : '0';
    $('cfgDosCompat').value = c.dosCompat ? '1' : '0';
    var heEl=$('cfgHttpsEnabled'); if(heEl){heEl.value=c.httpsEnabled?'1':'0'; httpsToggle();}
    var hcEl=$('cfgHttpsCert'); if(hcEl){hcEl._savedVal=c.httpsCert||''; if(c.httpsCert){var o=document.createElement('option');o.value=c.httpsCert;o.textContent=c.httpsCert+' (saved)';hcEl.appendChild(o);hcEl.value=c.httpsCert;}}
    var hkEl=$('cfgHttpsKey'); if(hkEl){hkEl._savedVal=c.httpsKey||''; if(c.httpsKey){var o=document.createElement('option');o.value=c.httpsKey;o.textContent=c.httpsKey+' (saved)';hkEl.appendChild(o);hkEl.value=c.httpsKey;}}
    var hkpEl=$('cfgHttpsKPass'); if(hkpEl) hkpEl.placeholder=c.httpsKPassSet?'(passphrase set — leave empty to keep)':'(only for encrypted keys)';
    var hpEl=$('cfgHttpPort'); if(hpEl) hpEl.value=c.httpPort||80;
    var hpsEl=$('cfgHttpsPort'); if(hpsEl) hpsEl.value=c.httpsPort||443;
    var tvEl=$('cfgTlsMinVer'); if(tvEl) tvEl.value=c.tlsMinVer||0;
    var tcEl=$('cfgTlsCiphers'); if(tcEl) tcEl.value=c.tlsCiphers||0;
    $('cfgWebAuth').value = c.webAuth ? '1' : '0';
    $('cfgWebUser').value = c.webUser || 'admin';
    $('cfgWebPass').value = '';
    webAuthToggle();
    eapRestoreSaved();
    if(c.dhcp){$('rDhcp').checked=true;}else{$('rStatic').checked=true;}
    cfgDhcpToggle();
  }).catch(function(){});
}

function cfgSave(){
  var dhcp=$('rDhcp').checked;
  var params=new URLSearchParams();
  params.set('ssid',$('cfgSsid').value.trim());
  if($('cfgPass').value.trim()) params.set('pass',$('cfgPass').value.trim());
  params.set('dhcp',dhcp?'true':'false');
  var hn=$('cfgHostname').value.trim().toLowerCase().replace(/[^a-z0-9.-]/g,'').substring(0,63);
  if(hn) params.set('hostname',hn);
  var em=parseInt($('cfgEapMode').value||0);
  params.set('eapMode',em);
  params.set('eapIdentity',$('cfgEapId').value.trim());
  params.set('eapUsername',$('cfgEapUser').value.trim());
  if($('cfgEapPass').value.trim()) params.set('eapPassword',$('cfgEapPass').value.trim());
  params.set('eapCaPath',$('cfgEapCa').value||'');
  params.set('eapCertPath',$('cfgEapCert').value||'');
  params.set('eapKeyPath',$('cfgEapKey').value||'');
  if($('cfgEapKPass').value.trim()) params.set('eapKeyPass',$('cfgEapKPass').value.trim());
  params.set('audioModule',$('cfgAudioModule').value);
  params.set('dosCompat',$('cfgDosCompat').value);
  params.set('httpsEnabled',$('cfgHttpsEnabled')?$('cfgHttpsEnabled').value:'0');
  params.set('httpsCert',$('cfgHttpsCert')?$('cfgHttpsCert').value:'');
  params.set('httpsKey',$('cfgHttpsKey')?$('cfgHttpsKey').value:'');
  var hkp=$('cfgHttpsKPass'); if(hkp&&hkp.value) params.set('httpsKPass',hkp.value);
  var hp=$('cfgHttpPort'); if(hp) params.set('httpPort',hp.value);
  var hps=$('cfgHttpsPort'); if(hps) params.set('httpsPort',hps.value);
  var tv=$('cfgTlsMinVer'); if(tv) params.set('tlsMinVer',tv.value);
  var tc=$('cfgTlsCiphers'); if(tc) params.set('tlsCiphers',tc.value);
  params.set('webAuth',$('cfgWebAuth').value);
  if($('cfgWebUser').value.trim()) params.set('webUser',$('cfgWebUser').value.trim());
  if($('cfgWebPass').value.trim()) params.set('webPass',$('cfgWebPass').value.trim());
  if(!dhcp){
    params.set('ip',$('cfgIp').value.trim());
    params.set('mask',$('cfgMask').value.trim());
    params.set('gw',$('cfgGw').value.trim());
    params.set('dns',$('cfgDns').value.trim());
  }
  spin('cfgActSp',true);$('cfgSaveBtn').disabled=true;
  fetch('/api/config/save?'+params.toString()).then(function(r){return r.text();}).then(function(m){
    spin('cfgActSp',false);$('cfgSaveBtn').disabled=false;
    cfgMsg('cfgActMsg',m,m.indexOf('OK')===0);
    log(m);
  }).catch(function(){spin('cfgActSp',false);$('cfgSaveBtn').disabled=false;cfgMsg('cfgActMsg','ERROR: request failed',false);});
}

function cfgReboot(){
  if(!confirm('Reboot the ESP32?'))return;
  spin('cfgActSp',true);
  fetch('/api/reboot').then(function(r){return r.text();}).then(function(m){
    spin('cfgActSp',false);
    cfgMsg('cfgActMsg','Rebooting... reconnect in ~5s',true);log(m);
  }).catch(function(){spin('cfgActSp',false);cfgMsg('cfgActMsg','Rebooting...',true);});
}

function cfgFactory(){
  if(!confirm('Factory reset will erase ALL settings and reboot. Continue?'))return;
  spin('cfgActSp',true);
  fetch('/api/factory').then(function(r){return r.text();}).then(function(m){
    spin('cfgActSp',false);
    cfgMsg('cfgActMsg','Factory reset done. Rebooting...',true);log(m);
  }).catch(function(){spin('cfgActSp',false);cfgMsg('cfgActMsg','Rebooting...',true);});
}

function sdCfgMsg(txt,ok){var el=$('sdCfgMsg');el.style.display='block';el.style.color=ok?'var(--green)':'var(--red)';el.textContent=txt;}
function sdSafeUnmount(){
  spin('sdCfgSp',true);$('sdUmBtn').disabled=true;$('sdMtBtn').disabled=true;
  fetch('/api/sd/unmount').then(function(r){return r.text();}).then(function(m){
    spin('sdCfgSp',false);$('sdUmBtn').disabled=false;$('sdMtBtn').disabled=false;
    sdCfgMsg(m, m.indexOf('OK')===0);
    log(m);loadStatus();
  }).catch(function(){spin('sdCfgSp',false);$('sdUmBtn').disabled=false;$('sdMtBtn').disabled=false;sdCfgMsg('ERROR: request failed',false);});
}
function sdRemount(){
  spin('sdCfgSp',true);$('sdUmBtn').disabled=true;$('sdMtBtn').disabled=true;
  fetch('/api/sd/mount').then(function(r){return r.text();}).then(function(m){
    spin('sdCfgSp',false);$('sdUmBtn').disabled=false;$('sdMtBtn').disabled=false;
    sdCfgMsg(m, m.indexOf('OK')===0);
    log(m);loadStatus();cdLoadDir('/');fmLoadDir('/');apBgTimer=setInterval(apBgPoll,2000);apBgPoll();setTimeout(apBgPoll,600);setTimeout(apBgPoll,1500);apPoll();logFetch();logFetch();
  }).catch(function(){spin('sdCfgSp',false);$('sdUmBtn').disabled=false;$('sdMtBtn').disabled=false;sdCfgMsg('ERROR: request failed',false);});
}

function siRow(tbl,label,val,cls){var tr=document.createElement('tr');var t1=document.createElement('td');t1.textContent=label;var t2=document.createElement('td');if(cls)t2.className=cls;t2.innerHTML=val;tbl.appendChild(tr);tr.append(t1,t2);}
function siBar(pct){return '<div class="si-bar"><div class="si-bar-i" style="width:'+Math.min(100,pct)+'%;background:'+(pct>85?'var(--red)':pct>65?'var(--yw, #e3b341)':'var(--bbg)')+'"></div></div>';}
function rssiBar(rssi){var pct=Math.max(0,Math.min(100,(rssi+100)*2));return '<div class="si-bar"><div class="si-bar-i" style="width:'+pct+'%"></div></div>';}
function fmtUp(s){var h=Math.floor(s/3600),m=Math.floor((s%3600)/60),sec=s%60;return (h?h+'h ':'')+(m?m+'m ':'')+sec+'s';}

function loadSysinfo(){
  spin('siSp',true);$('siBtn').disabled=true;
  fetch('/api/sysinfo').then(function(r){return r.json();}).then(function(s){
    spin('siSp',false);$('siBtn').disabled=false;
    $('siTime').textContent='Updated: '+new Date().toTimeString().slice(0,8);

    // WiFi
    var tw=$('siWifi');tw.innerHTML='';
    if(s.wifi_connected){
      var qcls=s.wifi_quality==='Excellent'?'si-ok':s.wifi_quality==='Good'?'si-ok':s.wifi_quality==='Fair'?'si-warn':'si-err';
      siRow(tw,'Status','Connected','si-ok');
      siRow(tw,'SSID',s.wifi_ssid);
      siRow(tw,'Band / Channel',(s.wifi_band||'?')+' &nbsp;·&nbsp; Ch '+s.wifi_channel);
      siRow(tw,'Signal',s.wifi_quality+' ('+s.wifi_rssi+' dBm)'+rssiBar(s.wifi_rssi),qcls);
      siRow(tw,'IP address',s.wifi_ip);
      siRow(tw,'Subnet mask',s.wifi_mask);
      siRow(tw,'Gateway',s.wifi_gw);
      siRow(tw,'DNS',s.wifi_dns);
      siRow(tw,'BSSID',s.wifi_bssid);
      siRow(tw,'Auth method', s.wifi_auth||'WPA2-Personal');
      // EAP security details
      if(s.eap_mode){
        siRow(tw,'EAP Identity', s.eap_identity||'(auto from cert CN)');
        if(s.eap_mode===2){
          siRow(tw,'Client cert', s.eap_cert||'(not set)');
          siRow(tw,'Client key',  s.eap_key ||'(not set)');
          siRow(tw,'Key passphrase', s.eap_kpass?'<span style="color:var(--ok)">&#10003; set</span>':'<span style="color:var(--muted)">(none)</span>');
          siRow(tw,'CA cert', s.eap_ca||'<span style="color:var(--muted)">(none — server not verified)</span>');
          if(s.cert_format) siRow(tw,'Cert format', s.cert_format);
          if(s.key_format)  siRow(tw,'Key format',  s.key_format);
        } else {
          siRow(tw,'CA cert', s.eap_ca||'<span style="color:var(--muted)">(none)</span>');
        }
      }
      siRow(tw,'Hostname / mDNS',(s.wifi_hostname||cfg.hostname||'?')+' &nbsp;&middot;&nbsp; <a href="http://'+(s.wifi_mdns||'?')+'" target="_blank" style="color:var(--accent)">http://'+(s.wifi_mdns||'?')+'</a>');
    } else {
      siRow(tw,'Status','Not connected','si-err');
    }

    // SD
    var ts=$('siSd');ts.innerHTML='';
    if(s.sd_ready){
      var pct=s.sd_total_mb>0?Math.round(s.sd_used_mb/s.sd_total_mb*100):0;
      siRow(ts,'Status','OK','si-ok');
      siRow(ts,'Total',s.sd_total_mb+' MB');
      siRow(ts,'Used',s.sd_used_mb+' MB ('+pct+'%)'+siBar(pct));
      siRow(ts,'Free',s.sd_free_mb+' MB');
    } else {
      siRow(ts,'Status','NOT FOUND','si-err');
    }

    // Image
    var ti=$('siImg');ti.innerHTML='';
    if(s.img_mounted){
      siRow(ti,'File',s.img_file);
      siRow(ti,'Size',s.img_size_mb+' MB ('+s.img_sectors+' sectors)');
      siRow(ti,'Sector format',s.img_rawsector+' B raw, header +'+s.img_hdroffset+' B');
      siRow(ti,'USB media',s.img_usbpresent?'Present':'Not present',s.img_usbpresent?'si-ok':'si-warn');
      siRow(ti,'Default on boot',s.img_default.length?s.img_default:'(none)');
    } else {
      siRow(ti,'Status','No disc mounted','si-warn');
      if(s.img_default.length) siRow(ti,'Default on boot',s.img_default);
    }

    // System
    var sy=$('siSys');sy.innerHTML='';
    siRow(sy,'Free heap',s.sys_heap+' B');
    siRow(sy,'Uptime',fmtUp(s.sys_uptime));
    siRow(sy,'CPU',s.sys_cpu_mhz+' MHz');
    siRow(sy,'Flash',s.sys_flash_mb+' MB');
    siRow(sy,'SDK',s.sys_sdk);
    dosCompatOn=s.dos_compat||false;
    siRow(sy,'HTTP port',s.httpPort||80);
    siRow(sy,'HTTPS',s.httpsEnabled?'<span style="color:var(--ok)">&#10003; Enabled (port 443)</span>':'<span style="color:var(--muted)">Disabled</span>');
    if(s.httpsEnabled){
      siRow(sy,'HTTPS port',s.httpsPort||443);
      if(s.httpsCert) siRow(sy,'TLS cert',s.httpsCert);
      var verNames=['TLS 1.2 only','TLS 1.2 + 1.3'];
      var cipNames=['Auto','Strong/GCM','Medium/CBC','All/Legacy'];
      siRow(sy,'TLS protocol',verNames[s.tlsMinVer||0]||'TLS 1.2 only');
      siRow(sy,'TLS ciphers',cipNames[s.tlsCiphers||0]||'Auto');
    }
    siRow(sy,'DOS compat',s.dos_compat?'<span style="color:var(--ok)">&#10003; Enabled (no USB re-enum on mount)</span>':'<span style="color:var(--muted)">Disabled</span>');
      // Audio section
      var ta=$('siAudio')&&$('siAudio').querySelector('tbody');
      if(ta){
        ta.innerHTML='';
        var audioModLabel=s.audio_module?'<span style="color:var(--ok)">&#10003; PCM5102 enabled</span>':'<span style="color:var(--muted)">disabled</span>';
        siRow(ta,'Module',audioModLabel);
        if(s.audio_module){
          siRow(ta,'I2S',s.i2s_ready?'<span style="color:var(--ok)">initialized</span>':'<span style="color:#e74c3c">init failed</span>');
          var astL={0:'STOPPED',1:'<span style="color:var(--ok)">PLAYING</span>',2:'<span style="color:#f5a623">PAUSED</span>'};
          siRow(ta,'Playback',astL[s.audio_state]||'?');
          siRow(ta,'Volume',s.audio_muted?'<span style="color:var(--muted)">MUTED</span>':(s.audio_volume||0)+'%');
          siRow(ta,'Audio tracks',(s.audio_tracks||0)+'');
        }
      }
    siRow(sy,'USB note','Full Speed 12 Mbit/s &rarr; ~600-900 KB/s real throughput');

  }).catch(function(){spin('siSp',false);$('siBtn').disabled=false;log('ERROR: sysinfo fetch failed.');});
}

var curDefault='', dosCompatOn=false, curMounted='';
function loadStatus(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(s){
    $('badge').textContent=s.mounted?'MOUNTED':'NONE';
    $('badge').className='badge '+(s.mounted?'on':'off');
    $('mname').textContent=s.mounted?s.file:'— nothing mounted —';
    curDefault=s['default']||'';
    curMounted=s.mounted?s.file:'';
    var db=$('defbar'),dbn=$('defbadge');
    if(s.mounted||curDefault.length){
      db.style.display='flex';
      dbn.textContent=curDefault.length?curDefault:'(none)';
      $('btnSetDef').style.display=s.mounted?'inline-flex':'none';
    } else {
      db.style.display='none';
    }
  }).catch(function(){});
}

function doSetDefault(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(s){
    if(!s.mounted){log('Nothing mounted.');return;}
    fetch('/api/setdefault?file='+encodeURIComponent(s.file)).then(function(r){return r.text();}).then(function(m){log(m);loadStatus();});
  });
}
function doClearDefault(){
  fetch('/api/cleardefault').then(function(r){return r.text();}).then(function(m){log(m);loadStatus();});
}

var cdPath='/',showBin=false;
function toggleBin(){showBin=!showBin;document.querySelector('.tog').classList.toggle('on',showBin);cdLoadDir(cdPath);}

function cdLoadDir(path){
  cdPath=path;renderBc('cdBc',path,cdLoadDir);spin('cdSp',true);
  $('cdBody').innerHTML='';$('cdNone').style.display='none';
  fetch('/api/ls?path='+encodeURIComponent(path)).then(function(r){return r.json();}).then(function(items){
    spin('cdSp',false);var tb=$('cdBody');
    if(path!=='/'){var up=path.lastIndexOf('/')>0?path.substring(0,path.lastIndexOf('/')):'/',tr=mkTr();tr.append(mkTd('ic','📁'),Object.assign(mkTd('nm d','.. (up)'),{onclick:function(){cdLoadDir(up);}}),mkTd('sz',''),mkTd('ac',''));tb.appendChild(tr);}
    var dirs=items.filter(function(i){return i.dir;}).sort(cmp);
    var files=items.filter(function(i){if(i.dir)return false;var nl=i.name.toLowerCase();if(nl.endsWith('.iso')||nl.endsWith('.cue'))return true;return nl.endsWith('.bin')&&showBin;}).sort(cmp);
    var shown=0;
    dirs.concat(files).forEach(function(item){
      var fp=jp(path,item.name),nl=item.name.toLowerCase();
      var isCue=nl.endsWith('.cue'),isIso=nl.endsWith('.iso');
      var icon=item.dir?'📁':(isCue?'📋':(isIso?'💿':'📀'));
      var tr=mkTr();
      var tdI=mkTd('ic',icon);
      var tdN=document.createElement('td');tdN.className='nm'+(item.dir?' d':'');
      var ns=document.createElement('span');ns.textContent=item.name;tdN.appendChild(ns);
      if(isCue){var cb=document.createElement('span');cb.className='cue-badge';cb.textContent='CUE';tdN.appendChild(cb);}
      if(fp===curDefault){var db2=document.createElement('span');db2.className='def-badge';db2.textContent='DEFAULT';tdN.appendChild(db2);}
      if(item.dir)tdN.onclick=(function(p){return function(){cdLoadDir(p);};})(fp);
      var tdS=mkTd('sz',item.dir?'':fmtSz(item.size));
      var tdA=mkTd('ac','');
      if(!item.dir){
        var mb=document.createElement('button');mb.className='btn g';mb.style.whiteSpace='nowrap';mb.textContent=isCue?'▶ Mount disc':'▶ Mount';
        mb.onclick=(function(p){return function(){doMount(p);};})(fp);tdA.appendChild(mb);
      }
      tr.append(tdI,tdN,tdS,tdA);tb.appendChild(tr);shown++;
    });
    if(shown===0&&path!=='/')$('cdNone').style.display='block';
  }).catch(function(){spin('cdSp',false);log('ERROR: cannot read directory.');});
}

function doMount(path){
  var doActualMount=function(){
    log('Mounting: '+path);
    fetch('/api/mount?file='+encodeURIComponent(path))
      .then(function(r){return r.text();})
      .then(function(m){log(m);loadStatus();})
      .catch(function(){log('ERROR: mount failed.');});
  };
  if(dosCompatOn&&curMounted){
    // DOS compat: eject → wait for OS to process → mount
    // Windows needs ~2s to fully process eject before new disc appears
    log('DOS compat: ejecting...');
    fetch('/api/umount').then(function(r){return r.text();})
      .then(function(m){
        log(m);
        curMounted='';
        loadStatus();
        setTimeout(doActualMount,2500);
      })
      .catch(function(){doActualMount();});
  } else {
    doActualMount();
  }
}
function doEject(){fetch('/api/umount').then(function(r){return r.text();}).then(function(m){log(m);loadStatus();});}

var fmPath='/';
function fmLoadDir(path){
  fmPath=path;renderBc('fmBc',path,fmLoadDir);spin('fmSp',true);
  $('fmBody').innerHTML='';$('fmNone').style.display='none';
  fetch('/api/ls?path='+encodeURIComponent(path)).then(function(r){return r.json();}).then(function(items){
    spin('fmSp',false);var tb=$('fmBody');
    if(path!=='/'){var up=path.lastIndexOf('/')>0?path.substring(0,path.lastIndexOf('/')):'/',tr=mkTr();tr.append(mkTd('ic','📁'),Object.assign(mkTd('nm d','.. (up)'),{onclick:function(){fmLoadDir(up);}}),mkTd('sz',''),mkTd('ac',''));tb.appendChild(tr);}
    if(!items||!items.length){if(path!=='/')$('fmNone').style.display='block';return;}
    var dirs=items.filter(function(i){return i.dir;}).sort(cmp);
    var files=items.filter(function(i){return!i.dir;}).sort(cmp);
    dirs.concat(files).forEach(function(item){
      var fp=jp(path,item.name),nl=item.name.toLowerCase();
      var isImg=!item.dir&&(nl.endsWith('.iso')||nl.endsWith('.bin')||nl.endsWith('.cue'));
      var icon=item.dir?'📁':(nl.endsWith('.cue')?'📋':(nl.endsWith('.iso')?'💿':(nl.endsWith('.bin')?'📀':'📄')));
      var tr=mkTr();
      var tdN=document.createElement('td');tdN.className='nm'+(item.dir?' d':'');tdN.textContent=item.name;
      if(item.dir)tdN.onclick=(function(p){return function(){fmLoadDir(p);};})(fp);
      var tdA=mkTd('ac','');
      if(isImg){var mb=document.createElement('button');mb.className='btn g';mb.style.whiteSpace='nowrap';mb.textContent='▶';mb.onclick=(function(p){return function(){doMount(p);};})(fp);tdA.appendChild(mb);}
      if(!item.dir){var dlb=document.createElement('a');dlb.className='btn gr';dlb.textContent='⬇';dlb.href='/api/download?path='+encodeURIComponent(fp);dlb.download=item.name;dlb.style.textDecoration='none';dlb.style.display='inline-flex';dlb.style.alignItems='center';dlb.title='Download';tdA.appendChild(dlb);}var db=document.createElement('button');db.className='btn r';db.textContent='🗑';
      db.onclick=(function(p,d){return function(){doDelete(p,d);};})(fp,item.dir);
      tdA.appendChild(db);tr.append(mkTd('ic',icon),tdN,mkTd('sz',item.dir?'':fmtSz(item.size)),tdA);tb.appendChild(tr);
    });
  }).catch(function(){spin('fmSp',false);log('ERROR: cannot read directory.');});
}

function doDelete(path,isDir){if(!confirm((isDir?'Delete folder "':'Delete file "')+path+'"'+(isDir?' and all contents?':'?')))return;log('Deleting: '+path);fetch('/api/delete?path='+encodeURIComponent(path)).then(function(r){return r.text();}).then(function(m){log(m);fmLoadDir(fmPath);loadStatus();}).catch(function(){log('ERROR: delete failed.');});}
function openMkdir(){$('mkName').value='';$('mkModal').classList.add('show');setTimeout(function(){$('mkName').focus();},80);}
function closeMkdir(){$('mkModal').classList.remove('show');}
function doMkdir(){var name=$('mkName').value.trim();if(!name)return;closeMkdir();var fp=(fmPath==='/'?'':fmPath)+'/'+name;fetch('/api/mkdir?path='+encodeURIComponent(fp)).then(function(r){return r.text();}).then(function(m){log(m);fmLoadDir(fmPath);}).catch(function(){log('ERROR: mkdir failed.');});}

var upQ=[],upIdx=0,upRun=false;
function openDz(){$('dz').style.display='block';}
function closeDz(){$('dz').style.display='none';$('pw').style.display='none';$('fi').value='';upQ=[];upIdx=0;}
function queueFiles(files){if(!files||!files.length)return;upQ=upRun?upQ.concat(Array.from(files)):Array.from(files);upIdx=upRun?upIdx:0;if(!upRun)uploadNext();}
function uploadNext(){
  if(upIdx>=upQ.length){upRun=false;log('All files uploaded.');fmLoadDir(fmPath);closeDz();return;}
  upRun=true;var file=upQ[upIdx];
  $('pw').style.display='block';$('pf').textContent='('+(upIdx+1)+'/'+upQ.length+') '+file.name;$('pbi').style.width='0%';$('pp').textContent='0%';
  var xhr=new XMLHttpRequest();xhr.open('POST','/api/upload?path='+encodeURIComponent(fmPath));
  xhr.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);$('pbi').style.width=p+'%';$('pp').textContent=p+'% ('+fmtSz(e.loaded)+'/'+fmtSz(e.total)+')';}};
  xhr.onload=function(){if(xhr.status===200)log('Uploaded: '+file.name);else log('ERROR: '+file.name);upIdx++;uploadNext();};
  xhr.onerror=function(){log('Network error: '+file.name);upIdx++;uploadNext();};
  var fd=new FormData();fd.append('file',file,file.name);xhr.send(fd);
}
(function(){var dz=$('dz'),tb=$('fmTbl');[dz,tb].forEach(function(el){el.addEventListener('dragover',function(e){e.preventDefault();openDz();dz.classList.add('ov');});el.addEventListener('dragleave',function(){dz.classList.remove('ov');});el.addEventListener('drop',function(e){e.preventDefault();dz.classList.remove('ov');if(e.dataTransfer.files.length)queueFiles(e.dataTransfer.files);});});})();

function renderBc(elId,path,fn){
  var bc=$(elId);bc.innerHTML='';
  var r=document.createElement('span');r.className='bcp';r.textContent='💾 /';r.onclick=function(){fn('/');};bc.appendChild(r);
  if(path!=='/')path.split('/').filter(Boolean).reduce(function(acc,p){
    acc+='/'+p;
    var sep=document.createElement('span');sep.className='bcs';sep.textContent=' / ';
    var sp=document.createElement('span');sp.className='bcp';sp.textContent=p;
    sp.onclick=(function(a){return function(){fn(a);};})(acc);
    bc.appendChild(sep);bc.appendChild(sp);return acc;
  },'');
}

function sdUnmount(){
  spin('sdCfgSp',true);
  fetch('/api/sd/unmount').then(function(r){return r.text();}).then(function(m){
    spin('sdCfgSp',false);
    var ok=m.indexOf('OK')===0;
    var el=$('sdCfgMsg');el.textContent=m;el.style.color=ok?'var(--ok)':'var(--err)';
    log(m);
  }).catch(function(){spin('sdCfgSp',false);});
}
function sdMount(){
  spin('sdCfgSp',true);
  fetch('/api/sd/mount').then(function(r){return r.text();}).then(function(m){
    spin('sdCfgSp',false);
    var ok=m.indexOf('OK')===0;
    var el=$('sdCfgMsg');el.textContent=m;el.style.color=ok?'var(--ok)':'var(--err)';
    log(m);
    if(ok){cdLoadDir('/');fmLoadDir('/');}
  }).catch(function(){spin('sdCfgSp',false);});
}

loadStatus();cdLoadDir('/');fmLoadDir('/');setInterval(loadStatus,5000);

function clearLog(){$('log').textContent='Log cleared.\n';}
</script>
<div id="mkModal" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,.7);z-index:1000;align-items:center;justify-content:center">
  <div style="background:var(--surface);border:1px solid var(--border);border-radius:var(--r);padding:20px;min-width:280px;max-width:420px;width:90%">
    <div class="ct" style="margin-bottom:12px">&#128193; New Folder</div>
    <input id="mkName" type="text" class="cfg-inp" style="width:100%;margin-bottom:12px" placeholder="Folder name" onkeydown="if(event.key==='Enter')doMkdir();if(event.key==='Escape')closeMkdir();">
    <div style="display:flex;gap:8px;justify-content:flex-end">
      <button class="btn" onclick="closeMkdir()">Cancel</button>
      <button class="btn b" onclick="doMkdir()">&#10003; Create</button>
    </div>
  </div>
</div>
</body>
</html>
)RAWHTML";
