#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LovyanGFX.hpp>
#define LGFX_USE_V1

// to do list
// increase bit depth from 8 to 24 bits (currently causes Idle task on core zero to fuck up watchdog)
// increase screen update speed so it is not noticeable
// increase framerate to at least 24 fps preferably 30 so that it is 

// SSID and Password for the access point

const char* ssid = "Cyber Gayzers 69420";
const char* apPassword = "";

// SSID and password for the hardcoded wifi network (will attempt to connect at boot)

const char* hardcodedssid = "YOUR SSID"; // replace with your SSID
const char* hardcodedpassword = "YOUR PASSWORD"; // replace with your Password

// Message that will display on boot (default connection instructions)

String DisplayMessage = "";
String Screencolor = "#011d57";

// image settings for displayed jpgs (720 x 480 is the max resolution with color that I have been able to make work)

uint8_t DisplayedImageWidth = 720;
uint8_t DisplayedImageHeight = 480;

// delay variables for periodic screen update

unsigned long previousMillis = 0;
const unsigned long interval = 10000; // Delay interval in milliseconds.

// image data containing variables including the base64 encoded string and the unpopulated variables for
// the raw image data and size of the image data, the unpopulated variables are populated later initially
// during setup and again whenever a new frame is submitted from the client webpage
const size_t MAX_DECODED_DATA_SIZE = 500000; // Adjust the size based on your image resolution and color depth
uint8_t* decodedData = nullptr;
size_t decodedSize = 0;
// global variable that allows any function to call for the screen to be redrawn

bool drawnewframe = true;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// data for the html webpage (includes websocket and screensharing scripting)

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
/* Your existing styles here */
.collapsible {
  background-color: #777;
  color: white;
  cursor: pointer;
  padding: 18px;
  width: 50%;
  border: none;
  text-align: left;
  outline: none;
  font-size: 15px;
}

.active, .collapsible:hover {
  background-color: #555;
}

.collapsible:after {
  content: '\002B';
  color: white;
  font-weight: bold;
  float: right;
  margin-left: 5px;
}

.active:after {
  content: "\2212";
}

.content {
  padding: 0 18px;
  max-height: 0;
  overflow: hidden;
  transition: max-height 0.2s ease-out;
  background-color: #f1f1f1;
}
</style>
</head>
<body>

<h2>GoggleOS Webserver</h2>

<button class="collapsible">Connect To Server</button>
<div class="content">
    <input type="text" id="serverIp" style='font-size: 18px;' placeholder="Enter IP address">
    <span id="connectionStatus" style='font-size: 18px; color: red;'>Not Connected</span>
    <br>
    <button id="connectButton" style='font-size: 18px;'>Connect</button>
</div>

<button class="collapsible">Enter New Wifi Credentials</button>
<div class="content">
    <input type="text" id="ssidInput" style='font-size: 18px;' placeholder="SSID" ><br>
    <input type="password" id="passwordInput" style='font-size: 18px;' placeholder="Password"><br>
    <button onclick="submitNetworkCredentials();" style='font-size: 18px;'>Connect</button>
</div>

<button class="collapsible">Submit Message</button>
<div class="content">
    <input type="text" id="wifiMessageInput" style='font-size: 18px;' placeholder="Message"><br>
    <button onclick="submitWifiMessage();" style='font-size: 18px;'>Submit</button>
</div>

<button class="collapsible">Submit New Frame</button>
<div class="content">
    <!-- Replace the text input with an input element of type "file" for image selection -->
    <input type="file" id="imageFileInput" accept="image/jpeg" style='font-size: 18px;'>
    <br>
    <button onclick="submitImage();" style='font-size: 18px;'>Submit</button>
</div>
<button class="collapsible">Screen Capture</button>
<div class="content">
    <br>
    <video autoplay height="480px" hidden></video>
    <br>
    <canvas id="canvas" width="720" height="480"></canvas><br>
    <button id="shareButton" onclick="startCapture();" style='font-size: 18px;'>Share</button>
    <button id="stopShareButton" onclick="stopCapture();" style='font-size: 18px; display: none;'>Stop Sharing</button>
    <button id="captureAndSendButton" onclick="captureAndSend();" style='font-size: 18px; display: none;'>Capture & Send</button>
    
    <!-- Toggleable button for auto capture -->
    <button id="autoCaptureToggleButton" onclick="toggleAutoCapture();" style='font-size: 18px;'>Start Auto Capture</button>
</div>

<button class="collapsible">Camera</button>
<div class="content">
    <!--placeholder for future development stuff-->
</div>

<button class="collapsible">Reflash</button>
<div class="content">
    <!--placeholder for future development stuff-->
</div>

<script>
var coll = document.getElementsByClassName("collapsible");
var i;

