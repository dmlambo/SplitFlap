# SplitFlap

ESP8266 based controller for split flap display modules. The code is all-in-one, meaning each module runs the same code, but in different modes set in EEPROM. The master runs the web server and controls the first unit, and each other unit acts as an I2C slave to control its own unipolar stepper motor.

## Attribution
This design is mostly [David Königsmann's fantastic design](https://github.com/Dave19171/split-flap) with changes to improve printability, improve durability, or reduce filament use. Specifically the unit has been reduced to 8 characters so it could be printed on an Ender 3 V2. That said, most of the changes are just personal preference, and hats off to David for his great design.

## Electronics
The name of the game is low-cost, and ease of construction. Not that David's design isn't already low cost, but I am a cheapskate. Each module's BOM is very low, making use of the ULN2003 that you usually get with your 28BYJ-48 steppers (as with David's) but instead of having an Arduino Nano for each, along with an ESP01, each module only uses a single ultra-cheap Wemos Lolin D1 Mini V4 (any version with the ESP8266EX, without the metal sheild, will work). The rest is just support components, and everything is through-hole mounted. 

## Software
The main difference between the two projects is in the software. Since David's module was split into two MCUs, on two different architectures, he had to maintain two different codebases. Therefore I chose to unify mine, not just for ease of programming (and debugging) but also easy of maintenance. Since everything is software controlled, including the I2C address, one only has to connect to Serial over USB on the master, or on the unit itself, to change any running parameters they want. 
Similarly, the web interface does a few things for you. There is a frontend to change what's displayed, and to change, for example, the timezone and date formating. However, there is also a RESTful API available if someone wanted to make their own front end.
Finally, indeed the software uses the Arduino platform code, but rather than being an Arduino project, it is a PlatformIO project. This was important for organization, since the way Arudino mangles up your code means compartmentalizing is often impossible.

## Future
I'd like to expand the software a little bit. For instance, I would like to go off-spec on I2C and give each module multiple addresses to get certain tasks done, like multi-client firmware update over I2C, and broadcast messages used to reset everything all at once, or to make sure all the clicks and clacks are perfectly in sync with a start signal.
