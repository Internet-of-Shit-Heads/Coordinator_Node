#define INITIAL_UPDATE_INTERVAL 30

#include <sensordata.pb.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <cryptlib.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#include <rflib.h>
//node configuration header
#include "cfglib.h"

static WiFiClientSecure wifiClient;
static PubSubClient client(wifiClient);

static rflib_msg_t ackmsg;
static rflib_msg_t msg;
static at_ac_tuwien_iot1718_N2C msg_to_recv;
static at_ac_tuwien_iot1718_C2N msg_to_send;
// initial value 01.01.2018
static uint32_t unix_time = 1514761200;
//current update interval for the nodes
static uint8_t update_interval = INITIAL_UPDATE_INTERVAL;
//last used update interval
static uint8_t last_update_interval = INITIAL_UPDATE_INTERVAL;
static uint32_t timeout = 0;
static char node_topic[50];

struct coordinator_config esp_config = {
  .mqtt_server = "192.168.44.1",
  .mqtt_user = "IoT_client",
  .mqtt_password = "leafy_switch_soup",
  .mqtt_topic_timestamp = "IoT/Time",
  .mqtt_topic_update_interval = "IoT/Interval",
  .mqtt_port = 1883,
  .wifi_ssid = "IoT_17_18",
  .wifi_password = "heavy_cat_radiator",
  .address = 0xF0F0F0F0E1LL,
  .channel = 0,
  .delay = 15,
  .retransmits = 15,
  .auth_key = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  .cepin = 0,
  .cspin = 15,
  .baud_rate = 115200,
  .debug = DEBUG_OFF,
};

//creating mqtt topic for all measurement values
void mqtt_create_topic(){
  char* tmp_string = "    ";
  memset(node_topic,'\0',sizeof(node_topic));
  strncat(node_topic, "IoT/Room",sizeof("IoT/Room"));
  sprintf(tmp_string,"%d", msg_to_recv.roomNo);
  strncat(node_topic, tmp_string,sizeof(tmp_string));
  strncat(node_topic, "/Node",sizeof("/Node"));
  sprintf(tmp_string,"%d", msg_to_recv.nodeId);
  strncat(node_topic, tmp_string,sizeof(tmp_string));
  strncat(node_topic, "/Sensor", sizeof("/Sensor"));
  sprintf(tmp_string,"%d", msg_to_recv.sensorId);
  strncat(node_topic, tmp_string,sizeof(tmp_string));
  //set sensor type
  switch(msg_to_recv.type){
    case 1:
      strncat(node_topic, "/temperature/", sizeof("/temperature/"));
      break;
    case 2:
      strncat(node_topic, "/humidity/", sizeof("/humidity/"));
      break;
    default:
      strncat(node_topic, "/unknown/", sizeof("/unknown/"));   
      break; 
  }
}

static bool pre_send(rflib_msg_t *msg, uint32_t timestamp)
{
  msg_to_send.timestamp = timestamp;
  pb_ostream_t stream = pb_ostream_from_buffer(msg->data, RFLIB_MAX_MSGSIZE);
  bool enc_res = pb_encode(&stream, at_ac_tuwien_iot1718_C2N_fields, &msg_to_send);
  msg->size = enc_res ? stream.bytes_written : 0;
  return enc_res;
}

static bool post_recv(rflib_msg_t *msg, at_ac_tuwien_iot1718_N2C *msg_to_recv)
{
  pb_istream_t stream = pb_istream_from_buffer(msg->data, msg->size);
  return pb_decode(&stream, at_ac_tuwien_iot1718_N2C_fields, msg_to_recv);
}

void callback(char* topic, byte* payload, unsigned int length) {
  if(strncmp(topic, esp_config.mqtt_topic_update_interval,10) == 0){
    update_interval = strtol((char*)payload, 0, 0);
    Serial.print("Got new update interval in seconds: "); 
    Serial.println(update_interval);//strtol(update_interval, 0, 0)); 
    msg_to_send.has_command = true;
    msg_to_send.timestamp = unix_time;
    Serial.print("last update interval: ");
    Serial.println(last_update_interval);
    timeout = msg_to_send.timestamp + 3 * last_update_interval;
    Serial.print("set last_update_timeout to: ");
    Serial.println(last_update_interval);
    msg_to_send.command.type = at_ac_tuwien_iot1718_Command_CommandType_NEW_UPDATE_INTERVAL;
    msg_to_send.command.has_param1 = true;
    msg_to_send.command.param1 = update_interval;
    memset(payload, 0, 10);
    last_update_interval = update_interval;
  } 
  if(strncmp(topic, esp_config.mqtt_topic_timestamp, 6) == 0){
    unix_time = strtol((char*)payload, 0, 0);
    Serial.print("Got Unix Timestamp: "); 
    Serial.println(unix_time); 
    memset(payload, 0, 10);
  }   
}

