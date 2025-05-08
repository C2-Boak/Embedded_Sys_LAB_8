//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "user_interface.h"

#include "code.h"
#include "alarm.h"
#include "smart_home_system.h"
#include "fire_alarm.h"
#include "intruder_alarm.h"
#include "date_and_time.h"
#include "temperature_sensor.h"
#include "gas_sensor.h"
#include "motion_sensor.h"
#include "matrix_keypad.h"
#include "display.h"
#include "motor.h"
#include "gate.h"
#include "light_level_control.h"

//=====[Declaration of private defines]========================================

#define DISPLAY_REFRESH_TIME_REPORT_MS 3000
#define DISPLAY_REFRESH_TIME_ALARM_MS 1000

//=====[Declaration of private data types]=====================================

typedef enum {
    DISPLAY_ALARM_STATE,
    DISPLAY_REPORT_STATE
} displayState_t;

//=====[Declaration and initialization of public global objects]===============

InterruptIn gateOpenButton(PF_9);
InterruptIn gateCloseButton(PF_8);

DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

//=====[Declaration and initialization of public global variables]=============

char codeSequenceFromUserInterface[CODE_NUMBER_OF_KEYS];

//=====[Declaration and initialization of private global variables]============

static displayState_t displayState = DISPLAY_REPORT_STATE;
static int displayRefreshTimeMs = DISPLAY_REFRESH_TIME_REPORT_MS;

static bool incorrectCodeState = OFF;
static bool systemBlockedState = OFF;

static bool codeComplete = false;
static int numberOfCodeChars = 0;

//=====[Declarations (prototypes) of private functions]========================

static void userInterfaceMatrixKeypadUpdate();
static void incorrectCodeIndicatorUpdate();
static void systemBlockedIndicatorUpdate();

static void userInterfaceDisplayInit();
static void userInterfaceDisplayUpdate();
static void userInterfaceDisplayReportStateInit();
static void userInterfaceDisplayReportStateUpdate();
static void userInterfaceDisplayAlarmStateInit();
static void userInterfaceDisplayAlarmStateUpdate();
void userInterfaceDisplayEventStored();

static void gateOpenButtonCallback();
static void gateCloseButtonCallback();

//=====[Implementations of public functions]===================================

void userInterfaceInit()
{
    gateOpenButton.mode(PullUp);
    gateCloseButton.mode(PullUp);

    gateOpenButton.fall(&gateOpenButtonCallback);
    gateCloseButton.fall(&gateCloseButtonCallback);

    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
    matrixKeypadInit(SYSTEM_TIME_INCREMENT_MS);
    userInterfaceDisplayInit();

    lightLevelControlInit();
}

void userInterfaceUpdate()
{
    userInterfaceMatrixKeypadUpdate();
    incorrectCodeIndicatorUpdate();
    systemBlockedIndicatorUpdate();
    userInterfaceDisplayUpdate();
    lightLevelControlUpdate();
}

bool incorrectCodeStateRead()
{
    return incorrectCodeState;
}

void incorrectCodeStateWrite(bool state)
{
    incorrectCodeState = state;
}

bool systemBlockedStateRead()
{
    return systemBlockedState;
}

void systemBlockedStateWrite(bool state)
{
    systemBlockedState = state;
}

bool userInterfaceCodeCompleteRead()
{
    return codeComplete;
}

void userInterfaceCodeCompleteWrite(bool state)
{
    codeComplete = state;
}

//=====[Implementations of private functions]==================================

static void userInterfaceMatrixKeypadUpdate()
{
    static int numberOfHashKeyReleased = 0;
    char keyReleased = matrixKeypadUpdate();

    if (keyReleased != '\0') {

        if (alarmStateRead() && !systemBlockedStateRead()) {
            if (!incorrectCodeStateRead()) {
                codeSequenceFromUserInterface[numberOfCodeChars] = keyReleased;
                numberOfCodeChars++;
                if (numberOfCodeChars >= CODE_NUMBER_OF_KEYS) {
                    codeComplete = true;
                    numberOfCodeChars = 0;
                }
            } else {
                if (keyReleased == '#') {
                    numberOfHashKeyReleased++;
                    if (numberOfHashKeyReleased >= 2) {
                        numberOfHashKeyReleased = 0;
                        numberOfCodeChars = 0;
                        codeComplete = false;
                        incorrectCodeState = OFF;
                    }
                }
            }
        } else if (!systemBlockedStateRead()) {
            if (keyReleased == 'A') {
                motionSensorActivate();
            }
            if (keyReleased == 'B') {
                motionSensorDeactivate();
            }
        }
    }
}

