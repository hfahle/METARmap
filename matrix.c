//#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "METARmap.h"
#include "matrix.h"

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
        },
        [1] =
        {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
        },
    },
};

ws2811_led_t *matrix;

void matrix_render(void)
{
    int x, y;

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            ledstring.channel[0].leds[(y * width) + x] = matrix[y * width + x];
        }
    }
   
    int ret = 0;
    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
	fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
    }
}

void matrix_clear(void)
{
    int x, y;

    for (y = 0; y < (height ); y++)
    {
        for (x = 0; x < width; x++)
        {
            matrix[y * width + x] = 0;
        }
    }
}

int dotspos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

ws2811_led_t dotcolors[] =
{
    0x00200000,  // red
    0x00201000,  // orange
    0x00202000,  // yellow
    0x00002000,  // green
    0x00002020,  // lightblue
    0x00000020,  // blue
    0x00100010,  // purple
    0x00200010,  // pink
};

ws2811_led_t dotcolors_rgbw[] =
{
    0x00200000,  // red
    0x10200000,  // red + W
    0x00002000,  // green
    0x10002000,  // green + W
    0x00000020,  // blue
    0x10000020,  // blue + W
    0x00101010,  // white
    0x10101010,  // white + W

};

void clear_ledstring(void) {
    int x, y;

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            ledstring.channel[0].leds[(y * width) + x] = 0;
        }
    }
    
    ws2811_render(&ledstring);
}

ws2811_return_t init_led_string(void)
{
    
    matrix = malloc(sizeof(ws2811_led_t) * width * height);
    
/*
 * PWM0, which can be set to use GPIOs 12, 18, 40, and 52.
 * Only 12 (pin 32) and 18 (pin 12) are available on the B+/2B/3B
 * PWM1 which can be set to use GPIOs 13, 19, 41, 45 and 53.
 * Only 13 is available on the B+/2B/PiZero/3B, on pin 33
 * PCM_DOUT, which can be set to use GPIOs 21 and 31.
 * Only 21 is available on the B+/2B/PiZero/3B, on pin 40.
 * SPI0-MOSI is available on GPIOs 10 and 38.
 * Only GPIO 10 is available on all models.

 * The library checks if the specified gpio is available
 * on the specific model (from model B rev 1 till 3B)
*/

    ledstring.channel[0].gpionum = gpio;
    ledstring.channel[0].invert= invert;
    ledstring.channel[0].count = width * height;
    ledstring.dmanum = dma;
	ledstring.channel[0].strip_type = strip;
    
    return ws2811_init(&ledstring);
}

void finish_led_string()
{
    ws2811_fini(&ledstring);	
    free(matrix);
}

void SetMatrixPixel(int pixnum, int iColorIndex)
{
    matrix[pixnum] = dotcolors[iColorIndex];
}
