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
.tab.disabled{opacity:.4;cursor:default}
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
<!-- Synchronous mode bootstrap: sets window.DEVICE_MODE before any inline JS runs.
     Script tag without async/defer = synchronous = blocks render until loaded.
     This means tabs and Status cards have correct state on first paint, no flash. -->
<script src="/api/mode.js"></script>
</head>
<body>
<div>
  <h1>&#128191; ESP32-S3 Virtual CD-ROM</h1>
  <p class="sub">MicroSD &#8594; USB CD-ROM for PC</p>
</div>
<div class="tabs">
  <div class="tab active" id="t0" onclick="showTab(0)">&#128249; CD-ROM</div>
  <div class="tab" id="t1" onclick="showTab(1)">&#127925; Audio</div>
  <div class="tab disabled" id="t6" onclick="showTab(6)" title="GoTek mode inactive — see Config">&#128190; GoTek</div>
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
  <div class="card" style="flex:1;display:flex;flex-direction:column;min-height:0">
    <div class="ap" id="audioPlayer" style="display:flex;flex-direction:column;flex:1;min-height:0;background:transparent;border:none;padding:0">
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
      <div class="ap-tracklist" id="apTrackList" style="flex:1;min-height:0;max-height:none"></div>
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

    <!-- Disc Image — hidden in GoTek mode (no disc) -->
    <div class="card" id="siImgCard">
      <div class="ct">&#128191; Disc Image</div>
      <table class="si-tbl" id="siImg"></table>
    </div>

    <!-- Audio — hidden in GoTek mode (I2S not initialised) -->
    <div class="card" id="siAudioCard">
      <div class="ct">&#127925; Audio</div>
      <table class="si-tbl" id="siAudio"><tbody></tbody></table>
    </div>

    <!-- GoTek — visible only in GoTek mode, right after Audio card -->
    <div class="card" id="siGotekCard" style="display:none">
      <div class="ct">&#128190; GoTek Floppy Emulator</div>
      <table class="si-tbl" id="siGotek"></table>
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
      <div class="cfg-row" style="margin-top:8px">
        <label class="cfg-lbl">TX power</label>
        <select class="cfg-inp" id="cfgWifiTxPower" style="width:100%">
          <option value="8">2 dBm &mdash; minimum</option>
          <option value="20">5 dBm</option>
          <option value="40">10 dBm &mdash; default</option>
          <option value="60">15 dBm</option>
          <option value="80">20 dBm &mdash; maximum (ESP32-S3 factory)</option>
        </select>
      </div>
      <div style="font-size:.75rem;color:var(--muted);margin-top:4px">
        &#9432; Applied immediately without reboot. Lower power reduces RF coupling into PCM5102 analog output. Default is 10 dBm.
      </div>
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
    <div class="card" id="cfgCardAudio">
      <div class="ct">&#127925; Audio Module</div>
      <div class="cfg-row">
        <label class="cfg-lbl">PCM5102 I2S module</label>
        <select class="cfg-inp" id="cfgAudioModule" style="width:100%">
          <option value="0">Disabled (no hardware connected)</option>
          <option value="1">GY-PCM5102 I2S &#8212; GPIO 8/15/16</option>
        </select>
      </div>
      <div style="font-size:.75rem;color:var(--muted);margin-top:6px">
        &#9432; Requires reboot. BCK&#8594;GPIO8 &nbsp;WS&#8594;GPIO15 &nbsp;DIN&#8594;GPIO16 &nbsp;VCC&#8594;3V3.
      </div>
      <!-- Win98 Stop/Pause detection -->
      <div style="margin-top:16px;padding-top:12px;border-top:1px solid var(--border)">
        <div class="cfg-row">
          <label class="cfg-lbl">&#9200; Win98 Stop/Pause</label>
          <div style="display:flex;align-items:center;gap:8px">
            <input type="number" class="cfg-inp" id="cfgWin98StopMs" min="0" max="9999" step="100" style="width:90px" placeholder="ms">
            <span style="color:var(--muted);font-size:0.82em">ms &nbsp;(0&nbsp;=&nbsp;off)</span>
          </div>
        </div>
        <div style="font-size:.75rem;color:var(--muted);margin-top:6px">
          &#9432; Detekce Stop/Pause Win98 CDPlayer. Pokud READ_SUB_CHANNEL polling ustane na tuto dobu, PCM5102 se zastaví. Default: 1200 ms.
        </div>
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

    <!-- Device Mode -->
    <div class="card">
      <div class="ct">&#128270; Device Mode</div>
      <div class="cfg-row">
        <label class="cfg-lbl">USB device profile</label>
        <select class="cfg-inp" id="cfgDeviceMode" style="width:100%" onchange="deviceModeChanged()">
          <option value="0">&#128249; CD-ROM emulator (default)</option>
          <option value="1">&#128190; GoTek floppy emulator (FlashFloppy)</option>
        </select>
      </div>
      <div id="cfgGotekDirRow" style="display:none;margin-top:10px">
        <div class="cfg-row">
          <label class="cfg-lbl">GoTek image folder</label>
          <input class="cfg-inp" id="cfgGotekDir" type="text" placeholder="/gotek" style="width:100%">
        </div>
        <div style="font-size:.75rem;color:var(--tx2);margin-top:6px">
          Place <code>.img</code> / <code>.adf</code> / <code>.hfe</code> files in this SD folder.
          FlashFloppy sees them as a FAT32 USB drive and navigates by filename order.
        </div>
      </div>
      <div id="cfgDeviceModeNote0" style="margin-top:8px;font-size:.75rem;background:rgba(56,139,253,.1);border:1px solid rgba(56,139,253,.3);border-radius:4px;padding:8px 12px">
        &#10003; <b>CD-ROM mode:</b> Full virtual CD-ROM — ISO/BIN/CUE, audio playback, DOS compat, SCSI commands.
      </div>
      <div id="cfgDeviceModeNote1" style="display:none;margin-top:8px;font-size:.75rem;background:rgba(210,153,34,.15);border:1px solid rgba(210,153,34,.4);border-radius:4px;padding:8px 12px">
        &#128190; <b>GoTek mode:</b> ESP32 acts as a USB flash drive for GoTek/FlashFloppy.
        Audio, CD-ROM, DOS compat and disc mounting are <b>disabled</b>.<br>
        &#9888; Requires <b>reboot</b> to take effect (USB descriptor changes at boot).
      </div>
    </div>

    <!-- DOS Compatibility Mode -->
    <div class="card" id="cfgCardDos">
      <div class="ct">&#128190; DOS Compatibility Mode</div>
      <div class="cfg-row">
        <label class="cfg-lbl">Mount behavior</label>
        <select class="cfg-inp" id="cfgDosCompat" style="width:100%" onchange="dosCompatChanged()">
          <option value="0">Normal (USB re-enumeration on mount/eject)</option>
          <option value="1">DOS compatible (UNIT ATTENTION only, no re-enum)</option>
        </select>
      </div>
      <div style="font-size:.75rem;color:var(--muted);margin-top:6px">
        &#9432; DOS compat prevents the device from disappearing on disc swap.
        Uses UNIT&nbsp;ATTENTION&nbsp;0x28 instead of USB re-enumeration.
        Automatically enabled when a DOS driver is selected above.
      </div>
      <div id="dosDriverSection" style="display:none;margin-top:12px">
        <div class="cfg-row">
          <label class="cfg-lbl">DOS CD-ROM driver</label>
          <select class="cfg-inp" id="cfgDosDriver" onchange="dosDriverChanged()">
            <option value="0">Generic &mdash; any ASPI driver [audio supported]</option>
            <option value="1">USBCD2.SYS &mdash; TEAC CD-56E [BROKEN &ndash; needs INT 13h hook]</option>
            <option value="2">ESPUSBCD.SYS &mdash; MATSHITA CR-572 [audio via CDP.COM]</option>
            <option value="3">DI1000DD.SYS + usbaspi1.sys &mdash; data only, no audio</option>
          </select>
        </div>
        <div id="dosDriverNote0" style="display:none;margin-top:8px;font-size:.75rem;background:rgba(46,204,113,.1);border:1px solid rgba(46,204,113,.3);border-radius:4px;padding:8px 12px">
          &#10003; <b>Generic mode:</b> No specific INQUIRY identity &mdash; works with any ASPI-compatible DOS driver.<br>
          Audio: READ&nbsp;TOC, PLAY, STOP, PAUSE, READ&nbsp;SUB-CHANNEL fully supported via SCSI.<br>
          Use with any standard USBASPI (e.g. <code>usbaspi1.sys</code> or <code>usbaspi2.sys</code>) + MSCDEX.<br>
          CONFIG.SYS: <code>usbaspi1.sys /w /v</code> + <code>MSCDEX.EXE /D:USBCD0</code>
        </div>
        <div id="dosDriverNote1" style="display:none;margin-top:8px;font-size:.75rem;background:rgba(241,196,15,.1);border:1px solid rgba(241,196,15,.3);border-radius:4px;padding:8px 12px">
          &#9888; <b>USBCD2.SYS mode:</b> Device identifies as <code>TEAC CD-56E</code>.<br>
          <b style="color:#e74c3c">Driver communication is broken:</b> USBCD2 uses INT&nbsp;13h AH=0x50 (non-standard) &mdash; standard USBASPI does not provide this hook.<br>
          Audio SCSI commands (READ&nbsp;TOC, PLAY, SUB-CHANNEL) are handled by firmware but unreachable via broken driver.<br>
          <b>Not recommended</b> &mdash; use Generic or ESPUSBCD.SYS instead.
        </div>
        <div id="dosDriverNote2" style="display:none;margin-top:8px;font-size:.75rem;background:rgba(52,152,219,.1);border:1px solid rgba(52,152,219,.3);border-radius:4px;padding:8px 12px">
          &#10003; <b>ESPUSBCD.SYS (Panasonic) &mdash; RECOMMENDED:</b> Device: <code>MATSHITA CD-ROM CR-572</code>, SCSI-2.<br>
          Audio: PLAY, STOP, RESUME, READ_SUB-CHANNEL via MSCDEX IOCTL.<br>
          Communicates via <b>SCSIMGR$ DOS device</b> &mdash; works with usbaspi1.sys/usbaspi2.sys.<br>
          <b style="color:#e67e22">CD Player compatibility:</b><br>
          &bull; <b style="color:#e74c3c">cdplayer.exe fails with original USBCD1.SYS:</b> missing IOCTL OUT sf3 &rarr; <code>error 3</code><br>
          &bull; <b style="color:#27ae60">CDP.COM works:</b> tolerates missing sf3, continues to PLAY/STOP/SEEK<br>
          &bull; <b style="color:#27ae60">ESPUSBCD.SYS:</b> custom DOS driver — full IOCTL audio support, cdplayer.exe works<br>
          CONFIG.SYS: <code>usbaspi2.sys /w /v</code> + <code>ESPUSBCD.SYS /D:USBCD0</code>
        </div>
        <div id="dosDriverNote3" style="display:none;margin-top:8px;font-size:.75rem;background:rgba(46,204,113,.1);border:1px solid rgba(46,204,113,.3);border-radius:4px;padding:8px 12px">
          &#128190; <b>DI1000DD.SYS (NOVAC) + USBASPI 2.20:</b> Data-only USB storage &mdash; <b>no audio</b>.<br>
          SD card files accessible as regular DOS drive letter (FAT).<br>
          Use <b>usbaspi1.sys</b> as ASPI layer (Panasonic v2.20): <code>usbaspi1.sys /w /v</code> + <code>DI1000DD.SYS</code><br>
          No MSCDEX needed. DI1000DD accepts device type 0x05 (CD-ROM) natively.
        </div>
      </div><!-- end dosDriverSection -->
    </div><!-- end DOS card -->

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

