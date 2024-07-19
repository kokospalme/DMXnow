/*
Artnet2DMXnow_master example
main.ino

This sketch demonstrates the use of Artnet protocol in a master device to send DMX data to slave devices. It initializes the Artnet configuration and sets up the device to communicate over Ethernet.

Key functionalities include:
- **setup():** Initializes the DMXnow library and sends a request to the slave devices. Configures Artnet settings and establishes an Ethernet connection.
- **loop():** Continuously calls the `serialInterface()` function to handle incoming serial commands.

This sketch utilizes the DMXnow and Artnet libraries for DMX communication.
DMXnow: https://github.com/kokospalme/DMXnow
Artnet: https://github.com/kokospalme/DMXnow

Licensed under the Creative Commons Attribution 4.0 International (CC BY 4.0).
To view a copy of this license, visit http://creativecommons.org/licenses/by/4.0/
*/
#include <Arduino.h>
#include "mySerialhandler.h"  //include serialhandler stuff
#include "myDevice.h" //include device specific stuff
#include "myArtnet.h"   //include artnet stuff

void setupArtnet(); //setup artnet
void sendDMX();

void setup() {
    mutex = xSemaphoreCreateMutex();  // Create the mutex

    Serial.begin(115200);
    delay(2000);
    Serial.print("\nDMXnow [Master] booting... ");Serial.println(esp_reset_reason());
    DMXnow::init();
    DMXnow::sendSlaveRequest();    
    delay(1000);    //wait...
    setupArtnet();
    delay(1000);
}

void loop() {
    serialInterface();
}

void setupArtnet(){
    artnet_config.inputuniverse[0] = 1;
    artnet_config.longname = "ESP32-C3 Artnet Node";
    artnet_config.shortname = "ESP32C3 Artnode";
    artnet_config.nodereport = "this is an Artnet Node from an ESP32-C3";

    Serial.println("booting ArtnetNode...");
    initEthernet(); //initialize ethernet connection

    Artnet::setConfig(artnet_config);
    Artnet::setArtDmxCallback(myArtDmxCallback);  
    Artnet::begin(mac, ip);
}