/*
DMXnow - DMX transmission over ESP-NOW between ESP32 / ESP32-C3 devices.
Originally inspired by https://github.com/Blinkinlabs/esp-now-dmx. Protocol v2 (single-packet
ESP-NOW 2.0 universes, capability discovery, multiple modes, typed settings) was inspired by a
draft ESP-NOW-2.0 DMX implementation by Carsten Koester (carsten@ckoester.net); see README.md
"Credits & prior art".

This library moves two fundamentally different kinds of data, and keeps them deliberately apart:

  - DMX CHANNELS (dmxnow_channel_t, DMXnow::setSlaveChannels()): live control values inside the
    DMX universe (Dimmer, RGB, Shutter, Pan/Tilt, ...). Change continuously, travel inside
    DMX_DATA packets broadcast every frame, and are only ever *described* (never individually
    requested/set) via the channel capability map.

  - SETTINGS (dmxnow_setting_t, DMXnow::registerSetting*()): configuration of the device itself
    (DMX address, universe, bit-depth mode, a default/failsafe scene, calibration, ...). Change
    rarely, travel as one-off SETTER/GETTER packets addressed to a single slave, and are declared
    with a name, a data type and a valid range so the library can parse/validate/dispatch them.

  Rule of thumb: if a lighting console would animate it every frame, it's a channel; if you'd
  only ever type it once into a patch sheet, it's a setting. See README.md for the full
  comparison table and a worked example of the same word ("brightness") meaning both.
*/
#ifndef DMXNOW_H
#define DMXNOW_H

#include <Arduino.h>
#include <vector>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_log.h>
#include <esp_err.h>
#include "esp_wifi.h"

// requires Arduino ESP32 core >= 3.3.0 (ESP-NOW 2.0 / ESP_NOW_MAX_DATA_LEN_V2 support)

#define DMX_UNIVERSES 4// Number of supported DMX universes
#define DMX_BUFSIZE 512// DMX slots per universe, excluding start code
#define DMX_BUFSIZE_WITH_STARTCODE 513

#define SLAVES_MAX 255
#define SETTER_NAME_LENGTH 24  //bytes (+1 when sent)
#define SETTER_VALUE_LENGTH 24  //bytes
#define SLAVENAME_LENGTH 16

// One ESP-NOW v2 packet (max 1470 bytes) carries a full DMX universe or a full capability
// description in a single transmission - no more splitting into parts.
#define DMXNOW_MAX_PACKET_SIZE  ESP_NOW_MAX_DATA_LEN_V2  // 1470

// A queue of outstanding sends, so DMX data, discovery and setter/getter packets can be
// fired off without waiting for the previous send's callback synchronously.
#define SEND_QUEUE_SIZE (DMX_UNIVERSES + 4)

#define DMXNOW_MAGIC 0x4E584D44UL  // ASCII "DMXN", little-endian on the wire
#define DMXNOW_PROTOCOL_VERSION 2

#define LOG_TAG "ESP TX"

struct SendQueueElem {
  uint8_t toBeSent = 0;
  uint16_t size = 0;
  uint8_t data[DMXNOW_MAX_PACKET_SIZE];
  uint8_t macAddr[6] = {0,0,0,0,0,0};
};

// ---- packet types (dmxnow_header_t.type) ----
#define DMXNOW_TYPE_DMXDATA             0x00
#define DMXNOW_TYPE_DISCOVERY_REQUEST   0x01
#define DMXNOW_TYPE_DISCOVERY_RESPONSE  0x02
#define DMXNOW_TYPE_SETTER              0x03
#define DMXNOW_TYPE_GETTER              0x04
#define DMXNOW_TYPE_GETTER_RESPONSE     0x05

// ---- header flags ----
#define DMXNOW_FLAG_NONE   0x00
#define DMXNOW_FLAG_RESET  0x01 // set on the first packets after a transmitter (re)started its sequence counter
#define DMXNOW_FLAG_NEW    0x02 // payload changed since the last transmission (vs. a periodic keep-alive resend)

