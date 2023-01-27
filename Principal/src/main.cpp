#include <Arduino.h>
#include <BluetoothSerial.h>
#include <ELMduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FS.h>
#include <SD.h>
#include <time.h>

//---------------------------------------------------------------
//		CONSTANTES
//---------------------------------------------------------------
const int WIFI_TIMEOUT_MS = 2000;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = 3600;

const char* MQTT_URL = "broker.emqx.io";
const char* MQTT_USERNAME = "emqx";
const char* MQTT_PASSWORD = "public";
const int MQTT_PORT = 1883;

const int LED_POWER = 4;
const int LED_CONNECTION = 21;

WiFiClient espClient;
PubSubClient MQTTclient(espClient);

BluetoothSerial SerialBT;
ELM327 myELM327;
uint8_t ELM_address[6]  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xBA};


//---------------------------------------------------------------
//		VARIABLES GLOBALES
//---------------------------------------------------------------
bool attached_card;
char wifi_ssid[] = "RouterPortatil";
char wifi_password[] = "Buenaventura2023";
int reconnect_time_ms;
bool encendido = false;
bool mqtt_connected;
struct tm timeinfo;
char mqtt_message[25];


//---------------------------------------------------------------
//		FUNCIONES
//---------------------------------------------------------------

/// @brief Se conecta a una red en especifico
bool connectToWiFi(const char* wifi_ssid, const char* wifi_password)
{
	WiFi.disconnect(true,true);
  delay(10);
	Serial.printf("\nConnecting to %s ...",wifi_ssid);
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifi_ssid,wifi_password);
	// Esperando a que se conecte o al timeout
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
		Serial.print(".");
		delay(100);
	}
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("Failed");
		return false; 
	}
	else {
		Serial.println("Conected!");
    return true;
	}
}

/// @brief Se conecta al servidor MQTT 
bool MQTTinitialize()
{
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (MQTTclient.connect("ESP32Client",MQTT_USERNAME,MQTT_PASSWORD,"vehiculos/placa/conectado",2,true,"N")) {
    Serial.println("connected");
    MQTTclient.publish("vehiculos/placa/conectado","Y",true);
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.println(MQTTclient.state());
    return false;
  }
}

/// @brief Inicializa el Bluetooth y se conecta al ELM327 
bool OBD2setup()
{
  Serial.println("\nConnecting to OBDII scanner..."); 
  SerialBT.begin("ESP32", true);
  SerialBT.setPin("1234");
  if (!SerialBT.connect(ELM_address)) {
    Serial.println("Couldn't connect to OBD scanner - Phase 1");
    return false;
  }
  if (!myELM327.begin(SerialBT, false, 500)) {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
    return false;
  }
  Serial.println("Connected to ELM327");
  return true;
}

/// @brief Disconnects BT and unlinks device
void BTdisconnect()
{
  SerialBT.disconnect();
  SerialBT.unpairDevice(ELM_address);
}

/// @brief Setup for SD Card
/// @return False if error
bool SDsetup()
{
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    return false;
  }
  if(SD.cardType() == CARD_NONE){
    Serial.println("No SD card attached");
    return false;
  }
  Serial.println("SD card detected");
  return true;
}

