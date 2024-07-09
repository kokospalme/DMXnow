/*

inspired from: https://github.com/Blinkinlabs/esp-now-dmx
*/
#ifndef DMXNOW_H
#define DMXNOW_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_log.h>
#include <esp_err.h>
#include "esp_wifi.h"

#define PEERS_MAX 10
#define SETTTER_NAME_LENGTH 24  //bytes (+1 when sent)
#define SETTER_VALUE_LENGTH 24  //bytes

#define SLAVE_CODE_REQUEST 254  //get slaves' info
#define SLAVE_CODE_SET 253
#define SLAVE_CODE_GET 252
#define SLAVE_SETGET_BRIGHTNESS "scene.brightness"
#define SLAVE_SETGET_STROBE "scene.strobe"
#define SLAVE_SETGET_PLAYMODE "scene.playmode"
#define SLAVE_SETGET_VARIABLE1 "scene.variable1"
#define SLAVE_SETGET_VARIABLE2 "scene.variable2"
#define SLAVE_SETGET_SPEED "scene.speed"
#define SLAVE_SETGET_SCALE "scene.scale"
#define SLAVE_SETGET_RGB "scene.rgb"
#define SLAVE_SETGET_RGB2 "scene.rgb2"
#define SLAVE_SETGET_RGB3 "scene.rgb3"
#define SLAVE_SETGET_RGB4 "scene.rgb4"

#define LOG_TAG "ESP TX"

typedef struct {
    uint8_t universe;   // DMX universe for this data, 254 for slave request
    uint8_t sequence;   // Sequence number
    uint8_t part;       // part (0...2)
    uint8_t data[171];      // Zeiger auf DMX data
} artnow_packet_t;


typedef struct {
    // uint8_t universe;   // DMX universe for this data, 254 for slave request
    uint8_t data[25];
} DMX_Packet;


typedef struct {
    uint8_t responsecode = 0;   //code for response
    uint8_t macAddress[6];      //slave's mac addres
    uint8_t wifiChannel = 0;    //wifichannel
    int8_t rssi = 0;           //signal strength
    uint8_t universe = 1;       //slave's universe from 0 ...16
    uint16_t dmxStart = 1;      //slave's sartaddress, starting at 1
    uint16_t dmxCount = 3;      //number of dmxchannels used

    // uint16_t scene_brightness = 0;    //scene
    // uint8_t scene_strobe = 0;
    // uint8_t scene_playmode = 0;
    // uint8_t scene_variable1 = 0;
    // uint8_t scene_variable2 = 0;
    // uint8_t scene_rgb[4][3] = {{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}}; // 4x RGB
    
} __attribute__((packed)) artnow_slave_t;


class DMXnow {
public:
//master stuff
    static void init();
    static void sendDMXData();  //ToDo: komprimieren
    static void onDataSent(const uint8_t* macAddr, esp_now_send_status_t status);

    static void sendSlaveRequest(); //!new
    static void sendSlaveSetter(const uint8_t *macAddr, String name, String value); 

    static void dataReceived(const uint8_t *macAddr, const uint8_t *data, int len); //!new
    
    
// slave stuff
    static void initSlave();
    static void registerPeer(const uint8_t* macAddr, int peerNo);
    static void sendSlaveresponse(const uint8_t* macMaster);
    static void slaveDataReceived(const uint8_t* macAddr, const uint8_t* data, int len);
private:
    static int findPeerByMac(const uint8_t* macAddr);

    //master stuff
    static uint8_t broadcastAddress[6];
    static uint8_t* dmxData;
    static artnow_packet_t packet;  //packet for sending Data

    //slave stuff
    static void slaveRequest(const uint8_t* macAddr, const uint8_t* data, int len);
    static void slaveReceiveSetter(const uint8_t *macAddr, const uint8_t *data, int len);
    static artnow_slave_t mySlaveData;
    static esp_now_peer_info_t peerInfo;
    static esp_now_peer_info_t peers[PEERS_MAX];
    static uint8_t freePeer;
};

#endif // DMXNOW_H
