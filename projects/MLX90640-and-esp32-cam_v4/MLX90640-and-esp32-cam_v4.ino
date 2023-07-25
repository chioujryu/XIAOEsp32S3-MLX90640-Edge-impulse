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

float minTemp = 100;  // 最大溫度
float maxTemp = 0;    // 最小溫度

char val; 



/* Buffer define -------------------------------------------------------- */
camera_fb_t * fb = NULL;
uint8_t * rgb888_buf_1 = NULL; // points to the rgb888 output of the capture
uint8_t * rgb888_buf_2 = NULL; // points to the rgb888 output of the capture

uint8_t * jpg_buf = NULL; // points to the jpg output of the capture


uint8_t * dstImage = NULL;  // resize 過後的照片

// RAW DATA OF IR CAM
float * mlx90640To; // 用來存儲從 MLX90640 熱像儀中讀取的 768 個數據點。




// =======Stores the camera configuration parameters=======
// 實例化 camera_config_t
// camera_config_t 是來自 ESP32 的官方相機函式庫 "esp_camera.h" 的結構體。
camera_config_t config; // camera_config_t 結構體是用來儲存相機的配置選項，例如解析度、幀速率、對比度等等。在使用相機函式庫時，我們可以創建一個 camera_config_t 對象，並使用相應的函數將其配置為相機所需的選項。

// =====================setup=====================
void setup()
{
  // =======設定Serial=======
  Serial.begin(115200);

  Wire.begin(); 
  Wire.setClock(400000); // Wire.setClock(400000) 設置 I2C 時鐘速率為 400 kHz，這是 I2C 協議的標準速率之一，通常用於高速數據傳輸。

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

  
  //esp_camera_fb_return(fb);  // 釋放照片緩衝區
  //fb = NULL;
  
  if (takeNewPhoto) 
  {
    captureESP32PhotoSaveSpiffs();
    captureThermalPhotoSaveSpiffs();
    takeNewPhoto = false;
  }
  if (savePhotoSD) 
  {
    // save esp32 photo
    char filename[32];
    sprintf(filename, "/%d_esp32_image.jpg", imageCount);  // filename 會等於 "/image1.jpg"、 "/image2.jpg" ....
    esp32_photo_save_to_SD_card(filename);
    Serial.printf("\nSaved picture：%s", filename);
    

    // save thermal photo
    char filename_thermal_photo[32];
    sprintf(filename_thermal_photo, "/%d_thermal_image.jpg", imageCount);  // filename 會等於 "/image1.jpg"、 "/image2.jpg" ....
    thermal_photo_save_to_SD_card(filename_thermal_photo);
    Serial.printf("\nSaved picture：%s", filename_thermal_photo);
      
    // save thermal buffer
    char filename_thermal_buffer[32];
    sprintf(filename_thermal_buffer, "/%d_thermal_32x24_buffer.txt", imageCount);  // filename 會等於 "/image1.jpg"、 "/image2.jpg" ....
    thermal_buffer_save_to_SD_card(filename_thermal_buffer);
    Serial.printf("\nSaved thermal buffer：%s", filename_thermal_buffer);
    
    imageCount++;  
    savePhotoSD = false;
    
  }
  delay(1);
  
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

// =======Capture ESP32 Photo and Save it to SPIFFS=======
void captureESP32PhotoSaveSpiffs( void ) {
  Serial.print("\nReady to capture esp32 photo");
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL; // reset to capture errors

  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  int fileSize = 0;
  
  do 
  {
    // ===Take a photo with the camera===
    fb = esp_camera_fb_get(); // 拍一張照片（將照片放進照片緩衝區）
    if (!fb) 
    {
      Serial.print("\nCamera capture failed");
      return;
    }

    // 縮小照片
    rgb888_buf_1 = RawJPGtoRGB888(fb, rgb888_buf_1);
    rgb888_buf_2 = resize_image(rgb888_buf_1, 320, 240, rgb888_buf_2, 96, 96, 3);
    size_t jpg_size = RGB888toJPG(rgb888_buf_2 , 96, 96);
    
    // ===Photo for view===
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE); // 將空白照片放進SPIFFS裡面

    // ===Insert the data in the photo file===
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      //file.write(fb->buf, fb->len); // payload (image), payload length，將拍到的照片寫進空白照片裡面
      file.write(jpg_buf, jpg_size); // payload (image), payload length，將拍到的照片寫進空白照片裡面
    }
    
    fileSize = file.size();   // 取得照片的大小
    
    // Close the file
    file.close();   // 關閉照片

    esp_camera_fb_return(fb);  // 釋放照片緩衝區
    fb = NULL;

    // ===check if file has been correctly saved in SPIFFS===
    ok = checkPhoto(SPIFFS);
  } while ( !ok );

  // ===印出儲存在 SPIFFS 照片的大小資訊===
  Serial.print("\nThe picture has a ");
  Serial.print("size of ");
  Serial.print(fileSize);
  Serial.print("bytes");
}


