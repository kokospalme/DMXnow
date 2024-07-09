#include <dmx.h>
#include <esp_now.h>
#include <WiFi.h>


// Hardware
#define PIN_DMX_SEL 1
#define PIN_LED_RX 0


#define DMX_UNIVERSE 0 // ID des DMX-Universums, in das die Daten geschrieben werden

// Buffer-Größen für DMX-Universen
#define DMX_BUFFER_SIZE 512
#define NUMBER_OF_UNIVERSES 4

// Prototypen der Funktionen und Methoden
void initDMX();
void readDMXData();
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void setupESPNow();
void sendDMXData(uint8_t universe);
void onESPNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void collectSlaveInfo();

// Globale Variablen und Puffer
uint8_t dmxBuffers[NUMBER_OF_UNIVERSES][DMX_BUFFER_SIZE]; // Puffer für DMX-Daten von 4 Universen

// Struktur zur Speicherung von Slave-Informationen
struct SlaveInfo {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_peer_num_t status;
};

void setup() {
    Serial.begin(115200);
    delay(2000);
    initDMX();// Initialisiere DMX
    setupESPNow();// Initialisiere ESP-NOW
    collectSlaveInfo();

    while(1){};
}

void loop() {
    
    readDMXData();// Lese DMX-Daten und aktualisiere den Puffer
    
    // Sende DMX-Daten über ESP-NOW
    for (uint8_t i = 0; i < NUMBER_OF_UNIVERSES; i++) {
        sendDMXData(i);
    }

    delay(100); // Wartezeit zwischen den Übertragungen
}

// Funktion zur Initialisierung der DMX
void initDMX() {
    DMX::Initialize(PIN_DMX_SEL, PIN_LED_RX, input);
    Serial.println("DMX Initialized");
}

// Funktion zum Lesen der DMX-Daten
void readDMXData() {
    uint8_t *data = dmxBuffers[DMX_UNIVERSE]; // Puffer für das spezifizierte DMX-Universum (pointer zum Buffer)
    if(DMX::IsHealthy()){
      DMX::ReadAll(data, 1, DMX_BUFFER_SIZE);
      // Serial.println("DMX data read into buffer");
      // Serial.println(data[511]);
    }

}

// Callback-Funktion für das Senden von ESP-NOW Daten
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("Data sent successfully");
    } else {
        Serial.println("Data sending failed");
    }
}

// Funktion zur Initialisierung von ESP-NOW
void setupESPNow() {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb(onDataSent);
    Serial.println("ESP-NOW initialized");
}

// Funktion zum Senden von DMX-Daten über ESP-NOW
void sendDMXData(uint8_t universe) {
    uint8_t *data = dmxBuffers[universe];
    size_t dataSize = DMX_BUFFER_SIZE;

    esp_err_t result = esp_now_send(NULL, data, dataSize);
    if (result == ESP_OK) {
        Serial.print("DMX data sent for universe: ");
        Serial.println(universe);
    } else {
        Serial.print("Error sending DMX data for universe: ");
        Serial.println(universe);
    }
}

// Callback-Funktion für ESP-NOW Sendestatus
void onESPNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("DMX data sent successfully");
    } else {
        Serial.println("Failed to send DMX data");
    }
}

// Funktion zur Sammlung von Slave-Informationen
void collectSlaveInfo() {
    // Anzahl der verbundenen Peers abrufen
    esp_now_peer_num_t peerCount;
    esp_now_get_peer_num(&peerCount);
    Serial.printf("Total connected peers: %d\n", peerCount.total_num);

    // Peer-MAC-Adressen abrufen und Informationen anzeigen
    esp_now_peer_info_t peerInfo;
    for (int i = 0; i < peerCount.total_num; i++) {
        // Peer-Info-Array abrufen
        if (esp_now_get_peer(peerInfo.peer_addr, &peerInfo) == ESP_OK) {
            // Peer MAC-Adresse anzeigen
            Serial.print("Peer MAC Address: ");
            for (int j = 0; j < ESP_NOW_ETH_ALEN; j++) {
                Serial.printf("%02X", peerInfo.peer_addr[j]);
                if (j < ESP_NOW_ETH_ALEN - 1) {
                    Serial.print(":");
                }
            }

            // Weitere Peer-Informationen anzeigen
            Serial.print("\nPeer Channel: ");
            Serial.println(peerInfo.channel);
            Serial.print("Peer Encrypt: ");
            Serial.println(peerInfo.encrypt ? "Yes" : "No");
            Serial.print("Peer LMK: ");
            for (int j = 0; j < ESP_NOW_KEY_LEN; j++) {
                Serial.printf("%02X", peerInfo.lmk[j]);
                if (j < ESP_NOW_KEY_LEN - 1) {
                    Serial.print(":");
                }
            }
            Serial.print("\n");
            Serial.println("-------------------");
        } else {
            Serial.printf("Failed to get peer %d info\n", i);
        }
    }
}