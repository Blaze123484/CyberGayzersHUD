# CyberGayzersHUD
A project repository for my CyberGayzers heads up display based on the esp32

This project uses an html file launched on the users computer to connect to the esp32 and transmits screen captures over websockets
to be displayed via NTSC by way of lovyan gfx. In order to utilize the sketch you must use an esp32 that has PSRAM such as an esp32 wrover module or
a modified AI THINKER esp32 cam board; this is because lovyan gfx requires a signifigant ammount of memory to render the 720 x 480 display output that 
this project uses as well as the fact that the image data array is allocated from the PSRAM heap in order to avoid interfering with the wifi and 
websockets handling. This project is currently in a fairly rough state, as it currently stands the esp32 can only handle about 1 frame per every 2.5 
seconds withought intermitent crashes, updating the screen takes about .5 seconds, and the color depth is stuck at 8 bits as increasing it cause the wifi handling to interfere with lovyan gfx and trigger a watchdog crash. Please do not hesitate to fork, make modifications, or reach out with solutions and or possible recomendations.

Video showcase
    Youtube link: [CyberGayzers HUD screen share showcase](https://www.youtube.com/watch?v=dNitqhnO4hg)

Utilized libraries include

AsyncTCP Library
    License: MIT License
    GitHub: [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
    
ESPAsyncWebServer Library
    License: MIT License
    GitHub: [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)

ArduinoJson Library
    License: MIT License
    GitHub: [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
    
LovyanGFX Library
    License: MIT License
    GitHub: [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
