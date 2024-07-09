#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Eindeutige MAC-Adresse für den ESP, die als Peer-Adresse verwendet wird
uint8_t broadcast_mac[] = {0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  Serial.begin(115200);
    delay(2000);
  // WiFi- und ESP-NOW-Setup
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() == ESP_OK) {
    Serial.println("ESP-NOW Init OK");
  } else {
    Serial.println("ESP-NOW Init Failed");
    delay(1000);
    ESP.restart(); // Neu starten, wenn die Initialisierung fehlschlägt
  }

  // Peer-Informationen einrichten
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  peerInfo.channel = 0; // Kanal für die Kommunikation (0-13)
  peerInfo.ifidx = WIFI_IF_STA; // WiFi-Interface (Station-Modus)
  peerInfo.encrypt = false; // Keine Verschlüsselung erforderlich
  memcpy(peerInfo.peer_addr, broadcast_mac, 6); // Broadcast-Adresse für alle Geräte im Netzwerk

  // Peer zum ESP-NOW-Netzwerk hinzufügen
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("ESP-NOW Peer erfolgreich hinzugefügt");
  } else {
    Serial.println("Fehler beim Hinzufügen des ESP-NOW Peers");
  }
}

void loop() {
  // Im Loop nichts tun, da der Slave nur auf eingehende Nachrichten wartet
  delay(1000);
  Serial.println("bored...");
}
