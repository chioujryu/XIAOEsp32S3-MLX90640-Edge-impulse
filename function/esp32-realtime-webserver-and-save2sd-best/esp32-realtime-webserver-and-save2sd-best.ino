#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <time.h>
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
#include <string>
#include "Esp.h"

#include "Arduino.h"

#include "webpage.h"
#include "camera_pins.h" 
#include "mlx90640_thermal_processing.hpp" 
#include "write_read_file.hpp" 
#include "image_processing.hpp" 


/* Constant defines -------------------------------------------------------- */
const uint16_t CAMERA_RAW_FRAME_BUFFER_COLS         = 320;
const uint16_t CAMERA_RAW_FRAME_BUFFER_ROWS         = 240;
const uint16_t CAMERA_RESIZE_FRAME_BUFFER_COLS      = 32*16;
const uint16_t CAMERA_RESIZE_FRAME_BUFFER_ROWS      = 24*16;
const uint16_t MLX90640_RAW_FRAME_BUFFER_COLS       = 32;
const uint16_t MLX90640_RAW_FRAME_BUFFER_ROWS       = 24;
const uint16_t CAMERA_FRAME_BYTE_SIZE               = 3;

const String SPIFFS_FILE_PHOTO_ESP = "/photo.jpg";
const String SPIFFS_FILE_PHOTO_MLX90640 = "/thermal_photo.jpg";
const String SPIFFS_FILE_PHOTO_THERMAL_ESP = "/photo.txt";

// Put your SSID & Password
const char* ssid = "test";  // Enter SSID here
const char* password = "test@1213";  //Enter Password here
const char* ap_ssid = "chioujryu";  // Enter SSID here
const char* ap_password = "0123456789";  //Enter Password here
bool ap_wifi_mode = false;

// instantiation 
WebServer server(80);  //創建 server 實例


// Variable define
boolean takeNewPhoto = false;   // 拍一張新照片
boolean savePhotoSD = false;    // 儲存照片到sd卡
int imageCount = 1;                // File Counter
bool whether_get_image_url = false;
char file_name[256];

/* Buffer define -------------------------------------------------------- */
float * mlx90640To                       = (float *) malloc(MLX90640_RAW_FRAME_BUFFER_COLS * MLX90640_RAW_FRAME_BUFFER_ROWS * sizeof(float));
float * minTemp_and_maxTemp_and_aveTemp  = (float *) malloc(3 * sizeof(float));
uint8_t * rgb888_raw_buf_mlx90640      = (uint8_t *) malloc(MLX90640_RAW_FRAME_BUFFER_COLS * MLX90640_RAW_FRAME_BUFFER_ROWS * CAMERA_FRAME_BYTE_SIZE * sizeof(uint8_t));
uint8_t * rgb888_resized_buf_mlx90640    = (uint8_t *) malloc(CAMERA_RAW_FRAME_BUFFER_COLS * CAMERA_RAW_FRAME_BUFFER_ROWS * CAMERA_FRAME_BYTE_SIZE * sizeof(uint8_t));

camera_fb_t * frame_buffer               = NULL;
uint8_t * rgb888_raw_buf_esp32       = (uint8_t *) malloc(CAMERA_RAW_FRAME_BUFFER_COLS * CAMERA_RAW_FRAME_BUFFER_ROWS * CAMERA_FRAME_BYTE_SIZE * sizeof(uint8_t));
uint8_t * rgb888_resized_buf_esp32       = (uint8_t *) malloc(CAMERA_RESIZE_FRAME_BUFFER_COLS * CAMERA_RESIZE_FRAME_BUFFER_ROWS * CAMERA_FRAME_BYTE_SIZE * sizeof(uint8_t));

uint8_t * rgb888_buf_esp_and_mlx90640_addition = (uint8_t *) malloc(CAMERA_RAW_FRAME_BUFFER_COLS * CAMERA_RAW_FRAME_BUFFER_ROWS * CAMERA_FRAME_BYTE_SIZE * sizeof(uint8_t));


