#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <WiFiClientSecure.h>
#include <time.h>
// #include <SPI.h>

char WIFI_SSID[32];
char WIFI_PASSWORD[32];
//char* WIFI_SSID = "Celular";
//char* WIFI_PASSWORD = "contra123";
//const char* WIFI_PASSWORD = "mAdz1c/aRad.eper4";
const int WIFI_TIMEOUT_MS = 5000;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = 3600;


//---------------------------------------------------------------
//		Funciones Wifi
//---------------------------------------------------------------

/// Muestra redes disponibles
void showNetworks() {
	int n = WiFi.scanNetworks();
	Serial.println("scan done");
	if (n == 0) {
		Serial.println("no networks found");
	} else {
	Serial.print(n);
	Serial.println(" networks found");
	}
	for (int i = 0; i < n; ++i) {
		// Print SSID and RSSI for each network found
		Serial.print(i + 1);
		Serial.print(": ");
		Serial.print(WiFi.SSID(i));
		Serial.print(" (");
		Serial.print(WiFi.RSSI(i));
		Serial.print(")");
		Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
		delay(10);
	}
}

/// Se conecta a una red en especifico
void connectToWiFi(char* WIFI_SSID, char* WIFI_PASSWORD) {
	WiFi.disconnect();
	Serial.printf("\nConnecting to %s ...",WIFI_SSID);
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
	// Esperando a que se conecte o al timeout
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
		Serial.print(".");
		delay(100);
	}
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("Failed");
		esp_deep_sleep_start();		// El ESP se suspende 
	}
	else {
		Serial.println("Conected!");
	}
}

/// Selecciona una red de las disponibles
void selectNetwork(char* WIFI_SSID, char* WIFI_PASSWORD) {
	Serial.print("Select SSID: ");
	while (!Serial.available()) ;
	char SSID_id = Serial.read();
	String SSID_str = WiFi.SSID(SSID_id - '1'); 
	SSID_str.toCharArray(WIFI_SSID,SSID_str.length()+1);

	Serial.printf("%s selected \n",WIFI_SSID);

	Serial.print("Password: ");
	int i; char pw_char;
	for (i=0; i<32; i++) {
		while (!Serial.available()) ;
		pw_char = Serial.read();
		Serial.print(pw_char);
		if (pw_char=='\r') {
			Serial.read();
			WIFI_PASSWORD[i] = '\0';
			break;
		}
		WIFI_PASSWORD[i] = pw_char;
	}
	if (i==32)	Serial.println("\nCharacter limit for password surpassed");
}

/// Lists all directories
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

void setup(){
  Serial.begin(921600);

  showNetworks();
	selectNetwork(WIFI_SSID, WIFI_PASSWORD);
  connectToWiFi(WIFI_SSID, WIFI_PASSWORD);
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi

  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  writeFile(SD, "/prueba.csv", "Estado, Timestamp\n");
}
bool encendido = true;
void loop(){
  //char row[50];
  struct tm timeinfo;
  char Timestamp[50];

	getLocalTime(&timeinfo);
	sprintf(Timestamp,"%02d/%02d/%d %02d:%02d:%02d",timeinfo.tm_mday, timeinfo.tm_mon + 1, \
	 timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  // long now = millis();
  // ltoa(now,TS_str,10);

  int i = 1;
  File file = SD.open("/prueba.csv", FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  else {
    if (encendido)
      file.print("ON,");
    else 
      file.print("OFF,");
    encendido = !encendido;
    file.print(Timestamp);
    file.print("\n");
    file.close();
    delay(500);
    Serial.printf("\rLinea escrita %d",i);
    i++;
  }
}