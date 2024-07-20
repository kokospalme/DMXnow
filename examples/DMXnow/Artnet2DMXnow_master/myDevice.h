#ifndef MYDEVICE_H
#define MYDEVICE_H
#include <Arduino.h>

/*** hardware ***/ 
#define ETH_SPI_HST     SPI2_HOST
#define SPI_CLOCK_SPEED 25
#define PIN_W5500_INT   5  // Interrupt Pin
#define PIN_SPI_MISO    2
#define PIN_SPI_MOSI    7
#define PIN_SPI_SCK     6
#define PIN_W5500_CS    8
#define ESP32_Ethernet_onEvent            ESP32_W5500_onEvent
#define ESP32_Ethernet_waitForConnect     ESP32_W5500_waitForConnect


/*** Serial commands ***/
#define SLAVE_SET_PREFIX "set."
#define SLAVE_GET_PREFIX "get."
#define SLAVE_SETGET_SELECT "slavemac" //select slave
#define SLAVE_SETGET_UNIVERSE "slave.universe" // universe 0...16 [uint8_t]
#define SLAVE_SETGET_DMXCHANNEL "slave.dmxchannel"  // dmx channel 1 ...512 [uint16_t]

#define SLAVE_SETGET_BRIGHTNESS "scene.brightness" // brightness (8bit) [uint8_t]
#define SLAVE_SETGET_BRIGHTNESS16 "scene.brightness16" // brightness (16bit) [uint16_t]
#define SLAVE_SETGET_STROBE "scene.strobe"  // strobevalue in Strobes per second [uint8_t]
#define SLAVE_SETGET_STROBEMODE "scene.strobemode" 
#define SLAVE_SETGET_BRIGHTNESS "scene.brightness" // brightness (8bit) [uint8_t]
#define SLAVE_SETGET_RGB "scene.rgb" // rgb
#define SLAVE_SETGET_RGB "scene.rgb2" // rgb2
#define SLAVE_SETGET_RGB "scene.rgb3" // rgb3
#define SLAVE_SETGET_RGB "scene.rgb4" // rgb4


uint8_t macSlave1[] = {0x60, 0x55, 0xF9, 0x21, 0x9E, 0x14};  //!for testing only
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t selectedSlave [] = {0,0,0,0,0,0};//slave1 60:55:F9:21:9E:14

#endif

