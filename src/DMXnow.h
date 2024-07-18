/*

inspired from: https://github.com/Blinkinlabs/esp-now-dmx
*/
#ifndef DMXNOW_H
#define DMXNOW_H

#include <Arduino.h>
#include <vector>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_log.h>
#include <esp_err.h>
#include "esp_wifi.h"


#define SLAVES_MAX 255
#define SETTTER_NAME_LENGTH 24  //bytes (+1 when sent)
#define SETTER_VALUE_LENGTH 24  //bytes

#define SLAVE_CODE_REQUEST 254  //get slaves' info
#define SLAVE_CODE_SET 253
#define SLAVE_CODE_GET 252

#define LOG_TAG "ESP TX"

#define DMXCHANNEL_PER_PACKET 171  //max. 171

typedef struct {
    uint8_t universe;   // DMX universe for this data, 254 for slave request
    uint8_t sequence;   // Sequence number
    uint8_t part;       // part (0...2)
    uint8_t data[DMXCHANNEL_PER_PACKET];      // Zeiger auf DMX data
} artnow_packet_t;


// typedef struct { //obsolet?
//     // uint8_t universe;   // DMX universe for this data, 254 for slave request
//     uint8_t data[25];
// } DMX_Packet;


typedef struct {
    uint8_t responsecode = 0;   //code for response
    uint8_t macAddress[6];      //slave's mac addres
    uint8_t wifiChannel = 0;    //wifichannel
    int8_t rssi = 0;           //signal strength
    uint8_t universe = 1;       //slave's universe from 0 ...16
    uint16_t dmxChannel = 1;      //slave's sartaddress, starting at 1
    uint16_t dmxCount = 3;      //number of dmxchannels used

} __attribute__((packed)) artnow_slave_t;


class DMXnow {
public:
//master stuff
    static void init();
    static void sendDMXData(uint8_t universe, uint16_t length, uint8_t sequence, uint8_t* data);
    static void onDataSent(const uint8_t* macAddr, esp_now_send_status_t status);

    static void sendSlaveRequest(); //!new
    static void sendSlaveSetter(const uint8_t *macAddr, String name, String value); 

    static void dataReceived(const uint8_t *macAddr, const uint8_t *data, int len); //!new
    
// slave stuff
    static void initSlave();
    static void registerPeer(const uint8_t* macAddr);
    static void deletePeer(const uint8_t* macAddr);
    static void sendSlaveresponse(const uint8_t* macMaster);
    static void slaveDataReceived(const uint8_t* macAddr, const uint8_t* data, int len);

    static void setSetterCallback(void (*fptr)(const uint8_t* macAddr, String name, String value));
private:
    static int findPeerByMac(const uint8_t* macAddr);

    //master stuff
    static uint8_t broadcastAddress[6];
    static uint8_t* dmxData;
    static artnow_packet_t packet;  //packet for sending Data
    static void addSlave(artnow_slave_t entry);
    static void deleteSlave(int index);
    static int findSlaveByMac(const uint8_t* macAddr);

    //slave stuff
    static void slaveRequest(const uint8_t* macAddr, const uint8_t* data, int len);
    static void slaveReceiveSetter(const uint8_t *macAddr, const uint8_t *data, int len);
    static artnow_slave_t mySlaveData;

    static std::vector<artnow_slave_t> slaveArray;  //aray for slaves
    static void (*setterCallback)(const uint8_t* macAddr, String name, String valueP);  //setter callback
};

#endif // DMXNOW_H
