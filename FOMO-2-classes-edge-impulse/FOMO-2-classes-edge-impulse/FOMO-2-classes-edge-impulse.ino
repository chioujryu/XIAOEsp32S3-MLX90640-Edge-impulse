/* Edge Impulse Arduino examples
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Includes ---------------------------------------------------------------- */
#include <Object-detection-FOMO-96x96-2-classes_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <Adafruit_MLX90640.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <time.h>

#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "esp_camera.h"
#include <SPIFFS.h>
#include <FS.h>   // SD Card ESP32
#include "SD.h"   // SD Card ESP32
#include "SPI.h"

#include "Esp.h"

#include "write_read_file.hpp"
#include "image_processing.hpp"
#include "bounding_box_processing.hpp"
#include "mlx90640_thermal_processing.hpp"
#include "webpage.h"

Adafruit_MLX90640 mlx; 
WebServer server(80);

// Select camera model - find more camera models in camera_pins.h file here
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h

//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#if defined(CAMERA_MODEL_ESP_EYE)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23

#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

#elif defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#elif defined(CAMERA_MODEL_XIAO_ESP32S3)
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

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RESIZED_FRAME_BUFFER_COLS       32*16
#define EI_CAMERA_RESIZED_FRAME_BUFFER_ROWS       24*16
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define img_width_for_inference                   96
#define img_height_for_inference                  96
#define MLX90640_RAW_FRAME_BUFFER_COLS            32
#define MLX90640_RAW_FRAME_BUFFER_ROWS            24
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static bool is_initialised = false;
uint8_t * snapshot_buf                          = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
uint8_t * resized_snapshot_buf                  = (uint8_t*)malloc(EI_CAMERA_RESIZED_FRAME_BUFFER_COLS * EI_CAMERA_RESIZED_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
uint8_t * croped_snapshot_buf                   = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
uint8_t * buf_after_addition                    = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
float * mlx90640To                              = (float *) malloc(MLX90640_RAW_FRAME_BUFFER_COLS * MLX90640_RAW_FRAME_BUFFER_ROWS * sizeof(float));
float * minTemp_and_maxTemp_and_aveTemp         = (float *) malloc(3 * sizeof(float));
float * min_max_ave_thermal_in_bd_box           = (float *) malloc(3 * sizeof(float));
uint8_t * rgb888_raw_buf_mlx90640               = (uint8_t *) malloc(MLX90640_RAW_FRAME_BUFFER_COLS * MLX90640_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE * sizeof(uint8_t));
uint8_t * rgb888_resized_buf_mlx90640           = (uint8_t *) malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE * sizeof(uint8_t));


size_t imageCount = 0;

const String SPIFFS_FILE_PHOTO_ESP = "/esp_photo.jpg";
const String SPIFFS_FILE_PHOTO_MLX90640 = "/thermal_photo.jpg";
const String SPIFFS_FILE_PHOTO_MLX90640_ESP = "/thermal_esp_photo.jpg";

// Put your SSID & Password
const char* ssid = "test";  // Enter SSID here
const char* password = "test@1213";  //Enter Password here
const char* ap_ssid = "chioujryu";  // Enter SSID here
const char* ap_password = "0123456789";  //Enter Password here
bool ap_wifi_mode = false;

char file_name[256];



static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

/* Function definitions ------------------------------------------------------- */
bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) ;
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);
void logMemory() ;
void mlx90640_capture(float * thermal_buf,
                      uint16_t thermal_buf_width,
                      uint16_t thermal_buf_height,
                      float * min_and_max_and_ave_temp, 
                      uint8_t * rgb888_buf_32x24_thermal, 
                      uint8_t * rgb888_buf_320x240_thermal);
void save_rgb888_image_to_spiffs( uint8_t * buf,
                                  uint16_t buf_width, 
                                  uint16_t buf_height, 
                                  uint16_t channels,  
                                  const String spffs_file_dir);

void handle_OnConnect();
void handle_NotFound();
void handleGetText_min_temp();
void handleGetText_max_temp();
void handleGetText_ave_temp() ;
                      
