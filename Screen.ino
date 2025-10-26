#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <TFT_eSPI.h>
#include <JPEGDecoder.h>

#define TFT_GREY 0x5AEB

AsyncWebServer *server;

TFT_eSPI tft = TFT_eSPI();

//##### SETTINGS FROM CONFIG FILE ///

String SSID = "";
String PSWD = "";
String USER = "";
String SPWD = "";
String PORT = "";
String IMAG = "";
String FTXT = "";  //## Free text for the top of the screen
String SCMA = "";
String SCMI = "";
String CAMA = "";
String CAMI = "";
String FULL = "";
String EMPT = "";
String FTX2 = "";  //## Free text for the top of the screen


//#### These values arent stored in the config ###
String SCVA = "";
String SCID = "";
String RSSI = "";
String TEMP = "";
String SCWE = "";
//#################################################

//######################################

const char* NewSSID = "setSSID";  // ### SSID
const char* NewPSWD = "setPSWD";  //### Password
const char* NewUSER = "setUSER";  //### Server Username
const char* NewSPWD = "setSPWD";  //### Server Password
const char* NewPORT = "setPORT";  //### Server Port
const char* NewIMAG = "setIMAG";  //### Background image 240 x 240
const char* NewFTXT = "setFTXT";  //### Free text for Line 1
const char* NewSCMA = "setSCMA";  //### Scale Value for a calibration (5kg)
const char* NewSCMI = "setSCMI";  //### Scale Value for an calibration (0kg)
const char* NewCAMA = "setCAMA";  //### Calibration Value Weight (5Kg etc)
const char* NewCAMI = "setCAMI";  //### Calibration Value Tare (0Kg)
const char* NewFULL = "setFULL";  //### Actual Weight of FULL keg
const char* NewEMPT = "setEMPT";  //### Actual Weight of Empty keg or known calibration value
const char* NewFTX2 = "setFTX2";  //### Free text for Line 2
const char* NewSCVA = "setSCVA";  //### Actual Scale Value


char http_username[] = "USERNAMEXXXXXXX";
char http_password[] = "PASSWORDXXXXXXX";



int LoopTimer = 0;
//### Timeout in ms ###
int Timeout = 20000;

String ip = "";

File myFile;

void setup(void) {

initSDCard();
delay(1000);
LoadConfig();
int Server_Port = PORT.toInt();
server = new AsyncWebServer(Server_Port);
server->serveStatic("/", SD, "/");


  server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    request->send(SD, "/index.html", "text/html");
  });

  server->on("/RESTART", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    request->send(200);
    delay(2000); 
    ESP.restart();
  });

  server->on("/RELOAD", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    request->send(SD, "/index.html", "text/html");
    delay(2000);    
    LoadConfig();

  });


//####################################################################
//########            MULTIPLE PARAMETER TEST                 ########
//####################################################################

server->on("/setSCVA", HTTP_GET, [](AsyncWebServerRequest* request) {
int params = request->params();
for(int i=0;i<params;i++){
 AsyncWebParameter* p = request->getParam(i);
   if (String(p->name().c_str()) == "setSCVA"){
      SCVA = p->value().c_str();      
      double ScaleValue = SCVA.toDouble(); //### Raw Value from the scales
      double ScaleDiff = SCMA.toDouble() - SCMI.toDouble(); //### Difference from Max (calibration) and Min (Tare) Values from the scales
      double ActDiff = CAMA.toDouble() - CAMI.toDouble(); //### Fill Keg weight (or calibration) - Empty Keg Weight (or Tare)
      double Slope = ActDiff / ScaleDiff; //### Slope of the line
      double ScaleDiff2 = ScaleValue - SCMI.toDouble();
      double Weight = ScaleDiff2 * Slope; //### This is the actual weight sat on the scales
      SCWE = String((Weight / 1000));
      Serial.println("Weight: " + String(Weight));           
      //#### We now need to turn the actual weight into a percentage to view on the scale ###
      double Perc = ((Weight - EMPT.toDouble())  / (FULL.toDouble() - EMPT.toDouble())) * 100;
      int intPerc = Perc;
      WriteSCVA(); //### Write a small HTML file with SCVA value in it
      Serial.println("Keg Percentage: " + String(intPerc));
      Serial.println("Screen Wifi RSSI: " + String(WiFi.RSSI()));
      String Fn = RSSItoImage(WiFi.RSSI());      
      drawSdJpeg(Fn.c_str(), 220, 0);
      DrawRemaining(intPerc);
      request->send(200);
      } // ### IF

    if (String(p->name().c_str()) == "setRSSI"){     
        RSSI = p->value().c_str();
        Serial.println("RSSI: " + RSSI);
    }

    if (String(p->name().c_str()) == "setSCID"){      
        SCID = p->value().c_str();
        Serial.println("Scales ID: " + SCID);  
    }

    if (String(p->name().c_str()) == "setTEMP"){      
        TEMP = p->value().c_str();
        Serial.println("Scales TEMP: " + TEMP);  
    }  
  }
  //### Draw remote RSSI image to line 2 ###
  //### Reset the timer to Zero to stop the "NoWifi" image getting displayed
  LoopTimer = 0;
  String RemoteRSSI = RSSItoImage(RSSI.toInt());      
  drawSdJpeg(RemoteRSSI.c_str(), 220, 20);

  
  //##########################################
  Serial.println("ID = " + SCID + " - Value = " + SCVA + " - RSSI = " + RSSI + " - Temperature = " + TEMP);
  request->send(200);
});

