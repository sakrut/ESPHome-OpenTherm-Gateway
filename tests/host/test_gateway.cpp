#include "opentherm_component.h"

#include <cstdio>

using namespace esphome;
using namespace esphome::opentherm;

static int failures = 0;

#define CHECK(cond)                                                \
  do                                                               \
  {                                                                \
    if (!(cond))                                                   \
    {                                                              \
      std::printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
      failures++;                                                  \
    }                                                              \
  } while (0)

static unsigned long frame(OpenThermMessageType type, uint8_t id, uint16_t data)
{
  return OpenTherm::buildResponse(type, static_cast<OpenThermMessageID>(id), data);
}

struct Gateway
{
  OpenthermComponent component{30000};
  sensor::Sensor rejected;

  Gateway()
  {
    bus = FakeBus{};
    climate::fake_restore.reset();
    fake_preferences.clear();
    fake_millis = 10000;
    component.set_rejected_frames_sensor(&rejected);
  }

  void start() { component.setup(); }

  // Relays one thermostat frame; returns the frames sent to the boiler
  std::vector<unsigned long> relay(unsigned long request)
  {
    bus.clear();
    fake_millis += 1000;
    OpenthermComponent::processRequest(request, OpenThermResponseStatus::SUCCESS);
    return bus.to_boiler;
  }

  bool forwards(unsigned long request) { return relay(request) == std::vector<unsigned long>{request}; }

  float rejectedCount()
  {
    component.update();
    return rejected.state;
  }
};

static void normal_traffic_is_never_rejected()
{
  Gateway gw;
  gw.start();
  const std::vector<unsigned long> cycle = {
      frame(READ_DATA, 0, 0x0A00), frame(WRITE_DATA, 1, 0x0500), frame(READ_DATA, 3, 0),
      frame(READ_DATA, 5, 0), frame(READ_DATA, 11, 0xF100), frame(WRITE_DATA, 11, 0xF014),
      frame(WRITE_DATA, 16, 0x1600), frame(WRITE_DATA, 24, 0x15CC), frame(WRITE_DATA, 24, 0x1600),
      frame(READ_DATA, 25, 0), frame(READ_DATA, 26, 0), frame(READ_DATA, 27, 0),
      frame(READ_DATA, 48, 0), frame(READ_DATA, 49, 0), frame(READ_DATA, 56, 0), frame(READ_DATA, 57, 0),
  };

  for (unsigned long request : cycle)
    CHECK(gw.forwards(request) == (request != frame(WRITE_DATA, 11, 0xF014))); // first TSP write held
  for (unsigned long request : cycle)
    CHECK(gw.forwards(request));
  CHECK(gw.rejectedCount() == 0);
}

static void single_corrupt_tsp_write_is_held_and_discarded()
{
  Gateway gw;
  gw.start();
  gw.forwards(frame(WRITE_DATA, 11, 0xF014));
  CHECK(gw.forwards(frame(WRITE_DATA, 11, 0xF014)));

  CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xF094)));
  CHECK(gw.forwards(frame(WRITE_DATA, 11, 0xF014)));
  CHECK(gw.rejectedCount() == 1);
}

static void repeated_write_is_forwarded_on_confirmation()
{
  Gateway gw;
  gw.start();
  CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xFAFF)));
  CHECK(gw.forwards(frame(WRITE_DATA, 11, 0xFAFF)));
  CHECK(gw.forwards(frame(WRITE_DATA, 11, 0xFAFF)));
}

static void pending_write_expires()
{
  Gateway gw;
  gw.start();
  CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xFA94)));
  fake_millis += 31000;
  CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xFA94)));
  CHECK(gw.forwards(frame(WRITE_DATA, 11, 0xFA94)));
}

static void tset_and_command_are_not_held()
{
  Gateway gw;
  gw.start();
  CHECK(gw.forwards(frame(WRITE_DATA, 1, 0x0500)));
  CHECK(gw.forwards(frame(WRITE_DATA, 1, 0x2800)));
  CHECK(gw.forwards(frame(WRITE_DATA, 4, 0x0100)));
}

