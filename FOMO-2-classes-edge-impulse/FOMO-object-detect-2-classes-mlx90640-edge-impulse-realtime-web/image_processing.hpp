/**
 * @brief      校正圖片，可以將一張照片縮放，並且再裁切回原本的大小
 * 
 * @param[in]  thermal_buf                        溫度的 buffer, 由於是用 mlx90640，所以會是 32x24 的 buffer
 * 
 * 
 * 
 * 
 * 
 */
void two_image_addition(uint8_t * srcImage1, 
                        float srcProportion1, 
                        uint8_t * srcImage2, 
                        float srcProportion2, 
                        uint8_t *dstImage, 
                        uint16_t width, 
                        uint16_t height, 
                        uint16_t channels)
{
  for(int i = 0; i < width*height*channels; i++){
    dstImage[i] = (srcImage1[i] * srcProportion1) + (srcImage2[i] * srcProportion2);
  }
}