// ---- channel function types (dmxnow_channel_t.type) ----
// Describes what a logical DMX function does, so a master can build a control surface / patch
// without any prior knowledge of the fixture.
#define DMXNOW_FN_NONE            0
#define DMXNOW_FN_DIMMER          1  // 1 logical channel
#define DMXNOW_FN_SHUTTER_STROBE  2  // 1 logical channel, behavior described by zones[] (see DMXNOW_STROBE_*)
#define DMXNOW_FN_RGB             3  // 3 logical channels (R,G,B)
#define DMXNOW_FN_RGBW            4  // 4 logical channels (R,G,B,W)
#define DMXNOW_FN_RGBWA           5  // 5 logical channels (R,G,B,W,Amber)
#define DMXNOW_FN_RGBWAU          6  // 6 logical channels (R,G,B,W,Amber,UV)
#define DMXNOW_FN_WHITE           7  // 1 logical channel
#define DMXNOW_FN_AMBER           8  // 1 logical channel
#define DMXNOW_FN_UV              9  // 1 logical channel
#define DMXNOW_FN_PAN             10 // 1 logical channel
#define DMXNOW_FN_TILT            11 // 1 logical channel
#define DMXNOW_FN_COLOR_WHEEL     12 // 1 logical channel, discrete slots described by zones[] (mode = slot id, label = color name)
#define DMXNOW_FN_GOBO_WHEEL      13 // 1 logical channel, discrete slots described by zones[] (mode = slot id, label = gobo name)
#define DMXNOW_FN_FOCUS           14 // 1 logical channel
#define DMXNOW_FN_ZOOM            15 // 1 logical channel
#define DMXNOW_FN_SPEED           16 // 1 logical channel, generic effect-speed/rate control
#define DMXNOW_FN_MACRO           17 // 1 logical channel, discrete slots described by zones[] (mode = program id, label = program name)
#define DMXNOW_FN_GENERIC         255 // 1 logical channel, no further meaning known to the master

// how many logical DMX slots (before *2 for 16 bit) a function type occupies
inline uint8_t dmxnow_fnChannelCount(uint8_t fnType) {
  switch (fnType) {
    case DMXNOW_FN_RGB:    return 3;
    case DMXNOW_FN_RGBW:   return 4;
    case DMXNOW_FN_RGBWA:  return 5;
    case DMXNOW_FN_RGBWAU: return 6;
    default:               return 1;
  }
}

#define DMXNOW_BITS_8  8
#define DMXNOW_BITS_16 16

// ---- dmxnow_zone_t.mode values, when the channel's type is DMXNOW_FN_SHUTTER_STROBE ----
// The zone's [value_from, value_to] range maps a coarse (8bit-equivalent) DMX value to a mode;
// param1/param2 carry the mode's rate range in Hz*10 (e.g. 200 = 20.0Hz), interpolated across the zone.
#define DMXNOW_STROBE_OFF     0
#define DMXNOW_STROBE_LINEAR  1
#define DMXNOW_STROBE_RANDOM  2
#define DMXNOW_STROBE_PULSE   3

// Sized so a full dmxnow_discovery_response_t stays within one ESP-NOW v2 packet (see the
// static_assert below) - raise either at the expense of the other if a fixture needs more of one.
#define DMXNOW_MAX_ZONES_PER_CHANNEL 8
#define DMXNOW_LABEL_LENGTH 8    // human readable zone label, e.g. "Red", "BlnkClr"; "" = unnamed
#define DMXNOW_MAX_CHANNELS 8    // logical functions describable per slave, per mode, in one discovery response

// A device can expose more than one channel map (e.g. an 8bit vs. a 16bit mode). Each mode gets
// its own full dmxnow_channel_t[DMXNOW_MAX_CHANNELS] table, both in a slave's own RAM and in the
// master's per-slave bookkeeping (dmxnow_slave_t.modes[]) - so this is a real RAM multiplier
// (roughly DMXNOW_MAX_CHANNELS * (5 + DMXNOW_MAX_ZONES_PER_CHANNEL * ~17) bytes per mode, per
// slave the master knows about). Kept small by default; raise only if a fixture genuinely needs
// more than 2 distinguishable modes.
#define DMXNOW_MAX_MODES 2
#define DMXNOW_MODE_NAME_LENGTH 12
#define DMXNOW_MODE_CURRENT 0xFF // dmxnow_discovery_request_t.mode: "describe whatever mode is active right now"

