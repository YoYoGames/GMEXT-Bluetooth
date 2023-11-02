
value += 50
if(value > 255)
	value = 0

text = "Analog Write: " + string(value)

arduino_analog_write(pin,value)
