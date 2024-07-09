#include <esp_now.h>
#include <WiFi.h>

uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   //broadcast-MAC
esp_now_peer_info_t peerInfo;


void onDataRecvUncompressed(const uint8_t* mac, const uint8_t* incomingData, int len) {

    Serial.print("Empfangene Daten: ");
    for (int i = 0; i < len; i++) {
        Serial.print(incomingData[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void setup(){
    Serial.begin(115200);
    delay(2000);
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(onDataRecvUncompressed);
}
 
void loop(){
    // char message[] = "Hi, this is a message from the transmitting ESP";
    String message = "Hello from the transmitting ESP. ";
    message += String(millis());
    esp_now_send(receiverAddress, (uint8_t *) message.c_str(), message.length());
    delay(1000);
}