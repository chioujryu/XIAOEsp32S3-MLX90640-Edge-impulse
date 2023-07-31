// function Declaration
uint32_t find_image_buffer_index( uint16_t image_width, 
                                  uint16_t x, 
                                  uint16_t y, 
                                  uint8_t channels);
void draw_pixel(uint8_t * image_buf, uint32_t index, uint8_t channels, uint8_t r, uint8_t g, uint8_t b);
void bounding_box_correction( uint16_t original_buf_width, 
                              uint16_t original_buf_height, 
                              uint16_t original_bb_x, 
                              uint16_t original_bb_y, 
                              uint16_t original_bb_width, 
                              uint16_t original_bb_height,
                              uint16_t corrected_buf_width,
                              uint16_t corrected_buf_height,
                              uint16_t * corrected_bb_x, 
                              uint16_t * corrected_bb_y, 
                              uint16_t * corrected_bb_width, 
                              uint16_t * corrected_bb_height);


                              

                                
// function implementation
void draw_bounding_box(uint8_t * image_buf, 
                        uint16_t image_width, 
                        uint16_t image_height, 
                        uint8_t channels, 
                        uint16_t bounding_box_center_x, 
                        uint16_t bounding_box_center_y, 
                        uint16_t bounding_box_width, 
                        uint16_t bounding_box_height)
{
  
  //uint32_t center_index = find_image_buffer_index(image_width, bounding_box_center_x, bounding_box_center_y, channels);
  uint32_t bd_box_leftTop_x = bounding_box_center_x - (bounding_box_width / 2);
  uint32_t bd_box_leftTop_y = bounding_box_center_y - (bounding_box_height / 2);
  uint32_t bd_box_leftBottom_x = bounding_box_center_x - (bounding_box_width / 2);
  uint32_t bd_box_leftBottom_y = bounding_box_center_y + (bounding_box_height / 2);
  uint32_t bd_box_rightTop_x = bounding_box_center_x + (bounding_box_width / 2);
  uint32_t bd_box_rightTop_y = bounding_box_center_y - (bounding_box_height / 2);
  uint32_t bd_box_rightBottom_x = bounding_box_center_x + (bounding_box_width / 2);
  uint32_t bd_box_rightBottom_y = bounding_box_center_y + (bounding_box_height / 2);


  // draw top line
  for (int i = 0; i < bounding_box_width; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x + i, bd_box_leftTop_y, channels);
    draw_pixel(image_buf, rgb888_index, channels, 255, 0, 0);
  }

  // draw bottom line
  for (int i = 0; i < bounding_box_width; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftBottom_x + i, bd_box_leftBottom_y, channels);
    draw_pixel(image_buf, rgb888_index, channels, 255, 0, 0);
  }

  // draw left line
  for(int i = 0; i < bounding_box_height; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x, bd_box_leftTop_y + i, channels);
    draw_pixel(image_buf, rgb888_index, channels, 255, 0, 0);
  }

  // draw right line
  for(int i = 0; i < bounding_box_height; i++){
    uint32_t rgb888_index = find_image_buffer_index(image_width, bd_box_leftTop_x + bounding_box_width, bd_box_leftTop_y + i, channels);
    draw_pixel(image_buf, rgb888_index, channels, 255, 0, 0);
  }

}


uint32_t find_image_buffer_index( uint16_t image_width, 
                                  uint16_t x, 
                                  uint16_t y, 
                                  uint8_t channels)
{
  return (image_width * y + x) * channels;
}

void draw_pixel(uint8_t * image_buf, uint32_t index, uint8_t channels, uint8_t r, uint8_t g, uint8_t b){
  image_buf[index] = b;
  image_buf[index + 1] = g;
  image_buf[index + 2] = r;
  index = index + channels;
}



void bounding_box_correction( uint16_t original_buf_width, 
                              uint16_t original_buf_height, 
                              uint16_t original_bb_x, 
                              uint16_t original_bb_y, 
                              uint16_t original_bb_width, 
                              uint16_t original_bb_height,
                              uint16_t corrected_buf_width,
                              uint16_t corrected_buf_height,
                              uint16_t * corrected_bb_x, 
                              uint16_t * corrected_bb_y, 
                              uint16_t * corrected_bb_width, 
                              uint16_t * corrected_bb_height)
{
  *corrected_bb_x = original_bb_x * (corrected_buf_width / original_buf_width);
  *corrected_bb_y = original_bb_y * (corrected_buf_height / original_buf_height);
  *corrected_bb_width = original_bb_width * (corrected_buf_width / original_buf_width);
  *corrected_bb_height = original_bb_height * (corrected_buf_height / original_buf_height);
}
