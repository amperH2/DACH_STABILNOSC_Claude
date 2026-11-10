// Isolated service firmware: never links the roof application.
#include <Arduino.h>
#include "astro_config.h"
#include "modbus_protocol.h"
HardwareSerial bus(2);
static bool isolatedConfirmed=false;
static uint16_t crc16(const uint8_t* data,size_t len){return astro_modbus::crc16(data,len);}
static size_t exchange(uint8_t* req,size_t len,uint8_t* resp,size_t max){
  uint16_t crc=crc16(req,len-2);req[len-2]=crc&255;req[len-1]=crc>>8;
  while(bus.available())bus.read();
  digitalWrite(21,HIGH);delayMicroseconds(50);bus.write(req,len);bus.flush();delayMicroseconds(300);digitalWrite(21,LOW);
  size_t n=0;uint32_t start=millis();
  while(millis()-start<250){while(bus.available()&&n<max)resp[n++]=bus.read();delay(1);}
  Serial.print("RX:");for(size_t i=0;i<n;++i)Serial.printf(" %02X",resp[i]);Serial.println();return n;
}
static bool readAddress(uint8_t addr){
  uint8_t req[8]={addr,3,0x40,0,0,1,0,0},resp[16]={};size_t n=exchange(req,8,resp,16);
  bool ok=n==7&&resp[0]==addr&&resp[1]==3&&resp[2]==2&&crc16(resp,5)==(uint16_t)(resp[5]|(resp[6]<<8));
  if(ok)Serial.printf("CONFIRMED address register=%u\n",(unsigned)((resp[3]<<8)|resp[4]));
  else Serial.println("UNCONFIRMED: no valid address response");
  return ok && ((resp[3]<<8)|resp[4])==addr;
}
static void command(String line){
  line.trim();
  if(line=="confirm-one-device"){isolatedConfirmed=true;Serial.println("Operator confirms only ONE isolated module is connected. Permission is valid for one write.");return;}
  if(line=="scan"){
    isolatedConfirmed=false;
    Serial.println("Read-only scan: addresses 1..16, selected baud. Use ONLY one module to diagnose.");
    for(uint8_t a=1;a<=16;++a){Serial.printf("Address %u\n",a);readAddress(a);}
    return;
  }
  int a=0,b=0;char tail=0;
  if(sscanf(line.c_str(),"read %d %c",&a,&tail)==1&&a>=1&&a<=255){readAddress(a);return;}
  if(sscanf(line.c_str(),"baud %d %c",&a,&tail)==1&&(a==4800||a==9600||a==19200||a==38400||a==57600||a==115200)){
    bus.updateBaudRate(a);isolatedConfirmed=false;Serial.printf("Service UART baud=%d; module configuration NOT changed\n",a);return;
  }
  if(sscanf(line.c_str(),"set %d %d %c",&a,&b,&tail)==2&&a>=1&&a<=255&&b>=1&&b<=255){
    if(!isolatedConfirmed){Serial.println("BLOCKED: first disconnect other modules and send confirm-one-device");return;}
    isolatedConfirmed=false;
    if(!readAddress(a)){Serial.println("BLOCKED: old address not confirmed; no write sent");return;}
    uint8_t req[8]={(uint8_t)a,6,0x40,0,0,(uint8_t)b,0,0},resp[16]={};
    exchange(req,8,resp,16);delay(300);
    Serial.println("Verifying NEW address (a write echo alone is not success):");
    if(readAddress(b))Serial.println("Address change response received. Verify register value and repeat after power cycle.");
    else Serial.println("FAILED/UNKNOWN: new address not confirmed. Do not reconnect both modules yet.");
    return;
  }
  Serial.println("Commands: read 1 | scan | baud 9600 | confirm-one-device | set 1 2");
}
void setup(){
  Serial.begin(115200);
  pinMode(21,OUTPUT);digitalWrite(21,LOW);bus.begin(MODBUS_RELAY_BAUD,SERIAL_8N1,18,17);
  // No GPIO for roof, relay writes, PWM output writes, I2C or network initialization.
}
void loop(){
  static uint32_t heartbeat=0;static String input;
  if(millis()-heartbeat>=5000){heartbeat=millis();Serial.println("RS485 SERVICE ACTIVE - normal roof firmware is not running. Type read 1 or scan.");}
  while(Serial.available()){
    char c=Serial.read();if(c=='\n'||c=='\r'){if(input.length())command(input);input="";}
    else if(input.length()<80)input+=c;
  }
  delay(1);
}
