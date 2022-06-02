#include <Arduino.h>
#include <CheapStepper.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include "Credentials.h"

// Define NTP Client to get time and stepper motor
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
CheapStepper stepper(2, 0, 4, 5);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
long currentTime, lastTime;
int count = 0;
char messages[50];

const char *ssid = MyNetwork;         // Defined in Credentials.h
const char *password = MyPassword;    // Defined in Credentials.h
const char *brokerUser = mqtt_user;   // Defined in Credentials.h
const char *brokerPass = mqtt_pass;   // Defined in Credentials.h
const char *broker = mqtt_broker;     // Defined in Credentials.h
const char *outTopic = mqtt_outTopic; // Defined in Credentials.h
const char *inTopic = mqtt_inTopic;   // Defined in Credentials.h
bool clockwise = true;
bool alarm_set = false;

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;
String HourMin;
String AlarmTime = "11:09";
const String TriggerPhrase = "Set Alarm - ";

void close_blinds()
{
  for (int i = 0; i < 4096; i++)
  {
    stepper.step(false);
    delay(0.5);
  }
}

void open_blinds()
{
  for (int j = 0; j < 5; j++)
  {
    for (int i = 0; i < 4096; i++)
    {
      stepper.step(true);
      delay(0.5);
    }
  }
}

void wifi_init()
{
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void clock_init()
{
  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example: GMT +1 = 3600.
  timeClient.setTimeOffset(-25200); // Vancouver is -8 = -28800 in Fall, -25200 in Spring
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message received from: ");
  Serial.println(topic);
  String s = "";
  for (unsigned int i = 0; i < length; i++)
  {
    s += (char)payload[i]; //set alarm - 11:30
  }
  Serial.println(s);
  size_t found = s.indexOf(TriggerPhrase);
  if (found != -1)
  {
    Serial.println("Great success!");
    AlarmTime = s.substring(TriggerPhrase.length());
    Serial.println("new alarm is " + AlarmTime);
    String Alarm_pub_msg = "Alarm set for " + String(AlarmTime);
    if (mqttClient.publish(outTopic, Alarm_pub_msg.c_str()))
    {
      alarm_set = true;
    }
  }
  else
  {
    if (mqttClient.publish(outTopic, "Alarm not set"))
    {
      alarm_set = false;
      Serial.println("Error - Alarm not set");
    }
  }
  if (s == "Close blinds")
  {
    if (mqttClient.publish(outTopic, "Closing blinds"))
    {
      close_blinds();
      Serial.println("Closing blinds");
    }
  }
  else if(s == "Open blinds")
  {
    if (mqttClient.publish(outTopic, "Opening blinds"))
    {
      open_blinds();
      Serial.println("Opening blinds");
    }
  }
  else
  {
    const char* msg = "Invalid argument. Valid commands are: 'Set Alarm - HH:MM', 'Open blinds', 'Close blinds'";
    if (mqttClient.publish(outTopic, msg))
    {
      Serial.println("Invalid argument");
    }
  }
}

void mqtt_init()
{
  mqttClient.setServer(broker, 1883);
  mqttClient.setCallback(callback);
}

long mqttRetry = millis();
void mqtt_loop()
{
  if (!mqttClient.connected() && ((millis() - mqttRetry) > 5000))
  {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("esp8266", brokerUser, brokerPass))
    {
      mqttClient.subscribe(inTopic);
      Serial.println("connected and subscribed");
    }
    else
    {
      Serial.println("failed, Reason = " + String(mqttClient.state() + " will try again in 5 seconds"));
      mqttRetry = millis();
    }
  }
  mqttClient.loop();
}

unsigned long timeMillis = millis();
void get_time()
{
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }
  if ((millis() - timeMillis) > 5000)
  {
    timeMillis = millis();
    formattedDate = timeClient.getFormattedDate();
    Serial.println(formattedDate);
    int splitT = formattedDate.indexOf("T"); // Extract date
    dayStamp = formattedDate.substring(0, splitT);
    Serial.print("DATE: " + String(dayStamp));
    timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1); // Extract time
    Serial.print(" HOUR: " + String(timeStamp));
    HourMin = timeStamp.substring(0, 5);
    Serial.println(" Formatted HOUR:MIN is: " + HourMin);
    // if (mqttClient.publish(outTopic, timeStamp.c_str())){
    //   Serial.println("Time published");
    // }
  }
}

void rotate_motors()
{
  for (int i = 0; i < 4096; i++)
  {
    stepper.step(clockwise);
    //stepper.stepCW();
    int currstep = stepper.getStep();
    if (currstep % 64 == 0)
    {
      Serial.print("Current position is ");
      Serial.println(currstep);
    }
    delay(0.5);
  }
  clockwise = !clockwise;
  delay(1000);
}

void check_alarm()
{
  if ((alarm_set) && HourMin == AlarmTime)
  {
    Serial.println("-----ALARM GOING OFF-----");
    rotate_motors();
  }
}

void setup()
{
  Serial.begin(115200);
  wifi_init();
  clock_init();
  mqtt_init();
  Serial.println("Starting");
}

void loop()
{
  get_time();
  check_alarm();
  mqtt_loop();
  // rotate_motors();
}