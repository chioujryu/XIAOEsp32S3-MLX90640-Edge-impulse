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
#include "Esp.h"

#include "Arduino.h"

#include "webpage.h"


/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define CAMERA_RESIZE_FRAME_BUFFER_COLS           160
#define CAMERA_RESIZE_FRAME_BUFFER_ROWS           120
#define CAMERA_THERMAL_FRAME_BUFFER_COLS          32
#define CAMERA_THERMAL_FRAME_BUFFER_ROWS          24
#define CAMERA_FRAME_BYTE_SIZE                    3


#define FILE_PHOTO "/photo.jpg"  // Photo File Name to save in SPIFFS，定義一個空白的jpg檔案
#define FILE_PHOTO_THERMAL "/thermal_photo.jpg"
#define FILE_BUFFER_THERMAL "/photo.txt"

// 從 camera_pins.h 啟用 CAMERA_MODEL_XIAO_ESP32S3
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#include "camera_pins.h" 


// =======Put your SSID & Password=======
const char* ssid = "test";  // Enter SSID here
const char* password = "test@1213";  //Enter Password here

//const char* ssid = "chioujryu";  // Enter SSID here
//const char* password = "123";  //Enter Password here

// ======= instantiation =======
WebServer server(80);  //創建 server 實例
Adafruit_MLX90640 mlx;// 創建 MLX90640實例

// =======Variable define=======
boolean takeNewPhoto = false;   // 拍一張新照片
boolean savePhotoSD = false;    // 儲存照片到sd卡
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check sd status
int imageCount = 1;                // File Counter

//float minTemp = 100;  // 最大溫度
//float maxTemp = 0;    // 最小溫度

char val; 



/* Buffer define -------------------------------------------------------- */
camera_fb_t * fb = NULL;
uint8_t * rgb888_buf_esp_320x240     = NULL; // points to the rgb888 output of the capture
uint8_t * rgb888_buf_esp_640x480     = NULL; // points to the rgb888 output of the capture
uint8_t * rgb888_buf_thermal_32x24   = NULL;
uint8_t * rgb888_buf_thermal_320x240 = NULL; // points to the rgb888 output of the capture
uint8_t * rgb888_buf_esp_and_thermal_addition_320x240 = NULL;
float * minTemp_and_maxTemp          = NULL;



