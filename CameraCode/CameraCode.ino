#include <esp32cam.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define FLASH_IO 4
#define BUTTON_IO 12
#define GREEN_LED 2
#define RED_LED 13

const char* ssid = "MY_SSID";
const char* password = "MY_PASSWORD";

const char* SERVER_URL = "SERVERL_URL_PLACE_HERE";

// begin connecting to the WIFI 
void connectToWiFi() {
  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

// Camera initialization
bool initCamera() {

  esp32cam::Config cfg;
  cfg.setPins(esp32cam::pins::AiThinker);
  cfg.setResolution(esp32cam::Resolution::find(640, 480));
  cfg.setJpeg(80);

  if (!esp32cam::Camera.begin(cfg)) {
    Serial.println("Camera init failed");
    return false;
  }

  Serial.println("Camera initialized");
  return true;
}

/*
1- Capture the image
2- Send the image via http post
*/
bool captureImage() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting.....");
    bool check = WiFi.reconnect();
    if (check) {
      Serial.println("Reconnected.....");
    } else {
      Serial.println("Failed to reconnect.....");
    }
  }

  digitalWrite(FLASH_IO, HIGH);
  delay(250);
  auto img = esp32cam::capture();
  digitalWrite(FLASH_IO, LOW);

  if (img == nullptr) {
    Serial.println("Failed to capture image");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  //Accepting self-signed certificates

  HTTPClient http;
  http.setTimeout(15000);
  Serial.println("[HTTP] begin...");
  if (!http.begin(client, SERVER_URL)) {
    Serial.println("Unable to connect to server");
    return false;
  }

  http.addHeader("Content-Type", "image/jpeg");
  Serial.println("[HTTP] POST...");
  int httpResponseCode = http.POST(img->data(), img->size());

  if (httpResponseCode == 200) {
    Serial.printf("Server response code: %d\n", httpResponseCode);
    String response = http.getString();
    Serial.println("Server response: " + response);
    digitalWrite(GREEN_LED, HIGH);
    delay(1000);
    digitalWrite(GREEN_LED, LOW);

  } else if (httpResponseCode == 400) {
    digitalWrite(RED_LED, HIGH);
    delay(1000);
    digitalWrite(RED_LED, LOW);
    Serial.println("[400]Error:\n" + http.getString());
    http.end();
    return false;
  } else {
    Serial.printf("Error during POST: %s\n", http.errorToString(httpResponseCode).c_str());
    Serial.printf("Response Code %s\n", httpResponseCode);
    http.end();
    return false;
  }

  http.end();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("SETTING UP");

  pinMode(FLASH_IO, OUTPUT);
  pinMode(BUTTON_IO, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  connectToWiFi();

  if (!initCamera()) {
    Serial.println("Camera setup failed. Restarting...");
    ESP.restart();
  }
}

void loop() {
  if (digitalRead(BUTTON_IO) == LOW) {
    Serial.println("Button pressed...");
    if (captureImage()) {
      Serial.println("Image captured and sent successfully");
    } else {
      Serial.println("Failed to send image");
    }
    delay(5000);  // Avoid bouncing or spamming
  }
}
