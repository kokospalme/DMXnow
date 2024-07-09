#include "DMXnow.h"

artnow_slave_t DMXnow::mySlaveData;

void DMXnow::initSlave(){
   WiFi.mode(WIFI_STA);
    
    Serial.println("Initialisiere DMXnow...");
    esp_now_init();
    esp_now_register_recv_cb(dataReceived);

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

/*
register peer to list
*/
void DMXnow::registerMaster(const uint8_t* macAddr, int peerNo){
    memcpy(peerInfo.peer_addr, macAddr, 6);
    // Serial.printf("register peer: %02X.%02X.%02X.%02X.%02X.%02X... ",peerInfo.peer_addr[0],peerInfo.peer_addr[1],peerInfo.peer_addr[2],peerInfo.peer_addr[3],peerInfo.peer_addr[4],peerInfo.peer_addr[5]);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    if(peerNo >= PEERS_MAX){
        Serial.println("too many peers!");
        return;
    }
    esp_err_t result = esp_now_add_peer(&peerInfo);

    if (result != ESP_OK) {
        Serial.print("error by adding peer: ");
        Serial.println(result);
        return;
    } else {
        Serial.println("Peer added.");
        peers[peerNo] = peerInfo;
        freePeer++;
        // Serial.println("\nList:");
        // for (int i = 0; i < PEERS_MAX; i++) {
        //     Serial.printf("peer%u: %02X.%02X.%02X.%02X.%02X.%02X... \n",i,peers[i].peer_addr[0],peers[i].peer_addr[1],peers[i].peer_addr[2],peers[i].peer_addr[3],peers[i].peer_addr[4],peers[i].peer_addr[5]);
        // }
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
        //valid universe...
    }else if(universe == SLAVE_CODE_REQUEST){
        slaveRequest(macAddr, data, len);
        
    }else if(universe == SLAVE_CODE_SET){
    
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
    int _findPeer = findPeerByMac(macAddr);
    if(_findPeer == -1){
        registerMaster(macAddr,freePeer);    //new peer
        sendSlaveresponse(macAddr);
    }else(sendSlaveresponse(macAddr));   //known peer
}

void DMXnow::slaveReceiveSetter(const uint8_t *macAddr, const uint8_t *data, int len) {
    artnow_packet_t *packet = (artnow_packet_t *) data;
    // Daten extrahieren
    char *data = (char *) packet->data;
    
    // Variablen extrahieren
    String receivedData(data);
    int separatorIndex = receivedData.indexOf(',');
    String variable = receivedData.substring(0, separatorIndex);
    String value = receivedData.substring(separatorIndex + 1);
    
    // Hier können Aktionen basierend auf der Variable und dem Wert durchgeführt werden
    Serial.printf("Set %s to %s\n", variable.c_str(), value.c_str());
}