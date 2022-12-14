#include <WiFi.h>
#include <PubSubClient.h>

// Replace the next variables with your SSID/Password combination
const char* WIFI_SSID = "Celular";
const char* WIFI_PASSWORD = "contra123";
// const char* WIFI_SSID = "redpucp";
// const char* WIFI_PASSWORD = "C9AA28BA93";

// Add your MQTT Broker IP address, example:
const char* MQTT_URL = "broker.emqx.io";
const char* MQTT_USERNAME = "emqx";
const char* MQTT_PASSWORD = "public";
const int MQTT_PORT = 1883;


WiFiClient espClient;
PubSubClient MQTTclient(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup_wifi() {
  WiFi.disconnect(true,true);
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void MQTTreconnect() {
  // Loop until we're reconnected
  if (!MQTTclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (MQTTclient.connect("ESP32Client",MQTT_USERNAME,MQTT_PASSWORD,"vehiculos/auto_prueba/conectado",2,true,"N")) {
      Serial.println("connected");
      MQTTclient.setKeepAlive(5);
    } else {
      Serial.print("failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//-------------------------------------------------------------------//
//-----------------    MAIN FUNCTIONS   -----------------------------//
//-------------------------------------------------------------------//

void setup() {
  Serial.begin(9600);
  setup_wifi();
  // espClient.setCACert(root_ca); 
  MQTTclient.setServer(MQTT_URL, MQTT_PORT);
}

void loop() {
  MQTTreconnect();
  if (MQTTclient.connected()) {
    //MQTTclient.loop();

    // long now = millis();
    // if (now - lastMsg > 1500) {
    //   ltoa(now,msg,10);
    //   Serial.print("Timestamp: ");
    //   Serial.println(now);
    //   client.publish("esp32/timestamp",msg);
    //   lastMsg = millis();
    // }
    Serial.println("ON\t09:10:15");
    MQTTclient.publish("buenaventura/auto","ON    09:10:15");
    delay(6000);
    Serial.println("OFF\t09:10:21");
    MQTTclient.publish("buenaventura/auto","OFF   09:10:21");
    delay(4500);
    Serial.println("ON\t09:10:26");
    MQTTclient.publish("buenaventura/auto","ON    09:10:26");
    delay(6000);
    Serial.println("OFF\t09:10:30");
    MQTTclient.publish("buenaventura/auto","OFF   09:10:30");
    delay(3000);
  }
}
