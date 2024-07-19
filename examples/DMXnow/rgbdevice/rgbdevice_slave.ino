/*
rgbdevice_slave.ino

This example demonstrates the use of ESP-NOW in a slave device to receive and process commands from a master device. The slave device handles two specific commands: setting brightness and setting RGB values. Commands are received via DMXnow and are processed using a callback function.

When a setter command is received:
- For setting universe (`slave.universe`), the slave verifies if the value is numeric and updates the universe accordingly.
- For setting DMX channel (`slave.dmxchannel`), the slave verifies if the value is numeric and updates the DMX channel accordingly.
- For setting brightness (`scene.brightness`), the slave verifies if the value is numeric and updates the brightness accordingly.
- For setting RGB values (`scene.rgb`), the slave verifies if the value is in a valid RGB format (three numeric values separated by dots) and updates the RGB array accordingly.

This example utilizes the DMXnow and Artnet libraries for communication and control.
DMXnow: https://github.com/kokospalme/DMXnow
Artnet: https://github.com/kokospalme/DMXnow

Licensed under the Creative Commons Attribution 4.0 International (CC BY 4.0).
To view a copy of this license, visit http://creativecommons.org/licenses/by/4.0/
*/

#include <Arduino.h>
#include <Artnet.h>
#include <DMXnow.h>
#include <preferences.h>
#include <FastLED.h>

// configuration of device (individual)
#define DEVICE_NAME "DMXnowDevice1"
#define CHANNELCOUNT 4

//known setter-names (individual)
#define SLAVE_SETGET_UNIVERSE "slave.universe" // universe 0...16 [uint8_t]
#define SLAVE_SETGET_DMXCHANNEL "slave.dmxchannel"  // dmx channel 1 ...512 [uint16_t]

#define SLAVE_SETGET_BRIGHTNESS "scene.brightness" // brightness (8bit) [uint8_t]
#define SLAVE_SETGET_RGB "scene.rgb" // rgb

//pixelstuff (individual)
#define FASTLED_ALLOW_INTERRUPTS 1
#define LED_PIN 5  //hardwarepin for data pin 
#define NUM_PIXELS 7  //number of pixels
CRGB leds[NUM_PIXELS];  //fastled object


struct myConfig_t{  //individual config of slave device
  uint8_t universe = 0;
  uint16_t dmxchannel = 1;
  uint8_t brightness = 100; //brightness value
  uint8_t rgb[3] = {100, 0, 0}; //rgb values
}config;
Preferences prefs;  //preferences to save config in
artnow_slave_t dmxnowCfg;

void getConfig(); //gets config from memory
void setterCallback(const uint8_t* macAddr, String name, String value); //callback function when a setter is received (tp change dmx channel, universe etc.)
void dmxCallback(uint8_t* data);  //callback when dmx data is received

bool isNumeric(String str); //helper function
bool isValidRGBString(String str);  //helper function
void getRgb(String str, uint8_t* rgbArray); //helper function


void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.print("\nbooting... ");Serial.println(esp_reset_reason());
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_PIXELS);

  getConfig();
  DMXnow::setSlaveconfig(dmxnowCfg);
  DMXnow::initSlave();  //initialize as slave
  DMXnow::setDmxCallback(dmxCallback);
  DMXnow::setSetterCallback(setterCallback);  //register a callback function for when a setter is coming
}

void loop() {
  // Serial.println("doing other stuff..");
  delay(5000);
}


/*
get config from preferences
*/
void getConfig(){
  prefs.begin(DEVICE_NAME, false);
  myConfig_t defaultConfig;

  config.universe = (uint8_t) prefs.getUInt("universe", defaultConfig.universe);
  config.dmxchannel = (uint16_t) prefs.getUShort("dmxchannel", defaultConfig.dmxchannel);
  config.brightness = (uint8_t) prefs.getUShort("brightness", defaultConfig.brightness);
  config.rgb[0] = (uint8_t) prefs.getUShort("rgbR", defaultConfig.rgb[0]);  //red
  config.rgb[1] = (uint8_t) prefs.getUShort("rgbG", defaultConfig.rgb[1]);  //green
  config.rgb[2] = (uint8_t) prefs.getUShort("rgbB", defaultConfig.rgb[2]);  //blue

  Serial.printf("***** %s booting... ****\nuniverse: %u\ndmxchannel: %u\n[RGB]brightness: [%u.%u.%u]%u\n******************\n", DEVICE_NAME, config.universe, config.dmxchannel, config.rgb[0] , config.rgb[1] , config.rgb[2], config.brightness );
  prefs.end();

  dmxnowCfg.universe = config.universe;
  dmxnowCfg.dmxChannel = config.dmxchannel;
  dmxnowCfg.dmxCount = CHANNELCOUNT;

  FastLED.setBrightness(config.brightness); //set "panic mode", so it's not dark when no data is received
  for(int i = 0; i < NUM_PIXELS; i ++){
    leds[i].r = config.rgb[0];
    leds[i].g = config.rgb[1];
    leds[i].b = config.rgb[2];
  }
  FastLED.show();

}