// Describes one value sub-range of a channel, e.g. a strobe mode/rate range, or a named
// color-wheel / gobo-wheel / macro slot. Meaning of `mode` depends on the channel's fn type.
typedef struct {
    uint8_t value_from = 0;   // inclusive, coarse (8bit-equivalent) DMX value
    uint8_t value_to = 0;     // inclusive
    uint8_t mode = 0;         // DMXNOW_STROBE_* for strobe channels, else an opaque slot/program id
    int16_t param1 = 0;       // e.g. rate at value_from [Hz*10], unused = 0
    int16_t param2 = 0;       // e.g. rate at value_to [Hz*10], unused = 0
    char label[DMXNOW_LABEL_LENGTH] = {0};
} __attribute__((packed)) dmxnow_zone_t;

// Describes one logical DMX function of a slave (e.g. "Dimmer", "Strobe", "Color1 RGB").
typedef struct {
    uint16_t offset = 0;      // 0-based logical-channel offset from the slave's dmxChannel start address
    uint8_t type = DMXNOW_FN_NONE;    // DMXNOW_FN_*
    uint8_t bits = DMXNOW_BITS_8;     // DMXNOW_BITS_8 or DMXNOW_BITS_16 (applies to every component, e.g. each of RGB)
    uint8_t zoneCount = 0;     // number of valid entries in zones[]
    dmxnow_zone_t zones[DMXNOW_MAX_ZONES_PER_CHANNEL];
} __attribute__((packed)) dmxnow_channel_t;

// Common header, present at the start of every DMXnow packet on the wire.
typedef struct {
    uint32_t magic = DMXNOW_MAGIC;
    uint8_t version = DMXNOW_PROTOCOL_VERSION;
    uint8_t type = DMXNOW_TYPE_DMXDATA;
    uint16_t sequence = 0;   // per-source, per-universe (for DMX data) or per-source (otherwise)
    uint8_t flags = DMXNOW_FLAG_NONE;
} __attribute__((packed)) dmxnow_header_t;

// A full (or partial) DMX universe in a single ESP-NOW v2 packet.
typedef struct {
    dmxnow_header_t hdr;
    uint8_t universe = 0;
    uint16_t offset = 0;     // start slot, 0 = start code
    uint16_t length = 0;     // number of valid bytes in payload[]
    uint8_t payload[DMX_BUFSIZE_WITH_STARTCODE] = {0};
} __attribute__((packed)) dmxnow_dmxdata_t;

// ---- named, typed settings (DMXnow::registerSetting*) ----
// A slave declares its settings once at boot (before DMXnow::initSlave()); each is advertised in
// the discovery response so a master can learn a device's setters/getters - name, data type,
// valid range - without hardcoding them, exactly like the channel capability map. Settings are
// mode-independent (they configure the device itself: addressing, defaults, calibration, ...).
#define DMXNOW_SETTING_UINT8   1
#define DMXNOW_SETTING_UINT16  2
#define DMXNOW_SETTING_INT8    3
#define DMXNOW_SETTING_INT16   4
#define DMXNOW_SETTING_BOOL    5
#define DMXNOW_SETTING_STRING  6

#define DMXNOW_SETTING_FLAG_SET 0x01 // this setting accepts DMXnow::sendSlaveSetter()
#define DMXNOW_SETTING_FLAG_GET 0x02 // this setting answers DMXnow::sendSlaveGetter()

#define DMXNOW_MAX_SETTINGS 8

// One named setting's schema: name, data type, and its valid range (numeric types: inclusive
// min/max; STRING: min unused, max = max length; BOOL: both unused).
typedef struct {
    char name[SETTER_NAME_LENGTH] = {0};
    uint8_t type = DMXNOW_SETTING_UINT8;
    int16_t min = 0;
    int16_t max = 0;
    uint8_t flags = 0; // DMXNOW_SETTING_FLAG_SET / DMXNOW_SETTING_FLAG_GET
} __attribute__((packed)) dmxnow_setting_t;

