#include "DMXnow.h"

uint8_t DMXnow::broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint16_t DMXnow::dmxnowTxSequence[DMX_UNIVERSES] = {0};
uint16_t DMXnow::dmxnowCtlSequence = 0;
std::vector<dmxnow_slave_t> DMXnow::slaveArray;
SendQueueElem DMXnow::sendQueue[SEND_QUEUE_SIZE];
uint8_t DMXnow::dmxBuf[DMX_UNIVERSES][DMX_BUFSIZE];
SemaphoreHandle_t DMXnow::dmxMutex = NULL;
bool DMXnow::isInitialized = false;
void (*DMXnow::discoveryCallback)(const dmxnow_slave_t& slave) = nullptr;
void (*DMXnow::getterResponseCallback)(const uint8_t* macAddr, String name, String value) = nullptr;

void DMXnow::init() {
    if(isInitialized) return;  //return if already initialized
    dmxMutex = xSemaphoreCreateMutex();  // Create the mutex
    WiFi.mode(WIFI_STA);
    // Serial.println("initialize DMXnow [Master]");
    esp_now_init();

    registerPeer(broadcastAddress);

    esp_now_register_recv_cb(ma_dataReceived);
    esp_now_register_send_cb(ma_dataSent);

    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        Serial.printf("My mac: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    baseMac[0], baseMac[1], baseMac[2],
                    baseMac[3], baseMac[4], baseMac[5]);
    } else {
        Serial.println("Failed to read MAC address");
        return;
    }
    Serial.println("");
    isInitialized = true;
}

void DMXnow::pushDMXData(uint8_t universe, uint16_t length, uint8_t sequence, uint8_t* data, bool send) {
    if(length < DMX_BUFSIZE){
        Serial.println("only full universes for now. party not fully implemented");
        return;
    }
    if(universe < 1 || universe > DMX_UNIVERSES){
        Serial.printf("only universe 1...%u are implemented for now.\n", DMX_UNIVERSES);
        return;
    }

    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        memcpy(dmxBuf[universe-1], data, DMX_BUFSIZE);
        xSemaphoreGive(dmxMutex);
    }

    if(send){
        dmxnow_dmxdata_t pkt;
        pkt.hdr.type = DMXNOW_TYPE_DMXDATA;
        pkt.hdr.sequence = dmxnowTxSequence[universe-1]++;
        pkt.hdr.flags = DMXNOW_FLAG_NEW;
        pkt.universe = universe;
        pkt.offset = 0;
        pkt.length = DMX_BUFSIZE_WITH_STARTCODE;
        pkt.payload[0] = 0; // DMX start code
        memcpy(&pkt.payload[1], data, DMX_BUFSIZE);

        // one single ESP-NOW v2 packet carries the whole universe - no more fragmenting into parts
        enqueueSend(broadcastAddress, (uint8_t*) &pkt, sizeof(pkt));
        processNextSend();
    }
}

void DMXnow::enqueueSend(const uint8_t* macAddr, const uint8_t* data, uint16_t size) {
    if (size > DMXNOW_MAX_PACKET_SIZE) {
        Serial.printf("DMXnow: packet too large (%u > %u), dropped.\n", size, DMXNOW_MAX_PACKET_SIZE);
        return;
    }
    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        bool queued = false;
        for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
            if (!sendQueue[i].toBeSent) {
                memcpy(sendQueue[i].macAddr, macAddr, 6);
                sendQueue[i].size = size;
                memcpy(sendQueue[i].data, data, size);
                sendQueue[i].toBeSent = 1;
                queued = true;
                break;
            }
        }
        if (!queued) {
            Serial.println("DMXnow: send queue is full, packet dropped.");
        }
        xSemaphoreGive(dmxMutex);
    }
}

void DMXnow::processNextSend() {
    if (xSemaphoreTake(dmxMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
            if (sendQueue[i].toBeSent) {
                esp_now_send(sendQueue[i].macAddr, sendQueue[i].data, sendQueue[i].size);

                bool isBroadcast = (memcmp(sendQueue[i].macAddr, broadcastAddress, 6) == 0);
                if (!isBroadcast) deletePeer(sendQueue[i].macAddr);

                memset(&(sendQueue[i]), 0, sizeof(SendQueueElem));
                xSemaphoreGive(dmxMutex);
                return; // next packet is triggered from the send-callback
            }
        }
    }
    xSemaphoreGive(dmxMutex);
}

