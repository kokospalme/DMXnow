#include "DMXnow.h"

uint8_t DMXnow::broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
artnow_packet_t DMXnow::packet;
std::vector<artnow_slave_t> DMXnow::slaveArray;

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

void DMXnow::sendDMXData(uint8_t universe, uint16_t length, uint8_t sequence, uint8_t* data) { //ToDO: daten komprimieren

    uint16_t _parts = _parts = (length + DMXCHANNEL_PER_PACKET - 1) / DMXCHANNEL_PER_PACKET;
    uint16_t _dmxCounter = 0;

    artnow_packet_t _packet;
    if(universe > 16 || universe < 0) return;
    _packet.sequence = sequence;
    _packet.universe = universe;
    _packet.sequence = 0;

    for(int i = 0; i < _parts; i++){
        
        uint16_t _length = DMXCHANNEL_PER_PACKET;
        if(i >= _parts-1) _length = length - (_parts - 1) * DMXCHANNEL_PER_PACKET;  //last part
        _packet.part = i;
        // Serial.printf("sending part %u (%u)bytes... ", i, _length);

        for(int j = 0; j < _length; j++){
            _packet.data[j] = data[_dmxCounter];
            _dmxCounter++;
        }

        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &_packet, sizeof(_packet));

        if (result != ESP_OK) {
            // Serial.printf("Error sending dmx data (part%i): ", i);
            // Serial.println(result);
        }
        // delay(1);

        
    }
    // Serial.println("");

    // uint8_t dmxData[NUM_DMX_CHANNELS];
    // for (int i = 0; i < NUM_DMX_CHANNELS; i++) {
    //     dmxData[i] = i * 10; // Beispiel-Daten
    // }

    // uint8_t compressedData[COMPRESSED_BUFFER_SIZE];
    // uint16_t compressedSize;
    // compressData(dmxData, sizeof(dmxData), compressedData, &compressedSize);

    // esp_now_send(broadcastAddress, compressedData, compressedSize);
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

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &_packet, sizeof(_packet));

    if (result == ESP_OK) {
        Serial.print("Slave request sent ... ");
    } else {
        Serial.print("Error sending slave request: ");
        Serial.println(result);
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
void DMXnow::dataReceived(const uint8_t *macAddr, const uint8_t *data, int len){
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