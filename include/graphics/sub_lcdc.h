#ifndef _SUB_LCDC_H
#define _SUB_LCDC_H

#include <stdint.h>

/* The monochrome touch-panel LCD mounted below the main 528x320 display. */
#define SUB_LCDC_WIDTH  208u
#define SUB_LCDC_HEIGHT 64u

/*
 * The buffer is row-major and MSB-first: bit 7 is x % 8 == 0.
 * sub_lcdc_flush() converts it to the controller's page-oriented bytes.
 */
void sub_lcdc_init(void);
void sub_lcdc_clear(uint8_t color);
void sub_lcdc_set_pixel(uint16_t x, uint16_t y, uint8_t color);
uint8_t sub_lcdc_get_pixel(uint16_t x, uint16_t y);
void sub_lcdc_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);
void sub_lcdc_flush(void);

#endif /* _SUB_LCDC_H */
