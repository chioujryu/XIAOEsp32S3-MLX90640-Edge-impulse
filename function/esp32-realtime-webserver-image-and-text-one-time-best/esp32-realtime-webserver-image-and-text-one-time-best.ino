#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <time.h>
#include <ArduinoJson.h> 
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
#include <base64.h>

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
float minTemp = 26.24;
float maxTemp = 64.33;
float aveTemp = 53.26;

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
  server.on("/data", HTTP_GET ,handle_get_all_data);
  server.onNotFound(handle_NotFound);  // http請求不可用時的回調函數
  server.begin(); 
  Serial.println("HTTP server started");
}



void loop()
{ 
  server.handleClient();  // WebServer 監聽客戶請求並處理

  captureESP32SaveToSpiffs(frame_buffer, SPIFFS_FILE_PHOTO_ESP);

  delay(300);
}


// webserver
void handle_OnConnect() {
  server.send(200, "text/html", index_html);
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}
void handle_get_all_data(){
  
    // 讀取圖片
    File file = SPIFFS.open("/photo.jpg", FILE_READ);
    String imageBase64;
    if (file) {
      int picSize = file.size();
      uint8_t *buf = new uint8_t[picSize];
      file.read(buf, picSize);
      file.close();

      // 將圖片轉換為 Base64 格式
      imageBase64 = base64::encode(buf, picSize);
      delete[] buf;
    } else {
      Serial.println("Failed to open file for reading");
      server.send(500, "text/plain", "Internal Server Error");
      return;
    }

    // 創建 JSON 對象
    String jsonData = "{";
    jsonData += "\"image\":\"" + imageBase64 + "\",";
    jsonData += "\"maxTemp\":" + String(maxTemp) + ",";
    jsonData += "\"minTemp\":" + String(minTemp) + ",";
    jsonData += "\"avgTemp\":" + String(aveTemp);
    jsonData += "}";

    // 傳送 JSON 數據
    server.send(200, "application/json", jsonData);
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

String packageData(){
  StaticJsonDocument<200> doc;
  doc["image"] = SPIFFS_FILE_PHOTO_ESP;
  doc["tempMin"] = minTemp;
  doc["tempMax"] = maxTemp;
  doc["tempAvg"] = aveTemp;

  String json;
  serializeJson(doc, json);

  return json;
}

void logMemory() {
  Serial.println("Used RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " bytes");
  Serial.println("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
}



  