/**
* @brief      Arduino setup function
*/
void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    //comment out the below line to start inference immediately after upload
    while (!Serial);
    Serial.println("Edge Impulse Inferencing Demo");
    if (ei_camera_init() == false) {
        ei_printf("Failed to initialize Camera!\r\n");
    }
    else {
        ei_printf("Camera initialized\r\n");
    }

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

    // setup I2C
    Wire.begin(); 
    Wire.setClock(400000);

    // Mounting SPIFFS，啟用 SPIFFS 檔案系統的意思
    if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      ESP.restart();
    }
    else {
      Serial.println("SPIFFS mounted successfully");
    }

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
    server.on("/esp32_photo_url", []() {getSpiffImg(server, SPIFFS_FILE_PHOTO_ESP , "image/jpg");});
    server.on("/mlx90640_esp32_addition_photo_url", []() {getSpiffImg(server, SPIFFS_FILE_PHOTO_MLX90640_ESP , "image/jpg");});
    server.on("/mlx90640_photo_url", []() {getSpiffImg(server, SPIFFS_FILE_PHOTO_MLX90640 , "image/jpg");});
    server.on("/getText_mintemp", handleGetText_min_temp);
    server.on("/getText_maxtemp", handleGetText_max_temp);
    server.on("/getText_avetemp", handleGetText_ave_temp);
    server.onNotFound(handle_NotFound);  // http請求不可用時的回調函數
    server.begin(); 
    Serial.println("HTTP server started");
    

    logMemory(); // 查看記憶體使用量

    ei_printf("\nStarting continious inference in 2 seconds...\n");
    ei_sleep(2000);
}

