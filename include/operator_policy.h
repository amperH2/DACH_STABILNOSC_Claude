#pragma once
#include <stdint.h>
namespace astro_policy {
constexpr bool mountBlocksClose(bool clearance,bool explicitlyBlocks,bool mountTrip) {
 return mountTrip && (!clearance || explicitlyBlocks);
}
constexpr bool weatherMeasurementsReady(bool fresh,bool windValid,bool skyValid) {
 return fresh && windValid && skyValid;
}
constexpr bool parseIPv4(const char* text,uint32_t& value) {
 value=0;
 for(unsigned part=0;part<4;++part) {
  if(*text<'0'||*text>'9')return false;
  unsigned number=0,digits=0;
  while(*text>='0'&&*text<='9'){number=number*10+unsigned(*text++-'0');if(++digits>3||number>255)return false;}
  value=(value<<8)|number;
  if(part<3){if(*text++!='.')return false;}else if(*text)return false;
 }
 return true;
}
// Exact IPv4 hosts only. No forwarded headers, subnet wildcards or learning from GET.
constexpr bool trustedIPv4(const char* peer,const char* list) {
 uint32_t source=0;if(!parseIPv4(peer,source)||!source)return false;
 bool matched=false;
 while(*list) {
  char host[16]={};unsigned n=0;
  while(*list && *list!=','){if(n>=15)return false;host[n++]=*list++;}
  uint32_t allowed=0;if(!parseIPv4(host,allowed)||!allowed)return false;
  if(source==allowed)matched=true;
  if(*list==','){++list;if(!*list)return false;}
 }
 return matched;
}
}
