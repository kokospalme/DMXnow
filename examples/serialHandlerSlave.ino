#include <Arduino.h>
#include <Artnet.h>
#include <AsyncUDP_ESP32_SC_Ethernet.h> 
#include <DMXnow.h>

#define SLAVE_SET_PREFIX "slave.set."
#define SLAVE_GET_PREFIX "slave.get."
#define SLAVE_SETGET_SELECT "slave.select" //select slave
#define SLAVE_SETGET_UNIVERSE "universe" // universe 0...16 [uint8_t]
#define SLAVE_SETGET_DMXCHANNEL "dmxchannel"  // dmx channel 1 ...512 [uint16_t]

#define SLAVE_SETGET_BRIGHTNESS "scene.brightness" // brightness (8bit) [uint8_t]
#define SLAVE_SETGET_BRIGHTNESS16 "scene.brightness16" // brightness (16bit) [uint16_t]
#define SLAVE_SETGET_STROBE "scene.strobe"  // strobevalue in Strobes per second [uint8_t]
#define SLAVE_SETGET_STROBEMODE "scene.strobemode" 
#define SLAVE_SETGET_RGB "scene.rgb" // rgb
#define SLAVE_SETGET_RGB "scene.rgb2" // rgb2
#define SLAVE_SETGET_RGB "scene.rgb3" // rgb3
#define SLAVE_SETGET_RGB "scene.rgb4" // rgb4


uint8_t macSlave1[] = {0x60, 0x55, 0xF9, 0x21, 0x9E, 0x14};  //!for testing only
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t selectedSlave [] = {0,0,0,0,0,0};//slave1 60:55:F9:21:9E:14

// Serial interface
void serialInterface();
bool validateMessage(String message, String prefix);
bool isNumber(String str);
int getUint8_tFromMessage(String message);
bool getMacFromMessage(String message);
bool getMacFromMessage(String message, uint8_t* macAddress);
bool isHexadecimal(String str);

void setup() {
  Serial.begin(115200);
  delay(2000);
  DMXnow::init();
  DMXnow::registerPeer(macSlave1, 0); //for texting only
  delay(1000);
  DMXnow::sendSlaveRequest();
}

void loop() {
  serialInterface();
}

void serialInterface() {
    String message = ""; // String to store the received message

    while (Serial.available() > 0) {
        char incomingByte = Serial.read(); // Read the next character from the serial interface
        if (incomingByte == '\n') {
            break; // Exit the loop if the end of the line is reached
        }
        message += incomingByte; // Add the character to the message
    }
    if(message == "") return;

    // Check the message
    if (validateMessage(message, SLAVE_SETGET_SELECT)) { // Select Slave
      if(getMacFromMessage(message, selectedSlave)){
        Serial.printf("select Slave %02X:%02X:%02X:%02X:%02X:%02X.\n", selectedSlave[0], selectedSlave[1], selectedSlave[2], selectedSlave[3], selectedSlave[4], selectedSlave[5]);
      }

    } else if(validateMessage(message, SLAVE_SETGET_BRIGHTNESS)){  // scene.brightness
      if(getUint8_tFromMessage(message) < 0) return; // Return if value is not valid
      Serial.printf("send \t%s:%u\t to selected slave... \n", SLAVE_SETGET_BRIGHTNESS, (uint8_t)getUint8_tFromMessage(message));
      DMXnow::sendSlaveSetter(selectedSlave, SLAVE_SETGET_BRIGHTNESS, String((uint8_t)getUint8_tFromMessage(message)));
    } else if(validateMessage(message, SLAVE_SETGET_UNIVERSE)){

    } else {
      Serial.println("unknown cmd");
    } 
}


bool validateMessage(String message, String prefix) {
    if (message.startsWith(prefix)) {
        int colonIndex = message.indexOf(':', prefix.length());
        if (colonIndex != -1) {
            String numberPart = message.substring(colonIndex + 1);
            numberPart.trim();
            if (isNumber(numberPart)) { // is number ?
                return true;
            } else if(getMacFromMessage(message)){  // is mac?
              return true;
            } else {
                Serial.println("Invalid number format: " + message);
            }
        } else {
            Serial.println("value not found: " + message);
        }
    }
    return false; // message not valid
}

bool isNumber(String str) {
    for (int i = 0; i < str.length(); i++) {
        if (!isDigit(str[i])) {
            return false;
        }
    }
    return true;
}

int getUint8_tFromMessage(String message) {
    int colonIndex = message.indexOf(':', strlen(SLAVE_SETGET_BRIGHTNESS));
    if (colonIndex != -1) {
        String numberPart = message.substring(colonIndex + 1);
        numberPart.trim();
        int value = numberPart.toInt(); // String to int
        if (value >= 0 && value <= 255) {
            return (uint8_t) value; // return
        }
    }
    return -1; // range not valid
}


bool getMacFromMessage(String message, uint8_t* macAddress) {
    uint8_t _mac[6] = {0,0,0,0,0,0};

    int colonIndex = message.indexOf(':', strlen(SLAVE_SELECT));
    if (colonIndex != -1) {
        String macPart = message.substring(colonIndex + 1);
        macPart.trim(); // Leerzeichen entfernen
        
        // Prüfen, ob die MAC-Adresse die richtige Länge hat
        if (macPart.length() != 17) {
            return false;
        }

        // Teile der MAC-Adresse extrahieren und in das Array schreiben
        int byteIndex = 0;
        for (int i = 0; i < 17; i += 3) {
            if (i > 0 && macPart[i - 1] != ':') {
                return false; // Trennzeichen ':' prüfen
            }

            String byteString = macPart.substring(i, i + 2);
            if (!isHexadecimal(byteString)) {
                return false; // Prüfen, ob die Zeichen hexadezimal sind
            }

            _mac[byteIndex++] = strtoul(byteString.c_str(), nullptr, 16);
        }
        for(int i = 0; i < 6;i++){  //write to array
          macAddress[i] = _mac[i];
        }
        return true;
    }

    return false;
}

bool getMacFromMessage(String message) {
    uint8_t _mac[6] = {0,0,0,0,0,0};

    int colonIndex = message.indexOf(':', strlen(SLAVE_SELECT));
    if (colonIndex != -1) {
        String macPart = message.substring(colonIndex + 1);
        macPart.trim(); // Leerzeichen entfernen
        
        // Prüfen, ob die MAC-Adresse die richtige Länge hat
        if (macPart.length() != 17) {
            return false;
        }

        // Teile der MAC-Adresse extrahieren und in das Array schreiben
        int byteIndex = 0;
        for (int i = 0; i < 17; i += 3) {
            if (i > 0 && macPart[i - 1] != ':') {
                return false; // Trennzeichen ':' prüfen
            }

            String byteString = macPart.substring(i, i + 2);
            if (!isHexadecimal(byteString)) {
                return false; // Prüfen, ob die Zeichen hexadezimal sind
            }

            _mac[byteIndex++] = strtoul(byteString.c_str(), nullptr, 16);
        }
        return true;
    }

    return false;
}

bool isHexadecimal(String str) {
    for (int i = 0; i < str.length(); i++) {
        char c = str[i];
        if (!isDigit(c) && !(c >= 'A' && c <= 'F') && !(c >= 'a' && c <= 'f')) {
            return false;
        }
    }
    return true;
}