static void userInterfaceDisplayReportStateInit()
{
    displayState = DISPLAY_REPORT_STATE;
    displayRefreshTimeMs = DISPLAY_REFRESH_TIME_REPORT_MS;

    displayClear();

    displayCharPositionWrite(0, 0);
    displayStringWrite("Temperature:");

    displayCharPositionWrite(0, 1);
    displayStringWrite("Gas:");

    displayCharPositionWrite(0, 2);
    displayStringWrite("Alarm:");
}

static void userInterfaceDisplayReportStateUpdate()
{
    char temperatureString[3] = "";
    char gasString[4] = "";

    sprintf(temperatureString, "%.0f", temperatureSensorReadCelsius());
    displayCharPositionWrite(12, 0);
    displayStringWrite(temperatureString);
    displayCharPositionWrite(14, 0);
    displayStringWrite("'C");

    sprintf(gasString, "%.0f", GasSenRead());
    displayCharPositionWrite(4, 1);
    displayStringWrite(gasString);
    displayCharPositionWrite(8, 1);
    displayStringWrite("PPM");

    displayCharPositionWrite(6, 2);
    displayStringWrite("OFF");
}

static void userInterfaceDisplayAlarmStateInit()
{
    displayState = DISPLAY_ALARM_STATE;
    displayRefreshTimeMs = DISPLAY_REFRESH_TIME_ALARM_MS;

    displayClear();
    userInterfaceDisplayAlarmStateUpdate();
}

static void userInterfaceDisplayAlarmStateUpdate()
{
    displayClear();

    if (gasDetectedRead() || overTemperatureDetectedRead()) {
        displayCharPositionWrite(0, 0);
        displayStringWrite("FIRE ALARM!");
        displayCharPositionWrite(0, 1);
        if (gasDetectedRead()) {
            displayStringWrite("Gas detected!");
        } else {
            displayStringWrite("Overtemp!");
        }
    } else if (intruderDetectedRead()) {
        displayCharPositionWrite(0, 0);
        displayStringWrite("INTRUDER ALERT!");
        displayCharPositionWrite(0, 1);
        displayStringWrite("Motion detected!");
    } else {
        displayCharPositionWrite(0, 0);
        displayStringWrite("ALARM ACTIVE");
        displayCharPositionWrite(0, 1);
        displayStringWrite("Check system.");
    }
}

static void userInterfaceDisplayInit()
{
    displayInit(DISPLAY_TYPE_LCD_HD44780, DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER);
    userInterfaceDisplayReportStateInit();
}

static void userInterfaceDisplayUpdate()
{
    static int accumulatedDisplayTime = 0;

    if (accumulatedDisplayTime >= displayRefreshTimeMs) {
        accumulatedDisplayTime = 0;

        switch (displayState) {
            case DISPLAY_REPORT_STATE:
                userInterfaceDisplayReportStateUpdate();

                if (alarmStateRead()) {
                    userInterfaceDisplayAlarmStateInit();
                }
                break;

            case DISPLAY_ALARM_STATE:
                userInterfaceDisplayAlarmStateUpdate();

                if (!alarmStateRead()) {
                    userInterfaceDisplayReportStateInit();
                }
                break;

            default:
                userInterfaceDisplayReportStateInit();
                break;
        }

    } else {
        accumulatedDisplayTime += SYSTEM_TIME_INCREMENT_MS;
    }
}

static void incorrectCodeIndicatorUpdate()
{
    incorrectCodeLed = incorrectCodeStateRead();
}

static void systemBlockedIndicatorUpdate()
{
    systemBlockedLed = systemBlockedState;
}

static void gateOpenButtonCallback()
{
    gateOpen();
}

static void gateCloseButtonCallback()
{
    gateClose();
}
void userInterfaceDisplayEventStored()
{
    displayClear();
    displayCharPositionWrite(0, 0);
    displayStringWrite("Events Stored");
    displayCharPositionWrite(0, 1);
    displayStringWrite("to SD Card");

    delay(4000);
    userInterfaceDisplayInit();
}