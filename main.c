/*
 * main for METARmap program
 * Program designed to light a series of leds with METAR data using airports in AirportList.dat 
 * Heather and Bill Fahle 2021 No copyright. Feel free to use this program for your own
 * purposes. We provide no guarantees, support, or warranty that this will work the way that 
 * you want it to work. 
 */
 
static char VERSION[] = "XX.YY.ZZ";
//#define __USE_XOPEN    // honestly not sure why needed, but needed for some of the time funcs

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>


#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"
#include "METARmap.h"

#include "ws2811.h"

struct stAirport {
    char sAirportCode[5];
    int iLedNo;
};

#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB	// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE              WS2811_STRIP_GBR	// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW	// SK6812RGBW (NOT SK6812RGB)

#define WIDTH                   50
#define HEIGHT                  1
#define LED_COUNT               (WIDTH * HEIGHT)

// condition defines for color choices index into the dotcolors array
#define VFR 3
#define IFR 0
#define LIFR 6
#define MVFR 5
#define NO_AIRPORT_DATA 2

int width = WIDTH;
int height = HEIGHT;
int led_count = LED_COUNT;

int clear_on_exit = 0;

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

static uint8_t running = 1;

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

static void ctrl_c_handler(int signum)
{
    (void)(signum);
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}


void parseargs(int argc, char **argv, ws2811_t *ws2811)
{
	int index;
	int c;

	static struct option longopts[] =
	{
		{"help", no_argument, 0, 'h'},
		{"dma", required_argument, 0, 'd'},
		{"gpio", required_argument, 0, 'g'},
		{"invert", no_argument, 0, 'i'},
		{"clear", no_argument, 0, 'c'},
		{"strip", required_argument, 0, 's'},
		{"height", required_argument, 0, 'y'},
		{"width", required_argument, 0, 'x'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "cd:g:his:vx:y:", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;

		case 'h':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			fprintf(stderr, "Usage: %s \n"
				"-h (--help)    - this information\n"
				"-s (--strip)   - strip type - rgb, grb, gbr, rgbw\n"
				"-x (--width)   - matrix width (default 8)\n"
				"-y (--height)  - matrix height (default 8)\n"
				"-d (--dma)     - dma channel to use (default 10)\n"
				"-g (--gpio)    - GPIO to use\n"
				"                 If omitted, default is 18 (PWM0)\n"
				"-i (--invert)  - invert pin output (pulse LOW)\n"
				"-c (--clear)   - clear matrix on exit.\n"
				"-v (--version) - version information\n"
				, argv[0]);
			exit(-1);

		case 'D':
			break;

		case 'g':
			if (optarg) {
				int gpio = atoi(optarg);
/*
	PWM0, which can be set to use GPIOs 12, 18, 40, and 52.
	Only 12 (pin 32) and 18 (pin 12) are available on the B+/2B/3B
	PWM1 which can be set to use GPIOs 13, 19, 41, 45 and 53.
	Only 13 is available on the B+/2B/PiZero/3B, on pin 33
	PCM_DOUT, which can be set to use GPIOs 21 and 31.
	Only 21 is available on the B+/2B/PiZero/3B, on pin 40.
	SPI0-MOSI is available on GPIOs 10 and 38.
	Only GPIO 10 is available on all models.

	The library checks if the specified gpio is available
	on the specific model (from model B rev 1 till 3B)

*/
		    ws2811->channel[0].gpionum = gpio;
		    }
		    break;

		case 'i':
			ws2811->channel[0].invert=1;
			break;

		case 'c':
			clear_on_exit=0;
			break;

		case 'd':
			if (optarg) {
				int dma = atoi(optarg);
				if (dma < 14) {
					ws2811->dmanum = dma;
				} else {
					printf ("invalid dma %d\n", dma);
					exit (-1);
				}
			}
			break;

		case 'y':
			if (optarg) {
				height = atoi(optarg);
				if (height > 0) {
					ws2811->channel[0].count = height * width;
				} else {
					printf ("invalid height %d\n", height);
					exit (-1);
				}
			}
			break;

		case 'x':
			if (optarg) {
				width = atoi(optarg);
				if (width > 0) {
					ws2811->channel[0].count = height * width;
				} else {
					printf ("invalid width %d\n", width);
					exit (-1);
				}
			}
			break;

		case 's':
			if (optarg) {
				if (!strncasecmp("rgb", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_RGB;
				}
				else if (!strncasecmp("rbg", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_RBG;
				}
				else if (!strncasecmp("grb", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_GRB;
				}
				else if (!strncasecmp("gbr", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_GBR;
				}
				else if (!strncasecmp("brg", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_BRG;
				}
				else if (!strncasecmp("bgr", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_BGR;
				}
				else if (!strncasecmp("rgbw", optarg, 4)) {
					ws2811->channel[0].strip_type = SK6812_STRIP_RGBW;
				}
				else if (!strncasecmp("grbw", optarg, 4)) {
					ws2811->channel[0].strip_type = SK6812_STRIP_GRBW;
				}
				else {
					printf ("invalid strip %s\n", optarg);
					exit (-1);
				}
			}
			break;

		case 'v':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			exit(-1);

		case '?':
			/* getopt_long already reported error? */
			exit(-1);

		default:
			exit(-1);
		}
	}
}

int main(int argc, char *argv[])
{
    int r,g,b;
    struct stAirport stAirports[LED_COUNT];
    int iColorIndex = NO_AIRPORT_DATA;
    int iContinue = 1;
    int iEOF;
    int numAirportsInFile = 0;
    char cWxReqString[512];
	
    ws2811_return_t ret;

    sprintf(VERSION, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
    printf("%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

    clear_on_exit = 1;
    ledstring.channel[0].strip_type = WS2811_STRIP_RGB;

    parseargs(argc, argv, &ledstring);

    ledstring.channel[0].count = width * height;

    matrix = malloc(sizeof(ws2811_led_t) * width * height);

    setup_handlers();

    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
	    fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
	    return ret;
    }

    printf("Program is starting ...\n");
    
    FILE *fAirports = fopen("AirportList.dat", "r");
    if(fAirports == NULL) {
	    printf("oops. Where da file?\n");
	    return 0;
    }
/*  
    FILE *fDay = fopen("day.dat", "rw");
    if(fDay != NULL) {
	for int(i = 0; i< 288-1; i++) {
	    memcpystDayRec[i+1]
	    
	}
    }
    FILE *Month = fopen("month.dat", "rw");
*/  
    
    strcpy(cWxReqString, AIRPTSTR);

    // read the file and build the string
    for (int i = 0; i < LED_COUNT; i++) {
	iEOF = fscanf(fAirports, "%4s %d",stAirports[i].sAirportCode, &stAirports[i].iLedNo);
	strcat(cWxReqString, stAirports[i].sAirportCode);
	strcat(cWxReqString, "%20");

	if(iEOF < 0) { // end of file 
		break;
	}
	numAirportsInFile++;
    }

    fclose(fAirports);

    printf("Passing this req %s\n", cWxReqString);
    struct MemoryStruct wxChunk = getData(cWxReqString);  // read all the wx data
//    printf("Return is %s\n", wxChunk.memory);

//    memset(stAirportCodes,0,LED_COUNT);
	    
    // Loop thru the airports, read the wx and light the LEDs
    for (int i = 0; i < numAirportsInFile; i++) {
	printf("%4s %d\n",stAirports[i].sAirportCode, stAirports[i].iLedNo); //for debugging showing where we're doing 
	if (stAirports[i].iLedNo >= width) {
	    printf("we don't want this one led #%d\n", stAirports[i].iLedNo);
	    continue;   // don't handle airports past our number of leds
	}

	char cCond = ParseTheData(stAirports[i].sAirportCode, wxChunk);
	switch(cCond) {
	    case 'I': 
		iColorIndex = IFR;
		break;
	    
	    case 'V': 
		iColorIndex = VFR;
		break;
	    
	    case 'M': 
		iColorIndex = MVFR;
		break;
	    
	    case 'L': 
		iColorIndex = LIFR;
		break;
	    
	    default:
		printf("not reporting\n");
		iColorIndex = NO_AIRPORT_DATA;
		break;
	    }
	    
	    matrix[stAirports[i].iLedNo] = dotcolors[iColorIndex];
    } 

    matrix_render();
    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
	fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
    }

    // 15 frames /sec
    usleep(1000000 / 15);
	
    if (clear_on_exit) {
	matrix_clear();
	matrix_render();
	ws2811_render(&ledstring);
    }

    ws2811_fini(&ledstring);	
    free(matrix);
    free (wxChunk.memory);	

    printf ("\n"); // to make all of the output print
    return ret;
}