/*
broadcast a discovery request; every slave that hears it answers describing its active mode
*/
void DMXnow::sendSlaveRequest() {
    dmxnow_discovery_request_t pkt;
    pkt.hdr.type = DMXNOW_TYPE_DISCOVERY_REQUEST;
    pkt.hdr.sequence = dmxnowCtlSequence++;
    pkt.mode = DMXNOW_MODE_CURRENT;
    enqueueSend(broadcastAddress, (uint8_t*) &pkt, sizeof(pkt));
    processNextSend();
}

/*
ask one already-known slave to describe a specific mode (0..modeCount-1), without switching its
live mode - lets the master build a full per-device/per-mode profile after an initial discovery.
*/
void DMXnow::sendSlaveModeRequest(const uint8_t *macAddr, uint8_t mode) {
    int _slave = findSlaveByMac(macAddr);
    if (_slave < 0) {
        Serial.println("no slave found.");
        return;
    }

    registerPeer(macAddr);

    dmxnow_discovery_request_t pkt;
    pkt.hdr.type = DMXNOW_TYPE_DISCOVERY_REQUEST;
    pkt.hdr.sequence = dmxnowCtlSequence++;
    pkt.mode = mode;
    enqueueSend(macAddr, (uint8_t*) &pkt, sizeof(pkt));
    processNextSend();
}

void DMXnow::sendSlaveSetter(const uint8_t *macAddr, String name, String value) {
    int _slave = findSlaveByMac(macAddr);
    if (_slave < 0) {
        Serial.println("no slave found.");
        return;
    }
    if (name.length() >= SETTER_NAME_LENGTH) {
        Serial.println("setter name too long");
        return;
    }
    if (value.length() >= SETTER_VALUE_LENGTH) {
        Serial.println("value too long");
        return;
    }

    registerPeer(macAddr);

    dmxnow_setter_t pkt;
    pkt.hdr.type = DMXNOW_TYPE_SETTER;
    pkt.hdr.sequence = dmxnowCtlSequence++;
    name.toCharArray(pkt.name, sizeof(pkt.name));
    value.toCharArray(pkt.value, sizeof(pkt.value));

    enqueueSend(macAddr, (uint8_t*) &pkt, sizeof(pkt));
    processNextSend();
}

void DMXnow::sendSlaveGetter(const uint8_t *macAddr, String name) {
    int _slave = findSlaveByMac(macAddr);
    if (_slave < 0) {
        Serial.println("no slave found.");
        return;
    }
    if (name.length() >= SETTER_NAME_LENGTH) {
        Serial.println("getter name too long");
        return;
    }

    registerPeer(macAddr);

    dmxnow_getter_t pkt;
    pkt.hdr.type = DMXNOW_TYPE_GETTER;
    pkt.hdr.sequence = dmxnowCtlSequence++;
    name.toCharArray(pkt.name, sizeof(pkt.name));

    enqueueSend(macAddr, (uint8_t*) &pkt, sizeof(pkt));
    processNextSend();
}

void DMXnow::setDiscoveryCallback(void (*fptr)(const dmxnow_slave_t& slave)) {
    discoveryCallback = fptr;
}

void DMXnow::setGetterResponseCallback(void (*fptr)(const uint8_t* macAddr, String name, String value)) {
    getterResponseCallback = fptr;
}

int DMXnow::getSlaveCount() {
    return (int) slaveArray.size();
}

const dmxnow_slave_t* DMXnow::getSlave(int index) {
    if (index < 0 || index >= (int) slaveArray.size()) return nullptr;
    return &slaveArray[index];
}

const dmxnow_slave_t* DMXnow::getSlaveByMac(const uint8_t* macAddr) {
    int _slave = findSlaveByMac(macAddr);
    if (_slave < 0) return nullptr;
    return &slaveArray[_slave];
}

