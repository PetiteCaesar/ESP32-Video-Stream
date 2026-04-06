#include <Arduino.h>
#include <TJpg_Decoder.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include ".config.h"
#include <TFT_eSPI.h>

uint16_t w = 0, h = 0;

uint8_t counter = 0;

uint8_t content[1] = {0};

const uint8_t bufLen = 10;
bool rec = false;
bool getSize = true;
WebSocketsServer webSocket = WebSocketsServer(80);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
TaskHandle_t Task1;
TaskHandle_t Task2;

bool display(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (y >= tft.height())
    return 0;
  tft.pushImage(x, y, w, h, bitmap); // takes like 44ms
  return 1;
}

bool Sdisplay(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (y >= tft.height())
    return 0;
   spr.pushImage(x, y, w, h, bitmap); // takes like 44ms
  return 1;
}

#define tickDelay 1

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {


  case WStype_DISCONNECTED:
    Serial.printf("[%u] Disconnected!\n", num);
    break;

  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Connection from ", num);
    Serial.println(ip.toString());
  }
  break;

  case WStype_TEXT:
    Serial.printf("[%u] Text: %s\n", num, payload);
    webSocket.sendTXT(num, payload);
    getSize = true;
    payload[length] = 0;
    spr.fillScreen(TFT_BLACK);
    if (strcmp((char *)payload, "recv") == 0)
    {
      rec = true;
    }
    else if (strcmp((char *)payload, "notrecv") == 0)
    {
      rec = false;
    }

    break;

  case WStype_BIN:
  {

    int t = millis();
    if(getSize) {
      TJpgDec.getJpgSize(&w, &h, (const uint8_t *)payload, length);
      getSize = false;
    }
    TJpgDec.drawJpg(0, 0, (const uint8_t *)payload, length);
    if (rec)
    {
      webSocket.sendBIN(num, content, 1);
    }
    int fps = 1000 / ((millis() - t) + 1);
    Serial.println(fps);
    Serial.println("fps\n");
    vTaskDelay(1);
    break;
  }
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
  default:
    break;
  }
}

void mainLoopS(void *pvParameters) {
  for(;;) {
    int t = millis();
    spr.pushSprite(int((320-w)/2), int((170-h)/2), 0, 0,w,h);
    int fps = 1000 / ((millis() - t) + 1);
    // vTaskDelay(10);
    Serial.println(fps);
    Serial.println("fps Dis\n");
    vTaskDelay(1); 
  }
}

void networking(void *pvParameters)
{
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());
  TickType_t xTimeIncrement = pdMS_TO_TICKS(tickDelay);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    webSocket.loop();
    vTaskDelayUntil(&xLastWakeTime, xTimeIncrement);
  }
}

void setup()
{
  Serial.begin(115200);
  tft.begin();

  tft.setRotation(3);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.fillScreen(TFT_RED);
  tft.setSwapBytes(true);
  spr.createSprite(302, 170);
  spr.setSwapBytes(true);
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(Sdisplay);
  // Connect to access point
  Serial.println();
  Serial.print("Connecting to network: ");
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    counter++;
    if (counter >= 60)
    { // after 30 seconds timeout - reset board
      ESP.restart();
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: ");
  Serial.println(WiFi.localIP()); // print LAN IP

  tft.fillScreen(TFT_BLACK);
  xTaskCreatePinnedToCore(mainLoopS, "Task1", 10000, NULL, 1, &Task2, 1);
  delay(500);
  xTaskCreatePinnedToCore(networking, "Task2", 10000, NULL, 1, &Task1, 0);
  delay(500);
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}
void loop(){}