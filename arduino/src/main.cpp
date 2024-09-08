#include <Preferences.h>
#include <esp_task_wdt.h>
#include <OneButton.h>
#include <ESP32Servo.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <PubSubClient.h>

#define BUTTON_PIN 13
#define SERVO_PIN 14
#define NUM_LEDS 1
#define LED_PIN 38

// 全局变量定义
WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;
Servo motor;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.tencent.com", 3600 * 8, 60000); // UTC+8
OneButton button(BUTTON_PIN, true);
CRGB leds[NUM_LEDS];
unsigned long longPressStartMillis = 0;

// 设备唯一标识
const String chipID = String((uint32_t)ESP.getEfuseMac(), HEX);
const String uniqueSSID = "fish_feeder_" + chipID;

// MQTT 配置
const char *mqtt_server = "192.168.22.59";
const int mqtt_port = 1883;
const char *MQTT_TOPIC_SWITCH = "fish_feeder_a/switch";
const char *MQTT_TOPIC_BUTTON = "fish_feeder_a/button";
const char *MQTT_TOPIC_HOUR = "fish_feeder_a/feed_hour";
const char *MQTT_TOPIC_MINUTE = "fish_feeder_a/feed_minute";

// 每日任务时间设置
bool feedTaskExecuted = false;
int feedHour = 8;
int feedMinute = 30;

const int feedSpeed = 50;      // 喂食电机速度，81-99 时停止，小于81时正转，数值越小速度越快，大于99时反转，数值越大速度越快
const int feedDuration = 400;  // 喂食电机转动时间
const int feedCount = 1;       // 每次喂食次数
const bool feedReverse = true; // 每次喂食前是否反转电机

void setupWiFi()
{
  wifiManager.autoConnect(uniqueSSID.c_str());
  Serial.println("Connected to WiFi");
}

void checkWiFiConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    ESP.restart(); // 如果WiFi连接丢失，则重启设备
  }
}

void setupNTP()
{
  timeClient.begin();
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }
  Serial.println("NTP time synchronized");
}

void resetSettings()
{
  Serial.println("Resetting settings...");
  preferences.clear();
  wifiManager.resetSettings();
  ESP.restart();
}

void feed()
{
  for (int i = 0; i < feedCount; i++)
  {
    Serial.println("Feed fish...");
    if (feedReverse)
    {
      motor.write(180); // 反转
      delay(100);
    }
    motor.write(0); // 正转
    delay(feedDuration);
    motor.write(90); // 停止
    if (feedCount > 1)
    {
      delay(2000);
    }
  }
}

void loadSettings()
{
  Serial.printf("Current feed time: %02d:%02d\n", feedHour, feedMinute);
}

void doubleClick()
{
  Serial.println("Double click");
  loadSettings();
}

void longPressStart()
{
  Serial.println("Long Press start");
  longPressStartMillis = millis();
}

void longPressStop()
{
  Serial.println("Long Press Stop");
  if (millis() - longPressStartMillis >= 5000)
  {
    resetSettings();
  }
}

void dailyTask()
{
  timeClient.update();
  if (timeClient.getHours() == feedHour && timeClient.getMinutes() == feedMinute && !feedTaskExecuted)
  {
    feed();
    feedTaskExecuted = true;
  }
  else if (timeClient.getHours() != feedHour || timeClient.getMinutes() != feedMinute)
  {
    feedTaskExecuted = false; // 重置标志，以便下一天能够再次执行
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == MQTT_TOPIC_BUTTON)
  {
    if (message == "PRESS")
    {
      feed();
    }
  }
  else if (String(topic) == MQTT_TOPIC_HOUR)
  {
    feedHour = message.toInt();
    preferences.putInt("feedHour", feedHour);
    Serial.printf("feedHour updated: %02d\n", feedHour);
  }
  else if (String(topic) == MQTT_TOPIC_MINUTE)
  {
    feedMinute = message.toInt();
    preferences.putInt("feedMinute", feedMinute);
    Serial.printf("feedMinute updated: %02d\n", feedMinute);
  }
}

void setupMQTT()
{
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  mqttClient.connect(uniqueSSID.c_str());
  Serial.println("MQTT connected");
  mqttClient.subscribe(MQTT_TOPIC_SWITCH);
  mqttClient.subscribe(MQTT_TOPIC_BUTTON);
  mqttClient.subscribe(MQTT_TOPIC_HOUR);
  mqttClient.subscribe(MQTT_TOPIC_MINUTE);
  Serial.println("MQTT Subscribed");
}

void checkMQTTConnection()
{
  if (!mqttClient.connected())
  {
    Serial.println("MQTT connection lost. Attempting to reconnect...");
    ESP.restart(); // 如果MQTT连接丢失，则重启设备
  }
}

void setup()
{
  Serial.begin(115200);
  motor.attach(SERVO_PIN);
  FastLED.addLeds<WS2812B, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(30);

  esp_task_wdt_init(60 * 5, true); // 启用看门狗，超时时间60秒
  esp_task_wdt_add(NULL);

  preferences.begin("FeedPreferences", false);
  feedHour = preferences.getInt("feedHour", 8);
  feedMinute = preferences.getInt("feedMinute", 30);

  setupWiFi();
  setupMQTT();
  setupNTP();

  button.attachClick(feed);
  button.attachDoubleClick(doubleClick);
  button.attachLongPressStart(longPressStart);
  button.attachLongPressStop(longPressStop);

  loadSettings();
  leds[0] = CRGB::Green;
  FastLED.show();
  Serial.println("Setup completed");
}

void loop()
{
  button.tick();
  dailyTask();
  checkWiFiConnection();
  checkMQTTConnection();
  mqttClient.loop();
  esp_task_wdt_reset();
}