void setup(){
  Serial.begin(esp_config.baud_rate);
  setup_certificates();
  wifi_connect();
  client.setServer(esp_config.mqtt_server, esp_config.mqtt_port);
  Serial.print("Begin init\n\r");
  delay(200);
  if (rflib_coordinator_init(esp_config.cepin, esp_config.cspin, esp_config.channel, &esp_config.address, 1, esp_config.delay, esp_config.retransmits) < 0) {
    Serial.print("Init failed :(\n\r");
    abort();
  }else{
    Serial.print("Init success :)\n\r"); 
    rflib_coordinator_set_reply(0, &ackmsg);       
  }
}

static void setup_certificates(){
  delay(10);
  SPIFFS.begin();
  File key = SPIFFS.open("/coordinator.key.der","r");
  if(wifiClient.loadPrivateKey(key, key.size())) {
    Serial.println("Loaded Key");
  }else{
    Serial.println("Didn't load Key");
  }  
  //load client cert
  File c_cert = SPIFFS.open("/coordinator.crt.der","r");
  if(wifiClient.loadCertificate(c_cert, c_cert.size())) {
    Serial.println("Loaded Cert");
  }else{
    Serial.println("Didn't load cert");
    return;
  }
  File ca = SPIFFS.open("/ca.der", "r"); 
  if (!ca) {
    Serial.println("Failed to open CA file");
  }else{
    Serial.println("Success to open CA file");
    if(wifiClient.loadCACert(ca)){
      Serial.println("CA loaded");
    }else{
      Serial.println("CA not loaded");
    }
  }
}

static void wifi_connect(){  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(esp_config.wifi_ssid);
  WiFi.begin(esp_config.wifi_ssid, esp_config.wifi_password);

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());   
}

static void reconnect(){
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnect WIFI");
    wifi_connect();
  }
  while(!wifiClient.connected()){
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", esp_config.mqtt_user, esp_config.mqtt_password)) {
      client.setCallback(callback);
      Serial.println("connected");
    }else{
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
  if (client.subscribe(esp_config.mqtt_topic_timestamp)){
    Serial.println("Subscribing to Time");  
  }
  if (client.subscribe(esp_config.mqtt_topic_update_interval)){
    Serial.println("Subscribing to Update Interval");  
  }
}

void loop(){
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if(unix_time > timeout){
    msg_to_send.has_command = false;
  }
  if(pre_send(&ackmsg, unix_time) < 0){
    return;
  }
  ackmsg.size = cryptlib_auth(ackmsg.data, ackmsg.size, RFLIB_MAX_MSGSIZE, esp_config.auth_key);
  if(!(ackmsg.size < 0)){
    rflib_coordinator_set_reply(0, &ackmsg);     
  }else{
    return;  
  }
  if(rflib_coordinator_available() == 0){
    rflib_coordinator_read(&msg);
    msg.size = cryptlib_verify(msg.data, msg.size, esp_config.auth_key);
    //Check for a valid message
    if(msg.size < 0){
      return;
    }
    if (post_recv(&msg, &msg_to_recv)){
       Serial.println("E tutto in ordine!");  
       Serial.print("Room number: ");
       Serial.println(msg_to_recv.roomNo);
       Serial.print("Type: ");
       Serial.println(msg_to_recv.type);
       Serial.print("Timestamp: ");
       Serial.println(msg_to_recv.timestamp);
       Serial.print("Sensordata: ");
       Serial.println(msg_to_recv.data);
       Serial.print("NodeID: ");
       Serial.println(msg_to_recv.nodeId);
       Serial.print("SebsorID: ");
       Serial.println(msg_to_recv.sensorId);
       //check for a valid time (inside of the update interval?)
       if(msg_to_recv.timestamp >= unix_time - update_interval){
          Serial.println("Benissimo!");
          //create mqtt topic
          mqtt_create_topic();
          Serial.print("Sending node topic: ");
          Serial.print(node_topic);
          Serial.println(msg_to_recv.data);
          client.publish(node_topic, String(msg_to_recv.data).c_str(), false);
        }else{
          Serial.println("Presto, presto, andiamo!");
        }
    }else{
     Serial.println("Decoding failed! :(");  
    }
  }
}
