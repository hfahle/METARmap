#include "ws2811.h"

extern int width;
extern int height;
extern int led_count;

extern int strip;
extern int dma;
extern int gpio;
extern int invert;

void matrix_render(void);
void matrix_clear(void);
void clear_ledstring(void);
ws2811_return_t init_led_string(void);
void finish_led_string(void);
void SetMatrixPixel(int pixnum, int iColorIndex);

