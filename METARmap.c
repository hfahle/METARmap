/**********************************************************************
* Filename    : METARmap.c
* Description : Light a string of LEDs to make a METAR map 
* Author      : Heather Fahle
* modification: 2021/01/15
**********************************************************************/
#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include <time.h>

#include "METARmap.h"

// this website returns the xml of the metar
// Curl Callback used by GetData 
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

struct MemoryStruct getData(char *url)
{
  CURL *curl_handle;
  CURLcode res;
 
  struct curl_slist * pheaders = NULL;
  struct MemoryStruct chunk;
 
  chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */ 
  chunk.size = 0;    /* no data at this point */ 
 
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* init the curl session */ 
  curl_handle = curl_easy_init();

  /* specify URL to get */ 
  curl_easy_setopt(curl_handle, CURLOPT_URL, url);
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
  /* we pass our 'chunk' struct to the callback function */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
 
  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent-rpi/1.0");
 
  /* get it! */ 
  res = curl_easy_perform(curl_handle);
 
  /* check for errors */ 
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  }
  else {
    /*
     * Now, our chunk.memory points to a memory block that is chunk.size
     * bytes big and contains the remote file.
     *
     * We will do something nice with it!
     */ 
 
  }
 
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
 
//  Remember that caller will be responsible for freeing up this memory
 
  /* we're done with libcurl, so clean it up */ 
  curl_global_cleanup();

  return chunk;
 
}

char GetVisibility(char *sRawData) 
{
    char cCond = 'L';  // set to Low IFR - If we miss it, we'll show low

    // Visibility always follows wind data which can either be CALM or nnnnnKT
    char *sSkyCond = strstr(sRawData, "CALM"); 
    if (sSkyCond == NULL) {
	sSkyCond = strstr(sRawData, "KT");
	if (sSkyCond != NULL)
	    sSkyCond += 3;
    } else
	sSkyCond += 5; 
    if(sSkyCond != NULL) {
	//printf("Visibility %10s\n", sSkyCond);	// for testing
	char *cLoc = strchr(sSkyCond, '/');   // low ifr will always be a partial mile eg 1/2
	if (cLoc == NULL || cLoc - sSkyCond > 2) { // the slash needs to be next "number" else could be 1 1/2 
	    if (isdigit(sSkyCond[1])) { // ie a 2-digit number, which is VFR
		cCond = 'V';
	    } else {
		int iDist = sSkyCond[0] - '0'; // convert to an integer
		if (iDist > 5)
			cCond = 'V';
		else if (iDist >= 3)
			cCond = 'M';
		else cCond = 'I';
	    }
	}   
} else 
	printf("Error - visibility is null\n");

return cCond;
}

char GetSkyCondition(char *sRawData) 
{
	char cCond = 'V'; 	// default is  VFR. Only presence of BKN or OVC will change cCond 
	char sCeilingHt[4];
	
	// We only care about ceiling (lowest of) BKN (broken) or OVC (overcast)
	char *sSkyCond = strstr(sRawData, "BKN"); 
	if (sSkyCond == NULL) 
		sSkyCond = strstr(sRawData, "OVC");
	
	if (sSkyCond != NULL) {
		sSkyCond += 3;
		strncpy(sCeilingHt, sSkyCond, 3);
		int iHeight = atoi(sCeilingHt);
		if (iHeight < 5)
			cCond = 'L';
		else if (iHeight < 10)
			cCond = 'I';
		else if (iHeight <= 30)
			cCond = 'M';
		
	}
	return cCond;
}

char GetFlightCategory(char *sFlightCat) {
	
	if(sFlightCat == NULL)
		return 'E';
		
	char sCat[5];
	
	char *sSkyCond = sFlightCat + 17;
	char cCond = sSkyCond[0];
	printf("%c", cCond);  // the first letter is all we need
	fflush(stdout);
	return cCond;
}

char IsMetarCurrent(char *sRawData)
{	
    #define METAR_BUFFER_LEN 20
    
    char metar_buffer[METAR_BUFFER_LEN +1 ];
    char nowtime_buffer[METAR_BUFFER_LEN +1 ];

    time_t tNow;
    time_t tMetar;
        
    struct tm stMetarTime;
    struct tm *stNowInfo;
    if (sRawData == NULL)
	return FALSE;
    time(&tNow);
    stNowInfo = gmtime( &tNow);
    
    sRawData += strlen("<observation_time>"); 	// get past the <observation_time> tag
    strncpy(metar_buffer, sRawData, METAR_BUFFER_LEN);
    metar_buffer[METAR_BUFFER_LEN] = 0;
        
    memset(&stMetarTime, 0, sizeof(stMetarTime));
    strptime(metar_buffer, "%Y-%m-%dT%H:%M:%S", &stMetarTime);
    tMetar = mktime(&stMetarTime);
    tNow = mktime(stNowInfo);
    
    double dDiff = difftime(tNow, tMetar);
    
    if(dDiff > (90.0* 60.0)) {// more than 90 minutes old -- we're not using this for decicion making but want up to date data
	printf("METAR data out of date %.0f seconds\n", dDiff);
	return FALSE;
    }
	
    else
	return TRUE;
}

char ParseTheData(char *sAirportCode, struct MemoryStruct sAirportData)
{
    char cCond = 'L';
    char cVis = 'L';	
    char cSky = 'L';
    char sSearchStr[16];
    char *sThisAirportData;
	
    sprintf(sSearchStr, "<raw_text>%4s", sAirportCode);
    
    sThisAirportData = strstr(sAirportData.memory, sSearchStr);
    if (sThisAirportData == NULL) {
	printf("%s data not reporting \n", sAirportCode);
	return 'E'; //Error airport not reporting
    }
    
    char *sAirportEnd = strstr(sThisAirportData, "</METAR>");
    
    char *sRawData = strstr(sThisAirportData, "<raw_text>");

    char *sFlightCat = strstr(sThisAirportData, "<flight_category>");
    if (sFlightCat > sAirportEnd) 
	sFlightCat = NULL;

    char *sDateTime =  strstr(sThisAirportData, "<observation_time>");
    if(sDateTime > sAirportEnd)
	sAirportEnd = NULL;
    
    if (IsMetarCurrent(sDateTime) == FALSE) { // Wx data expired
	    cCond = 'E';
    } else {
	    if (sFlightCat != NULL)      //  the tag at <flight_category> exists
		cCond = GetFlightCategory(sFlightCat); 
	    else {
		    cCond = 'E';
		    cVis = GetVisibility(sRawData);
		    cSky = GetSkyCondition(sRawData);
		    if (cSky == 'L' || cVis == 'L')
			    cCond = 'L';
		    else if (cSky == 'I' || cVis == 'I')
			    cCond = 'I';
		    else if (cSky == 'M' || cVis == 'M')
			    cCond = 'M';
		    else if (cSky == 'V' || cVis == 'V')
			    cCond = 'V';
	    }
		    
    }		
    
    return(cCond);
}
