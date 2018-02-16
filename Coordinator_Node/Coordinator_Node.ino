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

WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

static struct rflib_msg_t ackmsg = {
  9, "BAM OIDA"
};

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  int valuerflib = 0;
  Serial.begin(115200);

  Serial.print("Begin init\n\r");
  // Give some time 
  delay(100);
  if (valuerflib = rflib_coordinator_init(0, 15, 0, pipes, 1, 15, 15) < 0) {
    Serial.print("Init failed :(\n\r");
    Serial.print(valuerflib);
    abort();
  }else{
    Serial.print("Init success :(\n\r");  
  }
}

static bool ackmsg_loaded = false;
static void toggle_ackmsg(void)
{
  Serial.print("toggling\n\r");
  if (ackmsg_loaded) {
    rflib_coordinator_clear_reply();
    ackmsg_loaded = false;
  } else {
    rflib_coordinator_set_reply(0, &ackmsg);
    ackmsg_loaded = true;
  }
}

void setup_wifi() {
  
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
  
  File ca = SPIFFS.open("/ca.pem.der", "r"); 
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

void reconnect() {
  while (!wifiClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
static rflib_msg_t msg;
static uint8_t runcnt = 0;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;

    float newTemp = 80;//= hdc.readTemperature();
    float newHum = 70;//hdc.readHumidity();

    temp = newTemp;
    Serial.print("New temperature:");
    Serial.println(String(temp).c_str());
    client.publish(temperature_topic, String(temp).c_str(), false);

 /*   hum = newHum;
    Serial.print("New humidity:");
    Serial.println(String(hum).c_str());
    client.publish(humidity_topic, String(hum).c_str(), false);
*/
      if (runcnt % 5 == 0) {
        toggle_ackmsg();
      }
      runcnt++;

      if (rflib_coordinator_available() == 0) {
        rflib_coordinator_read(&msg);
        Serial.print("Message-Content");
        Serial.print((char *)msg.data);
        Serial.print("\n\r"); 
        //publish humidity value
        newHum = (float)* msg.data;
        Serial.println(String(newHum).c_str());
        client.publish(humidity_topic, String(newHum).c_str(), false);
      }
    }
}
