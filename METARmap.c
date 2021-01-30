/**********************************************************************
* Filename    : METARmap.c
* Description : Light a string of LEDs to make a METAR map
* Author      : Heather Fahle
* 
* Seriously, I shouldn't have to say this, but I am going to. Don't use 
* this for decision making with regards to actual flight. This has not 
* been certified by the FAA and may have bugs. Please use an official 
* source to obtain a briefing to determine flight conditions to assist
* in making a go/nogo decision. Don't include this in any program that
* you represent as anything resembling an official flight briefing
* unless you get it certified and do a LOT more testing than I have.
* THIS PROGRAM IS FOR FUN, PEOPLE. 
* modification: 2021/01/15 hlf, 2021/01/29 hlf
**********************************************************************/
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#include "METARmap.h"
#include "matrix.h"

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
    char cCond = 'E';  // set to Error

    // Visibility should always end with "SM"
    // If no visibility, I will assume unlimited for now but may change that. Thanks, non-standard RPH.
    char *sStartPos = strstr(sRawData, "CALM");
    if (sStartPos == NULL) {
	sStartPos = strstr(sRawData, "KT");
	if (sStartPos != NULL) {
	    sStartPos += 3;
	} else {
	    cCond = 'E';
	}
    } else {
	sStartPos += 5;
    }
    if(sStartPos != NULL) {
	char *sEndPos = strstr(sStartPos, "SM");
	if (sEndPos == NULL) {
	    cCond = 'E';
	} else {
	    char *cSlashLoc = strchr(sStartPos, '/');   // low ifr will always be a partial mile eg 1/2
	    if (cSlashLoc == NULL || cSlashLoc - sStartPos > 2) { // the slash needs to be next "number" else could be 1 1/2
		if (isdigit(sStartPos[1])) { // ie a 2-digit number, which is VFR
		    cCond = 'V';
		} else {
		    int iDist = sStartPos[0] - '0'; // convert to an integer
		    if (iDist > 5)
			cCond = 'V';
		    else if (iDist >= 3)
			cCond = 'M';
		    else cCond = 'I';
		}
	    }
	}
    } else {
	printf("Error - no wind so unsure where to find visibility\n");
    }

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

int NumRecsInHistory(FILE *fDay)
{
    // function always returns you to the beginning of the file
    // seek to the end of the file, see how far that was, then rewind back to the beginning
    fseek(fDay, 0L, SEEK_END);
    long int file_size = ftell(fDay);
    long int reclen = REC_LEN;
    long int numrecs = file_size/reclen;
    rewind(fDay);

    return (int)numrecs;
}

void Replay(void)
{
    #define REC_LEN (LED_COUNT*3)+1	// 3 for each led plus '\n'
    char historyFileName[16];

    if (test_mode == TRUE)
	strcpy(historyFileName, "daytest.dat");
    else
	strcpy(historyFileName, "day.dat");

    FILE *fDay = fopen(historyFileName, "r");
    if (fDay == NULL) {
	fprintf(stderr, "Can't do replay right now. File not available %d\n", errno);
	return;
    }

    int num_recs_to_play = NumRecsInHistory(fDay);
    if (num_replay_hours * HISTORY_RECS_PER_HOUR < num_recs_to_play)
	num_recs_to_play = num_replay_hours * HISTORY_RECS_PER_HOUR;

    /* ****************************************************************************************
     * No matter how many recs we replay, we want to do it over the course of a minute or less.
     * In the future, we may allow the user to change this, but for now, it's a minute, which
     * means that we want to make our sleep interval a function of the number of recs that we
     * are displaying.
     * 5 frames per seconds is one day (now 288 recs). usleep with 1000000 is one second
     * *****************************************************************************************/
    long int interval;

    if (num_recs_to_play < 60) {
	interval = SLOWEST_WE_GO;
    } else {
	if (test_mode == TRUE)
	    interval = 500000l / (long int)(num_recs_to_play / 60);	// go fast for test
	else
	    interval = 1000000l / (long int)(num_recs_to_play / 60);
    }

    if (interval > SLOWEST_WE_GO)
	interval = SLOWEST_WE_GO;

    char sPeriodicData[num_recs_to_play][REC_LEN+1]; // the length of a record plus null term
    memset(sPeriodicData, 0, num_recs_to_play*(REC_LEN+1));
    char sSumRec[4];

    int iColorIndex;

    // loop thru the first num_recs_to_play in file and read into memory in reverse order
    for (int i = 1; i < num_recs_to_play+1; i++) {		//1-based makes more sense when starting at the end
	if (running == 0)
	    break;
	if (fread(sPeriodicData[num_recs_to_play-i], REC_LEN, 1, fDay) < 1)
	    break; 	// end of file
	sPeriodicData[num_recs_to_play-i][REC_LEN] = 0;
    }
    fclose(fDay);

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
	    SetMatrixPixel(j, iColorIndex);
	}

	matrix_render();
	usleep(interval);
    }

    // blink the lights so we'll know that replay is done
    clear_ledstring();

    usleep(1000000 / 5);  //  1/5 of a second for the blink
    matrix_render();

    printf("X\n");

}

int LiveMetarMap(void)
{
    char cWxReqString[1024];
    int iEOF;
    struct stAirport stAirports[LED_COUNT];
    int numAirportsInFile = 0;
    char sSumRec[4];
    int iColorIndex = NO_AIRPORT_DATA;
    char sPeriodicData[REC_LEN+1]; // the length of a record plus null term

    memset(sPeriodicData, 0, REC_LEN+1);

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

	SetMatrixPixel(stAirports[i].iLedNo,iColorIndex);
	sprintf(sSumRec, "%02d%c", stAirports[i].iLedNo, cCond);
	memcpy(&sPeriodicData[stAirports[i].iLedNo*3], sSumRec, strlen(sSumRec));
	sPeriodicData[REC_LEN-1] = '\n';
	sPeriodicData[REC_LEN] = 0;
    }

    FILE *fNewDay = fopen("newday.dat", "w");
    fprintf(fNewDay, "%s", sPeriodicData);

    char historyFileName[16];
    if (test_mode == TRUE)
	strcpy(historyFileName, "daytest.dat");
    else
	strcpy(historyFileName, "day.dat");

    // read the history file (day.dat) and append all of the recs to our new file until max recs is reached
    FILE *fDay = fopen(historyFileName, "r");
    if(fDay != NULL) {
	int numrecs = NumRecsInHistory(fDay);
	if ( numrecs >= MAX_HISTORY_RECS) {
	    numrecs = MAX_HISTORY_RECS -1;
	    printf("history file full - roll one off so save %d\n", numrecs);
	}
	for (int i = 0; i < numrecs; i++) {
	    if (fread(&sPeriodicData, REC_LEN, 1, fDay) < 1)
		break; 	// end of file
	    sPeriodicData[REC_LEN] = 0;
	    fprintf(fNewDay, "%s", sPeriodicData);
	}
	fclose(fDay);
    }
    fclose(fNewDay);

    // copy newday to day
    if (rename("newday.dat", historyFileName) != 0) { // error returned
	fprintf(stderr, "can't copy over the newday file\n");
	printf("can't copy over the newday file\n");
    }

    free (wxChunk.memory);
}


