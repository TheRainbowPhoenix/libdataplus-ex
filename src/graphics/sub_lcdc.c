#include <graphics/sub_lcdc.h>

#define SUB_LCDC_BYTES_PER_ROW (SUB_LCDC_WIDTH / 8u)
#define SUB_LCDC0_COMMAND (*(volatile uint8_t *)0xB8000000u)
#define SUB_LCDC0_DATA    (*(volatile uint8_t *)0xB8010000u)
#define SUB_LCDC1_COMMAND (*(volatile uint8_t *)0xBA000000u)
#define SUB_LCDC1_DATA    (*(volatile uint8_t *)0xBA010000u)
#define SUB_LCDC_RESET_PORT (*(volatile uint8_t *)0xA4050138u)

static uint8_t framebuffer[SUB_LCDC_HEIGHT][SUB_LCDC_BYTES_PER_ROW];

static void sub_lcdc_delay(uint32_t count)
{
	while (count--) __asm__("nop");
}

static void sub_lcdc_command_both(uint8_t command)
{
	SUB_LCDC0_COMMAND = command;
	__asm__("synco");
	SUB_LCDC1_COMMAND = command;
	__asm__("synco");
}

void sub_lcdc_init(void)
{
	/* ROM32A 0x80064222, with its fixed board-presence check omitted. */
	SUB_LCDC_RESET_PORT &= (uint8_t)~8u;
	__asm__("synco");
	sub_lcdc_delay(10);
	SUB_LCDC_RESET_PORT |= 8u;
	__asm__("synco");
	sub_lcdc_delay(10);

	sub_lcdc_command_both(0xa2);
	sub_lcdc_command_both(0xa1);
	sub_lcdc_command_both(0xc0);
	sub_lcdc_command_both(0x25);
	/* ROM g_bSubLCDContrast boot value. */
	sub_lcdc_command_both(0x12);
	sub_lcdc_command_both(0xe5);
	sub_lcdc_command_both(0x34);
	sub_lcdc_command_both(0x3d);
	sub_lcdc_command_both(0x84);
	sub_lcdc_command_both(0x2b);
	sub_lcdc_delay(100);
	sub_lcdc_command_both(0x2f);
	sub_lcdc_delay(2000);
	sub_lcdc_command_both(0x40);
	sub_lcdc_clear(0);
	sub_lcdc_flush();
	sub_lcdc_command_both(0xaf);
}

static void sub_lcdc_write_page_byte(uint16_t x, uint8_t page, uint8_t value)
{
	(void)page;
	if ((x >> 3) > 12u) {
		SUB_LCDC1_COMMAND = (uint8_t)((x + 0x98u) & 0x0fu);
		__asm__("synco");
		SUB_LCDC1_DATA = value;
	} else {
		SUB_LCDC0_COMMAND = (uint8_t)((x + 0x1cu) & 0x0fu);
		__asm__("synco");
		SUB_LCDC0_DATA = value;
	}
	__asm__("synco");
}

void sub_lcdc_clear(uint8_t color)
{
	uint16_t y;
	uint16_t x;
	uint8_t value = color ? 0xffu : 0x00u;
	for (y = 0; y < SUB_LCDC_HEIGHT; ++y)
		for (x = 0; x < SUB_LCDC_BYTES_PER_ROW; ++x)
			framebuffer[y][x] = value;
}

void sub_lcdc_set_pixel(uint16_t x, uint16_t y, uint8_t color)
{
	uint8_t mask;
	if (x >= SUB_LCDC_WIDTH || y >= SUB_LCDC_HEIGHT)
		return;
	mask = (uint8_t)(0x80u >> (x & 7u));
	if (color)
		framebuffer[y][x >> 3] |= mask;
	else
		framebuffer[y][x >> 3] &= (uint8_t)~mask;
}

uint8_t sub_lcdc_get_pixel(uint16_t x, uint16_t y)
{
	if (x >= SUB_LCDC_WIDTH || y >= SUB_LCDC_HEIGHT)
		return 0;
	return (uint8_t)((framebuffer[y][x >> 3] & (uint8_t)(0x80u >> (x & 7u))) != 0);
}

void sub_lcdc_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color)
{
	uint16_t px;
	uint16_t py;
	uint16_t end_x = x + width;
	uint16_t end_y = y + height;
	if (end_x > SUB_LCDC_WIDTH) end_x = SUB_LCDC_WIDTH;
	if (end_y > SUB_LCDC_HEIGHT) end_y = SUB_LCDC_HEIGHT;
	for (py = y; py < end_y; ++py)
		for (px = x; px < end_x; ++px)
			sub_lcdc_set_pixel(px, py, color);
}

void sub_lcdc_flush(void)
{
	uint16_t x;
	uint8_t page;
	for (x = 0; x < SUB_LCDC_WIDTH; ++x) {
		for (page = 0; page < SUB_LCDC_HEIGHT / 8u; ++page) {
			uint8_t packed = 0;
			uint8_t bit;
			for (bit = 0; bit < 8u; ++bit) {
				if (sub_lcdc_get_pixel(x, (uint16_t)(page * 8u + bit)))
					packed |= (uint8_t)(1u << bit);
			}
			sub_lcdc_write_page_byte(x, page, packed);
		}
	}
}
