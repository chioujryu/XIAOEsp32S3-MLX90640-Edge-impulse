/* Includes ---------------------------------------------------------------- 
   這個你需要先在 Edge Impulse 訓練好模型，並下載成 Arduino 的 zip，才會有這個 Library*/
#include <XIAO_ESP32S3-CAM-Fruits-vs-Veggies-v2_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

// ===Adafruit_MLX90640===
#include <Adafruit_MLX90640.h>

#include <WiFi.h>
#include <WebServer.h>

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


/* Constant defines -------------------------------------------------------- */
#define CAMERA_RAW_FRAME_BUFFER_COLS              320
#define CAMERA_RAW_FRAME_BUFFER_ROWS              240
#define CAMERA_RESIZE_FRAME_BUFFER_COLS           160
#define CAMERA_RESIZE_FRAME_BUFFER_ROWS           120
#define MLX90640_RAW_FRAME_BUFFER_COLS            32
#define MLX90640_RAW_FRAME_BUFFER_ROWS            24
#define CAMERA_FRAME_BYTE_SIZE                    3

const String SPIFFS_FILE_PHOTO = "/photo.jpg";
const String SPIFFS_FILE_PHOTO_THERMAL = "/thermal_photo.jpg";
const String SPIFFS_FILE_BUFFER_THERMAL = "/photo.txt";


// Put your SSID & Password
const char* ssid = "test";  // Enter SSID here
const char* password = "test@1213";  //Enter Password here
const char* ap_ssid = "chioujryu";  // Enter SSID here
const char* ap_password = "0123456789";  //Enter Password here
bool ap_wifi_mode = false;

// instantiation 
WebServer server(80);  //創建 server 實例
Adafruit_MLX90640 mlx;// 創建 MLX90640實例


// Variable define
boolean takeNewPhoto = false;   // 拍一張新照片
boolean savePhotoSD = false;    // 儲存照片到sd卡
bool sd_sign = false;              // Check sd status
int imageCount = 1;                // File Counter


/* Buffer define -------------------------------------------------------- */
float * mlx90640To                       = (float *) malloc(32 * 24 * sizeof(float));
float * minTemp_and_maxTemp_and_aveTemp  = (float *) malloc(3 * sizeof(float));
uint8_t * rgb888_buf_32x24_mlx90640      = (uint8_t *) malloc(32 * 24 * 3 * sizeof(uint8_t));
uint8_t * rgb888_buf_320x240_mlx90640    = (uint8_t *) malloc(320 * 240 * 3 * sizeof(uint8_t));

camera_fb_t * frame_buffer               = NULL;
uint8_t * rgb888_buf_320x240_esp32       = (uint8_t *) malloc(320 * 240 * 3 * sizeof(uint8_t));
uint8_t * rgb888_buf_640x480_esp32       = (uint8_t *) malloc(640 * 480 * 3 * sizeof(uint8_t));

uint8_t * rgb888_buf_320x240_esp_and_thermal_addition = (uint8_t *) malloc(320 * 240 * 3 * sizeof(uint8_t));


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

  // MLX90640 setup
  if (!mlx.begin()) {
    Serial.println("Failed to initialize MLX90640!");
    while (1);
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
  server.on("/capture", handle_capture);
  server.on("/save", handle_save);
  server.on("/saved_esp32_photo", []() {getSpiffImg(server, SPIFFS_FILE_PHOTO , "image/jpg");});
  server.on("/saved_thermal_photo", []() {getSpiffImg(server, SPIFFS_FILE_PHOTO_THERMAL , "image/jpg");});
  server.onNotFound(handle_NotFound);  // http請求不可用時的回調函數
  server.begin(); 
  Serial.println("HTTP server started");
}



