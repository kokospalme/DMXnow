#include <Arduino.h>
#include <Artnet.h>
#include <AsyncUDP_ESP32_SC_Ethernet.h> 
#include <esp_now.h>
#include <WiFi.h>

#define NUM_DMX_CHANNELS 20

typedef struct {
    uint8_t data[NUM_DMX_CHANNELS];
} DMX_Packet;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void sendDMXData() {
    DMX_Packet dmxPacket;
    for (int i = 0; i < NUM_DMX_CHANNELS; i++) {
        dmxPacket.data[i] = i * 10; // Testdaten
    }
    esp_now_send(broadcastAddress, (uint8_t *) &dmxPacket, sizeof(dmxPacket));
    Serial.println("DMX-Daten gesendet");
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.println("Master booting...");
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Fehler beim Initialisieren von ESP-NOW");
        return;
    }
    Serial.println("ESP-NOW initialisiert");
    esp_now_register_send_cb(onDataSent);

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

void loop() {
    sendDMXData();
    delay(1000);
}