<!-- ═══ GoTek Panel ══════════════════════════════════════════════════ -->
<div class="panel" id="p6">

  <div id="gkCdromNotice" style="display:none">
    <div class="card">
      <div style="text-align:center;padding:32px 16px">
        <div style="font-size:3rem;margin-bottom:12px">&#128190;</div>
        <div style="font-size:1rem;color:var(--tx);margin-bottom:8px">GoTek mode not active</div>
        <div style="font-size:.82rem;color:var(--tx2);margin-bottom:20px">
          Currently running <b>CD-ROM emulation</b>. To use GoTek floppy emulation, switch mode in Config and restart.
        </div>
        <button class="btn b" onclick="showTab(5)">&#9881; Go to Config &rarr; Device Mode</button>
      </div>
    </div>
  </div>

  <div id="gkActivePanel" style="display:none;flex-direction:column;gap:clamp(6px,1vw,12px);flex:1;min-height:0">

    <!-- Current image — mirrors CD "Current medium" card -->
    <div class="card">
      <div class="ct">&#128190; GoTek Floppy Emulator</div>
      <div class="sbar" style="margin-bottom:6px">
        <span id="gkModeBadge" class="badge off">CD-ROM mode</span>
        <span id="gkReadyBadge" class="badge on" style="display:none">FS ready</span>
        <span id="gkUsbBadge" class="badge def" style="display:none;cursor:pointer" onclick="gkShowUsbDebug()" title="Click for USB diagnostic">USB info</span>
      </div>
      <div class="ct">&#128190; Current image</div>
      <div class="sbar">
        <span class="badge off" id="gkCurBadge">NONE</span>
        <span class="mname" id="gkCurName">&#8212; no slot selected &#8212;</span>
        <button class="btn gr" onclick="gkSelectSlot(-1)" title="GoTek raw mode — all slots for GoTek hardware">&#128190; GoTek mode</button>
      </div>
      <div id="gkUsbDebugBox" style="display:none;margin-top:8px;font-size:.72rem;background:rgba(0,0,0,.3);border:1px solid var(--border2);border-radius:4px;padding:8px 10px;color:var(--tx2)">
        Loading USB debug info&hellip;
      </div>
    </div>

    <!-- Image browser — mirrors CD "Browse SD" card -->
    <div class="card" style="flex:1;display:flex;flex-direction:column;min-height:0;overflow:hidden">
      <div class="ct" style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:6px">
        <span>&#128190; Image files &mdash; <b id="gkDir">/gotek</b> <span class="sp" id="gkSp"></span></span>
        <div style="display:flex;gap:6px;align-items:center">
          <button class="btn b" onclick="gkRefresh()" id="gkRefBtn">&#8635; Refresh</button>
          <button class="btn g" onclick="gkShowCreate()">&#43; New</button>
          <button class="btn b" onclick="gkShowUpload()">&#8679; Upload</button>
        </div>
      </div>
      <!-- Create row -->
      <div id="gkCreateRow" style="display:none;padding:8px 0 6px;gap:8px;flex-wrap:wrap;align-items:center;border-bottom:1px solid var(--border2)">
        <input type="text" id="gkNewName" placeholder="DSKA0000.img" class="cfg-inp" style="flex:1;min-width:140px">
        <select id="gkNewType" class="cfg-inp" style="width:130px">
          <option value="144">1.44 MB (PC)</option>
          <option value="720">720 KB (PC)</option>
          <option value="360">360 KB</option>
        </select>
        <button class="btn g" onclick="gkCreate()">&#43; Create</button>
        <span class="sp" id="gkCrSp"></span>
        <span id="gkCrMsg" style="font-size:.8rem"></span>
      </div>
      <!-- Upload row -->
      <div id="gkUploadRow" style="display:none;padding:8px 0 6px;gap:8px;align-items:center;flex-wrap:wrap;border-bottom:1px solid var(--border2)">
        <input type="file" id="gkUpFile" accept=".img,.ima,.adf,.hfe,.dsk,.st,.d81,.d64" style="flex:1;font-size:.8rem;color:var(--tx);background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:4px 8px">
        <button class="btn g" onclick="gkUpload()">&#8679; Upload</button>
        <span id="gkMsg" style="font-size:.8rem"></span>
        <div id="gkUpProg" style="display:none;width:100%">
          <div style="height:4px;background:var(--border);border-radius:2px">
            <div id="gkUpBar" style="height:4px;background:var(--green);border-radius:2px;width:0%;transition:width .2s"></div>
          </div>
          <span id="gkUpMsg" style="font-size:.75rem;color:var(--tx2)"></span>
        </div>
      </div>
      <div class="tbl-wrap" style="flex:1">
        <table id="gkTbl" style="width:100%">
          <thead><tr><th class="ic"></th><th>Filename</th><th class="sz">Size</th><th class="ac"></th></tr></thead>
          <tbody id="gkTbody"><tr><td colspan="4" style="color:var(--tx2);padding:12px">Loading&hellip;</td></tr></tbody>
        </table>
      </div>
    </div>

  </div>

