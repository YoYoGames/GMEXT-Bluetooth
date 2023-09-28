
#include "YoYoArduino.h"

void yyarduino_write_custom(char* resp_buff,uint16_t __size)
{
  //uint16_t __size = sizeof(resp_buff);
  char custom_response[__size+3];
  custom_response[0] = Action_custom;
  custom_response[1] = floor(__size/16);
  custom_response[2] = __size%16  ;
  
  memcpy(custom_response+3,resp_buff,__size);
  Serial.write(custom_response,__size+3);
}
        
void YoYoArduino_Tick(void (*function)(char*,long))
{
  if (Serial.available() >= 2)
  {
    int action,pin,value;
    
    action = Serial.read();
    pin = Serial.read();
    
    switch(action)
    {
      case Action_pinMode:
        while(!Serial.available()){}
        value = Serial.read();
        pinMode(pin,value);
      break;
      
      case Action_digitalWrite:
        while(!Serial.available()){}
        value = Serial.read();
        digitalWrite(pin,value);
      break;
      
      case Action_digitalRead:
        value = digitalRead(pin);
        char response_digital[3];
        response_digital[0] = action;
        response_digital[1] = pin;
        response_digital[2] = value;
        Serial.write(response_digital,3);
      break;
      
      case Action_analogWrite:
        while(!Serial.available()){}
        value = Serial.read();
        analogWrite(pin,value);
      break;
      
      case Action_analogRead:
        value = analogRead(pin);
        char response_analog[3];
        response_analog[0] = action;
        response_analog[1] = pin;
        response_analog[2] = value;
        Serial.write(response_analog,3);
      break;

      case Action_custom:
        byte byte0 = pin;//Serial.read();
        while(!Serial.available()){}
        byte byte1 = Serial.read();
        
        uint16_t _size = /*(byte2 << 16) |*/ (byte1 << 8) | (byte0);

        char buff[_size];
        uint16_t buff_ind = 0;
        for(int i = 0 ; i < _size ; i++)
        {
          while(!Serial.available()){}
          buff[i] = Serial.read();
        }
        
        if(_size > 0)
        {
          (*function)(buff,_size);
          //memcpy(_buff,buff,_size);
        }
      break;
    }
  }
}
