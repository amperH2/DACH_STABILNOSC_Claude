const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const source=fs.readFileSync(path.join(__dirname,'../src/web_ui.h'),'utf8');
new Function(source.slice(source.indexOf('<script>')+8,source.indexOf('</script>')));
const presentation=source.slice(source.indexOf('function parkPresentation('),source.indexOf('let lastStateAt=0;'));
const view=new Function(presentation+';return parkPresentation;')();
const baseline={park_guard_enabled:true,park_ra:true,park_dec:true,park_hw_stable:true,park_motion_ready:true,
  alpaca_fails:255,park_alpaca:{configured:true,transport_ready:true,attempted:true,attempt_age_s:5,success_seen:false,current:false,last_attempt_ok:false,value:null},
  safety_ext:{nina_active:false,safetymonitor_connected:true,safetymonitor_seen:true,safetymonitor_age_s:41687,alpaca_age_s:41687}};
const clone=x=>JSON.parse(JSON.stringify(x));
let v=view(baseline);assert(v.stable&&v.ready&&!v.current&&!v.ninaActive);assert(v.remote.includes('brak aktualnego potwierdzenia'));assert(v.nina.includes('41687 s'));
const fresh=clone(baseline);Object.assign(fresh.park_alpaca,{current:true,value:true,last_value:true,success_seen:true,success_age_s:2,last_attempt_ok:true});
v=view(fresh);assert(v.current);assert(v.remote.includes('PARK potwierdzony'));
for(const field of ['current','success_seen','last_attempt_ok','transport_ready','configured']){const bad=clone(fresh);bad.park_alpaca[field]=false;assert(!view(bad).current,field);}
for(const age of [16,41687,null,'2',-1]){const stale=clone(fresh);stale.park_alpaca.success_age_s=age;assert(!view(stale).current,'age '+age);}
const historical=clone(fresh);historical.park_alpaca.current=false;historical.park_alpaca.success_age_s=41687;v=view(historical);assert(!v.current&&v.remote.includes('historyczny'));assert(!v.remote.includes('PARK potwierdzony'));
const outside=clone(fresh);outside.park_alpaca.value=false;assert(view(outside).remote.includes('POZA PARKIEM'));
const pending=clone(baseline);pending.park_hw_stable=false;pending.park_motion_ready=false;assert(view(pending).hardware.includes('oczekiwanie'));
const absent=clone(baseline);delete absent.park_ra;assert(view(absent).hardware.includes('brak danych'));assert(!view(absent).ready);
for(const ra of [false,true])for(const dec of [false,true])for(const age of [0,2,3,41687])for(const connected of [false,true]){
 const st=clone(baseline);st.park_ra=ra;st.park_dec=dec;st.safety_ext={nina_active:true,safetymonitor_connected:connected,safetymonitor_seen:true,safetymonitor_age_s:age};
 const p=view(st);assert.equal(p.stable,ra&&dec);assert.equal(p.ready,ra&&dec);assert.equal(p.ninaActive,connected&&age<3);
}
assert(!source.includes('(Alpaca!)'));assert(!source.includes('Dach można bezpiecznie zamknąć'));assert(!source.includes('stabilny park - ruch dozwolony'));
const isNum=v=>v!==null&&v!==undefined&&typeof v!=='string'&&Number.isFinite(v);
const classifiers=new Function('isNum',source.slice(source.indexOf('function skyClass('),source.indexOf('// Neutralne pasma natezenia'))+';return {skyClass,wClass};')(isNum);
assert(classifiers.skyClass(null).txt.includes('Brak'));assert.equal(classifiers.wClass(null),'');assert.equal(classifiers.skyClass(12,{cloud_clear:10,cloud_scattered:6,cloud_overcast:2}).txt,'CZYSTE');assert.equal(classifiers.wClass(8,{wind_warn:5,wind_sustained:7}),'bad');
const relayElements={relayStateGrid:{},modbusRelaySummary:{}};
const relayCode=source.slice(source.indexOf('function renderRelayStates('),source.indexOf('function renderDiag('));
new Function('$',relayCode+';renderRelayStates({modbus_relay_state_valid:true,modbus_relay_readback_current:false,modbus_relay_read_age_ms:12000,modbus_relay_online:true,modbus_relay_ok:true,modbus_relay_commanded:[true],modbus_relay_actual:[false]});')(id=>relayElements[id]);
assert(relayElements.modbusRelaySummary.textContent.includes('NIEAKTUALNY'));assert(relayElements.relayStateGrid.innerHTML.includes('UNKNOWN'));assert(!relayElements.relayStateGrid.innerHTML.includes('MISMATCH'));
const elements={};for(const id of ['staleBanner','safetyTxt','safetyPill','chipPark','btnOpen','btnClose','btnStop'])elements[id]={hidden:true,disabled:false,style:{},setAttribute(){}};
const body={classList:{add(){},contains(){return true},toggle(){}}};
const staleCode=source.slice(source.indexOf('let lastStateAt=0;'),source.indexOf('function setPanelMode('));
new Function('$','document','setDataStatus',staleCode+';lastStateAt=Date.now()-11000;checkStateAge();')(id=>elements[id],{body},()=>{});
assert.equal(elements.staleBanner.hidden,false);assert(elements.staleBanner.textContent.includes('11 s temu'));assert.equal(elements.btnOpen.disabled,true);assert.equal(elements.btnClose.disabled,true);assert.equal(elements.btnStop.disabled,false);assert.equal(elements.safetyTxt.textContent,'BRAK DANYCH');
let saved,compact;const panels=[{open:false},{open:false}],button={setAttribute(){}};
const main={},sqm={},lower={},content={insertBefore(m,target){assert.equal(m,main);this.target=target;}};
const document={body:{classList:{toggle(name,on){compact=on;},contains(){return compact;}}},querySelectorAll(){return panels;},querySelector(name){return {'.content':content,'.main-grid':main,'.lower-grid':lower}[name];}};
const modeCode=source.slice(source.indexOf('function setPanelMode('),source.indexOf('/* ── Apply state ── */'));
new Function('$','document','localStorage',modeCode+';initPanels();if(!document.body.classList.contains("compact-panels"))throw Error("Default compact");togglePanels();')(id=>id==='btnPanels'?button:sqm,document,{getItem(){return null},setItem(k,v){saved=v;}});
assert(panels.every(p=>p.open));assert.equal(saved,'full');assert.equal(content.target,lower);
assert(source.indexOf('id="btnStop"')<source.indexOf('<details class="panel-details sub-details"'));
console.log('Status WWW PASS: separate RA/DEC, atpark and SafetyMonitor, stale/history/unknown, failed reads, 32 status combinations, stale panel STOP access, reversible full/compact mode');

