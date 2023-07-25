#include <Adafruit_MLX90640.h>

#include <FS.h>   // SD Card ESP32

#define FILE_PHOTO "/photo.jpg"  // Photo File Name to save in SPIFFS，定義一個空白的jpg檔案


// =======Variable define=======
bool takeNewPhoto = false;   // 拍一張新照片
bool savePhotoSD = false;    // 儲存照片到sd卡
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check sd status
int imageCount = 1;                // File Counter

// **************** instantiation ****************
Adafruit_MLX90640 mlx; // 創建MLX90640實例

void capturePhotoSaveSpiffs(void);
void photo_save_to_SD_card(const char * fileName);
void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len);
bool checkPhoto( fs::FS &fs );
void getSpiffImg(String path, String TyPe);
void CaptureThermalPhotoSaveSpiffs(void);
