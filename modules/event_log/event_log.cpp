//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "event_log.h"

#include "siren.h"
#include "fire_alarm.h"
#include "user_interface.h"
#include "date_and_time.h"
#include "pc_serial_com.h"
#include "motion_sensor.h"
#include "sd_card.h"
#include "light_level_control.h"

//=====[Declaration of private defines]========================================

//=====[Declaration of private data types]=====================================

typedef struct systemEvent {
    time_t seconds;
    char typeOfEvent[EVENT_LOG_NAME_MAX_LENGTH];
} systemEvent_t;

//=====[Declaration and initialization of public global objects]===============
Timer eventTimer;
//=====[Declaration of external public global variables]=======================

//=====[Declaration and initialization of public global variables]=============

//=====[Declaration and initialization of private global variables]============

static bool sirenLastState = OFF;
static bool gasLastState   = OFF;
static bool tempLastState  = OFF;
static bool ICLastState    = OFF;
static bool SBLastState    = OFF;
static bool motionLastState         = OFF;
static int eventsIndex     = 0;
static systemEvent_t arrayOfStoredEvents[EVENT_LOG_MAX_STORAGE];
bool eventsStored = false;
bool eventRead = false;
//=====[Declarations (prototypes) of private functions]========================
void eventLogReadFileFromSdCard(const char* fileName);
static void eventLogElementStateUpdate( bool lastState,
                                        bool currentState,
                                        const char* elementName );

//=====[Implementations of public functions]===================================




void eventLogUpdate()
{
    static bool timerStarted = false;
    static float delay;


    static float rawDelay;
    int mappedDelayMs;

    if (!timerStarted) {
        eventTimer.start();
        timerStarted = true;
    }

    rawDelay = lightLevelControlRead();


    mappedDelayMs = 100 + (int)(rawDelay * 1000);


    pcSerialComStringWrite("Potentiometer Value = ");
    pcSerialComFloatWrite(rawDelay);
    pcSerialComStringWrite(" | Delay = ");
    pcSerialComIntWrite(mappedDelayMs);
    pcSerialComStringWrite(" ms\r\n");

    // Display alarms with delay between each
    pcSerialComStringWrite("ALARM_ON\r\n");
    ThisThread::sleep_for(mappedDelayMs);

    pcSerialComStringWrite("GAS_DET\r\n");
    ThisThread::sleep_for(mappedDelayMs);

    pcSerialComStringWrite("OVER_TEMP\r\n");
    ThisThread::sleep_for(mappedDelayMs);

    if (eventTimer.elapsed_time().count() < delay * 1e6) {
        return;
    }

    eventTimer.reset();

    bool currentState = sirenStateRead();
    eventLogElementStateUpdate( sirenLastState, currentState, "ALARM" );
    sirenLastState = currentState;

    currentState = gasDetectorStateRead();
    eventLogElementStateUpdate( gasLastState, currentState, "GAS_DET" );
    gasLastState = currentState;

    currentState = overTemperatureDetectorStateRead();
    eventLogElementStateUpdate( tempLastState, currentState, "OVER_TEMP" );
    tempLastState = currentState;

    currentState = incorrectCodeStateRead();
    eventLogElementStateUpdate( ICLastState, currentState, "LED_IC" );
    ICLastState = currentState;

    currentState = systemBlockedStateRead();
    eventLogElementStateUpdate( SBLastState ,currentState, "LED_SB" );
    SBLastState = currentState;

    currentState = motionSensorRead();
    eventLogElementStateUpdate( motionLastState ,currentState, "MOTION" );
    motionLastState = currentState;
}




int eventLogNumberOfStoredEvents()
{
    return eventsIndex;
}

void eventLogRead( int index, char* str )
{
    str[0] = '\0';
    strcat( str, "Event = " );
    strcat( str, arrayOfStoredEvents[index].typeOfEvent );
    strcat( str, "\r\nDate and Time = " );
    strcat( str, ctime(&arrayOfStoredEvents[index].seconds) );
    strcat( str, "\r\n" );
}

