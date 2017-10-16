# ESP-IR
Universal remote control for TV/DVD/Light based on ESP8266

ESP_IR universal remote control described here uses a ESP-12 and an optional RF433 sender to control TV/DVD/lightning.
Features:
-	Simulates one or more IR remote controllers for TV/DVD/Interactive TV.
-	Simulate RF sender to control lights in living room.
-	Uses a minimal number of components.
-	Can be controlled by a tablet or other device through a built-in webserver.
-	Pictures of original remote controls are used to control the ESP-IR.
-	The strongest available WiFi network is automatically selected.  Passwords are kept in the SPIFFS filesystem.
-	Heavily commented source code, easy to add extra functionality.
-	Debug information through serial output.
-	Update of software over WiFi (OTA).

See pdf-file for a full description.

Update 20-07-2017: Correction for IRremoteESP8266 v2.0 library.
