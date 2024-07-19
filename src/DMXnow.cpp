#include "DMXnow.h"

uint8_t DMXnow::broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
artnow_packet_t DMXnow::packet;
std::vector<artnow_slave_t> DMXnow::slaveArray;
SendQueueElem DMXnow::sendQueue[SEND_QUEUE_SIZE];
uint8_t DMXnow::dmxBuf[DMX_UNIVERSES][512];
uint8_t DMXnow::dmxPrevBuf[DMX_UNIVERSES][512];
SemaphoreHandle_t DMXnow::dmxMutex = NULL;

void DMXnow::init() {
    dmxMutex = xSemaphoreCreateMutex();  // Create the mutex
    WiFi.mode(WIFI_STA);
    Serial.println("Initialisiere DMXnow...");
    esp_now_init();
    
    
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Fehler beim Hinzufügen des broadcast-Peers");
        return;
    }
    Serial.println("broadcast-Peer hinzugefügt");
    esp_now_register_recv_cb(ma_dataReceived);
    esp_now_register_send_cb(ma_dataSent);

    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        Serial.printf("My mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    baseMac[0], baseMac[1], baseMac[2],
                    baseMac[3], baseMac[4], baseMac[5]);
    } else {
        Serial.println("Failed to read MAC address");
    }
}

void DMXnow::pushDMXData(uint8_t universe, uint16_t length, uint8_t sequence, uint8_t* data, bool send) { //ToDO: daten komprimieren
    if(length < 512){
        Serial.println("only full universes for now. party not fully implemented");
        return;
    }
    if(universe < 1 || universe > 4){
        Serial.println("only universe 1...4 are implemented for now.");
        return;
    }


    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {    //put DMX data to buffer
        memcpy(dmxPrevBuf[universe-1], dmxBuf[universe-1], 512);// Copy the current frame to the previous frame data
        memcpy(dmxBuf[universe-1], data, 512);// Copy the new data to the current frame
        xSemaphoreGive(dmxMutex);
        delay(2);
    }
    if(send)sendQueueElement(universe, false, sequence);    //send if true


}

// from Github
void DMXnow::sendQueueElement(uint8_t universe, bool compressed, uint8_t sequence){
    Serial.print("send...");
   
    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
    // uncompressed, raw frame
        // First part
        uint8_t _keyframe = KEYYFRAME_CODE_UNCOMPRESSED;
        for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
            if (!sendQueue[i].toBeSent) {
            // This element is free to be filled
            sendQueue[i].toBeSent = 1;
            sendQueue[i].size = 173;
            sendQueue[i].data[0] = _keyframe; // uncompressed keyframe, part 1/3
            sendQueue[i].data[1] = universe;
            // sendQueue[i].data[2] = sequence; //todo: schauen ob sequence noch rein passt
            memcpy(sendQueue[i].data + SEND_QUEUE_OVERHEAD, dmxBuf[universe], 171);
            break;
            }
        }
        // Second part
        for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
            if (!sendQueue[i].toBeSent) {
            // This element is free to be filled
            sendQueue[i].toBeSent = 1;
            sendQueue[i].size = 173;
            sendQueue[i].data[0] = _keyframe + 1; // uncompressed keyframe, part 2/3
            sendQueue[i].data[1] = universe;
            // sendQueue[i].data[2] = sequence;
            memcpy(sendQueue[i].data + SEND_QUEUE_OVERHEAD, dmxBuf[universe] + 171, 171);
            break;
            }
        }
        // Third part
        for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
            if (!sendQueue[i].toBeSent) {
            // This element is free to be filled
            sendQueue[i].toBeSent = 1;
            sendQueue[i].size = 172;
            sendQueue[i].data[0] = _keyframe +SEND_QUEUE_OVERHEAD; // uncompressed keyframe, part 3/3
            sendQueue[i].data[1] = universe;
            // sendQueue[i].data[2] = sequence;
            memcpy(sendQueue[i].data + SEND_QUEUE_OVERHEAD, dmxBuf[universe] + 342, 170);
            break;
            }
        }

        xSemaphoreGive(dmxMutex);  //give mutex
        delay(2);   //time to breathe
    }

}

// from github
void DMXnow::processNextSend() {
    // Iterate through the sendQueue and trigger the first match
    for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
        if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
            if (sendQueue[i].toBeSent) {
            esp_now_send(broadcastAddress, sendQueue[i].data, sendQueue[i].size);
            Serial.println("sent.");
            // Zero that element so it won't be sent again
            memset(&(sendQueue[i]), 0, sizeof(SendQueueElem));
            xSemaphoreGive(dmxMutex);  //give mutex
            delay(1);
            return; // Stop here. Next packet send will be triggered in the send-callback-function
            }
        }
    }
    // If control flow reaches here, the send queue has been emptied
    xSemaphoreGive(dmxMutex);  //give mutex
    delay(1);
    //   memset((void*)line5.c_str(), 0, 25);
    //   sprintf((char*)line4.c_str(), "QUEUE EMPTY");
}


