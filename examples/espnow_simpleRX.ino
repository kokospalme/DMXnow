#include <esp_now.h>
#include <WiFi.h>

void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    Serial.print("Daten empfangen: ");
    for (int i = 0; i < data_len; i++) {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.println("Slave booting...");
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Fehler beim Initialisieren von ESP-NOW");
        return;
    }
    Serial.println("ESP-NOW initialisiert");
    esp_now_register_recv_cb(onDataRecv);
}

void loop() {}