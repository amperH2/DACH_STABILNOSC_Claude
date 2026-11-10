const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const source=fs.readFileSync(path.join(__dirname,'../src/web_ui.h'),'utf8');
const start=source.indexOf('async function submitManual(event){'),end=source.indexOf('async function localDo(',start);
let calls=[],closed=0,accepted=true;
const nodes={manualDialogError:{textContent:''},manualPassword:{value:'',focus(){}},manualSubmit:{disabled:false},manualCancel:{disabled:false}};
const submit=new Function('$','commandFetch','closeManualDialog','refresh','toast','URLSearchParams','let manualSubmitBusy=false;'+source.slice(start,end)+';return submitManual;')(
 id=>{assert(nodes[id],'unknown DOM '+id);return nodes[id]},async(url,options)=>{calls.push({url,body:new URLSearchParams(options.body)});return {ok:accepted,json:async()=>({ok:accepted,error:accepted?'':'Złe hasło'})}},()=>closed++,async()=>{},()=>{},URLSearchParams);
(async()=>{
 await submit({preventDefault(){}});assert.equal(calls.length,0);assert(nodes.manualDialogError.textContent.includes('hasło'));
 nodes.manualPassword.value='secret';await submit({preventDefault(){}});assert.equal(calls.length,1);assert.equal(closed,1);assert.equal(calls[0].url,'/cmd');assert.equal(calls[0].body.get('do'),'manual_on');assert.equal(calls[0].body.get('manual_pass'),'secret');assert(!calls[0].body.has('minutes'));assert.equal(nodes.manualPassword.value,'');assert(!nodes.manualSubmit.disabled);
 accepted=false;nodes.manualPassword.value='wrong';await submit({preventDefault(){}});assert.equal(closed,1);assert.equal(nodes.manualDialogError.textContent,'Złe hasło');assert.equal(nodes.manualPassword.value,'');assert(!nodes.manualCancel.disabled);
 console.log('MANUAL activation PASS: actual submit handler, empty password, persistent activation without duration, rejection and password cleanup');
})().catch(e=>{console.error(e);process.exitCode=1});
