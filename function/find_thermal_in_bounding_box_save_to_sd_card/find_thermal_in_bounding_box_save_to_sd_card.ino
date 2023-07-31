/* Includes ---------------------------------------------------------------- */
#include <Adafruit_MLX90640.h>

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
#include "mlx90640_thermal_processing.hpp"


float * mlx90640To                             = (float *) malloc(32 * 24 * sizeof(float));
uint8_t * rgb888_buf_32x24_mlx90640            = (uint8_t *)malloc(32 * 24 * 3 * sizeof(uint8_t));
size_t imageCount                              = 0;
float * minTemp_and_maxTemp_and_aveTemp        = (float *) malloc(3 * sizeof(float));

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
      Serial.println("Ready to capture a photo using MLX90640");
      
      mlx90640_capture_save_sd_to_card( filename, 
                                        mlx90640To, 
                                        minTemp_and_maxTemp_and_aveTemp,
                                        rgb888_buf_32x24_mlx90640);
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

void mlx90640_capture_save_sd_to_card(const char * file_dir, 
                                      float * thermal_buf, 
                                      float * min_max_ave_temp,
                                      uint8_t * rgb888_buf_32x24_thermal)
{
  // define variable
  uint8_t * jpg_buf_thermal = NULL;
  size_t jpg_size = 0;
  uint16_t thermal_buf_width = 32;
  uint16_t thermal_buf_height = 24;

  // 處理 thermal 相片
  mlx.getFrame(thermal_buf); // get thermal buffer
  find_min_max_average_temp(thermal_buf, thermal_buf_width, thermal_buf_height, min_max_ave_temp);
  MLX90640_thermal_to_rgb888(thermal_buf, rgb888_buf_32x24_thermal, min_max_ave_temp); 
  
  if(!fmt2jpg(rgb888_buf_32x24_thermal, 32*24*3, 32, 24, PIXFORMAT_RGB888, 31, &jpg_buf_thermal, &jpg_size)){
  Serial.println("Converted esp JPG size: " + String(jpg_size) + " bytes");
  }

  
  uint16_t bounding_box_x = 3;
  uint16_t bounding_box_y = 3;
  uint16_t bounding_box_width = 6;
  uint16_t bounding_box_height = 6;
  find_min_max_average_thermal_in_bounding_box( thermal_buf, 
                                                        32, 
                                                        24, 
                                                        bounding_box_x, 
                                                        bounding_box_y, 
                                                        bounding_box_width, 
                                                        bounding_box_height, 
                                                        min_max_ave_temp);

  Serial.println("bounding box max thermal: " + String(min_max_ave_temp[0]) + 
                 "\nbounding box min thermal: " + String(min_max_ave_temp[1]) + 
                 "\nbounding box ave thermal: " + String(min_max_ave_temp[2]));
  

  
  writeFile(SD, file_dir, jpg_buf_thermal, jpg_size);

  // release buffer pointer
  free(jpg_buf_thermal);
  jpg_buf_thermal = NULL;
  
}

void find_min_max_average_thermal_in_bounding_box(float * thermal_buf, 
                                                  uint16_t thermal_buf_width, 
                                                  uint16_t thermal_buf_height,
                                                  uint16_t bounding_box_x, 
                                                  uint16_t bounding_box_y, 
                                                  uint16_t bounding_box_width, 
                                                  uint16_t bounding_box_height,
                                                  float * min_max_ave_thermal_in_bd_box)
{
  /*
  * @brief 在 bounding box 中找到溫度的最大, 最小, 平均值
  *
  * @param thermal_buf 溫度的 buffer, 如果是用MLX90640, 那 buffer 的寬跟高會是 32 x 24
  * @param thermal_buf_width 這是 buffer 的寬度
  * @param thermal_buf_height 這是 buffer 的高度
  * @param bounding_box_x 這是 bounding box 中心點的 x 
  * @param bounding_box_y 這是 bounding box 中心點的 y
  * @param bounding_box_width 這是 bounding box 的寬度
  * @param bounding_box_height 這是 bounding box 高度
  * @param min_max_ave_thermal_in_bd_box 這是你最後要取得的最大, 最小, 平均值
  */
  if (bounding_box_x > thermal_buf_width || bounding_box_x < 0 || bounding_box_y > thermal_buf_height || bounding_box_y < 0){
    Serial.println("center of bounding is wrong, the center need to in 32x24");
    return;
  }

  // 獲取 bounding box 左上角的座標
  uint16_t bd_box_topLeft_x = bounding_box_x - (bounding_box_width / 2);
  uint16_t bd_box_topLeft_y = bounding_box_y - (bounding_box_height / 2);
  uint16_t bd_box_topLeft_index = find_image_buffer_index(thermal_buf_width, bd_box_topLeft_x, bd_box_topLeft_y, 1);
  Serial.println("bd_box_topLeft_index: " + String(bd_box_topLeft_index));

  // define variable
  float * bounding_box_buffer = (float *)malloc(bounding_box_width * bounding_box_height * sizeof(float));
  
  // 將 bounding box 裡面的溫度值取出來
  for (size_t i = 0; i < bounding_box_height; i++){
    for (size_t k = 0; k < bounding_box_width; k++){
      bounding_box_buffer[(bounding_box_width * i) + k] = thermal_buf[bd_box_topLeft_index + (thermal_buf_width * i) + k];
    }
  }

  // 找到 bounding box 裡面的溫度最大，最小，平均值
  find_min_max_average_temp(bounding_box_buffer, bounding_box_width, bounding_box_height, min_max_ave_thermal_in_bd_box);

  // release buffer
  free(bounding_box_buffer);
  bounding_box_buffer = NULL;
}

uint32_t find_image_buffer_index( uint16_t image_width, 
                                  uint16_t x, 
                                  uint16_t y, 
                                  uint8_t channels)
{
  return (image_width * y + x) * channels;
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
    //ESP.restart();
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
