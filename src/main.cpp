#include <Arduino.h>
#include <CheapStepper.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include "Credentials.h"
#include <vector>
#include <Alarms/Alarms.cpp>

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

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;
String HourMin;
String AlarmTime;
const String TriggerPhrase = "Set Alarm - ";
int Day_of_week;
String month;
std::vector<String> Week_days{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
std::vector<Alarm> alarms;

void close_blinds()
{
  for (int i = 0; i < 20480; i++)
  {
    stepper.step(true);
    delay(0.5);
  }
}

void open_blinds()
{
  for (int i = 0; i < 20480; i++)
  {
    stepper.step(false);
    delay(0.5);
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
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void checkDST()
{
  String dayNum = formattedDate.substring(8, 10);
  int day = timeClient.getDay();
  if (month.toInt() == 11 && dayNum.toInt() < 8 && day == 0)
  {
    // First sunday of November at 2AM
    timeClient.setTimeOffset(-25200);
  }
  else if (month.toInt() == 3 && (dayNum.toInt() > 7 && dayNum.toInt() < 15) && day == 0)
  {
    // Second sunday of March at 2AM
    timeClient.setTimeOffset(-28800);
  }
  //The exact time is not needed here since these will happen while im asleep.
}

void clock_init()
{
  timeClient.begin(); // Initialize a NTPClient to get time
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
    s += (char)payload[i];
  }
  //set alarm - 11:30/4
  //set alarm - HH:MM/D
  Serial.println(s);
  String s_str = String(s);   //string to convert all to lower for handling more inputs
  s_str.toLowerCase();
  size_t found = s.indexOf(TriggerPhrase);
    if (found != -1)
    {
      Serial.println("Great success!");
      int pos = s.indexOf("/");
      AlarmTime = s.substring(TriggerPhrase.length(), pos); // Parse into HHMM
      String DoW = s.substring(pos + 1);    // Parse into single character (which should be an int)
      int Alarm_DoW;  //Alarm Day of Week int (0 = Sunday, 6 = Saturday)
      if(isDigit(DoW[0]))
      {
        Alarm_DoW = DoW.toInt();
      }
      else{
        Alarm_DoW = 0;
      }
      Serial.println("new alarm is " + AlarmTime + " on " + Week_days.at(Alarm_DoW));
      String Alarm_pub_msg = "Alarm set for " + String(AlarmTime) + " on " + Week_days.at(Alarm_DoW);
      if (mqttClient.publish(outTopic, Alarm_pub_msg.c_str()))
      {
        Alarm A(Alarm_DoW, AlarmTime);
        alarms.push_back(A);
        Serial.println("Added Alarm for " + A.get_time() + " on " + A.get_day());
      }
    }
    else if (s_str == "close blinds")
    {
      if (mqttClient.publish(outTopic, "Closing blinds"))
      {
        close_blinds();
        Serial.println("Closing blinds");
      }
    }
    else if(s_str == "open blinds")
    {
      if (mqttClient.publish(outTopic, "Opening blinds"))
      {
        open_blinds();
        Serial.println("Opening blinds");
      }
    }
    else if (s_str == "delete alarms" || s_str == "clear alarms")
    {
      alarms.~vector(); // clears alarm vector
      Serial.println("Deleted all alarms");
    }
    else
    {
      const char* msg = "Invalid argument. Valid commands are: 'Set Alarm - HH:MM/D', 'Delete Alarms', 'Open blinds', 'Close blinds'";
      if (mqttClient.publish(outTopic, msg))
      {
        Serial.println(msg);
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
    formattedDate = timeClient.getFormattedDate();  // Comes in as YYYY-MM-DDTHH:MM:SSZ
    Serial.println(formattedDate);
    int splitT = formattedDate.indexOf("T"); 
    dayStamp = formattedDate.substring(0, splitT);
    Serial.print("DATE: " + String(dayStamp));      // Extract date to YYYY-MM-DD
    timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1); // Extract time to HH:MM:SS
    Serial.print(" HOUR: " + String(timeStamp));
    HourMin = timeStamp.substring(0, 5);            // Extract to HH:MM
    Serial.print(" Formatted HOUR:MIN is: " + HourMin);
    Day_of_week = timeClient.getDay();
    Serial.println(" Today is " + Week_days.at(Day_of_week));
    month = formattedDate.substring(5, 7);
  }
}

void check_alarm()
{
  for (unsigned int i = 0; i < alarms.size(); i++)
  {
    if ((alarms.at(i).get_day() == Day_of_week) && (alarms.at(i).get_time() == HourMin))
    {
      if (alarms.at(i).get_func() == "open blinds")
      {
        Serial.println("Alarm going off - open blinds");
        open_blinds();
      }
      alarms.at(i).ringing();
    }
  }
  auto iter = alarms.begin();
  while (iter != alarms.end())
  {
    if ((*iter).rung() && !(*iter).is_recurring())
    {
      iter = alarms.erase(iter);  //Deletes alarm obj from vector. Returns next valid iterator so else statement that follows is valid
    }
    else if (iter->rung() && iter->is_recurring())
    {
      iter->reset_rung(); // resets has_rung to false
    }
    else
    {
      iter++;
    }
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
}