// Callback signatures per data type - registerSetting*() takes the ones matching its own type, so
// a handler is written in its natural type instead of parsing strings by hand.
typedef void (*dmxnow_setting_uint8_set_cb_t)(const uint8_t* macAddr, uint8_t value);
typedef uint8_t (*dmxnow_setting_uint8_get_cb_t)(const uint8_t* macAddr);
typedef void (*dmxnow_setting_uint16_set_cb_t)(const uint8_t* macAddr, uint16_t value);
typedef uint16_t (*dmxnow_setting_uint16_get_cb_t)(const uint8_t* macAddr);
typedef void (*dmxnow_setting_int8_set_cb_t)(const uint8_t* macAddr, int8_t value);
typedef int8_t (*dmxnow_setting_int8_get_cb_t)(const uint8_t* macAddr);
typedef void (*dmxnow_setting_int16_set_cb_t)(const uint8_t* macAddr, int16_t value);
typedef int16_t (*dmxnow_setting_int16_get_cb_t)(const uint8_t* macAddr);
typedef void (*dmxnow_setting_bool_set_cb_t)(const uint8_t* macAddr, bool value);
typedef bool (*dmxnow_setting_bool_get_cb_t)(const uint8_t* macAddr);
typedef void (*dmxnow_setting_string_set_cb_t)(const uint8_t* macAddr, String value);
typedef String (*dmxnow_setting_string_get_cb_t)(const uint8_t* macAddr);

// Broadcast (or unicast) by the master; the addressed slave(s) answer with a
// dmxnow_discovery_response_t describing `mode` (DMXNOW_MODE_CURRENT = whatever is active now).
typedef struct {
    dmxnow_header_t hdr;
    uint8_t mode = DMXNOW_MODE_CURRENT;
} __attribute__((packed)) dmxnow_discovery_request_t;

// A slave's self-description: mode-independent addressing, which mode this response describes,
// and that mode's channel capability map - so the master can learn what each channel does without
// any prior knowledge of the fixture. To learn about a device's other modes, the master sends a
// unicast discovery request naming that mode (DMXnow::sendSlaveModeRequest); this does not switch
// the device's live mode, it only asks it to describe one.
typedef struct {
    dmxnow_header_t hdr;
    uint8_t macAddress[6] = {0,0,0,0,0,0};
    uint8_t wifiChannel = 0;
    int8_t rssi = 0;
    uint8_t universe = 0;
    uint16_t dmxChannel = 1;   // 1-based DMX start address (mode-independent)
    char slavename[SLAVENAME_LENGTH] = {0};

    uint8_t mode = 0;          // which mode this response describes
    uint8_t activeMode = 0;    // the mode the device is actually running right now
    uint8_t modeCount = 1;     // how many modes this device supports (0..modeCount-1)
    char modeName[DMXNOW_MODE_NAME_LENGTH] = {0};
    uint16_t dmxCount = 0;     // DMX footprint of `mode` (not necessarily of activeMode)

    uint8_t channelCount = 0;  // number of valid entries in channels[] (capped at DMXNOW_MAX_CHANNELS)
    dmxnow_channel_t channels[DMXNOW_MAX_CHANNELS];

    uint8_t settingCount = 0;  // number of valid entries in settings[] (capped at DMXNOW_MAX_SETTINGS), mode-independent
    dmxnow_setting_t settings[DMXNOW_MAX_SETTINGS];
} __attribute__((packed)) dmxnow_discovery_response_t;

typedef struct {
    dmxnow_header_t hdr;
    char name[SETTER_NAME_LENGTH] = {0};
    char value[SETTER_VALUE_LENGTH] = {0};
} __attribute__((packed)) dmxnow_setter_t;

typedef struct {
    dmxnow_header_t hdr;
    char name[SETTER_NAME_LENGTH] = {0};
} __attribute__((packed)) dmxnow_getter_t;

// same shape as a setter: the requested name plus its current value
typedef dmxnow_setter_t dmxnow_getter_response_t;

static_assert(sizeof(dmxnow_dmxdata_t) <= DMXNOW_MAX_PACKET_SIZE, "dmxnow_dmxdata_t exceeds ESP-NOW v2 packet size");
static_assert(sizeof(dmxnow_discovery_response_t) <= DMXNOW_MAX_PACKET_SIZE, "dmxnow_discovery_response_t exceeds ESP-NOW v2 packet size - lower DMXNOW_MAX_CHANNELS, DMXNOW_MAX_ZONES_PER_CHANNEL or DMXNOW_MAX_SETTINGS");