for (i = 0; i < coll.length; i++) {
  coll[i].addEventListener("click", function() {
    this.classList.toggle("active");
    var content = this.nextElementSibling;
    if (content.style.maxHeight){
      content.style.maxHeight = null;
    } else {
      content.style.maxHeight = content.scrollHeight + "px";
    } 
  });
}

var connectionStatus = document.getElementById("connectionStatus");

// WebSocket connection handling
document.getElementById("connectButton").addEventListener("click", function() {
  const serverIp = document.getElementById("serverIp").value;
  if (!serverIp) {
    console.error("Please enter the IP address of the ESP32 server.");
    return;
  }

  const websocketAddress = `ws://${serverIp}/ws`;
  websocket = new WebSocket(websocketAddress);
  console.log("attempting websocket connection");
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
});

function onOpen(event) {
    console.log('Connection opened');
    connectionStatus.textContent = "Connected";
    connectionStatus.style.color = "green";
}

function onClose(event) {
    console.log('Connection closed');
    connectionStatus.textContent = "Not Connected";
    connectionStatus.style.color = "red";
    stopCapture();
    clearInterval(autoCaptureInterval); // Stop the auto capture interval
    document.getElementById("autoCaptureToggleButton").textContent = "Start Auto Capture";
    
  setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
  var state;
  if (event.data == "1"){
    state = "ON";
  } else {
    state = "OFF";
  }
  document.getElementById('state').innerHTML = state;
}

// Consolidated function for submitting image data
async function submitImage() {
  var imageFileInput = document.getElementById("imageFileInput");
  var imageFile = imageFileInput.files[0]; // Get the selected image file

  if (!imageFile) {
    console.log("No image selected.");
    return;
  }

  // Read the selected image file as an ArrayBuffer (raw binary data)
  var reader = new FileReader();

  reader.onloadend = function (e) {
    if (e.target.readyState === FileReader.DONE) {
      var rawImageData = e.target.result;
      websocket.send("START_IMAGE_TRANSMISSION");
      websocket.send(new Uint8Array(rawImageData)); // Send the Uint8Array directly
      websocket.send("END_IMAGE_TRANSMISSION");
    } else {
      console.log("Error reading the file.");
    }
  };

  reader.readAsArrayBuffer(imageFile);
}

var canvas = document.querySelector('#canvas');
var ctx = canvas.getContext('2d');
var video = document.querySelector('video');
var desiredWidth = 720;
var desiredHeight = 480;

var shareButton = document.getElementById("shareButton");
var stopShareButton = document.getElementById("stopShareButton");
var captureAndSendButton = document.getElementById("captureAndSendButton");

async function startCapture() 
{
    video.srcObject = await navigator.mediaDevices.getDisplayMedia({ video: { cursor: "always" }, audio: false });
    video.addEventListener('play', function() {
        drawVideoFrame();
    });

    shareButton.style.display = "none";
    stopShareButton.style.display = "block";
    captureAndSendButton.style.display = "block";
}

function stopCapture() 
{
    const stream = video.srcObject;
    const tracks = stream.getTracks();

    tracks.forEach(function(track) {
        track.stop();
    });

    video.srcObject = null;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    shareButton.style.display = "block";
    stopShareButton.style.display = "none";
    captureAndSendButton.style.display = "none";
}

function drawVideoFrame() 
{
    if (video.paused || video.ended) {
        return;
    }
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(video, 0, 0, desiredWidth, desiredHeight);
    requestAnimationFrame(drawVideoFrame);
}

function captureAndSend() 
{
    const imageData = canvas.toDataURL('image/jpeg', 0.3);

    // Convert the image data URL to a Blob and send it as an ArrayBuffer
    fetch(imageData)
        .then(response => response.blob())
        .then(blob => {
            const reader = new FileReader();
            reader.onload = function(event) {
                const arrayBuffer = event.target.result;
                websocket.send("START_IMAGE_TRANSMISSION");
                websocket.send(new Uint8Array(arrayBuffer)); // Send the Uint8Array directly
                websocket.send("END_IMAGE_TRANSMISSION");
            };
            reader.readAsArrayBuffer(blob);
        });
}
var isAutoCaptureActive = false;

function toggleAutoCapture() {
    if (!isAutoCaptureActive) {
        autoCaptureInterval = setInterval(captureAndSend, 2500); // Capture and send every 2.5 seconds
        document.getElementById("autoCaptureToggleButton").textContent = "Stop Auto Capture";
    } else {
        clearInterval(autoCaptureInterval); // Stop the auto capture interval
        document.getElementById("autoCaptureToggleButton").textContent = "Start Auto Capture";
    }
    
    isAutoCaptureActive = !isAutoCaptureActive;
}

// ... (rest of the existing JavaScript code) ...

</script>

