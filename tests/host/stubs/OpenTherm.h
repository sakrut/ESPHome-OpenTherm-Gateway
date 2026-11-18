#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#define IRAM_ATTR

enum OpenThermResponseStatus
{
  NONE,
  SUCCESS,
  INVALID,
  TIMEOUT
};

enum OpenThermMessageType
{
  READ_DATA = 0,
  READ = READ_DATA,
  WRITE_DATA = 1,
  WRITE = WRITE_DATA,
  INVALID_DATA = 2,
  RESERVED = 3,
  READ_ACK = 4,
  WRITE_ACK = 5,
  DATA_INVALID = 6,
  UNKNOWN_DATA_ID = 7
};
typedef OpenThermMessageType OpenThermRequestType;

enum OpenThermMessageID
{
  Status = 0,
  TSet = 1,
  Command = 4,
  ASFflags = 5,
  TSPindexTSPvalue = 11,
  MaxRelModLevelSetting = 14,
  TrSet = 16,
  RelModLevel = 17,
  CHPressure = 18,
  Tr = 24,
  Tboiler = 25,
  Tdhw = 26,
  Toutside = 27,
  Tret = 28,
  Texhaust = 33,
  TdhwSet = 56,
  MaxTSet = 57,
  OEMDiagnosticCode = 115,
  OpenThermVersionMaster = 124,
  OpenThermVersionSlave = 125,
};

/// Fake bus: frames the gateway sends to the boiler and to the thermostat.
struct FakeBus
{
  std::vector<unsigned long> to_boiler;
  std::vector<unsigned long> to_thermostat;
  std::map<uint8_t, uint8_t> tsp;
  std::function<unsigned long(unsigned long)> boiler;

  void clear()
  {
    to_boiler.clear();
    to_thermostat.clear();
  }
};
inline FakeBus bus;

class OpenTherm
{
public:
  OpenTherm(int, int, bool is_slave = false) : is_slave_(is_slave) {}

  void begin(void (*)(void)) {}
  void begin(void (*)(void), void (*)(unsigned long, OpenThermResponseStatus)) {}
  bool isReady() { return true; }
  void process() {}
  void handleInterrupt() {}

  bool sendResponse(unsigned long response)
  {
    bus.to_thermostat.push_back(response);
    return true;
  }

  unsigned long sendRequest(unsigned long request)
  {
    bus.to_boiler.push_back(request);
    if (bus.boiler)
      return bus.boiler(request);
    uint8_t id = getDataID(request);
    uint16_t data = request & 0xFFFF;
    if (id == TSPindexTSPvalue)
    {
      uint8_t index = data >> 8;
      if (getMessageType(request) == WRITE_DATA)
        bus.tsp[index] = data & 0xFF;
      else
        data = (index << 8) | bus.tsp[index];
    }
    return buildResponse(getMessageType(request) == WRITE_DATA ? WRITE_ACK : READ_ACK, (OpenThermMessageID)id, data);
  }

  static bool parity(unsigned long frame)
  {
    return __builtin_popcountl(frame) & 1;
  }

  static OpenThermMessageType getMessageType(unsigned long message)
  {
    return static_cast<OpenThermMessageType>((message >> 28) & 7);
  }

  static OpenThermMessageID getDataID(unsigned long frame)
  {
    return (OpenThermMessageID)((frame >> 16) & 0xFF);
  }

  static unsigned long buildRequest(OpenThermMessageType type, OpenThermMessageID id, unsigned int data)
  {
    return buildResponse(type == WRITE_DATA ? WRITE_DATA : READ_DATA, id, data);
  }

  static unsigned long buildResponse(OpenThermMessageType type, OpenThermMessageID id, unsigned int data)
  {
    unsigned long frame = data | ((unsigned long)type << 28) | ((unsigned long)id << 16);
    return parity(frame) ? frame | (1ul << 31) : frame;
  }

  static bool isValidResponse(unsigned long response)
  {
    return !parity(response) && (getMessageType(response) == READ_ACK || getMessageType(response) == WRITE_ACK);
  }

  static bool isFault(unsigned long response) { return response & 0x1; }
  static bool isCentralHeatingActive(unsigned long response) { return response & 0x2; }
  static bool isHotWaterActive(unsigned long response) { return response & 0x4; }
  static bool isFlameOn(unsigned long response) { return response & 0x8; }
  static bool isDiagnostic(unsigned long response) { return response & 0x40; }
  static uint16_t getUInt(unsigned long response) { return response & 0xFFFF; }
  static float getFloat(unsigned long response) { return static_cast<int16_t>(getUInt(response)) / 256.0f; }
  static unsigned int temperatureToData(float temperature) { return static_cast<unsigned int>(temperature * 256); }
  static const char *statusToString(OpenThermResponseStatus status) { return status == TIMEOUT ? "TIMEOUT" : "INVALID"; }

private:
  bool is_slave_;
};
