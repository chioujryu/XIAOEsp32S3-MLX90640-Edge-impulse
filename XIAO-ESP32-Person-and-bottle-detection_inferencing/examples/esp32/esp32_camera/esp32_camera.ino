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
#include <XIAO-ESP32-Person-and-bottle-detection_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <Adafruit_MLX90640.h> 

#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>   // SD Card ESP32
#include "SD.h"   // SD Card ESP32
#include "SPI.h"
#include "webpage.h"

// Select camera model - find more camera models in camera_pins.h file here
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h

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

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

/* Address defines -------------------------------------------------------- */
#define FILE_PHOTO "/photo.jpg"  // Photo File Name to save in SPIFFS，定義一個空白的jpg檔案
#define FILE_PHOTO_THERMAL "/thermal_photo.jpg"
#define FILE_BUFFER_THERMAL "/photo.txt"

/* Put your SSID & Password  ----------------------------------------------- */
const char* ssid = "test";  // Enter SSID here
const char* password = "test@1213";  //Enter Password here
const char* AP_ssid = "chioujryu";  // Enter SSID here
const char* AP_password = "123";  //Enter Password here

/* instantiation   --------------------------------------------------------- */
WebServer server(80);  //創建 server 實例
Adafruit_MLX90640 mlx;// 創建 MLX90640實例

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static bool is_initialised = false;
uint8_t *snapshot_buf; //points to the output of the capture
float *thermal_buf; //points to the output of the thermal
uint8_t *adding_image_buf; //points to the output of the adding image
uint8_t *jpg_buf;
size_t jpg_size = 0;

boolean takeNewPhoto = false;   
boolean savePhotoSD = false;    
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check sd status
int imageCount = 1;                // File Counter

float minTemp = 100;  // max temperature
float maxTemp = 0;    // min temperature
char val; 

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

    // Wire begin
    Wire.begin(); 
    Wire.setClock(400000); // Wire.setClock(400000) 設置 I2C 時鐘速率為 400 kHz，這是 I2C 協議的標準速率之一，通常用於高速數據傳輸。


    // Mounting SPIFFS，啟用 SPIFFS 檔案系統的意思
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
  
    // MLX90640 IR Cam setup
    if (!mlx.begin()) {
      Serial.println("Failed to initialize MLX90640!");
      while (1);
    }
   
    // Connect to local wi-fi network
    Serial.println("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password); // 啟動 wi-fi
  
    // check wi-fi is connected to wi-fi networ
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");  
    Serial.println(WiFi.localIP());

    // 綁定http請求的回調函數
    server.on("/", handle_OnConnect);
    //server.on("/capture", ei_camera_capture);   // 讓他一直拍
    server.on("/save", handle_save);
    server.on("/saved_esp32_photo", []() {getSpiffImg(FILE_PHOTO, "image/jpg");});
    server.on("/saved_thermal_photo", []() {getSpiffImg(FILE_PHOTO_THERMAL, "image/jpg");});
    server.onNotFound(handle_NotFound);  // http請求不可用時的回調函數
    
    server.begin(); // 開啓WebServer功能
    Serial.println("HTTP server started");


    // Showing Starting continious inference in 2 seconds...
    ei_printf("\nStarting continious inference in 2 seconds...\n");
    ei_sleep(2000);
}

void handle_OnConnect() {
  server.send(200, "text/html", index_html);  // 傳網頁上去
}
void handle_save() {
  savePhotoSD = true;
  Serial.println("Saving Image to SD Card");
  server.send(200, "text/plain", "Saving Image to SD Card"); 
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}
void getSpiffImg(String path, String TyPe) 
{ 
  if(SPIFFS.exists(path))
  { 
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, TyPe);  // 串流返回文件。
    file.close();
  }
}

/**
* @brief      Get data and run inferencing
*
* @param[in]  debug  Get debug info if true
*/
void loop()
{

    // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
    if (ei_sleep(5) != EI_IMPULSE_OK) {
        return;
    }

    server.handleClient();  // WebServer 監聽客戶請求並處理

    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    thermal_buf = (float*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    adding_image_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    jpg_buf = (uint8_t*)malloc(15000);
    jpg_size = 0;

    // check if allocation was successful
    if(snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    /*
    if (takeNewPhoto) 
    {
      captureESP32PhotoSaveSpiffs();
      captureThermalPhotoSaveSpiffs();
      //takeNewPhoto = false;
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
    */



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


    free(snapshot_buf);
    free(thermal_buf);
    free(adding_image_buf);
    free(jpg_buf);
    
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

    /*
   // 圖片相疊
   two_image_rgb888_addition(
    snapshot_buf,
    0.5,
    thermal_buf,
    0.5,
    adding_image_buf,
    320,
    240)
    */

    
   //send_to_web(snapshot_buf, jpg_buf);
   

   esp_camera_fb_return(fb);

   if(!converted){
       ei_printf("Conversion failed\n");
       return false;
   }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
        || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
        out_buf,
        EI_CAMERA_RAW_FRAME_BUFFER_COLS,
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
        out_buf,
        img_width,
        img_height);
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

void send_to_web(uint8_t * rgb888_buf, uint8_t * jpg_buf){

  uint32_t fileSize = 0;
  
  fmt2jpg(rgb888_buf, 
        EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE, 
        EI_CAMERA_RAW_FRAME_BUFFER_COLS, 
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS, 
        PIXFORMAT_RGB888, 
        31, 
        &jpg_buf, 
        &jpg_size);
  
  File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE); // 將空白照片放進SPIFFS裡面
  
  // Insert the data in the photo file
  if (!file) {
  Serial.println("Failed to open file in writing mode");
  }
  else {
  file.write(jpg_buf, jpg_size); // payload (image), payload length，將拍到的照片寫進空白照片裡面
  }
  
  fileSize = file.size();   // 取得照片的大小
  
  file.close();   // 關閉照片
  
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif
