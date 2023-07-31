/* Includes ---------------------------------------------------------------- */
#include <XIAO-ESP32-Person-and-bottle-detection_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
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

float * mlx90640To                              = (float *) malloc(32 * 24 * sizeof(float));
float * minTemp_and_maxTemp_and_aveTemp         = (float *) malloc(3 * sizeof(float));
uint8_t * rgb888_buf_32x24_mlx90640             = (uint8_t *) malloc(32 * 24 * 3 * sizeof(uint8_t));
uint8_t * rgb888_buf_320x240_mlx90640           = (uint8_t *) malloc(320 * 240 * 3 * sizeof(uint8_t));
int imageCount                                  = 0;


Adafruit_MLX90640 mlx; 

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#if defined(CAMERA_MODEL_XIAO_ESP32S3)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#define LED_GPIO_NUM      21

#else
#error "Camera model not selected"
#endif

// 宣告函數
void logMemory();
void mlx90640_capture();
void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len);
void configInitCamera();


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("setup");
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

  // Initialize SD card
  if(!SD.begin(21)){
    Serial.println("Card Mount Failed");
    return;}
  uint8_t cardType = SD.cardType();
  
  // determine if the type of SD card is available
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

  //PSRAM Initialisation
  if(psramInit()){
          Serial.println("The PSRAM is correctly initialized");
  }else{
          Serial.println("PSRAM does not work");
  }

  // MLX90640 setup
  if (!mlx.begin()) {
    Serial.println("Failed to initialize MLX90640!");
    while (1);
  }else{Serial.println("MLX90640 initialized!");}

  configInitCamera();

  logMemory();
}

void loop() {

  bool whether_to_check_memory_usage     = false;

  if(Serial.available()>0){
    char receivedChar = Serial.read();
    if (receivedChar == '1'){
      Serial.println("Enter '1' in serial, capture a photo using MLX90640");
      char filename[32];
      sprintf(filename, "/%d_image.jpg", imageCount);
      mlx90640_capture_and_save_to_SD_card(filename, minTemp_and_maxTemp_and_aveTemp);
      whether_to_check_memory_usage = true;
      imageCount++;
    } 
  }
  if (whether_to_check_memory_usage == true){logMemory();}

  delay(100);
}

void logMemory() {
  Serial.println("Used RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " bytes");
  Serial.println("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
}

void mlx90640_capture_and_save_to_SD_card(const char * file_dir, float * min_and_max_and_ave_temp){
  // define buffer pointer
  uint8_t * jpg_buf_esp_and_thermal    = NULL;
  size_t jpg_size                      = 0;
  uint16_t thermal_buf_width = 32;
  uint16_t thermal_buf_height = 24;

  mlx.getFrame(mlx90640To); // get thermal buffer

  find_min_max_average_temp(mlx90640To, thermal_buf_width, thermal_buf_height, min_and_max_and_ave_temp); // find min and max temparature

  MLX90640_thermal_to_rgb888(mlx90640To, rgb888_buf_32x24_mlx90640, min_and_max_and_ave_temp); 
  ei::image::processing::resize_image(rgb888_buf_32x24_mlx90640, 32, 24, rgb888_buf_320x240_mlx90640, 320, 240, 3);
  if(!fmt2jpg(rgb888_buf_320x240_mlx90640, 320*240*3, 320, 240, PIXFORMAT_RGB888, 31, &jpg_buf_esp_and_thermal, &jpg_size)){
  Serial.println("Converted esp JPG size: " + String(jpg_size) + " bytes");
  }

  writeFile(SD, file_dir, jpg_buf_esp_and_thermal, jpg_size);

  // release buffer pointer
  free(jpg_buf_esp_and_thermal);
  jpg_buf_esp_and_thermal = NULL;
}

// SD card write file
void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len){
  
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
  }

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
void find_min_max_average_temp(float * buf, uint16_t width, uint16_t height, float * min_and_max_and_average){

  float minTemp = 100;
  float maxTemp = 0;
  float aveTemp = 0;

  // 找出圖像的最小溫度跟最大溫度
  for (int i = 0; i < width * height; i++) {  
    float temp = buf[i];  
    if (temp < minTemp) minTemp = temp;
    if (temp > maxTemp) maxTemp = temp; 
  }

  // 找出圖像的平均溫度
  for (int i = 0; i < width * height; i++){
    aveTemp += buf[i];
  }
  aveTemp = aveTemp / (width * height);

  min_and_max_and_average[0] = minTemp;
  min_and_max_and_average[1] = maxTemp;
  min_and_max_and_average[2] = aveTemp;

  Serial.println("minTemp: " + String(min_and_max_and_average[0]) + "  " +
                 "maxTemp: " + String(min_and_max_and_average[1]) + "  " +
                 "aveTemp: " + String(min_and_max_and_average[2]));
}

// 將單一 pixel 的溫度值，轉變成 R
uint8_t mapValueToColor_r(float temp, float * min_and_max_temp) {  // , float minTemp, float maxTemp
  
  float minTemp = min_and_max_temp[0];
  float maxTemp = min_and_max_temp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); 
  
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
uint8_t mapValueToColor_g(float temp, float * min_and_max_temp) {
  float minTemp = min_and_max_temp[0];
  float maxTemp = min_and_max_temp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); 
  
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
uint8_t mapValueToColor_b(float temp, float * min_and_max_temp) {
  float minTemp = min_and_max_temp[0];
  float maxTemp = min_and_max_temp[1];
  float ratio = (temp - minTemp) / (maxTemp - minTemp); 
  
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
void MLX90640_thermal_to_rgb888(float * thermal_buf, uint8_t * rgb888_buf, float * min_and_max_temp){  

  for (int i = 0; i < 768; i++) {
    float temperature = thermal_buf[i];
    uint8_t r = mapValueToColor_r(temperature, min_and_max_temp);
    uint8_t g = mapValueToColor_g(temperature, min_and_max_temp);
    uint8_t b = mapValueToColor_b(temperature, min_and_max_temp);
   
    rgb888_buf[i * 3] = b;  
    rgb888_buf[i * 3 + 1] = g;  
    rgb888_buf[i * 3 + 2] = r; 
  }
}
