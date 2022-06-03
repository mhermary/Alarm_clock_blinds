#include <Arduino.h>
#include "Alarms.h"

Alarm::Alarm(int day, String time, String function, bool recurring)
{
  this->day = day;
  this->time = time;
  this->function = function;
  this->recurring = recurring;
  this->has_rung = false;
}
Alarm::Alarm(int day, String time, String function)
{
  this->day = day;
  this->time = time;
  this->function = function;
  this->recurring = false;
  this->has_rung = false;
}
Alarm::Alarm(int day, String time)
{
  this->day = day;
  this->time = time;
  this->function = "open blinds";
  this->recurring = false;
  this->has_rung = false;
}
int Alarm::get_day()
{
  return day;
}
String Alarm::get_time()
{
  return time;
}
String Alarm::get_func()
{
  return function;
}
bool Alarm::is_recurring()
{
  return recurring;
}
bool Alarm::rung()
{
  return has_rung;
}
void Alarm::ringing()
{
  this->has_rung = true;
}

void Alarm::reset_rung()
{
  this->has_rung = false;
}