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
#include "image_processing.hpp" 


/* Constant defines -------------------------------------------------------- */
const uint16_t CAMERA_RAW_FRAME_BUFFER_COLS         = 320;
const uint16_t CAMERA_RAW_FRAME_BUFFER_ROWS         = 240;
const uint16_t CAMERA_RESIZE_FRAME_BUFFER_COLS      = 32*16;
const uint16_t CAMERA_RESIZE_FRAME_BUFFER_ROWS      = 24*16;
const uint16_t MLX90640_RAW_FRAME_BUFFER_COLS       = 32;
const uint16_t MLX90640_RAW_FRAME_BUFFER_ROWS       = 24;
const uint16_t CAMERA_FRAME_BYTE_SIZE               = 3;

const String SPIFFS_FILE_PHOTO = "/photo.jpg";
const String SPIFFS_FILE_PHOTO_THERMAL = "/thermal_photo.jpg";
const String SPIFFS_FILE_BUFFER_THERMAL = "/photo.txt";

// Put your SSID & Password
const char* ssid = "test";  // Enter SSID here
const char* password = "test@1213";  //Enter Password here
const char* ap_ssid = "chioujryu";  // Enter SSID here
const char* ap_password = "0123456789";  //Enter Password here
bool ap_wifi_mode = true;

// instantiation 
WebServer server(80);  //創建 server 實例
Adafruit_MLX90640 mlx;// 創建 MLX90640實例

// Variable define
boolean takeNewPhoto = false;   // 拍一張新照片
boolean savePhotoSD = false;    // 儲存照片到sd卡
bool sd_sign = false;              // Check sd status
int imageCount = 1;                // File Counter


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
    captureCorrectedESP32andMLX90640PhotoAdditionSaveToSpiffs(mlx90640To, 
                                                              MLX90640_RAW_FRAME_BUFFER_COLS,
                                                              MLX90640_RAW_FRAME_BUFFER_ROWS,
                                                              rgb888_raw_buf_mlx90640,  
                                                              rgb888_resized_buf_mlx90640, 
                                                              CAMERA_RAW_FRAME_BUFFER_COLS,
                                                              CAMERA_RAW_FRAME_BUFFER_ROWS,
                                                              minTemp_and_maxTemp_and_aveTemp,
                                                              frame_buffer,
                                                              CAMERA_RAW_FRAME_BUFFER_COLS,
                                                              CAMERA_RAW_FRAME_BUFFER_ROWS,
                                                              rgb888_raw_buf_esp32,
                                                              rgb888_resized_buf_esp32,
                                                              32*16,
                                                              24*16,
                                                              120,
                                                              100,
                                                              rgb888_buf_esp_and_mlx90640_addition,
                                                              SPIFFS_FILE_PHOTO);                                       
    takeNewPhoto = false;
    logMemory();
  }

  // capture and save to SD card
  if (savePhotoSD) {
    char file_name[32];
    // save esp32 photo
    sprintf(file_name, "/%d_esp32_image.jpg", imageCount);
    esp32_photo_correction_save_to_SD_card( frame_buffer, 
                                            CAMERA_RAW_FRAME_BUFFER_COLS,
                                            CAMERA_RAW_FRAME_BUFFER_ROWS,
                                            rgb888_raw_buf_esp32,
                                            rgb888_resized_buf_esp32,
                                            32*16,
                                            24*16,
                                            120,
                                            100,
                                            file_name, 
                                            SPIFFS_FILE_PHOTO );
    
    Serial.printf("Saved picture：%s\n", file_name);

    // save thermal photo
    sprintf(file_name, "/%d_mlx90640_image.jpg", imageCount);
    mlx90640_photo_save_to_SD_card( mlx90640To, 
                                    rgb888_raw_buf_mlx90640, 
                                    rgb888_resized_buf_mlx90640, 
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
void captureCorrectedESP32andMLX90640PhotoAdditionSaveToSpiffs( float * thermal_buf, 
                                                                uint16_t thermal_buf_raw_width,
                                                                uint16_t thermal_buf_raw_height,
                                                                uint8_t * rgb888_buf_32x24_thermal,  
                                                                uint8_t * rgb888_buf_320x240_thermal, 
                                                                uint16_t thermal_buf_resized_width,
                                                                uint16_t thermal_buf_resized_height,
                                                                float * minTemp_and_maxTemp_and_aveTemp,
                                                                camera_fb_t * fb,
                                                                uint16_t esp_fb_raw_width,
                                                                uint16_t esp_fb_raw_height,
                                                                uint8_t * rgb888_raw_buf_esp32,
                                                                uint8_t * rgb888_resized_buf_esp32,
                                                                uint16_t esp_fb_resize_width,
                                                                uint16_t esp_fb_resize_height,
                                                                uint16_t esp_fb_crop_start_x,
                                                                uint16_t esp_fb_crop_start_y,
                                                                uint8_t * rgb888_buf_esp_and_mlx90640_addition,
                                                                const String spffs_file_dir) {

    // declare variable
    uint8_t * jpg_buf_esp_and_thermal_addition = NULL;
    size_t jpg_size = 0;
  
    // mlx90640 capture
    mlx.getFrame(thermal_buf); // get thermal buffer
    find_min_max_average_temp(thermal_buf, thermal_buf_raw_width, thermal_buf_raw_height, minTemp_and_maxTemp_and_aveTemp);
    MLX90640_thermal_to_rgb888(thermal_buf, rgb888_buf_32x24_thermal, minTemp_and_maxTemp_and_aveTemp); 
    ei::image::processing::resize_image(rgb888_buf_32x24_thermal, 
                                        32, 
                                        24, 
                                        rgb888_buf_320x240_thermal, 
                                        thermal_buf_resized_width, 
                                        thermal_buf_resized_height, 
                                        3);
  
    // ESP32S3 Capture
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);  // dispose the buffered image
    fb = NULL; // reset to capture errors
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    }
    if(!fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_raw_buf_esp32)){Serial.println("fmt2jpg function failed");};
    ei::image::processing::resize_image(rgb888_raw_buf_esp32, 
                                        esp_fb_raw_width, 
                                        esp_fb_raw_height, 
                                        rgb888_resized_buf_esp32, 
                                        esp_fb_resize_width, 
                                        esp_fb_resize_height, 
                                        3);
                                        
    ei::image::processing::crop_image_rgb888_packed(rgb888_resized_buf_esp32, 
                                                    esp_fb_resize_width, 
                                                    esp_fb_resize_height, 
                                                    esp_fb_crop_start_x, 
                                                    esp_fb_crop_start_y, 
                                                    rgb888_raw_buf_esp32, 
                                                    esp_fb_raw_width, 
                                                    esp_fb_raw_height);

    // image addition
    two_image_addition( rgb888_raw_buf_esp32, 
                        0.5, 
                        rgb888_resized_buf_mlx90640, 
                        0.5, 
                        rgb888_buf_esp_and_mlx90640_addition, 
                        320, 
                        240, 
                        3);

    // rgb888 to jpg
    if( !fmt2jpg( rgb888_buf_esp_and_mlx90640_addition, 
                  esp_fb_raw_width*esp_fb_raw_height*3, 
                  320, 
                  240, 
                  PIXFORMAT_RGB888, 
                  31, 
                  &jpg_buf_esp_and_thermal_addition, 
                  &jpg_size)){Serial.println("Converted esp JPG size: " + String(jpg_size) + " bytes");}
    
    // save image in spiffs
    File file = SPIFFS.open(spffs_file_dir , FILE_WRITE); // 將空白照片放進SPIFFS裡面
  
    // ===Insert the data in the photo file===
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(jpg_buf_esp_and_thermal_addition, jpg_size); // payload (image), payload length，將拍到的照片寫進空白照片裡面
    }
    
    int fileSize = file.size();   // 取得照片的大小
    
    // Close the file
    file.close();   // 關閉照片
  
    // check if file has been correctly saved in SPIFFS
    if(checkPhotoExitInSPIFFS(SPIFFS, spffs_file_dir)){Serial.println("SPIFFS has file");}
    else{Serial.println("SPIFFS has no file");}
  
    Serial.println("The picture has a size of " + String(fileSize) + " bytes");

    // release buffer
    free(jpg_buf_esp_and_thermal_addition);
    esp_camera_fb_return(fb);//釋放照片
    fb = NULL;
}


