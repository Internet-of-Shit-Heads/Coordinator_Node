#ifndef __NODE_CONFIG_H__
#define __NODE_CONFIG_H__

static struct {
  uint64_t address = 0xF0F0F0F0E1LL;
  unsigned int baud_rate = 115200;
  uint16_t cepin = 0;
  uint16_t cspin = 15;
  uint8_t channel = 0;
  uint8_t delay = 15;
  uint8_t retransmits = 15;
  char* mqtt_server = "192.168.44.1";
  char* mqtt_user = "IoT_client";
  char* mqtt_password = "leafy_switch_soup";
  char* mqtt_topic_timestamp = "Time/1";
  char* mqtt_topic_update_interval = "Interval/1";
  uint32_t mqtt_port = 1883;
  char* wifi_ssid = "IoT_17_18";
  char* wifi_password = "heavy_cat_radiator";
  uint8_t mac_key[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
} coordinator_config;

#endif /* __NODE_CONFIG_H__ */
