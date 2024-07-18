#include "DMXnow.h"

artnow_slave_t DMXnow::mySlaveData;
void (*DMXnow::setterCallback)(const uint8_t* macAddr, String name, String valueP);

void DMXnow::initSlave(){
   WiFi.mode(WIFI_STA);
    
    Serial.println("Initialisiere DMXnow...");
    esp_now_init();
    esp_now_register_recv_cb(slaveDataReceived);

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Fehler beim Hinzufügen des broadcast-Peers");
        return;
    }
    Serial.println("broadcast-Peer hinzugefügt");

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


void DMXnow::registerPeer(const uint8_t* macAddr){
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result != ESP_OK) {
        Serial.print("error by register peer: ");
        Serial.println(result);
        return;
    } else {
        Serial.println("Peer added.");
    }
}

// Funktion zum Löschen eines Peers
void DMXnow::deletePeer(const uint8_t* macAddr) {

    esp_err_t result = esp_now_del_peer(macAddr);

    if (result != ESP_OK) {
        Serial.print("error by deleting peer: ");
        Serial.println(result);
    } else {
        Serial.println("Peer deleted.");
    }
}

// Funktion zum Senden der Antwort an den Master
void DMXnow::sendSlaveresponse(const uint8_t* macMaster) {
    esp_read_mac(mySlaveData.macAddress, ESP_MAC_WIFI_STA);
    mySlaveData.rssi = WiFi.RSSI();
    mySlaveData.responsecode = SLAVE_CODE_REQUEST;

    // Senden an Master
    Serial.printf("sending to %02X.%02X.%02X.%02X.%02X.%02X... ",macMaster[0],macMaster[1],macMaster[2],macMaster[3],macMaster[4],macMaster[5]);
    // esp_err_t result = esp_now_send(macMaster, (uint8_t *)&mySlaveData,  sizeof(artnow_slave_t));
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&mySlaveData,  sizeof(artnow_slave_t));

    if (result == ESP_OK) {
        Serial.println("Response sent to master");
    } else {
        Serial.print("Error sending response: ");
        Serial.println(result);
    }
}

void DMXnow::slaveDataReceived(const uint8_t* macAddr, const uint8_t* data, int len) {
    // Sicherstellen, dass die Länge der empfangenen Daten korrekt ist
    if (len < sizeof(artnow_packet_t)) {
        Serial.println("Received packet size mismatch");
        return;
    }

    artnow_packet_t* packet = (artnow_packet_t*)data;// Daten in die Struktur artnow_packet_t kopieren
    uint8_t universe = packet->universe;// Nachricht dekodieren

    if(universe >= 0 && universe <=16){
        Serial.println("data...");
        if(packet->part == 0) Serial.println(packet->data[0]);
    }else if(universe == SLAVE_CODE_REQUEST){
        slaveRequest(macAddr, data, len);
    }else if(universe == SLAVE_CODE_SET){
        slaveReceiveSetter(macAddr, data, len);
    }else{
        Serial.println("unknown universe");
    }
    uint8_t sequence = packet->sequence;
    uint8_t part = packet->part;
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


void DMXnow::slaveRequest(const uint8_t* macAddr, const uint8_t* data, int len){
    Serial.printf("slave request from %02X.%02X.%02X.%02X.%02X.%02X... ",macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
    // int _findPeer = findPeerByMac(macAddr);
    
    // if(_findPeer == -1){    //new peer
    //     registerPeer(macAddr);
    //     sendSlaveresponse(macAddr);
    // }else(sendSlaveresponse(macAddr));   //known peer

    registerPeer(macAddr);
    sendSlaveresponse(macAddr);
}

void DMXnow::slaveReceiveSetter(const uint8_t* macAddr, const uint8_t* data, int len) {

    // Serial.printf("setter from %02x:%02x:%02x:%02x:%02x:%02x: ", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

    artnow_packet_t* packet = (artnow_packet_t*)data; // put data to struct

    // decode incoming package
    uint8_t universe = packet->universe;
    uint8_t sequence = packet->sequence;
    uint8_t part = packet->part;

    //data to string
    char charArray[sizeof(packet->data) + 1]; // +1 für das Nullterminierungszeichen
    memcpy(charArray, packet->data, sizeof(packet->data));
    charArray[sizeof(packet->data)] = '\0'; // Nullterminierungszeichen hinzufügen

    String decodedData = String(charArray);

    // Serial.printf("Universe: %d, Sequence: %d, Part: %d, Data: %s ", universe, sequence, part, decodedData.c_str());
    int separator = decodedData.indexOf(":");

    if(separator<= 0){
        // Serial.println("no value found.");
        return;
    }
    String _name = decodedData.substring(0,separator);
    String _value = decodedData.substring(separator+1);

    // Serial.printf("Name: %s, value: %s\n",_name.c_str(), _value.c_str());

    if (setterCallback) (*setterCallback)(macAddr, _name, _value);  //call callbackfunction
    
}

void DMXnow::setSetterCallback(void (*fptr)(const uint8_t* macAddr, String name, String value)) {
    setterCallback = fptr;
}