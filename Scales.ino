/**
 *
 * HX711 library for Arduino - example file
 * https://github.com/bogde/HX711
 *
 * MIT License
 * (c) 2018 Bogdan Necula
 *
**/
#include "HX711.h"
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define PIN_SPI_CS 21

String ip = "";

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 15;
const int LOADCELL_SCK_PIN = 5;

String SSID = "";
String PSWD = "";
String HOST = "";

HX711 scale;


void setup() {

//WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);

delay(1000);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
delay(1000);
    if (!SD.begin(PIN_SPI_CS)) {
          Serial.println(F("SD CARD FAILED, OR NOT PRESENT! - REBOOT!"));
             delay(2000);
              ESP.restart();
  }
delay(1000);
  Serial.println(F("SD CARD INITIALIZED."));

// ### SD Card has initialised so read the config file ###
String ConfigString = ReadText("/CONFIG.txt");

Serial.println(F("CONFIG Data Loaded!"));
//tft.println('CONFIG Data Loaded.'); 
Serial.println(ConfigString);

SSID = Split(Split(ConfigString,'\n',0),'=',1);
PSWD = Split(Split(ConfigString,'\n',1),'=',1);
HOST = Split(Split(ConfigString,'\n',2),'=',1);



initWiFi();


}

void loop() {

if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
  
   HTTPClient http;

  uint64_t chipid;
  
  chipid = ESP.getEfuseMac();
  String SchipID = uint64ToString(chipid);

  long RSSI = WiFi.RSSI();
  int i = scale.read_average(50);   
  String S = "/setSCVA?setSCVA=" + String(i) + "&setRSSI=" + String(RSSI) + "&setSCID=" + SchipID;  
  Serial.println(S);
  String URL = "http://" + HOST + S;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
   http.begin(URL.c_str());  //Specify destination for HTTP request

   int httpResponseCode = http.GET();   //Send the actual POST request
  
   if(httpResponseCode>0){
  
    String response = http.getString();                       //Get the response to the request
  
    Serial.println(httpResponseCode);   //Print return code
    Serial.println(response);           //Print request answer
  
   }else{
       //Free resources
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
    http.end();
    WiFi.reconnect();
   }
  
   http.end();  //Free resources
  
 }else{
    Serial.println("Error in WiFi connection");   
    WiFi.reconnect();
 }
delay(5000);  
}


void initWiFi() {


int lSSID = SSID.length()+1;
char cSSID[lSSID];
int lPWD = PSWD.length()+1;
char cPWD[lPWD];
SSID.toCharArray(cSSID, lSSID);
PSWD.toCharArray(cPWD, lPWD);


//###################################################################
//### Fudge to remove any funny chars from the end of the strings ###
//###################################################################

  for (int i = 0; i <= SSID.length(); i++) {
    if (cSSID[i] == char(10)){
      cSSID[i] = '\0';
    }
    if (cSSID[i] == char(13)){
      cSSID[i] = '\0';
    }     
  }

  for (int i = 0; i <= PSWD.length(); i++) {
    if (cPWD[i] == char(10)){
      cPWD[i] = '\0';
    }
    if (cPWD[i] == char(13)){
      cPWD[i] = '\0';
    }     
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(cSSID, cPWD);
  Serial.print("Connecting to WiFi ...");
  int C = 0;
  while (WiFi.status() != WL_CONNECTED) {
    C = C + 1;
    if (C == 59){
      Serial.println("Failed to connect after 1 min, Rebooting!");
      delay(2000);
      ESP.restart();     
    }   
    Serial.print('.');
    delay(1000);
  }


  ip = IpAddress2String(WiFi.localIP());

  Serial.println("Connected - " + ip);

}


//############## CONVERT IP ADDRESS TO STRING SO IT CAN BE PRINTED TO TFT ###############
String IpAddress2String(const IPAddress &ipAddress) {
  return String(ipAddress[0]) + String(".") + String(ipAddress[1]) + String(".") + String(ipAddress[2]) + String(".") + String(ipAddress[3]);
}

String ReadText(const char *filename){
  File myfile;
  myfile = SD.open(filename, FILE_READ);

  // Check if the file opened successfully
  if (myfile) {
    // Read the double value from the file
    String value = myfile.readString();
    myfile.close();
    return value;
  } else {
    // If the file didn't open, print an error message
    Serial.println("Error opening file");
  }
}

  String Split(String data, char separator, int index) {
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;
    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
  }

  String uint64ToString(uint64_t input) {
  String result = "";
  uint8_t base = 10;

  do {
    char c = input % base;
    input /= base;

    if (c < 10)
      c +='0';
    else
      c += 'A' - 10;
    result = c + result;
  } while (input);
  return result;
}
