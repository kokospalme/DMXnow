/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/get-change-esp32-esp8266-mac-address-arduino/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.  
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

#include "WiFi.h"
 

uint8_t broadcastAddressMaster[] = {0x64, 0xE8, 0x33, 0xD7, 0xB2, 0x18};  //master
uint8_t broadcastAddressSlave1[] = {0x64, 0xE8, 0x33, 0xD7, 0x5B, 0x5C};  //slave1
uint8_t broadcastAddressSlave1[] = {0x60, 0x55, 0xF9, 0x21, 0x9E, 0x14};  //slave2

void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA);
  
}
 
void loop(){
  Serial.println(WiFi.macAddress());
  delay(2000);
}