// =====================setup=====================
void setup()
{
  // =======設定Serial=======
  Serial.begin(115200);

  Serial.println("setup");
  Serial.println("Total heap: " + String(ESP.getHeapSize()) + " bytes");
  Serial.println("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("Total PSRAM: " + String(ESP.getPsramSize()) + " bytes");
  Serial.println("Free PSRAM: " + String(ESP.getFreePsram()) + " bytes");

  Wire.begin(); 
  Wire.setClock(400000); // Wire.setClock(400000) 設置 I2C 時鐘速率為 400 kHz，這是 I2C 協議的標準速率之一，通常用於高速數據傳輸。

  //PSRAM Initialisation
  if(psramInit()){
          Serial.println("\nThe PSRAM is correctly initialized");
  }else{
          Serial.println("\nPSRAM does not work");
  }

  // =======Mounting SPIFFS，啟用 SPIFFS 檔案系統的意思=======
  if (!SPIFFS.begin(true)) 
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else 
  {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  // === MLX90640 setup ===
  if (!mlx.begin()) {
    Serial.println("Failed to initialize MLX90640!");
    while (1);
  }

  // =======Initialize the camera=======
  Serial.print("Initializing the camera module...");
  configInitCamera();
  Serial.println("Ok!");

  // =======Initialize SD card=======
  if(!SD.begin(21))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  // =======Determine if the type of SD card is available=======
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

  // ======= Calculate SD Size =======
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  
  // =======Connect to local wi-fi network=======
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // 啟動 wi-fi

  // =======check wi-fi is connected to wi-fi network=======
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  
  Serial.println(WiFi.localIP());
  

  /*
  // You can remove the password parameter if you want the AP to be open.
  // a valid password must have more than 7 characters
  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while(1);
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  */

  // =======Turn-off the 'brownout detector'=======
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector


  // =======綁定http請求的回調函數=======
  server.on("/", handle_OnConnect);
  server.on("/capture", handle_capture);
  server.on("/save", handle_save);
  server.on("/saved_esp32_photo", []() {getSpiffImg(FILE_PHOTO, "image/jpg");});
  server.on("/saved_thermal_photo", []() {getSpiffImg(FILE_PHOTO_THERMAL, "image/jpg");});
  server.onNotFound(handle_NotFound);  // http請求不可用時的回調函數

  server.begin(); // 開啓WebServer功能
  Serial.println("HTTP server started");




}

// =====================loop=====================
void loop()
{
  server.handleClient();  // WebServer 監聽客戶請求並處理

  rgb888_buf_esp_320x240     = (uint8_t *) ps_malloc(320*240*3*sizeof(uint8_t)); 
  rgb888_buf_esp_640x480     = (uint8_t *) ps_malloc(640*480*3*sizeof(uint8_t)); 
  rgb888_buf_thermal_32x24   = (uint8_t *) ps_malloc(32*24*3*sizeof(uint8_t)); 
  rgb888_buf_thermal_320x240 = (uint8_t *) ps_malloc(320*240*3*sizeof(uint8_t)); 
  rgb888_buf_esp_and_thermal_addition_320x240 = (uint8_t *) ps_malloc(320*240*3*sizeof(uint8_t)); 
  minTemp_and_maxTemp        = (float *) ps_malloc(2*sizeof(float));
  
  

  /*
  captureESP32PhotoSaveSpiffs();
  delay(100);
  captureThermalPhotoSaveSpiffs();
  */
  
  if(Serial.available()>0)
  {
    char receivedChar = Serial.read();
    if (receivedChar == '1'){
      /*
      // save esp32 photo
      char filename[32];
      sprintf(filename, "/%d_esp32_image.jpg", imageCount);  // filename 會等於 "/image1.jpg"、 "/image2.jpg" ....
      esp32_photo_save_to_SD_card(filename);
      Serial.printf("Saved picture：%s\n", filename);*/
      
      /*
      // save thermal photo
      char filename_thermal_photo[32];
      sprintf(filename_thermal_photo, "/%d_thermal_image.jpg", imageCount);  // filename 會等於 "/image1.jpg"、 "/image2.jpg" ....
      thermal_photo_save_to_SD_card(filename_thermal_photo);
      Serial.printf("Saved picture：%s\n", filename_thermal_photo);
        
      // save thermal buffer
      char filename_thermal_buffer[32];
      sprintf(filename_thermal_buffer, "/%d_thermal_32x24_buffer.txt", imageCount);  // filename 會等於 "/image1.jpg"、 "/image2.jpg" ....
      thermal_buffer_save_to_SD_card(filename_thermal_buffer);
      Serial.printf("Saved thermal buffer：%s\n", filename_thermal_buffer);*/
  
      // save esp32 image and thermal image addition
      char filename_esp_and_thermal_buffer[32];
      sprintf(filename_esp_and_thermal_buffer, "/%d_esp_and_thermal_addition.jpg", imageCount);  // filename 會等於 "/image1.jpg"、 "/image2.jpg" ....
      esp_and_thermal_image_addition_save_to_SD_card(filename_esp_and_thermal_buffer);
      Serial.printf("Saved esp and thermal image addition：%s\n", filename_esp_and_thermal_buffer);
      
      imageCount++;  
      savePhotoSD = false;

      logMemory();  // 查看記憶體用量
    }
  }
  delay(10);
  
  free(rgb888_buf_esp_320x240);
  free(rgb888_buf_esp_640x480);
  free(rgb888_buf_thermal_32x24);
  free(rgb888_buf_thermal_320x240);
  free(rgb888_buf_esp_and_thermal_addition_320x240);
  free(minTemp_and_maxTemp);
  
  rgb888_buf_esp_320x240 = NULL;
  rgb888_buf_esp_640x480 = NULL;
  rgb888_buf_thermal_32x24 = NULL;
  rgb888_buf_thermal_320x240 = NULL;
  rgb888_buf_esp_and_thermal_addition_320x240 = NULL;
  minTemp_and_maxTemp = NULL;

  
  
}

// =====================functions=====================

// =======webserver=======
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

/*
// =======Capture ESP32 Photo and Save it to SPIFFS=======
void captureESP32PhotoSaveSpiffs() {
  
  Serial.println("Ready to capture esp32 photo");
  
  uint8_t * jpg_buf_esp = NULL;
  size_t jpg_size = 0;
  
  // 清空照片的 buffer
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL; // reset to capture errors

  // 拍照
  fb = esp_camera_fb_get(); // 拍一張照片（將照片放進照片緩衝區）
  if (!fb) {
    Serial.println("Camera capture failed");
  }
  
  if(!fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_buf_esp_320x240)){Serial.println("fmt2jpg function failed");};
  ei::image::processing::resize_image(rgb888_buf_esp_320x240, 320, 240, rgb888_buf_esp_640x480, 640, 480, 3);    // 通道數
  if(!fmt2jpg(rgb888_buf_esp_640x480, 640*480*3, 640, 480, PIXFORMAT_RGB888, 31, &jpg_buf_esp, &jpg_size)){
    Serial.println("Converted esp JPG size: " + String(jpg_size) + " bytes");
  }

  
  // ===Photo for view===
  File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE); // 將空白照片放進SPIFFS裡面

  // ===Insert the data in the photo file===
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  }
  else {
    file.write(jpg_buf_esp, jpg_size); // payload (image), payload length，將拍到的照片寫進空白照片裡面
  }
  
  int fileSize = file.size();   // 取得照片的大小
  
  // Close the file
  file.close();   // 關閉照片

  // release buffer
  esp_camera_fb_return(fb);
  fb = NULL;
  free(jpg_buf_esp);
  jpg_buf_esp = NULL;
  jpg_size = 0;

  // ===check if file has been correctly saved in SPIFFS===
  if(checkPhotoExitInSPIFFS(SPIFFS)){Serial.println("SPIFFS has file");}
  else{Serial.println("SPIFFS has no file");}

  // ===印出儲存在 SPIFFS 照片的大小資訊===
  Serial.println("The picture has a size of " + String(fileSize) + " bytes");
}



// =======Capture Thermal Photo and Save it to SPIFFS=======
void captureThermalPhotoSaveSpiffs() {
  Serial.println("Ready to capture thermal photo");

  uint8_t * jpg_buf_thermal = NULL;
  size_t jpg_size = 0;
  mlx90640To = (float *) ps_malloc(768 * sizeof(float));
  
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  // ===Take a photo===
  mlx.getFrame(mlx90640To); // 用來從 MLX90640 熱像儀中讀取一個數據幀，並將其存儲在 mlx90640To 數組中。
  
  // 取最大最小溫度
  find_min_max_temp(mlx90640To, minTemp_and_maxTemp);

  thermal_to_rgb888(mlx90640To, rgb888_buf_thermal_32x24); 
  ei::image::processing::resize_image(rgb888_buf_thermal_32x24, 32, 24, rgb888_buf_thermal_320x240, 320, 240, 3);    // 通道數
  fmt2jpg(rgb888_buf_thermal_320x240, 320*240*3, 320, 240, PIXFORMAT_RGB888, 31, &jpg_buf_thermal, &jpg_size);
  Serial.println("Converted thermal JPG size: " + String(jpg_size) + "bytes");

  // Insert the data in the photo file 
  File file = SPIFFS.open(FILE_PHOTO_THERMAL, FILE_WRITE);

  // ===Insert the data in the photo file===
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  }
  else {
    file.write(jpg_buf_thermal, jpg_size); // payload (image), payload length, fb->len
  }

  int fileSize = file.size();   // 取得照片的大小

  file.close();
 
  // ===check if file has been correctly saved in SPIFFS===
  if(checkPhotoExitInSPIFFS(SPIFFS)){Serial.println("SPIFFS has file");}
  else{Serial.println("SPIFFS has no file");}

  // ===印出儲存在 SPIFFS 照片的大小資訊===
  Serial.println("The picture has a size of " + String(fileSize) + " bytes");

  // Release buffer
  free(jpg_buf_thermal);
  jpg_buf_thermal = NULL;
  jpg_size = 0;
  free(mlx90640To);
  mlx90640To = NULL;
}
*/

// =======Check if photo capture was successful=======
/*
這個程式碼使用檔案系統物件 fs 開啟名為 FILE_PHOTO 的檔案,並取得該檔案的大小 pic_sz。

然後檢查該照片檔案的大小是否大於 100 bytes。

如果大於 100 bytes 就回傳 true,表示照片檔案存在且檔案大小正確。

如果小於等於 100 bytes 就回傳 false,表示檔案可能不存在或檔案大小不正確。

所以這個函數用來檢查 FILE_PHOTO 這張照片檔案是否存在且大小正確。它利用檔案大小來判斷照片檔案是否合法有效。
 */
bool checkPhotoExitInSPIFFS( fs::FS &fs ) {
  File f_pic = fs.open(FILE_PHOTO);
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}


// 從 SPIFF 拿取照片
void getSpiffImg(String path, String TyPe) 
{ 
  if(SPIFFS.exists(path))
  { 
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, TyPe);  // 串流返回文件。
    file.close();
  }
}

/*
// =======Save esp32 photo to SD card=======
void esp32_photo_save_to_SD_card(const char * fileName) {

  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  do {
    // 清空照片
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);  // dispose the buffered image
    fb = NULL; // reset to capture errors

    // 拍照
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Failed to get camera frame buffer");
      return;
    }

    // ===Save photo to file===
    write_file_jpg(SD, fileName, fb->buf, fb->len);
   
    // Insert the data in the photo file 
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // 將檔案寫進照片裡面
    file.write(fb->buf, fb->len); // payload (image), payload length, fb->len

    file.close();

    // ===Release image buffer===
    esp_camera_fb_return(fb);  // 釋放掉 fb 的記憶體
    fb = NULL;
      
    // ===check if file has been correctly saved in SPIFFS===
    if(checkPhotoExitInSPIFFS(SPIFFS)){Serial.println("SPIFFS has file");}
    else{Serial.println("SPIFFS has no file");}
  } while ( !ok );
  
  Serial.print("ESP32 photo saved to file");
}

void thermal_photo_save_to_SD_card(const char * fileName){
  Serial.println("Ready to save thermal photo to SD card");
  uint8_t * jpg_buf_thermal = NULL;
  size_t jpg_size = 0;
  mlx90640To = (float *) ps_malloc(768 * sizeof(float));
  

  mlx.getFrame(mlx90640To); // 用來從 MLX90640 熱像儀中讀取一個數據幀，並將其存儲在 mlx90640To 數組中。


  
  // 取最大最小溫度
  find_min_max_temp(mlx90640To, minTemp_and_maxTemp);

  thermal_to_rgb888(mlx90640To, rgb888_buf_thermal_32x24); // 將開頭建立的 rgb888_buf_1 填滿 rgb 像素
  ei::image::processing::resize_image(rgb888_buf_thermal_32x24, 32, 24, rgb888_buf_thermal_320x240, 320, 240, 3);    // 通道數
  fmt2jpg(rgb888_buf_thermal_320x240, 320*240*3, 320, 240, PIXFORMAT_RGB888, 31, &jpg_buf_thermal, &jpg_size);
  Serial.printf("Converted thermal JPG size: %d bytes\n", jpg_size);

  // Save photo to SD card
  write_file_jpg(SD, fileName, jpg_buf_thermal, jpg_size); 

  // Insert the data in the photo file 
  File file = SPIFFS.open(FILE_PHOTO_THERMAL, FILE_WRITE);

  // 將檔案寫進照片裡面
  file.write(jpg_buf_thermal, jpg_size); // payload (image), payload length, fb->len

  file.close();

  // ===check if file has been correctly saved in SPIFFS===
  if(checkPhotoExitInSPIFFS(SPIFFS)){Serial.println("SPIFFS has file");}
  else{Serial.println("SPIFFS has no file");}


  Serial.println("Thermal photo saved to file");

  // release buffer
  free(mlx90640To);
  mlx90640To = NULL;
  free(jpg_buf_thermal);
  jpg_buf_thermal = NULL;
  jpg_size = 0;
  
}
*/

void esp_and_thermal_image_addition_save_to_SD_card(const char * fileName){
  
  Serial.println("Ready to save esp and thermal image addition to SD card");
  uint8_t * jpg_buf_esp_and_thermal = NULL;
  size_t jpg_size = 0;
  float * mlx90640To = (float *) ps_malloc(768 * sizeof(float));
  

  // 處理 thermal 相片
  mlx.getFrame(mlx90640To); // get thermal buffer
  thermal_to_rgb888(mlx90640To, rgb888_buf_thermal_32x24); 
  ei::image::processing::resize_image(rgb888_buf_thermal_32x24, 32, 24, rgb888_buf_thermal_320x240, 320, 240, 3);

  // 拍攝 esp 相片
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL; // reset to capture errors
  fb = esp_camera_fb_get(); // 拍一張照片（將照片放進照片緩衝區）
  if (!fb) {
    Serial.println("Camera capture failed");
  }
  
  if(!fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_buf_esp_320x240)){Serial.println("fmt2jpg function failed");};
  ei::image::processing::resize_image(rgb888_buf_esp_320x240, 320, 240, rgb888_buf_esp_640x480, 640, 480, 3);
  ei::image::processing::crop_image_rgb888_packed(rgb888_buf_esp_640x480, 640, 480, 160, 120, rgb888_buf_esp_320x240, 320, 240);
  two_image_addition(rgb888_buf_esp_320x240, 
                      0.5, 
                      rgb888_buf_thermal_320x240, 
                      0.5, 
                      rgb888_buf_esp_and_thermal_addition_320x240, 
                      320, 
                      240, 
                      3);

  
  if(!fmt2jpg(rgb888_buf_esp_and_thermal_addition_320x240, 320*240*3, 320, 240, PIXFORMAT_RGB888, 31, &jpg_buf_esp_and_thermal, &jpg_size)){
    Serial.println("Converted esp JPG size: " + String(jpg_size) + " bytes");
  }

  // Save photo to SD card
  write_file_jpg(SD, fileName, jpg_buf_esp_and_thermal, jpg_size); 
  
  // release buffer
  free(jpg_buf_esp_and_thermal);
  jpg_buf_esp_and_thermal = NULL;
  free(mlx90640To);
  mlx90640To = NULL;
  jpg_size = 0;
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL; // reset to capture errors
}