/**
* @brief      Get data and run inferencing
*
* @param[in]  debug  Get debug info if true
*/
void loop()
{
    server.handleClient();
    
    // 獲取 loop 開始時的時間
    unsigned long start_time = millis();

    // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
    if (ei_sleep(5) != EI_IMPULSE_OK) {
        return;
    }

    // check if allocation was successful
    if(snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;


    // mlx90640 拍照
    mlx90640_capture(mlx90640To,
                     MLX90640_RAW_FRAME_BUFFER_COLS,
                     MLX90640_RAW_FRAME_BUFFER_ROWS,
                     minTemp_and_maxTemp_and_aveTemp, 
                     rgb888_raw_buf_mlx90640, 
                     rgb888_resized_buf_mlx90640);

    // 儲存圖片到 spiffs
    save_rgb888_image_to_spiffs(rgb888_resized_buf_mlx90640,
                                EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
                                EI_CAMERA_RAW_FRAME_BUFFER_ROWS, 
                                EI_CAMERA_FRAME_BYTE_SIZE,  
                                SPIFFS_FILE_PHOTO_MLX90640);
                     
    // esp32 拍照
    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    // 將照片 放大
    ei::image::processing::resize_image(snapshot_buf, 
                                        EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
                                        EI_CAMERA_RAW_FRAME_BUFFER_ROWS, 
                                        resized_snapshot_buf, 
                                        EI_CAMERA_RESIZED_FRAME_BUFFER_COLS, 
                                        EI_CAMERA_RESIZED_FRAME_BUFFER_ROWS, 
                                        EI_CAMERA_FRAME_BYTE_SIZE);

    // 裁切照片
    ei::image::processing::crop_image_rgb888_packed(resized_snapshot_buf, 
                                                    EI_CAMERA_RESIZED_FRAME_BUFFER_COLS, 
                                                    EI_CAMERA_RESIZED_FRAME_BUFFER_ROWS, 
                                                    120, 
                                                    100, 
                                                    croped_snapshot_buf, 
                                                    EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
                                                    EI_CAMERA_RAW_FRAME_BUFFER_ROWS);
    
    // 儲存圖片到 spiffs
    save_rgb888_image_to_spiffs(croped_snapshot_buf,
                                EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
                                EI_CAMERA_RAW_FRAME_BUFFER_ROWS, 
                                EI_CAMERA_FRAME_BYTE_SIZE,  
                                SPIFFS_FILE_PHOTO_ESP);
    

    // 照片相加
    two_image_addition( croped_snapshot_buf, 
                        0.5, 
                        rgb888_resized_buf_mlx90640, 
                        0.5, 
                        buf_after_addition, 
                        EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
                        EI_CAMERA_RAW_FRAME_BUFFER_ROWS, 
                        EI_CAMERA_FRAME_BYTE_SIZE);

    // 儲存圖片到 spiffs
    save_rgb888_image_to_spiffs(buf_after_addition,
                                EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
                                EI_CAMERA_RAW_FRAME_BUFFER_ROWS, 
                                EI_CAMERA_FRAME_BYTE_SIZE,  
                                SPIFFS_FILE_PHOTO_MLX90640_ESP);


    // 確認照片是否跟要放進模型推論的照片大小有一致
    bool do_resize = false;
    if ((img_width_for_inference != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
        || (img_height_for_inference != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    // 將照片縮小到推論的照片大小
    if(do_resize){
      ei::image::processing::resize_image(croped_snapshot_buf, 
                                          EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
                                          EI_CAMERA_RAW_FRAME_BUFFER_ROWS, 
                                          snapshot_buf,
                                          img_width_for_inference, 
                                          img_height_for_inference, 
                                          EI_CAMERA_FRAME_BYTE_SIZE);
    }

    

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        return;
    }

    // print the predictions
    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    bool bb_found = result.bounding_boxes[0].value > 0;
    for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
        auto bb = result.bounding_boxes[ix];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("    %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\n", bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);

        uint16_t corrected_bb_x;
        uint16_t corrected_bb_y;
        uint16_t corrected_bb_width;
        uint16_t corrected_bb_height;

        // 校正 bounding box，給圖片相加用
        bounding_box_correction(  img_width_for_inference, 
                                  img_height_for_inference, 
                                  bb.x, 
                                  bb.y, 
                                  bb.width, 
                                  bb.height,
                                  EI_CAMERA_RAW_FRAME_BUFFER_COLS,
                                  EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
                                  &corrected_bb_x, 
                                  &corrected_bb_y, 
                                  &corrected_bb_width, 
                                  &corrected_bb_height);

        // 改成固定 bounding box 的長跟寬
        //corrected_bb_width = 16*3;
        //corrected_bb_height = 16*3;

        
        // Draw Bounding box
        draw_bounding_box(buf_after_addition, 
                          EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
                          EI_CAMERA_RAW_FRAME_BUFFER_ROWS, 
                          EI_CAMERA_FRAME_BYTE_SIZE, 
                          corrected_bb_x, 
                          corrected_bb_y, 
                          corrected_bb_width, 
                          corrected_bb_height);
                         
        
        // 校正 bounding box，以獲取 mlx90640 的 bounding box thermal 溫度
        bounding_box_correction(  img_width_for_inference, 
                                  img_height_for_inference, 
                                  bb.x, 
                                  bb.y, 
                                  bb.width, 
                                  bb.height,
                                  32,
                                  24,
                                  &corrected_bb_x, 
                                  &corrected_bb_y, 
                                  &corrected_bb_width, 
                                  &corrected_bb_height);
                                  
                                  
        // 找到 bounding box 裡面的最小、最大以及平均溫度
        find_min_max_average_thermal_in_bounding_box( mlx90640To, 
                                                      32, 
                                                      24,
                                                      corrected_bb_x, 
                                                      corrected_bb_y, 
                                                      corrected_bb_width, 
                                                      corrected_bb_height,
                                                      min_max_ave_thermal_in_bd_box);

          
                                          
    }
    if (!bb_found) {
        ei_printf("    No objects found\n");
    }
#else
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\n", result.classification[ix].label,
                                    result.classification[ix].value);
    }
#endif

#if EI_CLASSIFIER_HAS_ANOMALY == 1
        ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif




    Serial.println("save image to SPIFFS");


    // 印出整體的溫度值
    Serial.println("Thermal in whole enviroment ----- Min Temp:" + String(minTemp_and_maxTemp_and_aveTemp[0]) + " | " +
                   "Max Temp: " + String(minTemp_and_maxTemp_and_aveTemp[1]) + " | " +
                   "Ave Temp: " + String(minTemp_and_maxTemp_and_aveTemp[2]));       
                      
    // 印出 bounding box 裡面的溫度
    Serial.println("Thermal in bounding box ----- Min Temp:" + String(min_max_ave_thermal_in_bd_box[0]) + " | " +
                   "Max Temp: " + String(min_max_ave_thermal_in_bd_box[1]) + " | " +
                   "Ave Temp: " + String(min_max_ave_thermal_in_bd_box[2]));  


    logMemory(); // 紀錄記憶體用量


    // 獲取 loop 結束時的時間
    unsigned long end_time = millis();
    // 計算並打印 loop 花費的時間
    unsigned long duration = end_time - start_time;
    Serial.println("Time taken: " + String(duration) + " ms");
}

/**
 * @brief   Setup image sensor & start streaming
 *
 * @retval  false if initialisation failed
 */
bool ei_camera_init(void) {

    if (is_initialised) return true;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x\n", err);
      return false;
    }

    sensor_t * s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1); // flip it back
      s->set_brightness(s, 1); // up the brightness just a bit
      s->set_saturation(s, 0); // lower the saturation
    }

#if defined(CAMERA_MODEL_M5STACK_WIDE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#elif defined(CAMERA_MODEL_XIAO_ESP32S3)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);
#elif defined(CAMERA_MODEL_ESP_EYE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    s->set_awb_gain(s, 1);
#endif

    is_initialised = true;
    return true;
}

/**
 * @brief      Stop streaming of sensor data
 */
void ei_camera_deinit(void) {

    //deinitialize the camera
    esp_err_t err = esp_camera_deinit();

    if (err != ESP_OK)
    {
        ei_printf("Camera deinit failed\n");
        return;
    }

    is_initialised = false;
    return;
}