void loop()
{
  server.handleClient();  // WebServer 監聽客戶請求並處理

  // Only take photo and show on webserver
  if (takeNewPhoto) {
    captureESP32PhotoSaveSpiffs(frame_buffer, SPIFFS_FILE_PHOTO );
    captureMLX90640PhotoToRGB888SaveSpiffs( mlx90640To, 
                                            rgb888_buf_32x24_mlx90640,  
                                            rgb888_buf_320x240_mlx90640, 
                                            320,
                                            240,
                                            minTemp_and_maxTemp_and_aveTemp,
                                            SPIFFS_FILE_PHOTO_THERMAL) ;
                                            
    takeNewPhoto = false;
  }


  // capture and save to SD card
  if (savePhotoSD) {
    char file_name[32];
    // save esp32 photo
    sprintf(file_name, "/%d_esp32_image.jpg", imageCount);
    esp32_photo_save_to_SD_card(frame_buffer, file_name, SPIFFS_FILE_PHOTO );
    Serial.printf("Saved picture：%s\n", file_name);

    // save thermal photo
    sprintf(file_name, "/%d_mlx90640_image.jpg", imageCount);
    mlx90640_photo_save_to_SD_card( mlx90640To, 
                                    rgb888_buf_32x24_mlx90640, 
                                    rgb888_buf_320x240_mlx90640, 
                                    320,
                                    240,
                                    minTemp_and_maxTemp_and_aveTemp, 
                                    file_name,
                                    SPIFFS_FILE_PHOTO_THERMAL);
                                     
    Serial.printf("Saved picture：%s\n", file_name);

    // save thermal buffer
    sprintf(file_name, "/%d_thermal_32x24_buffer.txt", imageCount); 
    mlx90640_thermal_buffer_save_to_SD_card(mlx90640To, file_name);
    Serial.printf("Saved thermal buffer：%s\n", file_name);

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
  takeNewPhoto = true;
  Serial.println("Capturing Image for view only");
  server.send(200, "text/plain", "Taking Photo"); 
}
void handle_save() {
  savePhotoSD = true;
  Serial.println("Saving Image to SD Card");
  server.send(200, "text/plain", "Saving Image to SD Card"); 
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

// Capture ESP32 Photo and Save it to SPIFFS
void captureESP32PhotoSaveSpiffs(camera_fb_t * fb, const String spffs_file_dir) {

  // Capture
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL; // reset to capture errors
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
  }
  
  // Photo for view
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

  esp_camera_fb_return(fb);//釋放照片
  fb = NULL;

  // check if file has been correctly saved in SPIFFS
  if(checkPhotoExitInSPIFFS(SPIFFS, spffs_file_dir)){Serial.println("SPIFFS has file");}
  else{Serial.println("SPIFFS has no file");}

  Serial.println("The picture has a size of " + String(fileSize) + " bytes");
}


// Capture Thermal Photo and Save it to SPIFFS
void captureMLX90640PhotoToRGB888SaveSpiffs(  float * thermal_buf, 
                                              uint8_t * rgb888_buf_32x24_thermal,  
                                              uint8_t * rgb888_buf_320x240_thermal, 
                                              uint16_t resized_width,
                                              uint16_t resized_height,
                                              float * max_min_ave_temp,
                                              const String spiffs_file_dir) 
                                              
{
  size_t jpg_size = 0;
  uint8_t * jpg_buf_thermal = NULL;

  // Take a photo
  mlx.getFrame(thermal_buf); // 用來從 MLX90640 熱像儀中讀取一個數據幀，並將其存儲在 mlx90640To 數組中。
  
  // 取最大最小溫度
  find_min_max_average_temp(thermal_buf, 32, 24, max_min_ave_temp);

  MLX90640_thermal_to_rgb888(thermal_buf,  rgb888_buf_32x24_thermal, max_min_ave_temp);
  
  ei::image::processing::resize_image(rgb888_buf_32x24_thermal, 
                                      32, 
                                      24, 
                                      rgb888_buf_320x240_thermal, 
                                      resized_width, 
                                      resized_height, 
                                      3);   
  
  fmt2jpg(rgb888_buf_320x240_thermal, 
          resized_width*resized_height*3, 
          320, 
          240, 
          PIXFORMAT_RGB888, 
          31, 
          &jpg_buf_thermal, 
          &jpg_size);
          
  Serial.println("Converted thermal JPG size: " + String(jpg_size) + "bytes");

  // Insert the data in the photo file 
  File file = SPIFFS.open(spiffs_file_dir, FILE_WRITE);

  // 將檔案寫進照片裡面
  // ===Insert the data in the photo file===
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  }
  else {
    file.write(jpg_buf_thermal, jpg_size); // payload (image), payload length, fb->len
  }

  int fileSize = file.size();   // 取得照片的大小

  file.close();
 
  // check if file has been correctly saved in SPIFFS
  if(checkPhotoExitInSPIFFS(SPIFFS, spiffs_file_dir)){
    Serial.println("SPIFFS has file");
  }else{Serial.println("SPIFFS has no file");}
  

  Serial.println("The picture has a size of " + String(fileSize) + " bytes");

  // release buffer
  free(jpg_buf_thermal);
}