// One mode's channel map, as learned (or not yet learned) by the master.
typedef struct {
    bool known = false;       // has a discovery response for this mode actually arrived yet?
    char modeName[DMXNOW_MODE_NAME_LENGTH] = {0};
    uint16_t dmxCount = 0;
    uint8_t channelCount = 0;
    dmxnow_channel_t channels[DMXNOW_MAX_CHANNELS];
} dmxnow_mode_info_t;

// Master-side bookkeeping record for a discovered slave (RAM only, not sent on the wire).
// A fresh broadcast discovery only fills in modes[activeMode]; sendSlaveModeRequest() lets the
// master fill in the other modes[] entries on demand, building a full per-device/per-mode profile.
typedef struct {
    uint8_t macAddress[6] = {0,0,0,0,0,0};
    uint8_t wifiChannel = 0;
    int8_t rssi = 0;
    uint8_t universe = 0;
    uint16_t dmxChannel = 1;
    char slavename[SLAVENAME_LENGTH] = {0};

    uint8_t activeMode = 0;
    uint8_t modeCount = 1;
    dmxnow_mode_info_t modes[DMXNOW_MAX_MODES];

    uint8_t settingCount = 0;
    dmxnow_setting_t settings[DMXNOW_MAX_SETTINGS];

    unsigned long lastSeenMillis = 0;
} dmxnow_slave_t;


class DMXnow {
public:
//master stuff
    static void init();
    static void pushDMXData(uint8_t universe, uint16_t length, uint8_t sequence, uint8_t* data, bool send);

    static void sendSlaveRequest(); // broadcasts a discovery request; slaves answer describing their currently active mode
    static void sendSlaveModeRequest(const uint8_t *macAddr, uint8_t mode); // ask one known slave to describe a specific mode, without switching it live
    static void sendSlaveSetter(const uint8_t *macAddr, String name, String value);
    static void sendSlaveGetter(const uint8_t *macAddr, String name);

    static int getSlaveCount();
    static const dmxnow_slave_t* getSlave(int index);
    static const dmxnow_slave_t* getSlaveByMac(const uint8_t* macAddr);

    static void setDiscoveryCallback(void (*fptr)(const dmxnow_slave_t& slave)); // called whenever a slave (re)registers
    static void setGetterResponseCallback(void (*fptr)(const uint8_t* macAddr, String name, String value));

// slave stuff
    static void initSlave();
    static void setSlaveconfig(const dmxnow_slave_t& config); // mode-independent identity: universe, dmxChannel, slavename

    // declares (or updates) one mode's channel map. makeActive also switches the live mode
    // (activeMode) and is what most single-mode devices want; a multi-mode device registers each
    // mode once at boot (makeActive only for the one that's actually running), then calls
    // setActiveMode() whenever it switches.
    static void setSlaveChannels(uint8_t mode, const char* modeName, uint16_t dmxCount, const dmxnow_channel_t* channels, uint8_t count, bool makeActive = true);
    static void setActiveMode(uint8_t mode); // switch which registered mode is live right now
    static void registerPeer(const uint8_t* macAddr);
    static void deletePeer(const uint8_t* macAddr);
    static void sl_dataReceived(const esp_now_recv_info_t* info, const uint8_t* data, int len);

    static void setSetterCallback(void (*fptr)(const uint8_t* macAddr, String name, String value)); // fallback for names not covered by registerSetting*()
    static void setGetterCallback(String (*fptr)(const uint8_t* macAddr, String name)); // fallback for names not covered by registerSetting*(); return "" if name is unknown
    static void setDmxCallback(void (*fptr)(uint8_t* data));

