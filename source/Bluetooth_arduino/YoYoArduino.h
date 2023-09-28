
#define Action_pinMode 0
#define Action_digitalWrite 1
#define Action_digitalRead 2
#define Action_analogWrite 3 
#define Action_analogRead 4
#define Action_custom 5

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "Arduino.h"

void yyarduino_write_custom(char* resp_buff,uint16_t __size);
void YoYoArduino_Tick(void (*function)(char*,long));
