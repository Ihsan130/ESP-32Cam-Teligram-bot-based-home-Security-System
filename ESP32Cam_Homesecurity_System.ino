
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"


#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5

#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22


#define PIR_PIN 13
#define FLASH_LED_PIN 4

const char* AP_SSID = "ESP32-CAM-Setup";
const char* AP_PASSWORD = "camsetup123";

const unsigned long MOTION_COOLDOWN = 15000UL;
const unsigned long WIFI_RETRY_INTERVAL = 15000UL;
const unsigned long WIFI_CONNECT_TIMEOUT = 15000UL;
const unsigned long CAMERA_RETRY_INTERVAL = 10000UL;

const byte DNS_PORT = 53;


WebServer server(80);
DNSServer dnsServer;
Preferences preferences;


String savedSSID;
String savedPassword;

String botToken;
String chatID;

bool portalActive = false;
bool cameraReady = false;

unsigned long lastMotionTime = 0;
unsigned long lastWiFiRetry = 0;
unsigned long lastCameraRetry = 0;

int reconnectFailures = 0;

bool initCamera() {

  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  config.pixel_format = PIXFORMAT_JPEG;

  // Low-memory image configuration
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  if (psramFound()) {

    config.fb_location = CAMERA_FB_IN_PSRAM;

    Serial.println("PSRAM detected.");

  } else {

    config.fb_location = CAMERA_FB_IN_DRAM;
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 16;

    Serial.println("PSRAM not detected. Using smaller image.");
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.printf("Camera error: 0x%x\n", err);

    return false;
  }

  Serial.println("Camera initialized.");

  return true;
}


const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>ESP32-CAM Setup</title>

<style>

body {
  font-family: Arial;
  background: #f2f2f2;
  padding: 20px;
}

.container {
  max-width: 420px;
  margin: auto;
  background: white;
  padding: 20px;
  border-radius: 10px;
}

input, button {
  width: 100%;
  padding: 12px;
  margin: 10px 0;
  box-sizing: border-box;
}

button {
  background: #007bff;
  color: white;
  border: none;
  border-radius: 6px;
}

h3 {
  color: #007bff;
}

</style>

</head>

<body>

<div class="container">

<h2>ESP32-CAM Setup</h2>

<form action="/save" method="POST">

<h3>Home Wi-Fi Configuration</h3>

<label>Wi-Fi SSID</label>
<input name="ssid" required placeholder="Enter Wi-Fi SSID">

<label>Wi-Fi Password</label>
<input name="password" type="password"
placeholder="Enter Wi-Fi password">

<h3>Telegram Configuration</h3>

<label>Telegram Bot Token</label>
<input name="token" required
placeholder="Enter Telegram Bot Token">

<label>Telegram Chat ID</label>
<input name="chatid" required
placeholder="Enter Telegram Chat ID">

<button type="submit">Save & Connect</button>

</form>

</div>

</body>
</html>
)rawliteral";


bool connectWiFi(const String& ssid, const String& password) {

  if (ssid.isEmpty()) {
    return false;
  }

  WiFi.setSleep(false);

  Serial.print("Connecting to: ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < WIFI_CONNECT_TIMEOUT) {

    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi connected.");

    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    reconnectFailures = 0;

    return true;
  }

  Serial.println("Wi-Fi connection failed.");

  WiFi.disconnect(false);

  return false;
}


void handleRoot() {

  server.send_P(200, "text/html", PORTAL_HTML);
}

void handleSave() {

  if (!server.hasArg("ssid") ||
      !server.hasArg("token") ||
      !server.hasArg("chatid")) {

    server.send(400, "text/plain", "Required fields missing");

    return;
  }

  String newSSID = server.arg("ssid");
  String newPassword = server.arg("password");

  String newBotToken = server.arg("token");
  String newChatID = server.arg("chatid");

  newSSID.trim();
  newBotToken.trim();
  newChatID.trim();

  if (newSSID.isEmpty() ||
      newBotToken.isEmpty() ||
      newChatID.isEmpty()) {

    server.send(400, "text/plain", "Required fields cannot be empty");

    return;
  }

  // Send response before connection attempt

  server.send(200, "text/html",
    "<html><body><h3>Settings saved.</h3>"
    "<p>Connecting to Wi-Fi...</p>"
    "<p>You may close this page.</p>"
    "</body></html>");

  // Save configuration in Preferences

  preferences.putString("ssid", newSSID);
  preferences.putString("password", newPassword);

  preferences.putString("token", newBotToken);
  preferences.putString("chatid", newChatID);

  // Update running variables

  savedSSID = newSSID;
  savedPassword = newPassword;

  botToken = newBotToken;
  chatID = newChatID;

  Serial.println("Configuration saved.");

  delay(500);

  if (connectWiFi(savedSSID, savedPassword)) {

    if (portalActive) {

      dnsServer.stop();

      WiFi.softAPdisconnect(true);

      portalActive = false;

      Serial.println("Setup AP stopped.");
    }

  } else {

    Serial.println("Connection failed. AP remains active.");
  }
}

void handleRedirect() {

  server.sendHeader("Location", "http://192.168.4.1/", true);

  server.send(302, "text/plain", "");
}


void startPortal() {

  if (portalActive) {
    return;
  }

  Serial.println("Starting setup AP.");

  WiFi.mode(WIFI_AP_STA);

  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress apIP = WiFi.softAPIP();

  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", HTTP_GET, handleRoot);

  server.on("/save", HTTP_POST, handleSave);

  server.on("/generate_204", HTTP_GET, handleRedirect);
  server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
  server.on("/connecttest.txt", HTTP_GET, handleRedirect);
  server.on("/ncsi.txt", HTTP_GET, handleRedirect);
  server.on("/fwlink", HTTP_GET, handleRedirect);

  server.onNotFound(handleRedirect);

  server.begin();

  portalActive = true;

  Serial.print("AP IP: ");
  Serial.println(apIP);
}