static void manufacturer_specific_write_is_held()
{
  Gateway gw;
  gw.start();
  CHECK(!gw.forwards(frame(WRITE_DATA, 200, 0x1234)));
  CHECK(gw.forwards(frame(WRITE_DATA, 200, 0x1234)));
}

static void write_confirmations_one_disables_holding()
{
  Gateway gw;
  gw.component.set_write_confirmations(1);
  gw.start();
  CHECK(gw.forwards(frame(WRITE_DATA, 11, 0xF094)));
}

static void spare_bits_are_rejected()
{
  Gateway gw;
  gw.start();
  CHECK(gw.relay(frame(READ_DATA, 0, 0x0300) ^ 0x81000000).empty());
  CHECK(gw.rejectedCount() == 1);

  Gateway lenient;
  lenient.component.set_check_spare_bits(false);
  lenient.start();
  CHECK(lenient.relay(frame(READ_DATA, 0, 0x0300) ^ 0x81000000).size() == 1);
}

static void mismatched_boiler_response_is_not_relayed()
{
  Gateway gw;
  gw.start();
  bus.boiler = [](unsigned long) { return frame(READ_ACK, 26, 0x3000); };
  gw.relay(frame(READ_DATA, 25, 0));
  CHECK(bus.to_thermostat.empty());
  CHECK(gw.rejectedCount() == 1);

  bus.boiler = [](unsigned long) { return frame(READ_ACK, 25, 0x3000) ^ 0x1; };
  gw.relay(frame(READ_DATA, 25, 0));
  CHECK(bus.to_thermostat.empty());
}

static void error_responses_are_relayed()
{
  Gateway gw;
  gw.start();
  bus.boiler = [](unsigned long) { return frame(UNKNOWN_DATA_ID, 16, 0x1600); };
  gw.relay(frame(WRITE_DATA, 16, 0x1600));
  CHECK(bus.to_thermostat == std::vector<unsigned long>{frame(UNKNOWN_DATA_ID, 16, 0x1600)});

  bus.boiler = [](unsigned long) { return frame(DATA_INVALID, 11, 0x3300); };
  gw.relay(frame(READ_DATA, 11, 0x3300));
  CHECK(bus.to_thermostat.size() == 1);
}

static void tsp_read_ack_with_wrong_index_is_not_relayed()
{
  Gateway gw;
  gw.start();
  bus.boiler = [](unsigned long) { return frame(READ_ACK, 11, 0xFA58); };
  gw.relay(frame(READ_DATA, 11, 0xF100));
  CHECK(bus.to_thermostat.empty());
}

struct TspGateway : Gateway
{
  sensor::Sensor value;
  OpenthermNumber number;

  TspGateway()
  {
    component.add_tsp(0xFA, true, -5, 5, 300000, &value, &number);
    start();
  }
};

static void configured_tsp_rejects_out_of_range_thermostat_writes()
{
  TspGateway gw;
  for (int i = 0; i < 3; i++)
    CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xFA9C)));
  CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xFA06)));
  CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xFA06)));
  CHECK(gw.rejectedCount() == 5);

  CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xFAFF)));
  CHECK(gw.forwards(frame(WRITE_DATA, 11, 0xFAFF)));
  CHECK(!gw.forwards(frame(WRITE_DATA, 11, 0xEE14))); // unconfigured index: confirmation only
  CHECK(gw.forwards(frame(WRITE_DATA, 11, 0xEE14)));
}

static void tsp_is_polled_and_out_of_range_shows_unknown_number()
{
  TspGateway gw;
  bus.tsp[0xFA] = 0xFF;
  gw.component.update();
  fake_millis += 5000; // thermostat silent: queued read goes out
  gw.component.loop();
  CHECK(bus.to_boiler == std::vector<unsigned long>{frame(READ_DATA, 11, 0xFA00)});
  gw.component.update();
  CHECK(gw.value.state == -1);
  CHECK(gw.number.state == -1);

  bus.tsp[0xFA] = 0x9C;
  fake_millis += 300000;
  gw.component.update();
  fake_millis += 5000;
  gw.component.loop();
  gw.component.update();
  CHECK(gw.value.state == -100);
  CHECK(std::isnan(gw.number.state));
}

