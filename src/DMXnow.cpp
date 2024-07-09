#include "DMXnow.h"

uint8_t DMXnow::broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
artnow_packet_t DMXnow::packet;
esp_now_peer_info_t DMXnow::peerInfo;
esp_now_peer_info_t DMXnow::peers[PEERS_MAX];
uint8_t DMXnow::freePeer = 0;

void DMXnow::init() {
    WiFi.mode(WIFI_STA);
    Serial.println("Initialisiere DMXnow...");

    
    esp_now_init();
    
    
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Fehler beim Hinzufügen des Peers");
        return;
    }
    Serial.println("Peer hinzugefügt");
    esp_now_register_recv_cb(dataReceived);
}

void DMXnow::sendDMXData() { //ToDO: daten komprimieren
    // uint8_t dmxData[NUM_DMX_CHANNELS];
    // for (int i = 0; i < NUM_DMX_CHANNELS; i++) {
    //     dmxData[i] = i * 10; // Beispiel-Daten
    // }

    // uint8_t compressedData[COMPRESSED_BUFFER_SIZE];
    // uint16_t compressedSize;
    // compressData(dmxData, sizeof(dmxData), compressedData, &compressedSize);

    // esp_now_send(broadcastAddress, compressedData, compressedSize);
    Serial.println("ToDo: Komprimierte DMX-Daten senden.");
}



void DMXnow::onDataSent(const uint8_t* macAddr, esp_now_send_status_t status) {
    // Serial.print("Sende-Status: ");
    // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Erfolg" : "Fehlgeschlagen");
}



//!! NEW

/*
send slave request
*/
void DMXnow::sendSlaveRequest() {
    artnow_packet_t _packet;
    packet = _packet;

    packet.universe = SLAVE_CODE_REQUEST;
    packet.sequence = 0; // Hier sollte eine sinnvolle Sequenznummer gesetzt werden
    packet.part = 0;     // Teilnummer des Pakets
    // Hier könnten weitere Daten hinzugefügt werden, je nach Bedarf

    // Senden an Broadcast-Adresse
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &packet, sizeof(artnow_packet_t));
    if (result == ESP_OK) {
        Serial.println("Slave request sent");
    } else {
        Serial.print("Error sending slave request: ");
        Serial.println(result);
    }
}

void DMXnow::sendSlaveSetter(const uint8_t *macAddr, String name, String value) {
    artnow_packet_t _packet;

    _packet.universe = SLAVE_CODE_SET;
    _packet.sequence = 0; // Hier sollte eine sinnvolle Sequenznummer gesetzt werden
    _packet.part = 0;     // Teilnummer des Pakets

  
    if(name.length() > SETTTER_NAME_LENGTH){
        Serial.println("settername to long");
        return;
    }
    if(value.length() > SETTER_VALUE_LENGTH){
        Serial.println("value to long");
        return;
    }

    uint8_t _buffer[50];

    uint8_t _settername [SETTTER_NAME_LENGTH];
    uint8_t _settervalue [SETTER_VALUE_LENGTH];

    const char* __settername = name.c_str();
    const char* __settervalue = value.c_str();

    sprintf((char *)_settername, __settername);
    sprintf((char *)_settervalue, __settervalue);


    memcpy(_buffer, _settername, sizeof(_settername));
    size_t offset = sizeof(_settername);// Bestimme die Position, an der das zweite Array beginnen soll
    memcpy(_buffer + offset, _settervalue, sizeof(_settervalue));

    // memcpy(packet.data + _counter, _settervalue, SETTER_VALUE_LENGTH);
    // _counter += SETTER_VALUE_LENGTH;
    memcpy(_packet.data, _buffer, sizeof(_buffer));

    esp_err_t result = esp_now_send(macAddr, (uint8_t *) &_packet, sizeof(artnow_packet_t));// Sende das Paket
    
    if (result == ESP_OK) {
        Serial.printf("Slave setter (%s:%s) sent.\n",name.c_str(), _settervalue );
    } else {
        Serial.print("Error sending slave setter: ");
        Serial.println(result);
    }
    Serial.printf("Slave setter (%s:%s) sent.\n",name.c_str(), value.c_str() );









    // for(int i = 0; i < sizeof(_buffer); i++){
    //     Serial.print(_buffer[i]);
    // }

}


/*
receive Data from slave
*/
void DMXnow::dataReceived(const uint8_t *macAddr, const uint8_t *data, int len){
    // Sicherstellen, dass die Länge der empfangenen Daten korrekt ist
    Serial.println("packet received...");

    if (len < sizeof(artnow_slave_t)) {
        Serial.println("Received packet size mismatch");
        return;
    }
    artnow_slave_t* packet = (artnow_slave_t*)data;  // Daten in die Struktur artnow_slave_t kopieren

    Serial.printf("***** slave(%02X.%02X.%02X.%02X) *****\n",packet->macAddress[0],packet->macAddress[1],packet->macAddress[2],packet->macAddress[3]);
    Serial.printf("responsecode: %u\n",packet->responsecode);
    Serial.printf("universe: %u\n",packet->universe);
    Serial.printf("dmxStart: %u \n",packet->dmxStart);
    Serial.printf("dmxCount: %u\n",packet->dmxCount);
    Serial.printf("MAC:%02X.%02X.%02X.%02X\n",packet->macAddress[0],packet->macAddress[1],packet->macAddress[2],packet->macAddress[3]);
    Serial.printf("rssi: %i dB\n\n",packet->rssi);
    

}


int DMXnow::findPeerByMac(const uint8_t* macAddr) {

    // Serial.printf("\tlooking for peer: %02X.%02X.%02X.%02X.%02X.%02X... ",macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
    for (int i = 0; i < PEERS_MAX; i++) {
        
        if (memcmp(peers[i].peer_addr, macAddr, 6) == 0) {
            // Serial.printf("peer gefunden: %i\n", i);
            return i; // Peer gefunden
        }
    }
    // Serial.print("peer nicht gefunden.");
    return -1; // Peer nicht gefunden
}