</body>
</html>

)rawliteral";

// definitions and declarations of the lovyan GFX composite example, instructions and comments contained 
// within the lovyan GFX class declaration are not mine, I simply copied and google-translated 
// them from the example code

class LGFX : public lgfx::LGFX_Device
{
public:

  lgfx::Panel_CVBS _panel_instance;

  LGFX(void)
  {
    { // Set display panel control.
      auto cfg = _panel_instance.config();    // Get the structure for display panel settings.

      // 出力解像度を設定;
      cfg.memory_width  = 720; // output resolution width
      cfg.memory_height = 480; // output resolution height

      // 実際に利用する解像度を設定;
      cfg.panel_width  = 720;  // Actual width to use (set equal to or smaller than memory_width)
      cfg.panel_height = 480;  // Actual height to use (Set a value equal to or smaller than memory_height)

      // Set display position offset amount;
      cfg.offset_x = 0;       // Amount to shift the display position to the right (initial value 0)
      cfg.offset_y = 0;       // Amount to shift the display position downward (initial value 0)

      _panel_instance.config(cfg);


// Normally use the same value for memory_width and panel_width, with offset_x = 0. ;
// If you want to prevent the display at the edge of the screen from being hidden 
//outside the screen, set the value of panel_width smaller  than memory_width and 
// adjust the left/right position with offset_x. ;
// For example, if panel_width is set to 32 less than memory_width, setting offset_x 
// to 16 will center the left and right positions. ;
// Adjust the vertical direction (memory_height, panel_height, offset_y) as needed. ;

    }

    {
      auto cfg = _panel_instance.config_detail();

      // 出力信号の種類を設定;
       cfg.signal_type = cfg.signal_type_t::NTSC;
      // cfg.signal_type = cfg.signal_type_t::NTSC_J;
      // cfg.signal_type = cfg.signal_type_t::PAL;
      // cfg.signal_type = cfg.signal_type_t::PAL_M;
      // cfg.signal_type = cfg.signal_type_t::PAL_N;

      // Set the output GPIO number;
      cfg.pin_dac = 26;       // Only 25 or 26 can be selected as it uses DAC;

      // set PSRAM memory allocation;
      cfg.use_psram = 2;      // 0=no PSRAM used / 1=half PSRAM and half SRAM / 2=full PSRAM;

// set the amplitude strength of the output signal;
      cfg.output_level = 128; // initial value 128
// *Increase the value if the signal is attenuated due to reasons such as the GPIO having a protection resistor. ;
// * M5StackCore2 recommends 200 because GPIO has protection resistance. ;

// set the amplitude strength of the chroma signal;
      cfg.chroma_level = 128; // initial value 128
// Lower numbers desaturate, 0 is black and white. Increasing the number increases the saturation. ;

      _panel_instance.config_detail(cfg);
    }

    setPanel(&_panel_instance);
  }
};

LGFX gfx;

void notifyClients() 
{
  String stateStr = "Placeholder";
  ws.textAll(stateStr);
}

void handleRoot(AsyncWebServerRequest* request) 
{
  request->send(200, "text/html", index_html);
}

// draws the contents of the decoded uint8_t array to the screen and then writes the connected network, IP, and displaymessage
void drawframe()
{
  gfx.drawJpg(decodedData, decodedSize, 0, 0, 720, 480, 0, 0, 1, 1, top_left);
  gfx.setCursor(15,15);
  gfx.print(WiFi.SSID());
  gfx.print("  ");
  gfx.print(WiFi.localIP());
  gfx.print("  ");
  gfx.print(DisplayMessage);
}

// Function to clear the decodedData array
void clearDecodedData() {
  if (decodedData != nullptr) {
    memset(decodedData, 0, MAX_DECODED_DATA_SIZE);
    decodedSize = 0; // Reset the size to 0
  }
}


