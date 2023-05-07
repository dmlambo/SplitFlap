# SplitFlap

ESP8266 based controller for split flap display modules. The code is all-in-one, meaning each module runs the same code, but in different modes set in EEPROM. The master runs the web server and controls the first unit, and each other unit acts as an I2C slave the control its own unipolar stepper motor.