</div>

<!-- ═══ IMG Browser Modal ════════════════════════════════════════════ -->
<div id="imgModal" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,.78);z-index:100;display:none;align-items:center;justify-content:center;overflow:hidden">
  <div style="background:var(--surface);border:1px solid var(--border);border-radius:10px;width:min(calc(100% - clamp(12px,3vw,40px)),960px);height:calc(100% - clamp(12px,3vw,40px));display:flex;flex-direction:column;overflow:hidden">
    <!-- Header -->
    <div style="padding:10px 14px;border-bottom:1px solid var(--border);display:flex;align-items:center;gap:8px">
      <span id="imgModalTitle" style="color:#79c0ff;font-size:.9rem;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">&#128190;</span>
      <button class="btn gr" onclick="imgModalClose()" style="padding:3px 12px;font-size:.8rem">&#10005; Close</button>
    </div>
    <!-- Breadcrumb -->
    <div id="imgBreadcrumb" style="padding:5px 14px;font-size:.75rem;color:var(--tx2);border-bottom:1px solid var(--border2);background:rgba(0,0,0,.15)"></div>
    <!-- Toolbar: upload + mkdir -->
    <div style="padding:7px 14px;border-bottom:1px solid var(--border2);display:flex;gap:8px;align-items:center;flex-wrap:wrap">
      <input type="file" id="imgUpFile" style="flex:1;min-width:150px;font-size:.75rem;color:var(--tx);background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:3px 6px" onchange="imgCheckFileSize()">
      <button class="btn g" id="imgUpBtn" onclick="imgUpload()" style="font-size:.8rem;white-space:nowrap">&#8679; Write file</button>
      <span id="imgUpMsg" style="font-size:.72rem;color:var(--tx2)"></span>
      <span style="flex:1"></span>
      <input type="text" id="imgMkdirName" placeholder="Folder name" style="width:130px;font-size:.75rem;color:var(--tx);background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:3px 6px">
      <button class="btn gr" onclick="imgMkdir()" style="font-size:.8rem;white-space:nowrap">&#128193; New folder</button>
      <span id="imgMkdirMsg" style="font-size:.72rem;color:var(--tx2)"></span>
    </div>
    <!-- File list -->
    <div style="overflow-y:auto;overflow-x:hidden;flex:1;min-height:0">
      <table style="width:100%;border-collapse:collapse;font-size:.85rem">
        <thead><tr style="position:sticky;top:0;background:var(--surface);z-index:1;border-bottom:1px solid var(--border2)">
          <th class="ic"></th>
          <th style="text-align:left;padding:6px 6px;color:var(--tx2);font-weight:normal">Name</th>
          <th class="sz" style="padding:6px 10px;color:var(--tx2);font-weight:normal">Size</th>
          <th class="ac"></th>
        </tr></thead>
        <tbody id="imgTbody"><tr><td colspan="4" style="padding:16px;color:var(--tx2)">Loading&hellip;</td></tr></tbody>
      </table>
    </div>
    <!-- Footer: free space with visual bar -->
    <div style="padding:6px 14px;border-top:1px solid var(--border2)">
      <div style="display:flex;align-items:center;gap:8px;margin-bottom:4px">
        <span id="imgFooter" style="font-size:.72rem;color:var(--tx2);flex:1"></span>
        <span id="imgFreeWarn" style="font-size:.72rem;color:var(--red);display:none">&#9888; Not enough space</span>
      </div>
      <div style="height:5px;background:var(--border2);border-radius:3px;overflow:hidden">
        <div id="imgSpaceBar" style="height:100%;background:var(--green);border-radius:3px;width:0%;transition:width .3s"></div>
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
  fetch('/api/sysinfo').then(function(r){return r.json();}).then(function(s){
    dosCompatOn=s.dos_compat||false;
    gkApplyTabVisibility(s.device_mode===1);
  }).catch(function(){});
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
// apPendingTrack: track the user just clicked — held for up to 1.5 s while the firmware
// is still reporting the old track.  Prevents apLoad() from clobbering the optimistic
// highlight with stale server data and makes the UI feel instant.
var apPendingTrack=0, apPendingTime=0;

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

  // Pending-track guard: if the user clicked a track recently and the server hasn't
  // caught up yet, keep the optimistic highlight instead of reverting to the old track.
  var pendingActive = apPendingTrack>0 && (Date.now()-apPendingTime)<1500;
  if(pendingActive && sub.track===apPendingTrack){ apPendingTrack=0; pendingActive=false; }
  var displayTrack = pendingActive ? apPendingTrack : sub.track;

  var curTrk=apTracks.find(function(t){return t.num===displayTrack;});
  if(curTrk){
    if(!pendingActive) apTrack=curTrk.num;  // don't clobber apTrack while pending
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
  // Highlight current track row (use displayTrack so pending clicks stay highlighted)
  apTracks.forEach(function(t){
    var row=$('aptr'+t.num);
    if(row) row.classList.toggle('playing', t.num===displayTrack&&apState>0);
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
  // Optimistic UI: highlight immediately, keep for up to 1.5 s
  // even if the server still reports the old track during the switch.
  apPendingTrack=n; apPendingTime=Date.now();
  // Optimistic UI update — highlight immediately so user gets instant feedback.
  // Without this the 400ms background poll returns stale state and the row
  // appears unresponsive, causing users to click repeatedly.
  apTrack=n;
  apTracks.forEach(function(t){
    var row=$('aptr'+t.num);
    if(row) row.classList.toggle('playing', t.num===n);
  });
  $('apProgFill').style.width='0%';
  $('apStateBadge').textContent='PLAYING';
  $('apStateBadge').style.background='#e94560';
  $('apStateBadge').style.color='#fff';
  var trk=apTracks.find(function(t){return t.num===n;});
  if(trk){ $('apTrackName').textContent='Track '+trk.num+'  '+trk.title; $('apTimeTotal').textContent=apSectToMs(trk.len); }
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
  // Optimistic progress bar update
  $('apProgFill').style.width=(rel*100).toFixed(1)+'%';
  // seek API now calls audioPlay() internally — works from stopped/paused/playing
  fetch('/api/audio/seek?track='+t.num+'&rel='+rel.toFixed(4)).then(function(){apPoll();});
}
function apSeekRel(rel){
  var t=apTracks.find(function(t){return t.num===apTrack;})||apTracks[0];
  if(t) fetch('/api/audio/seek?track='+t.num+'&rel='+rel).then(function(){apPoll();});
}

function dosCompatChanged(){
  var v=parseInt($('cfgDosCompat').value||0);
  var sec=$('dosDriverSection');
  if(sec) sec.style.display=(v===1)?'block':'none';
  if(v===0 && $('cfgDosDriver')) $('cfgDosDriver').value='0';
  dosDriverChanged();
}
function dosDriverChanged(){
  var v=parseInt($('cfgDosDriver').value||0);
  var n0=$('dosDriverNote0'), n1=$('dosDriverNote1'), n2=$('dosDriverNote2');
  if(n0) n0.style.display=(v===0)?'block':'none';
  if(n1) n1.style.display=(v===1)?'block':'none';
  if(n2) n2.style.display=(v===2)?'block':'none';
  var n3=$('dosDriverNote3'); if(n3) n3.style.display=(v===3)?'block':'none';
  // All specific driver modes require DOS compat ON
  if(v>=1) $('cfgDosCompat').value='1';
}
function webAuthToggle(){
  $('webAuthFields').style.display=$('cfgWebAuth').value==='1'?'block':'none';
}
function cfgApplyMode(isGotek){
  // CD-ROM only cards — hidden in GoTek mode
  var cdCards=['cfgCardAudio','cfgCardDos'];
  cdCards.forEach(function(id){
    var el=$(id); if(el) el.style.display=isGotek?'none':'';
  });
  // Placeholder: add GoTek-only card IDs here when created
  // var gkCards=['cfgCardGotek'];
  // gkCards.forEach(function(id){ var el=$(id); if(el) el.style.display=isGotek?'':'none'; });
}
function deviceModeChanged(){
  var v=parseInt($('cfgDeviceMode')?$('cfgDeviceMode').value:0);
  var n0=$('cfgDeviceModeNote0'),n1=$('cfgDeviceModeNote1'),gr=$('cfgGotekDirRow');
  if(n0) n0.style.display=(v===0)?'block':'none';
  if(n1) n1.style.display=(v===1)?'block':'none';
  if(gr) gr.style.display=(v===1)?'block':'none';
  cfgApplyMode(v===1);
}
// ── GoTek tab functions ──────────────────────────────────────────────────────
var gkIsGotek=false;
function gkApplyTabVisibility(isGotek){
  var t0=$('t0'),t1=$('t1'),t6=$('t6');
  if(isGotek){
    if(t0){t0.classList.add('disabled');t0.title='CD-ROM inactive in GoTek mode';}
    if(t1){t1.classList.add('disabled');t1.title='Audio inactive in GoTek mode';}
    if(t6){t6.classList.remove('disabled');t6.title='';}
    if(curTab===0||curTab===1) showTab(6);
  } else {
    if(t0){t0.classList.remove('disabled');t0.title='';}
    if(t1){t1.classList.remove('disabled');t1.title='';}
    if(t6){t6.classList.add('disabled');t6.title='GoTek mode inactive — see Config';}
    if(curTab===6) showTab(0);
  }
  // Status cards — no showTab side effect here, safe to call at any time
  var ic=$('siImgCard'),ac=$('siAudioCard');
  if(ic) ic.style.display = isGotek ? 'none' : '';
  if(ac) ac.style.display = isGotek ? 'none' : '';
}
// ── Apply mode immediately using DEVICE_MODE from /api/mode.js ──────────────
(function(){
  var m = (typeof window.DEVICE_MODE !== 'undefined') ? window.DEVICE_MODE : 0;
  var t0=$('t0'),t1=$('t1'),t6=$('t6');
  var ic=$('siImgCard'),ac=$('siAudioCard');
  if(m===1){
    if(t0){t0.classList.add('disabled');t0.classList.remove('active');t0.title='CD-ROM inactive in GoTek mode';}
    if(t1){t1.classList.add('disabled');t1.classList.remove('active');t1.title='Audio inactive in GoTek mode';}
    if(t6){t6.classList.remove('disabled');t6.classList.add('active');t6.title='';}
    var p0=$('p0'),p6=$('p6');
    if(p0) p0.classList.remove('active');
    if(p6) p6.classList.add('active');
    if(ic) ic.style.display='none';
    if(ac) ac.style.display='none';
    curTab=6;
    // Load GoTek data — gkInit is hoisted so it's available here
    setTimeout(function(){gkInit();}, 0);
  } else {
    if(t0){t0.classList.remove('disabled');t0.title='';}
    if(t1){t1.classList.remove('disabled');t1.title='';}
    if(t6){t6.classList.add('disabled');t6.title='GoTek mode inactive — see Config';}
  }
})();
function gkInit(){
  fetch('/api/sysinfo').then(function(r){return r.json();}).then(function(s){
    gkIsGotek=(s.device_mode===1);
    gkApplyTabVisibility(gkIsGotek);
    var notice=$('gkCdromNotice'), panel=$('gkActivePanel');
    if(notice) notice.style.display=gkIsGotek?'none':'block';
    if(panel)  panel.style.display=gkIsGotek?'flex':'none';
    var badge=$('gkModeBadge');
    if(badge){badge.textContent=gkIsGotek?'GoTek mode active':'CD-ROM mode';badge.className='badge '+(gkIsGotek?'on':'off');}
    var rbadge=$('gkReadyBadge');
    if(rbadge){
      rbadge.style.display=gkIsGotek?'inline':'none';
      rbadge.textContent=s.gotek_ready?'FS ready':'FS building';
      rbadge.className='badge '+(s.gotek_ready?'on':'off');
    }
    var dirEl=$('gkDir'); if(dirEl) dirEl.textContent=s.gotek_dir||'/gotek';
    if(gkIsGotek){ gkLoadList(); gkShowUsbBadge(true); }
    else gkShowUsbBadge(false);
  }).catch(function(){
    // Even on error, show panels based on last known state
    var notice=$('gkCdromNotice'),panel=$('gkActivePanel');
    if(notice) notice.style.display='none';
    if(panel)  panel.style.display='flex';
  });
}
var gkSelIdx=-1;
var gkFilesList=[];

function gkShowUsbDebug(){
  var box=$('gkUsbDebugBox');
  if(!box) return;
  if(box.style.display!=='none'){box.style.display='none';return;}
  box.style.display='block';
  box.textContent='Loading…';
  fetch('/api/gotek/usbdebug').then(function(r){return r.json();}).then(function(d){
    var ok=d.gotek_mode_fn&&d.cfg_deviceMode===1;
    box.innerHTML='<b>GoTek mode function:</b> '+(d.gotek_mode_fn?'<span style="color:var(--green)">✓ true</span>':'<span style="color:var(--red)">✗ false — NVS read problem!</span>')
      +'<br><b>cfg.deviceMode:</b> '+d.cfg_deviceMode
      +'<br><b>USB PID:</b> '+d.pid_hex+' (GoTek) vs 0x1001 (CD-ROM)'
      +'<br><b>Sectors:</b> '+d.gk_sectors+' × 512 B'
      +(ok?'':'<br><br><b style="color:var(--red)">Problem detected!</b> GoTek mode not active in firmware. Save config and restart ESP32.');
  }).catch(function(){box.textContent='Error fetching debug info';});
}
function gkShowUsbBadge(show){ var b=$('gkUsbBadge'); if(b) b.style.display=show?'inline':'none'; }
function gkShowCreate(){var r=$('gkCreateRow');if(r){var v=r.style.display==='none'||!r.style.display;r.style.display=v?'flex':'none';}}
function gkShowUpload(){var r=$('gkUploadRow');if(r){var v=r.style.display==='none'||!r.style.display;r.style.display=v?'flex':'none';}}

function gkUpdateCurrent(selectedSlot){
  var badge=$('gkCurBadge'), nm=$('gkCurName');
  if(selectedSlot>=0 && selectedSlot<gkFilesList.length){
    var f=gkFilesList[selectedSlot];
    if(badge){badge.textContent='SLOT '+String(selectedSlot).padStart(3,'0');badge.className='badge on';}
    if(nm)    nm.textContent=f.name;
  } else {
    if(badge){badge.textContent='GoTek RAW';badge.className='badge def';}
    if(nm)    nm.textContent='All slots — for GoTek hardware';
  }
}

function gkSelectSlot(slot){
  var btn=event&&event.target?event.target:null;
  var origText=btn?btn.textContent:'';
  if(btn){btn.disabled=true;btn.textContent='⏳';}
  fetch('/api/gotek/select?slot='+slot)
    .then(function(r){return r.json();})
    .then(function(d){
      if(btn){btn.disabled=false;btn.textContent=origText;}
      if(d.ok){ gkSelIdx=slot; gkUpdateCurrent(slot); gkLoadList(); }
      else alert('Error: '+(d.error||'failed'));
    }).catch(function(){
      if(btn){btn.disabled=false;btn.textContent=origText;}
      // re-enum completed, just refresh
      gkUpdateCurrent(slot); gkLoadList();
    });
}

function gkLoadList(){
  fetch('/api/gotek/ls').then(function(r){return r.json();}).then(function(d){
    var tb=$('gkTbody'); if(!tb) return;
    if(!d.files||!d.files.length){
      gkFilesList=[];
      tb.innerHTML='<tr><td colspan="4" style="color:var(--tx2);padding:12px">'+
        'No image files in '+(d.dir||'/gotek')+'<br>'+
        '<small>Use New / Upload above, or copy .img files to SD card and click Refresh.</small></td></tr>';
      return;
    }
    fetch('/api/sysinfo').then(function(r2){return r2.json();}).then(function(s){
      gkFilesList=d.files;
      var cur=s.gotek_cur_slot!=null?s.gotek_cur_slot:-1;
      var sel=s.gotek_selected_slot!=null?s.gotek_selected_slot:-1;
      gkSelIdx=sel; gkUpdateCurrent(sel);
      tb.innerHTML='';
      d.files.forEach(function(f){
        var imgPath=($('gkDir')?$('gkDir').textContent:'/gotek')+'/'+f.name;
        var sz=f.size>1048576?(f.size/1048576).toFixed(2)+' MB':
               f.size>1024?(f.size/1024).toFixed(0)+' KB':f.size+' B';
        var isSel=(f.slot===sel), isCur=(f.slot===cur&&sel<0);
        var tr=mkTr();
        if(isSel) tr.style.background='rgba(63,185,80,.08)';
        else if(isCur) tr.style.background='rgba(56,139,253,.06)';
        // Icon cell
        tr.appendChild(mkTd('ic','💾'));
        // Name + free space cell with mini bar
        var tdN=document.createElement('td'); tdN.className='nm';
        var nameDiv=document.createElement('div'); nameDiv.textContent=f.name;
        // Space sub-line: text + mini bar (populated async)
        var spaceWrap=document.createElement('div');
        spaceWrap.style.cssText='margin-top:3px';
        var spaceDiv=document.createElement('div');
        spaceDiv.id='gkSpace_'+f.slot;
        spaceDiv.style.cssText='font-size:.68rem;color:var(--tx3);margin-top:1px';
        spaceDiv.textContent='loading…';
        var barWrap=document.createElement('div');
        barWrap.style.cssText='height:3px;background:var(--border2);border-radius:2px;margin-top:2px;overflow:hidden';
        var barFill=document.createElement('div');
        barFill.id='gkBar_'+f.slot;
        barFill.style.cssText='height:100%;width:0%;background:var(--green);border-radius:2px;transition:width .4s';
        barWrap.appendChild(barFill);
        spaceWrap.appendChild(spaceDiv); spaceWrap.appendChild(barWrap);
        tdN.appendChild(nameDiv); tdN.appendChild(spaceWrap);
        tr.appendChild(tdN);
        // Size column (image file size on disk)
        tr.appendChild(Object.assign(document.createElement('td'),{className:'sz',textContent:sz}));
        // Action cell
        var tdA=mkTd('ac','');
        // Mount button (green like CD ▶ Mount)
        var mb=document.createElement('button');
        mb.className='btn g'; mb.style.whiteSpace='nowrap';
        mb.textContent=isSel?'✓ Mounted':'▶ Mount';
        mb.onclick=(function(slot){return function(){gkSelectSlot(slot);};})(f.slot);
        tdA.appendChild(mb);
        // Edit button (blue, opens image editor)
        var eb=document.createElement('button');
        eb.className='btn b'; eb.title='Browse image contents';
        eb.textContent='💾';
        eb.onclick=(function(p,n){return function(){imgOpen(p,n);};})(imgPath,f.name);
        tdA.appendChild(eb);
        // Delete button
        var db=document.createElement('button');
        db.className='btn r'; db.title='Delete image file';
        db.textContent='🗑';
        db.onclick=(function(n){return function(){gkDelete(n);};})(f.name);
        tdA.appendChild(db);
        tr.appendChild(tdA);
        tb.appendChild(tr);
      });
      // Async: fetch free/used space for each slot — fills text + mini bar
      d.files.forEach(function(f){
        var spEl=$('gkSpace_'+f.slot), barEl=$('gkBar_'+f.slot);
        if(!spEl) return;
        var imgPath=($('gkDir')?$('gkDir').textContent:'/gotek')+'/'+f.name;
        fetch('/api/img/stat?img='+encodeURIComponent(imgPath))
          .then(function(r){return r.json();}).then(function(st){
            if(!st.ok||!spEl.parentNode) return;
            var freeKB=Math.round(st.free_bytes/1024);
            var usedKB=Math.round(st.used_bytes/1024);
            var totKB=Math.round(st.total_bytes/1024);
            var pct=totKB>0?Math.round(st.used_bytes/st.total_bytes*100):0;
            var col=pct>90?'var(--red)':pct>75?'#e3b341':'var(--green)';
            spEl.innerHTML='<span style="color:'+col+';font-weight:bold">'+usedKB+' KB</span>'
              +' <span style="color:var(--tx2)">used &bull; </span>'
              +'<span style="color:var(--tx2)">'+freeKB+' KB free</span>';
            if(barEl){ barEl.style.width=pct+'%'; barEl.style.background=col; }
          }).catch(function(){ if(spEl) { spEl.textContent='—'; spEl.style.color='var(--tx3)'; } });
      });
    }).catch(function(){
      // Fallback: sysinfo fetch failed — render without mounted slot info
      gkFilesList=d.files; gkUpdateCurrent(-1);
      tb.innerHTML='';
      d.files.forEach(function(f){
        var imgPath=($('gkDir')?$('gkDir').textContent:'/gotek')+'/'+f.name;
        var sz=f.size>1048576?(f.size/1048576).toFixed(2)+' MB':f.size>1024?(f.size/1024).toFixed(0)+' KB':f.size+' B';
        var tr=mkTr();
        var tdN=document.createElement('td'); tdN.className='nm';
        tdN.appendChild(Object.assign(document.createElement('div'),{textContent:f.name}));
        var spDiv=document.createElement('div'); spDiv.id='gkSpace_'+f.slot;
        spDiv.style.cssText='font-size:.68rem;color:var(--tx3);margin-top:1px'; spDiv.textContent='loading…';
        var bWrap=document.createElement('div'); bWrap.style.cssText='height:3px;background:var(--border2);border-radius:2px;margin-top:2px;overflow:hidden';
        var bFill=document.createElement('div'); bFill.id='gkBar_'+f.slot;
        bFill.style.cssText='height:100%;width:0%;background:var(--green);border-radius:2px;transition:width .4s';
        bWrap.appendChild(bFill); tdN.appendChild(spDiv); tdN.appendChild(bWrap);
        tr.appendChild(mkTd('ic','💾')); tr.appendChild(tdN);
        tr.appendChild(Object.assign(document.createElement('td'),{className:'sz',textContent:sz}));
        var tdA=mkTd('ac','');
        var mb=document.createElement('button'); mb.className='btn g'; mb.style.whiteSpace='nowrap';
        mb.textContent='▶ Mount'; mb.onclick=(function(s){return function(){gkSelectSlot(s);};})(f.slot);
        var eb=document.createElement('button'); eb.className='btn b'; eb.title='Browse'; eb.textContent='💾';
        eb.onclick=(function(p,n){return function(){imgOpen(p,n);};})(imgPath,f.name);
        var db=document.createElement('button'); db.className='btn r'; db.title='Delete'; db.textContent='🗑';
        db.onclick=(function(n){return function(){gkDelete(n);};})(f.name);
        tdA.appendChild(mb); tdA.appendChild(eb); tdA.appendChild(db);
        tr.appendChild(tdA); tb.appendChild(tr);
      });
      // Still try to fetch space info
      d.files.forEach(function(f){
        var spEl=$('gkSpace_'+f.slot), barEl=$('gkBar_'+f.slot);
        if(!spEl) return;
        var imgPath=($('gkDir')?$('gkDir').textContent:'/gotek')+'/'+f.name;
        fetch('/api/img/stat?img='+encodeURIComponent(imgPath))
          .then(function(r){return r.json();}).then(function(st){
            if(!st.ok||!spEl.parentNode) return;
            var freeKB=Math.round(st.free_bytes/1024), usedKB=Math.round(st.used_bytes/1024), totKB=Math.round(st.total_bytes/1024);
            var pct=totKB>0?Math.round(st.used_bytes/st.total_bytes*100):0;
            var col=pct>90?'var(--red)':pct>75?'#e3b341':'var(--green)';
            spEl.innerHTML='<span style="color:'+col+';font-weight:bold">'+usedKB+' KB</span><span style="color:var(--tx2)"> used &bull; '+freeKB+' KB free</span>';
            if(barEl){barEl.style.width=pct+'%';barEl.style.background=col;}
          }).catch(function(){if(spEl){spEl.textContent='—';spEl.style.color='var(--tx3)';}});
      });
    });
  }).catch(function(){var tb=$('gkTbody');if(tb)tb.innerHTML='<tr><td colspan="4" style="color:var(--red)">Load error</td></tr>';});
}

// ── IMG Browser ───────────────────────────────────────────────────────────────
var imgCurImg='', imgCurDir='/', imgDirStack=[];
function imgOpen(imgPath, displayName){
  imgCurImg=imgPath; imgCurDir='/'; imgDirStack=[];
  $('imgModalTitle').innerHTML='&#128190; '+escHtml(displayName);
  $('imgUpMsg').textContent='';
  document.body.style.overflow='hidden';
  var m=$('imgModal'); m.style.display='flex';
  imgLoadDir('/');
}
function imgModalClose(){
  $('imgModal').style.display='none';
  document.body.style.overflow='';
  imgCurImg='';
}
function imgRenderBc(dir){
  var parts=dir==='/'?['']:dir.split('/').filter(Boolean);
  var bc='<span style="cursor:pointer;color:var(--accent)" onclick="imgLoadDir(\'/\')">&#128190; /</span>';
  var path='';
  parts.forEach(function(p){path+='/'+p;(function(q){bc+=' / <span style="cursor:pointer;color:var(--accent)" onclick="imgLoadDir(\''+q+'\')">'+escHtml(p)+'</span>';})(path);});
  $('imgBreadcrumb').innerHTML=bc;
}
function imgLoadDir(dir){
  imgCurDir=dir;
  imgRenderBc(dir);
  $('imgTbody').innerHTML='<tr><td colspan="4" style="padding:12px;color:var(--tx2)">Loading&hellip;</td></tr>';
  fetch('/api/img/ls?img='+encodeURIComponent(imgCurImg)+'&dir='+encodeURIComponent(dir))
    .then(function(r){return r.json();}).then(function(d){
    var tb=$('imgTbody'); if(!tb) return;
    var rows='';
    if(dir!=='/'){
      rows+='<tr style="border-bottom:1px solid var(--border2)">'
        +'<td class="ic">&#128193;</td>'
        +'<td class="nm d" style="padding:5px 6px;cursor:pointer" onclick="imgNavUp()">.. (up)</td>'
        +'<td class="sz"></td><td class="ac"></td></tr>';
    }
    if(!d.files||!d.files.length){
      rows+='<tr><td colspan="4" style="padding:12px;color:var(--tx2)">(empty)</td></tr>';
    } else {
      var dirs=d.files.filter(function(e){return e.d;}).sort(function(a,b){return a.n.localeCompare(b.n);});
      var files=d.files.filter(function(e){return!e.d;}).sort(function(a,b){return a.n.localeCompare(b.n);});
      dirs.concat(files).forEach(function(e){
        var sz=e.d?'':(e.s>1048576?(e.s/1048576).toFixed(1)+' MB':e.s>1024?(e.s/1024).toFixed(0)+' KB':e.s+' B');
        var nm=e.n;
        var fpath=(dir==='/'?'':dir)+'/'+nm;
        var icon=e.d?'&#128193;':'&#128196;';
        var nameCell=e.d
          ?'<span style="color:#79c0ff;cursor:pointer" onclick="imgEnter(\''+escHtml(nm)+'\')">'+escHtml(nm)+'</span>'
          :'<span style="color:var(--tx)">'+escHtml(nm)+'</span>';
        var btns='';
        if(!e.d) btns+='<a class="btn gr" style="text-decoration:none;margin-right:3px" title="Download" href="/api/img/get?img='+encodeURIComponent(imgCurImg)+'&file='+encodeURIComponent(fpath)+'" download="'+escHtml(nm)+'">&#8681;</a>';
        btns+='<button class="btn r" title="'+(e.d?'Delete folder':'Delete')+'" onclick="imgDelete(\''+escHtml(nm)+'\','+(e.d?'true':'false')+')">&#128465;</button>';
        rows+='<tr style="border-bottom:1px solid rgba(255,255,255,.04)">'
          +'<td style="padding:5px 10px;color:var(--tx2)">'+icon+'</td>'
          +'<td style="padding:5px 6px">'+nameCell+'</td>'
          +'<td style="text-align:right;padding:5px 10px;color:var(--tx2);white-space:nowrap;font-size:.8rem">'+sz+'</td>'
          +'<td style="padding:5px 6px;white-space:nowrap">'+btns+'</td></tr>';
      });
    }
    tb.innerHTML=rows;
    var fc=d.free_clusters||0,bps=d.bps||512,spc=d.spc||1;
    var freeKB=Math.round(fc*spc*bps/1024);
    var totKB=Math.round((d.total||2880)*bps/1024);
    imgUpdateSpaceBar(freeKB, totKB);
    imgCheckFileSize(); // re-evaluate selected file against new free space
  }).catch(function(){
    $('imgTbody').innerHTML='<tr><td colspan="4" style="color:var(--red);padding:10px">Error reading image — must be FAT12</td></tr>';
  });
}
var imgFreeKB=0, imgTotKB=0;
function imgUpdateSpaceBar(freeKB, totKB){
  imgFreeKB=freeKB; imgTotKB=totKB;
  var usedKB=totKB-freeKB;
  var pct=totKB>0?Math.round(usedKB/totKB*100):0;
  var bar=$('imgSpaceBar'), foot=$('imgFooter');
  if(foot) foot.textContent='Used: '+usedKB+' KB  •  Free: '+freeKB+' KB  •  Total: '+totKB+' KB';
  if(bar){
    bar.style.width=pct+'%';
    bar.style.background=pct>90?'var(--red)':pct>75?'#e3b341':'var(--green)';
  }
}
function imgCheckFileSize(){
  var fi=$('imgUpFile'), warn=$('imgFreeWarn'), btn=$('imgUpBtn');
  if(!fi||!fi.files||!fi.files.length){
    if(warn) warn.style.display='none';
    if(btn) btn.disabled=false;
    return;
  }
  var sz=fi.files[0].size, freeB=imgFreeKB*1024;
  var tooLarge=sz>freeB;
  if(warn) warn.style.display=tooLarge?'inline':'none';
  if(btn){ btn.disabled=tooLarge; btn.title=tooLarge?'File too large — not enough space in image':''; }
}
// Fetch fresh space info without reloading dir listing
function imgRefreshSpace(){
  fetch('/api/img/stat?img='+encodeURIComponent(imgCurImg))
    .then(function(r){return r.json();}).then(function(d){
      if(d.ok) imgUpdateSpaceBar(Math.round(d.free_bytes/1024), Math.round(d.total_bytes/1024));
    }).catch(function(){});
}
function imgNavUp(){
  if(imgDirStack.length) imgCurDir=imgDirStack.pop();
  else imgCurDir='/';
  imgLoadDir(imgCurDir);
}
function imgEnter(name){
  imgDirStack.push(imgCurDir);
  imgCurDir=(imgCurDir==='/'?'':imgCurDir)+'/'+name;
  imgLoadDir(imgCurDir);
}
function imgDelete(name,isDir){
  var fpath=(imgCurDir==='/'?'':imgCurDir)+'/'+name;
  var msg=isDir?'Delete folder "'+name+'" and all contents?':'Delete file "'+name+'"?';
  if(!confirm(msg)) return;
  fetch('/api/img/rm?img='+encodeURIComponent(imgCurImg)+'&file='+encodeURIComponent(fpath))
    .then(function(r){return r.json();}).then(function(d){
      if(d.ok) imgLoadDir(imgCurDir);
      else alert('Error: '+(d.error||'delete failed'));
    }).catch(function(){alert('Delete failed');});
}
function imgMkdir(){
  var name=($('imgMkdirName').value||'').trim().toUpperCase().replace(/[^A-Z0-9_]/g,'');
  if(!name){alert('Enter folder name (A-Z 0-9 _ only).');return;}
  if(name.length>8) name=name.substring(0,8);
  $('imgMkdirMsg').textContent='';
  fetch('/api/img/mkdir?img='+encodeURIComponent(imgCurImg)+
        '&dir='+encodeURIComponent(imgCurDir)+
        '&name='+encodeURIComponent(name))
    .then(function(r){return r.json();}).then(function(d){
      if(d.ok){$('imgMkdirName').value='';$('imgMkdirMsg').textContent='';imgLoadDir(imgCurDir);}
      else{$('imgMkdirMsg').textContent='Error: '+(d.error||'failed');$('imgMkdirMsg').style.color='var(--red)';}
    }).catch(function(){$('imgMkdirMsg').textContent='Network error';$('imgMkdirMsg').style.color='var(--red)';});
}
function imgUpload(){
  var fi=$('imgUpFile'); if(!fi||!fi.files||!fi.files.length){alert('Select a file first.');return;}
  var file=fi.files[0];
  // Client-side space check
  if(file.size>imgFreeKB*1024){
    $('imgUpMsg').textContent='File too large ('+Math.round(file.size/1024)+' KB needed, only '+imgFreeKB+' KB free)';
    $('imgUpMsg').style.color='var(--red)'; return;
  }
  var fpath=(imgCurDir==='/'?'/':imgCurDir+'/')+file.name;
  $('imgUpMsg').textContent='Writing '+file.name+'…'; $('imgUpMsg').style.color='var(--tx2)';
  var fd=new FormData(); fd.append('file',file,file.name);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/api/img/put?img='+encodeURIComponent(imgCurImg)+'&file='+encodeURIComponent(fpath));
  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      var pct=Math.round(e.loaded/e.total*100);
      $('imgUpMsg').textContent='Uploading… '+pct+'%';
    }
  };
  xhr.onload=function(){
    try{var d=JSON.parse(xhr.responseText);
      if(d.ok){$('imgUpMsg').textContent='Done — '+Math.round(d.bytes/1024)+' KB';$('imgUpMsg').style.color='var(--green)';fi.value='';imgLoadDir(imgCurDir);}
      else{$('imgUpMsg').textContent='Error: '+(d.error||'failed');$('imgUpMsg').style.color='var(--red)';}
    }catch(e){$('imgUpMsg').textContent='Error '+xhr.status;$('imgUpMsg').style.color='var(--red)';}
  };
  xhr.onerror=function(){$('imgUpMsg').textContent='Network error';$('imgUpMsg').style.color='var(--red)';};
  xhr.send(fd);
}
// Close modal on backdrop click
document.addEventListener('click',function(e){
  var m=$('imgModal');
  if(m&&m.style.display!=='none'&&e.target===m) imgModalClose();
});
// ── end GoTek JS ─────────────────────────────────────────────────────────────
function gkRefresh(){
  spin('gkSp',true);$('gkRefBtn').disabled=true;
  fetch('/api/gotek/refresh').then(function(r){return r.json();}).then(function(d){
    spin('gkSp',false);$('gkRefBtn').disabled=false;
    $('gkMsg').textContent='Refreshed — '+d.files+' file(s) loaded';
    $('gkMsg').style.color='var(--green)';
    gkLoadList();
  }).catch(function(){spin('gkSp',false);$('gkRefBtn').disabled=false;$('gkMsg').textContent='Error';$('gkMsg').style.color='var(--red)';});
}
function gkCreate(){
  var nm=($('gkNewName').value||'').trim();
  var tp=$('gkNewType').value||'144';
  if(!nm){alert('Enter file name.');return;}
  if(!nm.includes('.')) nm+='.img';
  spin('gkCrSp',true);
  fetch('/api/gotek/create?name='+encodeURIComponent(nm)+'&type='+tp)
    .then(function(r){return r.json();}).then(function(d){
      spin('gkCrSp',false);
      if(d.ok){
        $('gkCrMsg').textContent='Created: '+d.name+' ('+d.kb+' KB)';
        $('gkCrMsg').style.color='var(--green)';
        $('gkNewName').value='';
        gkLoadList();
      } else {
        $('gkCrMsg').textContent='Error: '+(d.error||'failed');
        $('gkCrMsg').style.color='var(--red)';
      }
    }).catch(function(){spin('gkCrSp',false);$('gkCrMsg').textContent='Chyba';$('gkCrMsg').style.color='var(--red)';});
}
function gkDelete(name){
  if(!confirm('Delete '+name+' from GoTek image folder?')) return;
  fetch('/api/delete?path='+encodeURIComponent(($('gkDir')?$('gkDir').textContent:'/gotek')+'/'+name))
    .then(function(){gkRefresh();})
    .catch(function(){alert('Delete failed');});
}
function gkUpload(){
  var f=$('gkUpFile'); if(!f||!f.files||!f.files.length){alert('Select a file first.');return;}
  var file=f.files[0];
  var dir=$('gkDir')?$('gkDir').textContent:'/gotek';
  $('gkUpProg').style.display='block';
  $('gkUpBar').style.width='0%';
  $('gkUpMsg').textContent='Uploading '+file.name+'…';
  var fd=new FormData(); fd.append('file',file,file.name);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/api/upload?path='+encodeURIComponent(dir));
  xhr.upload.onprogress=function(e){if(e.lengthComputable){var pct=Math.round(e.loaded/e.total*100);$('gkUpBar').style.width=pct+'%';$('gkUpMsg').textContent=pct+'% uploaded';}};
  xhr.onload=function(){
    var ok=xhr.status===200&&xhr.responseText.indexOf('OK')===0;
    $('gkUpMsg').textContent=ok?'Upload complete!':'Upload error: '+xhr.status;
    $('gkUpMsg').style.color=ok?'var(--green)':'var(--red)';
    if(ok){setTimeout(function(){$('gkUpProg').style.display='none';f.value='';gkRefresh();},1500);}
  };
  xhr.onerror=function(){$('gkUpMsg').textContent='Network error';$('gkUpMsg').style.color='var(--red)';};
  xhr.send(fd);
}
function escHtml(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
// ── end GoTek JS ─────────────────────────────────────────────────────────────
function showTab(n){curTab=n;for(var i=0;i<7;i++){var tEl=$('t'+i);if(tEl&&!tEl.classList.contains('disabled'))tEl.classList.toggle('active',i===n);var pEl=$('p'+i);if(pEl)pEl.classList.toggle('active',i===n);}if(n===1){apStartPoll();}else{apStopPoll();}if(n===0)cdLoadDir(cdPath);if(n===2)fmLoadDir(fmPath);if(n===3){if($("logLiveUpdate")&&$("logLiveUpdate").checked){if(!logTimer)logTimer=setInterval(logFetch,1000);logFetch();}}else{if(n!==3&&logTimer&&!($("logLiveUpdate")&&$("logLiveUpdate").checked)){clearInterval(logTimer);logTimer=null;}}if(n===3){if(!logTimer&&$("logLiveUpdate")&&$("logLiveUpdate").checked){logTimer=setInterval(logFetch,1000);}logFetch();}else if(n!==3){if(logTimer&&!($("logLiveUpdate")&&$("logLiveUpdate").checked)){clearInterval(logTimer);logTimer=null;}}if(n===4)loadSysinfo();if(n===5)cfgLoad();if(n===6)gkInit();}

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
    var txpEl=$('cfgWifiTxPower'); if(txpEl) txpEl.value=c.wifiTxPower||40;
    var ddEl=$('cfgDosDriver'); if(ddEl) ddEl.value=c.dosDriver||0;
    $('cfgDosCompat').value = c.dosCompat ? '1' : '0'; dosCompatChanged();
    if($('cfgWin98StopMs')) $('cfgWin98StopMs').value = c.win98StopMs !== undefined ? c.win98StopMs : 1200;
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
    // Device mode
    var dmEl=$('cfgDeviceMode'); if(dmEl){dmEl.value=c.deviceMode||0; deviceModeChanged();}
    var gdEl=$('cfgGotekDir'); if(gdEl) gdEl.value=c.gotekDir||'/gotek';
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
  params.set('dosDriver',$('cfgDosDriver')?$('cfgDosDriver').value:'0');
  params.set('dosCompat',$('cfgDosCompat').value);
    if($('cfgWin98StopMs')) params.set('win98StopMs',$('cfgWin98StopMs').value||'1200');
  params.set('httpsEnabled',$('cfgHttpsEnabled')?$('cfgHttpsEnabled').value:'0');
  params.set('httpsCert',$('cfgHttpsCert')?$('cfgHttpsCert').value:'');
  params.set('httpsKey',$('cfgHttpsKey')?$('cfgHttpsKey').value:'');
  var hkp=$('cfgHttpsKPass'); if(hkp&&hkp.value) params.set('httpsKPass',hkp.value);
  var hp=$('cfgHttpPort'); if(hp) params.set('httpPort',hp.value);
  var hps=$('cfgHttpsPort'); if(hps) params.set('httpsPort',hps.value);
  var tv=$('cfgTlsMinVer'); if(tv) params.set('tlsMinVer',tv.value);
  var tc=$('cfgTlsCiphers'); if(tc) params.set('tlsCiphers',tc.value);
  var txp=$('cfgWifiTxPower'); if(txp) params.set('wifiTxPower',txp.value);
  params.set('webAuth',$('cfgWebAuth').value);
  if($('cfgWebUser').value.trim()) params.set('webUser',$('cfgWebUser').value.trim());
  if($('cfgWebPass').value.trim()) params.set('webPass',$('cfgWebPass').value.trim());
  var dm=$('cfgDeviceMode'); if(dm) params.set('deviceMode',dm.value);
  var gd=$('cfgGotekDir'); if(gd&&gd.value.trim()) params.set('gotekDir',gd.value.trim());
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
      siRow(tw,'TX power',s.wifi_tx_power+' dBm');
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
    var dosDriverNames=['Generic','USBCD2.SYS (TEAC CD-56E)','ESPUSBCD.SYS (Panasonic CR-572)','DI1000DD.SYS (USBASPI 2.20)'];
    var dosDriverName=dosDriverNames[s.dos_driver||0]||'Generic';
    siRow(sy,'DOS driver',s.dos_compat
      ?'<span style="color:var(--ok)">&#10003; '+dosDriverName+'</span>'
      :'<span style="color:var(--muted)">Disabled</span>');
    dosCompatOn=s.dos_compat||false;
    siRow(sy,'Device mode',s.device_mode===1
      ?'<span style="color:#e3b341">&#128190; GoTek floppy emulator</span>'
      :'<span style="color:var(--ok)">&#128249; CD-ROM emulator</span>');
    siRow(sy,'Debug logging',s.debug_mode
      ?'<span style="color:var(--warn)">&#9888; ON &mdash; verbose SCSI/API logs</span>'
      :'<span style="color:var(--ok)">OFF &mdash; quiet</span>');
      // GoTek section
      var gkCard=$('siGotekCard'), gkt=$('siGotek');
      // gkApplyTabVisibility handles siImgCard/siAudioCard visibility
      gkApplyTabVisibility(s.device_mode===1);
      if(gkCard&&gkt){
        if(s.device_mode===1){
          gkCard.style.display='';
          gkt.innerHTML='';
          siRow(gkt,'Status',s.gotek_ready?'<span style="color:var(--ok)">&#10003; Ready</span>':'<span style="color:var(--warn)">Building&hellip;</span>');
          siRow(gkt,'Image folder',s.gotek_dir||'/gotek');
          siRow(gkt,'Slots loaded',String(s.gotek_files||0));
          if(s.gotek_cur_slot>=0) siRow(gkt,'Active slot','<span style="color:var(--ok)">'+String(s.gotek_cur_slot).padStart(3,'0')+'</span>');
          siRow(gkt,'Virtual disk','1000 slots × 1.44 MB, 512 B sectors, raw (no FAT)');
        } else {
          gkCard.style.display='none';
        }
      }
      // Audio section
      var ta=$('siAudio')&&$('siAudio').querySelector('tbody');
      if(ta){
        ta.innerHTML='';
        var audioModLabel=s.audio_module?'<span style="color:var(--ok)">&#10003; PCM5102 enabled</span>':'<span style="color:var(--muted)">disabled</span>';
        siRow(ta,'Module',audioModLabel);
        siRow(ta,'Win98 Stop',s.win98_stop_ms>0
          ?'<span style="color:var(--ok)">'+s.win98_stop_ms+' ms</span>'
          :'<span style="color:var(--muted)">disabled</span>');
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
  var btn=event&&event.target?event.target:null;
  var origText=btn?btn.textContent:'';
  if(btn){btn.disabled=true;btn.textContent='⏳';}
  var doActualMount=function(){
    log('Mounting: '+path);
    fetch('/api/mount?file='+encodeURIComponent(path))
      .then(function(r){return r.text();})
      .then(function(m){
        if(btn){btn.disabled=false;btn.textContent=origText;}
        log(m);loadStatus();
      })
      .catch(function(){
        if(btn){btn.disabled=false;btn.textContent=origText;}
        log('ERROR: mount failed.');
      });
  };
  if(dosCompatOn&&curMounted){
    log('DOS compat: ejecting...');
    fetch('/api/umount').then(function(r){return r.text();})
      .then(function(m){
        log(m); curMounted=''; loadStatus();
        setTimeout(doActualMount,2500);
      })
      .catch(function(){doActualMount();});
  } else {
    doActualMount();
  }
}
function doEject(){
  var btn=event&&event.target?event.target:null;
  var origText=btn?btn.textContent:'';
  if(btn){btn.disabled=true;btn.textContent='⏳';}
  fetch('/api/umount').then(function(r){return r.text();}).then(function(m){
    if(btn){btn.disabled=false;btn.textContent=origText;}
    log(m);loadStatus();
  }).catch(function(){if(btn){btn.disabled=false;btn.textContent=origText;}});
}

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