void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) 
{
  AwsFrameInfo* info = (AwsFrameInfo*)arg;

  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
  {
      data[len] = 0;

      // Handle the received JSON data
      char* jsonString = (char*)malloc(len + 1); // +1 for null-terminator
      memcpy(jsonString, data, len);
      jsonString[len] = '\0'; // Null-terminate the string
      DynamicJsonDocument jsonDoc(1024);
      deserializeJson(jsonDoc, jsonString);

      String identifier = jsonDoc["identifier"].as<String>();

      // Saves new submitted wifi credentials
      if (identifier == "save-credentials") 
      {
        // Handle save-credentials request
        String ssid = jsonDoc["ssid"].as<String>();
        String password = jsonDoc["password"].as<String>();

        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.println("Connecting to a new Wi-Fi network...");
        Serial.println(WiFi.localIP());
        DisplayMessage = "Connecting to a new WIFI network";
        drawnewframe = true;

      }

      // Saves submitted message
      else if (identifier == "save-wifiMessage") 
      {
        // Handle save-wifiMessage request
        String wifiMessage = jsonDoc["WifiMessage"].as<String>();
        Serial.println("Message received: " + wifiMessage);
        DisplayMessage = wifiMessage;
        drawnewframe = true;

      }

      // Handles begining and end of image data transmission
      else if (strcmp((char*)data, "START_IMAGE_TRANSMISSION") == 0)
      {
        Serial.println("Begining of framedata transmission");
      }
      else if (strcmp((char*)data, "END_IMAGE_TRANSMISSION") == 0)
      {
        Serial.print("End of framedata transmission, size of image in bytes: ");
        Serial.println(decodedSize);
        gfx.clear();
        drawnewframe = true;
      }
      else 
      {
        Serial.println("non specified textual info recieved");
      }
      // Free memory used for JSON processing
      free(jsonString);
  }

  // Handles recieving framedata packets and saves to uint8_t array
  else if(info->opcode == WS_BINARY)
  {
    if(info->index == 0) // first packet of framedata
    {
      clearDecodedData();
      memcpy(&decodedData[decodedSize], data, len);
      decodedSize += len;
      /*
      Serial.print("First packet of framedata recieved, index: ");
      Serial.print(info->index);
      Serial.print(" opcode: ");
      Serial.print(info->opcode);
      Serial.print(" last: ");
      Serial.println(info->final);
      */
    } 
    else // any other packet of framedata
    {
      memcpy(&decodedData[decodedSize], data, len);
      decodedSize += len;
      /*
      Serial.print("Interim packet of framedata recieved, index: ");
      Serial.print(info->index);
      Serial.print(" opcode: ");
      Serial.print(info->opcode);
      Serial.print(" last: ");
      Serial.println(info->final);
      */
    } 
  }
}

// Websocket event handling
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// initializes websockets called in server task
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// setup runs once at the begining of every boot and creates pinned tasks as well as initializing processes that need to run

// the two looping functions (loop and serverTask) are used for different functions. loop is used for running the 
// lovyan gfx composite video and serverTask is used for the webserver and various background tasks (basically everything but video).
// I have structured the code this way because arduino causes code to be executed on core 1 by default and because I do not have total
// control over how lovyan gfx runs the composite video timings I cannot ensure that other processes will not interfere with the
// the display. forcing all other processes to run on the other core ensures that nothing takes CPU cycles that lovyan GFX needs to 
// keep the video running smoothly

// looping function that runs on core 0 (basically another loop that runs on the other core)
// do not move server task to after setup, it is not a default funciton and therefore must be 
// defined before it is called or refrenced (including in creatextaskpinnedtocore) I didn't do 
// a prototype because I didn't want to
void serverTask(void* parameter) {
  // Set ESP32 as Access Point

  server.on("/", HTTP_GET, handleRoot);
  WiFi.begin(hardcodedssid, hardcodedpassword);
  WiFi.softAP(ssid, apPassword);
  Serial.println("Access Point started.");
  initWebSocket();
  server.begin();
  
  while (1) {
    delay(1);  // Allow other tasks to run
  }
  ws.cleanupClients();
}

void setup() {
  // Create a task to run the server on Core 0
  xTaskCreatePinnedToCore(
    serverTask,    // Task function
    "Server Task", // Task name
    4096,          // Stack size (bytes)
    NULL,          // Task parameter
    0,             // Task priority
    NULL,          // Task handle
    0              // Core number (0 for Core 0)
  );

  Serial.begin(115200);

  // initialises psram and allocates 1.5 megabytes of space for the uint8_t imagebuffer
  if (psramFound()) {
    psramInit();
    Serial.println("PSRAM initialized.");
  }

  decodedData = (uint8_t*)ps_malloc(MAX_DECODED_DATA_SIZE);

  if (decodedData == nullptr) {
    Serial.println("PSRAM allocation failed for decodedData.");
    while (1) {} // Halt program execution if allocation fails
  }

  gfx.init();
  gfx.setColorDepth(8);

  // prints clock speed of CPU for debugging purposes
  Serial.print("Cpu clock speed: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" Mhz");
}

// looping funtion that runs on core 1
void loop() 
{
  // This mills if statement is used to update the screen if a new frame has not been drawn
  // in the last 10 seconds, this is done to ensure that an accurate IP address is shown withought
  // having to manually force a screen update from the webpage
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    drawframe();
    // Reset the timer
    previousMillis = currentMillis;
  }

  // this function only redraws the frame when the drawnewframe variable is set to true
  // which indicates that something has changed such as a new frame being submitted or 
  // a new messge being transmitted
  if(drawnewframe == true){
    drawframe();
    previousMillis = currentMillis;
    drawnewframe = false;
  }
}
