#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace astro_http {
constexpr uint32_t kRequestMs=2000, kIdleMs=1000;
constexpr size_t kInputBudget=256, kHeaderMax=4096, kLineMax=1024, kBodyMax=2047;
// Incremental, bounded parser. No socket reads, waits or dynamic allocations.
struct Request {
    char request[kLineMax+1]={}, line[kLineMax+1]={}, auth[512]={}, csrf[129]={}, origin[257]={}, host[257]={}, body[kBodyMax+1]={};
    size_t length=0, headerBytes=0, bodyLength=0, contentLength=0;
    uint32_t started=0, lastByte=0;
    unsigned phase=0; // request line, headers, body, complete, rejected
    bool hasLength=false, expectContinue=false, continueSent=false;
    constexpr void reset(uint32_t now) {
        request[0]=line[0]=auth[0]=csrf[0]=origin[0]=host[0]=body[0]=0;
        length=headerBytes=bodyLength=contentLength=0;
        started=lastByte=now; phase=0;
        hasLength=expectContinue=continueSent=false;
    }
    static constexpr size_t size(const char* text) { size_t n=0; while(text[n]) ++n; return n; }
    static constexpr void copy(char* to,const char* from) { size_t n=0; do { to[n]=from[n]; } while(from[n++]); }
    static constexpr bool equal(const char* a,const char* b) { while(*a && *a==*b) { ++a; ++b; } return *a==*b; }
    static constexpr bool hasSpace(const char* text) { while(*text) if(*text++==' ') return true; return false; }
    static constexpr bool prefix(const char* text,const char* key) {
        while (*key) { char c=*text++; if(c>='A' && c<='Z') c+=32; if(c!=*key++) return false; }
        return true;
    }
    constexpr bool complete() const { return phase==3; }
    constexpr bool failed() const { return phase==4; }
    constexpr bool expired(uint32_t now) const {
        return uint32_t(now-started)>=kRequestMs || uint32_t(now-lastByte)>=kIdleMs;
    }
    constexpr void reject() { phase=4; }
    constexpr void header() {
        while(length && (line[length-1]==' ' || line[length-1]=='\t')) line[--length]=0;
        if (!length) { phase=contentLength ? 2 : 3; return; }
        if (prefix(line,"content-length:")) {
            if(hasLength) { reject(); return; }
            hasLength=true; const char* p=line+15;
            while(*p==' ' || *p=='\t') ++p;
            if(*p<'0' || *p>'9') { reject(); return; }
            size_t number=0;
            while(*p>='0' && *p<='9') {
                number=number*10+unsigned(*p++-'0');
                if(number>kBodyMax) { reject(); return; }
            }
            while(*p==' ' || *p=='\t') ++p;
            if(*p) { reject(); return; }
            contentLength=number;
        } else if(prefix(line,"transfer-encoding:")) {
            reject(); // chunked requests are not supported by Alpaca/UI endpoints
        } else if(prefix(line,"authorization:")) {
            const char* p=line+14; while(*p==' ' || *p=='\t') ++p;
            if(size(p)>=sizeof(auth) || auth[0]) { reject(); return; }
            copy(auth,p);
        } else if(prefix(line,"x-command-token:")) {
            const char* p=line+16; while(*p==' ' || *p=='\t') ++p;
            if(size(p)>=sizeof(csrf) || csrf[0]) { reject(); return; }
            copy(csrf,p);
        } else if(prefix(line,"origin:") || prefix(line,"host:")) {
            const bool isOrigin=prefix(line,"origin:");const char* p=line+(isOrigin?7:5);
            while(*p==' ' || *p=='\t')++p;
            char* target=isOrigin?origin:host;
            if(!*p || size(p)>=257 || target[0]){reject();return;}copy(target,p);
        } else if(prefix(line,"expect:")) {
            const char* p=line+7; while(*p==' ' || *p=='\t') ++p;
            if(size(p)!=12 || !prefix(p,"100-continue")) { reject(); return; }
            expectContinue=true;
        }
    }
    constexpr void feed(char ch,uint32_t now) {
        if(phase>=3) return;
        lastByte=now;
        if(phase==2) {
            if(ch==0) { reject(); return; }
            body[bodyLength++]=ch; body[bodyLength]=0;
            if(bodyLength==contentLength) phase=3;
            return;
        }
        if(++headerBytes>kHeaderMax || ch==0) { reject(); return; }
        if(ch=='\r') return;
        if(ch=='\n') {
            line[length]=0;
            if(phase==0) {
                if(!length || !hasSpace(line)) { reject(); return; }
                copy(request,line); phase=1;
            } else header();
            length=0; line[0]=0;
        } else {
            if(length==kLineMax) { reject(); return; }
            line[length++]=ch;
        }
    }
};
}
