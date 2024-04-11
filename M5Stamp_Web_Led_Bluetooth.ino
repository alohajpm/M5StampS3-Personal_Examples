#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <NimBLEDevice.h>

const char* ssid = "JN24";
const char* password = "D3skt0pK1ng";

#define PIN_LED 21
#define NUM_LEDS 16
#define BRIGHTNESS 50

CRGB leds[NUM_LEDS];
WebServer server(80);

NimBLEServer* pServer = nullptr;
NimBLEService* pService = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to the WiFi network");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  initBluetooth();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/setColor", HTTP_GET, handleSetColor);
  server.on("/sendMessage", HTTP_POST, handleSendMessage);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  handleBluetoothMessages();
}

void handleRoot() {
  String html = R"(
    <html>
    <head>
      <title>M5StampS3 LED Control</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          background-color: #f2f2f2;
          display: flex;
          justify-content: center;
          align-items: center;
          height: 100vh;
          margin: 0;
        }
        .container {
          background-color: white;
          padding: 30px;
          border-radius: 10px;
          box-shadow: 0 4px 8px 0 rgba(0, 0, 0, 0.2);
          width: 400px;
        }
        .color-picker {
          margin-bottom: 20px;
        }
        .color-picker label {
          display: block;
          font-weight: bold;
          margin-bottom: 5px;
        }
        .animation-select {
          width: 100%;
          padding: 10px;
          margin-bottom: 20px;
          border: 1px solid #ccc;
          border-radius: 5px;
          box-sizing: border-box;
        }
        .submit-button {
          background-color: #4CAF50;
          color: white;
          padding: 12px 20px;
          border: none;
          border-radius: 4px;
          cursor: pointer;
          width: 100%;
        }
        .submit-button:hover {
          background-color: #45a049;
        }
        h1 {
          text-align: center;
          color: #333;
          margin-bottom: 20px;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>M5StampS3 LED Control</h1>
        <form action="/setColor">
          <div class="color-picker">
            <label for="color">Choose a color:</label>
            <input type="color" id="color" name="color" value="#ffffff">
          </div>
          <select class="animation-select" name="animation">
            <option value="0">Static</option>
            <option value="1">Rainbow</option>
            <option value="2">Breath</option>
            <option value="3">Sparkle</option>
          </select>
          <input class="submit-button" type="submit" value="Apply">
        </form>
        <hr>
        <h2>Bluetooth Message</h2>
        <form action="/sendMessage" method="post">
          <textarea name="message" rows="3" placeholder="Enter a message to broadcast via Bluetooth"></textarea>
          <input class="submit-button" type="submit" value="Send">
        </form>
      </div>
    </body>
    </html>
  )";
  server.send(200, "text/html", html);
}

void handleSetColor() {
  String colorValue = server.arg("color");
  int animation = server.arg("animation").toInt();
  setColor(colorValue, animation);
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleSendMessage() {
  if (server.hasArg("message")) {
    String message = server.arg("message");
    sendBluetoothMessage(message);
    Serial.println("Bluetooth message sent: " + message);
  }
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleBluetoothMessages() {
  if (pCharacteristic && pCharacteristic->getValue() != "") {
    std::string value = pCharacteristic->getValue();
    Serial.println("Received Bluetooth message: " + String(value.c_str()));
    // You can add additional logic here to handle the received Bluetooth message
  }
}

void setColor(String colorValue, int animation) {
  CRGB color = CRGB(
    (uint8_t)(strtoul(colorValue.substring(1, 3).c_str(), 0, 16)),
    (uint8_t)(strtoul(colorValue.substring(3, 5).c_str(), 0, 16)),
    (uint8_t)(strtoul(colorValue.substring(5, 7).c_str(), 0, 16))
  );

  switch (animation) {
    case 0: // Static
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
      }
      FastLED.show();
      break;
    case 1: // Rainbow
      for (int i = 0; i < 256; i++) {
        for (int j = 0; j < NUM_LEDS; j++) {
          leds[j] = CHSV(
            (uint8_t)(triwave8(j * 256 / NUM_LEDS + i * 3)),
            255,
            255
          );
        }
        FastLED.show();
        delay(10);
      }
      break;
    case 2: // Breath
      while (true) {
        for (int i = 0; i < 256; i++) {
          for (int j = 0; j < NUM_LEDS; j++) {
            leds[j] = CRGB(
              (uint8_t)(color.r * sin(i * 3.14 / 255)),
              (uint8_t)(color.g * sin(i * 3.14 / 255)),
              (uint8_t)(color.b * sin(i * 3.14 / 255))
            );
          }
          FastLED.show();
          delay(10);
        }
        for (int i = 255; i >= 0; i--) {
          for (int j = 0; j < NUM_LEDS; j++) {
            leds[j] = CRGB(
              (uint8_t)(color.r * sin(i * 3.14 / 255)),
              (uint8_t)(color.g * sin(i * 3.14 / 255)),
              (uint8_t)(color.b * sin(i * 3.14 / 255))
            );
          }
          FastLED.show();
          delay(10);
        }
      }
      break;
    case 3: // Sparkle
      for (int i = 0; i < 100; i++) {
        int randomLED = random(0, NUM_LEDS);
        leds[randomLED] = CRGB(
          (uint8_t)(color.r * (1 - (float)i / 100)),
          (uint8_t)(color.g * (1 - (float)i / 100)),
          (uint8_t)(color.b * (1 - (float)i / 100))
        );
        FastLED.show();
        delay(20);
      }
      break;
    default:
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(0, 0, 0);
      }
      FastLED.show();
      break;
  }
}

void initBluetooth() {
  NimBLEDevice::init("M5StampS3 LED Control");

  pServer = NimBLEDevice::createServer();
  pService = pServer->createService(NimBLEUUID((uint16_t)0xABCD));

  pCharacteristic = pService->createCharacteristic(
                    NimBLEUUID((uint16_t)0x1234),
                    NIMBLE_PROPERTY::READ |
                    NIMBLE_PROPERTY::WRITE
                  );

  pService->start();
  NimBLEAdvertising* pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

void sendBluetoothMessage(String message) {
  if (pCharacteristic) {
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
  }
}