bool sendTelegramPhoto(camera_fb_t* fb) {

  if (!fb || WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (botToken.isEmpty() || chatID.isEmpty()) {

    Serial.println("Telegram configuration missing.");

    return false;
  }

  WiFiClientSecure client;


  client.setInsecure();

  client.setTimeout(12000);

  if (!client.connect("api.telegram.org", 443)) {

    Serial.println("Telegram connection failed.");

    return false;
  }

  const char* boundary = "CAMBoundary7MA4YWxk";

  char url[180];

  snprintf(url, sizeof(url),
           "/bot%s/sendPhoto", botToken.c_str());

  String part1;

  part1.reserve(300);

  part1 += "--";
  part1 += boundary;
  part1 += "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  part1 += chatID;
  part1 += "\r\n--";
  part1 += boundary;
  part1 += "\r\nContent-Disposition: form-data; name=\"caption\"\r\n\r\n";
  part1 += "Motion detected!";
  part1 += "\r\n--";
  part1 += boundary;
  part1 += "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"capture.jpg\"\r\n";
  part1 += "Content-Type: image/jpeg\r\n\r\n";

  String part2;

  part2.reserve(50);

  part2 += "\r\n--";
  part2 += boundary;
  part2 += "--\r\n";

  size_t contentLength =
    part1.length() + fb->len + part2.length();

  client.printf("POST %s HTTP/1.1\r\n", url);
  client.println("Host: api.telegram.org");
  client.println("Connection: close");

  client.print("Content-Type: multipart/form-data; boundary=");
  client.println(boundary);

  client.print("Content-Length: ");
  client.println(contentLength);

  client.println();

  client.print(part1);

  size_t sent = client.write(fb->buf, fb->len);

  client.print(part2);

  if (sent != fb->len) {

    Serial.println("Photo data transmission incomplete.");

    client.stop();

    return false;
  }


  char responseBuffer[200] = {0};

  size_t responseLength = 0;

  bool telegramOK = false;

  unsigned long start = millis();

  while (millis() - start < 12000) {

    while (client.available()) {

      char c = client.read();


      if (responseLength < sizeof(responseBuffer) - 1) {

        responseBuffer[responseLength++] = c;

        responseBuffer[responseLength] = '\0';
      }


      if (strstr(responseBuffer, "\"ok\":true") != nullptr) {

        telegramOK = true;
      }


      if (responseLength >= sizeof(responseBuffer) - 1) {

       
        memmove(responseBuffer,
                responseBuffer + responseLength - 12,
                12);

        responseLength = 12;

        responseBuffer[responseLength] = '\0';
      }
    }

    if (!client.connected() && !client.available()) {
      break;
    }

    delay(10);
  }

  client.stop();

  Serial.println("Telegram response:");

  Serial.println(responseBuffer);

  if (telegramOK) {

    Serial.println("Telegram photo sent.");

    return true;
  }

  Serial.println("Telegram response not confirmed.");

  return false;
}


void handleMotion() {

  Serial.println("Motion detected.");

  digitalWrite(FLASH_LED_PIN, HIGH);

  delay(150);

  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {

    Serial.println("Capture failed.");

    digitalWrite(FLASH_LED_PIN, LOW);

    return;
  }

  Serial.printf("Captured: %u bytes\n", fb->len);

  sendTelegramPhoto(fb);

  // Release framebuffer immediately

  esp_camera_fb_return(fb);

  digitalWrite(FLASH_LED_PIN, LOW);

  Serial.println("Motion processing finished.");
}


void setup() {

  Serial.begin(115200);

  delay(500);

  pinMode(PIR_PIN, INPUT);

  pinMode(FLASH_LED_PIN, OUTPUT);

  digitalWrite(FLASH_LED_PIN, LOW);

  Serial.println();
  Serial.println("ESP32-CAM Security System");

  preferences.begin("camwifi", false);


  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");

  botToken = preferences.getString("token", "");
  chatID = preferences.getString("chatid", "");

  cameraReady = initCamera();

  WiFi.mode(WIFI_STA);

  WiFi.setSleep(false);

  if (savedSSID.isEmpty() ||
      botToken.isEmpty() ||
      chatID.isEmpty()) {

    Serial.println("Configuration incomplete.");

    startPortal();

  } else {

    if (!connectWiFi(savedSSID, savedPassword)) {

      startPortal();
    }
  }
}


void loop() {

  if (portalActive) {

    dnsServer.processNextRequest();

    server.handleClient();
  }

  // Wi-Fi recovery

  if (WiFi.status() != WL_CONNECTED) {

    if (millis() - lastWiFiRetry >= WIFI_RETRY_INTERVAL) {

      lastWiFiRetry = millis();

      if (!savedSSID.isEmpty()) {

        if (connectWiFi(savedSSID, savedPassword)) {

          if (portalActive) {

            dnsServer.stop();

            WiFi.softAPdisconnect(true);

            portalActive = false;
          }

        } else {

          reconnectFailures++;

          if (reconnectFailures >= 3 && !portalActive) {

            startPortal();
          }
        }
      }
    }

    delay(5);

    return;
  }

 
  if (!cameraReady) {

    if (millis() - lastCameraRetry >= CAMERA_RETRY_INTERVAL) {

      lastCameraRetry = millis();

      cameraReady = initCamera();
    }

    delay(5);

    return;
  }


  if (digitalRead(PIR_PIN) == HIGH) {

    if (millis() - lastMotionTime >= MOTION_COOLDOWN) {

      lastMotionTime = millis();

      handleMotion();
    }
  }

  delay(5);
}
