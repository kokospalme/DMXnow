#ifndef MYDEVICE_H
#define MYDEVICE_H
#include <Arduino.h>
#include <preferences.h>

/*** hardware ***/ 
#define ETH_SPI_HST     SPI2_HOST
#define PIN_NONE -1
#define SPI_CLOCK_SPEED 25
#define PIN_W5500_INT   5  // Interrupt Pin
#define PIN_SPI_MISO    2
#define PIN_SPI_MOSI    7
#define PIN_SPI_SCK     6
#define PIN_W5500_CS    8
#define ESP32_Ethernet_onEvent            ESP32_W5500_onEvent
#define ESP32_Ethernet_waitForConnect     ESP32_W5500_waitForConnect

#define DMXIO_USB 0 //inputs &outputs for dmx data
#define DMXIO_ARTNET 1
#define DMXIO_DMXNOW 2
#define DEVICE_NAME "USBnode1"

/*** Serial commands ***/
#define SLAVE_SET_PREFIX "set."
#define SLAVE_GET_PREFIX "get."
#define SLAVE_SETGET_SELECT "slavemac" //select slave
#define SLAVE_SETGET_UNIVERSE "slave.universe" // universe 0...16 [uint8_t]
#define SLAVE_SETGET_DMXCHANNEL "slave.dmxchannel"  // dmx channel 1 ...512 [uint16_t]
#define MASTER_REBOOT "master.reboot"
#define MASTER_SETGET_INPUT "master.input"  // dmx input: 0: Art-net, 1: UART
#define SERIAL_SETUP "setup"
#define SERIAL_EXIT "exit"

#define SLAVE_SETGET_BRIGHTNESS "scene.brightness" // brightness (8bit) [uint8_t]
#define SLAVE_SETGET_BRIGHTNESS16 "scene.brightness16" // brightness (16bit) [uint16_t]
#define SLAVE_SETGET_STROBE "scene.strobe"  // strobevalue in Strobes per second [uint8_t]
#define SLAVE_SETGET_STROBEMODE "scene.strobemode" 
#define SLAVE_SETGET_BRIGHTNESS "scene.brightness" // brightness (8bit) [uint8_t]
#define SLAVE_SETGET_RGB "scene.rgb" // rgb
#define SLAVE_SETGET_RGB "scene.rgb2" // rgb2
#define SLAVE_SETGET_RGB "scene.rgb3" // rgb3
#define SLAVE_SETGET_RGB "scene.rgb4" // rgb4

struct deviceconfig_t{
    uint8_t input = DMXIO_USB;
    uint8_t output = DMXIO_DMXNOW;
    int artnetUniverses[4] = {1,2,3,4};   //universes to listen to, -1= not listening
    artnow_slave_t dmxNowSlaveConfig;
}deviceconfig;

Preferences prefs;  //preferences to save config in
uint8_t macSlave1[] = {0x60, 0x55, 0xF9, 0x21, 0x9E, 0x14};  //!for testing only
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t selectedSlave [] = {0,0,0,0,0,0};//slave1 60:55:F9:21:9E:14
uint8_t dmxData[512];

void getConfig();
void uartCallback(uint8_t * data);
void dmxnowcallback(uint8_t* data);

void uartCallback(uint8_t * data){
    memcpy(dmxData, data + 1, 512);
    DMXnow::pushDMXData(1, 512, 0, dmxData, true);
}

void dmxnowcallback(uint8_t* data){
    memcpy((uint8_t *)dmxData + 0, data, 512);

    switch(deviceconfig.output){
    case DMXIO_ARTNET:
    //ToDo: send over Artnet
    break;
    case DMXIO_USB:
    for(int i = 0; i <512; i++){    //ToDo: edit dmx library starting at 0, not at 1
        DMX::Write(i+1, dmxData[i]);
    }
    break;
    }
}

void getConfig(){
    prefs.begin(DEVICE_NAME, false);
    deviceconfig_t defaultConfig;
    deviceconfig.input = (uint8_t) prefs.getUInt("input", defaultConfig.input);
    deviceconfig.output = (uint8_t) prefs.getUInt("output", defaultConfig.output);
    deviceconfig.artnetUniverses[0] = (uint8_t) prefs.getUInt("artnetUniverses0", defaultConfig.artnetUniverses[0]);
    deviceconfig.artnetUniverses[1] = (uint8_t) prefs.getUInt("artnetUniverses1", defaultConfig.artnetUniverses[1]);
    deviceconfig.artnetUniverses[2] = (uint8_t) prefs.getUInt("artnetUniverses2", defaultConfig.artnetUniverses[2]);
    deviceconfig.artnetUniverses[3] = (uint8_t) prefs.getUInt("artnetUniverses3", defaultConfig.artnetUniverses[3]);
    prefs.end();
}



#endif