//####################################################################
//####################################################################
//####################################################################

server->on("/setKEGS", HTTP_GET, [](AsyncWebServerRequest* request) { 
int params = request->params();
for(int i=0;i<params;i++){
 AsyncWebParameter* p = request->getParam(i);
   if (String(p->name().c_str()) == "setEMPT"){
      EMPT = p->value().c_str();      
   }
    if (String(p->name().c_str()) == "setFULL"){     
        FULL = p->value().c_str();
    }
  }
  Serial.println("EMPT = " + EMPT + " - FULL = " + FULL);
  request->send(SD, "/kegs.html", "text/html");
  WriteCONFIG();
  LoadConfig();
});

//###################################################
//################## SET CALIBRATION ################

server->on("/setCALIBRATION", HTTP_GET, [](AsyncWebServerRequest* request) { 
int params = request->params();
for(int i=0;i<params;i++){
 AsyncWebParameter* p = request->getParam(i);
 //### Scale Min Value [raw] ###
   if (String(p->name().c_str()) == "setCAMI"){
      CAMI = p->value().c_str();      
   }
  //### Scale Max Value [raw] ###
    if (String(p->name().c_str()) == "setCAMA"){     
        CAMA = p->value().c_str();
    }
     //### Weight used when reading max valueb[g] ###
      if (String(p->name().c_str()) == "setSCMI"){     
        SCMI = p->value().c_str();
    }    
   //### Weight used when reading min Value [g] ###
      if (String(p->name().c_str()) == "setSCMA"){     
        SCMA = p->value().c_str();
    }
  }
  Serial.println("CAMI = " + CAMI + " - CAMA = " + CAMA + " - SCMI = " + SCMI  + " - SCMA = " + SCMA);
  request->send(SD, "/calibration.html", "text/html");
  WriteCONFIG();
  LoadConfig();
});

//##### Change the port number
  server->on("/setPORT", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewPORT)) {
      message = request->getParam(NewPORT)->value();
      PORT = String(message);
      request->send(SD, "/wifi.html", "text/html");
      delay(2000);       
      WriteCONFIG();
      LoadConfig();      
    }
  });

  //##### Change the SSID
  server->on("/setSSID", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewSSID)) {
      message = request->getParam(NewSSID)->value();
      SSID = String(message);
      request->send(SD, "/wifi.html", "text/html");
      delay(2000);       
      WriteCONFIG();
      LoadConfig();     
    }
  });

  //##### Change the Password for WIFI
  server->on("/setPSWD", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewPSWD)) {
      message = request->getParam(NewPSWD)->value();
      PSWD = String(message);
      request->send(SD, "/wifi.html", "text/html");
      delay(2000);       
      WriteCONFIG();
      LoadConfig();     
    }
  });

  //##### Change the Server Username
  server->on("/setUSER", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewUSER)) {
      message = request->getParam(NewUSER)->value();
      USER = String(message);
      request->send(SD, "/wifi.html", "text/html");
      delay(2000);       
      WriteCONFIG();
      LoadConfig();
    }
  });

  //##### Change the Server Password
  server->on("/setSPWD", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewSPWD)) {
      message = request->getParam(NewSPWD)->value();
      SPWD = String(message);
      request->send(SD, "/wifi.html", "text/html");
      delay(2000);       
      WriteCONFIG();
      LoadConfig();  
    }
  });


