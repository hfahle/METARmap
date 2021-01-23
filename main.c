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
#include <errno.h>
#include <semaphore.h>
#include <sys/stat.h>

#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"
#include "METARmap.h"

#include "ws2811.h"

const char *semName = "METAR_MapInUse";

struct stAirport {
    char sAirportCode[5];
    int iLedNo;
};

#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

int width = WIDTH;
int height = HEIGHT;
int led_count = LED_COUNT;
int num_replay_days = 1;

int clear_on_exit = 0;
int replay_mode = 0;
int night_mode = 0;
int free_the_semaphore = 0;

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
	    {"freesem", no_argument, 0, 'f'},
	    {"night", no_argument, 0, 'n'},
	    {"replay", required_argument, 0, 'r'},
	    {"strip", required_argument, 0, 's'},
	    {"height", required_argument, 0, 'y'},
	    {"width", required_argument, 0, 'x'},
	    {"version", no_argument, 0, 'v'},
	    {0, 0, 0, 0}
    };

    while (1) {
	index = 0;
	c = getopt_long(argc, argv, "cd:fg:hinr:s:vx:y:", longopts, &index);

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
			"-r (--replay)  - replay the last 24 hrs lights\n"
			"-n (--night)   - night mode - record but don't light\n"
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
		clear_on_exit=1;
		break;

	case 'f':
		free_the_semaphore=1;
		break;

	case 'r':
		replay_mode=TRUE;
		if (optarg) {
			num_replay_days = atoi(optarg);
			if (num_replay_days > MAX_REPLAY_DAYS) {
				num_replay_days = MAX_REPLAY_DAYS;
			} 
		}
		break;

	case 'n':
		night_mode=TRUE;
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

void clear_ledstring(void) {
    int x, y;

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            ledstring.channel[0].leds[(y * width) + x] = 0;
        }
    }

}

int NumRecsInHistory(FILE *fDay)
{
    // seek to the end of the file, see how far it was, then rewind back to the beginning
    fseek(fDay, 0L, SEEK_END);
    long int file_size = ftell(fDay);
    long int reclen = REC_LEN;
    long int numrecs = file_size/reclen;
    printf("file len is %ld which is %ld recs of sz %ld\n", file_size, numrecs, reclen);
    rewind(fDay);
    
    return (int)numrecs;
}

void Replay(ws2811_led_t *matrix)
{
    #define REC_LEN (LED_COUNT*3)+1	// 3 for each led plus '\n' 

    FILE *fDay = fopen("day.dat", "r");
    if (fDay == NULL) {
	fprintf(stderr, "Can't do replay right now. File busy\n");
	return;
    }
    
    int num_recs_to_play = NumRecsInHistory(fDay);
    if (num_replay_days * HISTORY_RECS_PER_DAY < num_recs_to_play)
	num_recs_to_play = num_replay_days * HISTORY_RECS_PER_DAY;

    /* ****************************************************************************************
     * No matter how many recs we replay, we want to do it over the course of a minute or less.
     * In the future, we may allow the user to change this, but for now, it's a minute, which 
     * means that we want to make our sleep interval a function of the number of recs that we 
     * are displaying.
     * 5 frames per seconds is one day (now 288 recs). usleep with 1000000 is one second
     * *****************************************************************************************/
    long int interval = 1000000 / (long int)(num_recs_to_play / 60);

    char sPeriodicData[num_recs_to_play][REC_LEN+1]; // the length of a record plus null term
    memset(sPeriodicData, 0, num_recs_to_play*(REC_LEN+1)); 
    char sSumRec[4];
    
    int iColorIndex;
    
    // loop thru the recs and read into memory,  release the file, then loop again to display

    for (int i = 0; i < num_recs_to_play; i++) {		//loop through all history metar map recs
	if (running == 0)
	    break;
	if (fread(sPeriodicData[i], REC_LEN, 1, fDay) < 1)
	    break; 	// end of file
	sPeriodicData[i][REC_LEN] = 0;
    }
    fclose(fDay);
    
    printf("read them all\n");

    for (int i = 0; i < num_recs_to_play; i++) {		//loop through all metar map recs
	if (running == 0)
	    break;
	printf(".");
	fflush(stdout);
	for (int j = 0; j < LED_COUNT; j++) {
    	    char cCond = sPeriodicData[i][(j*3)+2];
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
		    iColorIndex = NO_AIRPORT_DATA;
		    break;
		}
	    matrix[j] = dotcolors[iColorIndex];
	}
	
	matrix_render();
	// 5 frames /sec
	//usleep(1000000 / 5);
	usleep(interval);

	
    }

    // blink the lights so we'll know that replay is done
    clear_ledstring();
    int ret = 0;
    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
	   fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
    }

    usleep(1000000 / 5);  //  5 frames per second
    matrix_render();

    printf("finished replay\n");
    
}

