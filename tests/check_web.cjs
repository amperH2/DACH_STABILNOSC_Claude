const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../src/web_ui.h'),'utf8');
const start=source.indexOf('  // Stable AUX IDs:');
const end=source.indexOf('  // Iluminator:',start);
const body=source.slice(start,end);
new Function(source.slice(source.indexOf('<script>')+8,source.indexOf('</script>')));
let elements={},calls=[];
const grid={dataset:{},set innerHTML(v){elements={};for(const m of v.matchAll(/id="aux(\d+)"/g)){const id='aux'+m[1];const state={textContent:''};elements[id]={style:{},querySelector(){return state},state};}}};
const $=id=>id==='auxGrid'?grid:elements[id];
const aux=id=>calls.push(id);
const render=new Function('st','can','$','aux',body);
function draw(st,can){render(st,can,$,aux);}

const st={aux_map:[1,2,3,4,5].map((id,i)=>({id,ch:[7,6,8,4,5][i]})),aux:[false,true,false,false,false],modbus_relay_state_valid:true,modbus_relay_actual:[false,false,false,false,false,true,false,false]};
draw(st,true);assert.equal(Object.keys(elements).length,5);assert.equal(elements.aux2.state.textContent,'ON');elements.aux2.onclick();assert.deepEqual(calls,[2]);
draw({...st,aux_map:st.aux_map.slice(0,3)},true);assert.equal(elements.aux4,undefined);assert.equal(Object.keys(elements).length,3);
draw({...st,modbus_relay_state_valid:false},false);assert.equal(elements.aux2.state.textContent,'UNKNOWN');assert.equal(elements.aux2.onclick,null);
draw({...st,modbus_relay_readback_current:false},true);assert.equal(elements.aux2.state.textContent,'UNKNOWN');assert.equal(typeof elements.aux2.onclick,'function');
console.log('WWW tests passed: five AUX, reserved-channel hiding, CH6 readback, stable ID, offline UNKNOWN');
