const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const source=fs.readFileSync(path.join(__dirname,'../src/web_ui.h'),'utf8');
const start=source.indexOf('async function commandFetch('),end=source.indexOf('\n}',start)+2;
const calls=[];
const fetch=async(url,options={})=>{calls.push({url,options});return {ok:true,json:async()=>({token:'boot-token'})};};
const transport=new Function('fetch','location','URL',source.slice(start,end)+';return commandFetch;')(fetch,{origin:'http://observatory'},URL);
(async()=>{
 await transport('/cmd?do=manual_on&manual_pass=private&minutes=5');
 assert.equal(calls[0].url,'/api/command-token');
 assert.equal(calls[1].url,'/cmd');assert.equal(calls[1].options.method,'POST');
 assert.equal(calls[1].options.headers['X-Command-Token'],'boot-token');
 assert(calls[1].options.body.includes('manual_pass=private'));
 await transport('/api/wind/calibration',{body:'verified=1'});
 assert.equal(calls.at(-1).url,'/api/wind/calibration');assert.equal(calls.at(-1).options.method,'POST');
 assert.equal(calls.at(-1).options.body,'verified=1');
 await transport('/cmd?do=stop');assert.equal(calls.at(-1).options.body,'do=stop');
 await transport('/api/state');assert.equal(calls.at(-1).url,'/api/state');assert.equal(calls.at(-1).options.method,undefined);
 console.log('Command transport PASS: POST/body/token, password absent from URL, calibration body, STOP and read-only GET');
})().catch(e=>{console.error(e);process.exitCode=1});