/// @brief Saves data in csv file 
void SaveData()
{
  File file = SD.open("/Data.csv", FILE_APPEND);
  getLocalTime(&timeinfo);
  if (encendido) {
    sprintf(mqtt_message,"%d-%02d-%02dT%02d:%02d:%02d,Y",timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
    timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  else {
    sprintf(mqtt_message,"%d-%02d-%02dT%02d:%02d:%02d,N",timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
    timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  file.print(mqtt_message);
  file.print("\n");
  file.close();
}

/// @brief Uploads data in the SD card to the cloud
void UploadData()
{
  char rx_char;
  char line[25];
  int id_char;
  File file = SD.open("/Data.csv", FILE_READ);

  while (file.available()) {
    for (id_char=0; id_char<25; id_char++) {
      rx_char = file.read();
      if (rx_char == '\n') {
        line[id_char] = '\0';
        break;
      } 
      line[id_char] = rx_char;
    }
    //Serial.println(line);
    MQTTclient.loop();
    MQTTclient.publish("vehiculos/auto_prueba/encendido",line);
    Serial.print("Publicando: ");
    Serial.println(line);
    digitalWrite(LED_CONNECTION, LOW);
    delay(150);
    digitalWrite(LED_CONNECTION, HIGH);
    delay(100);
  }
  SD.remove("/Data.csv");
}

/// @brief Set digital pins to 0
void pullDownPins() 
{
  int pines[] = {2,12,13,14,15,22,25,26,27,32,33};
  for (int i=0; i<11; i++) {
    pinMode(pines[i], INPUT_PULLDOWN);
  }
}


//---------------------------------------------------------------
//		MAIN SETUP
//---------------------------------------------------------------
void setup() {
  pullDownPins();
  pinMode(LED_CONNECTION, OUTPUT);
  pinMode(LED_POWER, OUTPUT);
  digitalWrite(LED_POWER, HIGH);
  digitalWrite(LED_CONNECTION, LOW);

  Serial.begin(9600);

  // Waiting for MQTT connection
  while (1) {
    if (!WiFi.isConnected()) {
      connectToWiFi(wifi_ssid, wifi_password);
    }
    if (WiFi.isConnected()) {
      MQTTclient.setServer(MQTT_URL, MQTT_PORT);
      MQTTclient.setKeepAlive(10);
      if (MQTTinitialize()) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
        digitalWrite(LED_CONNECTION, HIGH);
        break;
      }
    }
    delay(5*1000);
  }

  attached_card = SDsetup(); 
  if (attached_card) {
    Serial.println("SD card ready");
    UploadData();
  }
  else {
    Serial.println("No memory");
  }

  while (!OBD2setup()) {
    BTdisconnect();
    Serial.println("Reconnecting to ELM327...");
  }
 
}

//---------------------------------------------------------------
//		MAIN LOOP
//---------------------------------------------------------------
unsigned long lastBT = 0, last_reconect_try = 0, lastPub = 0, lastMQTT = 0;
unsigned long BT_time_ms = 120;
bool BT_connected = true;

void loop() {
  // MQTT ping every 4 seconds
  if (millis() - lastMQTT > 4000) {
    Serial.println("Looping MQTT...");
    lastMQTT = millis();
    MQTTclient.loop();
  }

  // BT message every 120ms 
  if (millis() - lastBT > BT_time_ms) {
    lastBT = millis();

    myELM327.rpm(); // Asks for information
    if (myELM327.nb_rx_state == ELM_SUCCESS) {
      BT_connected = true;
      if (!encendido) {
        Serial.println("on");
        encendido = true;
        reconnect_time_ms = 5*1000;
      }
    }
    else if (myELM327.nb_rx_state == ELM_NO_DATA) {
      BT_connected = true;
      if (encendido) {
        Serial.println("off");
        encendido = false;
        reconnect_time_ms = 30*1000;
      }
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
      BT_connected = false;
      myELM327.printError();
      BTdisconnect();
      OBD2setup();    // This may take longer than the keepAlive interval (3 to 18 seconds)
      MQTTclient.loop();
    }
  }

  // Checks connection
  if (!MQTTclient.connected() || !WiFi.isConnected()) {
    digitalWrite(LED_CONNECTION, LOW);

    // Tries to reconnect once every reconnect time
    if (millis() - last_reconect_try > reconnect_time_ms) {
      last_reconect_try = millis();
      if (!WiFi.isConnected()) {
        connectToWiFi(wifi_ssid, wifi_password);
      }
      if (WiFi.isConnected()) {
        MQTTinitialize();
        if (MQTTclient.connected()) {
          digitalWrite(LED_CONNECTION, HIGH);
          if (attached_card) {
            Serial.println("Uploading data...");
            UploadData();
          }
        }
      }
    }
  }

  // Send MQTT message each 10 seconds
  if (millis() - lastPub > 10000) {
    lastPub = millis();
    getLocalTime(&timeinfo);
    if (encendido) {
      sprintf(mqtt_message,"%d-%02d-%02dT%02d:%02d:%02d,Y",timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
      timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else {
      sprintf(mqtt_message,"%d-%02d-%02dT%02d:%02d:%02d,N",timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
      timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    if (WiFi.isConnected() && MQTTclient.connected()) {
      Serial.print("Publicando: ");
      Serial.println(mqtt_message);
      MQTTclient.publish("vehiculos/placa/encendido",mqtt_message,true);
      if (BT_connected) {
        MQTTclient.publish("vehiculos/placa/BT","Y",true);
      }
      else {
        MQTTclient.publish("vehiculos/placa/BT","N",true);
      }
    }
    else if (attached_card) {
      Serial.print("Guardando en SD: ");
      Serial.println(mqtt_message);
      SaveData();
    } 
    else {
      Serial.println("No hay conexion");
    }

  }
}