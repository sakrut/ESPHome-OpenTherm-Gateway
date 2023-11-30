#include "esphome.h"
#include "esphome/components/sensor/sensor.h"
#include "OpenTherm.h"
#include "opentherm_climate.h"

// Pins to OpenTherm Adapter
int inPin = 4; 
int outPin = 5;
int sInPin = 12; 
int sOutPin = 13;

OpenTherm ot(inPin, outPin, false);
OpenTherm sOT(sInPin, sOutPin, true);
void processRequest(unsigned long request, OpenThermResponseStatus status);

IRAM_ATTR void handleInterrupt() {
	ot.handleInterrupt();
}

IRAM_ATTR void sHandleInterrupt() {
  sOT.handleInterrupt();
}

class OpenthermComponent: public PollingComponent {
private:
  const char *TAG = "opentherm_component";
  static unsigned long  _lastStatusResponse;
public:
  Sensor *external_temperature_sensor = new Sensor();
  Sensor *return_temperature_sensor = new Sensor();
  Sensor *boiler_temperature = new Sensor();
  Sensor *pressure_sensor = new Sensor();
  Sensor *modulation_sensor = new Sensor();
  Sensor *heating_target_temperature_sensor = new Sensor();
  OpenthermClimate *hotWaterClimate = new OpenthermClimate();
  OpenthermClimate *heatingWaterClimate = new OpenthermClimate();
  BinarySensor *flame = new BinarySensor();
  
  OpenthermComponent(): PollingComponent(30000) {
  }
  
  void setup() override {
    // This will be called once to set up the component
    // think of it as the setup() call in Arduino
      ESP_LOGD("opentherm_component", "Setup");

      ot.begin(handleInterrupt);
      sOT.begin(sHandleInterrupt, processRequest);

      hotWaterClimate->setTargetTemperatureSetter([this](float temperature) {
            this->setHotWaterTemperature(temperature);
        });
      hotWaterClimate->setup();
      
      heatingWaterClimate->setTargetTemperatureSetter([this](float temperature) {
            this->setHeatingTargetTemperature(temperature);
        });
      heatingWaterClimate->setup();
  }

  float getExternalTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : NAN;
  }
  float getHeatingTargetTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TSet, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : NAN;
  }

  float getReturnTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tret, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : NAN;
  }
  
  float getHotWaterTargetTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TdhwSet, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : NAN;
  }
  float getHotWaterTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tdhw, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : NAN;
  }

  float getRoomTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tr, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : NAN;
  }

  bool setHotWaterTemperature(float temperature) {
      unsigned int data = ot.temperatureToData(temperature);
      unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, data);
      unsigned long response = ot.sendRequest(request);
      return ot.isValidResponse(response);
  }

  bool setHeatingTargetTemperature(float temperature) {
      unsigned int data = ot.temperatureToData(temperature);
      unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TSet, data);
      unsigned long response = ot.sendRequest(request);
      return ot.isValidResponse(response);
  }

  float getModulation() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::RelModLevel, 0));
    return ot.isValidResponse(response) ? ot.getFloat(response) : NAN;
  }

  float getPressure() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::CHPressure, 0));
    return ot.isValidResponse(response) ? ot.getFloat(response) : NAN;
  }

  void update() override {
    
    //Set/Get Boiler Status
    //auto response = ot.setBoilerStatus(true, true, false);
    bool isFlameOn = ot.isFlameOn(_lastStatusResponse);
    bool isCentralHeatingActive = ot.isCentralHeatingActive(_lastStatusResponse);
    bool isHotWaterActive = ot.isHotWaterActive(_lastStatusResponse);
    float return_temperature = getReturnTemperature();
    float hotWater_temperature = getHotWaterTemperature();


    float boilerTemperature = ot.getBoilerTemperature();
    float ext_temperature = getExternalTemperature();
    float pressure = getPressure();
    float modulation = getModulation();

    // Publish sensor values
    flame->publish_state(isFlameOn); 
    external_temperature_sensor->publish_state(ext_temperature);
    return_temperature_sensor->publish_state(return_temperature);
    boiler_temperature->publish_state(boilerTemperature);
    pressure_sensor->publish_state(pressure);
    modulation_sensor->publish_state(modulation);
    ESP_LOGI("opentherm_component", "Room: %i", getRoomTemperature());
    
    heating_target_temperature_sensor->publish_state(hotWaterClimate->target_temperature);

    // Publish status of thermostat that controls hot water
    hotWaterClimate->current_temperature = hotWater_temperature;
    hotWaterClimate->action = isHotWaterActive ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
    hotWaterClimate->target_temperature = getHotWaterTargetTemperature();
    hotWaterClimate->publish_state();
    
    // Publish status of thermostat that controls heating
    heatingWaterClimate->current_temperature = boilerTemperature;
    heatingWaterClimate->action = isCentralHeatingActive ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
    heatingWaterClimate->target_temperature = getHeatingTargetTemperature();
    heatingWaterClimate->publish_state();
  }

  
  static void processRequest(unsigned long request, OpenThermResponseStatus status) {
  
    unsigned long _lastRresponse = ot.sendRequest(request);
    sOT.sendResponse(_lastRresponse);

    OpenThermMessageID id = ot.getDataID(request);
    uint16_t data = ot.getUInt(request);
    float f = ot.getFloat(request);
    switch(id)
    {
      case OpenThermMessageID::Status:
      {
        _lastStatusResponse = _lastRresponse;
        ESP_LOGI("opentherm_component", "lastStatusResponse: %i", _lastStatusResponse);
        break;
      }
      case OpenThermMessageID::TSet:
      {
        break;
      }
      case OpenThermMessageID::Tboiler:
      {
        break;
      }
      default:
      {
      }
    }

    
  }

  void loop() override {
    sOT.process();
  }
};

unsigned long OpenthermComponent::_lastStatusResponse = 0;