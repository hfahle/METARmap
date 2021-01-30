/*
 * main for METARmap program
 * Program designed to light a series of leds with METAR data using airports in AirportList.dat,
 * along with the ability to replay up to 10 days of historical data 
 * Heather and Bill Fahle 2021 No copyright. Feel free to use this program for your own
 * purposes. We provide no guarantees, support, or warranty that this will work the way that 
 * you want it to work. 
 * Also, remember: This program is for fun. just fun. Don't use 
 * this for decision making with regards to actual flight. This has not 
 * been certified by the FAA and may have bugs. Please use an official 
 * source to obtain a briefing to determine flight conditions to assist
 * in making a go/nogo decision. Don't include this in any program that
 * you represent as anything resembling an official flight briefing
 * unless you get it certified and do a LOT more testing than I have.
 * THIS PROGRAM IS FOR FUN, PEOPLE. 
 * modification 2021/01/29 hlf
 */
 
static char VERSION[] = "XX.YY.ZZ";

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
#include <errno.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <pwd.h>

#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"
#include "METARmap.h"
#include "matrix.h"

#include "ws2811.h"

volatile uint8_t running = 1;

const char *semName = "METAR_MapInUse";

//#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

int width = WIDTH;
int height = HEIGHT;
int led_count = LED_COUNT;

int strip = WS2811_STRIP_RGB; //strip type - rgb (default), grb, gbr, rgbw
int dma = 10; // dma channel to use (default 10)
int gpio = 18; //	GPIO to use If omitted, default is 18
int invert =  0; // 1 to invert

int num_replay_hours = 4;
int clear_on_exit = 0;
int replay_mode = 0;
int night_mode = 0;
int test_mode = 0;
int free_the_semaphore = 0;

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


void parseargs(int argc, char **argv)
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
	    {"freesem", no_argument, 0, 'f'},
	    {"test", no_argument, 0, 't'},
	    {"night", no_argument, 0, 'n'},
	    {"replay_days", required_argument, 0, 'r'},
	    {"Replay_hrs", required_argument, 0, 'R'},
	    {"strip", required_argument, 0, 's'},
	    {"height", required_argument, 0, 'y'},
	    {"width", required_argument, 0, 'x'},
	    {"version", no_argument, 0, 'v'},
	    {0, 0, 0, 0}
    };

    while (1) {
	index = 0;
	c = getopt_long(argc, argv, "cd:fg:hinR:r:s:tvx:y:", longopts, &index);

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
			"-r (--replay)  - replay days range 1-10\n"
			"-R (--replay)  - replay hours range 1-240\n"
			"-t (--test)  	- operate in test mode\n"
			"-n (--night)   - night mode - record but don't light\n"
			"-v (--version) - version information\n"
			, argv[0]);
		exit(-1);

	case 'D':
		break;

	case 'g':
		if (optarg) {
			gpio = atoi(optarg);
	    }
	    break;

	case 'i':
		invert=1;
		break;

	case 'c':
		clear_on_exit=1;
		break;

	case 'f':
		free_the_semaphore=1;
		break;

	case 'r':
	case 'R':
	    {
		int mult = 1;
		replay_mode=TRUE;
		if (c == 'r')
		    mult = 24;
		if (optarg) {
			num_replay_hours = atoi(optarg)*mult;
			if (num_replay_hours > MAX_REPLAY_DAYS*24) {
				num_replay_hours = MAX_REPLAY_DAYS*24;
			} 
		}
	    }
		break;

	case 'n':
		night_mode=TRUE;
		break;

	case 't':
		test_mode=TRUE;
		break;

	case 'd':
		if (optarg) {
			dma = atoi(optarg);
			if (dma >= 14) {
				printf ("invalid dma %d\n", dma);
				exit (-1);
			}
		}
		break;

	case 'y':
		if (optarg) {
			height = atoi(optarg);
			if (height <= 0) {
				printf ("invalid height %d\n", height);
				exit (-1);
			}
		}
		break;

	case 'x':
		if (optarg) {
			width = atoi(optarg);
			if (width <= 0) {
				printf ("invalid width %d\n", width);
				exit (-1);
			}
		}
		break;

	case 's':
		if (optarg) {
			if (!strncasecmp("rgb", optarg, 4)) {
				strip = WS2811_STRIP_RGB;
			}
			else if (!strncasecmp("rbg", optarg, 4)) {
				strip = WS2811_STRIP_RBG;
			}
			else if (!strncasecmp("grb", optarg, 4)) {
				strip = WS2811_STRIP_GRB;
			}
			else if (!strncasecmp("gbr", optarg, 4)) {
				strip = WS2811_STRIP_GBR;
			}
			else if (!strncasecmp("brg", optarg, 4)) {
				strip = WS2811_STRIP_BRG;
			}
			else if (!strncasecmp("bgr", optarg, 4)) {
				strip = WS2811_STRIP_BGR;
			}
			else if (!strncasecmp("rgbw", optarg, 4)) {
				strip = SK6812_STRIP_RGBW;
			}
			else if (!strncasecmp("grbw", optarg, 4)) {
				strip = SK6812_STRIP_GRBW;
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
    int iContinue = 1;
    ws2811_return_t ws2811_ret;

    printf("Program is starting ...\n");
    sprintf(VERSION, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
    printf("%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
    
    setup_handlers();

    parseargs(argc, argv);
    
    // set a semaphore and check it so that only one instance of the program can run at a time. Both files and leds req exclusive use here
    sem_t *sem_id = sem_open(semName, O_CREAT, SEM_PERMISSIONS, 1);
    if (sem_id == SEM_FAILED) {
	fprintf(stderr, "Can't open the semaphore! This is a hot mess\n");
	return(1);
    }
    
    int sem_ret;
    sem_getvalue(sem_id, &sem_ret);
    
    if(free_the_semaphore && sem_ret == 0)  // the option to clear if needed. Reboot also works
	sem_post(sem_id);
	
    printf("sem open ret'd %d\n ", sem_ret);
    sem_wait(sem_id);	// wait for anybody else who has this process and its resources
    printf("we have the sem\n");  // free at last free at last

    ws2811_ret = init_led_string();
    if (ws2811_ret != WS2811_SUCCESS) {
	fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ws2811_ret));
	sem_post(sem_id);
	return ws2811_ret;
    }

    if(replay_mode == TRUE) {
	printf("replay\n");
	Replay();
    } else {
	if (LiveMetarMap() == 0) {
    	    sem_post(sem_id);
	    return 0;
	}
    }
	 
    if (!night_mode) { // don't blinky blinky all night
	matrix_render();
    }

    // 15 frames /sec
    usleep(1000000 / 15);
	
    if (clear_on_exit) {
	matrix_clear();
	matrix_render();
    }

    finish_led_string();

    printf ("freeing semaphore\n"); // to make all of the output print
    sem_post(sem_id);  // set him free

    return 0;
}
