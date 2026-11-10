#pragma once
#include "observatory_identity.h"
static const char INDEX_HTML[] = R"HTML(<!DOCTYPE html><html lang="pl"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#02040c">
<title>)HTML" OBSERVATORY_NAME R"HTML( — ASTRO-ROOF SAFE</title>
<style>
/* ══ ASTRO-ROOF SAFE · UI · deep-space instrument panel ══ */
:root{
  --bg:#020409;--card:rgba(11,20,40,.78);--card2:rgba(6,11,24,.62);
  --line:rgba(142,178,255,.13);--line2:rgba(142,178,255,.26);
  --mut:#6f86a6;--txt:#dbe9ff;--tit:#f4f8ff;
  --ok:#2ee6a6;--bad:#ff5470;--warn:#ffc24b;--info:#63a8ff;--vio:#a58bff;
  --rad:20px;--rad2:14px;
  --shadow:0 24px 70px -24px rgba(0,0,0,.75);
  --hl:inset 0 1px 0 rgba(255,255,255,.055);
  --font-head:ui-sans-serif,system-ui,-apple-system,'Segoe UI',sans-serif;
  --font-ui:ui-sans-serif,system-ui,-apple-system,'Segoe UI',sans-serif;
  --font-mono:ui-monospace,'SFMono-Regular',Consolas,monospace;
}
*{box-sizing:border-box;margin:0;padding:0}
html{scrollbar-width:thin;scrollbar-color:rgba(120,160,255,.28) transparent}
::-webkit-scrollbar{width:9px;height:9px}::-webkit-scrollbar-thumb{background:rgba(120,160,255,.22);border-radius:9px}::-webkit-scrollbar-track{background:transparent}
::selection{background:rgba(99,168,255,.35);color:#fff}
html,body{min-height:100%;background:var(--bg);color:var(--txt);font-family:var(--font-ui);font-weight:500}
body{overflow:auto;background:
  radial-gradient(60% 42% at 14% -6%,rgba(78,122,255,.20),transparent 60%),
  radial-gradient(46% 34% at 88% 6%,rgba(46,230,166,.08),transparent 60%),
  radial-gradient(70% 55% at 50% 112%,rgba(165,139,255,.10),transparent 62%),
  linear-gradient(180deg,#01030a 0%,#060d1f 52%,#02040c 100%)}
body::before{content:'';position:fixed;inset:0;pointer-events:none;opacity:.16;z-index:0;
  background-image:radial-gradient(#fff .6px,transparent .6px);background-size:30px 30px}
.app{position:relative;z-index:1;width:min(1420px,100%);min-height:100vh;margin:0 auto;padding:12px;display:grid;grid-template-rows:auto auto 1fr;gap:12px}
.topbar,.manual-banner,.estop-banner,.sky-bar-wrap,.card{box-shadow:var(--shadow),var(--hl)}

/* ── Topbar ── */
.topbar{display:flex;align-items:center;justify-content:space-between;gap:12px;background:linear-gradient(135deg,rgba(13,24,46,.9),rgba(6,11,24,.82));border:1px solid var(--line);border-radius:var(--rad);padding:11px 16px;min-height:60px}
.logo{display:flex;align-items:center;gap:13px;min-width:0}
.logo-icon{width:44px;height:44px;flex:0 0 auto;display:grid;place-items:center;font-size:24px;border-radius:14px;border:1px solid var(--line);background:radial-gradient(120% 120% at 30% 20%,rgba(99,168,255,.22),rgba(6,11,24,.4));filter:drop-shadow(0 0 14px rgba(99,168,255,.35))}
.logo-text{font-family:var(--font-head);font-weight:900;font-size:20px;line-height:1;white-space:nowrap;background:linear-gradient(120deg,#fff,#bdd6ff 55%,#8fb6ff);-webkit-background-clip:text;background-clip:text;color:transparent}
.logo-hw{font-family:var(--font-mono);font-size:9px;font-weight:800;color:#82cfff;margin-top:4px;line-height:1.25;max-width:58vw;white-space:normal;letter-spacing:.01em}
.logo-clock{font-family:var(--font-mono);font-size:9px;font-weight:700;color:var(--mut);margin-top:3px;line-height:1.2;max-width:58vw;white-space:normal}
 .logo-sub{font-family:var(--font-mono);font-size:10px;font-weight:700;color:var(--mut);margin-top:4px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:52vw;letter-spacing:.02em}
.topbar-right{display:flex;align-items:center;gap:8px;flex-wrap:wrap;justify-content:flex-end}
.safety-pill{display:flex;align-items:center;gap:9px;border-radius:999px;font-family:var(--font-head);font-weight:900;font-size:13px;letter-spacing:.09em;padding:9px 16px;border:1px solid;white-space:nowrap}
.safety-pill.safe{background:rgba(46,230,166,.10);border-color:rgba(46,230,166,.5);color:#a4ffdc;box-shadow:0 0 30px -6px rgba(46,230,166,.35)}
.safety-pill.unsafe{background:rgba(255,84,112,.11);border-color:rgba(255,84,112,.52);color:#ffb3c2;box-shadow:0 0 30px -6px rgba(255,84,112,.35)}
.safety-dot{width:8px;height:8px;border-radius:50%;background:currentColor;animation:pulse 1.6s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.35;transform:scale(.6)}}
.status-chip,.badge{display:inline-flex;align-items:center;gap:6px;font-size:10px;font-weight:700;letter-spacing:.06em;padding:6px 11px;border-radius:999px;border:1px solid;background:rgba(255,255,255,.03);white-space:nowrap}
.status-chip::before{content:'';width:6px;height:6px;border-radius:50%;background:currentColor;opacity:.9}
.chip-ok{border-color:rgba(46,230,166,.45);color:#9dffd6}
.chip-bad{border-color:rgba(255,84,112,.5);color:#ffb3c2}
.chip-off{border-color:rgba(120,144,170,.24);color:var(--mut)}
.btn-manual-on,.btn-manual-off{border:0;border-radius:999px;font-family:var(--font-head);font-weight:900;font-size:12px;letter-spacing:.03em;padding:10px 15px;cursor:pointer;transition:transform .15s,filter .15s,box-shadow .15s}
.btn-manual-on{background:linear-gradient(135deg,#14c98e,#8dffe6);color:#03271a;box-shadow:0 8px 22px -10px rgba(46,230,166,.55)}
.btn-manual-off{background:linear-gradient(135deg,#a70d25,#ff5470);color:#fff;white-space:nowrap;box-shadow:0 8px 22px -10px rgba(255,84,112,.55)}
.btn-logs{display:inline-flex;align-items:center;justify-content:center;border:1px solid rgba(99,168,255,.38);border-radius:999px;padding:9px 13px;background:linear-gradient(135deg,rgba(25,58,102,.9),rgba(18,35,64,.92));color:#dceaff;text-decoration:none;font-family:var(--font-head);font-size:11px;font-weight:900;letter-spacing:.035em;white-space:nowrap;box-shadow:0 8px 20px -12px rgba(99,168,255,.65);transition:transform .15s,filter .15s,box-shadow .15s}
.btn-logs:hover{filter:brightness(1.15);transform:translateY(-1px);box-shadow:0 10px 24px -10px rgba(99,168,255,.5)}
.btn-logs:focus-visible{outline:2px solid #8fc0ff;outline-offset:3px}
.btn-manual-on:hover,.btn-manual-off:hover{filter:brightness(1.1);transform:translateY(-1px)}
.btn-manual-on:active,.btn-manual-off:active{transform:scale(.97)}

/* ── Banery ── */
.manual-banner{display:none;background:linear-gradient(135deg,rgba(255,84,112,.16),rgba(255,194,71,.07));border:1px solid rgba(255,84,112,.45);border-radius:var(--rad);padding:11px 16px;align-items:center;gap:12px}
.manual-banner.show{display:flex}
.bench-banner{display:none;align-items:center;gap:10px;padding:11px 15px;border:1px solid rgba(255,194,71,.42);border-radius:var(--rad2);background:linear-gradient(110deg,rgba(255,194,71,.14),rgba(99,168,255,.07));color:#ffe3a0;font-size:12px;line-height:1.45}
.bench-banner.show{display:flex}
.aux-banner{display:none;padding:10px 15px;border:1px solid rgba(99,168,255,.32);border-radius:var(--rad2);background:rgba(21,44,66,.78);color:#bfdcff;font-size:11px;line-height:1.45}
.aux-banner.show{display:block}
.bench-banner b{font-size:10px;letter-spacing:.12em;white-space:nowrap;color:#ffd16a}
.mb-left{flex:1;min-width:0}
.mb-title{font-family:var(--font-head);font-size:15px;font-weight:900;color:#ffc3cd}
.mb-sub{font-size:11.5px;color:#ff9fb0}.mb-sub strong{font-family:var(--font-mono)}
.estop-banner{display:none;background:linear-gradient(135deg,rgba(255,30,60,.26),rgba(120,0,16,.16));border:1.5px solid rgba(255,60,90,.7);border-radius:var(--rad);padding:13px 17px;align-items:center;gap:14px;animation:estopPulse 1.1s ease-in-out infinite}
.estop-banner.show{display:flex}
.eb-left{flex:1;min-width:0}
.eb-title{font-family:var(--font-head);font-size:17px;font-weight:900;color:#ffd2da}
.eb-sub{font-size:12px;color:#ffb3c0;overflow:hidden;text-overflow:ellipsis}
.btn-estop-reset{border:0;border-radius:13px;font-family:var(--font-head);font-weight:900;font-size:13px;padding:12px 17px;cursor:pointer;background:linear-gradient(135deg,#3a4a63,#66748f);color:#eaf3ff;white-space:nowrap}
.btn-estop-reset:disabled{opacity:.4;cursor:not-allowed}
@keyframes estopPulse{0%,100%{box-shadow:0 0 26px rgba(255,40,70,.20)}50%{box-shadow:0 0 44px rgba(255,40,70,.5)}}
.estop-banner.warn{background:linear-gradient(135deg,rgba(255,194,71,.18),rgba(120,80,0,.1));border-color:rgba(255,194,71,.6);animation:none;box-shadow:var(--shadow)}
.estop-banner.warn .eb-title{color:#ffe3a0}.estop-banner.warn .eb-sub{color:#ffd98a}

/* ── Sky bar (hero) ── */
.content{min-height:0;display:grid;grid-template-rows:auto 1fr;gap:12px}
.sky-bar-wrap{background:linear-gradient(135deg,rgba(13,25,48,.92),rgba(7,13,28,.8));border:1px solid var(--line);border-radius:var(--rad);padding:13px 15px;display:grid;grid-template-columns:auto auto 1fr auto;align-items:center;gap:14px;position:relative;overflow:hidden}
.sky-bar-wrap::after{content:'';position:absolute;left:16px;right:16px;top:0;height:2px;border-radius:2px;background:linear-gradient(90deg,transparent,var(--info),var(--ok),var(--vio),transparent);background-size:200% 100%;animation:hairMove 7s linear infinite;opacity:.85}
@keyframes hairMove{0%{background-position:0% 0}100%{background-position:200% 0}}
.sky-icon{width:56px;height:56px;display:grid;place-items:center;font-size:30px;border-radius:16px;border:1px solid var(--line);background:radial-gradient(120% 120% at 30% 18%,rgba(99,168,255,.2),rgba(4,8,18,.5))}
.sky-status{font-family:var(--font-head);font-size:24px;font-weight:900;letter-spacing:.01em;white-space:nowrap}
.sky-reason-txt{font-size:11px;color:#ffadbd;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sky-cells{display:grid;grid-template-columns:repeat(4,minmax(80px,1fr));gap:8px}
.sky-cell,.metric,.roof-row,.limit-box,.aux-btn,.diag-cell{background:linear-gradient(180deg,rgba(255,255,255,.05),rgba(255,255,255,.014));border:1px solid var(--line);border-radius:var(--rad2);box-shadow:var(--hl)}
.sky-cell{padding:9px 11px;text-align:center}
.ck,.metric .k,.roof-label,.lk,.aux-btn .an{font-size:9px;font-weight:600;color:var(--mut);text-transform:uppercase;letter-spacing:.13em}
.cv{font-family:var(--font-mono);font-size:15px;font-weight:800;margin-top:3px}
.cv.ok{color:var(--ok)}.cv.warn{color:var(--warn)}.cv.bad{color:var(--bad)}

/* ── Karty ── */
.main-grid{min-height:0;display:grid;grid-template-columns:minmax(0,1.05fr) minmax(420px,.95fr);gap:12px}
.card{background:linear-gradient(150deg,rgba(12,22,43,.9),rgba(5,10,22,.82));border:1px solid var(--line);border-radius:var(--rad);padding:14px;overflow:hidden;display:flex;flex-direction:column;gap:11px;position:relative}
.card::before{content:'';position:absolute;inset:0 0 auto 0;height:1px;background:linear-gradient(90deg,transparent 4%,rgba(142,178,255,.28),transparent 96%)}
.card-title{font-family:var(--font-head);font-size:12.5px;font-weight:900;color:#eef4ff;text-transform:uppercase;letter-spacing:.12em;display:flex;align-items:center;gap:9px}
.card-title::before{content:'';width:4px;height:17px;border-radius:5px;background:linear-gradient(180deg,var(--info),var(--ok));box-shadow:0 0 16px rgba(99,168,255,.45)}

/* ── Metryki pogodowe (2×4, wypelniaja karte) ── */
.metrics{flex:1;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));grid-auto-rows:1fr;gap:10px}
.metric{padding:13px 15px;min-height:70px;display:flex;flex-direction:column;justify-content:space-between;gap:6px;position:relative;overflow:hidden;transition:border-color .2s,transform .2s}
.metric::before{content:'';position:absolute;left:0;top:13px;bottom:13px;width:3px;border-radius:3px;background:linear-gradient(180deg,var(--info),var(--ok));opacity:0;transition:opacity .25s}
.metric:hover{border-color:var(--line2);transform:translateY(-1px)}
.metric:hover::before{opacity:.85}
.metric .k{display:flex;align-items:center;gap:7px}
.metric .k i{font-style:normal;font-size:13px;line-height:1;opacity:.9}
.metric .v{font-family:var(--font-mono);font-size:26px;font-weight:800;letter-spacing:-.02em;color:var(--tit);white-space:nowrap}
.metric .v small{font-family:var(--font-ui);font-size:10px;font-weight:600;color:var(--mut);margin-left:5px;letter-spacing:.08em}
.heater-warn{display:none;background:rgba(255,194,71,.1);border:1px solid rgba(255,194,71,.32);border-radius:var(--rad2);padding:10px 12px;font-size:11.5px;color:#ffe3a0}
.heater-warn.show{display:block}
.heaters-panel{margin-top:10px}
.heater-module-state{font-size:11px;color:var(--mut);margin:-2px 0 10px}
.heater-module-state.ok{color:var(--ok)}
.heater-module-state.bad{color:var(--bad)}
.heater-channel-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}
.heater-channel{border:1px solid var(--line);background:rgba(255,255,255,.025);border-radius:var(--rad2);padding:11px 13px;display:flex;justify-content:space-between;align-items:center;gap:10px}
.heater-channel .hn{font-size:11px;color:var(--mut);font-weight:700}
.heater-channel .hv{font-size:16px;font-weight:800;color:var(--ok);text-align:right}
.heater-channel .hw{font-size:10px;color:var(--mut);text-align:right;margin-top:2px}
.heater-note{font-size:10px;color:var(--mut);margin-top:9px;line-height:1.5}
.hm-row{display:grid;grid-template-columns:72px 84px minmax(0,1fr) 40px;gap:8px;align-items:center;margin-top:8px;font-size:11px}
.hm-row.off{opacity:.45}
.hm-n{color:var(--mut);font-weight:700}
.hm-mode{border:1px solid var(--line);background:rgba(255,255,255,.04);color:var(--mut);border-radius:8px;padding:6px 4px;font-size:10px;font-weight:800;cursor:pointer}
.hm-mode.manual{border-color:rgba(255,194,71,.5);color:#ffe3a0;background:rgba(255,194,71,.12)}
.hm-mode:disabled{cursor:not-allowed}
.hm-row input[type=range],.led-row input[type=range]{width:100%;min-width:0}
.hm-val{text-align:right;font-weight:800;color:var(--ok)}
.led-row{display:none;grid-template-columns:auto minmax(0,1fr) 40px;gap:8px;align-items:center;margin:8px 0;font-size:11px;color:var(--mut)}
.led-row.show{display:grid}
.led-row b{text-align:right;color:#ffe6b8}
@media(max-width:760px){.heater-channel-grid{grid-template-columns:1fr}}

/* ── Dach ── */
.roof-svg-wrap{flex:1;min-height:220px;display:flex;align-items:center;justify-content:center;background:radial-gradient(circle at 50% 28%,rgba(99,168,255,.13),transparent 56%);border:1px solid rgba(99,168,255,.1);border-radius:18px;overflow:hidden;position:relative}
#roofSvg{width:100%;max-width:560px;height:260px;filter:drop-shadow(0 18px 35px rgba(0,0,0,.45))}
#roofL,#roofR{transition:transform .9s cubic-bezier(.4,0,.2,1);transform-box:fill-box}
.roof-row{display:flex;align-items:center;justify-content:space-between;padding:11px 14px}
.roof-state-val{font-family:var(--font-head);font-size:20px;font-weight:900;color:var(--tit)}
.limits{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.limit-box{padding:10px;text-align:center;transition:border-color .25s,background .25s}
.lv{font-family:var(--font-mono);font-size:13px;font-weight:800;margin-top:2px}
.limit-box.active{background:rgba(46,230,166,.1);border-color:rgba(46,230,166,.45)}
.limit-box.active .lv{color:var(--ok)}

/* ── Przyciski ── */
.btns{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.btns-1{grid-template-columns:1fr}
button{cursor:pointer;border:0;border-radius:var(--rad2);padding:13px 6px;font-family:var(--font-head);font-weight:900;font-size:14px;letter-spacing:.03em;transition:transform .15s,filter .15s,opacity .15s,box-shadow .15s}
button:disabled{opacity:.35;cursor:not-allowed}
button:not(:disabled):hover{filter:brightness(1.12);transform:translateY(-1px)}
button:not(:disabled):active{transform:scale(.98)}
.btn-open{background:linear-gradient(135deg,#1350c6,#63a8ff);color:#eaf3ff;box-shadow:0 12px 28px -12px rgba(99,168,255,.55)}
.btn-close{background:linear-gradient(135deg,#9f102b,#ff5470);color:#fff0f3;box-shadow:0 12px 28px -12px rgba(255,84,112,.55)}
.btn-stop{background:linear-gradient(135deg,#8a5200,#ffc24b);color:#1a0d00;box-shadow:0 12px 28px -12px rgba(255,194,71,.5)}
.btn-hazard{background:linear-gradient(135deg,#7a0d1c,#ff2d4a);color:#fff;box-shadow:0 12px 30px -12px rgba(255,45,74,.6)}
.btn-pc{background:linear-gradient(135deg,#372a63,#6a4fd0);color:#e8defc;border:1px solid rgba(165,139,255,.35);font-size:15px;box-shadow:0 12px 28px -12px rgba(165,139,255,.45)}
.btn-light{background:linear-gradient(135deg,#3a2c1a,#7a5a2e);color:#ffe6b8;border:1px solid rgba(255,196,110,.3);font-size:15px;box-shadow:0 12px 28px -12px rgba(255,196,110,.35)}
.btn-light.on{background:linear-gradient(135deg,#a8720f,#ffd27a);color:#241400;box-shadow:0 0 26px -6px rgba(255,196,110,.7),0 12px 28px -12px rgba(255,196,110,.55)}

/* ── AUX + stopka ── */
.aux-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.aux-btn{padding:10px 4px;text-align:center;cursor:pointer;user-select:none;transition:border-color .2s,background .2s,transform .15s}
.aux-btn:hover{border-color:var(--line2);transform:translateY(-1px)}
.aux-btn .av{font-family:var(--font-mono);font-size:13px;font-weight:800;margin-top:3px}
.aux-btn.on{background:rgba(46,230,166,.1);border-color:rgba(46,230,166,.45);box-shadow:0 0 22px -8px rgba(46,230,166,.5),var(--hl)}
.aux-btn.on .av{color:var(--ok)}
.foot-line{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-top:auto}
.badge{font-family:var(--font-mono);font-size:9.5px;font-weight:700;padding:5px 9px;border-color:rgba(120,144,170,.22);color:var(--mut)}
.badge.wifi{color:#d5b2ff;border-color:rgba(165,139,255,.3)}
.badge.eth{color:var(--info);border-color:rgba(99,168,255,.3)}
#tcaWarn{color:#ffadbd;font-size:11px;display:none}
.toast{position:fixed;bottom:18px;left:50%;transform:translate(-50%,8px);background:#08101e;border:1px solid var(--line2);padding:11px 20px;border-radius:999px;font-weight:700;font-size:12px;letter-spacing:.02em;opacity:0;transition:opacity .2s,transform .2s;pointer-events:none;box-shadow:var(--shadow);z-index:999}
.toast.show{opacity:1;transform:translate(-50%,0)}

/* ── WEATHER FX — animowana pogoda nad dachem ── */
.fx{position:absolute;inset:0;pointer-events:none;border-radius:18px;overflow:hidden;z-index:3}
.fx>div{position:absolute;inset:0}
.fx .fx-clouds{opacity:0;transition:opacity 1.6s ease;
  background:radial-gradient(58% 46% at 28% 30%,rgba(120,134,158,.55),transparent 70%),
             radial-gradient(48% 38% at 72% 38%,rgba(108,122,148,.5),transparent 72%),
             radial-gradient(70% 50% at 52% 22%,rgba(96,110,136,.4),transparent 75%);
  filter:blur(2px);animation:cloudDrift 26s ease-in-out infinite alternate}
@keyframes cloudDrift{0%{transform:translateX(-10px)}100%{transform:translateX(14px)}}
.fx .fx-gust{left:-40%;right:-40%;top:24%;height:52%;opacity:0;transition:opacity .8s ease;
  background:repeating-linear-gradient(90deg,rgba(190,216,255,0) 0 46px,rgba(190,216,255,.10) 46px 64px,rgba(190,216,255,0) 64px 130px);
  animation:windFlow var(--wdur,3.2s) linear infinite}
@keyframes windFlow{from{transform:translateX(-130px)}to{transform:translateX(0)}}
.fx.windy .fx-gust{opacity:.85}
.fx .fx-rain{top:-25%;opacity:0;transition:opacity .5s ease;
  background-image:repeating-linear-gradient(74deg,rgba(150,200,255,0) 0 7px,rgba(150,200,255,.40) 7px 8px,rgba(150,200,255,0) 8px 14px);
  background-size:26px 64px;animation:rainFall .5s linear infinite}
@keyframes rainFall{from{background-position:0 0}to{background-position:-18px 64px}}
.fx.raining .fx-rain{opacity:.55}
#roofGlow.pulse{animation:glowPulse 3.4s ease-in-out infinite}
@keyframes glowPulse{0%,100%{opacity:.10}50%{opacity:.30}}
#roofSvg .twinkle{animation:tw 3s ease-in-out infinite}
#roofSvg .twinkle:nth-child(2n){animation-duration:4.2s}
#roofSvg .twinkle:nth-child(3n){animation-duration:2.4s}
@keyframes tw{0%,100%{opacity:.85}50%{opacity:.25}}

/* ── Park banner ── */
.park-banner{display:none;background:rgba(255,194,71,.1);border:1px solid rgba(255,194,71,.35);border-radius:var(--rad2);padding:10px 12px;font-size:12px;color:#ffe3a0;transition:all .5s}
.park-banner.show{display:flex;align-items:center;gap:8px}
.park-banner.danger{background:rgba(255,84,112,.1);border-color:rgba(255,84,112,.45);color:#ffbdca}
@keyframes parkPulse{0%,100%{box-shadow:0 0 0 0 rgba(46,230,166,.4)}50%{box-shadow:0 0 0 8px rgba(46,230,166,0)}}
.park-banner.anim-park{animation:parkPulse 1.2s ease-in-out 4}

/* ── Diagnostyka + progi ── */
.lower-grid{display:grid;grid-template-columns:minmax(0,1.1fr) minmax(0,.9fr);gap:12px;margin-top:12px}
.diag-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.diag-cell{border-radius:12px;padding:9px 10px;display:flex;flex-direction:column;gap:3px;min-width:0;transition:border-color .25s}
.diag-cell .dn{font-size:8.5px;font-weight:600;color:var(--mut);text-transform:uppercase;letter-spacing:.11em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.diag-cell .dv{font-family:var(--font-mono);font-size:12px;font-weight:700;color:#eaf3ff;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.diag-cell .ds{font-size:8.5px;font-weight:800;letter-spacing:.08em}
.diag-cell.ok{border-color:rgba(46,230,166,.32)}.diag-cell.ok .ds{color:var(--ok)}
.diag-cell.bad{border-color:rgba(255,84,112,.48);background:rgba(255,84,112,.07)}.diag-cell.bad .ds{color:var(--bad)}
.diag-cell.off .ds{color:var(--mut)}
.diag-foot{margin-top:auto;font-family:var(--font-mono);font-size:10px;color:var(--mut);display:flex;flex-wrap:wrap;gap:4px 14px;padding-top:9px}
.cfg-note{font-size:10.5px;color:var(--mut);line-height:1.45}
.cfg-sec{font-size:11px;font-weight:700;color:#cfe2ff;letter-spacing:.05em;margin-top:4px}
.cfg-rows{display:flex;flex-direction:column;gap:6px}
.cfg-row{display:flex;align-items:center;justify-content:space-between;gap:8px;font-size:12.5px;color:var(--txt)}
.cfg-row span{color:#b7c7dd}
.cfg-row input{width:82px;background:rgba(2,8,20,.72);border:1px solid var(--line);border-radius:11px;color:#eaf3ff;font-family:var(--font-mono);font-weight:700;font-size:14px;padding:8px 9px;text-align:center;transition:border-color .2s,box-shadow .2s}
.cfg-row input:focus{outline:none;border-color:var(--info);box-shadow:0 0 0 3px rgba(99,168,255,.16)}
.cfg-btns{display:grid;grid-template-columns:1.4fr 1fr;gap:8px;margin-top:4px}
.btn-save{background:linear-gradient(135deg,#0d7a52,#2ee6a6);color:#03271a;border:0;border-radius:12px;padding:12px 6px;font-family:var(--font-head);font-weight:900;font-size:13px;cursor:pointer;box-shadow:0 10px 24px -12px rgba(46,230,166,.5)}
.btn-default{background:linear-gradient(135deg,#3a4a63,#66748f);color:#eaf3ff;border:0;border-radius:12px;padding:12px 6px;font-family:var(--font-head);font-weight:900;font-size:13px;cursor:pointer}
.btn-seed{background:linear-gradient(135deg,#b8791a,#ffcf5c);color:#2a1c00;border:0;border-radius:12px;padding:12px 6px;font-family:var(--font-head);font-weight:900;font-size:13px;cursor:pointer;box-shadow:0 10px 24px -12px rgba(255,207,92,.5)}
.btn-forget{background:linear-gradient(135deg,#4a2f2f,#7a4a4a);color:#ffdede;border:0;border-radius:12px;padding:12px 6px;font-family:var(--font-head);font-weight:900;font-size:13px;cursor:pointer}
.cfg-btns button:hover{filter:brightness(1.12)}
.cfg-msg{font-size:11px;font-weight:800;min-height:14px}
.cfg-msg.ok{color:var(--ok)}.cfg-msg.bad{color:var(--bad)}
.cfg-toggle{display:flex;align-items:center;gap:9px;font-size:12.5px;color:#b7c7dd;cursor:pointer;padding:2px 0}
.cfg-toggle input{width:18px;height:18px;accent-color:#2ee6a6;cursor:pointer;flex:0 0 auto}

/* ── Responsywnosc ── */
@media(max-width:980px){
  .app{height:auto}
  .main-grid{grid-template-columns:1fr}
  .sky-bar-wrap{grid-template-columns:auto 1fr}
  .sky-reason-txt{grid-column:1/-1}
  .sky-cells{grid-column:1/-1;width:100%;grid-template-columns:repeat(4,1fr)}
  .metrics{grid-auto-rows:auto}
  .metric{min-height:78px}
  .roof-svg-wrap{min-height:190px}#roofSvg{height:220px}
  .lower-grid{grid-template-columns:1fr}
  .diag-grid{grid-template-columns:repeat(2,1fr)}
}
@media(max-width:560px){
  .topbar{align-items:flex-start;flex-direction:column}
  .topbar-right{width:100%;justify-content:space-between}
  .logo-hw{max-width:88vw}
  .logo-clock{max-width:88vw}
  .logo-sub{max-width:88vw}
  .sky-status{font-size:20px}
  .sky-cells{grid-template-columns:repeat(2,1fr)}
  .metric .v{font-size:21px}
  .aux-grid{grid-template-columns:repeat(4,1fr)}
  .roof-svg-wrap{min-height:170px}#roofSvg{height:190px}
}

/* ══════════════════════════════════════════════════════════════════════
   EYE-CANDY: gwiazdozbior tla, animacje wejscia, spotlight kart,
   flash przy zmianie wartosci, ripple na przyciskach.
   Czysto kosmetyczne - zero nowych ID/klas na elementach, ktore czyta JS
   logiki (refresh/cmd/...), wiec nic w istniejacej logice sie nie psuje.
   ══════════════════════════════════════════════════════════════════════ */
#starfield{position:fixed;inset:0;z-index:0;pointer-events:none}
.shooting-star{position:fixed;z-index:0;pointer-events:none;width:2px;height:2px;border-radius:50%;
  background:#fff;box-shadow:0 0 6px 1px #cfe3ff,0 0 0 1px rgba(255,255,255,.4);
  filter:drop-shadow(0 0 3px #9fc3ff);opacity:0;
  animation:shootAcross var(--sdur,1.4s) linear forwards}
.shooting-star::before{content:'';position:absolute;top:50%;right:0;width:var(--slen,120px);height:1px;
  background:linear-gradient(90deg,transparent,rgba(210,230,255,.9));transform:translateY(-50%)}
@keyframes shootAcross{
  0%{opacity:0;transform:translate(0,0)}
  4%{opacity:1}
  85%{opacity:.9}
  100%{opacity:0;transform:translate(var(--sx,-360px),var(--sy,220px))}
}

/* ── wejscie elementow: delikatny fade+rise, staggered ── */
@keyframes riseIn{from{opacity:0;transform:translateY(14px)}to{opacity:1;transform:translateY(0)}}
.topbar,.manual-banner.show,.estop-banner.show,.sky-bar-wrap,.main-grid>.card,.lower-grid>.card{
  animation:riseIn .62s cubic-bezier(.2,.7,.3,1) both}
.topbar{animation-delay:.02s}
.sky-bar-wrap{animation-delay:.09s}
.main-grid>.card:nth-child(1){animation-delay:.16s}
.main-grid>.card:nth-child(2){animation-delay:.23s}
.lower-grid>.card:nth-child(1){animation-delay:.30s}
.lower-grid>.card:nth-child(2){animation-delay:.37s}

/* ── karty: spotlight podazajacy za kursorem + delikatne unoszenie ── */
.card{--mx:50%;--my:0%;transition:transform .35s cubic-bezier(.2,.7,.3,1),border-color .35s;position:relative}
.card::after{content:'';position:absolute;inset:0;border-radius:var(--rad);pointer-events:none;opacity:0;
  transition:opacity .35s ease;
  background:radial-gradient(420px circle at var(--mx) var(--my),rgba(120,168,255,.12),transparent 62%)}
.card:hover::after{opacity:1}
.card:hover{transform:translateY(-2px);border-color:var(--line2)}

/* ── sky-icon: oddech + lekki dryf, glow wg stanu (kolor podbija JS przez inline style) ── */
.sky-icon{animation:skyFloat 4.4s ease-in-out infinite;will-change:transform}
@keyframes skyFloat{0%,100%{transform:translateY(0) scale(1)}50%{transform:translateY(-3px) scale(1.035)}}

/* ── flash: krotkie podswietlenie wartosci ktora sie wlasnie zmienila ── */
@keyframes flashVal{
  0%{text-shadow:0 0 0 rgba(120,168,255,0)}
  30%{text-shadow:0 0 14px rgba(120,168,255,.9)}
  100%{text-shadow:0 0 0 rgba(120,168,255,0)}
}
.val-flash{animation:flashVal .9s ease-out}

/* ── ripple na klikalnych elementach ── */
button,.aux-btn,.limit-box{position:relative;overflow:hidden}
.ripple-fx{position:absolute;border-radius:50%;background:rgba(255,255,255,.45);
  transform:scale(0);animation:rippleOut .55s ease-out forwards;pointer-events:none}
@keyframes rippleOut{to{transform:scale(2.6);opacity:0}}

/* ── delikatna poswiata calego topbara przy SAFE, oddychajaca ── */
.safety-pill.safe{animation:pillBreath 3.2s ease-in-out infinite}
@keyframes pillBreath{0%,100%{box-shadow:0 0 30px -6px rgba(46,230,166,.35)}50%{box-shadow:0 0 42px -4px rgba(46,230,166,.55)}}

@media(prefers-reduced-motion:reduce){
  #starfield,.shooting-star,.topbar,.manual-banner.show,.estop-banner.show,.sky-bar-wrap,
  .main-grid>.card,.lower-grid>.card,.sky-icon,.safety-pill.safe,.sqm-card{animation:none!important}
  .card{transition:none}
}

/* ══════════════════════════════════════════════════════════════════════
   KARTA "NATEZENIE OSWIETLENIA" (TSL2591) — wlasny gauge, poza siatka
   ══════════════════════════════════════════════════════════════════════ */
.sqm-card{animation:riseIn .62s cubic-bezier(.2,.7,.3,1) both;animation-delay:.12s}
.sqm-main{display:flex;align-items:baseline;justify-content:space-between;flex-wrap:wrap;gap:10px;margin:8px 0 16px}
.sqm-lux-wrap{display:flex;align-items:baseline;gap:7px}
.sqm-lux{font-family:var(--font-head);font-size:42px;font-weight:900;line-height:1;
  background:linear-gradient(135deg,#eaf3ff,#8fb8ff);-webkit-background-clip:text;background-clip:text;color:transparent}
.sqm-lux-unit{font-size:15px;color:var(--mut);font-weight:700}
.sqm-status{font-size:13px;font-weight:800;padding:6px 14px;border-radius:999px;border:1px solid var(--line);
  background:rgba(255,255,255,.03);white-space:nowrap;transition:color .4s,border-color .4s}
.sqm-gauge-wrap{margin:6px 0 2px}
.sqm-gauge{position:relative;height:10px;border-radius:999px;
  background:linear-gradient(90deg,#050a16 0%,#173a66 22%,#3f7ecb 48%,#ffd873 74%,#fff6dd 100%);
  box-shadow:inset 0 0 0 1px var(--line)}
.sqm-dot{position:absolute;top:50%;left:0%;width:16px;height:16px;border-radius:50%;background:#fff;
  border:3px solid #63a8ff;transform:translate(-50%,-50%);box-shadow:0 0 14px 2px rgba(120,168,255,.85);
  transition:left .9s cubic-bezier(.2,.7,.3,1)}
.sqm-gauge-labels{display:flex;justify-content:space-between;font-size:11px;color:var(--mut);margin-top:7px}
.sqm-raw{display:flex;gap:10px;margin-top:14px}
.sqm-raw-item{flex:1;display:flex;justify-content:space-between;align-items:center;padding:9px 13px;
  border-radius:10px;border:1px solid var(--line);background:rgba(255,255,255,.02);font-size:13px}
.sqm-raw-item span{color:var(--mut);min-width:0;line-height:1.45}
.sqm-raw-item{gap:12px;min-width:0}
.sqm-raw-item small{display:block;font-size:11px;margin-top:3px}
.sqm-raw-item b{flex-shrink:0}
.tsl-note{font-size:12px;line-height:1.5;color:var(--mut);margin:12px 0 0}
.sqm-raw-item b{font-family:var(--font-head);font-size:15px}
@media(max-width:600px){.sqm-lux{font-size:34px}.sqm-status{font-size:12px}.sqm-raw{flex-direction:column}.sqm-status{white-space:normal}}

/* v17.8 dashboard refresh: calmer contrast, cleaner cards, clearer states */
:root{
  --bg:#09131e;--card:#111e2c;--card2:#0e1925;
  --line:#233548;--line2:#38516a;
  --mut:#9aabbd;--txt:#e3ebf3;--tit:#f7f9fb;
  --ok:#35d39b;--bad:#ff647d;--warn:#ffc45d;--info:#63b7dc;--vio:#a99aff;
  --rad:18px;--rad2:12px;
  --shadow:0 14px 38px -22px rgba(0,0,0,.78);
  --hl:inset 0 1px 0 rgba(255,255,255,.035);
}
body{background:radial-gradient(80% 45% at 50% -10%,rgba(31,91,115,.18),transparent 70%),linear-gradient(180deg,#0b1622,#08111a 70%,#071019)}
body::before,#starfield,.shooting-star{display:none!important}
.app{width:min(1480px,100%);padding:clamp(12px,2vw,26px);gap:14px}
.topbar{background:rgba(16,29,43,.96);border-color:#263b50;border-radius:16px;padding:13px 18px}
.logo-icon{background:#142536;border-color:#2a4054;filter:none}
.logo-text{background:none;color:#f4f8fb}
.logo-sub{color:#a2b3c4}
.status-chip,.badge{font-size:10px;letter-spacing:.035em}
.btn-logs{background:#14283a;border-color:#31506a;box-shadow:none}
.sky-bar-wrap{background:linear-gradient(105deg,#142a39,#122333);border:1px solid #294257;border-radius:16px;box-shadow:none}
.sky-icon{filter:none}
.card{background:#111e2c;border-color:#24374b;border-radius:16px;padding:17px;gap:12px;box-shadow:0 12px 34px -22px rgba(0,0,0,.75)}
.card::before{background:linear-gradient(90deg,transparent 5%,rgba(65,190,174,.35),transparent 95%)}
.card::after{display:none}
.card:hover{transform:none;border-color:#35536a}
.card-title{font-size:13px;letter-spacing:.045em;text-transform:none;color:#f1f6fa}
.card-title::before{background:#39cdb2;box-shadow:none}
.metric,.sky-cell,.diag-cell,.sqm-raw-item{background:#152536;border-color:#263c50;box-shadow:none;border-radius:13px}
.metric{min-height:74px;padding:13px 14px}
.metric .v{font-size:25px;color:#f4f7fa}
.metric .k,.ck,.roof-label,.lk,.aux-btn .an{color:#a1b3c4;letter-spacing:.08em}
.roof-svg-wrap{background:radial-gradient(circle at 50% 28%,rgba(61,144,170,.12),transparent 60%);border-color:#21394d;border-radius:15px}
.limits{gap:9px}
.limit-box{background:#152536;border-color:#263c50;border-radius:12px}
.limit-box.active{background:rgba(53,211,155,.1);border-color:rgba(53,211,155,.48)}
.btn-open{background:#2367b7;box-shadow:none}
.btn-close{background:#b93850;box-shadow:none}
.btn-stop{background:#d29632;box-shadow:none}
.btn-hazard{background:#a62b3c;box-shadow:none}
.btn-pc,.btn-light{box-shadow:none}
.aux-btn{background:#152536;border-color:#263c50;border-radius:12px}
.diag-grid{grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}
.diag-cell{padding:10px 11px;gap:4px}
.diag-cell .dn{color:#a1b3c4;font-size:8px;letter-spacing:.075em}
.diag-cell .dv{font-size:12px;color:#edf3f8}
.diag-section-heading{display:flex;align-items:baseline;justify-content:space-between;gap:10px;padding:5px 1px 7px;border-bottom:1px solid #273a4d;color:#eef4f8;font-size:11px;font-weight:800;letter-spacing:.045em;text-transform:uppercase}
.diag-section-heading small{color:#8fa2b5;font-size:9px;font-weight:600;letter-spacing:.04em;text-transform:none}
.input-diag-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}
.input-diag-cell{min-height:82px}
.input-diag-head{display:flex;align-items:center;gap:7px;min-width:0}
.input-tag{flex:0 0 auto;border:1px solid #38536a;border-radius:6px;padding:2px 5px;color:#93cddd;font:700 8px var(--font-mono);letter-spacing:.03em}
.input-diag-cell .dn{white-space:normal;line-height:1.25}
.diag-cell.idle{border-color:#2a3d50;background:#142230}
.diag-cell.idle .ds{color:#9bafc1}
.diag-cell.warn{border-color:rgba(255,196,93,.5);background:rgba(255,196,93,.08)}
.diag-cell.warn .ds{color:#ffd17b}
.diag-cell.off{opacity:.72}
.diag-foot{color:#9aabbd;border-top:1px solid #273a4d}
.relay-state-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:7px}
  .relay-state-row{display:grid;grid-template-columns:minmax(92px,1fr) auto auto;align-items:center;gap:9px;padding:9px 10px;background:#152536;border:1px solid #263c50;border-radius:11px;font-size:11px}
  .relay-state-row.mismatch{border-color:rgba(255,100,125,.7);background:rgba(255,100,125,.08)}
  .relay-name{color:#e3ebf3;font-weight:700;white-space:nowrap}
  .relay-state-val{color:#a1b3c4;font-family:var(--font-mono);font-size:10px;white-space:nowrap}
  .relay-state-val b{color:#f1f6fa}
  .relay-state-summary{font-size:9px;font-weight:700;color:#9aabbd}
  .relay-state-summary.online{color:#35d39b}
  .relay-state-summary.offline{color:#ff8798}
  .relay-state-summary.unknown{color:#ffc45d}
.sqm-gauge{box-shadow:inset 0 0 0 1px #31465a}
.sqm-status{background:#142536}
.manual-banner,.estop-banner,.bench-banner{border-radius:14px;box-shadow:none}
.bench-banner{color:#ffe2a5}
button:focus-visible,a:focus-visible,.aux-btn:focus-visible{outline:2px solid #74c8da;outline-offset:3px}
.topbar,.manual-banner.show,.estop-banner.show,.sky-bar-wrap,.main-grid>.card,.lower-grid>.card{animation:none}
.sky-icon,.safety-pill.safe{animation:none}
@media(max-width:1100px){.diag-grid,.input-diag-grid{grid-template-columns:repeat(3,minmax(0,1fr))}}
@media(max-width:980px){.diag-grid,.input-diag-grid{grid-template-columns:repeat(2,minmax(0,1fr))}}
@media(max-width:560px){.app{padding:10px}.card{padding:13px}.diag-grid,.input-diag-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.input-diag-cell{min-height:78px}}
@media(max-width:560px){.relay-state-grid{grid-template-columns:1fr}.relay-state-row{grid-template-columns:minmax(92px,1fr) auto auto}}
/* ASTRO-ROOF compact observatory palette; all status and sensor sections stay in place. */
:root{--bg:#07131a;--card:rgba(15,31,39,.92);--card2:rgba(9,22,29,.86);--line:rgba(151,190,192,.16);--line2:rgba(151,190,192,.30);--info:#57c8bc;--vio:#90a7ff;--rad:16px;--rad2:11px}
body{background:linear-gradient(180deg,#07131a 0%,#0c1b22 54%,#071117 100%)}
body::before{opacity:.06}
.app{width:min(1320px,100%);padding:10px;gap:10px}
.topbar{padding:8px 12px;min-height:54px}
.card{padding:12px;gap:9px}
.sky-bar-wrap{padding:10px 12px;gap:10px}
.btn-restart{border:1px solid rgba(255,190,115,.4);border-radius:999px;padding:9px 12px;background:rgba(115,68,29,.22);color:#ffe0bc;font:800 11px var(--font-head);letter-spacing:.04em;cursor:pointer;white-space:nowrap;transition:filter .15s,transform .15s}
.btn-restart:hover{filter:brightness(1.18);transform:translateY(-1px)}
.btn-restart:focus-visible{outline:2px solid #ffd09a;outline-offset:3px}
.btn-restart:disabled{opacity:.55;cursor:wait}

/* Compact/full mode changes presentation only. Emergency controls stay outside details. */
.panel-details>summary{cursor:pointer;list-style-position:inside;font-weight:800;color:var(--tit);padding:5px 0}
.panel-details[open]>summary{margin-bottom:12px}
.panel-details>summary:focus-visible{outline:2px solid var(--info);outline-offset:4px}
.sub-details{margin-top:10px;border-top:1px solid var(--line);padding-top:8px}
.stale-banner{padding:12px;border:1px solid var(--warn);border-radius:12px;color:#ffe4ab;background:#3a2714}
.stale-banner[hidden]{display:none}
.park-overview{font-size:12px;line-height:1.5;border:1px solid var(--line);padding:9px;border-radius:10px;margin-bottom:10px}
.park-overview>div{overflow-wrap:anywhere}
.data-stale .content{opacity:.55}
.compact-panels .roof-svg-wrap{display:none}
.compact-panels .card{min-height:0}
.compact-panels .content{grid-template-rows:auto;align-content:start}
.compact-panels .lower-grid{margin-top:0}
@media(max-width:980px){.compact-panels #roofCard{order:-1}}


.manual-dialog{background:#111b23;color:#e4edf4;border:1px solid #456375;border-radius:14px;padding:24px;width:min(480px,calc(100vw - 32px));max-height:85vh;overflow:auto;box-sizing:border-box}
.manual-dialog::backdrop{background:rgba(0,0,0,.7)}
.manual-dialog h2{margin:0 0 14px}.manual-dialog p{line-height:1.5}
.manual-dialog label{display:block;margin:16px 0 6px}.manual-dialog input{box-sizing:border-box;width:100%;padding:10px;background:#0b1218;color:#fff;border:1px solid #567083;border-radius:6px;font-size:16px}
.manual-dialog .manual-actions{display:flex;gap:12px;justify-content:flex-end;margin-top:20px}
.manual-error{color:#ffb4a5;min-height:1.5em;white-space:pre-wrap}
.control-policy{line-height:1.5;color:#b6c9d6;margin:10px 0}

.logo-text{text-transform:uppercase;overflow-wrap:anywhere}
.observatory-subtitle{font-size:12px;line-height:1.6;color:#bed1df}
.observatory-system{font-size:10px;letter-spacing:1.5px;font-weight:800;color:#7edbd0;margin:3px 0 6px}
@media(max-width:600px){.logo-text{font-size:21px}.observatory-subtitle{font-size:11px}}
/* UI 2026-11-03: presentation only; IDs, controls, status meanings and order retained. */
body,button,input,select,summary{font-family:'Segoe UI',system-ui,-apple-system,sans-serif}
:root{--bg:#080f1a;--card:#101e2c;--card2:#142638;--line:#263b4e;--line2:#3f6078;--txt:#e7eff7;--tit:#f7fbff;--mut:#a7b8c9;--info:#69dfcf;--ok:#45dfa9;--warn:#ffcf79;--bad:#ff7a89;--font-head:'Segoe UI',system-ui,sans-serif}
body{background:radial-gradient(ellipse at 8% 0%,#123340 0%,transparent 42%),radial-gradient(ellipse at 95% 20%,#18233e 0%,transparent 48%),#080f1a;background-attachment:fixed;color:var(--txt)}
.app{width:min(1420px,100%);padding:20px;gap:16px}
.topbar{padding:20px 22px;border:1px solid #355364;border-radius:22px;background:linear-gradient(120deg,#142d3a,#101c2e);box-shadow:0 16px 48px #0004;gap:22px}
.logo-icon{width:50px;height:50px;border-radius:16px;background:#183e48;border:1px solid #41847f;font-size:27px}
.logo-text{font-size:25px;letter-spacing:-.7px;color:#f6fbff}
.logo-text::before{content:'PANEL OBSERWATORIUM';display:block;font-size:9px;letter-spacing:2.3px;font-weight:800;color:#7edbd0;margin-bottom:5px}
.logo-hw{color:#9cb2c3;font-size:9px;line-height:1.6;max-width:470px}
.logo-sub,.logo-clock{line-height:1.6;color:#b3c6d6}
.topbar-right{gap:8px}
.card{border-radius:20px;padding:18px;background:linear-gradient(155deg,#132535,#101c2a);border:1px solid #2a4054;box-shadow:0 12px 32px #0002;gap:12px}
.card::before,.card::after{display:none}
.card:hover{transform:none;border-color:#3a5a70}
.panel-details>summary{font-size:14px;padding:5px 0;line-height:1.5;letter-spacing:.05px}
.panel-details[open]>summary{padding-bottom:13px;margin-bottom:12px;border-bottom:1px solid #2b4052}
.sub-details>summary{font-size:12px;color:#bfd3e3}
.sky-bar-wrap{border-radius:21px;padding:18px 22px;background:linear-gradient(115deg,#153642,#14293c);border-color:#36586b;box-shadow:0 12px 32px #0002}
.sky-status{letter-spacing:-.35px}.sky-cells{gap:10px}
.sky-cell,.metric,.diag-cell,.input-diag-cell,.sqm-raw-item{border:1px solid #2c465b;border-radius:14px;background:#14283a;box-shadow:none}
.metric{padding:15px;min-height:82px}.metric .k{font-size:10px;letter-spacing:.6px}.metric .v{font-size:28px;letter-spacing:-.7px}
.diag-cell,.input-diag-cell{padding:12px;line-height:1.5}
.sqm-lux{font-size:46px;letter-spacing:-1.8px;background:none;color:#f1f8ff}
.sqm-status{background:#153743;border-color:#3c727c;color:#bdece8}
.sqm-gauge{height:8px}.sqm-gauge-labels{margin-top:10px}.sqm-raw-item{padding:13px 15px}
.tsl-note{color:#afc0cf;max-width:65ch}
.park-overview{background:#0b1925;border:1px solid #2b495d;padding:13px;border-radius:14px;line-height:1.7}
.roof-svg-wrap{border-radius:17px;background:radial-gradient(ellipse at 50% 20%,#173e55,#0c1a29 80%)}
.btn-logs,.btn-restart{border-radius:10px;padding:10px 13px;font-size:10px;letter-spacing:.25px;box-shadow:none}
.btn-logs{background:#193448;border-color:#3d627a;color:#d9edf6}
.btn-restart{background:#3a2c20;border-color:#806141;color:#ffe0b8}
.status-chip{padding:8px 10px;border-radius:9px;font-size:9px}
.btn-manual-on,.btn-manual-off,.safety-pill{border-radius:10px}
.roof-controls{gap:10px}.roof-controls button{border-radius:12px;min-height:46px;font-size:12px;box-shadow:inset 0 1px 0 #ffffff24}
.btn-open{background:#245ba0}.btn-close{background:#9b3e53}.btn-stop{background:#f1ba52;color:#211906}
.btn-hazard{border-radius:12px;min-height:44px;background:#a52f47;border:1px solid #ee7e91;color:#fff}
.btn-pc,.btn-light,.btn-save,.btn-default,.btn-seed,.btn-forget{border-radius:11px;min-height:40px}
button:disabled{cursor:not-allowed}button:focus-visible,summary:focus-visible,a:focus-visible{outline:3px solid #79e3d3;outline-offset:3px}
@media(max-width:980px){.topbar{align-items:flex-start}.topbar-right{justify-content:flex-start}.app{padding:14px;gap:12px}}
@media(max-width:600px){.app{padding:10px;gap:11px}.topbar{padding:16px;border-radius:18px;gap:14px}.logo-text{font-size:23px}.logo-icon{width:43px;height:43px}.topbar-right{gap:7px}.card{padding:15px;border-radius:17px}.sky-bar-wrap{padding:15px;border-radius:17px}.panel-details>summary{font-size:13px}.sqm-lux{font-size:40px}.metric .v{font-size:25px}.sqm-raw{gap:9px}.diag-grid,.input-diag-grid{gap:8px}.diag-cell,.input-diag-cell{padding:9px}.roof-controls button{min-height:46px}.btn-logs,.btn-restart{padding:10px;font-size:9px}}
@media(prefers-reduced-motion:reduce){*,*::before,*::after{animation:none!important;transition:none!important;scroll-behavior:auto!important}}


/* Local OFL font: title only, no remote font request. */
@font-face{font-family:'Specula Fraktur';src:url(data:font/woff2;base64,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) format('woff2');font-weight:400;font-style:normal;font-display:swap}
.logo-text{font-family:'Specula Fraktur',Georgia,serif;font-size:clamp(28px,3.1vw,42px);font-weight:400;font-synthesis:none;line-height:1.25;text-transform:none;letter-spacing:.015em;white-space:normal;overflow-wrap:normal;color:#edcc86;background:linear-gradient(110deg,#fff1c8 4%,#e8bf70 46%,#c89545 76%,#f7e6b2 100%);-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent;text-shadow:0 1px 14px rgba(217,171,83,.13);filter:drop-shadow(0 0 9px rgba(213,166,68,.13))}
.logo-text::before{font-family:var(--font-ui);font-weight:800;font-size:9px;line-height:1.5;letter-spacing:2px;color:#80c9bf;-webkit-text-fill-color:#80c9bf;text-shadow:none}
@media(max-width:600px){.logo-text{font-size:clamp(26px,7.6vw,36px);letter-spacing:0}.logo>div:last-child{min-width:0}}

</style></head>
<body>
<div class="app">

<!-- TOP BAR -->
<div class="topbar">
  <div class="logo">
    <div class="logo-icon">🔭</div>
    <div>
      <div class="logo-text">)HTML" OBSERVATORY_NAME R"HTML(</div>
      <div class="observatory-subtitle">Obserwatorium zdalne</div>
      <div class="observatory-system">ASTRO-ROOF SAFE</div>
      <div class="logo-hw">SPRZET: WAVESHARE ESP32-S3-POE-ETH-8DI-8DO | DACH: WAVESHARE 17658 MODBUS/RS485</div>
      <div class="logo-sub" id="netText">Łączenie...</div>
      <div class="logo-clock" id="clockText">Zegar: sprawdzam RTC...</div>
    </div>
  </div>
  <div class="topbar-right">
    <button class="btn-logs" type="button" onclick="showNotifications()">Powiadomienia</button>
    <button class="btn-logs" id="btnPanels" type="button" onclick="togglePanels()">Pokaż wszystko</button>
    <span class="status-chip chip-off" id="chipLink">LINK</span>
    <span class="status-chip chip-off" id="chipData" title="Ostatnia odpowiedź urządzenia">DANE…</span>
    <span class="status-chip chip-off" id="chipPark">PARK</span>
    <a class="btn-logs" href="/log" aria-label="Pokaż logi zdarzeń">📋 POKAŻ LOGI</a>
    <button class="btn-restart" id="btnRestart" onclick="restartController()">↻ RESTART ESP</button>
    <button class="btn-manual-on" id="btnManualOn" onclick="setManual(true)">🔓 MANUAL</button>
    <div id="safetyPill" class="safety-pill unsafe">
      <span class="safety-dot"></span>
      <span id="safetyTxt" title="Ocena SafetyMonitor; warunek parku i pozostałe blokady ruchu są sprawdzane osobno.">UNSAFE</span>
    </div>
  </div>
</div>

<details class="card" id="panelNotifications" style="margin:12px 0">
  <summary class="card-title">Powiadomienia</summary>
  <p>Alarmy są dodatkiem do lokalnych zabezpieczeń. Test nie steruje dachem. Wyłączenie kanału nie zmienia SafetyMonitor.</p>
  <div id="notifyHealth">Sprawdzam stan…</div>
  <div id="notifyChannels"></div>
  <p>Klucze ustawiaj wyłącznie w secrets.ini. Heartbeat testuje świeże wykonanie głównej pętli, czujników i obsługi dachu. Brak pingu musi być monitorowany poza ESP32.</p>
</details>

<dialog id="manualDialog" class="manual-dialog" aria-labelledby="manualDialogTitle" oncancel="if(manualSubmitBusy)event.preventDefault();else closeManualDialog()">
  <form onsubmit="submitManual(event)" novalidate>
    <h2 id="manualDialogTitle">Sterowanie ręczne — MANUAL</h2>
    <p>Możesz otworzyć dach bez NINA i przy zwykłych chmurach. Deszcz, silny wiatr i brak wiarygodnych danych pogodowych blokują OPEN. PARK i aktywne zabezpieczenia ruchu pozostają wymagane.</p>
    <p>Wybranie trybu nie uruchamia napędu. Ruch zlecasz osobno przyciskami OPEN / CLOSE.</p>
    <p id="manualWindNotice"></p>
    <label for="manualPassword">Hasło panelu WWW</label>
    <input id="manualPassword" type="password" autocomplete="current-password" required>
    <p>MANUAL nie wygasa i pozostaje po restarcie. Po restarcie ponownie podaj hasło, aby odblokować sterowanie. Każdy ręczny OPEN/CLOSE wymaga hasła i potwierdzenia. Deszcz i silny wiatr mogą zażądać zamknięcia.</p>
    <p id="manualDialogError" class="manual-error" role="status" aria-live="polite"></p>
    <div class="manual-actions">
      <button type="button" class="btn-stop" onclick="cmd('stop')">■ STOP dachu</button>
      <button type="button" class="btn" id="manualCancel" onclick="closeManualDialog()">Anuluj</button>
      <button type="submit" class="btn-manual-on" id="manualSubmit">Włącz MANUAL</button>
    </div>
  </form>
</dialog>
<!-- MANUAL BANNER (slim row) -->
<div class="manual-banner" id="manualBanner">
  <div class="mb-left">
    <div class="mb-title">⚠️ TRYB RĘCZNY</div>
    <div class="mb-sub">Zwykłe chmury i brak NINA: ręcznie; deszcz i wiatr chronione · <strong id="manualCountdown">Bez automatycznego powrotu</strong> · powrót do AUTO może zamknąć dach przy złej pogodzie · restart zachowuje MANUAL, ale blokuje autoryzację ruchu</div>
  </div>
  <button class="btn-manual-on" onclick="setManual(true)">Odblokuj MANUAL</button>
  <button class="btn-manual-off" onclick="setManual(false)">🔒 POWRÓT DO AUTO</button>
</div>

<!-- BANER E-STOP / UTRATA LACZNOSCI / BRAK LINKU -->
<div class="estop-banner" id="estopBanner">
  <div class="eb-left">
    <div class="eb-title" id="estopTitle">⛔ OCHRONA MONTAŻU</div>
    <div class="eb-sub" id="estopSub">Krańcówka overtravel zadziałała</div>
  </div>
  <button class="btn-estop-reset" id="btnEstopReset" onclick="estopReset()">↺ PRZYWRÓĆ ZASILANIE MONTAŻU</button>
</div>

<div class="bench-banner" id="benchBanner" role="status">
  <b>BRAK ŁĄCZNOŚCI Z MODUŁEM DACHU</b>
  <span>Brak potwierdzenia komunikacji Modbus z Waveshare 17658. Nie jest to informacja o włączeniu trybu testowego.</span>
</div>
<div class="stale-banner" id="staleBanner" role="alert">Oczekiwanie na aktualne dane ESP32.</div>
<div class="aux-banner" id="auxBanner" role="status">
AUX1=CH7, AUX2=CH6, AUX3=CH8, AUX4=CH4, AUX5=CH5. Kanał zajęty przez włączoną funkcję systemową nie jest dostępny jako AUX.
</div>

<!-- CONTENT AREA -->
<div class="content">

  <!-- SKY STATUS BAR -->
  <div class="sky-bar-wrap">
    <div class="sky-icon" id="skyIcon">❓</div>
    <div class="sky-status" id="skyStatus" style="color:var(--mut)">Oczekiwanie...</div>
    <div class="sky-reason-txt" id="skyReason"></div>


    <div class="sky-cells">
      <div class="sky-cell"><div class="ck">MLX Delta</div><div class="cv" id="sbDelta">—</div></div>
      <div class="sky-cell"><div class="ck">Wiatr śr/podmuch</div><div class="cv" id="sbWind">—</div></div>
      <div class="sky-cell"><div class="ck">RG11</div><div class="cv" id="sbRain">—</div></div>
      <div class="sky-cell" id="uvCell"><div class="ck">UV</div><div class="cv" id="sbUv">—</div></div>
    </div>
  </div>

  <!-- NATEZENIE OSWIETLENIA (TSL2591) -->
  <div class="card sqm-card" id="sqmCard">
    <details class="panel-details" id="panel0"><summary>💡 Natężenie oświetlenia (TSL2591)</summary>
    <div class="sqm-main">
      <div class="sqm-lux-wrap">
        <div class="sqm-lux" id="tslLux">—</div>
        <div class="sqm-lux-unit">lx</div>
      </div>
      <div class="sqm-status" id="tslStatus">Oczekiwanie na dane...</div>
    </div>
    <div class="sqm-gauge-wrap">
      <div class="sqm-gauge"><div class="sqm-dot" id="sqmDot"></div></div>
      <div class="sqm-gauge-labels"><span>◔ Ciemno</span><span>◑ Umiarkowanie</span><span>● Jasno</span></div>
    </div>
    <div class="sqm-raw">
      <div class="sqm-raw-item"><span>Kanał szerokopasmowy (VIS + IR)<small>Surowy odczyt — nie lx</small></span><b id="tslFull">—</b></div>
      <div class="sqm-raw-item"><span>Kanał podczerwieni (IR)<small>Surowy odczyt — nie lx</small></span><b id="tslIr">—</b></div>
    </div>
    <p class="tsl-note">Pomiar natężenia oświetlenia. Nie jest to pomiar SQM ani ocena w skali Bortle’a.</p>
  </details></div>

  <!-- THREE PWM HEATERS -->
  <div class="card heaters-panel" id="heatersPanel">
    <details class="panel-details" id="panel1"><summary>🔥 Grzałki 12 V — automatyka punktu rosy i sterowanie ręczne</summary>
    <div class="heater-module-state" id="heaterModuleStatus">Wyłączone do czasu konfiguracji sprzętu</div>
    <div class="heater-channel-grid">
      <div class="heater-channel"><div class="hn">GRZAŁKA 1</div><div><div class="hv" id="heaterDuty1">0% · OFF</div><div class="hw" id="heaterPower1">0 / 20 W (szacunek)</div></div></div>
      <div class="heater-channel"><div class="hn">GRZAŁKA 2</div><div><div class="hv" id="heaterDuty2">0% · OFF</div><div class="hw" id="heaterPower2">0 / 20 W (szacunek)</div></div></div>
      <div class="heater-channel"><div class="hn">GRZAŁKA 3</div><div><div class="hv" id="heaterDuty3">0% · OFF</div><div class="hw" id="heaterPower3">0 / 20 W (szacunek)</div></div></div>
    </div>
    <div id="heaterManual">
      <div class="hm-row off" id="hmRow1"><span class="hm-n">Grzałka 1</span><button class="hm-mode" id="hmMode1" onclick="heaterMode(1)">AUTO</button><input type="range" id="hmRange1" min="0" max="100" step="5" value="0" disabled oninput="hmShow(1)" onchange="heaterSet(1)"><span class="hm-val" id="hmVal1">0%</span></div>
      <div class="hm-row off" id="hmRow2"><span class="hm-n">Grzałka 2</span><button class="hm-mode" id="hmMode2" onclick="heaterMode(2)">AUTO</button><input type="range" id="hmRange2" min="0" max="100" step="5" value="0" disabled oninput="hmShow(2)" onchange="heaterSet(2)"><span class="hm-val" id="hmVal2">0%</span></div>
      <div class="hm-row off" id="hmRow3"><span class="hm-n">Grzałka 3</span><button class="hm-mode" id="hmMode3" onclick="heaterMode(3)">AUTO</button><input type="range" id="hmRange3" min="0" max="100" step="5" value="0" disabled oninput="hmShow(3)" onchange="heaterSet(3)"><span class="hm-val" id="hmVal3">0%</span></div>
    </div>
    <div class="heater-note">Moc jest szacowana z wypełnienia PWM i wpisanej mocy grzałki, nie mierzona. Tryb AUTO używa temperatury i wilgotności do obliczenia punktu rosy; błędne albo nieaktualne dane oznaczają 0%. Tryb RĘCZNIE działa tylko dla kanału włączonego w ustawieniach, nie jest zapisywany po restarcie i po ustawionym czasie (domyślnie 120 min) wraca sam do AUTO.</div>
  </details></div>

  <!-- 2-COLUMN MAIN GRID -->
  <div class="main-grid">

    <!-- LEFT: WEATHER -->
    <div class="card">
      <details class="panel-details" id="panel2"><summary>🌡 Pogoda</summary>
      <div class="metrics">
        <div class="metric"><div class="k"><i>🌡</i>T powietrza</div><div class="v"><span id="tAir">—</span><small>°C</small></div></div>
        <div class="metric"><div class="k"><i>💧</i>Wilgotność</div><div class="v"><span id="hAir">—</span><small>%</small></div></div>
        <div class="metric"><div class="k"><i>🌫</i>Pkt rosy</div><div class="v"><span id="dew">—</span><small>°C</small></div></div>
        <div class="metric"><div class="k"><i>📊</i>Ciśnienie</div><div class="v"><span id="press">—</span><small>hPa</small></div></div>
        <div class="metric"><div class="k"><i>🌌</i>T niebo</div><div class="v"><span id="tSky">—</span><small>°C</small></div></div>
        <div class="metric"><div class="k"><i>📡</i>T otocz.</div><div class="v"><span id="tAmb">—</span><small>°C</small></div></div>
        <div class="metric" id="luxMetric"><div class="k"><i>✨</i>SkyLux</div><div class="v"><span id="lux">—</span><small>lx</small></div></div>
        <div class="metric" id="tExtMetric"><div class="k"><i>🌲</i>T zewn.</div><div class="v"><span id="tExt">—</span><small>°C</small></div></div>
      </div>
      <div class="heater-warn" id="heaterWarn">🔥 Wysoka wilgotność</div>
    </details></div>

    <!-- RIGHT: ROOF -->
    <div class="card" id="roofCard">
      <div class="card-title">🏠 Dach — Roll-Off</div>
      <div class="park-overview" id="parkOverview">Oczekiwanie na odczyt parku i łączności.</div>

      <!-- ROLL-OFF SVG ANIMATION -->
      <div class="roof-svg-wrap">
        <svg id="roofSvg" viewBox="0 0 420 240" xmlns="http://www.w3.org/2000/svg">
          <defs>
            <filter id="softGlow"><feGaussianBlur stdDeviation="10"/></filter>
            <linearGradient id="nightRoof" x1="0" y1="0" x2="1" y2="1">
              <stop offset="0%" stop-color="#173a65"/>
              <stop offset="55%" stop-color="#0b2342"/>
              <stop offset="100%" stop-color="#061122"/>
            </linearGradient>
            <linearGradient id="wallGrad" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stop-color="#102b4a"/>
              <stop offset="100%" stop-color="#060d1b"/>
            </linearGradient>
            <linearGradient id="floorGrad" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stop-color="#12233a"/>
              <stop offset="100%" stop-color="#07101d"/>
            </linearGradient>
          </defs>

          <!-- Niebo i gwiazdy -->
          <rect x="0" y="0" width="420" height="240" rx="22" fill="rgba(2,6,16,.12)"/>
          <g opacity=".75">
            <circle class="twinkle" cx="42" cy="32" r="1.3" fill="#d8eaff"/>
            <circle class="twinkle" cx="88" cy="18" r=".9" fill="#d8eaff"/>
            <circle class="twinkle" cx="142" cy="37" r="1.1" fill="#d8eaff"/>
            <circle class="twinkle" cx="226" cy="20" r="1.4" fill="#d8eaff"/>
            <circle class="twinkle" cx="319" cy="34" r="1" fill="#d8eaff"/>
            <circle class="twinkle" cx="375" cy="24" r="1.2" fill="#d8eaff"/>
            <circle class="twinkle" cx="62" cy="76" r=".8" fill="#d8eaff"/>
            <circle class="twinkle" cx="348" cy="80" r=".7" fill="#d8eaff"/>
          </g>

          <!-- Blask przy otwartym dachu -->
          <ellipse id="roofGlow" cx="210" cy="118" rx="82" ry="28" fill="#25e6a4" opacity="0" filter="url(#softGlow)"/>

          <!-- Szyny roll-off -->
          <line x1="25" y1="117" x2="395" y2="117" stroke="#62a8ff" stroke-width="2" opacity=".28" stroke-dasharray="8 6"/>
          <line x1="25" y1="126" x2="395" y2="126" stroke="#62a8ff" stroke-width="1.4" opacity=".18" stroke-dasharray="8 6"/>

          <!-- Budynek -->
          <rect x="72" y="116" width="276" height="82" rx="8" fill="url(#wallGrad)" stroke="#244c78" stroke-width="1.4"/>
          <rect x="88" y="132" width="64" height="49" rx="4" fill="#07101d" stroke="#173a65" opacity=".95"/>
          <rect x="268" y="132" width="48" height="49" rx="4" fill="#07101d" stroke="#173a65" opacity=".95"/>
          <rect x="66" y="198" width="288" height="12" rx="4" fill="url(#floorGrad)" stroke="#1a3a5c"/>

          <!-- Montaż i teleskop -->
          <g opacity=".95">
            <line x1="210" y1="168" x2="210" y2="126" stroke="#2f5f90" stroke-width="7" stroke-linecap="round"/>
            <circle cx="210" cy="124" r="12" fill="#183a60" stroke="#62a8ff" stroke-width="1"/>
            <g transform="translate(210 124) rotate(-28)">
              <rect x="-12" y="-7" width="72" height="14" rx="7" fill="#1b4674" stroke="#7eb8ff" stroke-width="1"/>
              <circle cx="62" cy="0" r="8" fill="#07101d" stroke="#7eb8ff"/>
              <line x1="-10" y1="0" x2="-28" y2="0" stroke="#315d8d" stroke-width="5" stroke-linecap="round"/>
            </g>
            <path d="M210 168 L184 198 M210 168 L236 198" stroke="#2f5f90" stroke-width="5" stroke-linecap="round"/>
          </g>

          <!-- Park indicator -->
          <circle id="parkDot2" cx="332" cy="137" r="9" fill="#ff4d6d"/>
          <text x="332" y="141" text-anchor="middle" font-size="10" fill="#fff" font-family="sans-serif" font-weight="700">P</text>

          <!-- Lewy panel dachu: jedzie w lewo -->
          <g id="roofL" style="transition:transform .9s cubic-bezier(.4,0,.2,1)">
            <path d="M72 116 L104 84 H210 V116 Z" fill="url(#nightRoof)" stroke="#62a8ff" stroke-width="2"/>
            <path d="M104 84 H210 V92 H96 Z" fill="#2a65a6" opacity=".55"/>
            <circle cx="86" cy="119" r="6" fill="#07101d" stroke="#7eb8ff" stroke-width="1.3"/>
            <circle cx="198" cy="119" r="6" fill="#07101d" stroke="#7eb8ff" stroke-width="1.3"/>
          </g>

          <!-- Prawy panel dachu: jedzie w prawo -->
          <g id="roofR" style="transition:transform .9s cubic-bezier(.4,0,.2,1)">
            <path d="M210 84 H316 L348 116 H210 Z" fill="url(#nightRoof)" stroke="#62a8ff" stroke-width="2"/>
            <path d="M210 84 H316 L324 92 H210 Z" fill="#2a65a6" opacity=".55"/>
            <circle cx="222" cy="119" r="6" fill="#07101d" stroke="#7eb8ff" stroke-width="1.3"/>
            <circle cx="334" cy="119" r="6" fill="#07101d" stroke="#7eb8ff" stroke-width="1.3"/>
          </g>

          <!-- opis -->
          <text id="roofLabel" x="210" y="229" text-anchor="middle" font-size="14" fill="#7890aa" font-family="monospace" font-weight="700">UNKNOWN</text>
        </svg>
        <div class="fx" id="weatherFx">
          <div class="fx-clouds" id="fxClouds"></div>
          <div class="fx-gust"></div>
          <div class="fx-rain"></div>
        </div>
      </div>

      <!-- Park guard info -->
      <div class="park-banner" id="parkBanner">
        <span>🅿</span><span id="parkBannerText">Park Guard</span>
      </div>

      <!-- Roof state row -->
      <div class="roof-row">
        <div class="roof-label">Stan dachu</div>
        <div class="roof-state-val" id="roofState">—</div>
      </div>

      <!-- Limit switches -->
      <div class="limits">
        <div class="limit-box" id="limC"><div class="lk">Limit CLOSED</div><div class="lv" id="limCv">—</div></div>
        <div class="limit-box" id="limO"><div class="lk">Limit OPEN</div><div class="lv" id="limOv">—</div></div>
      </div>

      <div id="weatherPolicyBanner" class="control-policy" role="status"></div>
      <div id="controlPolicy" class="control-policy" role="status">Oczekiwanie na aktualny stan sterowania.</div>
      <!-- Buttons -->
      <div class="btns">
        <button class="btn-open"  id="btnOpen"  onclick="cmd('open')">▲ OPEN</button>
        <button class="btn-close" id="btnClose" onclick="cmd('close')">▼ CLOSE</button>
        <button class="btn-stop"  id="btnStop"  onclick="cmd('stop')">■ STOP</button>
      </div>
        <div id="faultRecovery" hidden>
          <p id="faultRecoveryReason"></p>
          <button class="btn" id="btnFaultAck" onclick="ackFault()" disabled>Potwierdź usunięcie błędu po inspekcji</button>
        </div>
        <div id="faultAckResult" role="status" aria-live="polite" hidden></div>

      <!-- awaryjny STOP montażu (panic) -->
      <div class="btns btns-1">
        <button class="btn-hazard" id="btnMountEstop" onclick="mountEstop()">⛔ OCHRONA MONTAŻU</button>
      </div>

      <details class="panel-details sub-details" id="panelAux"><summary>PC, światło, AUX i DO — sterowanie dodatkowe</summary>
      <!-- budzenie komputera (Wake-on-LAN) -->
      <div class="btns btns-1">
        <button class="btn-pc" id="btnPcWake" onclick="cmd('pcwake')">🖥️ OBUDŹ PC</button>
      </div>

      <!-- Lokalny iluminator TCA9554 i niezależna czerwona taśma PWM -->
      <div class="btns btns-1">
        <button class="btn-light" id="btnLight" onclick="toggleLight()">💡 ŚWIATŁO TCA</button>
        <button class="btn-light" id="btnRedLed" onclick="toggleRedLed()">🔴 CZERWONA TAŚMA</button>
      </div>
      <div class="led-row" id="ledRow"><span>Jasność taśmy COB</span><input type="range" id="ledRange" min="1" max="100" step="1" value="50" oninput="ledShow()" onchange="setLedBright()"><b id="ledVal">50%</b></div>

      <!-- AUX -->
      <div class="card-title">⚡ AUX</div>
      <div class="aux-grid" id="auxGrid">
        <span>Odczyt mapy AUX...</span>
      </div>

      <div id="localDoCard" hidden>
        <div class="card-title">DO1–DO7 · lokalne wyjścia ON/OFF</div>
        <div class="aux-grid" id="localDoGrid" style="grid-template-columns:repeat(auto-fit,minmax(110px,1fr))"></div>
        <div class="cfg-note">Osobne od przekaźników AUX. DO8 należy do ŚWIATŁA. Stan oznacza odczyt TCA, nie pomiar odbiornika.</div>
      </div>
      <!-- Footer -->
      <div class="foot-line">
        <span id="tcaWarn">⚠ Sterownik przekaźników niedostępny</span>
        <div style="display:flex;gap:4px">
          <span class="badge wifi" id="badgeWifi" style="display:none">WiFi</span>
          <span class="badge eth"  id="badgeEth"  style="display:none">ETH</span>
          <span class="badge">:11111</span>
        </div>
      </div>
      </details>
    </div>
  </div><!-- .main-grid -->

  <!-- DIAGNOSTYKA + PROGI -->
  <div class="lower-grid">
    <!-- DIAGNOSTYKA -->
    <div class="card">
      <details class="panel-details" id="panel4"><summary>🩺 Diagnostyka</summary>
      <div class="diag-section-heading"><span>Urządzenia i pomiary</span><small>status oraz odczyty</small></div>
      <div class="diag-grid" id="diagGrid"></div>
      <div class="diag-section-heading"><span>Wejścia cyfrowe</span><small>DI1-DI8</small></div>
      <div class="input-diag-grid" id="inputDiagGrid"></div>
      <div class="diag-section-heading"><span>WAVESHARE 17658 · przekaźniki</span><small id="modbusRelaySummary">Modbus: —</small></div>
      <div class="relay-state-grid" id="relayStateGrid"></div>
      <div class="diag-foot">ACT to odczyt cewek FC01, nie pomiar ruchu dachu. Odczyt starszy niż 3 s jest nieaktualny (UNKNOWN), także podczas ruchu bez odpytywania FC01.</div>
      <div class="diag-foot" id="diagFoot">—</div>
    </details></div>

    <!-- EDYTOWALNE PROGI -->
    <div class="card">
      <details class="panel-details" id="panel5"><summary>🎚 Progi bezpieczeństwa</summary>
      <div class="cfg-note">Zapisywane na stałe (NVS). Wymagane: <b>overcast &lt; scattered &lt; clear</b> oraz <b>reset ≤ trwały ≤ podmuch</b>.</div>


      <div class="cfg-sec">Udział czujników w SAFE / UNSAFE</div>
      <div class="cfg-note">Zaznaczony czujnik może blokować otwarcie i uruchamiać ochronę pogodową. Odznaczony nadal pokazuje pomiary. Awaria wybranego czujnika oznacza nieznane warunki i blokuje sesję. Blokady montażu, krańcówek i przekaźników pozostają zawsze aktywne.</div>
      <div id="weatherPolicyStatus" class="cfg-note" role="status">Oczekiwanie na konfigurację pogody.</div>
      <label class="cfg-toggle"><input type="checkbox" id="wp_rain" onchange="weatherPolicyDirty()"><span>Deszcz — RG11</span></label>
      <label class="cfg-toggle"><input type="checkbox" id="wp_wind" onchange="weatherPolicyDirty()"><span>Wiatr — średnia, porywy i wiarygodność anemometru</span></label>
      <label class="cfg-toggle"><input type="checkbox" id="wp_clouds" onchange="weatherPolicyDirty()"><span>Chmury — trwałe gęste zachmurzenie i dostępność MLX</span></label>
      <label class="cfg-toggle"><input type="checkbox" id="wp_humidity" onchange="weatherPolicyDirty()"><span>Trwała wysoka wilgotność</span></label>
      <label class="cfg-toggle"><input type="checkbox" id="wp_dew" onchange="weatherPolicyDirty()"><span>Trwałe ryzyko rosy — temperatura powietrza minus punkt rosy</span></label>
      <div class="cfg-note">Wilgotność i rosa są domyślnie tylko informacją. Ich pomiar pochodzi z istniejącego źródła pogody, nie z wybranego czujnika temperatury focusera.</div>
      <div class="cfg-rows">
       <label class="cfg-row"><span>Wilgotność UNSAFE od [%]</span><input id="wp_humidity_limit" type="number" min="50" max="100" step="1" oninput="weatherPolicyDirty()"></label>
       <label class="cfg-row"><span>Margines rosy UNSAFE do [°C]</span><input id="wp_dew_margin" type="number" min="0" max="10" step="0.1" oninput="weatherPolicyDirty()"></label>
       <label class="cfg-row"><span>Potwierdzenie wilgotności / rosy [s]</span><input id="wp_moisture_hold_sec" type="number" min="10" max="1800" step="1" oninput="weatherPolicyDirty()"></label>
       <label class="cfg-row"><span>Potwierdzenie gęstych chmur [s]</span><input id="wp_cloud_hold_sec" type="number" min="30" max="1800" step="1" oninput="weatherPolicyDirty()"></label>
       <label class="cfg-row"><span>Stabilna poprawa przed otwarciem AUTO [s]</span><input id="wp_reopen_sec" type="number" min="60" max="3600" step="1" oninput="weatherPolicyDirty()"></label>
      </div>
      <div class="cfg-note">Deszcz zachowuje szybką reakcję. Wilgotność wraca do normy 3 punkty procentowe poniżej progu, margines rosy 1°C powyżej progu. Zmiana wymaga potwierdzonego zamknięcia dachu i MANUAL; zapis nie uruchamia żadnego ruchu. Przycisk „Domyślne” poniżej nie zmienia wyboru czujników.</div>
      <button class="btn-save" id="btnWeatherPolicySave" onclick="saveWeatherPolicy()" disabled>Zapisz udział czujników i czasy</button>
      <button class="btn" onclick="discardWeatherPolicy()">Odrzuć niezapisane zmiany</button>
      <div id="weatherPolicyMessage" class="cfg-note" role="status"></div>

      <div class="cfg-sec">☁️ Chmury — MLX delta (°C)</div>
      <div class="cfg-note">Klasyfikacja nieba nie oznacza natychmiastowego UNSAFE. Alarm gęstych chmur wymaga czasu potwierdzenia ustawionego powyżej i uwzględnia histerezę, jeśli ochrona od chmur jest włączona.</div>
      <div class="cfg-rows">
        <label class="cfg-row"><span>CLEAR (czyste ≥)</span><input id="cfgCcClr" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>SCATTERED (małe chmury ≥)</span><input id="cfgCcSca" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>OVERCAST (UNSAFE poniżej)</span><input id="cfgCcOvc" type="number" step="1" inputmode="numeric"></label>
      </div>
      <label class="cfg-toggle"><input type="checkbox" id="cfgCcTrig" disabled><span>Udział chmur — wybór w sekcji powyżej</span></label>

      <div class="cfg-sec">💨 Wiatr — m/s</div>
      <div class="cfg-rows">
        <label class="cfg-row"><span>Ostrzeżenie (tylko UI)</span><input id="cfgWWarn" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>Trwały limit (UNSAFE)</span><input id="cfgWSus" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>Podmuch limit (UNSAFE)</span><input id="cfgWGust" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>Reset histerezy (poniżej)</span><input id="cfgWRst" type="number" step="1" inputmode="numeric"></label>
      </div>
      <label class="cfg-toggle"><input type="checkbox" id="cfgAnemEn" disabled><span>Pomiar anemometru dostępny w firmware — udział w ochronie wybierz powyżej</span></label>


      <div class="cfg-sec">Kalibracja anemometru</div>
      <div class="cfg-note">Najpierw potwierdź model i zasilanie czujnika. Wartości początkowe są propozycją, nie kalibracją. Zapis wymaga zamkniętego dachu i trybu MANUAL. Zapis nie zmienia progów zamknięcia.</div>
      <div class="cfg-rows">
<label class="cfg-row"><span>Korekcja ADC — mnożnik</span><input id="wc_gain" type="number" step="any" value="1"></label>
<label class="cfg-row"><span>Korekcja ADC — offset [V]</span><input id="wc_offset" type="number" step="any" value="0"></label>
<label class="cfg-row"><span>Rzeczywisty dzielnik napięcia</span><input id="wc_ratio" type="number" step="any" value="1"></label>
<label class="cfg-row"><span>Napięcie przy 0 m/s [V]</span><input id="wc_zero" type="number" step="any" value="0.4"></label>
<label class="cfg-row"><span>Napięcie górnego punktu [V]</span><input id="wc_top" type="number" step="any" value="2"></label>
<label class="cfg-row"><span>Prędkość górnego punktu [m/s]</span><input id="wc_maxMS" type="number" step="any" value="32.4"></label>
</div>
      <label class="cfg-row"><span>Model czujnika</span><input id="wc_model" type="text" maxlength="63" placeholder="Model z dokumentacji"></label>
      <label class="cfg-toggle"><input id="wc_verified" type="checkbox"><span>Potwierdziłem model, prawidłowe zasilanie, charakterystykę i pomiar ADC miernikiem</span></label>
      <button onclick="saveWindCalibration()">Zapisz kalibrację wiatru</button>
      <div class="cfg-note" id="wc_status">Pomiar niezweryfikowany</div>

      <div class="cfg-sec">🔥🔥 Trzy grzałki 12 V (moduł PWM Waveshare)</div>
      <div class="cfg-note">PWM CH1–CH3: główne grzałki, PWM CH4: ręczna jasność czerwonej taśmy COB. Wymaga modułu Modbus PWM 4CH pod innym adresem niż 17658. Obsługę modułu włączasz w hardware.ini; wyjścia startują OFF.</div>
      <div class="cfg-rows">
        <label class="cfg-row"><span>Grzałka 1 — moc znamionowa (W)</span><input id="cfgHeaterW1" type="number" min="1" max="60" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>Grzałka 2 — moc znamionowa (W)</span><input id="cfgHeaterW2" type="number" min="1" max="60" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>Grzałka 3 — moc znamionowa (W)</span><input id="cfgHeaterW3" type="number" min="1" max="60" step="1" inputmode="numeric"></label>
      </div>
      <label class="cfg-toggle"><input type="checkbox" id="cfgHeaterEn1"><span>Automatyka grzałki 1</span></label>
      <label class="cfg-toggle"><input type="checkbox" id="cfgHeaterEn2"><span>Automatyka grzałki 2</span></label>
      <label class="cfg-toggle"><input type="checkbox" id="cfgHeaterEn3"><span>Automatyka grzałki 3</span></label>

      <div class="cfg-sec">🤖 Automatyczna kalibracja chmur</div>
      <label class="cfg-toggle"><input type="checkbox" id="cfgAutoCloud"><span><b>Automatyczna kalibracja (samoucząca) — zalecane</b></span></label>
      <div class="cfg-note" id="acStatus">uczenie…</div>
      <div class="cfg-btns">
        <button class="btn-seed" id="btnCloudSeed" onclick="cloudSeed()">☀️ Ucz teraz: niebo czyste</button>
        <button class="btn-forget" id="btnCloudForget" onclick="cloudForget()">🗑 Zapomnij naukę</button>
      </div>
      <div class="cfg-note">Tapnij „niebo czyste” w bezchmurną noc — automat sam dobierze wszystko dla Twojego czujnika. Bez wpisywania liczb. Progi CLEAR/SCATTERED/OVERCAST wyżej znaczą teraz „ile °C poniżej czystego nieba”.</div>

      <div class="cfg-sec">🔄 MLX90614 — automatyczny czujnik zapasowy</div>
      <div class="cfg-btns">
        <button class="btn-mlx-primary" id="btnMlxPrimary" onclick="mlxSwitchover(0)">📡 Użyj MLX1</button>
        <button class="btn-mlx-secondary" id="btnMlxSecondary" onclick="mlxSwitchover(1)">📡 Użyj MLX2 (backup)</button>
      </div>
      <div class="cfg-note" id="mlxStatus">Aktualnie: MLX1</div>
      <div class="cfg-note">MLX1 (0x5A) jest domyślny. Po awarii firmware automatycznie przełączy bezpieczeństwo na sprawny MLX2 (0x5B). Powrót do MLX1 wykonaj ręcznie.</div>

      <div class="cfg-sec">🛰 Ręczna korekcja AAG (tylko gdy auto wyłączone)</div>
      <label class="cfg-toggle"><input type="checkbox" id="cfgSkyEn"><span>Korekcja nieba AAG (kompensuje wpływ T powietrza na deltę)</span></label>
      <div class="cfg-rows">
        <label class="cfg-row"><span>K1 (składnik liniowy)</span><input id="cfgSkyK1" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>K2</span><input id="cfgSkyK2" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>K3 (składnik wykładniczy)</span><input id="cfgSkyK3" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>K4</span><input id="cfgSkyK4" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>K5</span><input id="cfgSkyK5" type="number" step="1" inputmode="numeric"></label>
        <label class="cfg-row"><span>Offset T nieba (°C)</span><input id="cfgMlxSO" type="number" step="0.1" inputmode="decimal"></label>
        <label class="cfg-row"><span>Offset T otoczenia MLX (°C)</span><input id="cfgMlxAO" type="number" step="0.1" inputmode="decimal"></label>
      </div>
      <div class="cfg-note">Domyślne K: <b>33 / 0 / 4 / 100 / 100</b> (model AAG CloudWatcher). Delta liczona od T&nbsp;powietrza (BME280), nie od puszki MLX — grzałka nie zawyża odczytu.</div>

      <div class="cfg-sec">🌐 Sieć dla Telegram</div>
      <div class="cfg-rows">
        <label class="cfg-row"><span>Transport</span><select id="cfgNetMode"><option value="0">AUTO (ETH → WiFi)</option><option value="1">TYLKO Ethernet (kabel)</option><option value="2">TYLKO WiFi</option></select></label>
      </div>

      <div class="cfg-btns">
        <button class="btn-save" id="btnCfgSave" onclick="saveCfg()">💾 Zapisz progi</button>
        <button class="btn-default" id="btnCfgDef" onclick="resetCfg()">↺ Domyślne</button>
      </div>
      <div class="cfg-msg" id="cfgMsg"></div>
    </details></div>
  </div>

</div><!-- .content -->

</div><!-- .app -->
<div class="toast" id="toast"></div>

<script>
async function commandFetch(url,options={}){
  const u=new URL(url,location.origin);
  if(u.pathname==='/cmd'||u.pathname==='/relay'||u.pathname==='/relaytest'||u.pathname==='/api/wind/calibration'||u.pathname==='/api/weather/policy'||u.pathname==='/api/notifications'){
    const r=await fetch('/api/command-token',{cache:'no-store'});
    if(!r.ok)throw Error('Brak autoryzacji');
    const token=(await r.json()).token;
    return fetch(u.pathname,{...options,method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Command-Token':token},body:options.body||u.search.slice(1)});
  }
  return fetch(url,options);
}

const $=id=>document.getElementById(id);
function toast(msg,dur=1600){const t=$('toast');t.textContent=msg;t.classList.add('show');clearTimeout(window._tt);window._tt=setTimeout(()=>t.classList.remove('show'),dur);}
let stateRefreshBusy=false;
let lastRoofState=null;
function setDataStatus(ok,detail){
  const el=$('chipData'); if(!el)return;
  el.className='status-chip '+(ok?'chip-ok':'chip-bad');
  el.textContent=ok?'DANE LIVE':'BRAK DANYCH';
  el.title=detail|| (ok?'Urządzenie odpowiada':'Brak odpowiedzi urządzenia');
}
function fmt(v,d=1){if(v===null||v===undefined||!isFinite(+v))return'—';return(+v).toFixed(d);}
function fmtTime(sec){if(!sec||sec<=0)return'0:00';const m=Math.floor(sec/60),s=sec%60;return m+':'+(s<10?'0':'')+s;}

/* ── Sky (progi zgodne z firmware) ── */
function skyClass(d,lim={}){
  const clear=isNum(lim.cloud_clear)?lim.cloud_clear:16,scattered=isNum(lim.cloud_scattered)?lim.cloud_scattered:9,overcast=isNum(lim.cloud_overcast)?lim.cloud_overcast:3;
  if(!isNum(d))return{icon:'❓',txt:'Brak MLX',col:'#4a6480'};
  if(d>=clear)return{icon:'✨',txt:'CZYSTE',col:'#00e5a0'};
  if(d>=scattered) return{icon:'⛅',txt:'CZESC. CHMURY',col:'#7ee0a0'};
  if(d>=overcast) return{icon:'☁️',txt:'ZACHMURZENIE',col:'#ffb020'};
  return{icon:'🌧',txt:'GESTE CHMURY',col:'#ff3d5a'};
}
// wiatr klasyfikowany wg sredniej trwalej: <10 ok, <14 ostrzezenie, wyzej alarm
function wClass(w,lim={}){if(!isNum(w))return'';const warn=isNum(lim.wind_warn)?lim.wind_warn:10,limit=isNum(lim.wind_sustained)?lim.wind_sustained:14;if(w>limit)return'bad';if(w>warn)return'warn';return'ok';}
function uvClass(u){if(!isNum(u))return'';if(u>7)return'bad';if(u>3)return'warn';return'ok';}

// Neutralne pasma natezenia oswietlenia w luksach; nie wyznaczaja
// rodzaju zmierzchu, SQM ani klasy Bortle. Granice pasm bez zmian.
// fix: isFinite(null)===true w JS (klasyczna pulapka!) - trzeba jawnie
// odrzucic null/undefined PRZED isFinite, tak jak robi to fmt() gdzie indziej
// w tym pliku. Bez tego np. tslStatusFor(null) falszywie "przechodzil" jako
// liczba i dawal bledny status, a dalej .toFixed() na null wywalalo cala
// applyState() w polowie (stad puste diagnostyka/progi przy braku czujnika).
function isNum(v){ return v!==null && v!==undefined && typeof v!=='string' && isFinite(v); }
function tslStatusFor(lux){
  if(!isNum(lux)) return {txt:'Brak danych',col:'var(--mut)'};
  if(lux<0.001)  return {txt:'Bardzo ciemno',col:'#8fb8ff'};
  if(lux<0.01)   return {txt:'Bardzo ciemno',col:'#63a8ff'};
  if(lux<0.3)    return {txt:'Ciemno',col:'#a6c9ff'};
  if(lux<50)     return {txt:'Umiarkowane oświetlenie',col:'#ffd873'};
  if(lux<1000)   return {txt:'Jasno',col:'#ffb84d'};
  return {txt:'Bardzo jasno',col:'#ffe3a3'};
}
// pozycja na gauge - skala logarytmiczna od 0.0001 do 100000 lx (9 dekad)
function sqmPercent(lux){
  if(!isNum(lux)||lux<=0) return 0;
  var lo=Math.log10(0.0001), hi=Math.log10(100000);
  var v=(Math.log10(lux)-lo)/(hi-lo);
  return Math.max(0,Math.min(100,v*100));
}

/* ══ ROLL-OFF ROOF ANIMATION ══
   Lewy panel jedzie w lewo (translateX ujemny)
   Prawy panel jedzie w prawo (translateX dodatni)
   Wartość przesunięcia ~ 110px = szerokość jednego panelu
*/
const ROOF_SLIDE = 158; // px w jednostkach SVG

function animateRoof(state){
  const L=$('roofL'), R=$('roofR');
  const glow=$('roofGlow'), lbl=$('roofLabel');
  switch(state){
    case 'OPEN':
      L.style.transform=`translateX(-${ROOF_SLIDE}px)`;
      R.style.transform=`translateX(${ROOF_SLIDE}px)`;
      glow.style.opacity='0.2';
      lbl.textContent='OTWARTY';lbl.style.fill='#00e5a0';
      break;
    case 'OPENING':
      L.style.transform=`translateX(-${ROOF_SLIDE*0.6}px)`;
      R.style.transform=`translateX(${ROOF_SLIDE*0.6}px)`;
      glow.style.opacity='0.08';
      lbl.textContent='OTWIERANIE...';lbl.style.fill='#4d9fff';
      break;
    case 'CLOSING':
      L.style.transform=`translateX(-${ROOF_SLIDE*0.3}px)`;
      R.style.transform=`translateX(${ROOF_SLIDE*0.3}px)`;
      glow.style.opacity='0';
      lbl.textContent='ZAMYKANIE...';lbl.style.fill='#ffb020';
      break;
    case 'CLOSED':
      L.style.transform='';
      R.style.transform='';
      glow.style.opacity='0';
      lbl.textContent='ZAMKNIETY';lbl.style.fill='#4a6480';
      break;
    case 'STOPPED':
      L.style.transform=`translateX(-${ROOF_SLIDE*0.4}px)`;
      R.style.transform=`translateX(${ROOF_SLIDE*0.4}px)`;
      glow.style.opacity='0';
      lbl.textContent='ZATRZYMANY';lbl.style.fill='#ffb020';
      break;
    default:
      L.style.transform='';R.style.transform='';
      glow.style.opacity='0';
      lbl.textContent=state||'UNKNOWN';lbl.style.fill='#4a6480';
  }
}

function updateParkDot(parked,guardEnabled){
  const d=$('parkDot2');
  if(!guardEnabled)d.setAttribute('fill','#4a6480');
  else if(parked)  d.setAttribute('fill','#00e5a0');
  else             d.setAttribute('fill','#ff3d5a');
}

function renderHeaters(hs){
  hs=hs||{};
  const status=$('heaterModuleStatus');
  if(!hs.firmware_enabled){
    status.textContent='Funkcja wyłączona w konfiguracji firmware — wyjścia pozostają OFF.';
    status.className='heater-module-state bad';
  }else if(!hs.module_online){
    status.textContent='Moduł PWM 4CH (RS485, adres 2) offline — grzałki zablokowane na 0%.';
    status.className='heater-module-state bad';
  }else if(!hs.sensor_valid){
    status.textContent='Brak lub nieaktualne dane temperatury/wilgotności — bezpieczne 0%.';
    status.className='heater-module-state bad';
  }else{
    const margin=Number(hs.dew_margin_c);
    status.textContent='PWM online · T '+fmt(hs.t_air_c,1)+'°C · RH '+fmt(hs.humidity_pct,0)+'% · margines rosy '+(isFinite(margin)?fmt(margin,1)+'°C':'—');
    status.className='heater-module-state ok';
  }
  const channels=Array.isArray(hs.channels)?hs.channels:[];
  for(let i=0;i<3;i++){
    const ch=channels[i]||{};
    const duty=Math.max(0,Math.min(100,Number(ch.duty_pct)||0));
    const rated=Math.max(0,Number(ch.rated_w)||0);
    const estimated=Math.max(0,Number(ch.estimated_w)||0);
    const manual=!!ch.manual;
    const active=!!hs.firmware_enabled&&!!hs.module_online&&!!ch.enabled&&duty>0&&(manual||!!hs.sensor_valid);
    $('heaterDuty'+(i+1)).textContent=duty.toFixed(0)+'% · '+(active?(manual?'RĘCZNIE':'AUTO'):'OFF');
    {
      const row=$('hmRow'+(i+1)), mb=$('hmMode'+(i+1)), rg=$('hmRange'+(i+1));
      if(row&&mb&&rg){
        const usable=!!hs.firmware_enabled&&!!hs.module_online&&!!ch.enabled;
        row.classList.toggle('off',!usable);
        mb.disabled=!usable;
        mb.classList.toggle('manual',manual);
        mb.textContent=manual?'RĘCZNIE':'AUTO';
        rg.disabled=!(usable&&manual);
        if(document.activeElement!==rg){
          rg.value=manual?Math.round(Number(ch.manual_pct)||0):Math.round(duty);
          $('hmVal'+(i+1)).textContent=rg.value+'%';
        }
      }
    }
    $('heaterDuty'+(i+1)).style.color=active?'var(--ok)':'var(--mut)';
    $('heaterPower'+(i+1)).textContent=estimated.toFixed(1)+' / '+rated.toFixed(0)+' W (szacunek)';
  }
}


function parkPresentation(st){
  const a=st.park_alpaca||{}, x=st.safety_ext||{};
  const enabled=st.park_guard_enabled===true;
  const known=typeof st.park_ra==='boolean'&&typeof st.park_dec==='boolean';
  const hw=enabled&&known&&st.park_ra&&st.park_dec;
  const stable=hw&&st.park_hw_stable===true;
  const ready=enabled&&stable&&st.park_motion_ready===true;
  const hardware=!enabled?'RA/DEC: Park Guard wyłączony':!known?'RA/DEC: brak danych':
    'RA: '+(st.park_ra?'PARK':'poza parkiem')+' · DEC: '+(st.park_dec?'PARK':'poza parkiem')+
    (hw?(stable?' · stabilne ≥5 s':' · oczekiwanie na stabilny park'):'');
  const condition=(st.close_clearance_confirmed===true?'CLOSE: potwierdzony prześwit, bez czekania na park. OPEN: park wymagany. ':'')+(!enabled?'Warunek parku: wyłączony':ready?'Warunek parku: spełniony':'Warunek parku: niespełniony');
  const current=a.configured===true&&a.transport_ready===true&&a.success_seen===true&&a.last_attempt_ok===true&&
    a.current===true&&typeof a.value==='boolean'&&Number.isFinite(a.success_age_s)&&a.success_age_s>=0&&a.success_age_s<=15;
  let remote;
  if(current) remote='Alpaca montażu: '+(a.value?'PARK potwierdzony':'POZA PARKIEM')+' · odczyt '+a.success_age_s+' s temu';
  else if(a.configured===false) remote='Alpaca montażu: nieskonfigurowana';
  else {
    remote='Alpaca montażu: brak aktualnego potwierdzenia';
    if(a.attempted&&Number.isFinite(a.attempt_age_s)) remote+=' · próba '+a.attempt_age_s+' s temu';
    if(a.success_seen&&typeof a.last_value==='boolean'&&Number.isFinite(a.success_age_s)) remote+=' · ostatni poprawny: '+(a.last_value?'PARK':'POZA PARKIEM')+', '+a.success_age_s+' s temu (historyczny)';
  }
  const smSeen=x.safetymonitor_seen===true;
  const smAge=smSeen&&Number.isFinite(x.safetymonitor_age_s)&&x.safetymonitor_age_s>=0?x.safetymonitor_age_s:null;
  const ninaActive=x.nina_active===true&&x.safetymonitor_connected===true&&smAge!==null&&smAge<3;
  const nina='NINA / SafetyMonitor: '+(ninaActive?'AKTYWNY':'NIEAKTYWNY')+
    ' · Connected='+(x.safetymonitor_connected===true?'true':'false')+
    (smAge===null?' · brak poll od uruchomienia':' · poll '+smAge+' s temu');
  return {enabled,known,hw,stable,ready,hardware,condition,current,remote,ninaActive,nina};
}
let lastStateAt=0;
function markStateStale(detail){
  const banner=$('staleBanner');
  if(banner){banner.hidden=false;banner.textContent='BRAK AKTUALNYCH DANYCH ESP32. '+(detail||'')+
    (lastStateAt?' Ostatni poprawny odczyt '+Math.floor((Date.now()-lastStateAt)/1000)+' s temu.':'');}
  document.body.classList.add('data-stale');
  const txt=$('safetyTxt');if(txt)txt.textContent='BRAK DANYCH';
  const pill=$('safetyPill');if(pill){pill.className='safety-pill unsafe';pill.style.cssText='';}
  const park=$('chipPark');if(park){park.textContent='PARK: stare dane';park.className='status-chip chip-off';}
  if($('btnFaultAck'))$('btnFaultAck').disabled=true;
  for(const id of ['btnOpen','btnClose']){const b=$(id);if(b)b.disabled=true;}
  setDataStatus(false,detail||'Dane nieaktualne');
}
function checkStateAge(){if(lastStateAt&&Date.now()-lastStateAt>10000)markStateStale('Brak świeżego odczytu ponad 10 s.');}
function setPanelMode(compact){
  document.body.classList.toggle('compact-panels',compact);
  document.querySelectorAll('details.panel-details').forEach(d=>{d.open=!compact;});
  const button=$('btnPanels');if(button){button.textContent=compact?'Pokaż wszystko':'Zwiń szczegóły';button.setAttribute('aria-expanded',String(!compact));}
  const content=document.querySelector('.content'),main=document.querySelector('.main-grid');
  if(content&&main)content.insertBefore(main,compact?$('sqmCard'):document.querySelector('.lower-grid'));
  try{localStorage.setItem('astro-panel-mode',compact?'compact':'full');}catch(_){}
}
function togglePanels(){setPanelMode(!document.body.classList.contains('compact-panels'));}
function initPanels(){let compact=true;try{compact=localStorage.getItem('astro-panel-mode')!=='full';}catch(_){}setPanelMode(compact);}

/* ── Apply state ── */
function applyState(st){
  lastRoofState=st;
  renderFaultRecovery(st);
  if($('controlPolicy'))$('controlPolicy').textContent=controlPolicyText(st);
  lastStateAt=Date.now();
  document.body.classList.remove('data-stale');
  if($('staleBanner'))$('staleBanner').hidden=true;
  const manual=!!st.manual_mode, safe=!!st.safe;
  // Opcjonalne czujniki sa ukrywane w WWW, gdy sa wylaczone w secrets.ini.
  // Dzieki temu brak niepodlaczonego sensora nie wyglada jak awaria.
  const ft=st.features||{};
  const setVis=(id,on)=>{const e=$(id); if(e)e.style.display=on?'':'none';};
  setVis('uvCell', !!ft.si1145);
  setVis('sqmCard', !!ft.tsl2591);
  setVis('luxMetric', !!ft.bh1750);
  setVis('tExtMetric', !!ft.ds18b20);
  const hideMetric=(id,on)=>{const e=$(id),box=e&&e.closest('.metric');if(box)box.style.display=on?'':'none';};
  const airEnabled=!!(ft.bme280||ft.dht22);
  hideMetric('tAir',airEnabled); hideMetric('hAir',airEnabled); hideMetric('dew',airEnabled);
  hideMetric('press',!!(ft.bme280||ft.bmp280));
  const bench=$('benchBanner');
  if(bench) bench.className='bench-banner'+(!st.modbus_relay_ok?' show':'');

  // Manual banner
  $('manualBanner').className='manual-banner'+(manual?' show':'');
  $('manualCountdown').textContent=manual?(st.manual_authorized?'Autoryzowany — bez limitu czasu':'Po restarcie wymagane hasło'):'—';
  $('btnManualOn').style.display=manual?'none':'';

  // Safety pill
  const pill=$('safetyPill');
  if(manual){
    pill.className='safety-pill unsafe';
    pill.style.cssText='background:rgba(239,68,68,.2);border-color:rgba(239,68,68,.7);color:#fca5a5';
    $('safetyTxt').textContent='MANUAL';
  } else {
    pill.className='safety-pill '+(safe?'safe':'unsafe');
    pill.style.cssText='';
    $('safetyTxt').textContent=safe?'SAFE':'UNSAFE';
  }

  // Chips
  const pg=!!st.park_guard_enabled, pk=st.park_motion_ready===undefined?!!st.telescope_parked:!!st.park_motion_ready;
  const cp=$('chipPark');
  if(!pg){cp.className='status-chip chip-off';cp.textContent='PARK OFF';}
  else if(pk){cp.className='status-chip chip-ok';cp.textContent='PARK: spełniony';cp.title='Spełniony warunek parku; pozostałe blokady obowiązują osobno';}
  else{cp.className='status-chip chip-bad';cp.textContent='PARK: brak zgody';cp.title='Warunek parku nie jest spełniony';}
  // Net
  const wip=st.net&&st.net.wifi, eip=st.net&&st.net.eth;
  $('netText').textContent=(eip?'ETH:'+eip:'')+(eip&&wip?' | ':'')+(wip?'WiFi:'+wip:'')||'Brak sieci';
  const clock=$('clockText');
  if(clock){const rtcState=!st.rtc_present?'RTC niewykryty':(st.rtc_time_valid?'RTC OK':(st.rtc_time_lost?'RTC wymaga synchronizacji':'RTC bez ustawionego czasu'));clock.textContent='Zegar: '+(st.clock_time||'brak czasu')+' | '+rtcState;}
  $('badgeWifi').style.display=wip?'':'none';
  $('badgeEth').style.display=eip?'':'none';

  // Sky bar
  const delta=st.sensors&&st.sensors.cloud_delta;
  const lim=st.limits||{}, cloudClear=isNum(lim.cloud_clear)?lim.cloud_clear:16,cloudScattered=isNum(lim.cloud_scattered)?lim.cloud_scattered:9;
  const sk=skyClass(delta,lim);
  $('skyIcon').textContent=sk.icon;
  $('skyStatus').textContent=st.sky_condition||sk.txt;
  $('skyStatus').style.color=sk.col;
  $('skyReason').textContent=(!safe&&st.reason)?st.reason:'';

  const dv=$('sbDelta');
  dv.textContent=isNum(delta)?fmt(delta,1)+'°C':'BRAK';
  dv.className='cv '+(isNum(delta)?(delta>=cloudClear?'ok':delta>=cloudScattered?'warn':'bad'):'');
  const w=st.sensors&&st.sensors.wind;
  const wavg=st.sensors&&st.sensors.wind_avg;
  const wgust=st.sensors&&st.sensors.wind_gust;
  const wfault=st.sensors&&st.sensors.wind_fault;
  const wv=$('sbWind');
  if(st.sensors && st.sensors.wind_valid===false){wv.textContent='NIEWIARYGODNY';wv.className='cv bad';}
  else if(wfault){wv.textContent='AWARIA';wv.className='cv bad';}
  else{
    const shown=isNum(wavg)?wavg:w;
    wv.textContent=isNum(shown)?(fmt(shown,1)+(isNum(wgust)?' / '+fmt(wgust,0):'')+' m/s'):'—';
    wv.className='cv '+wClass(shown,lim);
  }
  const rain=st.sensors&&st.sensors.rain;
  const rv=$('sbRain');rv.textContent=rain?'DESZCZ':'SUCHO';rv.className='cv '+(rain?'bad':'ok');
  const uv=st.sensors&&st.sensors.uv;
  const uvel=$('sbUv');uvel.textContent=isNum(uv)?fmt(uv,1):'—';uvel.className='cv '+uvClass(uv);

  // Weather
  const s=st.sensors||{};
  $('tAir').textContent=fmt(s.t_air,1);$('hAir').textContent=fmt(s.h_air,0);
  $('dew').textContent=fmt(s.dew,1);$('press').textContent=fmt(s.pressure,0);
  $('tSky').textContent=fmt(s.t_sky,1);$('tAmb').textContent=fmt(s.t_amb,1);
  $('lux').textContent=fmt(s.lux,0);
  $('tExt').textContent=(s.t_ext!==null&&isNum(s.t_ext))?fmt(s.t_ext,1):'BRAK';

  // karta natezenia oswietlenia (TSL2591)
  {
    const tl=s.tsl_lux;
    $('tslLux').textContent = isNum(tl) ? (tl<1 ? tl.toFixed(4) : (tl<100 ? tl.toFixed(2) : fmt(tl,0))) : '—';
    const tst=tslStatusFor(tl);
    const tstEl=$('tslStatus');
    tstEl.textContent=tst.txt; tstEl.style.color=tst.col; tstEl.style.borderColor=tst.col;
    $('sqmDot').style.left=sqmPercent(tl)+'%';
    $('tslFull').textContent = (s.tsl_full!==undefined && s.tsl_full!==null) ? s.tsl_full : '—';
    $('tslIr').textContent   = (s.tsl_ir!==undefined && s.tsl_ir!==null)   ? s.tsl_ir   : '—';
  }
  $('heaterWarn').className='heater-warn'+(st.heater_needed?' show':'');
  renderHeaters(st.heaters);

  // Roof
  const rs=(st.roof&&st.roof.state)||'UNKNOWN';
  $('roofState').textContent=rs;
  animateRoof(rs);
  updateParkDot(pk,pg);

  // ── WEATHER FX (deszcz / wiatr / zachmurzenie nad dachem) ──
  const fx=$('weatherFx');
  if(fx){
    fx.classList.toggle('raining', !!rain);
    const wspeed=isNum(wavg)?wavg:(isNum(w)?w:0);
    fx.classList.toggle('windy', wspeed>4 && !wfault);
    // szybszy wiatr -> krotszy czas animacji smug (0.6s..3.2s)
    fx.style.setProperty('--wdur', Math.max(0.6, 3.2 - wspeed*0.16).toFixed(2)+'s');
    const cl=$('fxClouds');
    if(cl){
      let frac=0;
      if(isNum(delta)) frac = delta>=16 ? 0 : (delta<=0 ? .9 : (1-(delta/16))*.9);
      if(rain) frac=Math.max(frac,.7);
      cl.style.opacity=frac.toFixed(2);
    }
    // poswiata "otwartego nieba" pulsuje tylko gdy dach otwarty i jest pogodnie
    const glow=$('roofGlow');
    if(glow) glow.classList.toggle('pulse', rs==='OPEN' && safe);
  }

  const lc=st.roof&&st.roof.limit_closed, lo=st.roof&&st.roof.limit_open;
  $('limC').className='limit-box'+(lc?' active':'');$('limCv').textContent=lc?'ACTIVE':'—';
  $('limO').className='limit-box'+(lo?' active':'');$('limOv').textContent=lo?'ACTIVE':'—';

  // A park condition is not a guarantee that all movement interlocks pass.
  const park=parkPresentation(st), pb=$('parkBanner');
  pb.className=park.enabled?'park-banner show'+(park.ready?'':' danger'):'park-banner';
  pb.style.background='';pb.style.borderColor='';pb.style.color='';
  $('parkBannerText').textContent=park.hardware+' · '+park.condition;
  const overview=$('parkOverview');
  if(overview){overview.replaceChildren();for(const text of [park.hardware,park.condition,park.remote,park.nina]){const row=document.createElement('div');row.textContent=text;overview.appendChild(row);}}

  // Buttons — ruch dachu/AUX steruje zewnetrznym Waveshare 17658 przez Modbus RTU.
  const can=!!st.modbus_relay_ok;
  $('tcaWarn').style.display=can?'none':'';
  const ob=!!st.hard_weather_block||st.open_weather_measurements_ready===false||(!manual&&!safe);
  $('btnOpen').disabled=!st.roof_motion_enabled||!can||rs==='OPEN'||rs==='OPENING'||ob;
  $('btnOpen').style.opacity=ob?'0.35':'';
  $('btnClose').disabled=!st.roof_motion_enabled||!can||rs==='CLOSED'||rs==='CLOSING';
  // STOP ZAWSZE aktywny (o ile Modbus relay jest online). Wczesniej dzialal tylko
  // gdy ESP "myslal", ze dach jedzie - a jesli stan sie rozjedzie z rzeczywistoscia
  // (np. sterownik Elmes jedzie, a ESP mysli ze stoi), nie dalo sie go zatrzymac!
  // STOP to hamulec bezpieczenstwa - musi byc dostepny ZAWSZE.
  $('btnStop').disabled=!st.roof_motion_enabled;

  // Stable AUX IDs: preserve AUX1=CH7; the API supplies all physical channel assignments.
  if(Array.isArray(st.aux_map)){
    const grid=$('auxGrid');
    const signature=st.aux_map.map(a=>a.id+':'+a.ch).join(',');
    if(grid && grid.dataset.map!==signature){
      grid.dataset.map=signature;
      grid.innerHTML=st.aux_map.map(a=>'<div class="aux-btn" id="aux'+a.id+'"><div class="an">AUX '+a.id+' · CH'+a.ch+'</div><div class="av">UNKNOWN</div></div>').join('');
    }
  }
  if(Array.isArray(st.aux)){
    for(let i=1;i<=st.aux.length;i++){
      const el=$('aux'+i);if(!el)continue;
      const on=!!st.aux[i-1], known=(!!st.modbus_relay_state_valid&&st.modbus_relay_readback_current!==false);
      const mapping=st.aux_map.find(a=>a.id===i);
      const act=known&&mapping?!!st.modbus_relay_actual[mapping.ch-1]:null;
      el.className='aux-btn '+(!known?'unknown':(act?'on':'off'));
      el.querySelector('.av').textContent=known?(act?'ON':'OFF'):'UNKNOWN';
      el.style.opacity=can?'':'0.4';
      el.onclick=can?()=>aux(i):null;
    }
  }

  // Local DO rendering; names use textContent, never HTML interpolation.
  if(st.local_do){
    const card=$('localDoCard'),grid=$('localDoGrid');
    const list=st.local_do.channels.filter(c=>c.enabled);
    if(card && grid){
      card.hidden=!st.local_do.supported || !list.length;
      const signature=JSON.stringify(list.map(c=>[c.id,c.name]));
      if(grid.dataset.map!==signature){
        grid.replaceChildren();grid.dataset.map=signature;
        for(const c of list){
          const button=document.createElement('button');button.type='button';button.id='localDo'+c.id;
          button.className='aux-btn';
          button.title='DO'+c.id+' · '+c.name;
          const label=document.createElement('div');label.className='an';label.textContent='DO'+c.id+' · '+c.name;
          label.style.overflowWrap='anywhere';
          const value=document.createElement('div');value.className='av';
          button.append(label,value);grid.append(button);
        }
      }
      for(const c of list){
        const button=$('localDo'+c.id);if(!button)continue;
        const known=st.local_do.online && typeof c.on==='boolean';
        button.className='aux-btn '+(!known?'unknown':(c.on?'on':'off'));
        button.querySelector('.av').textContent=known?(c.on?'ON':'OFF'):'UNKNOWN';
        button.disabled=!can || !known;
        button.onclick=can&&known?()=>localDo(c.id,!c.on):null;
      }
    }
  }

  // Iluminator: ON tylko przy dachu CLOSED i dostepnym TCA9554.
  {
    const lb=$('btnLight');
    if(lb){
      const on=!!st.illum;
      const available=st.illum_enabled!==false && !!st.illum_available;
      lb.classList.toggle('on',on);
      lb.disabled=!on && !available;
      lb.textContent=on?'💡 ŚWIATŁO: ON':
        (!available?'🔒 ŚWIATŁO: OFFLINE':'💡 ŚWIATŁO: OFF');
    }
    const led=st.led||{};
    const red=$('btnRedLed');
    if(red){const on=!!led.on;red.style.display=led.firmware_enabled?'':'none';red.classList.toggle('on',on);red.disabled=!led.module_online;red.textContent=on?'🔴 TAŚMA: ON':'🔴 TAŚMA: OFF';}
    const lr=$('ledRow'), lg=$('ledRange');
    if(lr&&lg){
      lr.classList.toggle('show',!!led.firmware_enabled);
      if(document.activeElement!==lg){
        lg.value=Math.round(Number(led.brightness_pct)||50);
        $('ledVal').textContent=lg.value+'%';
      }
    }
  }

  // ── E-STOP / łączność / link ──
  const ex=st.safety_ext||{};
  const eb=$('estopBanner');
  const estop=!!ex.mount_estop, commsLost=!!ex.comms_lost, linkUp=(ex.link_up!==false);
  if(estop){
    eb.className='estop-banner show';
    $('estopTitle').textContent='⛔ OCHRONA MONTAŻU';
    $('estopSub').textContent=(ex.mount_estop_reason||'Krańcówka overtravel zadziałała')
      +(ex.mount_limit_raw?' — krańcówka NADAL w stanie TRIP (sprawdź montaż/kabel)':' — przywrócenie wymaga inspekcji i hasła');
    $('btnEstopReset').style.display='';
    $('btnEstopReset').disabled=!!ex.mount_limit_raw;
  } else if(commsLost){
    eb.className='estop-banner show warn';
    $('estopTitle').textContent='📡 BRAK ŁĄCZNOŚCI Z NINA';
    $('estopSub').textContent='Dach otwarty, a ESP nie dostaje zapytań Alpaca >5 min. Sprawdź PC/sieć. (Pogoda nadal pilnowana lokalnie.)';
    $('btnEstopReset').style.display='none';
  } else if(!linkUp){
    eb.className='estop-banner show warn';
    $('estopTitle').textContent='🌐 BRAK SIECI (WiFi + Ethernet)';
    $('estopSub').textContent='ESP odcięty od LAN — brak zdalnego dostępu i komunikacji z NINA. Sprawdź niezależny kanał alarmowy; ten status nie potwierdza dostarczenia alarmu.';
    $('btnEstopReset').style.display='none';
  } else {
    eb.className='estop-banner';
  }
  // chip LINK
  const clk=$('chipLink');
  if(linkUp){clk.className='status-chip chip-ok';clk.textContent=(ex.eth_link?'ETH':'')+(ex.eth_link&&ex.wifi_link?'+':'')+(ex.wifi_link?'WiFi':'')||'LINK';}
  else{clk.className='status-chip chip-bad';clk.textContent='BRAK SIECI';}
  // Mount protection always blocks OPEN; CLOSE follows the geometry policy.
  if(estop){$('btnOpen').disabled=true;if(st.mount_protection_blocks_close!==false)$('btnClose').disabled=true;}
  const me=$('btnMountEstop'); if(me){me.disabled=estop;me.style.opacity=estop?'0.4':'';}

  // Diagnostyka + progi
  renderDiag(st);
  populateCfg(st.limits);
  populateWeatherPolicy(st);
  // status aktywnego MLX
  const mlxStatus=$('mlxStatus'), mlxBtn1=$('btnMlxPrimary'), mlxBtn2=$('btnMlxSecondary');
  if(mlxStatus && st.mlx_active!==undefined){
    const n=Number(st.mlx_active), fail=!!st.mlx_failover;
    mlxStatus.textContent='Aktualnie: MLX'+n+(fail?' (AUTOMATYCZNY BACKUP)':'');
    if(mlxBtn1) mlxBtn1.style.opacity=n===1?'1':'0.5';
    if(mlxBtn2) mlxBtn2.style.opacity=n===2?'1':'0.5';
  }
}

/* ── Diagnostyka ── */
function diagCell(name,ok,val,offText,statusText){
  const cls = (ok===null)?'off':(ok?'ok':'bad');
  const status = statusText||((ok===null)?(offText||'OFF'):(ok?'OK':'AWARIA'));
  return `<div class="diag-cell ${cls}"><div class="dn">${name}</div><div class="dv">${val||'—'}</div><div class="ds">${status}</div></div>`;
}
function inputDiagCell(channel,name,value,status,kind){
  return `<div class="diag-cell input-diag-cell ${kind}"><div class="input-diag-head"><span class="input-tag">${channel}</span><span class="dn">${name}</span></div><div class="dv">${value}</div><div class="ds">${status}</div></div>`;
}
function renderInputDiag(st){
  const g=$('inputDiagGrid'); if(!g) return;
  const roof=st.roof||{}, safety=st.safety_ext||{}, d=st.diag||{}, sensors=st.sensors||{};
  const park=!!st.park_guard_enabled;
  const collision=!!d.collision_en;
  const items=[];
  const closed=!!roof.limit_closed, opened=!!roof.limit_open;
  items.push(inputDiagCell('DI1','Limit CLOSED',closed?'pozycja potwierdzona':'brak potwierdzenia',closed?'AKTYWNA':'OCZEKUJE',closed?'ok':'idle'));
  items.push(inputDiagCell('DI2','Limit OPEN',opened?'pozycja potwierdzona':'brak potwierdzenia',opened?'AKTYWNA':'OCZEKUJE',opened?'ok':'idle'));
  if(safety.mount_estop_enabled){
    const trip=!!safety.mount_limit_raw;
    items.push(inputDiagCell('DI3','Overtravel montażu',trip?'TRIP':'wejście nieaktywne',trip?'ALARM':'OK',trip?'bad':'ok'));
  } else items.push(inputDiagCell('DI3','Overtravel montażu','wyłączone w firmware','OFF','off'));
  const pressed=!!st.emergency_close_pressed;
  items.push(inputDiagCell('DI4','Przycisk awaryjny CLOSE',pressed?'wciśnięty':'spoczynek',pressed?'ZAMYKANIE':'GOTOWY',pressed?'warn':'ok'));
  if(collision){
    const blocked=!!d.collision_active;
    items.push(inputDiagCell('DI5','Czujnik kolizji',blocked?'przeszkoda / kontakt':'czysto',blocked?'TRIP':'OK',blocked?'bad':'ok'));
  } else items.push(inputDiagCell('DI5','Czujnik kolizji','wyłączony w firmware','OFF','off'));
  if(park){
    const ra=st.park_ra===true, dec=st.park_dec===true;
    items.push(inputDiagCell('DI6','Park osi RA',ra?'sygnał PARK aktywny':'brak sygnału PARK',ra?'PARK':'NIEPOTWIERDZONY',ra?'ok':'warn'));
    items.push(inputDiagCell('DI7','Park osi DEC',dec?'sygnał PARK aktywny':'brak sygnału PARK',dec?'PARK':'NIEPOTWIERDZONY',dec?'ok':'warn'));
  } else {
    items.push(inputDiagCell('DI6','Park osi RA','Park Guard wyłączony','OFF','off'));
    items.push(inputDiagCell('DI7','Park osi DEC','Park Guard wyłączony','OFF','off'));
  }
  const rain=!!sensors.rain;
  items.push(inputDiagCell('DI8','RG11 - deszcz',rain?'wykryto deszcz':'sucho',rain?'DESZCZ':'OK',rain?'bad':'ok'));
  g.innerHTML=items.join('');
}
function renderRelayStates(st){
  const g=$('relayStateGrid'), summary=$('modbusRelaySummary');
  if(!g) return;
  const names=st.relay_names||['OPEN','CLOSE','STOP','CH4','CH5','CH6','CH7','CH8'];
  const cmd=Array.isArray(st.modbus_relay_commanded)?st.modbus_relay_commanded:[];
  const act=Array.isArray(st.modbus_relay_actual)?st.modbus_relay_actual:[];
  const valid=(!!st.modbus_relay_state_valid&&st.modbus_relay_readback_current!==false);
  const online=!!st.modbus_relay_online&&st.modbus_relay_readback_current!==false;
  const commandOnline=!!st.modbus_relay_ok;
  if(summary){
    summary.className='relay-state-summary '+(online?'online':(!commandOnline&&!valid?'offline':'unknown'));
    summary.textContent=st.modbus_relay_readback_current===false&&st.modbus_relay_state_valid?'Modbus: ODCZYT NIEAKTUALNY':online?((st.relay_backend||'Relay')+': ONLINE'):
      (!commandOnline&&!valid?'Modbus: OFFLINE':
        (!commandOnline?'Modbus: COMMAND ERROR':'Modbus: READBACK ERROR'));
    if(Number.isFinite(st.modbus_relay_read_age_ms))summary.textContent+=' · odczyt '+st.modbus_relay_read_age_ms+' ms temu';
  }
  let h='';
  for(let i=0;i<8;i++){
    const commanded=!!cmd[i];
    const known=valid && (act[i]===true || act[i]===false);
    const actual=known?!!act[i]:null;
    const mismatch=known && commanded!==actual;
    h+='<div class="relay-state-row'+(mismatch?' mismatch':'')+'">';
    h+='<span class="relay-name">CH'+(i+1)+' '+names[i]+'</span>';
    h+='<span class="relay-state-val">CMD: <b>'+(commanded?'ON':'OFF')+'</b></span>';
    h+='<span class="relay-state-val">ACT: <b>'+(known?(actual?'ON':'OFF'):'UNKNOWN')+(mismatch?' · MISMATCH':'')+'</b></span>';
    h+='</div>';
  }
  g.innerHTML=h;
}
function denseCloudPresentation(st){
  if(st.dense_cloud_calibrated===true)return {value:true,label:'AKTYWNA',text:'Oddzielna ochrona pogodowa; działa również w MANUAL.'};
  if(st.dense_cloud_calibrated===false)return {value:null,label:'NIEURUCHOMIONA',text:'Dodatkowa ochrona wyłączona — progi do ustalenia na podstawie pomiarów. Kalibracja wiatru i zwykłe progi MLX są osobnymi ustawieniami. Ten komunikat nie oznacza błędu ich zapisu.'};
  return {value:null,label:'BRAK DANYCH',text:'Sterownik nie podał stanu dodatkowej ochrony.'};
}
function renderDiag(st){
  const g=$('diagGrid'); if(!g) return;
  renderRelayStates(st);
  const d=st.diag||{}, s=st.sensors||{}, net=st.net||{}, ft=st.features||{};
  const f=(v,u,dd)=>isNum(v)?(fmt(v,dd===undefined?1:dd)+(u||'')):'—';
  let h='';
  const dense=denseCloudPresentation(st);
  h+=diagCell('Dodatkowa ochrona bardzo gęstego zachmurzenia',dense.value,dense.text,dense.label);
  const activeMLX=Number(st.mlx_active)===2?2:1;
  const activeMLXValid=activeMLX===2?s.mlx2_valid===true:s.mlx1_valid===true;
  h+=diagCell('Chmury — aktywny MLX'+activeMLX,activeMLXValid&&isNum(s.cloud_delta),isNum(s.cloud_delta)?('Δ '+fmt(s.cloud_delta,1)+'°C · raw '+fmt(s.cloud_delta_raw,1)+'°C'):'brak świeżych danych');
  for(const n of [1,2]){const valid=s['mlx'+n+'_valid']===true,age=s['mlx'+n+'_age_ms'];h+=diagCell('MLX'+n+' — ważność próbki',valid,Number.isFinite(age)?('Wiek '+age+' ms'+(valid?' · ważna':' · nieważna lub po błędach')):'Brak poprawnej próbki');}
  { // status auto-kalibracji w sekcji ustawien
    const el=document.getElementById('acStatus');
    if(el){
      if(s.ac_trusted && isNum(s.ac_expected)){
        el.textContent='✓ Nauczone. Czyste niebo teraz ≈ '+fmt(s.ac_expected,1)+'°C, obecnie '+fmt(s.ac_gap,1)+'°C poniżej czystego. Nauczone przedziały temp.: '+(s.ac_bins||0)+'/13.';
      } else {
        el.textContent='⏳ Jeszcze się uczę tej temperatury (nauczone '+(s.ac_bins||0)+'/13 przedziałów). Do czasu nauki działa surowa delta — bezpiecznie. Tapnij „niebo czyste” w bezchmurną noc, żeby przyspieszyć.';
      }
    }
  }
  const wc=s.wind_cal||{};
  for(const key of ['gain','offset','ratio','zero','top','maxMS']){
    const input=$('wc_'+key);
    if(input && !input.dataset.edited && Number.isFinite(wc[key])) input.value=wc[key];
    if(input) input.oninput=()=>{input.dataset.edited='1';};
  }
  const modelInput=$('wc_model');
  if(modelInput){if(!modelInput.dataset.edited) modelInput.value=wc.model||'';modelInput.oninput=()=>{modelInput.dataset.edited='1';};}
  const windStatusEl=$('wc_status');
  if(windStatusEl) windStatusEl.textContent='Stan: '+(s.wind_status||'UNKNOWN')+' · '+
    (s.wind_good_samples||0)+'/64 poprawnych próbek · okno '+((s.wind_window_ms||0)/1000).toFixed(1)+' s / '+(s.wind_window_samples||0)+' próbek · wiek: '+(s.wind_age_ms===null?'brak':s.wind_age_ms+' ms')+
    (s.wind_stable_warning?' · UWAGA: stałe napięcie; cisza lub usterka — wymaga porównania':'');
  h+=diagCell('ADC wiatru', s.wind_adc_calibrated===true,
    'raw '+f(s.wind_adc_raw,'',1)+' · ADC '+f(s.wind_adc_volts,' V',3)+' · czujnik '+f(s.wind_sensor_volts,' V',3)+' · próbne '+(Number.isFinite(s.wind_provisional_ms)?s.wind_provisional_ms.toFixed(2):'—')+' m/s (niewiarygodne bez potwierdzenia)');
  if(d.anem_en===false){
    h+=diagCell('Anemometr', null, (isNum(d.wind_volts)?fmt(d.wind_volts,2)+'V · ':'')+'wył.', 'OFF');
  } else {
    h+=diagCell('Anemometr', s.wind_valid===true && !d.wind_fault, (s.wind_status||'UNKNOWN')+' · '+(s.wind_valid===true?f(s.wind_avg,'',1)+'/'+f(s.wind_gust,' m/s',0):'brak wiarygodnej prędkości')+(s.wind_holding===true?' · odrzucono 1 zakłócenie; poprzedni pomiar maks. 2,5 s':''));
  }
  h+=diagCell('RG11 (deszcz)', st.rg11_enabled?!s.rain:null,st.rg11_enabled?(s.rain?'SYGNAŁ DESZCZU':'brak sygnału deszczu'):'wyłączony','OFF');
  if(ft.dht22) h+=diagCell('DHT22', !!d.dht_ok&&isNum(s.t_air)&&isNum(s.h_air), f(s.t_air,'°C')+' · '+f(s.h_air,'%',0));
  if(ft.ds18b20) h+=diagCell('DS18B20', !!d.ds_ok&&isNum(s.t_ext), f(s.t_ext,'°C'));
  if(ft.bme280 || ft.bmp280) h+=diagCell('Barometr', !!d.baro_ok&&isNum(s.pressure), (d.baro||'?')+' · '+f(s.pressure,'hPa',0));
  if(ft.bh1750) h+=diagCell('BH1750', !!d.bh_ok&&isNum(s.lux), f(s.lux,'lx',0));
  if(ft.tsl2591) h+=diagCell('TSL2591 (natężenie, lx)', !!d.tsl_ok&&isNum(s.tsl_lux), (isNum(s.tsl_lux)?(s.tsl_lux<1?s.tsl_lux.toFixed(4):fmt(s.tsl_lux,1))+' lx':'brak')+(d.tsl_ok?(' · gain '+['1x','25x','428x','9876x'][d.tsl_gain||0]):''));
  if(ft.si1145) h+=diagCell('SI1145 (UV)', !!d.uv_ok&&isNum(s.uv), f(s.uv,'',1));
  h+=diagCell('Waveshare 17658 (Modbus)',st.modbus_relay_ok?(st.modbus_relay_readback_current===false?null:true):false,st.modbus_relay_ok?('addr '+(d.modbus_addr||1)+(st.modbus_relay_readback_current===false?' · brak bieżącego FC01':'')):'błąd komunikacji','ODCZYT NIEAKTUALNY');
  const netOk=null;
  h+=diagCell('Adresy interfejsów',netOk,(net.eth?('ETH '+net.eth+' '):'')+(net.wifi?('WiFi '+net.wifi):''),'ADRESY');
  // link / łączność z NINA / Ochrona montażu
  if(st.roof&&st.roof.stop_attempted){
    h+=diagCell('Ostatni STOP', st.roof.stop_command_confirmed?(st.roof.stop_release_confirmed?true:null):false,
      st.roof.stop_command_confirmed?(st.roof.stop_release_confirmed?'Komenda potwierdzona; postój nie jest mierzony':'Komenda potwierdzona; zwolnienie przekaźnika oczekuje/niepotwierdzone'):'STOP NIEPOTWIERDZONY — stan napędu nieznany','ZWOLNIENIE NIEPOTWIERDZONE');
  }
  const ex2=st.safety_ext||{};
  h+=diagCell('Link sieci', typeof ex2.link_up==='boolean'?ex2.link_up:null, (ex2.eth_link?'ETH ':'')+(ex2.wifi_link?'WiFi':'')||'brak');
  const park=parkPresentation(st);
  h+=diagCell('NINA / SafetyMonitor',park.ninaActive?true:null,park.nina,'NIEAKTYWNY');
  h+=diagCell('Zapytania Alpaca do ESP',null,ex2.alpaca_seen?('Ostatnie zapytanie '+ex2.alpaca_age_s+' s temu; to nie odczyt parku montażu'):'Brak zapytań od uruchomienia','HISTORIA');
  if(ex2.mount_estop_enabled){
    h+=diagCell('Ochrona montażu', ex2.mount_estop?false:true,
        ex2.mount_estop?(ex2.mount_limit_raw?'TRIP (aktywna)':'zatrzask — inspekcja i hasło'):'gotowy');
  } else if(ex2.mount_estop){
    h+=diagCell('Ochrona montażu', false, 'aktywny (ręczny)');
  }
  if(ex2.ota_active) h+=diagCell('OTA', null, '⬆ wgrywanie...', 'OTA');
  h+=diagCell('Fizyczny park RA/DEC',park.enabled?(park.stable?true:null):null,park.hardware,park.enabled?'BRAK STABILNEGO PARKU':'WYŁĄCZONY');
  h+=diagCell('Alpaca — park montażu',park.current?st.park_alpaca.value:null,park.remote,'BRAK POTWIERDZENIA',park.current?(st.park_alpaca.value?'PARK':'POZA PARKIEM'):undefined);
  h+=diagCell('Warunek Park Guard',park.enabled?(park.ready?true:null):null,park.condition+'; pozostałe blokady sprawdzane osobno',park.enabled?'NIESPEŁNIONY':'WYŁĄCZONY');
  // Zatrzaski/awarie ruchu - tylko gdy aktywne
  if(d.wind_latch) h+=diagCell('Zatrzask wiatru', false, 'aktywny');
  if(d.cloud_latch) h+=diagCell('Zatrzask chmur', false, 'aktywny');
  if(d.motion_fault) h+=diagCell('Ruch dachu', false, d.motion_fault);
  g.innerHTML=h;
  renderInputDiag(st);

  // Stopka: FW, uptime, IP
  const up=d.uptime_s||0; const dd=Math.floor(up/86400), hh=Math.floor((up%86400)/3600), mm=Math.floor((up%3600)/60);
  const upStr=(dd?dd+'d ':'')+hh+'h '+mm+'m';
  $('diagFoot').innerHTML=`<span>FW: ${st.fw||'?'}</span><span>Uptime: ${upStr}</span><span>${(net.http||'')}</span>`;
}


let weatherPolicyEdited=false,weatherPolicyLatest=null;
function weatherPolicyDirty(){weatherPolicyEdited=true;}
function discardWeatherPolicy(){weatherPolicyEdited=false;if(weatherPolicyLatest)populateWeatherPolicy(weatherPolicyLatest);}
function populateWeatherPolicy(st){
 weatherPolicyLatest=st;
 const p=st.weather_policy,button=$('btnWeatherPolicySave');
 if(!p||p.version!==1){button.disabled=true;$('weatherPolicyStatus').textContent='Ta wersja firmware nie udostępnia wyboru czujników.';return;}
 button.disabled=!(st.manual_mode===true&&st.roof&&st.roof.state==='CLOSED'&&st.roof.limit_closed===true&&st.roof.limit_open===false);
 const skipped=[['rain','deszcz'],['wind','wiatr'],['clouds','chmury'],['humidity','wilgotność'],['dew','ryzyko rosy']].filter(x=>p[x[0]]===false).map(x=>x[1]);
 $('weatherPolicyStatus').textContent=(p.config_fault?'BŁĄD ZAPISU — UNSAFE. ':'')+'Poza decyzją SAFE/UNSAFE: '+(skipped.join(', ')||'brak')+'. Otwarcie AUTO: '+(p.open_ready?'okres poprawy zakończony':'oczekiwanie na poprawę i stabilne pomiary')+'.';
 $('weatherPolicyBanner').textContent='Ochrona pogodowa pomija: '+(skipped.join(', ')||'żadnego czujnika')+(p.config_fault?' · BŁĄD KONFIGURACJI':'');
 if(weatherPolicyEdited)return;
 for(const key of ['rain','wind','clouds','humidity','dew'])$('wp_'+key).checked=p[key]===true;
 for(const key of ['humidity_limit','moisture_hold_sec','cloud_hold_sec','reopen_sec'])$('wp_'+key).value=p[key];
 $('wp_dew_margin').value=(p.dew_tenths/10).toFixed(1);
}
async function saveWeatherPolicy(){
 const body=new URLSearchParams();
 for(const key of ['rain','wind','clouds','humidity','dew'])body.set(key,$('wp_'+key).checked?'1':'0');
 for(const key of ['humidity_limit','moisture_hold_sec','cloud_hold_sec','reopen_sec']){
  const el=$('wp_'+key);if(!el.checkValidity()||!el.value){$('weatherPolicyMessage').textContent='Sprawdź zakresy i całkowite wartości czasu.';return;}body.set(key,el.value);
 }
 const margin=$('wp_dew_margin');if(!margin.checkValidity()||!margin.value){$('weatherPolicyMessage').textContent='Nieprawidłowy margines rosy.';return;}
 body.set('dew_tenths',String(Math.round(Number(margin.value)*10)));
 $('btnWeatherPolicySave').disabled=true;
 try{
  const response=await commandFetch('/api/weather/policy',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString(),cache:'no-store'});
  const result=await response.json();if(!response.ok||!result.ok)throw new Error(result.error||'Nie zapisano konfiguracji');
  weatherPolicyEdited=false;$('weatherPolicyMessage').textContent='Zapisano. Pomiary pozostają aktywne; obowiązuje nowy wybór czujników. Dach nie został uruchomiony.';
 }catch(error){$('weatherPolicyMessage').textContent='Nie zapisano: '+error.message;}
 await refresh(false);
}

/* ── Edytowalne progi ── */
function populateCfg(lim){
  if(!lim) return;
  // nie nadpisuj pola, ktore wlasnie edytujesz (focus)
  const set=(id,v)=>{const el=$(id); if(el && (window._cfgForce||document.activeElement!==el) && isFinite(v)) el.value=Math.round(v);};
  set('cfgCcClr',lim.cloud_clear); set('cfgCcSca',lim.cloud_scattered); set('cfgCcOvc',lim.cloud_overcast);
  set('cfgWWarn',lim.wind_warn); set('cfgWSus',lim.wind_sustained); set('cfgWGust',lim.wind_gust); set('cfgWRst',lim.wind_reset);
  const tg=$('cfgCcTrig'); if(tg && (window._cfgForce||document.activeElement!==tg)) tg.checked=!!lim.cloud_trigger;
  const an=$('cfgAnemEn'); if(an && (window._cfgForce||document.activeElement!==an)) an.checked=(lim.anemometer!==false);
  const heaterEn=Array.isArray(lim.heater_enabled)?lim.heater_enabled:[];
  const heaterW=Array.isArray(lim.heater_watts)?lim.heater_watts:[];
  for(let i=0;i<3;i++){
    const el=$('cfgHeaterEn'+(i+1));
    if(el && (window._cfgForce||document.activeElement!==el)) el.checked=!!heaterEn[i];
    set('cfgHeaterW'+(i+1),heaterW[i]);
    if(el) el.disabled=!lim.heater_firmware_enabled;
  }
  /* kalibracja MLX + tryb sieci */
  const setF=(id,v,dp)=>{const el=$(id); if(el && (window._cfgForce||document.activeElement!==el) && isFinite(v)) el.value=(+v).toFixed(dp);};
  set('cfgSkyK1',lim.sky_k1); set('cfgSkyK2',lim.sky_k2); set('cfgSkyK3',lim.sky_k3);
  set('cfgSkyK4',lim.sky_k4); set('cfgSkyK5',lim.sky_k5);
  setF('cfgMlxSO',lim.mlx_sky_off,1); setF('cfgMlxAO',lim.mlx_amb_off,1);
  const se=$('cfgSkyEn'); if(se && (window._cfgForce||document.activeElement!==se)) se.checked=!!lim.sky_corr;
  const nm=$('cfgNetMode'); if(nm && (window._cfgForce||document.activeElement!==nm) && lim.net_mode!==undefined) nm.value=String(lim.net_mode);
  const ac=$('cfgAutoCloud'); if(ac && (window._cfgForce||document.activeElement!==ac)) ac.checked=!!lim.auto_cloud;
}
function cfgMsg(txt,ok){const m=$('cfgMsg'); m.textContent=txt; m.className='cfg-msg '+(ok?'ok':'bad');}
async function cloudSeed(){
  try{ const r=await commandFetch('/cmd?do=cloudseed&ajax=1',{cache:'no-store'}); const j=await r.json();
    if(j.ok){toast('☀️ Nauczono: niebo czyste');} else {toast('✗ '+(j.error||'nie udało się'));}
    await refresh(false);
  }catch(e){toast('✗ '+e.message);}
}
async function cloudForget(){
  if(!confirm('Skasować całą naukę auto-kalibracji chmur?')) return;
  try{ const r=await commandFetch('/cmd?do=cloudforget&ajax=1',{cache:'no-store'}); const j=await r.json();
    if(j.ok){toast('🗑 Nauka skasowana');} await refresh(false);
  }catch(e){toast('✗ '+e.message);}
}
async function mlxSwitchover(useSecondary){
  try{
    const use = useSecondary ? 1 : 0;
    const r=await commandFetch('/cmd?do=mlx_switchover&use='+use+'&ajax=1',{cache:'no-store'});
    if(!r.ok)throw new Error('HTTP '+r.status);
    const j=await r.json();
    if(j.ok){
      const msg = useSecondary ? '📡 MLX2 (backup)' : '📡 MLX1';
      $('mlxStatus').textContent = 'Aktualnie: ' + msg;
      toast('✓ Przełączono na ' + msg);
      $('btnMlxPrimary').style.opacity = useSecondary ? '0.5' : '1';
      $('btnMlxSecondary').style.opacity = useSecondary ? '1' : '0.5';
    }else{
      toast('✗ '+(j.error||'błąd przełączenia'));
    }
    await refresh(false);
  }catch(e){toast('✗ Błąd: '+e.message);}
}
async function saveWindCalibration(){
  const body=new URLSearchParams();
  for(const key of ['gain','offset','ratio','zero','top','maxMS','model']) body.set(key,$('wc_'+key).value);
  body.set('verified',$('wc_verified').checked?'1':'0');
  try{
    const response=await commandFetch('/api/wind/calibration',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString(),cache:'no-store'});
    const result=await response.json();
    if(!response.ok || !result.ok) throw new Error(result.error||'Błąd zapisu');
    toast('Kalibracja zapisana. Pomiar wymaga ponownego zebrania próbek.',3500);
    await refresh(false);
  }catch(error){toast(error.message,5000);}
}
async function saveCfg(){
  const v=id=>$(id).value;
  const q='cloud_clear='+v('cfgCcClr')+'&cloud_scattered='+v('cfgCcSca')+'&cloud_overcast='+v('cfgCcOvc')
         +'&wind_warn='+v('cfgWWarn')+'&wind_sustained='+v('cfgWSus')+'&wind_gust='+v('cfgWGust')+'&wind_reset='+v('cfgWRst')
         +'&cloud_trigger='+($('cfgCcTrig').checked?1:0)
         +'&heater1='+($('cfgHeaterEn1').checked?1:0)+'&heater2='+($('cfgHeaterEn2').checked?1:0)+'&heater3='+($('cfgHeaterEn3').checked?1:0)
          +'&heater_w1='+v('cfgHeaterW1')+'&heater_w2='+v('cfgHeaterW2')+'&heater_w3='+v('cfgHeaterW3')
         +'&anemometer='+($('cfgAnemEn').checked?1:0)
         +'&sky_corr='+($('cfgSkyEn').checked?1:0)
         +'&sky_k1='+v('cfgSkyK1')+'&sky_k2='+v('cfgSkyK2')+'&sky_k3='+v('cfgSkyK3')
         +'&sky_k4='+v('cfgSkyK4')+'&sky_k5='+v('cfgSkyK5')
         +'&mlx_sky_off='+v('cfgMlxSO')+'&mlx_amb_off='+v('cfgMlxAO')
         +'&net_mode='+$('cfgNetMode').value
         +'&auto_cloud='+($('cfgAutoCloud').checked?1:0);
  try{
    const r=await commandFetch('/cmd?do=setcfg&ajax=1&'+q,{cache:'no-store'});
    const j=await r.json();
    if(j.ok){cfgMsg('✓ Zapisano i utrwalono w pamięci',true);toast('✓ Progi zapisane');}
    else{cfgMsg('✗ '+(j.error||'odrzucono'),false);}
    await refresh(false);
  }catch(e){cfgMsg('✗ Błąd: '+e.message,false);}
}
async function resetCfg(){
  try{
    const r=await commandFetch('/cmd?do=cfgdefault&ajax=1',{cache:'no-store'});
    const j=await r.json();
    cfgMsg(j.ok?'✓ Przywrócono domyślne':'✗ Błąd',!!j.ok);
    // wymus repopulacje pol po przywroceniu
    window._cfgForce=true; await refresh(false); window._cfgForce=false;
    toast('↺ Domyślne progi');
  }catch(e){cfgMsg('✗ Błąd: '+e.message,false);}
}

/* ── API ── */
async function refresh(show){
  if(stateRefreshBusy)return;
  stateRefreshBusy=true;
  const controller=new AbortController();
  const timeout=setTimeout(()=>controller.abort(),2500);
  try{
    const r=await commandFetch('/api/state',{cache:'no-store',signal:controller.signal});
    if(!r.ok)throw new Error('HTTP '+r.status);
    applyState(await r.json());
    setDataStatus(true,'Ostatnia odpowiedź: '+new Date().toLocaleTimeString());
    if(show)toast('Odświeżono ✓');
  }catch(e){
    markStateStale(e.name==='AbortError'?'Przekroczono limit 2,5 s — urządzenie nie odpowiada':'Brak odpowiedzi: '+e.message);
    if(show)toast('Offline: '+e.message);
  }finally{
    clearTimeout(timeout);
    stateRefreshBusy=false;
  }
}
let faultAckBusy=false;
function faultRecoveryView(st){
  const roof=st&&st.roof||{};
  const visible=roof.state==='ERROR'&&['COMMUNICATION','PARK_LOST'].includes(roof.fault_class);
  const reasons=[];
  if(!st||!st.manual_mode||!st.manual_authorized)reasons.push('Włącz i autoryzuj MANUAL');
  if(roof.fault_class==='PARK_LOST'&&!(st.park_ra===true&&st.park_dec===true&&st.park_hw_stable===true))reasons.push('PARK RA/DEC musi wrócić i pozostać stabilny');
  if(roof.limit_closed&&roof.limit_open)reasons.push('sprzeczne krańcówki — wymagana inspekcja');
  if(!roof.stop_command_confirmed||!roof.stop_release_confirmed)reasons.push('wymagane potwierdzenie STOP i zwolnienia przekaźnika');
  return {visible,enabled:visible&&reasons.length===0,reason:reasons.length?reasons.join(' · '):'Po inspekcji nieruchomego dachu można potwierdzić usunięcie przyczyny błędu. Sterownik sprawdzi STOP, wyjścia i blokady. Następnie ochrona pogodowa może zażądać CLOSE.'};
}
function renderFaultRecovery(st){
  const view=faultRecoveryView(st),panel=$('faultRecovery'),button=$('btnFaultAck'),reason=$('faultRecoveryReason');
  if(panel)panel.hidden=!view.visible;
  if(button){button.disabled=!view.enabled||faultAckBusy;button.textContent=faultAckBusy?'Sprawdzanie…':'Potwierdź usunięcie błędu po inspekcji';}
  if(reason)reason.textContent=view.reason;
}
async function ackFault(){
  if(faultAckBusy||!faultRecoveryView(lastRoofState).enabled)return;
  if(!confirm('Po inspekcji: napęd fizycznie nieruchomy, dach i tor ruchu sprawdzone, przyczyna błędu usunięta i PARK sprawdzony? Po odblokowaniu ochrona pogodowa może zażądać CLOSE.'))return;
  const password=prompt('Hasło panelu WWW — potwierdzenie inspekcji:');if(!password)return;
  faultAckBusy=true;renderFaultRecovery(lastRoofState);
  const result=$('faultAckResult');
  const report=text=>{if(result){result.hidden=false;result.textContent=text;}toast(text,7000);};
  try{
    const r=await commandFetch('/cmd',{body:new URLSearchParams({do:'fault_ack',confirm:'INSPECTED_STATIONARY',manual_pass:password,ajax:'1'}).toString()});
    const j=await r.json();
    if(!r.ok||j.ok!==true)report('Potwierdzenie odrzucone: '+(j.error||'sterownik nie potwierdził operacji (HTTP '+r.status+')'));
    else report('Błąd odblokowany po inspekcji. Sprawdź stan: ochrona pogodowa pozostaje aktywna.');
    await refresh(false);
  }catch(e){report('Brak potwierdzenia ze sterownika: '+e.message);}
  finally{faultAckBusy=false;renderFaultRecovery(lastRoofState);}
}
async function cmd(a){
  const body=new URLSearchParams({do:a,ajax:'1'});
  if(lastRoofState&&lastRoofState.manual_mode&&(a==='open'||a==='close')){
    if(!lastRoofState.manual_authorized){await setManual(true);return;}
    const weather=lastRoofState.safe?'Warunki SAFE.':('UNSAFE: '+(lastRoofState.reason||'brak wiarygodnych danych')+'.');
    const nina=lastRoofState.safety_ext&&lastRoofState.safety_ext.nina_active?'NINA aktywna.':'NINA nieaktywna.';
    if(!confirm('MANUAL '+a.toUpperCase()+': '+weather+' '+nina+' Deszcz i niewiarygodna pogoda blokują OPEN. Czy zlecić ruch? Geometria, krańcówki i aktywne zabezpieczenia nadal obowiązują.'))return;
    const password=prompt('Hasło panelu WWW — '+a.toUpperCase()+':');if(!password)return;
    body.set('confirm','MANUAL_MOVE');body.set('manual_pass',password);
  }
  try{
    const r=await commandFetch('/cmd',{body:body.toString(),cache:'no-store'});
    if(!r.ok){
      // firmware zwraca powod odmowy w JSON ("error") - pokaz go zamiast samego "HTTP 409"
      let msg='HTTP '+r.status;
      try{const j=await r.json();if(j&&j.error)msg=j.error;}catch(_){}
      await refresh(false);
      toast('🔒 '+a.toUpperCase()+' odrzucone: '+msg,7000);
      return;
    }
    await refresh(false);toast('✓ '+a.toUpperCase());
  }catch(e){toast('Błąd: '+e.message);}
}
function controlPolicyText(st){
  if(st.hard_weather_block)return 'Otwarcie zablokowane przez ochronę pogodową również w MANUAL. '+(st.reason||'Sprawdź deszcz i wiatr.');
  if(st.open_weather_measurements_ready===false)return 'OPEN zablokowane: brak świeżych, wiarygodnych danych pogodowych (wiatr lub oba MLX). MANUAL nie omija tej blokady.';
  if(st.manual_mode)return 'MANUAL: bez NINA i przy zwykłych chmurach. OPEN wymaga wiarygodnej pogody i PARK. Każdy ręczny ruch wymaga hasła. Deszcz i silny wiatr mogą zażądać CLOSE zgodnie z geometrią i blokadami.';
  return 'AUTO: otwarcie wymaga SAFE, aktywnej NINA i zgody zabezpieczeń ruchu. Do świadomego sterowania bez NINA wybierz MANUAL.';
}
let manualSubmitBusy=false;
function closeManualDialog(){
  $('manualPassword').value='';
  $('manualDialog').close();
}
async function setManual(on){
  if(on){
    $('manualDialogError').textContent='';$('manualPassword').value='';
    const wind=lastRoofState&&lastRoofState.sensors||{};
    $('manualWindNotice').textContent=wind.wind_valid===true?'':'Wiatr: brak potwierdzonego wiarygodnego pomiaru ('+(wind.wind_status||'brak danych')+'). OPEN będzie odrzucone do odzyskania wiarygodnego pomiaru wiatru.';
    $('manualDialogTitle').textContent=lastRoofState&&lastRoofState.manual_mode?'Odblokuj MANUAL':'Sterowanie ręczne — MANUAL';
    $('manualSubmit').textContent=lastRoofState&&lastRoofState.manual_mode?'Odblokuj MANUAL':'Włącz MANUAL';
    if(!$('manualDialog').open)$('manualDialog').showModal();
    $('manualPassword').focus();return;
  }
  if(!confirm('Wrócić do AUTO? Przy złej pogodzie sterownik może zamknąć dach, jeżeli pozwolą na to zabezpieczenia ruchu.'))return;
  try{
    const r=await commandFetch('/cmd?do=manual_off&ajax=1');
    const j=await r.json();
    if(!r.ok||j.ok!==true)throw Error(j.error||'Sterownik nie potwierdził AUTO');
    await refresh(false);toast('AUTO — przywrócono automatyczną ocenę warunków.',5000);
  }catch(e){toast('Nie potwierdzono zmiany trybu: '+e.message,7000);}
}
async function submitManual(event){
  event.preventDefault();if(manualSubmitBusy)return;
  const error=$('manualDialogError'),password=$('manualPassword').value;
  error.textContent='';
  if(!password){error.textContent='Wpisz hasło panelu WWW.';$('manualPassword').focus();return;}
  manualSubmitBusy=true;$('manualSubmit').disabled=true;$('manualCancel').disabled=true;
  try{
    const body=new URLSearchParams({do:'manual_on',ajax:'1',manual_pass:password}).toString();
    const r=await commandFetch('/cmd',{body});const j=await r.json();
    if(!r.ok||j.ok!==true){error.textContent=j.error||'Sterownik nie potwierdził MANUAL. Spróbuj ponownie.';return;}
    closeManualDialog();await refresh(false);toast('MANUAL autoryzowany bez limitu czasu. Nie zlecono ruchu.',6000);
  }catch(e){error.textContent='Brak potwierdzenia ze sterownika: '+e.message+'. Sprawdź bieżący tryb przed ponowieniem.';}
  finally{manualSubmitBusy=false;$('manualSubmit').disabled=false;$('manualCancel').disabled=false;$('manualPassword').value='';}
}
async function localDo(id,on){
  try{
    const r=await commandFetch('/cmd?do=localdo&n='+id+'&st='+(on?1:0)+'&ajax=1',{cache:'no-store'});
    const result=await r.json();
    if(!r.ok || !result.ok)throw new Error(result.error||('HTTP '+r.status));
    await refresh(false);
  }
  catch(e){toast('Błąd DO'+id+': '+e.message);}
}
async function aux(n){
  const el=$('aux'+n);const on=el&&el.classList.contains('on');
  try{
    const r=await commandFetch('/cmd?aux='+n+'&st='+(on?0:1)+'&ajax=1',{cache:'no-store'});
    const j=await r.json();
    if(!r.ok||!j.ok) throw new Error(j.error||('HTTP '+r.status));
    await refresh(false);
  }catch(e){toast('Błąd AUX'+n+': '+e.message);}
}
function hmShow(i){$('hmVal'+i).textContent=$('hmRange'+i).value+'%';}
async function heaterApply(i,manual,pct){
  try{
    const r=await commandFetch('/cmd?do=heater&ch='+i+'&mode='+(manual?'manual':'auto')+'&pct='+encodeURIComponent(pct)+'&ajax=1',{cache:'no-store'});
    const j=await r.json().catch(()=>({ok:r.ok}));
    if(!r.ok||j.ok===false)toast('🔒 '+(j.error||'Grzałka: odrzucone'),3000);
    await refresh(false);
  }catch(e){toast('Błąd grzałki: '+e.message);}
}
function heaterMode(i){const b=$('hmMode'+i);heaterApply(i,!b.classList.contains('manual'),$('hmRange'+i).value);}
function heaterSet(i){const b=$('hmMode'+i);if(b.classList.contains('manual'))heaterApply(i,true,$('hmRange'+i).value);}
function ledShow(){$('ledVal').textContent=$('ledRange').value+'%';}
async function setLedBright(){
  try{
    const r=await commandFetch('/cmd?do=ledbright&pct='+encodeURIComponent($('ledRange').value)+'&ajax=1',{cache:'no-store'});
    const j=await r.json().catch(()=>({ok:r.ok}));
    if(!r.ok||j.ok===false)toast('🔒 '+(j.error||'Jasność: odrzucone'),3000);
    await refresh(false);
  }catch(e){toast('Błąd jasności: '+e.message);}
}
async function toggleRedLed(){
  const el=$('btnRedLed');const on=el&&el.classList.contains('on');
  try{
    const r=await commandFetch('/cmd?do=led&st='+(on?0:1)+'&ajax=1',{cache:'no-store'});
    const j=await r.json().catch(()=>({ok:r.ok}));
    if(!r.ok||j.ok===false) throw new Error(j.error||('HTTP '+r.status));
    await refresh(false);
  }catch(e){toast('Błąd czerwonej taśmy: '+e.message);}
}
async function toggleLight(){
  const el=$('btnLight');const on=el&&el.classList.contains('on');
  try{
    const r=await commandFetch('/cmd?do=light&st='+(on?0:1)+'&ajax=1',{cache:'no-store'});
    const j=await r.json().catch(()=>({ok:r.ok}));
    await refresh(false);
    toast(j.ok===false?'🔒 Nie udało się przełączyć światła':(on?'💡 Światło OFF':'💡 Światło ON'));
  }catch(e){toast('Błąd światła: '+e.message);}
}
/* Ochrona montażu */
async function mountEstop(){
  if(!confirm('Wyzwolić ochronę montażu i zażądać odcięcia jego zasilania? OPEN będzie zablokowane. CLOSE zależy od potwierdzonego prześwitu i aktywnych zabezpieczeń. To nie jest awaryjny STOP dachu.'))return;
  try{
    const r=await commandFetch('/cmd?do=estop&ajax=1',{cache:'no-store'});
    const j=await r.json();await refresh(false);
    toast(r.ok&&j.ok?'⛔ OCHRONA MONTAŻU — polecenie potwierdzone':('⛔ '+(j.error||'Brak potwierdzenia ochrony montażu')),7000);
  }catch(e){toast('Błąd: '+e.message);}
}
async function estopReset(){
  if(!lastRoofState||!lastRoofState.manual_mode||!lastRoofState.manual_authorized){await setManual(true);return;}
  if(!confirm('Po inspekcji montażu: wejście ochrony nieaktywne, dach nieruchomy, można przywrócić zasilanie montażu? Aktywnego wejścia TRIP nie można obejść.'))return;
  const password=prompt('Hasło panelu WWW — przywrócenie zasilania montażu:');if(!password)return;
  try{
    const r=await commandFetch('/cmd',{body:new URLSearchParams({do:'estop_reset',confirm:'INSPECTED_MOUNT',manual_pass:password,ajax:'1'}).toString(),cache:'no-store'});
    const j=await r.json();
    await refresh(false);
    toast(j.ok?'↺ Przywrócenie montażu potwierdzone':('✗ '+(j.error||'reset odrzucony')),3000);
  }catch(e){toast('Błąd: '+e.message);}
}

async function restartController(){
  if(!confirm('Uruchomić ponownie sterownik? Restart jest dostępny tylko wtedy, gdy dach stoi. Panel na chwilę zniknie podczas ponownego uruchamiania.'))return;
  const b=$('btnRestart'); if(b)b.disabled=true;
  try{
    const r=await commandFetch('/cmd?do=restart&ajax=1',{cache:'no-store'});
    const j=await r.json().catch(()=>({ok:r.ok,error:''}));
    if(!r.ok||!j.ok){if(b)b.disabled=false;toast('Restart odrzucony: '+(j.error||'sprawdź, czy dach stoi'),3500);return;}
    toast('Sterownik uruchamia się ponownie. Odświeżam panel za chwilę.',3500);
  }catch(e){if(b)b.disabled=false;toast('Brak odpowiedzi sterownika: '+e.message,3500);}
}
window.addEventListener('load',()=>{
  initPanels();
  setInterval(checkStateAge,1000);
  refresh(false);
  setInterval(()=>refresh(false),3000);
});

/* Lekkie efekty interfejsu: uruchamiane po zdarzeniu, bez stalego renderowania tla. */
(function(){
  'use strict';
  var reduceMotion = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  if(reduceMotion) return;

  /* ── Gwiazdozbior tla: canvas z wolno migoczacymi gwiazdami ── */
  function initStarfield(){
    var cv=document.getElementById('starfield'); if(!cv) return;
    var ctx=cv.getContext('2d');
    var stars=[], W=0, H=0, dpr=Math.min(window.devicePixelRatio||1,2);
    function resize(){
      W=window.innerWidth; H=window.innerHeight;
      cv.width=W*dpr; cv.height=H*dpr; cv.style.width=W+'px'; cv.style.height=H+'px';
      ctx.setTransform(dpr,0,0,dpr,0,0);
      var n=Math.min(140,Math.round((W*H)/9000));
      stars=[];
      for(var i=0;i<n;i++){
        stars.push({x:Math.random()*W,y:Math.random()*H,r:Math.random()*1.3+.25,
          ph:Math.random()*Math.PI*2,sp:.004+Math.random()*.012});
      }
    }
    var t=0;
    function draw(){
      t+=1;
      ctx.clearRect(0,0,W,H);
      for(var i=0;i<stars.length;i++){
        var s=stars[i];
        var a=.35+.65*(0.5+0.5*Math.sin(t*s.sp+s.ph));
        ctx.beginPath();
        ctx.fillStyle='rgba(216,234,255,'+a.toFixed(3)+')';
        ctx.arc(s.x,s.y,s.r,0,6.283);
        ctx.fill();
      }
      requestAnimationFrame(draw);
    }
    window.addEventListener('resize',resize,{passive:true});
    resize(); draw();
  }

  /* ── Przelatujaca gwiazda: co jakis czas, losowa trajektoria ── */
  function spawnShootingStar(){
    var el=document.createElement('div');
    el.className='shooting-star';
    var startX=Math.random()*window.innerWidth*0.7+window.innerWidth*0.2;
    var startY=Math.random()*window.innerHeight*0.25;
    var len=90+Math.random()*90, dur=(0.9+Math.random()*0.9).toFixed(2)+'s';
    var dx=-(220+Math.random()*220), dy=140+Math.random()*160;
    el.style.left=startX+'px'; el.style.top=startY+'px';
    el.style.setProperty('--slen',len+'px');
    el.style.setProperty('--sdur',dur);
    el.style.setProperty('--sx',dx+'px');
    el.style.setProperty('--sy',dy+'px');
    el.style.transform='rotate('+(Math.atan2(dy,dx)*180/Math.PI)+'deg)';
    document.body.appendChild(el);
    setTimeout(function(){ el.remove(); }, 2200);
    scheduleShootingStar();
  }
  function scheduleShootingStar(){
    var delay=7000+Math.random()*13000; // co 7-20s
    setTimeout(spawnShootingStar, delay);
  }

  /* ── Karty: spotlight podazajacy za kursorem ── */
  function initCardSpotlight(){
    document.querySelectorAll('.card').forEach(function(card){
      card.addEventListener('pointermove',function(e){
        var r=card.getBoundingClientRect();
        card.style.setProperty('--mx',((e.clientX-r.left)/r.width*100).toFixed(1)+'%');
        card.style.setProperty('--my',((e.clientY-r.top)/r.height*100).toFixed(1)+'%');
      },{passive:true});
    });
  }

  /* ── Flash na wartosciach: MutationObserver zamiast grzebania w refresh() ── */
  function initValueFlash(){
    var watch=document.querySelectorAll('.metric .v,.cv,#skyStatus,#roofState,.lv,.av,#roofLabel');
    var prev=new WeakMap();
    watch.forEach(function(el){ prev.set(el, el.textContent); });
    var mo=new MutationObserver(function(muts){
      var touched=new Set();
      muts.forEach(function(m){
        var el=m.target.nodeType===1?m.target:m.target.parentElement;
        while(el && !touched.has(el)){
          if(prev.has(el)){
            if(prev.get(el)!==el.textContent){
              prev.set(el, el.textContent);
              el.classList.remove('val-flash');
              void el.offsetWidth; // restart animacji
              el.classList.add('val-flash');
            }
            touched.add(el);
            break;
          }
          el=el.parentElement;
        }
      });
    });
    watch.forEach(function(el){
      mo.observe(el,{characterData:true,childList:true,subtree:true});
    });
  }

  /* ── Ripple na klikniecie ── */
  function initRipple(){
    document.addEventListener('pointerdown',function(e){
      var t=e.target.closest('button,.aux-btn,.limit-box');
      if(!t || t.disabled) return;
      var r=t.getBoundingClientRect();
      var d=Math.max(r.width,r.height)*1.4;
      var sp=document.createElement('span');
      sp.className='ripple-fx';
      sp.style.width=sp.style.height=d+'px';
      sp.style.left=(e.clientX-r.left-d/2)+'px';
      sp.style.top=(e.clientY-r.top-d/2)+'px';
      t.appendChild(sp);
      setTimeout(function(){ sp.remove(); },600);
    },{passive:true});
  }

  window.addEventListener('load',function(){
    initCardSpotlight();
    initValueFlash();
    initRipple();
  });
})();

function showNotifications(){const p=$('panelNotifications');p.open=true;p.scrollIntoView({behavior:'smooth',block:'start'});refreshNotifications();}
const notifyErrors={0:'brak', '-1':'Brak połączenia TLS/sieci', '-2':'Nieprawidłowy zegar — TLS wymaga czasu UTC', '-3':'Brak świeżej, zdrowej logiki sterowania', '-4':'Niepełna lub nieprawidłowa odpowiedź HTTPS', '-5':'Brak poprawnych kluczy konfiguracji', '-6':'Brak pamięci na zadanie/kolejkę', '-7':'Kolejka pełna — komunikat pominięty', '-8':'Kanał wyłączony lub zadanie wygasło'};
function renderNotifications(data){
 const grid=$('notifyChannels');grid.replaceChildren();
 $('notifyHealth').textContent=data.controller_healthy?'Główna logika: świeża i zdrowa':'Główna logika: brak potwierdzenia zdrowia — heartbeat nie wysyła OK';
 for(const ch of data.channels){
  const row=document.createElement('div');row.style.cssText='border:1px solid var(--line);padding:12px;margin:8px 0;border-radius:8px';
  const title=document.createElement('strong');title.textContent=ch.name;row.appendChild(title);
  const label=document.createElement('label');label.style.margin='0 12px';
  const toggle=document.createElement('input');toggle.type='checkbox';toggle.checked=ch.enabled;toggle.disabled=!ch.compiled;
  toggle.onchange=()=>notificationAction(ch.id,'enable',toggle.checked);label.append(toggle,document.createTextNode(' włączony'));row.appendChild(label);
  const test=document.createElement('button');test.textContent='Wyślij test';test.disabled=!ch.compiled||!ch.enabled||!ch.configured;
  test.onclick=()=>notificationAction(ch.id,'test');row.appendChild(test);
  const state=document.createElement('div');state.textContent=!ch.compiled?'Obsługa wyłączona flagą sprzętową':!ch.enabled?'Wyłączony w WWW':!ch.configured?'Brak konfiguracji w secrets.ini':ch.inflight?'Trwa próba wysyłki':'Gotowy do prób wysyłki';row.appendChild(state);
  const detail=document.createElement('div');detail.textContent='Ostatnia akceptacja usługi: '+(ch.last_success_age_s===null?'brak':ch.last_success_age_s+' s temu')+' · ostatnia próba: '+(ch.last_attempt_age_s===null?'brak':ch.last_attempt_age_s+' s temu')+' · błąd: '+(notifyErrors[ch.last_error]||('HTTP '+ch.last_error))+' · pominięte: '+ch.dropped;row.appendChild(detail);
  grid.appendChild(row);
 }
}
async function refreshNotifications(){
 if(!$('panelNotifications').open)return;
 try{const r=await fetch('/api/notifications',{cache:'no-store'});if(!r.ok)throw Error('Brak połączenia/autoryzacji');renderNotifications(await r.json());}
 catch(e){$('notifyHealth').textContent=e.message;for(const el of $('notifyChannels').querySelectorAll('button,input'))el.disabled=true;}
}
async function notificationAction(channel,action,enabled){
 try{const body=new URLSearchParams({channel:String(channel),action});if(action==='enable')body.set('enabled',enabled?'1':'0');
  const r=await commandFetch('/api/notifications',{body:body.toString()});const data=await r.json();
  if(!r.ok||!data.ok)throw Error(data.error||'Nie przyjęto polecenia');
  toast(action==='test'?'Test zlecony — sprawdź wynik i telefon':'Ustawienie zapisane');await refreshNotifications();
 }catch(e){toast(e.message,5000);await refreshNotifications();}
}
setInterval(refreshNotifications,10000);

</script></body></html>)HTML";
static const size_t INDEX_HTML_LEN = sizeof(INDEX_HTML) - 1;
