#ifndef MYARTNET_H
#define MYARTNET_H

#include <Arduino.h>
#include <AsyncUDP_ESP32_SC_Ethernet.h> 
#include <Artnet.h>
#include <DMXnow.h>
#include "myDevice.h"

#define TIME_BEFORE_REBOOT_MS 10000 //time before reboot device when no connection is established
unsigned long ethernetTimer = 0;
bool ethernetActive = true;

artnet_config_s artnet_config;

struct artnet_package_t{
    uint16_t universe = 0;
    uint16_t length = 0;
    uint8_t sequence;
    uint8_t data[512];
    bool newData = false;
};
artnet_package_t artnet_package;
SemaphoreHandle_t mutex = NULL;


void setupArtnet(); //setup artnet
bool initEthernet();
void myArtDmxCallback(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP);
void myArtSyncCallback(IPAddress remoteIP);


byte ip[] = {10, 0, 1, 199};    // Change ip and mac address for your setup
byte broadcast[] = {10, 0, 1, 255};
byte mac[] = {0x04, 0xE9, 0xE5, 0x00, 0x69, 0xEC};


void setupArtnet(){
    artnet_config.inputuniverse[0] = 1;
    artnet_config.longname = "ESP32-C3 Artnet Node";
    artnet_config.shortname = "ESP32C3 Artnode";
    artnet_config.nodereport = "this is an Artnet Node from an ESP32-C3";

    Serial.println("booting ArtnetNode...");
    if(!initEthernet()) return; //initialize ethernet connection, return and don't activate Artnet if there is no connection

    Artnet::setArtDmxCallback(myArtDmxCallback);
    Artnet::setConfig(artnet_config);
    Artnet::setLocalip(ETH.localIP());
    Artnet::setBroadcast(ETH.broadcastIP());
    Artnet::begin(mac, ip);
}

// Initialisierung der Ethernet-Verbindung
bool initEthernet() {
    Serial.println("Initialisiere Ethernet...");

    // Ethernet-Adapter starten
    ESP32_Ethernet_onEvent();
    delay(10);
    ETH.setHostname(artnet_config.shortname.c_str());
    ETH.begin(PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_SCK, PIN_W5500_CS, PIN_W5500_INT, SPI_CLOCK_SPEED, ETH_SPI_HST);
    // Konfiguration der statischen IP-Adresse
    // ETH.config(myIP, myGW, mySN, myDNS);
    delay(10);

    ethernetTimer = millis();

    // while(!ESP32_W5500_eth_connected){}
    while(!ESP32_W5500_eth_connected && millis() <= ethernetTimer + TIME_BEFORE_REBOOT_MS){
        Serial.print(" .");
        delay(500);
    }    //wait
    if(!ESP32_W5500_eth_connected){

        if(ethernetActive){
            Serial.println("no ethernetconnection. Reboot ...\n");
            ESP.restart();    //restart if ethernet should be active, but it's not
        }
        Serial.println("no ethernet connection. Continue anymway");
        return false;
    }
    Serial.println("ethernet OK.");
    return true;
    // Lokale IP-Adresse ausgeben
    Serial.print("Ethernet IP: ");
    Serial.println(ETH.localIP());
}

void myArtDmxCallback(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP) {
    // Serial.print("Universe: ");
    // Serial.print(universe);
    // Serial.print(", Length: ");
    // Serial.print(length);
    // Serial.print(", Sequence: ");
    // Serial.print(sequence);
    // Serial.print(", Remote IP: ");
    // Serial.print(remoteIP);
    // Serial.print(", Data: \t");
    // for (int i = 0; i < 10; i++) {
    //     Serial.print(data[i]);
    //     Serial.print(" ");
    // }
    // Serial.println();
    // if(universe == 1){
        
    // }
    // DMXnow::sendDMXData(universe, length, sequence, data);
    // artnetStatistic = millis() - artnetStatistic;
    // Serial.printf("%u:\n", artnetStatistic);

    artnet_package.universe = universe;
    artnet_package.length = length;
    artnet_package.sequence = sequence;
    artnet_package.newData = true;
    memcpy(artnet_package.data, data, length);
    DMXnow::pushDMXData(artnet_package.universe, artnet_package.length, artnet_package.sequence, artnet_package.data, true);

    // if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    //     // Update artnet_package with received data
    //     artnet_package.universe = universe;
    //     artnet_package.length = length;
    //     artnet_package.sequence = sequence;
    //     artnet_package.newData = true;
    //     memcpy(artnet_package.data, data, length);
    //     DMXnow::pushDMXData(artnet_package.universe, artnet_package.length, artnet_package.sequence, artnet_package.data, true);
    //     xSemaphoreGive(mutex);  //give mutex
    //     delay(10);   //time to breathe
    // }
    
}



void myArtSyncCallback(IPAddress remoteIP) {
    Serial.print("Sync received from: ");
    Serial.println(remoteIP);

    // Hier kannst du Aktionen ausführen, die bei einem Sync benötigt werden
}

#endif