#include <esp_now.h>
#include <WiFi.h>

#define UNI_SLAVE_REQUEST 254

typedef struct {
    uint8_t universe;   // DMX universe for this data, 254 for slave request
    uint8_t sequence;   // Sequence number
    uint8_t part;       // part (0...2)
    uint8_t data[];     // DMX data
} __attribute__((packed)) artnow_packet_t;

typedef struct {
    uint8_t responsecode = 0;   //code for response
    uint8_t macAddress[6];      //slave's mac addres
    uint8_t wifiChannel = 0;    //wifichannel
    int8_t rssi = 0;           //signal strength
    uint8_t universe = 1;       //slave's universe from 0 ...16
    uint16_t dmxStart = 1;      //slave's sartaddress, starting at 1
    uint16_t dmxCount = 3;      //number of dmxchannels used
} __attribute__((packed)) artnow_slave_t;


#define PEERS_MAX 10
artnow_slave_t mySlaveData;
esp_now_peer_info_t peerInfo;
esp_now_peer_info_t peers[PEERS_MAX];
uint8_t freePeer = 0;

uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t macMaster[6] = {0x80, 0x65, 0x99, 0x85, 0x52, 0xF8};

//Funktionen
void registerMaster(const uint8_t* macAddr, int peerNo);
void sendSlaveresponse(const uint8_t* macMaster);
int findPeerByMac(const uint8_t* macAddress);
void messageReceived(const uint8_t* macAddr, const uint8_t* data, int len);


// Funktion zum Senden der Antwort an den Master
void sendSlaveresponse(const uint8_t* macMaster) {
    mySlaveData.rssi = WiFi.RSSI();
    mySlaveData.responsecode = UNI_SLAVE_REQUEST;

    Serial.println("DMX-Daten gesendet");

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

int findPeerByMac(const uint8_t* macAddr) {

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

void messageReceived(const uint8_t* macAddr, const uint8_t* data, int len) {
    // Sicherstellen, dass die Länge der empfangenen Daten korrekt ist
    if (len < sizeof(artnow_packet_t)) {
        Serial.println("Received packet size mismatch");
        return;
    }

    artnow_packet_t* packet = (artnow_packet_t*)data;// Daten in die Struktur artnow_packet_t kopieren
    uint8_t universe = packet->universe;// Nachricht dekodieren

    if(universe >= 0 && universe <=16){
        //valid universe...
    }else if(universe == UNI_SLAVE_REQUEST){
        Serial.printf("slave request from %02X.%02X.%02X.%02X.%02X.%02X... ",macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
        int _findPeer = findPeerByMac(macAddr);
        if(_findPeer == -1){
            registerMaster(macAddr,freePeer);    //new peer
            sendSlaveresponse(macAddr);
        }else(sendSlaveresponse(macAddr));   //known peer
        
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

void onDataSent(const uint8_t* macAddr, esp_now_send_status_t status) {
    Serial.print("Sende-Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Erfolg" : "Fehlgeschlagen");
}


/*
register peer to list
*/
void registerMaster(const uint8_t* macAddr, int peerNo){
    memcpy(peerInfo.peer_addr, macMaster, 6);
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

void setup(){
    Serial.begin(115200);
    delay(2000); // uncomment if your serial monitor is empty
    WiFi.mode(WIFI_STA);
    
    esp_now_init();
    esp_now_register_recv_cb(messageReceived);
    
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Fehler beim Hinzufügen des Peers");
        return;
    }
    Serial.println("Peer hinzugefügt");

}


void loop(){
    // esp_now_send(broadcastAddress, (uint8_t *) &mySlaveData, sizeof(artnow_slave_t));

    sendSlaveresponse(broadcastAddress);
    delay(2000);
}