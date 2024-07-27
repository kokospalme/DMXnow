/*
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
#include <DMXnow.h>
#include <dmx.h>
#include "mySerialhandler.h"  //include serialhandler stuff
#include "myDevice.h" //include device specific stuff
#include "myArtnet.h"   //include artnet stuff

void setup() {
    mutex = xSemaphoreCreateMutex();  // Create the mutex
    getConfig();    //get config from memory
    Serial.begin(115200);   //start serial communication
    delay(100);
    Serial.print("\nDMX node 1 booting. to enter setup, send serial. Reboot reason: ");Serial.println(esp_reset_reason());


    while(millis() < SERIALINTERFACE_WAIT_MS || setupmode){
        serialInterface();
    }
    Serial.println("exit setup\n");
    delay(100);

    //output
    if(deviceconfig.output == DMXIO_DMXNOW){    //setup dmxnow as output
        DMXnow::init();
        DMXnow::sendSlaveRequest();
        delay(2000);
    }else if(deviceconfig.output == DMXIO_ARTNET){   //setup artnet as output
        setupArtnet(true);
    } else if(deviceconfig.output == DMXIO_USB){    //setup USB as output
        Serial.end();
        DMX::Initialize(PIN_NONE,PIN_NONE,  output);
    }
    
    //input
    if(deviceconfig.input == DMXIO_USB){
        if(deviceconfig.output == DMXIO_USB){
            Serial.println("input can't be output (USB).");
            return;
        }
        Serial.println("start DMX as input.");
        delay(50);
        Serial.end();
        DMX::setCallback(uartCallback);
        DMX::Initialize(PIN_NONE,PIN_NONE,  input);
    }else if(deviceconfig.input == DMXIO_ARTNET){
        if(deviceconfig.output == DMXIO_ARTNET){
            Serial.println("input can't be output (Art-net).");
            return;
        }
        setupArtnet(false);
    }else if(deviceconfig.input == DMXIO_DMXNOW){
        if(deviceconfig.output == DMXIO_DMXNOW){
            Serial.println("input can't be output (DMXnow).");
            return;
        }
        DMXnow::setDmxCallback(dmxnowcallback);
        DMXnow::setSlaveconfig(deviceconfig.dmxNowSlaveConfig);
        DMXnow::initSlave();
    }
    delay(100);
}

void loop() {
    delay(1);
}
