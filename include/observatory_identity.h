#pragma once
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#define OBSERVATORY_PREFIX "[" OBSERVATORY_SHORT_NAME "]"
// Add once at presentation/transport boundaries. Keep event causes and sources.
inline String observatoryMessage(const String& text) {
  if(text==OBSERVATORY_PREFIX || text.startsWith(OBSERVATORY_PREFIX " ")) return text;
  return String(OBSERVATORY_PREFIX " ")+text;
}
inline void observatoryCopyMessage(char* dest,size_t capacity,const char* text) {
  if(!text) text="";
  const size_t n=strlen(OBSERVATORY_PREFIX);
  const bool prefixed=strncmp(text,OBSERVATORY_PREFIX,n)==0 && (text[n]==' ' || text[n]=='\0');
  snprintf(dest,capacity,prefixed?"%s":"%s %s",prefixed?text:OBSERVATORY_PREFIX,text);
}
