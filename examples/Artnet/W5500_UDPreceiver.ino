#include <Arduino.h>
#include <SPI.h>
#include <AsyncUDP_ESP32_SC_Ethernet.h>

// SPI Pin Definitionen für den W5500 Ethernet Adapter
#define ETH_SPI_HOST        SPI2_HOST
#define SPI_CLOCK_MHZ       25
#define INT_GPIO            5  // Interrupt Pin
#define MISO_GPIO           2
#define MOSI_GPIO           7
#define SCK_GPIO            6
#define CS_GPIO             8

// Art-Net Port
#define ARTNET_PORT         6454

// MAC-Adressen und Netzwerkkonfiguration
#define NUMBER_OF_MAC       20

#define ESP32_Ethernet_onEvent            ESP32_W5500_onEvent
#define ESP32_Ethernet_waitForConnect     ESP32_W5500_waitForConnect

byte mac[][NUMBER_OF_MAC] = {
    { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 },
    // Weitere MAC-Adressen können hier hinzugefügt werden
};

// Statische IP-Adresse, Gateway, Subnetz und DNS-Server
IPAddress myIP(192, 168, 2, 232);
IPAddress myGW(192, 168, 2, 1);
IPAddress mySN(255, 255, 255, 0);
IPAddress myDNS(8, 8, 8, 8);

// UDP-Instanz
AsyncUDP udp;

// Funktion zur Paketverarbeitung
void parsePacket(AsyncUDPPacket packet) {
    Serial.println("Art-Net Packet Received:");
    Serial.print("From: ");
    Serial.print(packet.remoteIP());
    Serial.print(":");
    Serial.println(packet.remotePort());
    Serial.print("Length: ");
    Serial.println(packet.length());

    // Hier können spezifische Art-Net-Daten verarbeitet werden
    if (packet.length() >= 18) {
        char header[8];
        memcpy(header, packet.data(), 8);
        header[7] = 0; // Null-terminieren

        if (strcmp(header, "Art-Net") == 0) {
            Serial.println("Valid Art-Net Packet");
            // Zusätzliche Verarbeitung hier, z.B. DMX-Daten extrahieren
        }
    }

    Serial.println();
}

// Initialisierung der Ethernet-Verbindung
void initEthernet() {
    Serial.println("Initialisiere Ethernet...");
    
    // Ethernet-Adapter starten

    ESP32_Ethernet_onEvent();
    
    ETH.begin(MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST);

    // Konfiguration der statischen IP-Adresse
    // ETH.config(myIP, myGW, mySN, myDNS);
    
    // Auf Verbindung warten
    ESP32_Ethernet_waitForConnect();

    // Lokale IP-Adresse ausgeben
    Serial.print("Ethernet IP: ");
    Serial.println(ETH.localIP());
}

void setup() {
    // Serielle Verbindung starten
    Serial.begin(115200);
    while (!Serial && (millis() < 5000));
    delay(500);

    // Ethernet-Verbindung initialisieren
    initEthernet();

    // UDP-Server auf dem Art-Net-Port starten
    if (udp.listen(ARTNET_PORT)) {
        Serial.print("Listening for Art-Net on port: ");
        Serial.println(ARTNET_PORT);

        // Funktion zur Verarbeitung eingehender Pakete registrieren
        udp.onPacket([](AsyncUDPPacket packet) {
            parsePacket(packet);
        });
    } else {
        Serial.println("Failed to start UDP server");
    }
}

void loop() {
    // Hauptschleife, kann für andere Aufgaben genutzt werden
    delay(1000);
}
