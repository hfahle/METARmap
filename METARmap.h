#include "ws2811.h"

#define AIRPTSTR   "https://www.aviationweather.gov/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=5&mostRecentForEachStation=true&stationString="

#define TRUE 1
#define FALSE 0
#define WIDTH                   50
#define HEIGHT                  1
#define LED_COUNT               (WIDTH * HEIGHT)
// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB	// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE              WS2811_STRIP_GBR	// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW	// SK6812RGBW (NOT SK6812RGB)

#define VFR 3
#define IFR 0
#define LIFR 6
#define MVFR 5
#define NO_AIRPORT_DATA 2

 // these 3 work together
#define HISTORY_RECS_PER_DAY 288
#define MAX_HISTORY_RECS 2880
#define MAX_REPLAY_DAYS 10    

#define SEM_PERMISSIONS S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
extern const char *semName;

// condition defines for color choices index into the dotcolors array

#define REC_LEN (LED_COUNT*3)+1	// 3 for each led plus '\n' 

struct MemoryStruct getData(char *url);
int ReadWeatherData(char *cWxString);
char ParseTheData(char *sAirportCode, struct MemoryStruct sAirportData);

// This structure is used by GetData 
struct MemoryStruct {
  char *memory;
  size_t size;
};