static void number_writes_with_read_back_and_refuses_out_of_range()
{
  TspGateway gw;
  bus.clear();
  gw.number.set(-1);
  CHECK(bus.to_boiler == (std::vector<unsigned long>{frame(WRITE_DATA, 11, 0xFAFF), frame(READ_DATA, 11, 0xFA00)}));
  CHECK(gw.number.state == -1);
  CHECK(gw.value.state == -1);

  bus.clear();
  gw.number.set(6);
  CHECK(bus.to_boiler.empty());
}

struct ClimateGateway : Gateway
{
  OpenthermClimate heating;

  explicit ClimateGateway(climate::ClimateMode mode)
  {
    heating.set_climate_type(ClimateType::HEATING_WATER);
    component.register_climate(&heating);
    start();
    heating.setup();
    heating.mode = mode;
  }
};

static void ch_off_clears_ch_enable_in_status()
{
  ClimateGateway gw(climate::CLIMATE_MODE_OFF);
  auto sent = gw.relay(frame(READ_DATA, 0, 0x0300));
  CHECK(sent == std::vector<unsigned long>{frame(READ_DATA, 0, 0x0200)});
  CHECK(!OpenTherm::parity(sent.at(0)));

  CHECK(gw.forwards(frame(WRITE_DATA, 1, 0x0500))); // only Status is touched
  CHECK(gw.forwards(frame(WRITE_DATA, 0, 0x0300))); // a WRITE Status is not turned into a READ
}

static void ch_off_does_not_launder_corrupted_status()
{
  ClimateGateway gw(climate::CLIMATE_MODE_OFF);
  CHECK(gw.relay(frame(READ_DATA, 0, 0x0300) ^ 0x81000000).empty());
}

static void ch_heat_forwards_status_unchanged()
{
  ClimateGateway gw(climate::CLIMATE_MODE_HEAT);
  CHECK(gw.forwards(frame(READ_DATA, 0, 0x0300)));
}

static climate::ClimateMode boot_heating_climate()
{
  OpenthermClimate heating;
  heating.set_climate_type(ClimateType::HEATING_WATER);
  heating.setup();
  return heating.mode;
}

static void set_heating_mode(climate::ClimateMode mode)
{
  OpenthermClimate heating;
  heating.set_climate_type(ClimateType::HEATING_WATER);
  heating.setup();
  climate::ClimateCall call;
  call.mode = mode;
  heating.control(call);
}

static void climate_mode_survives_reboot_but_not_upgrade()
{
  fake_preferences.clear();
  climate::fake_restore = climate::ClimateDeviceRestoreState{climate::CLIMATE_MODE_OFF}; // saved by older firmware
  CHECK(boot_heating_climate() == climate::CLIMATE_MODE_HEAT);
  climate::fake_restore.reset();

  set_heating_mode(climate::CLIMATE_MODE_OFF);
  CHECK(boot_heating_climate() == climate::CLIMATE_MODE_OFF);
  set_heating_mode(climate::CLIMATE_MODE_HEAT);
  CHECK(boot_heating_climate() == climate::CLIMATE_MODE_HEAT);
  fake_preferences.clear();
}

int main()
{
  normal_traffic_is_never_rejected();
  single_corrupt_tsp_write_is_held_and_discarded();
  repeated_write_is_forwarded_on_confirmation();
  pending_write_expires();
  tset_and_command_are_not_held();
  manufacturer_specific_write_is_held();
  write_confirmations_one_disables_holding();
  spare_bits_are_rejected();
  mismatched_boiler_response_is_not_relayed();
  error_responses_are_relayed();
  tsp_read_ack_with_wrong_index_is_not_relayed();
  configured_tsp_rejects_out_of_range_thermostat_writes();
  tsp_is_polled_and_out_of_range_shows_unknown_number();
  number_writes_with_read_back_and_refuses_out_of_range();
  ch_off_clears_ch_enable_in_status();
  ch_off_does_not_launder_corrupted_status();
  ch_heat_forwards_status_unchanged();
  climate_mode_survives_reboot_but_not_upgrade();

  std::printf(failures ? "%d check(s) failed\n" : "all host tests passed\n", failures);
  return failures ? 1 : 0;
}