//##### Change the Scale Minimum value (nothing on scale value)
  server->on("/setSCMI", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewSCMI)) {
      message = request->getParam(NewSCMI)->value();
      SCMI = String(message);
      request->send(SD, "/calibration.html", "text/html");
      delay(2000); 
      WriteCONFIG();
      LoadConfig();  
    }
  });


//##### Change the Scale Maximum value (Known weight scale value)
  server->on("/setSCMA", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewSCMA)) {
      message = request->getParam(NewSCMA)->value();
      SCMA = String(message);
      request->send(SD, "/calibration.html", "text/html");
      delay(2000);       
      WriteCONFIG();
      LoadConfig();  
    }
  });

//##### Change the Calibration Max value (20kg etc)
  server->on("/setCAMA", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewCAMA)) {
      message = request->getParam(NewCAMA)->value();
      CAMA = String(message);
      request->send(SD, "/calibration.html", "text/html");
      delay(2000);       
      WriteCONFIG();
      LoadConfig();      
    }
  });

//##### Change the Calibration Minumum value (usually zero)
  server->on("/setCAMI", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewCAMI)) {
      message = request->getParam(NewCAMI)->value();
      CAMI = String(message);
      request->send(SD, "/calibration.html", "text/html");
      delay(2000);       
      WriteCONFIG();
      LoadConfig();     
    }
  });


//##### Change the Known weight of a FULL keg
  server->on("/setFULL", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewFULL)) {
      message = request->getParam(NewFULL)->value();
      FULL = String(message);
      request->send(SD, "/kegs.html", "text/html");
      delay(2000); 
      WriteCONFIG();
      LoadConfig();     
    }
  });


//##### Change the known weight of an EMPTY keg
  server->on("/setEMPT", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewEMPT)) {
      message = request->getParam(NewEMPT)->value();
      EMPT = String(message);
      request->send(SD, "/kegs.html", "text/html");
      delay(2000);
      WriteCONFIG();
      LoadConfig();      
    }
     
  });


//#### Set the top line of free text
  server->on("/setFTXT", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewFTXT)) {
      message = request->getParam(NewFTXT)->value();
      FTXT = message;

      drawSdJpeg("/top.jpg", 0, 0);  //### Clear the top of the screen for the message

      tft.setTextSize(1);
      tft.setCursor(1, 1, 2);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.println(FTXT);
      tft.setCursor(1, 20, 2);
      tft.println(FTX2);
      request->send(SD, "/screen.html", "text/html");
      delay(2000); 
      WriteCONFIG();
      LoadConfig(); 
    }
 
  });

//#### Set the second line of free text
  server->on("/setFTX2", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewFTX2)) {
      message = request->getParam(NewFTX2)->value();
      FTX2 = message;

      drawSdJpeg("/top.jpg", 0, 0);  //### Clear the top of the screen for the message

      tft.setTextSize(1);
      tft.setCursor(1, 1, 2);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.println(FTXT);
      tft.setCursor(1, 20, 2);
      tft.println(FTX2);

      request->send(SD, "/screen.html", "text/html");
      delay(2000); 
      WriteCONFIG();
      LoadConfig();
    }
  
  });


//#### CHange the Beer LOGO 240 x 240 jpeg
  server->on("/setIMAG", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam(NewIMAG)) {
      message = request->getParam(NewIMAG)->value();
      IMAG = "/LOGO-" + message;
      String Fn = IMAG;
      drawSdJpeg(Fn.c_str(), 0, 40);  //### Clear the top of the screen for the message
      
      WriteCONFIG();
      LoadConfig();
      delay(100);
      CompileImageList();      
      delay(100);
      request->send(SD, "/screen.html", "text/html");    }
    
  });