/**
 * @brief      Capture, rescale and crop image
 *
 * @param[in]  img_width     width of output image
 * @param[in]  img_height    height of output image
 * @param[in]  out_buf       pointer to store output image, NULL may be used
 *                           if ei_camera_frame_buffer is to be used for capture and resize/cropping.
 *
 * @retval     false if not initialised, image captured, rescaled or cropped failed
 *
 */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        ei_printf("Camera capture failed\n");
        return false;
    }

   bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);

   esp_camera_fb_return(fb);
   fb = NULL;

   if(!converted){
       ei_printf("Conversion failed\n");
       return false;
   }

    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix + 2];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }
    // and done!
    return 0;
}

void logMemory() 
{
  Serial.println("Used RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " bytes");
  Serial.println("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
}

/**
 * @brief      Capture, thermal buf
 *
 * @param[in]  thermal_buf                        溫度的 buffer, 由於是用 mlx90640，所以會是 32x24 的 buffer
 * @param[in]  thermal_buf_width                  溫度 buffer 的寬度
 * @param[in]  thermal_buf_height                 溫度 buffer 的高度
 * @param[in]  min_and_max_and_ave_temp           溫度的最大、最小、平均溫度，要是有 3 個空間的浮點數指標陣列             
 * @param[in]  rgb888_buf_32x24_thermal           溫度的 buffer 轉成 rgb888
 * @param[out]  rgb888_buf_320x240_thermal        rgb888 320x240，會當作輸出參數
 */
void mlx90640_capture(float * thermal_buf,
                      uint16_t thermal_buf_width,
                      uint16_t thermal_buf_height,
                      float * min_and_max_and_ave_temp, 
                      uint8_t * rgb888_buf_32x24_thermal, 
                      uint8_t * rgb888_buf_320x240_thermal){
  mlx.getFrame(thermal_buf); // get thermal buffer
  find_min_max_average_temp(thermal_buf, thermal_buf_width, thermal_buf_height, min_and_max_and_ave_temp); // find min and max temparature
  MLX90640_thermal_to_rgb888(thermal_buf, rgb888_buf_32x24_thermal, min_and_max_and_ave_temp);
  ei::image::processing::resize_image(rgb888_buf_32x24_thermal, 32, 24, rgb888_buf_320x240_thermal, 320, 240, 3);
}

void save_rgb888_image_to_spiffs( uint8_t * buf,
                                  uint16_t buf_width, 
                                  uint16_t buf_height, 
                                  uint16_t channels,  
                                  const String spffs_file_dir)
{
    uint8_t * jpg_buf = NULL;
    size_t jpg_size = 0;
    if(!fmt2jpg(buf, 
                buf_width * buf_height * channels, 
                buf_width, 
                buf_height, 
                PIXFORMAT_RGB888, 
                31, 
                &jpg_buf, 
                &jpg_size)){Serial.println("Converted esp JPG size: " + String(jpg_size) + " bytes");}
    
    // save image in spiffs
    File file = SPIFFS.open(spffs_file_dir , FILE_WRITE); // 將空白照片放進SPIFFS裡面
  
    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(jpg_buf, jpg_size); // payload (image), payload length，將拍到的照片寫進空白照片裡面
    }
    
    int fileSize = file.size();   // 取得照片的大小
    Serial.println("SPIFF file size: " + String(fileSize));

    if(!checkPhotoExitInSPIFFS(SPIFFS, spffs_file_dir)){Serial.println("No file in the SPIFFS");};
    
    file.close();   // 關閉照片
 
    // release buffer
    free(jpg_buf);
}


void handle_OnConnect() {
  server.send(200, "text/html", index_html);
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}
void handleGetText_min_temp() {
  char charArray[20];
  //snprintf(charArray, sizeof(charArray), "%.2f", minTemp_and_maxTemp_and_aveTemp[0]);
  dtostrf(minTemp_and_maxTemp_and_aveTemp[0],        // 要轉換的浮點數
          8,            // 轉換後的字符串總寬度（包括小數點和正負號）
          2,            // 小數位數
          charArray);   // 存儲轉換結果的字符陣列
  server.send(200, "text/plain", charArray);
}
void handleGetText_max_temp() {
  char charArray[20]; 
  snprintf(charArray, sizeof(charArray), "%.2f", minTemp_and_maxTemp_and_aveTemp[1]);
  server.send(200, "text/plain", charArray);
}
void handleGetText_ave_temp() {
  char charArray[20]; 
  snprintf(charArray, sizeof(charArray), "%.2f", minTemp_and_maxTemp_and_aveTemp[2]);
  server.send(200, "text/plain", charArray);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif
