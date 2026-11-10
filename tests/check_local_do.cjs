const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../src/web_ui.h'),'utf8').replace(/\r\n/g,'\n');
new Function(source.slice(source.indexOf('<script>')+8,source.indexOf('</script>')));
const body=source.slice(source.indexOf('  // Local DO rendering;'),source.indexOf('  // Iluminator:'));
let nodes={},calls=[];
class Element{
 constructor(){this.dataset={};this.children=[];this.style={};this.hidden=false;}
 set id(v){this._id=v;nodes[v]=this;}get id(){return this._id;}
 append(...children){this.children.push(...children);}
 replaceChildren(){this.children=[];for(const id of Object.keys(nodes))if(id.startsWith('localDo')&&!['localDoCard','localDoGrid'].includes(id))delete nodes[id];}
 querySelector(selector){return this.children.find(c=>c.className===selector.slice(1));}
}
nodes.localDoCard=new Element();nodes.localDoGrid=new Element();
const document={createElement:()=>new Element()},$=id=>nodes[id];
const run=new Function('st','can','$','document','localDo',body);
const channels=Array.from({length:7},(_,i)=>({id:i+1,name:i===0?'<img onerror=alert(1)>':'Nazwa '+(i+1),enabled:true,on:false}));
const state={local_do:{supported:true,online:true,channels}};
run(state,true,$,document,(id,on)=>calls.push([id,on]));
assert.equal(nodes.localDoGrid.children.length,7);assert.equal(nodes.localDoCard.hidden,false);
assert.equal(nodes.localDo1.querySelector('.an').textContent,'DO1 · <img onerror=alert(1)>');
nodes.localDo7.onclick();assert.deepEqual(calls,[[7,true]]);
channels[2].enabled=false;channels[3].on=true;
run(state,true,$,document,()=>{});assert.equal(nodes.localDo3,undefined);assert.equal(nodes.localDo4.querySelector('.av').textContent,'ON');
state.local_do.online=false;run(state,true,$,document,()=>{});
assert.equal(nodes.localDo4.querySelector('.av').textContent,'UNKNOWN');assert.equal(nodes.localDo4.disabled,true);assert.equal(nodes.localDo4.onclick,null);
state.local_do.online=true;run(state,false,$,document,()=>{});assert.equal(nodes.localDo1.onclick,null);
state.local_do.supported=false;run(state,true,$,document,()=>{});assert.equal(nodes.localDoCard.hidden,true);
const match=source.match(/async function localDo\(id,on\)\{([\s\S]*?)\n\}\nasync function aux/);assert(match);
const send=new Function('commandFetch','refresh','toast','return async function(id,on){'+match[1]+'}');
(async()=>{
 let url,refreshed=0,error='';
 const action=send(async value=>{url=value;return {ok:true,json:async()=>({ok:true})};},async()=>refreshed++,msg=>error=msg);
 await action(7,true);assert.equal(url,'/cmd?do=localdo&n=7&st=1&ajax=1');assert.equal(refreshed,1);assert.equal(error,'');
 const denied=send(async()=>({ok:false,status:409,json:async()=>({ok:false,error:'disabled'})}),async()=>{},msg=>error=msg);
 await denied(8,true);assert(error.includes('disabled'));
 console.log('Local DO tests PASS: seven/stable channels, text-only names, readback states, offline/auth disable, spare hiding, request URL and server rejection. Simulated DOM, no device.');
})().catch(e=>{console.error(e);process.exitCode=1;});
