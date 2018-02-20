#include <sensordata.pb.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#include <rflib.h>

#define wifi_ssid "IoT_17_18"
#define wifi_password "heavy_cat_radiator"
#define mqtt_server "192.168.44.1"
#define mqtt_user "IoT_client"
#define mqtt_password "leafy_switch_soup"

#define humidity_topic "IoT/lab/humidity/"
#define temperature_topic "IoT/lab/temperature/"

const uint64_t pipes[1] = { 0xF0F0F0F0E1LL };

static WiFiClientSecure wifiClient;
static PubSubClient client(wifiClient);

static struct rflib_msg_t ackmsg;
static at_ac_tuwien_iot1718_N2C msg_to_recv;
static char* timestring = "1514761200"; // initial value 01.01.2018

/*static struct {
  uint64_t address = 0xF0F0F0F0E1LL;
  unsigned int baud_rate = 57600;
  uint16_t cepin = 9;
  uint16_t cspin = 10;
  uint8_t channel = 0;
  uint8_t delay = 15;
  uint8_t retransmits = 15;
} coordinator_config;
*/
static bool pre_send(rflib_msg_t *msg, uint32_t timestamp)
{
  static at_ac_tuwien_iot1718_C2N msg_to_send = at_ac_tuwien_iot1718_C2N_init_zero;
  msg_to_send.timestamp = timestamp;

  pb_ostream_t stream = pb_ostream_from_buffer(msg->data, RFLIB_MAX_MSGSIZE);
  bool enc_res = pb_encode(&stream, at_ac_tuwien_iot1718_C2N_fields, &msg_to_send);
  msg->size = enc_res ? stream.bytes_written : 0;
  //Serial.println(msg->size);
  return enc_res;
}

static bool post_recv(rflib_msg_t *msg, at_ac_tuwien_iot1718_N2C *msg_to_recv)
{
  pb_istream_t stream = pb_istream_from_buffer(msg->data, msg->size);
  return pb_decode(&stream, at_ac_tuwien_iot1718_N2C_fields, msg_to_recv);
}

void callback(char* topic, byte* payload, unsigned int length) {
  if(strncmp(topic, "Time/1",6) == 0){
    strncpy(timestring, (char*)payload, 10);
    Serial.print("Got Unix Timestamp: "); 
    Serial.println(timestring); 
  }  
}

void setup() {
  Serial.begin(115200);
  setup_certificates();
  wifi_connect();
  client.setServer(mqtt_server, 1883);
  Serial.print("Begin init\n\r");
  // Give some time 
  delay(100);
  if (rflib_coordinator_init(0, 15, 0, pipes, 1, 15, 15) < 0) {
    Serial.print("Init failed :(\n\r");
    abort();
  }else{
    Serial.print("Init success :)\n\r"); 
    //rflib_coordinator_set_reply(0, &ackmsg);       
  }
}

static bool ackmsg_loaded = false;
static void toggle_ackmsg(void)
{
  pre_send(&ackmsg, 5);//timestamp is static 5  
  Serial.print("Sending\n\r");
  if (ackmsg_loaded) {
    rflib_coordinator_clear_reply();
    ackmsg_loaded = false;
  } else {
    rflib_coordinator_set_reply(0, &ackmsg);
    ackmsg_loaded = true;
  }
}

static void setup_certificates() {
  
  delay(10);
  //load ca cert
  SPIFFS.begin();

  File key = SPIFFS.open("/coordinator.key.der","r");
  if(wifiClient.loadPrivateKey(key, key.size())) {
    Serial.println("Loaded Key");
  } else {
    Serial.println("Didn't load Key");
  }  

  //load client cert
  File c_cert = SPIFFS.open("/coordinator.crt.der","r");
  if(wifiClient.loadCertificate(c_cert, c_cert.size())) {
    Serial.println("Loaded Cert");
  } else {
    Serial.println("Didn't load cert");
    return;
  }
  
  File ca = SPIFFS.open("/ca.der", "r"); 
  if (!ca) {
    Serial.println("Failed to open CA file");
  }
  else {
  Serial.println("Success to open CA file");
  if(wifiClient.loadCACert(ca))
    Serial.println("CA loaded");
    else
    Serial.println("CA not loaded");
  }
}

static void wifi_connect(){  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());   
}

static void reconnect() {
  while (!wifiClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      client.setCallback(callback);
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  if (client.subscribe("Time/1")){
    Serial.println("Subscribing to Time");  
  }
}

static long lastMsg = 0;
static float temp = 0.0;
static float hum = 0.0;
static rflib_msg_t msg;
static char node_topic[50];
static char* tmp_string = "    ";

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
      pre_send(&ackmsg, strtol(timestring, 0, 0));//send timestamp to nodes  
      rflib_coordinator_set_reply(0, &ackmsg);      
      if (rflib_coordinator_available() == 0) {
        rflib_coordinator_read(&msg);
        if (post_recv(&msg, &msg_to_recv)){
            Serial.println("Tutto bene!");  
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
          }else{
            Serial.println("Decoding failed! :(");  
        }
        //create mqtt topic
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
        if(msg_to_recv.type == 1){
          strncat(node_topic, "/temperature/", sizeof("/temperature/"));  
        }else if(msg_to_recv.type == 2){
          strncat(node_topic, "/humidity/", sizeof("/humidity/"));  
        }else{
          strncat(node_topic, "/unknown/", sizeof("/unknown/"));  
        }

        Serial.print(node_topic);
        Serial.println(msg_to_recv.data);
        client.publish(node_topic, String(msg_to_recv.data).c_str(), false);
        memset(node_topic,'\0',50);
      }
}
