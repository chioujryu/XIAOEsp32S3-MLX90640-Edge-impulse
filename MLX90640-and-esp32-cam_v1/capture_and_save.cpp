#include "capture_and_save.h"

#include <SPIFFS.h>
#include <FS.h>   // SD Card ESP32
#include "SD.h"   // SD Card ESP32
#include "SPI.h"
#include "esp_camera.h"
#include "esp_timer.h"

#include <stdint.h>

#include "buffer_frame.h"
#include "image_processing.h"

// =======Capture Photo and Save it to SPIFFS=======
void capturePhotoSaveSpiffs(void) {
  camera_fb_t * fb = NULL; // FrameBuffer pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  int fileSize = 0;

  do 
  {
    // ===Take a photo with the camera===
    fb = esp_camera_fb_get(); // 拍一張照片（將照片放進照片緩衝區）
    if (!fb) 
    {
      Serial.println("Camera capture failed");
      return;
    }
    
    //// ===Photo for view===
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE); // 將空白照片放進SPIFFS裡面

    // ===Insert the data in the photo file===
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length，將拍到的照片寫進空白照片裡面
    }
    
    fileSize = file.size();   // 取得照片的大小
    file.close();   // Close the file
    esp_camera_fb_return(fb);  // 釋放照片緩衝區

    // ===check if file has been correctly saved in SPIFFS===
    ok = checkPhoto(SPIFFS);
  } while ( !ok );

  // ===印出儲存在 SPIFFS 照片的大小資訊===
  Serial.print("The picture has a ");
  Serial.print("size of ");
  Serial.print(fileSize);
  Serial.println(" bytes");
}


// =======Save pictures to SD card=======
void photo_save_to_SD_card(const char * fileName) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // ===Take a photo===
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Failed to get camera frame buffer");
      return;
    }

    // ===Save photo to file===
    writeFile(SD, fileName, fb->buf, fb->len);   // fileName = image1.jpg
  
    //// Insert the data in the photo file 
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);
    
    
    file.write(fb->buf, fb->len); // payload (image), payload length, fb->len
    file.close();

    // ===Release image buffer===
   esp_camera_fb_return(fb);  // 釋放照片緩衝區
      
    // ===check if file has been correctly saved in SPIFFS===
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
  Serial.println("Photo saved to file");
}

// =======SD card write file=======
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

// =======Check if photo capture was successful=======
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
};

/*
// =======Capture Thermal Photo and Save it to SPIFFS=======
void CaptureThermalPhotoSaveSpiffs(void) {
  thermal_buf = new float[768];
  delete[] thermal_buf;
  mlx.getFrame(thermal_buf);

  if (!thermal_buf) 
  {
    Serial.println("fail to take a thermal photo");
  }

  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  int fileSize = 0;

  // convert thermal to RGB888
  thermal_to_rgb888();

  // resize the RGB888

  // convert RGB888 to JPG
  

  do 
  {
    //// ===Photo for view===
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE); // 將空白照片放進SPIFFS裡面

    // ===Insert the data in the photo file===
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length，將拍到的照片寫進空白照片裡面
    }
    
    fileSize = file.size();   // 取得照片的大小
    file.close();   // Close the file
    esp_camera_fb_return(fb);  // 釋放照片緩衝區

    // ===check if file has been correctly saved in SPIFFS===
    ok = checkPhoto(SPIFFS);
  } while ( !ok );

  // ===印出儲存在 SPIFFS 照片的大小資訊===
  Serial.print("The picture has a ");
  Serial.print("size of ");
  Serial.print(fileSize);
  Serial.println(" bytes");
}

*/
