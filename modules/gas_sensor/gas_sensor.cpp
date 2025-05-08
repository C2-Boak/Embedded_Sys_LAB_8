//=====[Libraries]=============================================================

#include "fire_alarm.h"
#include "mbed.h"
#include "gas_sensor.h"


#define Dangerous_Gas_Level                    3000


AnalogIn GasSen0127(A2);


float Gas_Level            = 0.0;



float GasSen0127V(float analogRead)
{
    if (analogRead > 0.3){
        return (analogRead*4800);
    }else{
        return 0.00;
    }

}

float GasSenRead (){
    Gas_Level = GasSen0127V(GasSen0127);

    return Gas_Level;

}

bool gasDetectedRead(){
    if (Gas_Level > Dangerous_Gas_Level) {
        return 1;
    }else{
        return 0;
    }
}

void gasSensorReset()
{
    Gas_Level = 0.0;
}