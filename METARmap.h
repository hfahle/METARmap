#define AIRPTSTR   "https://www.aviationweather.gov/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=5&mostRecentForEachStation=true&stationString="

#define TRUE 1
#define FALSE 0

struct MemoryStruct getData(char *url);
int ReadWeatherData(char *cWxString);
char ParseTheData(char *sAirportCode, struct MemoryStruct sAirportData);

// This structure is used by GetData 
struct MemoryStruct {
  char *memory;
  size_t size;
};