// dmx callback function
void dmxCallback(uint8_t* data){
  uint16_t counter = config.dmxchannel-1;
  FastLED.setBrightness(data[counter]);
  // Serial.println(counter);
  for(int i = 0; i < NUM_PIXELS; i ++){
    leds[i].r = data[counter + 1];
    leds[i].g = data[counter + 2];
    leds[i].b = data[counter + 3];
  }
  FastLED.show();
}

/*
callback function
*/
void setterCallback(const uint8_t* macAddr, String name, String value){ //callback function for setter
  Serial.printf("setter from %02x:%02x:%02x:%02x:%02x:%02x: %s:%s\t", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5],name.c_str(), value.c_str());

  if(name == SLAVE_SETGET_UNIVERSE && isNumeric(value)){  //got universe setter
    int _uni = value.toInt();
    if(_uni >= 0 && _uni <= 16){
      Serial.printf("valid[%s]:%i\n",name.c_str(), _uni);
      config.universe = (uint8_t) _uni;
      dmxnowCfg.universe = (uint8_t) _uni;
      prefs.begin(DEVICE_NAME, false);  //safe config
      prefs.putUInt("universe", (uint32_t) config.universe);
      prefs.end();

    }else{
      Serial.printf("%i not valid.\n", config.universe);
    }

  } else  if(name == SLAVE_SETGET_DMXCHANNEL && isNumeric(value)){  //got dmxchannel setter
    int _channel = value.toInt();
    if(_channel >= 1 && _channel <= (512 - CHANNELCOUNT +1)){
      Serial.printf("valid[%s]:%i\n",name.c_str(), _channel);
      config.dmxchannel = (uint16_t) _channel;
      dmxnowCfg.dmxChannel = (uint16_t) _channel;
      prefs.begin(DEVICE_NAME, false);  //safe config
      prefs.putUShort("dmxchannel", (uint16_t) config.dmxchannel);
      prefs.end();
    }else{
      Serial.printf("%i not valid.\n", _channel);
    }
  } else if(name == SLAVE_SETGET_BRIGHTNESS && isNumeric(value)){ //got rgb setter
    int _bright = value.toInt();
    if(_bright >= 0 && _bright <= 255){
      Serial.printf("valid[%s]:%i\n",name.c_str(), _bright);
      config.brightness = (uint8_t) _bright;
      prefs.begin(DEVICE_NAME, false);  //safe config
      prefs.putUShort("brightness", config.brightness);
      prefs.end();
    }else{
      Serial.printf("%i not valid.\n", _bright);
    }
  }else if(name == SLAVE_SETGET_RGB && isValidRGBString(value)){ //got rgb setter
    getRgb(value, config.rgb);
    Serial.printf("got a valid [%s] setter: %u.%u.%u\n", name.c_str(), config.rgb[0], config.rgb[1], config.rgb[2]);
    prefs.begin(DEVICE_NAME, false);  //safe config
    prefs.putUShort("rgbR", config.rgb[0]);
    prefs.putUShort("rgbG", config.rgb[1]);
    prefs.putUShort("rgbB", config.rgb[2]);
    prefs.end();
  }else {
    Serial.println("unknown setter.");
    return;
  }

  DMXnow::setSlaveconfig(dmxnowCfg);

}

/*
checks if number is numeric
*/
bool isNumeric(String str) {
    for (size_t i = 0; i < str.length(); i++) {
        if (!isdigit(str.charAt(i))) {
            return false;
        }
    }
    return true;
}

/*
checks if String is valid RGB
*/
bool isValidRGBString(String str) {
    
    int firstDotIndex = str.indexOf('.');// Split the string by dots
    int secondDotIndex = str.indexOf('.', firstDotIndex + 1);

    
    if (firstDotIndex == -1 || secondDotIndex == -1 || str.indexOf('.', secondDotIndex + 1) != -1) {// Check if there are exactly two dots
        return false;
    }

    String red = str.substring(0, firstDotIndex);// Extract the three components
    String green = str.substring(firstDotIndex + 1, secondDotIndex);
    String blue = str.substring(secondDotIndex + 1);

    return isNumeric(red) && isNumeric(green) && isNumeric(blue);// Check if all components are numeric
}

/*
gets rgb array 
*/
void getRgb(String str, uint8_t* rgbArray) {
    int firstDotIndex = str.indexOf('.');
    int secondDotIndex = str.indexOf('.', firstDotIndex + 1);

    // Extract the three components
    String red = str.substring(0, firstDotIndex);
    String green = str.substring(firstDotIndex + 1, secondDotIndex);
    String blue = str.substring(secondDotIndex + 1);

    // Convert them to integers and store in the array
    rgbArray[0] = (uint8_t) red.toInt();
    rgbArray[1] = (uint8_t) green.toInt();
    rgbArray[2] = (uint8_t) blue.toInt();
}