// Save esp32 photo to SD card
void esp32_photo_correction_save_to_SD_card(camera_fb_t * fb, 
                                            uint16_t esp_fb_raw_width,
                                            uint16_t esp_fb_raw_height,
                                            uint8_t * src_rgb888_buf,
                                            uint8_t * resized_rgb888_buf,
                                            uint16_t esp_fb_resize_width,
                                            uint16_t esp_fb_resize_height,
                                            uint16_t esp_fb_crop_start_x,
                                            uint16_t esp_fb_crop_start_y,
                                            const char * fileName, 
                                            const String spiffs_file_dir) {
    
    // declare variable
    uint8_t * jpg_buf_esp_and_thermal_addition = NULL;
    size_t jpg_size = 0;
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

      if(!fmt2rgb888(fb->buf, fb->len, fb->format, src_rgb888_buf)){Serial.println("fmt2jpg function failed");};
      ei::image::processing::resize_image(src_rgb888_buf, 
                                          esp_fb_raw_width, 
                                          esp_fb_raw_height, 
                                          resized_rgb888_buf, 
                                          esp_fb_resize_width, 
                                          esp_fb_resize_height, 
                                          3);
                                          
      ei::image::processing::crop_image_rgb888_packed(resized_rgb888_buf, 
                                                      esp_fb_resize_width, 
                                                      esp_fb_resize_height, 
                                                      esp_fb_crop_start_x, 
                                                      esp_fb_crop_start_y, 
                                                      src_rgb888_buf, 
                                                      esp_fb_raw_width, 
                                                      esp_fb_raw_height);
                                                      
      // rgb888 to jpg
      if( !fmt2jpg( src_rgb888_buf, 
                    esp_fb_raw_width*esp_fb_raw_height*3, 
                    esp_fb_raw_width, 
                    esp_fb_raw_height, 
                    PIXFORMAT_RGB888, 
                    31, 
                    &jpg_buf_esp_and_thermal_addition, 
                    &jpg_size)){Serial.println("Converted esp JPG size: " + String(jpg_size) + " bytes");}
  
      // Save photo to file
      write_file_jpg(SD, fileName, jpg_buf_esp_and_thermal_addition, jpg_size);
     
      // Insert the data in the photo file of SPIFFS
      File file = SPIFFS.open(spiffs_file_dir, FILE_WRITE);
      file.write(jpg_buf_esp_and_thermal_addition, jpg_size); // payload (image), payload length, fb->len
      fileSize = file.size();   // get file size
      file.close();
  
      // Release image buffer
      free(jpg_buf_esp_and_thermal_addition);
      jpg_buf_esp_and_thermal_addition = NULL;
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



  
