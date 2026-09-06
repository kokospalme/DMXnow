#include "DMXnow.h"
#include <cstddef>
#include "esp_mac.h"

dmxnow_slave_t DMXnow::mySlaveData;
uint16_t DMXnow::slaveTxSequence = 0;
uint16_t DMXnow::slaveExpectedDmxSequence[DMX_UNIVERSES] = {0};
bool DMXnow::slaveDmxSequenceKnown[DMX_UNIVERSES] = {false};
unsigned int DMXnow::rxSeqErrors = 0;
void (*DMXnow::setterCallback)(const uint8_t* macAddr, String name, String valueP) = nullptr;
String (*DMXnow::getterCallback)(const uint8_t* macAddr, String name) = nullptr;
void (*DMXnow::dmxCallback)(uint8_t* data) = nullptr;
DMXnow::dmxnow_setting_slot_t DMXnow::mySettings[DMXNOW_MAX_SETTINGS];
uint8_t DMXnow::mySettingCount = 0;

// Arduino's String::toInt() returns 0 on invalid input with no way to tell that apart from an
// actual "0" - needed here since setting values must be validated, not silently misread.
static bool dmxnow_parseSignedInt(const String& value, long& out) {
    if (value.length() == 0) return false;
    char* endPtr = nullptr;
    long v = strtol(value.c_str(), &endPtr, 10);
    if (endPtr == value.c_str() || *endPtr != '\0') return false;
    out = v;
    return true;
}

// Non-legacy PHY rate so ESP-NOW negotiates protocol v2 (up to 1470 bytes/packet) with this peer,
// instead of falling back to the 250-byte v1 limit.
static esp_now_rate_config_t dmxnow_rate_config = {
    .phymode = WIFI_PHY_MODE_HT20,
    .rate = WIFI_PHY_RATE_MCS1_SGI,
    .ersu = false,
    .dcm = false
};

void DMXnow::initSlave(){
    if(isInitialized) return;  //return if already initialized
    dmxMutex = xSemaphoreCreateMutex();  // Create the mutex

    WiFi.mode(WIFI_STA);
    Serial.println("Initialisiere DMXnow[slave]...");
    esp_now_init();

    registerPeer(broadcastAddress);
    esp_now_register_recv_cb(sl_dataReceived);

    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        Serial.printf("My mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    baseMac[0], baseMac[1], baseMac[2],
                    baseMac[3], baseMac[4], baseMac[5]);
    } else {
        Serial.println("Failed to read MAC address");
    }

    Serial.println("");

    isInitialized = true;
}

void DMXnow::setSlaveconfig(const dmxnow_slave_t& config){
    mySlaveData.universe = config.universe;
    mySlaveData.dmxChannel = config.dmxChannel;
    memcpy(mySlaveData.slavename, config.slavename, sizeof(mySlaveData.slavename));
}

void DMXnow::setSlaveChannels(uint8_t mode, const char* modeName, uint16_t dmxCount, const dmxnow_channel_t* channels, uint8_t count, bool makeActive) {
    if (mode >= DMXNOW_MAX_MODES) {
        Serial.printf("DMXnow: mode %u exceeds DMXNOW_MAX_MODES (%u), ignored.\n", mode, DMXNOW_MAX_MODES);
        return;
    }
    if (count > DMXNOW_MAX_CHANNELS) {
        Serial.printf("DMXnow: %u channels declared, only %u fit in one discovery response - truncating.\n", count, DMXNOW_MAX_CHANNELS);
        count = DMXNOW_MAX_CHANNELS;
    }

    dmxnow_mode_info_t& slot = mySlaveData.modes[mode];
    slot.known = true;
    strncpy(slot.modeName, modeName, sizeof(slot.modeName) - 1);
    slot.modeName[sizeof(slot.modeName) - 1] = '\0';
    slot.dmxCount = dmxCount;
    slot.channelCount = count;
    memcpy(slot.channels, channels, count * sizeof(dmxnow_channel_t));

    if (mode + 1 > mySlaveData.modeCount) {
        mySlaveData.modeCount = mode + 1;
    }
    if (makeActive) {
        setActiveMode(mode);
    }
}