/*
send slave request
slaves are answering their info
*/
void DMXnow::sendSlaveRequest() {
    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        if (!sendQueue[SEND_QUEUE_SIZE].toBeSent) {
            // This element is free to be filled
            sendQueue[SEND_QUEUE_SIZE].toBeSent = 1;
            sendQueue[SEND_QUEUE_SIZE].size = SEND_QUEUE_OVERHEAD;
            sendQueue[SEND_QUEUE_SIZE].data[0] = KEYYFRAME_CODE_UNCOMPRESSED; // uncompressed keyframe
            sendQueue[SEND_QUEUE_SIZE].data[1] = SLAVE_CODE_REQUEST;
        }else{
            Serial.println("queue is busy");
        }
        xSemaphoreGive(dmxMutex);  //give mutex
        delay(1); 
    }
}

void DMXnow::sendSlaveSetter(const uint8_t *macAddr, String name, String value) {
    int _slave = findSlaveByMac(macAddr);
    if(_slave < 0 ){  //return if slave is unknown
        Serial.println("no slave found.");
        return;
    }

    registerPeer(macAddr);

    artnow_packet_t _packet;
    _packet.universe = SLAVE_CODE_SET;
    _packet.sequence = 0;
    _packet.part = 0;

    if(name.length() > SETTTER_NAME_LENGTH){
        Serial.println("settername to long");
        return;
    }
    if(value.length() > SETTER_VALUE_LENGTH){
        Serial.println("value to long");
        return;
    }

    //buffer
    String bufString = name;
    bufString+= ":";
    bufString+=value;

    char charArray[bufString.length() + 1]; // +1 für das Nullterminierungszeichen
    bufString.toCharArray(charArray, sizeof(charArray));

    memcpy(_packet.data, charArray, sizeof(_packet.data));    //char array to buffer

    // output
    esp_err_t result = esp_now_send(macAddr, (uint8_t *)&_packet,  sizeof(_packet));

    if (result == ESP_OK) {
        Serial.println("sent setter to slave");
    } else {
        Serial.print("Error sending setter. ");
        Serial.println(result);
    }

    deletePeer(macAddr);
}


/*
receive Data from slave
*/
void DMXnow::ma_dataReceived(const uint8_t *macAddr, const uint8_t *data, int len){
    // Sicherstellen, dass die Länge der empfangenen Daten korrekt ist
    Serial.println("packet received...");

    if (len < sizeof(artnow_slave_t)) {
        Serial.println("Received packet size mismatch");
        return;
    }
    artnow_slave_t* packet = (artnow_slave_t*)data;  // put data to slave packet
    
    if(packet->responsecode == SLAVE_CODE_REQUEST){ //answer to slave request
        Serial.printf("***** slave(%02X:%02X:%02X:%02X:%02X:%02X) *****\n",packet->macAddress[0],packet->macAddress[1],packet->macAddress[2],packet->macAddress[3],packet->macAddress[4],packet->macAddress[5]);
        // Serial.printf("responsecode: %u\n",packet->responsecode);
        // Serial.printf("universe: %u\n",packet->universe);
        // Serial.printf("dmxStart: %u \n",packet->dmxChannel);
        // Serial.printf("dmxCount: %u\n",packet->dmxCount);
        // Serial.printf("MAC:%02X:%02X:%02X:%02X:%02X:%02X\n",packet->macAddress[0],packet->macAddress[1],packet->macAddress[2],packet->macAddress[3], packet->macAddress[4],packet->macAddress[5]);
        // Serial.printf("rssi: %i dB\n",packet->rssi);
        
        int _slave = findSlaveByMac(packet->macAddress);
        if( _slave == -1){   //unknown slave
            addSlave(*packet);
            Serial.println("slave added.");
        }else{  //known slave
            slaveArray[_slave] = *packet;   //overwrite slave
            Serial.println("known slave overwritten.");
        }
        //ToDo: callback slave response
        Serial.println("");
    }else if(packet->responsecode == SLAVE_CODE_SET){
        //got setter
    }else if(packet->responsecode == SLAVE_CODE_GET){
        //got getter
    }else{
        Serial.println("unknown responsecode.\n");
        return;
    }
}

void DMXnow::ma_dataSent(const uint8_t* mac, esp_now_send_status_t sendStatus) {
  switch (sendStatus)
  {
    case ESP_NOW_SEND_SUCCESS:
      // Send the next packet
    //   memset((void*)line4.c_str(), 0, 25);
    //   sprintf((char*)line4.c_str(), "SEND_SUCCESS");
      processNextSend();
      break;

    case ESP_NOW_SEND_FAIL:
      // Empty the sendQueue
    //   memset((void*)line4.c_str(), 0, 25);
    //   sprintf((char*)line4.c_str(), "SEND_FAIL");
      memset(sendQueue, 0, SEND_QUEUE_SIZE*sizeof(SendQueueElem));
      break;

    default:
      break;
  }
}


void DMXnow::addSlave(artnow_slave_t entry) {
    slaveArray.push_back(entry);
}

void DMXnow::deleteSlave(int index) {
    if (index >= 0 && index < slaveArray.size()) {
        slaveArray.erase(slaveArray.begin() + index);
    }
}

int DMXnow::findSlaveByMac(const uint8_t* macAddr) {
    int _arraysize = (int) slaveArray.size();
    Serial.printf("size:%i\n",_arraysize);
    for (int i = 0; i < _arraysize; i++) {
        if (memcmp(slaveArray[i].macAddress, macAddr, 6) == 0) {
            // Serial.printf("peer gefunden: %i\n", i);
            return i; // Peer gefunden
        }
    }
    return -1;
}