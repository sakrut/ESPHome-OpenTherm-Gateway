#include "esphome.h"
#include "esphome/components/sensor/sensor.h"
#include "OpenTherm.h"
#include "opentherm_switch.h"
#include "opentherm_climate.h"
#include "opentherm_binary.h"
#include "opentherm_output.h"

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

public:
  Switch *thermostatSwitch = new OpenthermSwitch();
  Sensor *external_temperature_sensor = new Sensor();
  Sensor *return_temperature_sensor = new Sensor();
  Sensor *boiler_temperature = new Sensor();
  Sensor *pressure_sensor = new Sensor();
  Sensor *modulation_sensor = new Sensor();
  Sensor *heating_target_temperature_sensor = new Sensor();
  OpenthermClimate *hotWaterClimate = new OpenthermClimate();
  OpenthermClimate *heatingWaterClimate = new OpenthermClimate();
  BinarySensor *flame = new OpenthermBinarySensor();
  
  OpenthermComponent(): PollingComponent(60000) {
  }
  
  void setup() override {
    // This will be called once to set up the component
    // think of it as the setup() call in Arduino
      ESP_LOGD("opentherm_component", "Setup");

      ot.begin(handleInterrupt);
      sOT.begin(sHandleInterrupt, processRequest);

      thermostatSwitch->add_on_state_callback([=](bool state) -> void {
        ESP_LOGD ("opentherm_component", "termostatSwitch_on_state_callback %d", state);    
      });

      hotWaterClimate->set_temperature_settings(5, 6);
      heatingWaterClimate->set_temperature_settings(0, 0);
      hotWaterClimate->setup();
      heatingWaterClimate->setup();
  }

  float getExternalTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }
  float getHeatingTargetTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TSet, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  float getReturnTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tret, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }
  
  float getHotWaterTargetTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TdhwSet, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }
  float getHotWaterTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tdhw, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  float getRoomTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tr, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  bool setHotWaterTemperature(float temperature) {
	    unsigned int data = ot.temperatureToData(temperature);
      unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, data);
      unsigned long response = ot.sendRequest(request);
      return ot.isValidResponse(response);
  }

  float getModulation() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::RelModLevel, 0));
    return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  float getPressure() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::CHPressure, 0));
    return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  void update() override {

    ESP_LOGD("opentherm_component", "update heatingWaterClimate: %i", heatingWaterClimate->mode);
    ESP_LOGD("opentherm_component", "update hotWaterClimate: %i", hotWaterClimate->mode);
    
    bool enableCentralHeating = heatingWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT;
    bool enableHotWater = hotWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT;
    bool enableCooling = false; // this boiler is for heating only

    
    //Set/Get Boiler Status
    auto response = ot.setBoilerStatus(enableCentralHeating, enableHotWater, enableCooling);
    bool isFlameOn = ot.isFlameOn(response);
    bool isCentralHeatingActive = ot.isCentralHeatingActive(response);
    bool isHotWaterActive = ot.isHotWaterActive(response);
    float return_temperature = getReturnTemperature();
    float hotWater_temperature = getHotWaterTemperature();

  
    

    // Set hot water temperature
    /////setHotWaterTemperature(hotWaterClimate->target_temperature);

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
    heatingWaterClimate->action = isCentralHeatingActive && isFlameOn ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
    hotWaterClimate->target_temperature = getHeatingTargetTemperature();
    heatingWaterClimate->publish_state();
  }

  
  static void processRequest(unsigned long request, OpenThermResponseStatus status) {
    const byte msgType = (request << 1) >> 29;
    const int dataId = (request >> 16) & 0xFF;
    
    unsigned long _lastRresponse = ot.sendRequest(request);
    sOT.sendResponse(_lastRresponse);

  }

  void loop() override {
    sOT.process();
  }
};