//#### Upload Beer Logos
  server->on("/upload", HTTP_POST, [](AsyncWebServerRequest* request) {
      if (!request->authenticate(http_username, http_password)) {
        return request->requestAuthentication();
      }
      //request->send(200);
    },
    handleUpload);


  Serial.begin(115200);

  digitalWrite(15, HIGH);  // TFT screen chip select
  digitalWrite(5, HIGH);   // SD card chips select, must use GPIO 5 (ESP32 SS)
  CompileImageList();
  initWiFi();
  

  //##### Get the screen up and running ####
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.begin();
  tft.setRotation(0);

  drawSdJpeg("/top.jpg", 0, 0);

  String BGimage = IMAG;
  drawSdJpeg(BGimage.c_str(), 0, 40);  

  drawSdJpeg("/19.jpg", 0, 280);


  tft.setTextSize(1);
  tft.setCursor(1, 1, 2);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.println(FTXT);
  tft.setCursor(1, 20, 2);
  tft.println(FTX2);

server->begin();
}



void loop() {

  uint16_t x, y;

  tft.getTouchRaw(&x, &y);

  if(x > 1200 & x < 2600 & y > 1200 & y < 2600){
  //tft.background(200, 100, 0);
  tft.fillScreen(TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);

  tft.setCursor(1, 1, 2);
  String A = "Host ip: " + ip;
  tft.println(A.c_str());

  tft.setCursor(1, 20, 2);
  String B = "Host port: " + PORT;
  tft.println(B.c_str());
  
  tft.setCursor(1, 40, 2);
  String C = "SSID: " + SSID;
  tft.println(C.c_str());

  
 


//String SSID = "";
//String PSWD = "";
//String USER = "";
//String SPWD = "";
//String PORT = "";
//String IMAG = "";
//String FTXT = "";  //## Free text for the top of the screen
//String SCMA = "";
//String SCMI = "";
//String CAMA = "";
//String CAMI = "";
//String FULL = "";
//String EMPT = "";
//String FTX2 = "";
  
    delay(6000);
ESP.restart();

  }

//Serial.println(LoopTimer);
LoopTimer = LoopTimer + 1;
    if (LoopTimer > Timeout){
    //### Reset the count
    LoopTimer = 0;
      
    //### Draw NoWifi from SCales ###
    drawSdJpeg("/NoWifi.jpg", 220,20);

    //### Draw the Server RSSI ###
    String Fn = RSSItoImage(WiFi.RSSI());      
    drawSdJpeg(Fn.c_str(), 220, 0);
  }
delay(1);
}


//######################################
//##########  FUNCTIONS ################
//######################################



String ReadText(const char* filename) {
  File myfile;
  myfile = SD.open(filename, FILE_READ);

  if (myfile) {
    String value = myfile.readString();                                         
    //Serial.print(String(value));
    myfile.close();
    return value;
  } else {
    Serial.println("Error opening file");
  }
}