/*
receive data from a slave (discovery response or getter response)
*/
void DMXnow::ma_dataReceived(const esp_now_recv_info_t* info, const uint8_t *data, int len) {
    if (len < (int) sizeof(dmxnow_header_t)) {
        return;
    }
    const dmxnow_header_t* hdr = (const dmxnow_header_t*) data;
    if (hdr->magic != DMXNOW_MAGIC || hdr->version != DMXNOW_PROTOCOL_VERSION) {
        return; // not a (compatible) DMXnow packet, ignore
    }

    switch (hdr->type) {
        case DMXNOW_TYPE_DISCOVERY_RESPONSE: {
            if (len < (int) sizeof(dmxnow_discovery_response_t)) {
                Serial.println("discovery response: packet size mismatch");
                return;
            }
            const dmxnow_discovery_response_t* response = (const dmxnow_discovery_response_t*) data;
            int8_t rssi = (info && info->rx_ctrl) ? info->rx_ctrl->rssi : 0;
            addSlave(response, rssi);
            break;
        }

        case DMXNOW_TYPE_GETTER_RESPONSE: {
            if (len < (int) sizeof(dmxnow_getter_response_t)) {
                Serial.println("getter response: packet size mismatch");
                return;
            }
            const dmxnow_getter_response_t* response = (const dmxnow_getter_response_t*) data;
            if (getterResponseCallback) {
                getterResponseCallback(info->src_addr, String(response->name), String(response->value));
            }
            break;
        }

        default:
            // masters don't expect any other packet type from a slave
            break;
    }
}

void DMXnow::ma_dataSent(const esp_now_send_info_t* tx_info, esp_now_send_status_t sendStatus) {
  switch (sendStatus)
  {
    case ESP_NOW_SEND_SUCCESS:
      processNextSend();
      break;

    case ESP_NOW_SEND_FAIL:
      memset(sendQueue, 0, SEND_QUEUE_SIZE*sizeof(SendQueueElem));
      break;

    default:
      break;
  }
}

void DMXnow::addSlave(const dmxnow_discovery_response_t* response, int8_t rssi) {
    if (response->mode >= DMXNOW_MAX_MODES) {
        Serial.printf("DMXnow: discovery response for mode %u exceeds DMXNOW_MAX_MODES (%u), ignored.\n", response->mode, DMXNOW_MAX_MODES);
        return;
    }

    // a response only ever describes ONE mode - merge it into the slave's record instead of
    // replacing it wholesale, so modes learned earlier (via sendSlaveModeRequest) aren't lost.
    int _slave = findSlaveByMac(response->macAddress);
    if (_slave == -1) {
        dmxnow_slave_t entry;
        memcpy(entry.macAddress, response->macAddress, 6);
        slaveArray.push_back(entry);
        _slave = (int) slaveArray.size() - 1;
        Serial.println("slave added.");
    } else {
        Serial.println("known slave updated.");
    }

    dmxnow_slave_t& entry = slaveArray[_slave];
    entry.wifiChannel = response->wifiChannel;
    entry.rssi = rssi;
    entry.universe = response->universe;
    entry.dmxChannel = response->dmxChannel;
    memcpy(entry.slavename, response->slavename, sizeof(entry.slavename));
    entry.activeMode = response->activeMode;
    entry.modeCount = response->modeCount;
    entry.lastSeenMillis = millis();

    // settings are mode-independent - every response carries the full current list
    entry.settingCount = response->settingCount;
    memcpy(entry.settings, response->settings, sizeof(entry.settings));

    dmxnow_mode_info_t& modeInfo = entry.modes[response->mode];
    modeInfo.known = true;
    memcpy(modeInfo.modeName, response->modeName, sizeof(modeInfo.modeName));
    modeInfo.dmxCount = response->dmxCount;
    modeInfo.channelCount = response->channelCount;
    memcpy(modeInfo.channels, response->channels, sizeof(modeInfo.channels));

    Serial.printf("***** slave (%02X:%02X:%02X:%02X:%02X:%02X) [universe %u, ch %u..%u] mode %u/%u '%s' (%u functions) *****\n",
        entry.macAddress[0], entry.macAddress[1], entry.macAddress[2], entry.macAddress[3], entry.macAddress[4], entry.macAddress[5],
        entry.universe, entry.dmxChannel, entry.dmxChannel + modeInfo.dmxCount - 1,
        response->mode, entry.modeCount, modeInfo.modeName, modeInfo.channelCount);

    if (discoveryCallback) {
        discoveryCallback(entry);
    }
}

void DMXnow::deleteSlave(int index) {
    if (index >= 0 && index < (int) slaveArray.size()) {
        slaveArray.erase(slaveArray.begin() + index);
    }
}

int DMXnow::findSlaveByMac(const uint8_t* macAddr) {
    int _arraysize = (int) slaveArray.size();
    for (int i = 0; i < _arraysize; i++) {
        if (memcmp(slaveArray[i].macAddress, macAddr, 6) == 0) {
            return i;
        }
    }
    return -1;
}
