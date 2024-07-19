#include "DMXnow.h"

artnow_slave_t DMXnow::mySlaveData;
void (*DMXnow::setterCallback)(const uint8_t* macAddr, String name, String valueP);

void DMXnow::initSlave(){
    dmxMutex = xSemaphoreCreateMutex();  // Create the mutex
    WiFi.mode(WIFI_STA);

    Serial.println("Initialisiere DMXnow...");
    esp_now_init();
    

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  // Der Kanal kann auf 0 gesetzt werden, um den aktuellen Kanal zu verwenden
    peerInfo.encrypt = false;

    // Überprüfen, ob der Peer bereits hinzugefügt wurde
    if (!esp_now_is_peer_exist(broadcastAddress)) {
        esp_err_t addStatus = esp_now_add_peer(&peerInfo);
        if (addStatus != ESP_OK) {
            Serial.print("Error adding broadcast peer: ");
            Serial.println(addStatus);
            return;
        }
    }
    
    esp_now_register_recv_cb(sl_dataReceived);
    
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        Serial.printf("My mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    baseMac[0], baseMac[1], baseMac[2],
                    baseMac[3], baseMac[4], baseMac[5]);
    } else {
        Serial.println("Failed to read MAC address");
    }

    Serial.println("");
}


void DMXnow::registerPeer(const uint8_t* macAddr){
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = 0;  // Der Kanal kann auf 0 gesetzt werden, um den aktuellen Kanal zu verwenden
    peerInfo.encrypt = false;

    // Überprüfen, ob der Peer bereits hinzugefügt wurde
    if (!esp_now_is_peer_exist(macAddr)) {
        esp_err_t addStatus = esp_now_add_peer(&peerInfo);
        if (addStatus != ESP_OK) {
            Serial.print("Error adding peer: ");
            Serial.println(addStatus);
            return;
        }
    }
}

// Funktion zum Löschen eines Peers
void DMXnow::deletePeer(const uint8_t* macAddr) {

    esp_err_t result = esp_now_del_peer(macAddr);

    if (result != ESP_OK) {
        Serial.print("error by deleting peer: ");
        Serial.println(result);
    } else {
        // Serial.println("Peer deleted.");
    }
}

// Funktion zum Senden der Antwort an den Master
void DMXnow::sl_sendResponse(const uint8_t* macMaster) {
    esp_read_mac(mySlaveData.macAddress, ESP_MAC_WIFI_STA);
    mySlaveData.rssi = WiFi.RSSI();
    mySlaveData.responsecode = SLAVE_CODE_REQUEST;

    // Senden an Master
    Serial.printf("sending to broadcast");
    // esp_err_t result = esp_now_send(macMaster, (uint8_t *)&mySlaveData,  sizeof(artnow_slave_t));
    uint8_t _data[4] = {1,2,3,4};
    esp_err_t result = esp_now_send(broadcastAddress, _data,  sizeof(_data));

    if (result == ESP_OK) {
        Serial.println("Response sent to master");
    } else {
        Serial.print("Error sending response: ");
        Serial.println(result);
    }
}

