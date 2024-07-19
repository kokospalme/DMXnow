/*
Artnet2DMXnow_master example
myDevice.h

This header file defines the hardware configuration and serial command handling for the project. It specifies the hardware pins and command prefixes used to communicate with slave devices over the serial interface.

Key functionalities include:
- **Hardware Pin Definitions:** Configures pins for the Ethernet connection and other hardware interfaces.
- **Serial Commands:** Defines prefixes and parameters for serial commands used to control the device.

Global variables:
- **MAC Addresses:** Includes test MAC addresses and the broadcast address.

This file is essential for configuring and managing hardware interfaces and serial communications.

Licensed under the Creative Commons Attribution 4.0 International (CC BY 4.0).
To view a copy of this license, visit http://creativecommons.org/licenses/by/4.0/
*/

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

uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t selectedSlave [] = {0,0,0,0,0,0};

#endif

