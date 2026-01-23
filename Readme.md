Description
This is the firmware for the gate node

Features
It communicates with the home node using espnow
Host a http server on its softap for ota update
Uses PWM to open/close the linear actuator


Working

On bootup it scans all channels of wifi until the communication with home node is success.
So, it keeps deinit and init of espnow until it hears from home node.
Once that phase is over, it starts softap and http server.
It listens for commands from home node and controls the linear acctuator accordingly, and sends back ack

OTA

Connect to the softap named esp32_config
password: esp32pass

Then open
192.168.4.1


user name: admin
password : pindora