// =======Capture Thermal Photo and Save it to SPIFFS=======
void captureThermalPhotoSaveSpiffs( void ) {
  Serial.print("\nReady to capture thermal photo");
  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  int fileSize = 0;
  do 
  {
    mlx90640To = (float *) heap_caps_malloc(768*4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // ===Take a photo===
    mlx.getFrame(mlx90640To); // 用來從 MLX90640 熱像儀中讀取一個數據幀，並將其存儲在 mlx90640To 數組中。

    
    Serial.print("\nmlx90640To = ");
    for (int i = 0 ; i < 768; i++){  
      Serial.print(mlx90640To[i]);
      Serial.print(", ");
    }

    
    // 取最大最小溫度
    float * minTemp_and_maxTemp = find_min_max_temp(mlx90640To);
    minTemp = minTemp_and_maxTemp[0];
    maxTemp = minTemp_and_maxTemp[1];
  
    //Serial.printf("\ncaptureThermalPhotoSaveSpiffs     minTemp = %f, maxTemp = %f\n", minTemp, maxTemp);
  
    rgb888_buf_1 = thermal_to_rgb888(mlx90640To); // 將開頭建立的 rgb888_buf_1 填滿 rgb 像素
    rgb888_buf_2 = resize_image(rgb888_buf_1,32,24, rgb888_buf_2, 320,240,3);  // 將 32x24 變成 320x240
    size_t jpg_size = RGB888toJPG(rgb888_buf_2, 320, 240); // 將 RGB888 轉成 JPG 
    Serial.printf("\nConverted thermal JPG size: %d bytes", jpg_size);
   
    // Insert the data in the photo file 
    File file = SPIFFS.open(FILE_PHOTO_THERMAL, FILE_WRITE);
  
    // 將檔案寫進照片裡面
    file.write(jpg_buf, jpg_size); // payload (image), payload length, fb->len

    fileSize = file.size();   // 取得照片的大小
  
    file.close();
  
    // ===Release image buffer===
    heap_caps_free(jpg_buf);
    //free(jpg_buf);
    //jpg_buf = NULL;
   
    // ===check if file has been correctly saved in SPIFFS===
    ok = checkPhoto(SPIFFS);
    
  } while ( !ok );

  // ===印出儲存在 SPIFFS 照片的大小資訊===
  Serial.print("\nThe picture has a ");
  Serial.print("size of ");
  Serial.print(fileSize);
  Serial.println("bytes");
}


// ===將 RAW JPG 照片轉換成 RGB888===
uint8_t * RawJPGtoRGB888(camera_fb_t * fb, uint8_t * rgb888_buf){
  
  rgb888_buf = (uint8_t *) heap_caps_malloc(fb->width * fb->height * CAMERA_FRAME_BYTE_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  size_t RGB888_buf_size = 0;
  
  if(rgb888_buf == NULL)
  {
    printf("\nMalloc failed to allocate buffer for RGB888.");
  }
  else
  {
    // Convert the RAW image into JPG
    fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_buf);
  }
  
  esp_camera_fb_return(fb);  // 釋放照片緩衝區
  fb = NULL;    // 釋放照片緩衝區
  
  return rgb888_buf;
}

// ===將 RGB888 照片轉換成 JPG===
size_t RGB888toJPG(uint8_t * rgb888_buf, uint16_t height, uint16_t width){
  
  jpg_buf = (uint8_t *) heap_caps_malloc(10000, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  size_t jpg_size = 0;
  
  if(jpg_buf == NULL){
      Serial.printf("\nMalloc failed to allocate buffer for JPG.");
      fprintf(stderr, "\nmalloc failed: %s\n", strerror(errno));
      exit(1);
  }else{
      // Convert the RAW image into JPG
      // The parameter "31" is the JPG quality. Higher is better.
      fmt2jpg(rgb888_buf, height*width*3, height, width, PIXFORMAT_RGB888, 31, &jpg_buf, &jpg_size);
      Serial.printf("\nConverted JPG size: %d bytes", jpg_size);
  }
  
  heap_caps_free(rgb888_buf);
  rgb888_buf = NULL;
  
  return jpg_size;
}




// ===RGB888 Resize Image===
uint8_t * resize_image(uint8_t *srcImage,int srcWidth,int srcHeight,uint8_t *dstImage,int dstWidth,int dstHeight,int pixel_size_B){

  dstImage = (uint8_t *) heap_caps_malloc(dstHeight*dstWidth*pixel_size_B, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  
  ei::image::processing::resize_image(srcImage,
                                      srcWidth,
                                      srcHeight,
                                      dstImage,
                                      dstWidth,
                                      dstHeight,
                                      pixel_size_B);    // 通道數

  //free(srcImage);    // 釋放照片緩衝區
  heap_caps_free(srcImage);     // 釋放照片緩衝區
  //srcImage = NULL;   // 釋放照片緩衝區
  
  return dstImage;
}

// =======Check if photo capture was successful=======
/*
這個程式碼使用檔案系統物件 fs 開啟名為 FILE_PHOTO 的檔案,並取得該檔案的大小 pic_sz。

然後檢查該照片檔案的大小是否大於 100 bytes。

如果大於 100 bytes 就回傳 true,表示照片檔案存在且檔案大小正確。

如果小於等於 100 bytes 就回傳 false,表示檔案可能不存在或檔案大小不正確。

所以這個函數用來檢查 FILE_PHOTO 這張照片檔案是否存在且大小正確。它利用檔案大小來判斷照片檔案是否合法有效。
 */
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open(FILE_PHOTO);
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}


// =======從 SPIFF 拿取照片=======
void getSpiffImg(String path, String TyPe) 
{ 
  if(SPIFFS.exists(path))
  { 
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, TyPe);  // 串流返回文件。
    file.close();
  }
}


// =======Save esp32 photo to SD card=======
void esp32_photo_save_to_SD_card(const char * fileName) {

  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // dispose the buffered image
  fb = NULL; // reset to capture errors

  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  do {
    // ===Take a photo===
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Failed to get camera frame buffer");
      return;
    }

    // ===Save photo to file===
    writeFile(SD, fileName, fb->buf, fb->len);
   
    // Insert the data in the photo file 
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // 將檔案寫進照片裡面
    file.write(fb->buf, fb->len); // payload (image), payload length, fb->len

    file.close();

    // ===Release image buffer===
    esp_camera_fb_return(fb);  // 釋放掉 fb 的記憶體
    fb = NULL;
      
    // ===check if file has been correctly saved in SPIFFS===
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
  
  Serial.print("ESP32 photo saved to file");
}

void thermal_photo_save_to_SD_card(const char * fileName){
  Serial.print("\nReady to save thermal photo to SD card");
  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  mlx90640To = (float *) heap_caps_malloc(768*4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  do
  {
    // ===Take a photo===
    mlx.getFrame(mlx90640To); // 用來從 MLX90640 熱像儀中讀取一個數據幀，並將其存儲在 mlx90640To 數組中。
    
    Serial.print("\nmlx90640To = ");
    for (int i = 0 ; i < 32*24; i++){  
      Serial.print(mlx90640To[i]);
      Serial.print(", ");
    }
    
    // 取最大最小溫度
    float * minTemp_and_maxTemp = find_min_max_temp(mlx90640To);
    minTemp = minTemp_and_maxTemp[0];
    maxTemp = minTemp_and_maxTemp[1];
  
    Serial.printf("\nminTemp = %f, maxTemp = %f", minTemp, maxTemp);
  
    rgb888_buf_1 = thermal_to_rgb888(mlx90640To);                                                       // 將開頭建立的 rgb888_buf_1 填滿 rgb 像素
    rgb888_buf_2 = resize_image(rgb888_buf_1,32,24, rgb888_buf_2, 320,240,3);  // 將 32x24 變成 320x240
    size_t jpg_size = RGB888toJPG(rgb888_buf_2, 320, 240);                     // 將 RGB888 轉成 JPG 
    Serial.printf("\nConverted thermal JPG size: %d bytes", jpg_size);
  
    // ===Save photo to file===
    writeFile(SD, fileName, jpg_buf, jpg_size); 

    // Insert the data in the photo file 
    File file = SPIFFS.open(FILE_PHOTO_THERMAL, FILE_WRITE);
  
    // 將檔案寫進照片裡面
    file.write(jpg_buf, jpg_size); // payload (image), payload length, fb->len
  
    file.close();
  
    // ===Release image buffer===
    //free(jpg_buf);
    heap_caps_free(jpg_buf);
    //jpg_buf = NULL;
   
      
    // ===check if file has been correctly saved in SPIFFS===
    ok = checkPhoto(SPIFFS);
  }while(!ok);

  Serial.print("\nThermal photo saved to file");
}


void thermal_buffer_save_to_SD_card(const char * fileName){
  Serial.print("\nReady to save thermal buffer to SD card");
  
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
  Serial.print("\nthermalData = ");
  Serial.print(thermalData);

  //free(mlx90640To);
  heap_caps_free(mlx90640To);

  
  const char * data = thermalData.c_str();

  writeFileTxt(SD, fileName, data);

  Serial.print("\nThermal buffer saved to file");
}


// =======SD card write file=======
void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len){
    
  // 參數說明:
  // fs: 檔案系統物件
  // path: 欲開啟寫入的檔案路徑
  // data: 欲寫入檔案的資料指標 (optional)
  // message: 欲寫入檔案的資料指標 (optional)
  // len: data 的資料長度
  
  // 如果寫入的資料是照片的data，那請將 message 輸入為 "1"
  
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

void writeFileTxt(fs::FS &fs, const char * path,const char * message){
    
  Serial.printf("\nWriting file: %s", path);
  
  File file = fs.open(path, FILE_WRITE);
  if(!file){
      Serial.print("\nFailed to open file for writing");
      return;
  }
  if(file.print(message)){
    Serial.print("\nFile written");
  } else {
    Serial.print("\nWrite failed");
  }
  
  file.close();
}
    




// =======定義相機的 Pin 腳=======
void configInitCamera(){
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
    Serial.printf("Camera init failed with error 0x%x", err);
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
float * find_min_max_temp(float mlx90640To[768]){

  static float minTemp_and_maxTemp[2];

  float minTemp = 100;
  float maxTemp = 0;

  // 找出圖像的最小溫度跟最大溫度
  for (int i = 0; i < 768; i++) {  
    float temp = mlx90640To[i];  
    if (temp < minTemp) minTemp = temp;
    if (temp > maxTemp) maxTemp = temp; 
  }
  
  //Serial.println("");
  //Serial.printf("minTemp = %f", minTemp);
  //Serial.printf("\nmaxTemp = %f", maxTemp);

  minTemp_and_maxTemp[0] = minTemp;
  minTemp_and_maxTemp[1] = maxTemp;
  
  return minTemp_and_maxTemp;
}



// 將單一 pixel 的溫度值，轉變成 R
uint8_t mapValueToColor_r(float temp) {  // , float minTemp, float maxTemp
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  //Serial.printf("\nratio = %f", ratio);
  
  float r;
  if (ratio >= 0.66) {
    r = 255;
  } else if (ratio >= 0.33) {
    r = 255;
  } else {
    r = 255 * ratio / 0.33;
  }
  uint8_t int_r = r;
  //Serial.printf("\n int_r = %d \n", int_r);
  //Serial.printf("\n sizeof int_r = %d \n", sizeof(int_r));
  return int_r;
}

// 將單一 pixel 的溫度值，轉變成 G
uint8_t mapValueToColor_g(float temp) {  // , float minTemp, float maxTemp
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  //Serial.printf("\nratio = %f", ratio);
  
  float g;
  if (ratio >= 0.66) {
    g = 255 * (1 - (ratio - 0.66) / 0.34);
  } else if (ratio >= 0.33) {
    g = 255 * (ratio - 0.33) / 0.33;  // 修改此处使绿色显示较少
  } else {
    g = 255;
  }
  uint8_t int_g = g;
  //Serial.printf("\nint_g = %d  \n",int_g);
  //Serial.printf("\n sizeof int_g = %d  \n",sizeof(int_g));

  return int_g;
}

// 將單一 pixel 的溫度值，轉變成 B
uint8_t mapValueToColor_b(float temp) {  // , float minTemp, float maxTemp
  float ratio = (temp - minTemp) / (maxTemp - minTemp); // 這是計算數值在最小值和最大值之間所佔的比例。它等於 (value - minValue) / (maxValue - minValue)。
  //Serial.printf("\nratio = %f", ratio);
  
  float b;
  if (ratio >= 0.66) {
    b = 255 * (1 - (ratio - 0.66) / 0.34);
  } else if (ratio >= 0.33) {
    b = 0;
  } else {
    b = 255 * (1 - ratio / 0.33);  // 修改此处使蓝色更深
  }
  uint8_t int_b = b;
  //Serial.printf("\n int_b = %d  \n",int_b);
  //Serial.printf("\n int_b = %d  \n", sizeof(int_b));

  return int_b;
}

// 將溫度的 pixel 轉換成 RGB888
uint8_t * thermal_to_rgb888(float * mlx90640To){  //float mlx90640To_data[768], float minTemp, float maxTemp
  
  //Serial.println("");
  //Serial.printf("minTemp = %f", minTemp);
  //Serial.printf("\nmaxTemp = %f", maxTemp);


  rgb888_buf_1 = (uint8_t *) heap_caps_malloc(32*24*3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  // 將 mlx90640To_data 的每個像素的溫度值 mapping 成 rgb，並存到 rgb888_photo
  for (int i = 0; i < 768; i++) {
    float temperature = mlx90640To[i];
    //Serial.printf("temperature = %f", temperature);
    uint8_t r = mapValueToColor_r(temperature);
    uint8_t g = mapValueToColor_g(temperature);
    uint8_t b = mapValueToColor_b(temperature);
   
    rgb888_buf_1[i * 3] = b;   // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
    rgb888_buf_1[i * 3 + 1] = g;  // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
    rgb888_buf_1[i * 3 + 2] = r;  // imgData.data 是裡面有 24 x 32 x 4 個數值的 陣列
  }

  //free(mlx90640To);
  heap_caps_free(mlx90640To);
  mlx90640To = NULL;

  return rgb888_buf_1;
}

// 將溫度的 pixel 轉換成灰階 
uint8_t getGrayscaleValue(float temp, float minTemp, float maxTemp) {
  
  float ratio = (temp - minTemp) / (maxTemp - minTemp);  
  uint8_t value = ratio * 255;
  return value;
}