void DMXnow::sl_dataReceived(const uint8_t* macAddr, const uint8_t* data, int len) {
    switch (data[0]) {
    case KEYYFRAME_CODE_UNCOMPRESSED: // KeyFrame uncompressed 1/3
        if(data[1] == SLAVE_CODE_REQUEST){  //slave request
            Serial.println("got slave Request!");
            sl_responseRequest(macAddr, data, len);
        }else if(data[1] == SLAVE_CODE_SET){    //setter
            Serial.println("got setter");
            sl_responseSetter(macAddr, data, len);
        }else if(data[1] == SLAVE_CODE_GET){    //getter
            Serial.println("got getter");
            //ToDo: implementing getter
        }else if (data[1] == 0) {
        memcpy(dmxBuf[0], data + 2, len - 2);   //dmx data...
        }
        break;
    case KEYYFRAME_CODE_UNCOMPRESSED + 1: // KeyFrame uncompressed 2/3
        if (data[1] == 0) {
        memcpy(dmxBuf[0] + 171, data + 2, len - 2);
        }
        break;
    case KEYYFRAME_CODE_UNCOMPRESSED + 2: // KeyFrame uncompressed 3/3
        if (data[1] == 0) {
        memcpy(dmxBuf[0] + 343, data + 2, len - 2);
        }
        break;
    // case 0x14: // KeyFrame compressed 1/1    //ToDo: compressing
    //     memset(dmxCompBuf, 0, 600);
    //     memcpy(dmxCompBuf, data + 2, len - 2);
    //     dmxCompSize = len - 2;
    //     unCompressDmxBuf(data[1]);
    //     break;
    // case 0x15: // KeyFrame compressed 1/2
    //     memset(dmxCompBuf, 0, 600);
    //     memcpy(dmxCompBuf, data + 2, len - 2);
    //     dmxCompSize = len - 2;
    //     break;
    // case 0x16: // KeyFrame compressed 2/2
    //     memcpy(dmxCompBuf + 230, data + 2, len - 2);
    //     dmxCompSize += len - 2;
    //     unCompressDmxBuf(data[1]);
    //     break;
    //     case 0x17: // KeyFrame compressed 1/3
    //     memset(dmxCompBuf, 0, 600);
    //     memcpy(dmxCompBuf, data + 2, len - 2);
    //     dmxCompSize = len - 2;
    //     break;
    // case 0x18: // KeyFrame compressed 2/3
    //     memcpy(dmxCompBuf + 230, data + 2, len - 2);
    //     dmxCompSize += len - 2;
    //     break;
    // case 0x19: // KeyFrame compressed 3/3
    //     memcpy(dmxCompBuf + 460, data + 2, len - 2);
    //     dmxCompSize += len - 2;
    //     unCompressDmxBuf(data[1]);
    //     break;
    }




    // artnow_packet_t* packet = (artnow_packet_t*)data;// Daten in die Struktur artnow_packet_t kopieren
    // uint8_t universe = packet->universe;// Nachricht dekodieren

    // if(universe >= 0 && universe <=16){
    //     Serial.println("data...");
    //     if(packet->part == 0) Serial.println(packet->data[0]);
    // }else if(universe == SLAVE_CODE_REQUEST){
    //     slaveRequest(macAddr, data, len);
    // }else if(universe == SLAVE_CODE_SET){
    //     slaveReceiveSetter(macAddr, data, len);
    // }else{
    //     Serial.println("unknown universe");
    // }
    // uint8_t sequence = packet->sequence;
    // uint8_t part = packet->part;
    // Je nach Bedarf können hier weitere Aktionen hinzugefügt werden, z.B. DMX-Daten verarbeiten

    // // Beispiel: DMX-Daten ausgeben
    // Serial.print("Received DMX data for universe ");
    // Serial.println(universe);
    // for (int i = 0; i < sizeof(packet->data); i++) {
    //     Serial.print(packet->data[i]);
    //     Serial.print(" ");
    // }
    // Serial.println();
}


void DMXnow::sl_responseRequest(const uint8_t* macAddr, const uint8_t* data, int len){
    Serial.printf("slave request from %02X.%02X.%02X.%02X.%02X.%02X... ",macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
    // int _findPeer = findPeerByMac(macAddr);
    
    // if(_findPeer == -1){    //new peer   //ToDo: obsolet??
    //     registerPeer(macAddr);
    //     sendSlaveresponse(macAddr);
    // }else(sendSlaveresponse(macAddr));   //known peer

    registerPeer(macAddr);
    sl_sendResponse(macAddr);
}

void DMXnow::sl_responseSetter(const uint8_t* macAddr, const uint8_t* data, int len) {

    // Serial.printf("setter from %02x:%02x:%02x:%02x:%02x:%02x: ", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

    //data to string
    char charArray[sizeof(data) - SEND_QUEUE_OVERHEAD + 1]; // +1 für das Nullterminierungszeichen
    memcpy(charArray, data + SEND_QUEUE_OVERHEAD, sizeof(charArray));
    charArray[sizeof(data) - SEND_QUEUE_OVERHEAD + 1] = '\0'; // Nullterminierungszeichen hinzufügen

    String decodedData = String(charArray);

    // Serial.printf("Universe: %d, Sequence: %d, Part: %d, Data: %s ", universe, sequence, part, decodedData.c_str());
    int separator = decodedData.indexOf(":");

    if(separator<= 0){
        // Serial.println("no value found.");
        return;
    }
    String _name = decodedData.substring(0,separator);
    String _value = decodedData.substring(separator+1);

    Serial.printf("Name: %s, value: %s\n",_name.c_str(), _value.c_str());

    if (setterCallback) (*setterCallback)(macAddr, _name, _value);  //call callbackfunction
    
}

void DMXnow::setSetterCallback(void (*fptr)(const uint8_t* macAddr, String name, String value)) {
    setterCallback = fptr;
}