void DMXnow::setActiveMode(uint8_t mode) {
    if (mode >= DMXNOW_MAX_MODES || !mySlaveData.modes[mode].known) {
        Serial.printf("DMXnow: mode %u was never declared via setSlaveChannels(), ignoring setActiveMode().\n", mode);
        return;
    }
    mySlaveData.activeMode = mode;
}

void DMXnow::registerPeer(const uint8_t* macAddr){
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = 0;  // 0 = use the current channel
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(macAddr)) {
        esp_err_t addStatus = esp_now_add_peer(&peerInfo);
        if (addStatus != ESP_OK) {
            Serial.print("Error adding peer: ");
            Serial.println(addStatus);
            return;
        }
    }

    esp_err_t rateStatus = esp_now_set_peer_rate_config((uint8_t*) macAddr, &dmxnow_rate_config);
    if (rateStatus != ESP_OK) {
        Serial.printf("Failed to set ESP-NOW v2 rate for peer: %d\n", rateStatus);
    }
}

void DMXnow::deletePeer(const uint8_t* macAddr) {
    esp_err_t result = esp_now_del_peer(macAddr);
    if (result != ESP_OK) {
        Serial.print("error by deleting peer: ");
        Serial.println(result);
    }
}

/*
answer a discovery request with our identity plus one mode's capability map, in a single ESP-NOW
v2 packet. requestedMode is DMXNOW_MODE_CURRENT (describe whatever is active) or a specific,
previously-declared mode index - describing a mode does NOT switch the device into it.
*/
void DMXnow::sl_responseDiscovery(uint8_t requestedMode) {
    esp_read_mac(mySlaveData.macAddress, ESP_MAC_WIFI_STA);

    uint8_t primaryChannel = 0;
    wifi_second_chan_t secondChannel;
    if (esp_wifi_get_channel(&primaryChannel, &secondChannel) == ESP_OK) {
        mySlaveData.wifiChannel = primaryChannel;
    }

    uint8_t mode = (requestedMode == DMXNOW_MODE_CURRENT) ? mySlaveData.activeMode : requestedMode;
    if (mode >= DMXNOW_MAX_MODES || !mySlaveData.modes[mode].known) {
        mode = mySlaveData.activeMode; // requested mode was never declared - fall back to what we actually have
    }
    const dmxnow_mode_info_t& slot = mySlaveData.modes[mode];

    dmxnow_discovery_response_t pkt;
    pkt.hdr.type = DMXNOW_TYPE_DISCOVERY_RESPONSE;
    pkt.hdr.sequence = slaveTxSequence++;
    memcpy(pkt.macAddress, mySlaveData.macAddress, 6);
    pkt.wifiChannel = mySlaveData.wifiChannel;
    pkt.rssi = 0; // the receiver fills this in from its own radio metadata
    pkt.universe = mySlaveData.universe;
    pkt.dmxChannel = mySlaveData.dmxChannel;
    memcpy(pkt.slavename, mySlaveData.slavename, sizeof(pkt.slavename));

    pkt.mode = mode;
    pkt.activeMode = mySlaveData.activeMode;
    pkt.modeCount = mySlaveData.modeCount;
    memcpy(pkt.modeName, slot.modeName, sizeof(pkt.modeName));
    pkt.dmxCount = slot.dmxCount;
    pkt.channelCount = slot.channelCount;
    memcpy(pkt.channels, slot.channels, sizeof(pkt.channels));

    pkt.settingCount = mySettingCount;
    for (uint8_t i = 0; i < mySettingCount; i++) {
        pkt.settings[i] = mySettings[i].desc;
    }

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &pkt, sizeof(pkt));
    if (result == ESP_OK) {
        Serial.println("Discovery response sent to master");
    } else {
        Serial.print("Error sending discovery response: ");
        Serial.println(result);
    }
}

