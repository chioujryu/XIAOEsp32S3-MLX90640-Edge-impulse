// function Declaration
uint32_t find_image_buffer_index( uint16_t image_width, 
                                  uint16_t x, 
                                  uint16_t y, 
                                  uint8_t channels);

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



uint32_t find_image_buffer_index( uint16_t image_width, 
                                  uint16_t x, 
                                  uint16_t y, 
                                  uint8_t channels)
{
  return (image_width * y + x) * channels;
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
  *corrected_bb_x = original_bb_x * ((float)corrected_buf_width / (float)original_buf_width);
  *corrected_bb_y = original_bb_y * ((float)corrected_buf_height / (float)original_buf_height);
  *corrected_bb_width = original_bb_width * ((float)corrected_buf_width / (float)original_buf_width);
  *corrected_bb_height = original_bb_height * ((float)corrected_buf_height / (float)original_buf_height);
}