int LiveMetarMap(ws2811_led_t *matrix)
{
    int iHistory = TRUE;
    char cWxReqString[1024];
    int iEOF;
    struct stAirport stAirports[LED_COUNT];
    int numAirportsInFile = 0;
    char sSumRec[4];
    int iColorIndex = NO_AIRPORT_DATA;

    // read the history file (day.dat) and roll all the recs up one until max recs is reached 
    char sPeriodicData[REC_LEN+1]; // the length of a record plus null term
    memset(sPeriodicData, 0, REC_LEN+1); 
    
    FILE *fDay    = fopen("day.dat", "r+");
    FILE *fNewDay = fopen("newday.dat", "w");

    // if the file exists, copy its contents to a tempfile, rolling off the oldest if the file is full
    if(fDay != NULL) {
	int numrecs = NumRecsInHistory(fDay);
	printf("%d recs \n", numrecs);
	if ( numrecs == MAX_HISTORY_RECS) { // skip the first one
	    printf("history file full at %d. Let's roll one off\n", numrecs);
	    if (fread(sPeriodicData, REC_LEN, 1, fDay) < 1) { // skip past the 1st rec
		iHistory = FALSE;
		printf(" history file seems to be trashed \n");
	    }
	} 
        sPeriodicData[REC_LEN] = 0;
	for (int i = 0; i < MAX_HISTORY_RECS; i++) {
	    fflush(stdout);
	    if (fread(sPeriodicData, REC_LEN, 1, fDay) < 1)
		break; 	// end of file
	    sPeriodicData[REC_LEN] = 0;
	    fprintf(fNewDay, "%s", sPeriodicData);
	}
	fclose(fDay);
    }
    fclose(fNewDay);

    FILE *fAirports = fopen("AirportList.dat", "r");
    if(fAirports == NULL) {
	    printf("oops. Where da file?\n");
	    return 0;
    }

    strcpy(cWxReqString, AIRPTSTR);

    // read the file and build the string for the call to aviation wx
    for (int i = 0; i < LED_COUNT; i++) {
	iEOF = fscanf(fAirports, "%4s %d",stAirports[i].sAirportCode, &stAirports[i].iLedNo);
	if(iEOF < 0) { // end of file 
		break;
	}
	strcat(cWxReqString, stAirports[i].sAirportCode);
	strcat(cWxReqString, "%20");

	numAirportsInFile++;
    }

    fclose(fAirports);

    printf("Passing this req %s\n", cWxReqString);
    struct MemoryStruct wxChunk = getData(cWxReqString);  // read all the wx data

    // initialize the sPeriodicData rec with all 'E's
    memset(sPeriodicData, 0, REC_LEN);
    for (int i = 0; i < LED_COUNT; i++) {
	sprintf(sSumRec, "%02d%c", i, 'E');
	strcat(sPeriodicData, sSumRec);
    }

    // Loop thru the airports, read the wx, light the LEDs and build daily periodic rec
    for (int i = 0; i < numAirportsInFile; i++) {
	if (stAirports[i].iLedNo >= width) {
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
		iColorIndex = NO_AIRPORT_DATA;
		break;
	    }
	matrix[stAirports[i].iLedNo] = dotcolors[iColorIndex];
	sprintf(sSumRec, "%02d%c", stAirports[i].iLedNo, cCond);
	memcpy(&sPeriodicData[stAirports[i].iLedNo*3], sSumRec, strlen(sSumRec));
    } 

    fNewDay = fopen("newday.dat", "a");
    if(fNewDay == NULL) {
	fprintf(stderr, "cant open newday file\n");
	iHistory = FALSE;
    } else {
	printf("going to add our rec to newday");
	iHistory = TRUE; // we'll have at least one record
	fprintf(fNewDay, "%s\n", sPeriodicData);
	fclose(fNewDay);
    }

    // copy newday to day
    if (iHistory) {
	if (rename("newday.dat", "day.dat") != 0) {
	    printf("copying newday to dat \n");
	    fprintf(stderr, "can't copy over the newday file\n");
	    printf("can't copy over the newday file\n");
	}
    }
    
    free (wxChunk.memory);	
}

int main(int argc, char *argv[])
{
    int iContinue = 1;
    ws2811_return_t ret;

    printf("Program is starting ...\n");
    sprintf(VERSION, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
    printf("%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
    
    ledstring.channel[0].strip_type = WS2811_STRIP_RGB;

    ledstring.channel[0].count = width * height;

    matrix = malloc(sizeof(ws2811_led_t) * width * height);

    setup_handlers();

    parseargs(argc, argv, &ledstring);

    sem_t *sem_id = sem_open(semName, O_CREAT, SEM_PERMISSIONS, 1);
    if (sem_id == SEM_FAILED) {
	fprintf(stderr, "This isn't good. Reboot this hot mess\n");
	return(1);
    }
    
    int sem_ret;
    sem_getvalue(sem_id, &sem_ret);
    
    if(free_the_semaphore && sem_ret == 0)
	sem_post(sem_id);
	
    printf("sem open ret'd a %d and the ret is %d\n ", sem_id, sem_ret);
    printf("sem wait\n");
    sem_wait(sem_id);	// wait for anybody else who has this process and its resources
    printf("sem relase\n");  // free at last free at last

    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
	fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
	sem_post(sem_id);
	return ret;
    }

    if(replay_mode == TRUE) {
	printf("replay\n");
	Replay(matrix);
    } else {
	if (LiveMetarMap(matrix) == 0) {
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

    ws2811_fini(&ledstring);	
    free(matrix);

    printf ("freeing semaphore\n"); // to make all of the output print
    sem_post(sem_id);  // set him free

    return ret;
}