void setup()
{
  // 設定Serial
  Serial.begin(115200);

  Serial.println("setup");
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

  // setup I2C
  Wire.begin(); 
  Wire.setClock(400000); // Wire.setClock(400000) 設置 I2C 時鐘速率為 400 kHz，這是 I2C 協議的標準速率之一，通常用於高速數據傳輸。


  //PSRAM Initialisation
  if(psramInit()){
          Serial.println("\nThe PSRAM is correctly initialized");
  }else{
          Serial.println("\nPSRAM does not work");
  }

  // Mounting SPIFFS，啟用 SPIFFS 檔案系統的意思
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    Serial.println("SPIFFS mounted successfully");
  }

  // Initialize the camera
  Serial.print("Initializing the camera module...");
  configInitCamera();
  Serial.println("Ok!");

  // Initialize SD card
  if(!SD.begin(21)){
    Serial.println("Card Mount Failed");
    return;}
  // Determine if the type of SD card is available
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;}
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
  // Calculate SD Size
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  
  // wife setting
  if (ap_wifi_mode != true){
    // Connect to local wi-fi network
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
  
    // check wi-fi is connected to wi-fi network
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");  
    Serial.println(WiFi.localIP());
    Serial.println("ID: " + String(ssid) + " | " + "PASSWORD: " + String(password));
  }else{
    // You can remove the password parameter if you want the AP to be open.
    // a valid password must have more than 7 characters
    if (!WiFi.softAP(ap_ssid, ap_password)) {
      log_e("Soft AP creation failed.");
      while(1);
    }
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    Serial.println("ID: " + String(ap_ssid) + " | " + "PASSWORD: " + String(ap_password));
  }
  
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  // http server
  server.on("/", handle_OnConnect);
  server.on("/save", handle_save);
  server.on("/capture", handle_capture);
  server.on("/getText", handleGetText);
  server.on("/get_esp32_photo_url", []() {getSpiffImg(server, SPIFFS_FILE_PHOTO_ESP , "image/jpg");});
  server.onNotFound(handle_NotFound);  // http請求不可用時的回調函數
  server.begin(); 
  Serial.println("HTTP server started");
}



void loop()
{
  server.handleClient();  // WebServer 監聽客戶請求並處理

  // Only take photo and show on webserver
  if (whether_get_image_url){
    Serial.println("ready to capture a image to SPIFFS");
    captureESP32SaveToSpiffs(frame_buffer, SPIFFS_FILE_PHOTO_ESP);
    whether_get_image_url = false;
    
  }
                    
  // capture and save to SD card
  if (savePhotoSD) {
    // save esp32 photo
    sprintf(file_name, "/%d_esp32_image_.jpg", imageCount);
    esp32_photo_save_to_SD_card(frame_buffer, file_name);
    imageCount++;
    savePhotoSD = false;
    logMemory();  // check memory usage
  }
  delay(100);
}

// webserver
void handle_OnConnect() {
  server.send(200, "text/html", index_html);
}
void handle_capture() {
  whether_get_image_url = true;
  server.send(200, "text/plain", "capture a photo"); 
}
void handle_save() {
  savePhotoSD = true;
  Serial.println("Saving Image to SD Card");
  server.send(200, "text/plain", "Saving Image to SD Card"); 
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}
void handleGetText() {
  server.send(200, "text/plain", file_name);
}
void getDynamicText(char *buffer, size_t size) {
  time_t currentTime = time(NULL);
  snprintf(buffer, size, "Current Time: %s", asctime(localtime(&currentTime)));
}

// Capture ESP32 Photo and Save it to SPIFFS
void captureESP32SaveToSpiffs( camera_fb_t * fb, const String spffs_file_dir) {
                                                                 
    // ESP32S3 Capture
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);  // dispose the buffered image
    fb = NULL; // reset to capture errors
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    }
    
    // save image in spiffs
    File file = SPIFFS.open(spffs_file_dir , FILE_WRITE); // 將空白照片放進SPIFFS裡面
  
    // ===Insert the data in the photo file===
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length，將拍到的照片寫進空白照片裡面
    }
    
    int fileSize = file.size();   // 取得照片的大小
    
    // Close the file
    file.close();   // 關閉照片
  
    // check if file has been correctly saved in SPIFFS
    if(checkPhotoExitInSPIFFS(SPIFFS, spffs_file_dir)){Serial.println("SPIFFS has file");}
    else{Serial.println("SPIFFS has no file");}
  
    Serial.println("The picture has a size of " + String(fileSize) + " bytes");

    // release buffer
    esp_camera_fb_return(fb);//釋放照片
    fb = NULL;
}


// Save esp32 photo to SD card
void esp32_photo_save_to_SD_card(camera_fb_t * fb, const char * file_name) {
    
    // declare variable
    uint8_t * jpg_buf = NULL;
    size_t jpg_size = 0;
    size_t fileSize = 0;
    bool ok = 0; // Boolean indicating if the picture has been taken correctly
    
    
    // Clean buffer
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);  // dispose the buffered image
    fb = NULL; // reset to capture errors

    // capture
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Failed to get camera frame buffer");
      return;
    }

    // Save photo to file
    write_file_jpg(SD, file_name, fb->buf, fb->len);

    // Release image buffer
    esp_camera_fb_return(fb);  // 釋放掉 fb 的記憶體
    fb = NULL;
      
    Serial.println("The picture has a size of " + String(fileSize) + " bytes");
}


void logMemory() {
  Serial.println("Used RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " bytes");
  Serial.println("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
}



  
