#include <Arduino.h>

class Alarm
{
  private:
    int day;
    String time;
    String function;
    bool recurring;
    bool has_rung;

  public:
    Alarm(int day, String time, String function, bool recurring);
    Alarm(int day, String time, String function);
    Alarm(int day, String time);
    int get_day();
    String get_time();
    String get_func();
    bool is_recurring();
    bool rung();
    void ringing();
    void reset_rung();
};