// Save esp32 photo to SD card
void esp32_photo_save_to_SD_card(camera_fb_t * fb, const char * fileName, const String spiffs_file_dir) {
  size_t fileSize = 0;
  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  do {
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
    write_file_jpg(SD, fileName, fb->buf, fb->len);
   
    // Insert the data in the photo file of SPIFFS
    File file = SPIFFS.open(spiffs_file_dir, FILE_WRITE);
    file.write(fb->buf, fb->len); // payload (image), payload length, fb->len
    fileSize = file.size();   // get file size
    file.close();

    // Release image buffer
    esp_camera_fb_return(fb);  // 釋放掉 fb 的記憶體
    fb = NULL;
      
    // check if file has been correctly saved in SPIFFS
    ok = checkPhotoExitInSPIFFS(SPIFFS, spiffs_file_dir);
  } while ( !ok );
  Serial.println("The picture has a size of " + String(fileSize) + " bytes");
}

void mlx90640_photo_save_to_SD_card(  float * thermal_buf, 
                                      uint8_t * rgb888_buf_32x24_thermal, 
                                      uint8_t * rgb888_buf_320x240_thermal, 
                                      uint16_t resized_width,
                                      uint16_t resized_height,
                                      float * max_min_ave_temp, 
                                      const char * fileName,
                                      const String spiffs_file_dir)
                                      
{

  // define buffer
  uint8_t * jpg_buf_thermal = NULL;
  size_t jpg_size = 0;
  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  size_t fileSize = 0;
  do
  {
    // ===Take a photo===
    mlx.getFrame(thermal_buf); // 用來從 MLX90640 熱像儀中讀取一個數據幀，並將其存儲在 mlx90640To 數組中。
    
    find_min_max_average_temp(thermal_buf, 32, 24, max_min_ave_temp);

    MLX90640_thermal_to_rgb888(thermal_buf,  rgb888_buf_32x24_thermal, max_min_ave_temp);
    
    ei::image::processing::resize_image(rgb888_buf_32x24_thermal, 32, 24, rgb888_buf_320x240_thermal, resized_width, resized_height, 3);   
    
    fmt2jpg(rgb888_buf_320x240_thermal, 
            resized_width * resized_height * 3, 
            resized_width, 
            resized_height, 
            PIXFORMAT_RGB888, 
            31, 
            &jpg_buf_thermal, 
            &jpg_size);
            
    Serial.println("Converted thermal JPG size: " + String(jpg_size) + "bytes");
    
  
    // Save photo to file
    write_file_jpg(SD, fileName, jpg_buf_thermal, jpg_size); 

    // Insert the data in the photo file 
    File file = SPIFFS.open(spiffs_file_dir, FILE_WRITE);
    file.write(jpg_buf_thermal, jpg_size); // payload (image), payload length, fb->len
    fileSize = file.size();   // get file size
    file.close();
   

    // check if file has been correctly saved in SPIFFS
    ok = checkPhotoExitInSPIFFS(SPIFFS, spiffs_file_dir);
  }while(!ok);

  Serial.print("\nThermal photo saved to file");

  free(jpg_buf_thermal);
}


void mlx90640_thermal_buffer_save_to_SD_card(float * thermal_buf ,const char * fileName){

  // get thermal buf from mlx90640
  mlx.getFrame(thermal_buf); 
   
  // 32x24 format 轉換成 string
  String thermalData = "";
  for (int i = 0; i < 768; i++) {   // 768 = 24*32
    thermalData += String(thermal_buf[i], 2);
    if (i < 767) {
      thermalData += ",";
    }
  }
  Serial.println("thermalData = " + String(thermalData));
  
  const char * data = thermalData.c_str();

  write_file_txt(SD, fileName, data);
}


void logMemory() {
  Serial.println("Used RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " bytes");
  Serial.println("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
}



  
