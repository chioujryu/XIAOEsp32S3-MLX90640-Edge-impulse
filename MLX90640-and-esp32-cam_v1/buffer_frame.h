#include <WebServer.h>
#include <WiFiClient.h>

// **************** instantiation ****************
WebServer server(80);  // 創建WebServer實例

// **************** Buffer define ****************
uint8_t * rgb888_buf_1;  // points to the rgb888 output of the capture
uint8_t * rgb888_buf_2;  // resize 過後的照片
uint8_t * jpg_buf;       // points to the jpg output of the capture
float * thermal_buf; // 用來存儲從 MLX90640 熱像儀中讀取的 768 個數據點。