void DMXnow::sl_dataReceived(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < (int) sizeof(dmxnow_header_t)) return;
    const dmxnow_header_t* hdr = (const dmxnow_header_t*) data;
    if (hdr->magic != DMXNOW_MAGIC || hdr->version != DMXNOW_PROTOCOL_VERSION) return;

    switch (hdr->type) {
        case DMXNOW_TYPE_DMXDATA: {
            size_t fixedSize = offsetof(dmxnow_dmxdata_t, payload);
            if ((size_t) len < fixedSize) return;
            const dmxnow_dmxdata_t* pkt = (const dmxnow_dmxdata_t*) data;
            if (pkt->universe != mySlaveData.universe) return;
            // only full-buffer transmissions (offset 0, one universe per packet) are implemented for now
            if (pkt->offset != 0 || pkt->length < 1 || pkt->length > DMX_BUFSIZE_WITH_STARTCODE) return;
            if ((size_t) len < fixedSize + pkt->length) return; // truncated/malformed packet

            if (slaveDmxSequenceKnown[0] && !(hdr->flags & DMXNOW_FLAG_RESET) && hdr->sequence != slaveExpectedDmxSequence[0]) {
                rxSeqErrors++;
            }
            slaveExpectedDmxSequence[0] = (uint16_t)(hdr->sequence + 1);
            slaveDmxSequenceKnown[0] = true;

            uint16_t copyLen = pkt->length - 1; // minus the DMX start code
            if (copyLen > DMX_BUFSIZE) copyLen = DMX_BUFSIZE;
            memcpy(dmxBuf[0], &pkt->payload[1], copyLen);

            if (dmxCallback) {
                dmxCallback(dmxBuf[0]);
            }
            break;
        }

        case DMXNOW_TYPE_DISCOVERY_REQUEST: {
            if (len < (int) sizeof(dmxnow_discovery_request_t)) return;
            const dmxnow_discovery_request_t* req = (const dmxnow_discovery_request_t*) data;
            sl_responseDiscovery(req->mode);
            break;
        }

        case DMXNOW_TYPE_SETTER:
            if (len < (int) sizeof(dmxnow_setter_t)) return;
            sl_responseSetter(info->src_addr, data, len);
            break;

        case DMXNOW_TYPE_GETTER:
            if (len < (int) sizeof(dmxnow_getter_t)) return;
            sl_responseGetter(info->src_addr, data, len);
            break;

        default:
            break;
    }
}

void DMXnow::sl_responseSetter(const uint8_t* macAddr, const uint8_t* data, int len) {
    const dmxnow_setter_t* pkt = (const dmxnow_setter_t*) data;

    String _name(pkt->name);
    String _value(pkt->value);
    if (_name.length() == 0) {
        Serial.println("setter: empty name, ignored.");
        return;
    }

    Serial.printf("setter from %02x:%02x:%02x:%02x:%02x:%02x: %s:%s\n",
        macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5], _name.c_str(), _value.c_str());

    int slotIndex = findSettingByName(_name.c_str());
    if (slotIndex >= 0) {
        if (!dispatchSetting(macAddr, slotIndex, _value)) {
            Serial.printf("setter '%s': value '%s' rejected (not settable, wrong type, or out of range).\n", _name.c_str(), _value.c_str());
        }
        return;
    }

    // not a registered typed setting - fall back to the free-form callback, if any
    if (setterCallback) {
        setterCallback(macAddr, _name, _value);
    }
}

void DMXnow::sl_responseGetter(const uint8_t* macAddr, const uint8_t* data, int len) {
    const dmxnow_getter_t* pkt = (const dmxnow_getter_t*) data;
    String _name(pkt->name);
    String _value = "";

    int slotIndex = findSettingByName(_name.c_str());
    if (slotIndex >= 0 && (mySettings[slotIndex].desc.flags & DMXNOW_SETTING_FLAG_GET) && mySettings[slotIndex].getCallback) {
        _value = readSettingValue(macAddr, slotIndex);
    } else if (getterCallback) {
        _value = getterCallback(macAddr, _name);
    }

    registerPeer(macAddr);

    dmxnow_getter_response_t resp;
    resp.hdr.type = DMXNOW_TYPE_GETTER_RESPONSE;
    resp.hdr.sequence = slaveTxSequence++;
    _name.toCharArray(resp.name, sizeof(resp.name));
    _value.toCharArray(resp.value, sizeof(resp.value));

    esp_err_t result = esp_now_send(macAddr, (uint8_t*) &resp, sizeof(resp));
    if (result != ESP_OK) {
        Serial.print("Error sending getter response: ");
        Serial.println(result);
    }
}

void DMXnow::setSetterCallback(void (*fptr)(const uint8_t* macAddr, String name, String value)) {
    setterCallback = fptr;
}

void DMXnow::setGetterCallback(String (*fptr)(const uint8_t* macAddr, String name)) {
    getterCallback = fptr;
}

void DMXnow::setDmxCallback(void (*fptr)(uint8_t* data)) {
    dmxCallback = fptr;
}

void DMXnow::registerSetting(const char* name, uint8_t type, int16_t min, int16_t max, void* setCb, void* getCb) {
    if (mySettingCount >= DMXNOW_MAX_SETTINGS) {
        Serial.printf("DMXnow: setting '%s' exceeds DMXNOW_MAX_SETTINGS (%u), ignored.\n", name, DMXNOW_MAX_SETTINGS);
        return;
    }
    if (!setCb && !getCb) {
        Serial.printf("DMXnow: setting '%s' registered with neither a set nor a get callback, ignored.\n", name);
        return;
    }

    dmxnow_setting_slot_t& slot = mySettings[mySettingCount];
    strncpy(slot.desc.name, name, sizeof(slot.desc.name) - 1);
    slot.desc.name[sizeof(slot.desc.name) - 1] = '\0';
    slot.desc.type = type;
    slot.desc.min = min;
    slot.desc.max = max;
    slot.desc.flags = (setCb ? DMXNOW_SETTING_FLAG_SET : 0) | (getCb ? DMXNOW_SETTING_FLAG_GET : 0);
    slot.setCallback = setCb;
    slot.getCallback = getCb;
    mySettingCount++;
}

int DMXnow::findSettingByName(const char* name) {
    for (uint8_t i = 0; i < mySettingCount; i++) {
        if (strcmp(mySettings[i].desc.name, name) == 0) return i;
    }
    return -1;
}

bool DMXnow::dispatchSetting(const uint8_t* macAddr, int slotIndex, const String& value) {
    dmxnow_setting_slot_t& slot = mySettings[slotIndex];
    if (!(slot.desc.flags & DMXNOW_SETTING_FLAG_SET) || !slot.setCallback) return false;

    long v = 0;
    switch (slot.desc.type) {
        case DMXNOW_SETTING_UINT8:
        case DMXNOW_SETTING_UINT16:
        case DMXNOW_SETTING_INT8:
        case DMXNOW_SETTING_INT16:
            if (!dmxnow_parseSignedInt(value, v)) return false;
            if (v < slot.desc.min || v > slot.desc.max) return false;
            break;

        case DMXNOW_SETTING_BOOL:
            if (value == "1" || value.equalsIgnoreCase("true")) v = 1;
            else if (value == "0" || value.equalsIgnoreCase("false")) v = 0;
            else return false;
            break;

        case DMXNOW_SETTING_STRING:
            if ((int) value.length() > slot.desc.max) return false; // max = max length for STRING
            break;

        default:
            return false;
    }

    switch (slot.desc.type) {
        case DMXNOW_SETTING_UINT8:  ((dmxnow_setting_uint8_set_cb_t) slot.setCallback)(macAddr, (uint8_t) v); break;
        case DMXNOW_SETTING_UINT16: ((dmxnow_setting_uint16_set_cb_t) slot.setCallback)(macAddr, (uint16_t) v); break;
        case DMXNOW_SETTING_INT8:   ((dmxnow_setting_int8_set_cb_t) slot.setCallback)(macAddr, (int8_t) v); break;
        case DMXNOW_SETTING_INT16:  ((dmxnow_setting_int16_set_cb_t) slot.setCallback)(macAddr, (int16_t) v); break;
        case DMXNOW_SETTING_BOOL:   ((dmxnow_setting_bool_set_cb_t) slot.setCallback)(macAddr, v != 0); break;
        case DMXNOW_SETTING_STRING: ((dmxnow_setting_string_set_cb_t) slot.setCallback)(macAddr, value); break;
    }
    return true;
}

