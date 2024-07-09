#include <Arduino.h>
#include <Artnet.h>
#include <AsyncUDP_ESP32_SC_Ethernet.h> 

// SPI Pin Definitionen für den W5500 Ethernet Adapter
#define ETH_SPI_HST     SPI2_HOST
#define SPI_CLOCK_SPEED 25
#define PIN_W5500_INT   5  // Interrupt Pin
#define PIN_SPI_MISO    2
#define PIN_SPI_MOSI    7
#define PIN_SPI_SCK     6
#define PIN_W5500_CS    8

#define ESP32_Ethernet_onEvent            ESP32_W5500_onEvent
#define ESP32_Ethernet_waitForConnect     ESP32_W5500_waitForConnect

artnet_config_s artnet_config;
uint8_t universe1[512]; //array für DMX-daten

void initEthernet();
void myArtDmxCallback(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP);
void myArtSyncCallback(IPAddress remoteIP);

// Change ip and mac address for your setup
byte ip[] = {10, 0, 1, 199};
byte broadcast[] = {10, 0, 1, 255};
byte mac[] = {0x04, 0xE9, 0xE5, 0x00, 0x69, 0xEC};


void setup(){
  artnet_config.inputuniverse[0] = 1;
  artnet_config.longname = "ESP32-C3 Artnet Node";
  artnet_config.shortname = "ESP32C3 Artnode";
  artnet_config.nodereport = "this is an Artnet Node from an ESP32-C3";
  

  Serial.begin(115200);
  Serial.println("booting ArtnetNode...");
  
  initEthernet(); //initialize ethernet connection

  Artnet::setConfig(artnet_config);
  Artnet::setArtDmxCallback(myArtDmxCallback);
  Artnet::setArtSyncCallback(myArtSyncCallback);
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

void myArtDmxCallback(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP) {
  Serial.print("Universe: ");
  Serial.print(universe);
  Serial.print(", Length: ");
  Serial.print(length);
  Serial.print(", Sequence: ");
  Serial.print(sequence);
  Serial.print(", Remote IP: ");
  Serial.print(remoteIP);
  Serial.print(", Data: \t");
  for (int i = 0; i < 10; i++) {
      Serial.print(data[i]);
      Serial.print(" ");
  }
  Serial.println();
}

void myArtSyncCallback(IPAddress remoteIP) {
  Serial.print("Sync received from: ");
  Serial.println(remoteIP);

  // Hier kannst du Aktionen ausführen, die bei einem Sync benötigt werden
}