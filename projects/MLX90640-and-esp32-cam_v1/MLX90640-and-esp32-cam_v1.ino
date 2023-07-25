#include <WiFi.h>
#include <Wire.h>   // 它包含了用於 I2C 通訊的函式和定義。

#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>   // SD Card ESP32
#include "SD.h"   // SD Card ESP32
#include "SPI.h"
#include "Arduino.h"


#include "camera_pin.h"
#include "capture_and_save.h"
#include "image_processing.h"
#include "webpage.h"
#include "buffer_frame.h"

/*
// **************** Constant defines ****************
#define CAMERA_RAW_FRAME_BUFFER_COLS           320
#define CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define CAMERA_RESIZE_FRAME_BUFFER_COLS        160
#define CAMERA_RESIZE_FRAME_BUFFER_ROWS        120
#define CAMERA_THERMAL_FRAME_BUFFER_COLS       32
#define CAMERA_THERMAL_FRAME_BUFFER_ROWS       24
#define CAMERA_FRAME_BYTE_SIZE                 3
*/



// **************** Put your SSID & Password ****************
const char* ssid = "test";  // Enter SSID here
const char* password = "test@1213";  //Enter Password here


void setup(){
  // setup Serial
  Serial.begin(115200);
  delay(100);

  // Mounting SPIFFS，啟用 SPIFFS 檔案系統的意思
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  // Initialize the camera
  Serial.print("Initializing the camera module...");
  configInitCamera();
  Serial.println("Ok!");

  // Initialize SD card 
  if(!SD.begin(21))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  
  // Determine if the type of SD card is available
  uint8_t cardType = SD.cardType();
  
  if(cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  sd_sign = true; // sd initialization check passes

  // Connect to local wi-fi network
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // 啟動 wi-fi
  
  // check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  
  Serial.println(WiFi.localIP());
  
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  // 綁定http請求的回調函數
  server.on("/", handle_OnConnect);
  server.on("/capture", handle_capture);
  server.on("/save", handle_save);
  server.on("/saved_photo", []() {getSpiffImg(FILE_PHOTO, "image/jpg");});
  server.onNotFound(handle_NotFound);  // http請求不可用時的回調函數

  server.begin(); // 開啓WebServer功能
  Serial.println("HTTP server started");
}


void loop()
{
  server.handleClient();  // WebServer 監聽客戶請求並處理
  
  if (takeNewPhoto) 
  {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }
  
  if (savePhotoSD) 
  {
    char filename[32];
    sprintf(filename, "/image%d.jpg", imageCount);  // filename 會等於 "/image1.jpg"、 "/image2.jpg" ....
    photo_save_to_SD_card(filename);
    Serial.printf("Saved picture：%s\n", filename);
    Serial.println("");
    imageCount++;  
    savePhotoSD = false;
  }
  
  delay(1);
}

// webserver
void handle_OnConnect() {
  server.send(200, "text/html", index_html);  // 傳網頁上去
}
void handle_capture() {
  takeNewPhoto = true;
  Serial.println("Capturing Image for view only");
  server.send(200, "text/plain", "Taking Photo"); 
}
void handle_save() {
  savePhotoSD = true;
  Serial.println("Saving Image to SD Card");
  server.send(200, "text/plain", "Saving Image to SD Card"); 
}
void handle_savedPhoto() {
  Serial.println("Saving Image");
  server.send(200, "image/jpg", FILE_PHOTO); 
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}