String DMXnow::readSettingValue(const uint8_t* macAddr, int slotIndex) {
    dmxnow_setting_slot_t& slot = mySettings[slotIndex];
    switch (slot.desc.type) {
        case DMXNOW_SETTING_UINT8:  return String(((dmxnow_setting_uint8_get_cb_t) slot.getCallback)(macAddr));
        case DMXNOW_SETTING_UINT16: return String(((dmxnow_setting_uint16_get_cb_t) slot.getCallback)(macAddr));
        case DMXNOW_SETTING_INT8:   return String(((dmxnow_setting_int8_get_cb_t) slot.getCallback)(macAddr));
        case DMXNOW_SETTING_INT16:  return String(((dmxnow_setting_int16_get_cb_t) slot.getCallback)(macAddr));
        case DMXNOW_SETTING_BOOL:   return ((dmxnow_setting_bool_get_cb_t) slot.getCallback)(macAddr) ? "1" : "0";
        case DMXNOW_SETTING_STRING: return ((dmxnow_setting_string_get_cb_t) slot.getCallback)(macAddr);
        default:                    return "";
    }
}

void DMXnow::registerSettingUint8(const char* name, uint8_t min, uint8_t max, dmxnow_setting_uint8_set_cb_t setCb, dmxnow_setting_uint8_get_cb_t getCb) {
    registerSetting(name, DMXNOW_SETTING_UINT8, min, max, (void*) setCb, (void*) getCb);
}

void DMXnow::registerSettingUint16(const char* name, uint16_t min, uint16_t max, dmxnow_setting_uint16_set_cb_t setCb, dmxnow_setting_uint16_get_cb_t getCb) {
    registerSetting(name, DMXNOW_SETTING_UINT16, min, max, (void*) setCb, (void*) getCb);
}

void DMXnow::registerSettingInt8(const char* name, int8_t min, int8_t max, dmxnow_setting_int8_set_cb_t setCb, dmxnow_setting_int8_get_cb_t getCb) {
    registerSetting(name, DMXNOW_SETTING_INT8, min, max, (void*) setCb, (void*) getCb);
}

void DMXnow::registerSettingInt16(const char* name, int16_t min, int16_t max, dmxnow_setting_int16_set_cb_t setCb, dmxnow_setting_int16_get_cb_t getCb) {
    registerSetting(name, DMXNOW_SETTING_INT16, min, max, (void*) setCb, (void*) getCb);
}

void DMXnow::registerSettingBool(const char* name, dmxnow_setting_bool_set_cb_t setCb, dmxnow_setting_bool_get_cb_t getCb) {
    registerSetting(name, DMXNOW_SETTING_BOOL, 0, 1, (void*) setCb, (void*) getCb);
}

void DMXnow::registerSettingString(const char* name, uint8_t maxLength, dmxnow_setting_string_set_cb_t setCb, dmxnow_setting_string_get_cb_t getCb) {
    if (maxLength >= SETTER_VALUE_LENGTH) {
        Serial.printf("DMXnow: setting '%s' maxLength %u exceeds SETTER_VALUE_LENGTH (%u), clamped.\n", name, maxLength, SETTER_VALUE_LENGTH - 1);
        maxLength = SETTER_VALUE_LENGTH - 1;
    }
    registerSetting(name, DMXNOW_SETTING_STRING, 0, maxLength, (void*) setCb, (void*) getCb);
}