String Split(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
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


void initSDCard() {
  if (!SD.begin()) {
    Serial.println("Card Mount Failed. Rebooting ...");
    tft.println("CMF - Rebooting ...");
    delay(2000);
    ESP.restart();
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    tft.println("No SD card attached");
    return;
  }
  Serial.println(F("SD CARD INITIALISED."));
  Serial.print("SD Card Type: ");
  tft.println("Card Type:");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
    tft.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
    tft.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
    tft.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
    tft.println("Unknown");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}


void initWiFi() {


  int lSSID = SSID.length() + 1;
  char cSSID[lSSID];
  int lPWD = PSWD.length() + 1;
  char cPWD[lPWD];
  SSID.toCharArray(cSSID, lSSID);
  PSWD.toCharArray(cPWD, lPWD);


  //###################################################################
  //### Fudge to remove any funny chars from the end of the strings ###
  //###################################################################

  for (int i = 0; i <= SSID.length(); i++) {
    if (cSSID[i] == char(10)) {
      cSSID[i] = '\0';
    }
    if (cSSID[i] == char(13)) {
      cSSID[i] = '\0';
    }
  }

  for (int i = 0; i <= PSWD.length(); i++) {
    if (cPWD[i] == char(10)) {
      cPWD[i] = '\0';
    }
    if (cPWD[i] == char(13)) {
      cPWD[i] = '\0';
    }
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(cSSID, cPWD);
  Serial.print("Connecting to WiFi ...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
    drawSdJpeg("/NoWifi.jpg", 220, 0);
  }


  ip = IpAddress2String(WiFi.localIP());

  Serial.println("Server running on " + ip);
}




//############## CONVERT IP ADDRESS TO STRING SO IT CAN BE PRINTED TO TFT ###############
String IpAddress2String(const IPAddress& ipAddress) {
  return String(ipAddress[0]) + String(".") + String(ipAddress[1]) + String(".") + String(ipAddress[2]) + String(".") + String(ipAddress[3]);
}


//############# FOR LOADING JPEG IMAGES TO THE TFT DISPLAY #############################
void drawSdJpeg(const char* filename, int xpos, int ypos) {

  // Open the named file (the Jpeg decoder library will close it)
  File jpegFile = SD.open(filename, FILE_READ);  // or, file handle reference for SD library

  if (!jpegFile) {
    Serial.print("ERROR: File \"");
    Serial.print(filename);
    Serial.println("\" not found!");
    return;
  }

  //Serial.println("===========================");
  //Serial.print("Drawing file: ");
  //Serial.println(filename);
  ///
  //Serial.println("===========================");

  // Use one of the following methods to initialise the decoder:
  bool decoded = JpegDec.decodeSdFile(jpegFile);  // Pass the SD file handle to the decoder,
  //bool decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)

  if (decoded) {
    // print information about the image to the serial port
    //jpegInfo();
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  } else {
    Serial.println("Jpeg file format not supported!");
  }
}

//####################################################################################################
// Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
//####################################################################################################
// This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
// fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
void jpegRender(int xpos, int ypos) {

  //jpegInfo(); // Print information from the JPEG file (could comment this line out)

  uint16_t* pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // Fetch data from the file, decode and display
  while (JpegDec.read()) {  // While there is more data in the file
    pImg = JpegDec.pImage;  // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w) {
      uint16_t* cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++) {
        p += mcu_w;
        for (int w = 0; w < win_w; w++) {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    // draw image MCU block only if it will fit on the screen
    if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ((mcu_y + win_h) >= tft.height())
      JpegDec.abort();  // Image has run off bottom of screen so abort decoding
  }

  tft.setSwapBytes(swapBytes);

  //showTime(millis() - drawTime); // These lines are for sketch testing only
}

//####################################################################################################
// Print image information to the serial port (optional)
//####################################################################################################
// JpegDec.decodeFile(...) or JpegDec.decodeArray(...) must be called before this info is available!
void jpegInfo() {

  // Print information extracted from the JPEG file
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");
  Serial.println(JpegDec.width);
  Serial.print("Height     :");
  Serial.println(JpegDec.height);
  Serial.print("Components :");
  Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");
  Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");
  Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");
  Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");
  Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");
  Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}


void handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!index) {
    Serial.printf("BodyStart: %u B\n", total);
  }
  for (size_t i = 0; i < len; i++) {
    Serial.write(data[i]);
  }
  if (index + len == total) {
    Serial.printf("BodyEnd: %u B\n", total);
  }
}

void DrawRemaining(int Percentage) {

  double xx = int(Percentage * 0.2);

  //### Fudge for if weight is heavier than max for some reason
  if (xx > 19) {
    xx = 19;
  }

  if (xx < 0) {
    xx = 0;
  }

  int x = xx;


  String Fn = String(x);
  Fn = "/" + Fn + ".jpg";

  tft.setRotation(0);
  drawSdJpeg(Fn.c_str(), 0, 280);
}

void LoadConfig(){
  // ### SD Card has initialised so read the config file ###
  String ConfigString = ReadText("/CONFIG.txt");

  Serial.println(F("CONFIG Data Loaded!"));
  //tft.println('CONFIG Data Loaded.');


  SSID = Split(Split(ConfigString, '\n', 0), '=', 1);
  PSWD = Split(Split(ConfigString, '\n', 1), '=', 1);
  USER = Split(Split(ConfigString, '\n', 2), '=', 1);
  SPWD = Split(Split(ConfigString, '\n', 3), '=', 1);
  PORT = Split(Split(ConfigString, '\n', 4), '=', 1);
  IMAG = Split(Split(ConfigString, '\n', 5), '=', 1);
  FTXT = Split(Split(ConfigString, '\n', 6), '=', 1);
  SCMA = Split(Split(ConfigString, '\n', 7), '=', 1);
  SCMI = Split(Split(ConfigString, '\n', 8), '=', 1);
  CAMA = Split(Split(ConfigString, '\n', 9), '=', 1);
  CAMI = Split(Split(ConfigString, '\n', 10), '=', 1);  
  FULL = Split(Split(ConfigString, '\n', 11), '=', 1);
  EMPT = Split(Split(ConfigString, '\n', 12), '=', 1);
  FTX2 = Split(Split(ConfigString, '\n', 13), '=', 1);
  

  int lUSER = USER.length() + 1;
  char cUSER[lUSER];
  int lSPWD = SPWD.length() + 1;
  char cSPWD[lSPWD];
  USER.toCharArray(cUSER, lUSER);
  SPWD.toCharArray(cSPWD, lSPWD);


  //###################################################################
  //### Fudge to remove any funny chars from the end of the strings ###
  //###################################################################

  for (int i = 0; i <= USER.length(); i++) {
    if (cUSER[i] == char(10)) {
      cUSER[i] = '\0';
    }
    if (cUSER[i] == char(13)) {
      cUSER[i] = '\0';
    }
  }

  for (int i = 0; i <= SPWD.length(); i++) {
    if (cSPWD[i] == char(10)) {
      cSPWD[i] = '\0';
    }
    if (cSPWD[i] == char(13)) {
      cSPWD[i] = '\0';
    }
  }


    for (int i = 0; i <= 15; i++) {
    if (i <= lUSER) {
        http_username[i] = cUSER[i];
    }        
    else {
        http_username[i] = '\0';
      }
    

     if (i <= lSPWD) {
        http_password[i] = cSPWD[i];
     }        
    else {
        http_password[i] = '\0';
      }
    }

}


void handleUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);

  String Fname = String("/LOGO-" + filename);

  if (!index) {
    logmessage = "Upload Start: " + String(Fname);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = SD.open("/" + Fname, FILE_WRITE);
    Serial.println(logmessage);
  }

  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    logmessage = "Writing file: " + String(Fname) + " index=" + String(index) + " len=" + String(len);
    Serial.println(logmessage);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(Fname) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    Serial.println(logmessage);
    drawSdJpeg(Fname.c_str(), 0, 40);
    CompileImageList();
    delay(100);
    request->redirect("/screen.html");
  }
}

