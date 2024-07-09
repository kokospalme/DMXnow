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
        Serial.println("Fehler beim Hinzufügen des broadcast-Peers");
        return;
    }
    Serial.println("broadcast-Peer hinzugefügt");
    esp_now_register_recv_cb(dataReceived);

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

    _packet.universe = SLAVE_CODE_REQUEST;
    _packet.sequence = 0; // Hier sollte eine sinnvolle Sequenznummer gesetzt werden
    _packet.part = 0;     // Teilnummer des Pakets


    DMX_Packet _test;
    // _test.universe = 1;
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &_packet, sizeof(_packet));

    if (result == ESP_OK) {
        Serial.print("Slave request sent ... ");
    } else {
        Serial.print("Error sending slave request: ");
        Serial.println(result);
    }
}

void DMXnow::sendSlaveSetter(const uint8_t *macAddr, String name, String value) {

    if(name.length() > SETTTER_NAME_LENGTH){
        Serial.println("settername to long");
        return;
    }
    if(value.length() > SETTER_VALUE_LENGTH){
        Serial.println("value to long");
        return;
    }

    //buffer
    // Konvertiere String in ein char-Array (C-String)
    char charArray[name.length() + 1]; // +1 für das Nullterminierungszeichen
    name.toCharArray(charArray, sizeof(charArray));

    memcpy(packet.data, charArray, sizeof(packet.data));    //char array to buffer

    //output
    esp_err_t result = esp_now_send(macAddr, (uint8_t *)&packet,  sizeof(artnow_packet_t));

    if (result == ESP_OK) {
        Serial.println("sent setter to slave");
    } else {
        Serial.print("Error sending setter. ");
        Serial.println(result);
    }

    // String receivedString(packet.data);
    // String result;
    // size_t length2 = strnlen((const char*)_buffer, SETTTER_NAME_LENGTH); // Länge des Null-terminierten Strings
    // result.reserve(length2);
    // for (size_t i = 0; i < length2; ++i) {
    //     result += (char)_buffer[i];
    // }

    // Serial.printf("string: %s\n", result.c_str());



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