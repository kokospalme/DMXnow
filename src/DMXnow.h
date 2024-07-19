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


#define DMX_UNIVERSES 4// Number of supported DMX universes
#define SEND_QUEUE_SIZE 4*DMX_UNIVERSES + 1// How many entries our send-queue will be able to hold
#define SEND_QUEUE_OVERHEAD 2   //bytes overhead

#define SLAVES_MAX 255
#define SETTTER_NAME_LENGTH 24  //bytes (+1 when sent)
#define SETTER_VALUE_LENGTH 24  //bytes

#define SLAVE_CODE_REQUEST 254  //get slaves' info
#define SLAVE_CODE_SET 253
#define SLAVE_CODE_GET 252

#define KEYYFRAME_CODE_UNCOMPRESSED 0x11


#define LOG_TAG "ESP TX"

#define DMXCHANNEL_PER_PACKET 171  //max. 171

struct SendQueueElem {
  uint8_t toBeSent = 0;
  uint8_t size = 0;
  uint8_t data[250];
  uint8_t macAddr[6] = {0,0,0,0,0,0};
};

typedef struct {
    uint8_t universe;   // DMX universe for this data, 254 for slave request
    uint8_t sequence;   // Sequence number
    uint8_t part;       // part (0...2)
    uint8_t data[DMXCHANNEL_PER_PACKET];      // Zeiger auf DMX data
} artnow_packet_t;


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
    static void pushDMXData(uint8_t universe, uint16_t length, uint8_t sequence, uint8_t* data, bool send);
    static void onDataSent(const uint8_t* macAddr, esp_now_send_status_t status);

    static void sendSlaveRequest();
    static void sendSlaveSetter(const uint8_t *macAddr, String name, String value); 

    
    
// slave stuff
    static void initSlave();
    static void setSlaveconfig(artnow_slave_t config);
    static void registerPeer(const uint8_t* macAddr);
    static void deletePeer(const uint8_t* macAddr);
    static void sl_dataReceived(const uint8_t* macAddr, const uint8_t* data, int len);

    static void setSetterCallback(void (*fptr)(const uint8_t* macAddr, String name, String value));
    static void setDmxCallback(void (*fptr)(uint8_t* data));
private:
    static int findPeerByMac(const uint8_t* macAddr);

    //master stuff
    static uint8_t broadcastAddress[6];
    // static uint8_t* dmxData; //ToDo obsolet??
    static artnow_packet_t packet;  //ToDo: obsolet?? packet for sending Data

    static void addSlave(artnow_slave_t entry); //add slave
    static void addBroadastpeer();
    static void deleteSlave(int index); //remote slave from list
    static int findSlaveByMac(const uint8_t* macAddr);  //searching slaves list
    static void sendQueueElement(uint8_t universe, bool compressed, uint8_t sequence);  //put element in queue (dmx data or setter/getter etc.)
    static void processNextSend();  //process sending a message
    static void ma_dataReceived(const uint8_t *macAddr, const uint8_t *data, int len); //callback when msg is received
    static void ma_dataSent(const uint8_t* mac, esp_now_send_status_t sendStatus);   //calback when message is sent

    //slave stuff
    static void sl_responseRequest(const uint8_t* macMaster);
    static void sl_responseSetter(const uint8_t *macAddr, const uint8_t *data, int len);
    static artnow_slave_t mySlaveData;

    static std::vector<artnow_slave_t> slaveArray;  //aray for slaves
    static void (*setterCallback)(const uint8_t* macAddr, String name, String valueP);  //setter callback
    static void (*dmxCallback)(uint8_t* data);  //dmx callback

    //div. stuff
    static SendQueueElem sendQueue[SEND_QUEUE_SIZE];
    static uint8_t dmxBuf[DMX_UNIVERSES][512];// Stores the latest values of all universes
    static uint8_t dmxPrevBuf[DMX_UNIVERSES][512];// Stores the previous values of all universes for diff calculation
    static SemaphoreHandle_t dmxMutex;
};

#endif // DMXNOW_H
