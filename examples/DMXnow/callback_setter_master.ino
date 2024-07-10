#include <Arduino.h>
#include <Artnet.h>
#include <AsyncUDP_ESP32_SC_Ethernet.h> 
#include <DMXnow.h>

#define SLAVE_SET_PREFIX "set."
#define SLAVE_GET_PREFIX "get."
#define SLAVE_SETGET_SELECT "slavemac" //select slave
#define SLAVE_SETGET_UNIVERSE "slave.universe" // universe 0...16 [uint8_t]
#define SLAVE_SETGET_DMXCHANNEL "slave.dmxchannel"  // dmx channel 1 ...512 [uint16_t]

#define SLAVE_SETGET_BRIGHTNESS "scene.brightness" // brightness (8bit) [uint8_t]
#define SLAVE_SETGET_BRIGHTNESS16 "scene.brightness16" // brightness (16bit) [uint16_t]
#define SLAVE_SETGET_STROBE "scene.strobe"  // strobevalue in Strobes per second [uint8_t]
#define SLAVE_SETGET_STROBEMODE "scene.strobemode" 
#define SLAVE_SETGET_BRIGHTNESS "scene.brightness" // brightness (8bit) [uint8_t]
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
int getIntFromMessage(String message);
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

    // Check message
    if(message.startsWith(SLAVE_SET_PREFIX)){ //setter
      message = message.substring(message.indexOf(".")+1);
      // Serial.println(message);

      if(message.startsWith(SLAVE_SETGET_SELECT)){  //select slave
        if(getMacFromMessage(message, selectedSlave)){
          Serial.printf("select Slave %02X:%02X:%02X:%02X:%02X:%02X.\n", selectedSlave[0], selectedSlave[1], selectedSlave[2], selectedSlave[3], selectedSlave[4], selectedSlave[5]);
        }
      }else if(message.startsWith(SLAVE_SETGET_UNIVERSE)){  //universe
        uint8_t _universe = (uint8_t) getUint8_tFromMessage(message);
        Serial.printf("send setter:%s:%u\n",SLAVE_SETGET_UNIVERSE,  _universe); 
        if(_universe >=0 && _universe <=16)DMXnow::sendSlaveSetter(selectedSlave, SLAVE_SETGET_UNIVERSE,String(_universe));

      }else if(message.startsWith(SLAVE_SETGET_DMXCHANNEL)){  //dmxchannel
        uint16_t _channel = (uint16_t) getIntFromMessage(message);
        Serial.printf("send setter:%s:%u\n",SLAVE_SETGET_DMXCHANNEL,  _channel); 
        if(_channel > 0 && _channel < 512)DMXnow::sendSlaveSetter(selectedSlave, SLAVE_SETGET_DMXCHANNEL,String(_channel));

      }else{
        Serial.printf("unknown setter:%s/n",message); 
        return;
      }

    }else if(message.startsWith(SLAVE_GET_PREFIX)){ //getter
      //ToDo: tba
    }else {
      Serial.println("unknown cmd");
      return;
    } 
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
    int colonIndex = message.indexOf(':');
    if (colonIndex != -1) {
        String numberPart = message.substring(colonIndex + 1);
        numberPart.trim();
        int value = numberPart.toInt(); // String to int
        if (value >= 0 && value <= 255) {
            return value; // return
        }
    }
    return -1; // range not valid
}

int getIntFromMessage(String message) {
    int colonIndex = message.indexOf(':');
    if (colonIndex != -1) {
        String numberPart = message.substring(colonIndex + 1);
        numberPart.trim();
        int value = numberPart.toInt(); // String to int
        Serial.println(value);
        return value; // return
    }
    return -1; // range not valid
}


bool getMacFromMessage(String message, uint8_t* macAddress) {
    uint8_t _mac[6] = {0,0,0,0,0,0};

    int colonIndex = message.indexOf(':', strlen(SLAVE_SETGET_SELECT));
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

    int colonIndex = message.indexOf(':', strlen(SLAVE_SETGET_SELECT));
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