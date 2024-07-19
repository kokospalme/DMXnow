/*
Artnet2DMXnow_master example
myArtnet.h

This header file contains definitions and functions for managing Artnet communication in the project. It sets up the Artnet configuration and provides callback functions for handling DMX data and sync messages.

Key functionalities include:
- **initEthernet():** Initializes the Ethernet connection and configures the static IP address.
- **myArtDmxCallback():** Callback function invoked when DMX data is received. It processes the data and updates the DMXnow library.
- **myArtSyncCallback():** Callback function invoked when an Artnet synchronization message is received.

Global variables:
- `artnet_config`: Stores Artnet configuration data.
- `artnet_package`: Holds the received Artnet package data.
- `mutex`: Semaphore for synchronizing access to Artnet data.

This file relies on the Artnet library for DMX communication.
Artnet: https://github.com/kokospalme/DMXnow

Licensed under the Creative Commons Attribution 4.0 International (CC BY 4.0).
To view a copy of this license, visit http://creativecommons.org/licenses/by/4.0/
*/


#ifndef MYARTNET_H
#define MYARTNET_H

#include <Arduino.h>
#include <AsyncUDP_ESP32_SC_Ethernet.h> 
#include <Artnet.h>
#include <DMXnow.h>
#include "myDevice.h"

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

unsigned long artnetStatistic = 0;

void initEthernet();
void myArtDmxCallback(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP);
void myArtSyncCallback(IPAddress remoteIP);


byte ip[] = {10, 0, 1, 199};    // Change ip and mac address for your setup
byte broadcast[] = {10, 0, 1, 255};
byte mac[] = {0x04, 0xE9, 0xE5, 0x00, 0x69, 0xE8};

// Initialisierung der Ethernet-Verbindung
void initEthernet() {
    Serial.println("initialize Ethernet...");

    
    ESP32_Ethernet_onEvent();// start Ethernet-Adapter
    delay(10);
    ETH.setHostname(artnet_config.shortname.c_str());
    ETH.begin(PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_SCK, PIN_W5500_CS, PIN_W5500_INT, SPI_CLOCK_SPEED, ETH_SPI_HST);
    
    // ETH.config(myIP, myGW, mySN, myDNS); // config with static ip
    delay(10);
    ESP32_Ethernet_waitForConnect();

    Serial.print("Ethernet IP: ");  //local ip
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

    artnet_package.universe = universe;
    artnet_package.length = length;
    artnet_package.sequence = sequence;
    artnet_package.newData = true;
    memcpy(artnet_package.data, data, length);
    DMXnow::pushDMXData(artnet_package.universe, artnet_package.length, artnet_package.sequence, artnet_package.data, true);
    
}



void myArtSyncCallback(IPAddress remoteIP) {
    Serial.print("Sync received from: ");
    Serial.println(remoteIP);

    // Hier kannst du Aktionen ausführen, die bei einem Sync benötigt werden
}

#endif