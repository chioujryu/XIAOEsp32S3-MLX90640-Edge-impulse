#include <SPIFFS.h>
#include <FS.h>   // SD Card ESP32
#include "SD.h"   // SD Card ESP32
#include "SPI.h"


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