void logMemory() {
  Serial.println("Used RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " bytes");
  Serial.println("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
}

/*
void thermal_buffer_save_to_SD_card(const char * fileName){
  Serial.println("Ready to save thermal buffer to SD card");
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  // ===Take a photo===
  mlx.getFrame(mlx90640To); // 用來從 MLX90640 熱像儀中讀取一個數據幀，並將其存儲在 mlx90640To 數組中。
   
  // 32x24 format 轉換成 string
  String thermalData = "";
  for (int i = 0; i < 768; i++) {   // 768 = 24*32
    thermalData += String(mlx90640To[i],2);
    if (i < 767) {
      thermalData += ",";
    }
  }
  Serial.println("thermalData = " + String(thermalData));
  
  const char * data = thermalData.c_str();

  write_file_txt(SD, fileName, data);

  Serial.println("Thermal buffer saved to file");
}
*/

// SD card write file
void write_file_jpg(fs::FS &fs, const char * path, uint8_t * data, size_t len){
  
  Serial.printf("Writing file: %s\n", path);
  
  File file = fs.open(path, FILE_WRITE);
  if(!file){
      Serial.println("Failed to open file for writing");
      return;
  }
  if(file.write(data, len) != len){
      Serial.println("Write failed");
  }
  file.close();
}

void write_file_txt(fs::FS &fs, const char * path,const char * message){
    
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

// =======定義相機的 Pin 腳=======
void configInitCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; 
  config.frame_size = FRAMESIZE_QVGA;  // FRAMESIZE_QQVGA = 160x120 | FRAMESIZE_QVGA = 320x240 | FRAMESIZE_VGA = 640x480
  config.pixel_format = PIXFORMAT_JPEG; // for streaming  PIXFORMAT_GRAYSCALE, PIXFORMAT_JPEG
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;   // CAMERA_GRAB_LATEST （The only solution I have found to avoid that lag/delay with the buffered data is to add:）, CAMERA_GRAB_WHEN_EMPTY
  config.fb_location = CAMERA_FB_IN_PSRAM; 
  config.jpeg_quality = 5;   // 越低相片品質愈好
  config.fb_count = 1;
  
  // ===Initialize the Camera===
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x \n", err);
    return;
    //ESP.restart();
  }
  camera_sign = true; // Camera initialization check passes


  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2   設置飽和度
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
  s->set_aec2(s, 0);           // 0 = disable , 1 = enable
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 0);            // 0 = disable , 1 = enable
  s->set_wpc(s, 1);            // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 1);           // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
  s->set_vflip(s, 1);          // 0 = disable , 1 = enable
  s->set_dcw(s, 1);            // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);       // 0 = disable , 1 = enable  
}


