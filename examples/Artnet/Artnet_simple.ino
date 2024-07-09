#include <Arduino.h>
#include <Artnet.h>
#include <AsyncUDP_ESP32_SC_Ethernet.h> 

#define ESP32_Ethernet_onEvent            ESP32_W5500_onEvent
#define ESP32_Ethernet_waitForConnect     ESP32_W5500_waitForConnect

// SPI Pin Definitionen für den W5500 Ethernet Adapter
#define ETH_SPI_HST     SPI2_HOST
#define SPI_CLOCK_SPEED 25
#define PIN_W5500_INT   5  // Interrupt Pin
#define PIN_SPI_MISO    2
#define PIN_SPI_MOSI    7
#define PIN_SPI_SCK     6
#define PIN_W5500_CS    8

Artnet artnet;

// Change ip and mac address for your setup
byte ip[] = {10, 0, 1, 199};
byte broadcast[] = {10, 0, 1, 255};
byte mac[] = {0x04, 0xE9, 0xE5, 0x00, 0x69, 0xEC};

void initEthernet();

void setup(){
  Serial.begin(115200);
  Serial.println("booting ArtnetNode...");
  
  initEthernet();
  Artnet::begin(mac, ip);

}

void loop(){
  Serial.println("doing stuff...");
  delay(1500);
}


// Initialisierung der Ethernet-Verbindung
void initEthernet() {
    Serial.println("Initialisiere Ethernet...");
    
    // Ethernet-Adapter starten
    ESP32_Ethernet_onEvent();
    ETH.begin(PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_SCK, PIN_W5500_CS, PIN_W5500_INT, SPI_CLOCK_SPEED, ETH_SPI_HST);
    // Konfiguration der statischen IP-Adresse
    // ETH.config(myIP, myGW, mySN, myDNS);
    ESP32_Ethernet_waitForConnect();

    // Lokale IP-Adresse ausgeben
    Serial.print("Ethernet IP: ");
    Serial.println(ETH.localIP());
}
