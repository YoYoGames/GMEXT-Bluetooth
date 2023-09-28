
#include "YoYoArduino.h"

void setup() 
{
  Serial.begin(115200);
}

void ECHOExample(char* buff, long _size)
{
    String resp = "ECHO: " + String(buff);
    uint16_t __size = resp.length()+1;
    char resp_buff[__size];//+1 due the end line /0
    resp.toCharArray(resp_buff, __size); 

    yyarduino_write_custom(resp_buff, __size);
}

void loop() 
{
  YoYoArduino_Tick(ECHOExample);
}