// 找出陣列中的最大溫度跟最小溫度
void find_min_max_temp(float * mlx90640To, float * min_and_max){

  float minTemp = 100;
  float maxTemp = 0;

  // 找出圖像的最小溫度跟最大溫度
  for (int i = 0; i < 768; i++) {  
    float temp = mlx90640To[i];  
    if (temp < minTemp) minTemp = temp;
    if (temp > maxTemp) maxTemp = temp; 
  }

  min_and_max[0] = minTemp;
  min_and_max[1] = maxTemp;
}



// 將單一 pixel 的溫度值，轉變成 R
uint8_t mapValueToColor_r(float temp) {  // , float minTemp, float maxTemp
  
  float minTemp = minTemp_and_maxTemp[0];
  float maxTemp = minTemp_and_maxTemp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  
  float r;
  if (ratio >= 0.66) {
    r = 255;
  } else if (ratio >= 0.33) {
    r = 255;
  } else {
    r = 255 * ratio / 0.33;
  }
  uint8_t int_r = r;
  return int_r;
}

// 將單一 pixel 的溫度值，轉變成 G
uint8_t mapValueToColor_g(float temp) {
  float minTemp = minTemp_and_maxTemp[0];
  float maxTemp = minTemp_and_maxTemp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  
  float g;
  if (ratio >= 0.66) {
    g = 255 * (1 - (ratio - 0.66) / 0.34);
  } else if (ratio >= 0.33) {
    g = 255 * (ratio - 0.33) / 0.33;  // 修改此处使绿色显示较少
  } else {
    g = 255;
  }
  uint8_t int_g = g;

  return int_g;
}