void WriteSCVA(){
              SD.remove("/scva.html");
              myFile = SD.open("/scva.html", FILE_WRITE);
              if (myFile) {
                      myFile.println("<html><head><meta http-equiv=\"refresh\" content=\"2\"></head><body>Current Scale Value: " + String(SCVA) + "<br>Current Scale Weight: " + String(SCWE) + "</body></html>");
              }
}

void WriteCONFIG(){
              SD.remove("/CONFIG.txt");
              delay(100);
              myFile = SD.open("/CONFIG.txt", FILE_WRITE);
              if (myFile) {
                      myFile.print("SSID=" + SSID + '\n' + "PSWD=" + PSWD + '\n' + "USER=" + USER + '\n' + "SPWD=" + SPWD  + '\n' + "PORT=" + PORT + '\n' + "IMAG=" + IMAG + '\n' + "FTXT=" + FTXT + '\n' + "SCMA=" + SCMA + '\n' + "SCMI=" + SCMI + '\n' + "CAMA=" + CAMA + '\n' + "CAMI=" + CAMI + '\n' + "FULL=" + FULL + '\n' + "EMPT=" + EMPT + '\n' + "FTX2=" + FTX2);

                      myFile.close();

                      //ESP.restart(); 
              }
}           

String RSSItoImage(int RSSI){
int w = RSSI * -1;
  if(w < 45){                       
    return "/wifigood.jpg";
  }
  if(w <= 60){
    return "/wifiok.jpg";
  }
  if(w <= 70){
    return "/wifipoor.jpg";
  }
  if(w > 70){
    return "/wifishit.jpg";
  }

}

void CompileImageList(){

  myFile = SD.open("/");
  String HeaderText = ReadText("/header.txt");
  String HTMLfile = "";
     while (true) {

      File entry = myFile.openNextFile();
      if (! entry) {
         break;
      }

      String FN = String(entry.name());

      String S1 = Split(FN,'-',0);
      String S2 = Split(FN,'-',1);

      if (S1 == "LOGO"){
        HTMLfile = HTMLfile + "<a href=\"setIMAG?setIMAG=" + S2 + "\" target=\"_parent\"><button class=\"button button1\">" + S2 + "</button></a><br>\n";
      }
      
      entry.close();
   }
   HTMLfile = HeaderText + HTMLfile + "</body></html>";
              SD.remove("/LOGO.html");
              delay(50);
              myFile = SD.open("/LOGO.html", FILE_WRITE);
              delay(50);
              if (myFile) {
                      myFile.print(HTMLfile);
              }
              myFile.close();              
}