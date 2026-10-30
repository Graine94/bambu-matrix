#pragma once
// Веб-морда: http://bambu-matrix.local
// Настройки сохраняются сразу при изменении любого поля (POST /api/settings).
// Язык: английский в разметке, русский — словарь RU по ключам data-i18n; выбор хранится в cfg.lang.

const char WEB_PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#000000">
<meta name="apple-mobile-web-app-capable" content="yes"><meta name="apple-mobile-web-app-status-bar-style" content="black-translucent"><meta name="apple-mobile-web-app-title" content="Matrix">
<title>Bambu Matrix</title>
<style>
/* В духе iOS 26: сгруппированные карточки, стеклянная панель сверху, переключатели и ползунки как в системе */
:root{color-scheme:dark;--bg:#000;--card:#1c1c1e;--text:#fff;--muted:rgba(235,235,245,.6);--line:rgba(84,84,88,.6);--fill:rgba(118,118,128,.24);--fill2:rgba(118,118,128,.36);
--accent:#f2554d;--tint:rgba(242,85,77,.2);--ok:#30d158;--bad:#ff453a;--led:#ff5a50;--off:#1f1f22;--panel:#0c0c0e;--glass:rgba(22,22,24,.7);--r:26px}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html{background:var(--bg)}
body{margin:0;background:radial-gradient(120% 60% at 50% -10%,rgba(242,85,77,.16),transparent 60%) no-repeat,var(--bg);color:var(--text);
font:17px/1.3 -apple-system,BlinkMacSystemFont,"SF Pro Text",system-ui,sans-serif;letter-spacing:-.01em;-webkit-font-smoothing:antialiased;min-height:100vh}
main{max-width:680px;margin:0 auto;padding:0 max(16px,env(safe-area-inset-right)) calc(40px + env(safe-area-inset-bottom)) max(16px,env(safe-area-inset-left))}
/* панель сверху: стекло, маленький заголовок появляется при прокрутке */
.nav{position:sticky;top:0;z-index:10;display:flex;align-items:center;justify-content:space-between;gap:12px;
padding:calc(8px + env(safe-area-inset-top)) max(16px,env(safe-area-inset-right)) 8px max(16px,env(safe-area-inset-left));
background:transparent;transition:background .25s,box-shadow .25s;-webkit-backdrop-filter:saturate(180%) blur(20px);backdrop-filter:saturate(180%) blur(20px)}
body.scrolled .nav{background:var(--glass);box-shadow:0 .5px 0 var(--line)}
.nav b{font-size:17px;font-weight:600;opacity:0;transition:opacity .25s}
body.scrolled .nav b{opacity:1}
h1{font-size:34px;line-height:41px;font-weight:700;letter-spacing:.01em;margin:2px 4px 18px}
h2{font-size:20px;font-weight:600;letter-spacing:-.02em;margin:30px 4px 10px}
/* сегменты EN/RU */
.lang{display:flex;padding:2px;border-radius:9px;background:var(--fill)}
.lang button{border:0;border-radius:7px;padding:4px 14px;font-size:13px;font-weight:600;color:var(--text);background:transparent;transition:background .2s,box-shadow .2s}
.lang button.on,.seg button.on{background:#636366;box-shadow:0 3px 8px rgba(0,0,0,.12),0 3px 1px rgba(0,0,0,.04)}
/* карточки и строки */
.card{background:var(--card);border-radius:var(--r);overflow:hidden}
.foot{font-size:13px;line-height:1.35;color:var(--muted);margin:8px 16px 0}
.opt,.row{position:relative;display:flex;align-items:center;gap:6px 12px;min-height:52px;padding:11px 16px;flex-wrap:wrap}
/* тонкий разделитель над строкой, с отступом слева, как в iOS */
.card .opt::before,.card .row::before,.card .status::after{content:'';position:absolute;top:0;left:16px;right:0;border-top:.5px solid var(--line)}
.card>.opt:first-child::before,.card>.status:first-child::after,#printerBox>.status::after{display:none}
.status{position:relative}
.opt>.t{flex:1 1 120px;min-width:0}
.opt>.t>b{display:block;font-weight:400}
.opt>.t>small,.lead{display:block;color:var(--muted);font-size:13px;line-height:1.35;margin-top:2px}
.opt>.c{display:flex;align-items:center;gap:10px;margin-left:auto;flex-shrink:0;max-width:100%}
.opt.rng{flex-direction:column;align-items:stretch;gap:10px}
.opt.rng>.c{margin-left:0}
.opt.rng>.t{flex:none;display:flex;justify-content:space-between}
.val{font-variant-numeric:tabular-nums;color:var(--muted)}
/* подпись единиц одной ширины — поля «°C» и «мин» стоят ровно друг под другом */
.unit{color:var(--muted);font-size:15px;min-width:2.2em}
.hidden{display:none!important}
/* поля ввода */
input,select{font:inherit;color:var(--text);background:var(--fill);border:0;border-radius:10px;padding:7px 12px;max-width:100%;outline:none}
input:focus{box-shadow:0 0 0 3px var(--tint)}
input[type=number]{width:64px;text-align:center;-moz-appearance:textfield}
input::-webkit-inner-spin-button{-webkit-appearance:none}
input[type=time]{min-width:92px}
input[type=time]::-webkit-calendar-picker-indicator{display:none}
select{field-sizing:content;-webkit-appearance:none;appearance:none;background:transparent;color:var(--muted);text-align:right;text-align-last:right;padding:6px 22px 6px 4px;cursor:pointer;
background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='9' height='14' fill='none' stroke='%23999' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='M1.5 5 4.5 2l3 3M1.5 9l3 3 3-3'/%3E%3C/svg%3E");background-repeat:no-repeat;background-position:right 4px center}
select option{color:#fff;background:#1c1c1e}
/* ползунок: заливка до бегунка через --p */
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:28px;padding:0;background:transparent;cursor:pointer}
input[type=range]::-webkit-slider-runnable-track{height:4px;border-radius:2px;background:linear-gradient(90deg,var(--accent) var(--p,50%),var(--fill2) var(--p,50%))}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:28px;height:28px;margin-top:-12px;border-radius:50%;background:#fff;box-shadow:0 .5px 4px rgba(0,0,0,.12),0 6px 13px rgba(0,0,0,.12)}
input[type=range]::-moz-range-track{height:4px;border-radius:2px;background:var(--fill2)}
input[type=range]::-moz-range-progress{height:4px;border-radius:2px;background:var(--accent)}
input[type=range]::-moz-range-thumb{width:28px;height:28px;border:0;border-radius:50%;background:#fff;box-shadow:0 .5px 4px rgba(0,0,0,.12),0 6px 13px rgba(0,0,0,.12)}
input[type=range]:focus{box-shadow:none}
/* переключатель iOS */
input.switch{-webkit-appearance:none;appearance:none;width:51px;height:31px;border-radius:16px;background:var(--fill2);position:relative;cursor:pointer;padding:0;margin:0;flex-shrink:0;transition:background .25s}
input.switch::after{content:'';position:absolute;top:2px;left:2px;width:27px;height:27px;border-radius:50%;background:#fff;box-shadow:0 3px 8px rgba(0,0,0,.15),0 3px 1px rgba(0,0,0,.06);transition:transform .25s cubic-bezier(.3,1.4,.6,1)}
input.switch:checked{background:var(--accent)}
input.switch:checked::after{transform:translateX(20px)}
input.switch:focus{box-shadow:none}
/* сегменты во всю ширину строки */
.seg{flex:1 0 100%;display:flex;padding:2px;border-radius:10px;background:var(--fill)}
.seg button{flex:1;border:0;border-radius:8px;padding:7px 4px;font-size:13px;font-weight:500;color:var(--text);background:transparent;transition:background .2s}
.seg button:active{transform:none}
/* кнопки-капсулы */
button{font:inherit;font-size:15px;font-weight:600;color:var(--accent);background:var(--tint);border:0;border-radius:999px;padding:7px 16px;cursor:pointer;transition:transform .15s,opacity .15s}
button:active{transform:scale(.96);opacity:.8}
button.primary{background:var(--accent);color:#fff}
button.demo{padding:5px 12px;font-size:14px}
.wide{width:100%;padding:12px 16px}
/* живая матрица */
.hero{background:var(--panel);border-radius:var(--r);padding:16px 14px 14px;box-shadow:0 20px 40px -20px rgba(242,85,77,.45)}
.panel{background:var(--panel);border-radius:14px;padding:8px}
canvas{width:100%;height:auto;display:block}
/* подробности под спойлером: строка-сводка, по нажатию — плитки */
#statBox{margin-top:12px}
#statBox>summary{display:flex;align-items:center;gap:10px;min-height:52px;padding:0 16px}
#statBox>summary>span:first-child{flex:none}
#statSum{flex:1;min-width:0;text-align:right;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-size:15px}
#statBox>summary::after{content:'›';color:var(--muted);font-size:22px;opacity:.6;transition:transform .25s}
#statBox[open]>summary::after{transform:rotate(90deg)}
#statBox[open] #statSum{visibility:hidden}
#statBox[open] .stat{animation:drop .3s ease}
@keyframes drop{from{opacity:0;transform:translateY(-6px)}}
.stat{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;padding:0 12px 12px}
.stat div{background:var(--fill);border-radius:20px;padding:11px 14px;font-size:13px;color:var(--muted)}
.stat b{display:block;font-size:19px;line-height:1.25;color:var(--text);font-weight:600;font-variant-numeric:tabular-nums;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
/* виды прогресса */
.styles{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
.style{position:relative;background:var(--card);border-radius:22px;padding:8px 8px 10px;cursor:pointer;box-shadow:inset 0 0 0 2px transparent;transition:box-shadow .2s,transform .15s}
.style:active{transform:scale(.98)}
.style.sel{box-shadow:inset 0 0 0 2.5px var(--accent)}
.style.sel::after{content:'✓';position:absolute;top:-6px;right:-6px;width:24px;height:24px;border-radius:50%;background:var(--accent);color:#fff;font-size:14px;font-weight:700;display:grid;place-items:center;box-shadow:0 2px 6px rgba(0,0,0,.2)}
.style .panel{border-radius:14px;padding:6px}
.style div:last-child{font-size:13px;line-height:1.3;color:var(--muted);margin:8px 4px 0}
/* статусы */
.status{padding:13px 16px;color:var(--muted);font-size:15px;display:flex;align-items:center;gap:10px}
.status::before{content:'';width:8px;height:8px;border-radius:50%;background:var(--muted);flex-shrink:0}
.status.ok{color:var(--text)}.status.ok::before{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.status.bad{color:var(--bad)}.status.bad::before{background:var(--bad)}
.hint{color:var(--muted);font-size:13px}
.notes{white-space:pre-wrap;font:13px/1.45 ui-monospace,Menlo,monospace;color:var(--muted);background:var(--fill);border-radius:14px;padding:10px 12px;margin:0 16px 10px;max-height:200px;overflow:auto}
/* строки формы: подпись слева, поле справа */
.row label{color:var(--text);flex:0 0 auto}
.row>input:not(.switch){flex:1 1 160px;text-align:right;background:transparent;padding:6px 0}
.row>input:not(.switch):focus{box-shadow:none}
.found{display:flex;flex-direction:column;gap:8px;width:100%}
.found button{text-align:left;font-weight:500;color:var(--text);background:var(--fill);border-radius:14px;padding:10px 14px}
.found button.sel{background:var(--tint);color:var(--accent);box-shadow:inset 0 0 0 2px var(--accent)}
#cityRes:empty,#cityMsg:empty{display:none}
#cityBox:has(#cityMsg:empty){display:none}
#cityRes{margin-top:8px}
#cityQ{flex:1;min-width:0}
.srch>.c{flex:1;margin-left:0}
/* ссылки как строки списка */
.nav-row{position:relative;display:flex;align-items:center;justify-content:space-between;min-height:48px;padding:0 16px;color:var(--text);text-decoration:none}
.nav-row+.nav-row::before,#picCheck>summary::before{content:'';position:absolute;top:0;left:16px;right:0;border-top:.5px solid var(--line)}
.nav-row::after{content:'›';color:var(--muted);font-size:22px;line-height:1;opacity:.6}
.nav-row:active{background:var(--fill)}
/* раскрывающиеся блоки */
details>summary{list-style:none;cursor:pointer}
details>summary::-webkit-details-marker{display:none}
#printerBox>summary{display:flex;align-items:center;gap:10px;min-height:56px;padding:12px 16px}
#printerBox>summary h2{margin:0;font-size:17px;font-weight:600}
#printerBox>summary::after,#picCheck>summary::after{content:'›';margin-left:auto;color:var(--muted);font-size:22px;opacity:.6;transition:transform .25s}
#printerBox[open]>summary::after,#picCheck[open]>summary::after{transform:rotate(90deg)}
#pSummary{font-size:15px;flex:1;min-width:0;text-align:right;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#pSummary.ok{color:var(--ok)}#pSummary.bad{color:var(--bad)}
#printerBox[open] #pSummary{visibility:hidden}
#picCheck>summary{position:relative;display:flex;align-items:center;min-height:48px;padding:0 16px}
#picCheck .hint{display:block;padding:0 16px 8px;margin:0}
.variants{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;padding:4px 16px 8px}
.variants button{color:var(--text);background:var(--fill);font-weight:500}
.variants button.sel{background:var(--tint);color:var(--accent);box-shadow:inset 0 0 0 2px var(--accent)}
/* «Забыть принтер» — красная строка во всю ширину, как в iOS */
button.destructive{position:relative;display:block;width:100%;min-height:50px;border-radius:0;background:transparent;color:var(--bad);font-weight:400;font-size:17px}
button.destructive::before{content:'';position:absolute;top:0;left:16px;right:0;border-top:.5px solid var(--line)}
button.destructive:active{transform:none;background:var(--fill)}
body.setup #forget{display:none}
#picMsg{padding:0 16px 8px}#picMsg:empty{display:none}
/* первый запуск (body.setup): только подключение к принтеру; до первого ответа ESP ничего не показываем */
body.wait main{visibility:hidden}
.only-setup{display:none}
body.setup main>section:not(.printer):not(.only-setup){display:none}
body.setup .only-setup{display:block}
body.setup .printer .adv{display:none}
body.setup #printerBox>summary{display:none}
body.setup.serial-err .printer .adv{display:flex}
.setup-card{padding:18px 16px}
.setup-card h2{margin:0 0 6px}
.lead{font-size:15px;margin:0}
.done{margin:0 16px 12px;padding:14px;border-radius:18px;background:color-mix(in srgb,var(--ok) 14%,transparent);font-size:15px}
.done button{margin-top:12px}
.skip{text-align:center;color:var(--muted);font-size:15px;margin-top:20px}
.skip a{color:var(--accent);text-decoration:none;font-weight:600}
/* «Сохранено» — капсула снизу */
#saved{position:fixed;left:50%;bottom:calc(20px + env(safe-area-inset-bottom));transform:translate(-50%,0);padding:10px 18px;border-radius:999px;font-size:15px;font-weight:600;color:var(--text);
background:var(--glass);-webkit-backdrop-filter:saturate(180%) blur(20px);backdrop-filter:saturate(180%) blur(20px);box-shadow:0 10px 30px rgba(0,0,0,.15);transition:opacity .25s,transform .25s;z-index:20}
#saved:empty{opacity:0;transform:translate(-50%,12px);pointer-events:none}
@media(max-width:560px){
.stat{grid-template-columns:repeat(2,1fr)}
.variants{grid-template-columns:repeat(2,1fr)}
}
</style></head><body class="wait">
<header class="nav"><b>Bambu Matrix</b><div class="lang"><button data-lang="en">EN</button><button data-lang="ru">RU</button></div></header>
<main>
<h1>Bambu Matrix</h1>

<section>
<div class="hero"><canvas id="live" width="640" height="80"></canvas></div>
<details id="statBox" class="card">
<summary><span data-i18n="details">Details</span><span id="statSum" class="val"></span></summary>
<div class="stat">
<div><span data-i18n="stPrinter">Printer</span><b id="sState">—</b></div>
<div><span data-i18n="stProgress">Progress</span><b id="sPct">—</b></div>
<div><span data-i18n="stLeft">Time left</span><b id="sRem">—</b></div>
<div><span data-i18n="stLayer">Layer</span><b id="sLayer">—</b></div>
<div><span data-i18n="stNozzle">Nozzle</span><b id="sNozzle">—</b></div>
<div><span data-i18n="stBed">Bed</span><b id="sBed">—</b></div>
<div><span data-i18n="stConn">Connection</span><b id="sConn">—</b></div>
<div><span data-i18n="stBright">Brightness</span><b id="sBright">—</b></div>
</div>
</details>
</section>

<section>
<h2 data-i18n="styleTitle">Progress style</h2>
<div class="styles" id="styles"></div>
<button id="demo" class="wide" style="margin-top:12px" data-i18n="demoMatrix">Show demo on the matrix</button>
</section>

<section>
<h2 data-i18n="screenTitle">Display and brightness</h2>
<div class="card">
<div class="opt rng"><div class="t"><b data-i18n="dayBright">Daytime brightness</b><span id="bOut" class="val"></span></div><div class="c"><input type="range" id="brightness" min="0" max="15" step="1"></div></div>
<div class="opt sel"><div class="t"><b data-i18n="nightMode">Night mode</b><small data-i18n="nightModeHint">Dim the matrix at night</small></div><div class="c"><select id="nightMode"><option value="0" data-i18n="nightOff">Off</option><option value="1" data-i18n="nightSched">On a schedule</option><option value="2" data-i18n="nightSun">Sunset to sunrise</option></select></div></div>
<div id="nightOpts">
<div class="opt in rng"><div class="t"><b data-i18n="nightBright">Night brightness</b><span id="nbOut" class="val"></span></div><div class="c"><input type="range" id="nightBrightness" min="0" max="15" step="1"></div></div>
<div class="opt in" id="nightSched"><div class="t"><b data-i18n="night">Night</b></div><div class="c"><input type="time" id="nightStart"><span class="unit">–</span><input type="time" id="nightEnd"></div></div>
<div class="opt in" id="nightSunRow"><div class="t"><small id="nightSun"></small></div></div>
</div>
<div class="opt"><div class="t"><b data-i18n="fade">Smooth fade</b><small data-i18n="fadeHint">The picture fades in and out gradually instead of switching at once</small></div><div class="c"><input type="checkbox" class="switch" id="fade"></div></div>
</div>
</section>

<section>
<h2 data-i18n="printTitle">While printing</h2>
<div class="card">
<div class="opt"><div class="t"><b data-i18n="heat">Heating</b><small data-i18n="heatHint">While the printer warms up, show nozzle and bed temperatures</small></div><div class="c"><button class="demo" data-type="heat" data-i18n="demo">Demo</button><input type="checkbox" class="switch" id="heatView"></div></div>
<div class="opt"><div class="t"><b data-i18n="pause">On pause</b></div><div class="c"><button class="demo" data-type="pause" data-i18n="demo">Demo</button></div>
<div class="seg" data-for="pauseMode"><button data-v="0" data-i18n="pause0">Screen blinks</button><button data-v="1" data-i18n="pause1">Bar blinks</button><button data-v="2" data-i18n="pause2">PAUSE</button></div>
<select id="pauseMode" class="hidden"><option value="0"></option><option value="1"></option><option value="2"></option></select></div>
</div>
</section>

<section>
<h2 data-i18n="afterTitle">After printing</h2>
<div class="card">
<div class="opt"><div class="t"><b data-i18n="doneAnim">Finish animation</b><small data-i18n="doneAnimHint">A check mark and sparks for 10 seconds when the print is done</small></div><div class="c"><button class="demo" data-type="done" data-i18n="demo">Demo</button><input type="checkbox" class="switch" id="doneAnim"></div></div>
<div class="opt"><div class="t"><b data-i18n="cool">Bed cooldown</b><small data-i18n="coolHint">After the print the matrix shows COOL and the bed temperature while it cools down. Then READY — this means the part can be taken off</small></div><div class="c"><button class="demo" data-type="cool" data-i18n="demo">Demo</button><input type="checkbox" class="switch" id="coolView"></div></div>
<div class="opt in" id="coolOpts"><div class="t"><b data-i18n="coolTarget">Show READY at</b><small data-i18n="coolTargetHint">Bed temperature at which the part comes off easily</small></div><div class="c"><input type="number" id="coolTarget" min="20" max="60"><span class="unit">°C</span></div></div>
<div class="opt"><div class="t"><b data-i18n="doneTimeout">Turn the display off after</b><small id="doneHint"></small></div><div class="c"><input type="number" id="doneTimeout" min="0" max="1440"><span class="unit" data-i18n="min">min</span></div></div>
</div>
</section>

<section>
<h2 data-i18n="idleTitle">When the printer is idle or off</h2>
<div class="card">
<div class="opt sel"><div class="t"><b data-i18n="idleShow">Show</b></div><div class="c"><button class="demo" data-type="rain" id="rainDemo" data-i18n="demo">Demo</button><select id="idleMode"><option value="0" data-i18n="idle0">Nothing</option><option value="1" data-i18n="idle1">Clock</option><option value="4" data-i18n="idle4">Clock and weather</option><option value="3" data-i18n="idle3">“Rain” screensaver</option></select></div></div>
<div class="opt in" id="clockOpts"><div class="t"><b data-i18n="clockBlink">Blinking colon</b></div><div class="c"><input type="checkbox" class="switch" id="clockBlink"></div></div>
</div>
</section>

<section>
<h2 data-i18n="timeTitle">Time and weather</h2>
<div class="card">
<div class="opt sel"><div class="t"><b data-i18n="tz">Time zone</b><small data-i18n="tzHint">For the clock and the night schedule</small></div><div class="c"><select id="tzOffset"></select></div></div>
<div class="opt"><div class="t"><b data-i18n="city">City</b></div><div class="c"><span id="cityNow" class="val">—</span><button id="cityEdit" class="demo" data-i18n="cityEdit">Change</button></div></div>
<div id="citySearch">
<div class="opt srch"><div class="c"><input id="cityQ" type="search" placeholder="Find a city" data-i18n-ph="cityPh"><button id="cityFind" data-i18n="find">Find</button></div></div>
<div class="opt" id="cityBox"><div class="t"><small id="cityMsg"></small><div id="cityRes" class="found"></div></div></div>
<div class="opt" id="cityActs"><div class="c" style="margin-left:0"><button id="cityClear" class="demo" data-i18n="cityClear">Remove city</button><button id="cityCancel" class="demo" data-i18n="cancel">Cancel</button></div></div>
</div>
<div class="opt"><div class="t"><b data-i18n="weather">Weather</b><small id="wNow">—</small></div></div>
<div class="opt"><div class="t"><b data-i18n="sun">Sunrise and sunset</b><small id="wSun">—</small></div></div>
</div>
<p class="foot" data-i18n="weatherHint">The city is used for the weather, the automatic time zone and night from sunset to sunrise. Data from open-meteo.com, or met.no if it is unreachable — no key or sign-up, updated every 15 minutes.</p>
</section>

<!-- Первый запуск: пока с этими настройками ни разу не подключились, видно только это и раздел «Принтер» -->
<section class="only-setup">
<div class="card setup-card">
<h2 data-i18n="setupTitle">Connect to your printer</h2>
<p class="lead" data-i18n="setupLead">The matrix is already on Wi-Fi. Pick your printer and enter its Access Code — the other settings open once it connects.</p>
</div>
</section>

<section class="printer">
<details id="printerBox" class="card" style="margin-top:30px">
<summary><h2 data-i18n="printerTitle">Printer</h2><span id="pSummary" class="hint"></span></summary>
<div id="pStatus" class="status">—</div>
<div id="doneBox" class="done hidden"><span data-i18n="doneBox">Done! The printer is connected — progress will appear on the matrix while printing.</span><br><button id="openSettings" class="primary" data-i18n="openSettings">Open settings</button></div>
<div class="row"><label data-i18n="found">Found on the network</label><div id="found" class="found"><span class="hint" data-i18n="searching">searching…</span></div></div>
<div class="row"><label for="ip" data-i18n="ip">IP address</label><input id="ip" placeholder="192.168.1.50" inputmode="decimal"></div>
<div class="row"><label for="code">Access Code</label><input id="code" placeholder="8 characters from the printer screen" data-i18n-ph="codePh" autocomplete="off" autocapitalize="off"></div>
<div class="row adv"><label data-i18n="serial">Serial number</label><span id="serial" class="val" style="margin-left:auto">—</span></div>
<div class="row adv"><label for="serialIn" data-i18n="serialIn">Serial number, manual</label><input id="serialIn" placeholder="only if it wasn't detected" data-i18n-ph="serialPh" autocapitalize="characters"></div>
<button id="forget" class="destructive" data-i18n="forget">Forget this printer</button>
</details>
<p class="foot" data-i18n="codeHint">The IP and Access Code are on the printer screen in the network (WLAN) settings. The printer and the matrix must be on the same network.</p>
</section>

<p class="only-setup skip"><a href="#" id="skipSetup" data-i18n="skip">Set up without the printer</a> <span data-i18n="skipWhy">— if it's off right now</span></p>

<section class="links">
<h2 data-i18n="sysTitle">System</h2>
<div class="card">
<div class="opt"><div class="t"><b data-i18n="fwVer">Firmware version</b></div><div class="c"><span id="fwVer" class="val">—</span></div></div>
<div id="updStatus" class="status">—</div>
<pre id="updNotes" class="notes hidden"></pre>
<div class="opt"><div class="c" style="margin-left:0;flex-wrap:wrap"><button id="updInstall" class="primary hidden" data-i18n="updInstall">Install update</button><button id="updCheck" data-i18n="updCheck">Check for updates</button></div></div>
</div>
<div class="card" style="margin-top:14px">
<a class="nav-row" href="/wifi">Wi-Fi</a><a class="nav-row" href="/update" data-i18n="update">Update firmware</a><a class="nav-row" href="/info" data-i18n="info">Info</a><a class="nav-row" href="/restart" data-i18n="restart">Restart</a>
<details id="picCheck">
<summary data-i18n="picTitle">Picture looks wrong?</summary>
<p class="hint" data-i18n="picHint">Matrix modules are wired differently, and the matrix itself can't tell which kind you have. Press the variants one by one and keep the one where the matrix reads <b>ABC 123</b> left to right with the arrow pointing right.</p>
<div class="variants"><button data-mod="1" data-i18n="v1">Variant 1</button><button data-mod="0" data-i18n="v2">Variant 2</button><button data-mod="2" data-i18n="v3">Variant 3</button><button data-mod="3" data-i18n="v4">Variant 4</button></div>
<div class="opt"><div class="c" style="margin-left:0"><button id="picSave" class="primary" data-i18n="picSave">Save this variant</button><button id="picCancel" data-i18n="cancel">Cancel</button></div></div>
<div class="hint" id="picMsg"></div>
<div class="opt"><div class="t"><b data-i18n="flip">Rotate 180°</b><small data-i18n="flipHint">if the matrix is mounted upside down</small></div><div class="c"><input type="checkbox" class="switch" id="flip"></div></div>
</details>
</div>
</section>
<div id="saved"></div>
</main>
<script>
/* ---------- языки: английский в разметке, русский — здесь ---------- */
const RU={liveTitle:'Сейчас на матрице',details:'Подробности',stPrinter:'Принтер',stProgress:'Прогресс',stLeft:'Осталось',stLayer:'Слой',stNozzle:'Сопло',stBed:'Стол',stConn:'Связь',stBright:'Яркость',
styleTitle:'Вид прогресса',demoMatrix:'Показать демо на матрице',screenTitle:'Экран и яркость',dayBright:'Яркость днём',nightMode:'Ночной режим',nightModeHint:'Ночью матрица светит тусклее',nightOff:'Выключен',nightSched:'По расписанию',nightSun:'От заката до рассвета',
nightBright:'Яркость ночью',night:'Ночь',fade:'Плавное включение',fadeHint:'Картинка проявляется и гаснет постепенно, а не мгновенно',min:'мин',flip:'Повернуть на 180°',
printTitle:'Во время печати',heat:'Нагрев',heatHint:'Пока принтер греется — температура сопла и стола',demo:'Демо',pause:'На паузе',pause0:'Мигает экран',pause1:'Мигает полоса',pause2:'PAUSE',
afterTitle:'После печати',doneAnim:'Анимация окончания',doneAnimHint:'Галочка и искры 10 секунд после окончания печати',cool:'Остывание стола',coolHint:'После печати на матрице отображается COOL и температура стола, пока он остывает. После — READY, это значит деталь можно снимать',coolTarget:'Показать READY при',coolTargetHint:'Температура стола, при которой деталь легко снимается',doneTimeout:'Погасить экран через',
idleTitle:'Когда принтер не печатает (или выключен)',idleShow:'Показывать',idle0:'Ничего',idle1:'Часы',idle4:'Часы и погода',idle3:'Заставка «дождь»',clockBlink:'Мигающее двоеточие',
timeTitle:'Время и погода',tz:'Часовой пояс',tzHint:'Для часов и ночного расписания',city:'Город',cityClear:'Убрать город',cityEdit:'Изменить',cityPh:'Найти город',find:'Найти',weather:'Погода',sun:'Восход и закат',
weatherHint:'Город нужен для погоды, автоматического часового пояса и ночи от заката до рассвета. Данные с open-meteo.com, а если он недоступен — с met.no, без ключа и регистрации, обновляются раз в 15 минут.',
setupTitle:'Подключение к принтеру',setupLead:'Матрица уже в Wi-Fi. Осталось выбрать принтер и ввести его Access Code — остальные настройки откроются после подключения.',
printerTitle:'Принтер',forget:'Забыть этот принтер',doneBox:'Готово! Принтер подключён — во время печати на матрице появится прогресс.',openSettings:'Открыть настройки',found:'Найдены в сети',searching:'ищу…',ip:'IP-адрес',codePh:'8 символов с экрана принтера',
codeHint:'IP и Access Code есть на экране принтера в настройках сети (WLAN). Принтер и матрица должны быть в одной сети.',serial:'Серийный номер',serialIn:'Серийник вручную',serialPh:'только если не определился сам',
picTitle:'Изображение выглядит неправильно?',picHint:'Модули матриц разводят по-разному, и сама матрица не может определить, какие у тебя. Нажимай варианты по очереди и оставь тот, при котором на матрице читается <b>ABC 123</b> слева направо, а стрелка смотрит вправо.',v1:'Вариант 1',v2:'Вариант 2',v3:'Вариант 3',v4:'Вариант 4',picSave:'Сохранить этот вариант',cancel:'Отмена',flipHint:'если матрица висит вверх ногами',
fwVer:'Версия прошивки',updInstall:'Обновить',updCheck:'Проверить обновления',
skip:'Настроить без принтера',skipWhy:'— если он сейчас выключен',sysTitle:'Система',update:'Обновить прошивку',info:'Инфо',restart:'Перезагрузить'};
/* строки, которые собираются в коде: [английский, русский], {0} — подстановка */
const T={updChecking:['Checking for updates…','Проверяю обновления…'],updNever:['Press “Check for updates” to see if there is a new version','Нажми «Проверить обновления», чтобы узнать, есть ли новая версия'],updLatest:['You have the latest version','Установлена последняя версия'],
updAvailable:['Version {0} is available','Доступна версия {0}'],updNoRelease:['No releases on GitHub yet','На GitHub ещё нет релизов'],updError:["Couldn't reach GitHub — try again later",'Не удалось связаться с GitHub — попробуй позже'],
updFailed:['The update failed — the current version keeps working','Обновление не удалось — текущая версия продолжает работать'],updInstalling:['Installing the update — the matrix shows OTA and restarts, this page reconnects by itself','Ставлю обновление — на матрице OTA, потом перезагрузка, страница переподключится сама'],
updDone:['Updated to {0}','Обновлено до {0}'],updConfirm:['Install version {0}? The matrix will restart, settings are kept.','Установить версию {0}? Матрица перезагрузится, настройки сохранятся.'],
sumOff:['printer is off','принтер выключен'],sumErr:['needs attention','требует внимания'],
picShown:['Showing it on the matrix for 30 seconds — take a look','Показываю на матрице 30 секунд — посмотри'],picPick:['Pick a variant first','Сначала выбери вариант'],
saved:['Saved','Сохранено'],saveFail:['Could not save','Не удалось сохранить'],noPrinters:['nobody yet — is the printer on and on the same network?','пока никого — принтер включён и в той же сети?'],printer:['Printer','Принтер'],
needCode:['Enter the Access Code from the printer screen.','Введи Access Code с экрана принтера.'],notSet:['Printer not set up — pick it from the ones found on the network or enter its IP, then the Access Code.','Принтер не настроен — выбери его из найденных в сети или введи IP, затем Access Code.'],
badCode:['The printer rejected the Access Code — check the code on the printer screen.','Принтер отклонил Access Code — проверь код на экране принтера.'],badSerial:["Couldn't get the serial number — enter it manually below.",'Не удалось узнать серийный номер — введи его вручную ниже.'],
connected:['Connected','Подключено'],forgetConfirm:['Forget this printer? The IP, Access Code and serial number are erased, and the matrix will wait for a new connection. Other settings stay.','Забыть этот принтер? IP, Access Code и серийный номер сотрутся, матрица будет ждать нового подключения. Остальные настройки останутся.'],notFound:["The printer at {0} doesn't respond. Check the IP, that the printer is on and on the same network.",'Принтер по адресу {0} не отвечает. Проверь IP, что принтер включён и в той же сети.'],
offline:['The printer is off or unreachable.','Принтер выключен или недоступен.'],connecting:['Connecting…','Подключаюсь…'],
sunNoCity:['Pick a city under “Time and weather” — until then night follows the schedule {0}–{1}.','Выбери город в разделе «Время и погода» — пока ночь считается по расписанию {0}–{1}.'],sunToday:['Tonight is from {0} to {1}.','Сегодня ночь с {0} до {1}.'],sunWait:['Waiting for sunset and sunrise times…','Жду время заката и рассвета…'],
neverOff:["0 — keep it on","0 — не гасить"],fromReady:['Counted from READY; 0 keeps it on','Отсчёт от READY; 0 — не гасить'],fromFinish:['Counted from the end of the print; 0 keeps it on','Отсчёт от окончания печати; 0 — не гасить'],
cityNone:['not selected','не выбран'],cityEmpty:['Enter a city name','Введи название города'],citySearching:['Searching…','Ищу…'],cityPick:['Pick a city:','Выбери город:'],cityNothing:['Nothing found','Ничего не нашлось'],cityNoNet:['No connection to open-meteo.com','Нет связи с open-meteo.com'],
tzAuto:['Auto (by city)','Авто (по городу)'],demoState:['Demo','Демо'],hm:['{0} h {1} min','{0} ч {1} мин'],no:['no','нет'],noEsp:['no connection to the ESP','нет связи с ESP'],day:['day','день'],nightW:['night','ночь'],
updated:[', updated {0} min ago',', обновлено {0} мин назад'],loading:['loading…','загружается…'],wFail:['no data: open-meteo.com and met.no are unreachable, retrying every minute','нет данных: open-meteo.com и met.no недоступны, повтор раз в минуту'],noCity:['no city selected','город не выбран'],sunTimes:['sunrise {0}, sunset {1}','восход {0}, закат {1}'],
w0:['clear','ясно'],w1:['partly cloudy','переменная облачность'],w3:['overcast','пасмурно'],w45:['fog','туман'],w71:['snow','снег'],w95:['thunderstorm','гроза'],w51:['rain','дождь'],
s1:['Percent and framed bar','Процент и полоса в рамке'],s3:['Small text and thin bar','Мелкий текст и тонкая полоса'],s4:['10% segments','Сегменты по 10%'],s5:['Fill the whole matrix','Заливка всей матрицы'],s6:['Percent, bar and time','Процент, полоса и время'],s7:['Big digits and a line','Крупные цифры и линия'],
IDLE:['Idle','Простой'],PREPARE:['Preparing','Подготовка'],RUNNING:['Printing','Печать'],PAUSE:['Paused','Пауза'],FINISH:['Finished','Готово'],FAILED:['Failed','Ошибка'],SLICING:['Slicing','Нарезка']};
let lang='en';
function t(k,...a){const v=(T[k]||[k,k])[lang=='ru'?1:0];return v.replace(/\{(\d)\}/g,(m,i)=>a[i])}
/* английский текст запоминаем из разметки при первом переключении, чтобы можно было вернуться */
function applyLang(l){lang=l=='ru'?'ru':'en';document.documentElement.lang=lang;
document.querySelectorAll('[data-i18n]').forEach(el=>{if(el.dataset.en===undefined)el.dataset.en=el.innerHTML;el.innerHTML=lang=='ru'&&RU[el.dataset.i18n]!==undefined?RU[el.dataset.i18n]:el.dataset.en});
document.querySelectorAll('[data-i18n-ph]').forEach(el=>{if(el.dataset.enPh===undefined)el.dataset.enPh=el.placeholder;el.placeholder=lang=='ru'?RU[el.dataset.i18nPh]:el.dataset.enPh});
document.querySelectorAll('.lang button').forEach(b=>b.classList.toggle('on',b.dataset.lang==lang));
const o=document.querySelector('#tzOffset option[value="9999"]');if(o)o.textContent=t('tzAuto');
if(cfg.style)buildStyles();showCity();refresh();findPrinters()}

/* ---------- превью видов прогресса ---------- */
const F3={'0':'111101101101111','1':'010110010010111','2':'111001111100111','3':'111001111001111','4':'101101111001001','5':'111100111001111','6':'111100111101111','7':'111001001001001','8':'111101111101111','9':'111101111001111','-':'000000111000000'};
const F6={'0':'111101101101101111','1':'010110010010010111','2':'111001001111100111','3':'111001111001001111','4':'101101101111001001','5':'111100111001001111','6':'111100111101101111','7':'111001001001001001','8':'111101111101101111','9':'111101101111001111'};
const P5=['11001','11010','00100','01011','10011'],P6=['110001','110010','000100','001000','010011','100011'];
const F5={'0':[62,81,73,69,62],'1':[4,2,127],'2':[113,73,73,73,70],'3':[65,73,73,73,54],'4':[15,8,8,8,127],'5':[79,73,73,73,49],'6':[62,73,73,73,48],'7':[3,1,1,1,127],'8':[54,73,73,73,54],'9':[6,73,73,73,62],'%':[99,19,8,100,99],':':[108,108],'-':[8,8,8,8]};
const $=id=>document.getElementById(id);
function mk(){return new Uint8Array(64)}
function px(f,x,y){if(x>=0&&x<64&&y>=0&&y<8)f[x]|=1<<y}
function rect(f,x0,y0,x1,y1){for(let x=x0;x<=x1;x++)for(let y=y0;y<=y1;y++)px(f,x,y)}
function frame(f,a,b){for(let x=a;x<=b;x++)f[x]|=0x81;f[a]=f[b]=255}
function rows(f,x,y,r){r.forEach((l,ry)=>[...l].forEach((v,rx)=>{if(v=='1')px(f,x+rx,y+ry)}));return x+r[0].length+1}
function sw(s){let w=0;for(const c of s)w+=(c==':'?1:c=='%'?5:3)+1;return w-1}
function small(f,x,y,s){for(const c of s){if(c==':'){px(f,x,y+1);px(f,x,y+3);x+=2;continue}if(c=='%'){x=rows(f,x,y,P5);continue}const g=F3[c];if(g)for(let i=0;i<15;i++)if(g[i]=='1')px(f,x+i%3,y+(i/3|0));x+=4}}
function mw(s){let w=0;for(const c of s)w+=(c==':'?1:c=='%'?6:3)+1;return w-1}
function mid(f,x,y,s){for(const c of s){if(c==':'){px(f,x,y+1);px(f,x,y+4);x+=2}else if(c=='%')x=rows(f,x,y,P6);else{const g=F6[c];if(g)for(let i=0;i<18;i++)if(g[i]=='1')px(f,x+i%3,y+(i/3|0));x+=4}}}
function bw(s){let w=0;for(const c of s)w+=(F5[c]||[0]).length+1;return w-1}
function big(f,x,s,inv){for(const c of s){const g=F5[c]||[0];g.forEach((b,i)=>{const X=x+i;if(X>=0&&X<64)f[X]=inv?f[X]^b:f[X]|b});x+=g.length+1}}
function sc(p,n){return Math.round(Math.min(100,Math.max(0,p))*n/100)}
function draw(style,p,rem){const f=mk(),P=p+'%',T=Math.floor(rem/60)+':'+String(rem%60).padStart(2,'0');let k;
switch(style){
case 1:mid(f,0,1,P);frame(f,19,63);k=sc(p,43);if(k)rect(f,20,2,19+k,5);break;
case 3:small(f,0,0,P);small(f,64-sw(T),0,T);k=sc(p,64);if(k)rect(f,0,6,k-1,7);break;
case 4:mid(f,0,1,P);{const full=Math.floor(p/10);for(let i=0;i<10;i++){const x=15+i*5;if(i<full)rect(f,x,1,x+3,6);else{px(f,x,1);px(f,x+3,1);px(f,x,6);px(f,x+3,6)}}}break;
case 5:k=sc(p,512);for(let i=0;i<k;i++)px(f,i>>3,7-i%8);break;
case 6:mid(f,0,1,P);mid(f,64-mw(T),1,T);frame(f,16,46);k=sc(p,29);if(k)rect(f,17,2,16+k,5);break;
default:big(f,(64-bw(P))>>1,P);k=sc(p,64);if(k)rect(f,0,7,k-1,7)}
return f}
function paint(cv,f){const c=cv.getContext('2d'),s=cv.width/64,css=getComputedStyle(document.documentElement);const on=css.getPropertyValue('--led'),off=css.getPropertyValue('--off');c.clearRect(0,0,cv.width,cv.height);
for(let x=0;x<64;x++)for(let y=0;y<8;y++){const lit=(f[x]>>y)&1;c.fillStyle=lit?on:off;c.shadowColor=lit?on:'transparent';c.shadowBlur=lit?s*.6:0;c.beginPath();c.arc(x*s+s/2,y*s+s/2,s*.36,0,7);c.fill()}}

/* ---------- настройки ---------- */
let cfg={},timer;
function save(){clearTimeout(timer);timer=setTimeout(async()=>{const r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(cfg)});$('saved').textContent=r.ok?t('saved'):t('saveFail');setTimeout(()=>$('saved').textContent='',1500)},300)}
const STYLES=[1,3,4,5,6,7];
function buildStyles(){const box=$('styles');box.innerHTML='';STYLES.forEach((n,i)=>{const d=document.createElement('div');d.className='style'+(cfg.style==n?' sel':'');d.innerHTML='<div class="panel"><canvas width="320" height="40"></canvas></div><div>'+(i+1)+'. '+t('s'+n)+'</div>';d.onclick=()=>{cfg.style=n;buildStyles();save()};box.appendChild(d);paint(d.querySelector('canvas'),draw(n,42,83))})}
/* ползунок iOS: закрашенная часть дорожки — через --p */
function fill(el){el.style.setProperty('--p',(el.value-el.min)/(el.max-el.min)*100+'%')}
function bind(id,num){const el=$(id);el.addEventListener(el.type=='range'?'input':'change',()=>{if(el.type=='range')fill(el);cfg[id]=el.type=='checkbox'?el.checked:num?+el.value:el.value;if(id=='brightness')$('bOut').textContent=el.value;if(id=='nightBrightness')$('nbOut').textContent=el.value;save()})}
const hm=m=>String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0');
let sun=null;
/* зависимые настройки показываем только когда они имеют смысл */
function refresh(){if(!cfg.style)return;
document.querySelectorAll('.seg').forEach(g=>g.querySelectorAll('button').forEach(b=>b.classList.toggle('on',b.dataset.v==$(g.dataset.for).value)));const nm=+$('nightMode').value;$('nightOpts').classList.toggle('hidden',nm==0);$('nightSched').classList.toggle('hidden',nm==2);$('nightSunRow').classList.toggle('hidden',nm!=2);
$('nightSun').textContent=nm!=2?'':!cfg.city?t('sunNoCity',hm(cfg.nightStart),hm(cfg.nightEnd)):sun?t('sunToday',hm(sun[1]),hm(sun[0])):t('sunWait');
$('coolOpts').classList.toggle('hidden',!$('coolView').checked);const im=+$('idleMode').value;$('clockOpts').classList.toggle('hidden',im!=1&&im!=4);$('rainDemo').classList.toggle('hidden',im!=3);
$('doneHint').textContent=+$('doneTimeout').value==0?t('neverOff'):$('coolView').checked?t('fromReady'):t('fromFinish')}
async function init(){
const tz=$('tzOffset');for(let m=-720;m<=840;m+=30){if(m%60&&![-210,210,270,330,570,630].includes(m))continue;const h=Math.trunc(m/60),mm=Math.abs(m%60);const o=document.createElement('option');o.value=m;o.textContent='UTC'+(m<0?'−':'+')+Math.abs(h)+(mm?':'+mm:'');tz.appendChild(o)}
{const o=document.createElement('option');o.value=9999;tz.prepend(o)}
cfg=await(await fetch('/api/settings')).json();
$('clockBlink').checked=cfg.clockBlink;$('serialIn').value=cfg.serial||'';
['ip','code'].forEach(k=>$(k).value=cfg[k]);
['brightness','nightBrightness','nightMode','doneTimeout','idleMode','tzOffset','pauseMode','coolTarget'].forEach(k=>$(k).value=cfg[k]);
$('nightStart').value=hm(cfg.nightStart);$('nightEnd').value=hm(cfg.nightEnd);$('nbOut').textContent=cfg.nightBrightness;fill($('brightness'));fill($('nightBrightness'));
$('flip').checked=cfg.flip;$('heatView').checked=cfg.heatView;$('doneAnim').checked=cfg.doneAnim;$('coolView').checked=cfg.coolView;$('fade').checked=cfg.fade;$('bOut').textContent=cfg.brightness;
['brightness','nightBrightness','nightMode','doneTimeout','idleMode','tzOffset','pauseMode','coolTarget'].forEach(k=>bind(k,true));
bindPicCheck();bindUpdates();
['nightStart','nightEnd'].forEach(k=>$(k).addEventListener('change',()=>{const [h,m]=$(k).value.split(':');cfg[k]=(+h)*60+(+m);save()}));
document.querySelectorAll('select,input').forEach(el=>el.addEventListener('change',refresh));bind('flip');bind('heatView');bind('doneAnim');bind('coolView');bind('fade');bind('ip');bind('code');bind('clockBlink');
$('serialIn').addEventListener('change',()=>{cfg.serial=$('serialIn').value.trim();save()});
document.querySelectorAll('.seg').forEach(g=>g.querySelectorAll('button').forEach(b=>b.onclick=()=>{const el=$(g.dataset.for);el.value=b.dataset.v;el.dispatchEvent(new Event('change'))}));
$('cityFind').onclick=findCity;$('cityQ').addEventListener('keydown',e=>{if(e.key=='Enter')findCity()});
$('cityEdit').onclick=()=>{cityEditing=true;showCity();$('cityQ').focus()};
$('cityCancel').onclick=()=>{cityEditing=false;$('cityRes').innerHTML='';$('cityMsg').textContent='';showCity()};
$('cityClear').onclick=()=>{cityEditing=false;cfg.city='';cfg.lat=0;cfg.lon=0;cfg.cityTz=9999;showCity();refresh();save()};
document.querySelectorAll('.lang button').forEach(b=>b.onclick=()=>{cfg.lang=b.dataset.lang;applyLang(cfg.lang);save()});
applyLang(cfg.lang);
$('demo').onclick=()=>fetch('/api/demo',{method:'POST'});
$('forget').onclick=async()=>{if(!confirm(t('forgetConfirm')))return;
const r=await fetch('/api/printer/forget',{method:'POST'});if(!r.ok){$('saved').textContent=t('saveFail');return}
cfg.ip=cfg.code=cfg.serial='';['ip','code','serialIn'].forEach(k=>$(k).value='');lastErr='';setSetup(true);findPrinters()};
$('openSettings').onclick=()=>setSetup(false);$('skipSetup').onclick=e=>{e.preventDefault();setSetup(false)};
document.querySelectorAll('.demo[data-type]').forEach(b=>b.onclick=()=>fetch('/api/demo?type='+b.dataset.type,{method:'POST'}));
/* открыт ли спойлер с подробностями — помним в этом браузере */
try{$('statBox').open=localStorage.statOpen=='1'}catch(e){}
$('statBox').addEventListener('toggle',()=>{try{localStorage.statOpen=$('statBox').open?'1':'0'}catch(e){}});
addEventListener('scroll',()=>document.body.classList.toggle('scrolled',scrollY>40),{passive:true});
poll();setInterval(poll,700);setInterval(findPrinters,3000)}

/* ---------- обновления прошивки с GitHub ---------- */
let fwAtStart=null,updLocal='',updLatest='';  /* updLocal: 'checking' | 'installing' — пока ESP не ответила сама */
function showUpdate(s){const u=s.upd||{};updLatest=u.latest||'';if(fwAtStart===null)fwAtStart=s.fw;$('fwVer').textContent=s.fw||'—';
if(updLocal=='installing'&&s.fw!==fwAtStart){updLocal='';fwAtStart=s.fw;$('updStatus').textContent=t('updDone',s.fw);$('updStatus').className='status ok';$('updInstall').classList.add('hidden');$('updNotes').classList.add('hidden');return}
if(updLocal=='checking'&&u.state==0&&u.checked)updLocal='';
let m,c='';
if(updLocal=='installing'||u.state==2)m=t('updInstalling');
else if(updLocal=='checking'||u.state==1)m=t('updChecking');
else if(!u.checked)m=t('updNever');
else if(u.error=='no_release')m=t('updNoRelease');
else if(u.error=='install'){m=t('updFailed');c='bad'}
else if(u.error){m=t('updError');c='bad'}
else if(u.available){m=t('updAvailable',u.latest);c='ok'}
else m=t('updLatest');
$('updStatus').textContent=m;$('updStatus').className='status '+c;
const showNotes=u.available&&u.notes&&!updLocal;$('updNotes').textContent=showNotes?u.notes:'';$('updNotes').classList.toggle('hidden',!showNotes);
$('updInstall').classList.toggle('hidden',!(u.available&&u.state==0&&!updLocal))}
function bindUpdates(){
$('updCheck').onclick=()=>{updLocal='checking';fetch('/api/update/check',{method:'POST'})};
$('updInstall').onclick=()=>{if(!confirm(t('updConfirm',updLatest)))return;updLocal='installing';fetch('/api/update/install',{method:'POST'})}}

/* ---------- проверка изображения: тип модулей подбирается на глаз ---------- */
let testMod=null;
function modTest(type){return fetch('/api/modtest?type='+type,{method:'POST'})}
function markVariants(){document.querySelectorAll('.variants button').forEach(b=>b.classList.toggle('sel',+b.dataset.mod===(testMod===null?cfg.moduleType:testMod)))}
function endTest(msg){if(testMod!==null)modTest(-1);testMod=null;markVariants();$('picMsg').textContent=msg||''}
function bindPicCheck(){
document.querySelectorAll('.variants button').forEach(b=>b.onclick=()=>{testMod=+b.dataset.mod;modTest(testMod);markVariants();$('picMsg').textContent=t('picShown')});
$('picSave').onclick=()=>{if(testMod===null){$('picMsg').textContent=t('picPick');return}cfg.moduleType=testMod;save();endTest(t('saved'))};
$('picCancel').onclick=()=>endTest();
$('picCheck').addEventListener('toggle',()=>{if(!$('picCheck').open)endTest()});
markVariants()}

/* ---------- принтер ---------- */
/* принтеры, которые сами объявили о себе в сети */
async function findPrinters(){if(!cfg.style)return;try{const j=await(await fetch('/api/printers')).json(),box=$('found');box.innerHTML='';
if(!j.printers.length){box.innerHTML='<span class="hint">'+t('noPrinters')+'</span>';return}
j.printers.forEach(p=>{const b=document.createElement('button');b.textContent=[p.name||t('printer'),p.model,p.ip].filter(Boolean).join(' · ');if(p.ip==cfg.ip)b.className='sel';
b.onclick=()=>{cfg.ip=p.ip;$('ip').value=p.ip;save();findPrinters();if(!cfg.code)$('code').focus()};box.appendChild(b)})}catch(e){}}
/* первый запуск: решаем по первому ответу ESP — подключались ли уже с этими настройками */
let setupMode=null;
function setSetup(on){setupMode=on;$('printerBox').open=on;document.body.classList.remove('wait');document.body.classList.toggle('setup',on);if(!on)window.scrollTo(0,0)}
function setupState(s){$('forget').classList.toggle('hidden',!s.configured&&!cfg.ip);if(setupMode===null)setSetup(!s.verified);document.body.classList.toggle('serial-err',s.error=='serial');$('doneBox').classList.toggle('hidden',!(setupMode&&s.verified))}
function printerStatus(s){const el=$('pStatus');let m,c='';
if(!s.configured)m=cfg.ip?t('needCode'):t('notSet');
else if(s.error=='code'){m=t('badCode');c='bad'}
else if(s.error=='serial'){m=t('badSerial');c='bad'}
else if(s.mqtt){m=t('connected')+(s.serial?' · '+s.serial:'');c='ok'}
else if(s.error=='not_found'&&!s.verified){m=t('notFound',cfg.ip);c='bad'}
else m=s.verified?t('offline'):t('connecting');
el.textContent=m;el.className='status '+c;
const sum=$('pSummary');sum.textContent=s.mqtt?m:c=='bad'?t('sumErr'):s.verified?t('sumOff'):m;sum.className='hint '+c;
if((s.error=='code'||s.error=='serial')&&lastErr!==s.error)$('printerBox').open=true;lastErr=s.error}
let lastErr='';

/* ---------- время и погода ---------- */
// Сдвиг часового пояса города от UTC в минутах, сейчас (для met.no — он пояс не сообщает)
function tzMin(z){try{const d=new Date(),p=new Intl.DateTimeFormat('en-US',{timeZone:z,hourCycle:'h23',year:'numeric',month:'numeric',day:'numeric',hour:'numeric',minute:'numeric'}).formatToParts(d),g=k=>+p.find(x=>x.type==k).value;
return Math.round((Date.UTC(g('year'),g('month')-1,g('day'),g('hour'),g('minute'))-d.getTime())/900000)*15}catch(e){return 9999}}
/* город выбран — поиск спрятан за кнопкой «Изменить» */
let cityEditing=false;
function showCity(){$('cityNow').textContent=cfg.city||t('cityNone');const has=!!cfg.city;$('cityEdit').classList.toggle('hidden',!has||cityEditing);$('citySearch').classList.toggle('hidden',has&&!cityEditing);$('cityActs').classList.toggle('hidden',!has)}
async function findCity(){const q=$('cityQ').value.trim();$('cityRes').innerHTML='';if(!q){$('cityMsg').textContent=t('cityEmpty');return}
$('cityMsg').textContent=t('citySearching');try{const j=await(await fetch('https://geocoding-api.open-meteo.com/v1/search?count=5&format=json&language='+lang+'&name='+encodeURIComponent(q))).json();
const r=j.results||[];$('cityMsg').textContent=r.length?t('cityPick'):t('cityNothing');
r.forEach(c=>{const b=document.createElement('button');b.textContent=[c.name,c.admin1,c.country].filter(Boolean).join(', ');b.onclick=()=>{cityEditing=false;cfg.city=c.name;cfg.lat=c.latitude;cfg.lon=c.longitude;cfg.cityTz=tzMin(c.timezone);showCity();$('cityRes').innerHTML='';$('cityMsg').textContent='';$('cityQ').value='';refresh();save()};$('cityRes').appendChild(b)})}
catch(e){$('cityMsg').textContent=t('cityNoNet')}}
const WT=c=>t(c==0?'w0':c<=2?'w1':c==3?'w3':c==45||c==48?'w45':(c>=71&&c<=77)||c==85||c==86?'w71':c>=95?'w95':'w51');

/* ---------- статус ---------- */
async function poll(){try{const s=await(await fetch('/api/state')).json();const f=mk();if(s.display)for(let i=0;i<64;i++)f[i]=parseInt(s.fb.substr(i*2,2),16);paint($('live'),f);
$('sState').textContent=s.demo?t('demoState'):(T[s.state]?t(s.state):s.state||'—');$('sPct').textContent=s.percent>=0?s.percent+'%':'—';
$('sRem').textContent=s.remaining>=0?t('hm',Math.floor(s.remaining/60),s.remaining%60):'—';
$('sLayer').textContent=s.layers>0?s.layer+' / '+s.layers:'—';
$('statSum').textContent=[$('sState').textContent,$('sPct').textContent,$('sRem').textContent].filter(v=>v&&v!='—').join(' · ');
const tt=(c,g)=>c>=0?Math.round(c)+'°'+(g>0?' / '+Math.round(g)+'°':''):'—';$('sNozzle').textContent=tt(s.nozzle,s.nozzleTarget);$('sBed').textContent=tt(s.bed,s.bedTarget);$('sConn').textContent=s.mqtt?'OK, '+s.wifi+' dBm':t('no');$('serial').textContent=s.serial||'—';printerStatus(s);setupState(s);showUpdate(s);
sun=s.sunrise!==undefined?[s.sunrise,s.sunset]:null;$('wSun').textContent=sun?t('sunTimes',hm(sun[0]),hm(sun[1])):'—';$('sBright').textContent=(s.night?t('nightW'):t('day'))+', '+(s.night?cfg.nightBrightness:cfg.brightness);refresh();
$('wNow').textContent=s.wTemp!==undefined?(s.wTemp>0?'+':'')+Math.round(s.wTemp)+'°, '+WT(s.wCode)+(s.wAge>0?t('updated',s.wAge):'')+(s.wSrc?' · '+s.wSrc:''):(cfg.city?(s.wFail?t('wFail'):t('loading')):t('noCity'))}catch(e){$('sConn').textContent=t('noEsp');if(setupMode===null)setSetup(false)}}
init().catch(()=>setSetup(false));
</script></body></html>)rawliteral";