    // Declares one named, typed setting before DMXnow::initSlave() - the library then parses,
    // range-checks and dispatches incoming setters/getters for it automatically, and advertises
    // its name/type/range in the discovery response. setCb and/or getCb may be nullptr (a
    // get-only setting, e.g. a sensor reading, or a set-only one with no readback).
    static void registerSettingUint8(const char* name, uint8_t min, uint8_t max, dmxnow_setting_uint8_set_cb_t setCb = nullptr, dmxnow_setting_uint8_get_cb_t getCb = nullptr);
    static void registerSettingUint16(const char* name, uint16_t min, uint16_t max, dmxnow_setting_uint16_set_cb_t setCb = nullptr, dmxnow_setting_uint16_get_cb_t getCb = nullptr);
    static void registerSettingInt8(const char* name, int8_t min, int8_t max, dmxnow_setting_int8_set_cb_t setCb = nullptr, dmxnow_setting_int8_get_cb_t getCb = nullptr);
    static void registerSettingInt16(const char* name, int16_t min, int16_t max, dmxnow_setting_int16_set_cb_t setCb = nullptr, dmxnow_setting_int16_get_cb_t getCb = nullptr);
    static void registerSettingBool(const char* name, dmxnow_setting_bool_set_cb_t setCb = nullptr, dmxnow_setting_bool_get_cb_t getCb = nullptr);
    static void registerSettingString(const char* name, uint8_t maxLength, dmxnow_setting_string_set_cb_t setCb = nullptr, dmxnow_setting_string_get_cb_t getCb = nullptr);
private:
    //master stuff
    static uint8_t broadcastAddress[6];
    static uint16_t dmxnowTxSequence[DMX_UNIVERSES];
    static uint16_t dmxnowCtlSequence; // sequence counter for discovery/setter/getter packets

    static void addSlave(const dmxnow_discovery_response_t* response, int8_t rssi);
    static void deleteSlave(int index);
    static int findSlaveByMac(const uint8_t* macAddr);
    static void enqueueSend(const uint8_t* macAddr, const uint8_t* data, uint16_t size);
    static void processNextSend();
    static void ma_dataReceived(const esp_now_recv_info_t* info, const uint8_t *data, int len);
    static void ma_dataSent(const esp_now_send_info_t* tx_info, esp_now_send_status_t sendStatus);

    static void (*discoveryCallback)(const dmxnow_slave_t& slave);
    static void (*getterResponseCallback)(const uint8_t* macAddr, String name, String value);

    //slave stuff
    static void sl_responseDiscovery(uint8_t requestedMode);
    static void sl_responseSetter(const uint8_t* macAddr, const uint8_t *data, int len);
    static void sl_responseGetter(const uint8_t* macAddr, const uint8_t *data, int len);
    static dmxnow_slave_t mySlaveData;
    static uint16_t slaveTxSequence;
    static uint16_t slaveExpectedDmxSequence[DMX_UNIVERSES];
    static bool slaveDmxSequenceKnown[DMX_UNIVERSES];

    // named, typed settings registry
    typedef struct {
        dmxnow_setting_t desc;
        void* setCallback = nullptr;
        void* getCallback = nullptr;
    } dmxnow_setting_slot_t;
    static dmxnow_setting_slot_t mySettings[DMXNOW_MAX_SETTINGS];
    static uint8_t mySettingCount;
    static void registerSetting(const char* name, uint8_t type, int16_t min, int16_t max, void* setCb, void* getCb);
    static int findSettingByName(const char* name);
    static bool dispatchSetting(const uint8_t* macAddr, int slotIndex, const String& value); // parse+range-check+invoke; false if rejected
    static String readSettingValue(const uint8_t* macAddr, int slotIndex); // invoke getter, native type -> String

    static std::vector<dmxnow_slave_t> slaveArray;  //array of known slaves (master side)
    static void (*setterCallback)(const uint8_t* macAddr, String name, String valueP);
    static String (*getterCallback)(const uint8_t* macAddr, String name);
    static void (*dmxCallback)(uint8_t* data);

    //div. stuff
    static SendQueueElem sendQueue[SEND_QUEUE_SIZE];
    static uint8_t dmxBuf[DMX_UNIVERSES][DMX_BUFSIZE];// Stores the latest values of all universes
    static SemaphoreHandle_t dmxMutex;
    static bool isInitialized;

    static unsigned int rxSeqErrors; // diagnostic: number of DMX sequence gaps detected (slave side)
public:
    static unsigned int getRxSeqErrors() { return rxSeqErrors; };
};

#endif // DMXNOW_H