// 將單一 pixel 的溫度值，轉變成 B
uint8_t mapValueToColor_b(float temp) {
  float minTemp = minTemp_and_maxTemp[0];
  float maxTemp = minTemp_and_maxTemp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  
  float b;
  if (ratio >= 0.66) {
    b = 255 * (1 - (ratio - 0.66) / 0.34);
  } else if (ratio >= 0.33) {
    b = 0;
  } else {
    b = 255 * (1 - ratio / 0.33);  // 修改此处使蓝色更深
  }
  uint8_t int_b = b;

  return int_b;
}

// 將溫度的 pixel 轉換成 RGB888
void thermal_to_rgb888(float * mlx90640To, uint8_t * rgb888_buf){  //float mlx90640To_data[768], float minTemp, float maxTemp

  // 將 mlx90640To_data 的每個像素的溫度值 mapping 成 rgb，並存到 rgb888_photo
  for (int i = 0; i < 768; i++) {
    float temperature = mlx90640To[i];
    uint8_t r = mapValueToColor_r(temperature);
    uint8_t g = mapValueToColor_g(temperature);
    uint8_t b = mapValueToColor_b(temperature);
   
    rgb888_buf[i * 3] = b;   // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
    rgb888_buf[i * 3 + 1] = g;  // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
    rgb888_buf[i * 3 + 2] = r;  // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
  }
}

void two_image_addition(uint8_t * srcImage1, 
                        float srcProportion1, 
                        uint8_t * srcImage2, 
                        float srcProportion2, 
                        uint8_t *dstImage, 
                        uint16_t width, 
                        uint16_t height, 
                        uint16_t channels)
{
  // 如果 兩張照片的透明比例相加不等於 1，則讓他們調整為相加等於 1
  if (srcProportion1 + srcProportion2 != 1){
      float sumProportion = (srcProportion1 + srcProportion2);
      srcProportion1 = srcProportion1 + (sumProportion/2);
      srcProportion2 = srcProportion2 + (sumProportion/2);
  }
  
  for(int i = 0; i < width*height*channels; i++){
    srcImage1[i] = srcImage1[i] * srcProportion1;
    srcImage2[i] = srcImage2[i] * srcProportion2;
  }

  for(int i = 0; i < width*height*channels; i++){
    dstImage[i] = srcImage1[i] + srcImage2[i];
  }
}
