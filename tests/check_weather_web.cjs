const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const s=fs.readFileSync(path.join(__dirname,'../src/web_ui.h'),'utf8');
const a=s.indexOf('let weatherPolicyEdited=false'),b=s.indexOf('/* ── Edytowalne progi ── */',a);assert(a>=0&&b>a);
const elements={};const $=id=>elements[id]||(elements[id]={value:'',checked:false,disabled:false,textContent:'',checkValidity(){return this.value!==''}});
let calls=[],fail=false;const commandFetch=async(url,options)=>{calls.push({url,options});return{ok:!fail,json:async()=>fail?{ok:false,error:'Zamknij dach'}:{ok:true}}};const refresh=async()=>{};
const api=new Function('$','commandFetch','refresh','URLSearchParams',s.slice(a,b)+';return {populateWeatherPolicy,weatherPolicyDirty,discardWeatherPolicy,saveWeatherPolicy};')($,commandFetch,refresh,URLSearchParams);
const policy={version:1,rain:true,wind:true,clouds:true,humidity:false,dew:false,humidity_limit:90,dew_tenths:30,moisture_hold_sec:60,cloud_hold_sec:180,reopen_sec:300,open_ready:false,config_fault:false};
const state={weather_policy:policy,manual_mode:true,roof:{state:'CLOSED',limit_closed:true,limit_open:false}};
(async()=>{
 api.populateWeatherPolicy(state);assert.equal($('wp_dew_margin').value,'3.0');assert.equal($('btnWeatherPolicySave').disabled,false);
 $('wp_wind').checked=false;api.weatherPolicyDirty();api.populateWeatherPolicy(state);assert.equal($('wp_wind').checked,false,'Polling must preserve edits');
 await api.saveWeatherPolicy();assert.equal(calls[0].url,'/api/weather/policy');assert.equal(calls[0].options.method,'POST');
 const body=new URLSearchParams(calls[0].options.body);assert.equal(body.get('wind'),'0');assert.equal(body.get('rain'),'1');assert.equal(body.get('dew_tenths'),'30');assert(!body.has('do'));
 api.populateWeatherPolicy({...state,roof:{state:'OPEN'}});assert.equal($('btnWeatherPolicySave').disabled,true);
 api.populateWeatherPolicy({});assert.equal($('btnWeatherPolicySave').disabled,true);
 api.populateWeatherPolicy(state);fail=true;await api.saveWeatherPolicy();assert($('weatherPolicyMessage').textContent.includes('Zamknij dach'));
 console.log('PASS weather WWW: persisted-state display, explicit five selectors, edits survive polling, authenticated POST transport, disabled outside closed MANUAL, server errors');
})().catch(e=>{console.error(e);process.exitCode=1});
