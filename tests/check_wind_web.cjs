const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const source=fs.readFileSync(path.join(__dirname,'../src/web_ui.h'),'utf8');
const a=source.indexOf('async function saveWindCalibration(){');
const b=source.indexOf('async function saveCfg(){',a);
const values={gain:'1',offset:'0',ratio:'1',zero:'0.4',top:'2',maxMS:'32.4',model:'Test sensor'};
let checked=false,calls=[],messages=[],refreshCount=0,responseOK=true;
const $=id=>id==='wc_verified'?{checked}:{value:values[id.slice(3)]};
const fetch=async(url,options)=>{calls.push({url,options});return {ok:responseOK,json:async()=>responseOK?{ok:true}:{ok:false,error:'Zamknij dach'}};};
const toast=m=>messages.push(m),refresh=async()=>{refreshCount++};
const save=new Function('$','commandFetch','toast','refresh','URLSearchParams',source.slice(a,b)+'\nreturn saveWindCalibration;')($,fetch,toast,refresh,URLSearchParams);
(async()=>{
 await save();assert.equal(calls[0].options.method,'POST');assert.equal(calls[0].url,'/api/wind/calibration');
 const body=new URLSearchParams(calls[0].options.body);assert.equal(body.get('verified'),'0');assert.equal(body.get('model'),'Test sensor');assert.equal(body.get('zero'),'0.4');
 assert.equal(body.has('wind_gust'),false);assert.equal(body.has('wind_sustained'),false);assert.equal(refreshCount,1);
 checked=true;await save();assert.equal(new URLSearchParams(calls[1].options.body).get('verified'),'1');
 responseOK=false;await save();assert.equal(messages.at(-1),'Zamknij dach');assert.equal(refreshCount,2);
 console.log('Wind WWW tests PASS: POST, safe default, explicit confirmation, independent calibration, server rejection');
})().catch(e=>{console.error(e);process.exitCode=1});