void eventLogWrite( bool currentState, const char* elementName )
{
    char eventAndStateStr[EVENT_LOG_NAME_MAX_LENGTH] = "";


    strcat( eventAndStateStr, elementName );
    if ( currentState ) {
        strcat( eventAndStateStr, "_ON" );
    } else {
        strcat( eventAndStateStr, "_OFF" );
    }

    arrayOfStoredEvents[eventsIndex].seconds = time(NULL);
    strcpy( arrayOfStoredEvents[eventsIndex].typeOfEvent, eventAndStateStr );
    if ( eventsIndex < EVENT_LOG_MAX_STORAGE - 1 ) {
        eventsIndex++;
    } else {
        eventsIndex = 0;
    }

    pcSerialComStringWrite(eventAndStateStr);
    pcSerialComStringWrite("\r\n");

}


bool eventLogSaveToSdCard()
{
    char fileName[SD_CARD_FILENAME_MAX_LENGTH];
    char eventStr[EVENT_STR_LENGTH] = "";


    time_t seconds;
    int i;

    seconds = time(NULL);
    fileName[0] = '\0';

    strftime( fileName, SD_CARD_FILENAME_MAX_LENGTH,
              "%Y_%m_%d_%H_%M_%S", localtime(&seconds) );
    strcat( fileName, ".txt" );

    for (i = 0; i < eventLogNumberOfStoredEvents(); i++) {
        eventLogRead( i, eventStr );
        if ( sdCardWriteFile( fileName, eventStr ) ){
            pcSerialComStringWrite("Storing event ");
            pcSerialComIntWrite(i+1);
            pcSerialComStringWrite(" in file ");
            pcSerialComStringWrite(fileName);
            pcSerialComStringWrite("\r\n");
            eventsStored = true;
        }
    }

    if ( eventsStored ) {
        pcSerialComStringWrite("File successfully written\r\n\r\n");

    } else {
        pcSerialComStringWrite("There are no events to store ");
        pcSerialComStringWrite("or SD card is not available\r\n\r\n");
    }

    return true;
}




bool eventLogLoadFromString(const char* eventStr)
{
    // Simulated parser, return true if non-empty
    return strlen(eventStr) > 0;
}

bool eventLogReadFileFromSdCard()
{
    DIR* dir;
    struct dirent* entry;
    char fullPath[128];
    char eventStr[EVENT_STR_LENGTH];
    FILE* file;
    size_t bytesRead;

    dir = opendir(("/sd/"));
    if (!dir) {
        pcSerialComStringWrite("Failed to open SD card directory\r\n");
        eventRead = false;
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {

        if (strstr(entry->d_name, ".txt") != NULL) {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", "/sd/", entry->d_name);

            file = fopen(fullPath, "r");
            if (!file) {
                pcSerialComStringWrite("Failed to open file ");
                pcSerialComStringWrite(fullPath);
                pcSerialComStringWrite("\r\n");
                continue;
            }

            bytesRead = fread(eventStr, 1, EVENT_STR_LENGTH - 1, file);
            eventStr[bytesRead] = '\0';
            fclose(file);

            if (eventLogLoadFromString(eventStr)) {
                pcSerialComStringWrite("Read events from file ");
                pcSerialComStringWrite(fullPath);
                pcSerialComStringWrite("\r\n");
                eventRead = true;
            } else {
                pcSerialComStringWrite("Failed to parse file ");
                pcSerialComStringWrite(fullPath);
                pcSerialComStringWrite("\r\n");
            }
        }
    }

    closedir(dir);
    return eventRead;
}
//=====[Implementations of private functions]==================================

static void eventLogElementStateUpdate( bool lastState,
                                        bool currentState,
                                        const char* elementName )
{
    if ( lastState != currentState ) {
        eventLogWrite( currentState, elementName );
    }
}

