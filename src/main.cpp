
/*    ___           ___                       ___                       ___
     /  /\         /__/\          ___        /  /\        ___          /__/|
    /  /::\       _\_ \:\        /  /\      /  /::\      /  /\        |  |:|
   /  /:/\:\     /__/\ \:\      /  /:/     /  /:/\:\    /  /:/        |  |:|
  /  /:/~/::\   _\_ \:\ \:\    /  /:/     /  /:/~/:/   /__/::\      __|__|:|
 /__/:/ /:/\:\ /__/\ \:\ \:\  /  /::\    /__/:/ /:/___ \__\/\:\__  /__/::::\____
 \  \:\/:/__\/ \  \:\ \:\/:/ /__/:/\:\   \  \:\/:::::/    \  \:\/\    ~\~~\::::/
  \  \::/       \  \:\ \::/  \__\/  \:\   \  \::/~~~~      \__\::/     |~~|:|~~
   \  \:\        \  \:\/:/        \  \:\   \  \:\          /__/:/      |  |:|
    \  \:\        \  \::/          \__\/    \  \:\         \__\/       |  |:|
     \__\/         \__\/                     \__\/                     |__|/

 ***************************************************************************
 *                                                                         *
 *   AWTRIX 3, a custom firmware for the Ulanzi clock                  *
 *                                                                         *
 *   Copyright (C) 2024  Stephan Mühl aka Blueforcer                       *
 *                                                                         *
 *   This work is licensed under a                                         *
 *   Creative Commons Attribution-NonCommercial-ShareAlike                 *
 *   4.0 International License.                                            *
 *                                                                         *
 *   More information:                                                     *
 *   https://github.com/Blueforcer/awtrix3/blob/main/LICENSE.md       *
 *                                                                         *
 *   This firmware is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
 *                                                                         *
 ***************************************************************************/


#include <Arduino.h>
#include "DisplayManager.h"
#include "PeripheryManager.h"
#include "MQTTManager.h"
#include "ServerManager.h"
#include "Globals.h"
#include "UpdateManager.h"
#include "timer.h"

TaskHandle_t taskHandle;
volatile bool StopTask = false;
bool stopBoot;

void BootAnimation(void *parameter)
{
  const TickType_t xDelay = 1 / portTICK_PERIOD_MS;
  while (true)
  {
    if (StopTask)
    {
      break;
    }
    DisplayManager.HSVtext(4, 6, "AWTRIX", true, 0);
    vTaskDelay(xDelay);
  }
  vTaskDelete(NULL);
}

void setup()
{
  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);
  delay(2000);
  Serial.begin(115200);
  loadSettings();
  PeripheryManager.setup();
  ServerManager.loadSettings();
  DisplayManager.setup();
  DisplayManager.HSVtext(9, 6, VERSION, true, 0);
  delay(500);
  xTaskCreatePinnedToCore(BootAnimation, "Task", 10000, NULL, 1, &taskHandle, 0);
  ServerManager.setup();
  if (ServerManager.isConnected)
  {
    // timer_init();
    DisplayManager.loadNativeApps();
    DisplayManager.loadCustomApps();
    UpdateManager.setup();
    DisplayManager.startArtnet();
    StopTask = true;
    float x = 4;
    String textForDisplay = "AWTRIX   " + ServerManager.myIP.toString();

    if (WEB_PORT != 80)
    {
      textForDisplay += ":" + String(WEB_PORT);
    }

    int textLength = textForDisplay.length() * 4;
    while (x >= -textLength)
    {
      DisplayManager.HSVtext(x, 6, textForDisplay.c_str(), true, 0);
      x -= 0.18;
    }

    
      if (MQTT_HOST != "")
      {
        DisplayManager.HSVtext(4, 6, "MQTT...", true, 0);
        MQTTManager.setup();
        MQTTManager.tick();
      }
    
  }
  else
  {
    AP_MODE = true;
    StopTask = true;
  }
  delay(200);
  DisplayManager.setBrightness(BRIGHTNESS);
}

// ---------- 夜间模式（定时熄屏） ----------
// 每个主循环检查一次当前时间是否处于夜间时段，仅在跨边界时切换屏幕电源，
// 不打断时段内的手动开/屏操作（下次跨边界时自动恢复调度）。
void nightModeTick()
{
  if (!NIGHT_ACTIVE)
  {
    if (NIGHT_STATE)
    {
      NIGHT_STATE = false;
      DisplayManager.setPower(true);
    }
    return;
  }

  struct tm *t = timer_localtime();
  if (t->tm_year < 120) // 时间尚未同步（仍停留在 1970），不处理
    return;

  int minuteOfDay = t->tm_hour * 60 + t->tm_min;
  bool inNight;
  if (NIGHT_START == NIGHT_END)
    inNight = false; // 起止时间相同视为不启用
  else if (NIGHT_START < NIGHT_END)
    inNight = (minuteOfDay >= NIGHT_START && minuteOfDay < NIGHT_END);
  else
    inNight = (minuteOfDay >= NIGHT_START || minuteOfDay < NIGHT_END); // 跨午夜

  if (inNight != NIGHT_STATE)
  {
    NIGHT_STATE = inNight;
    DisplayManager.setPower(!inNight);
    if (DEBUG_MODE)
    {
      DEBUG_PRINTF("NightMode: %s (%02d:%02d)", inNight ? "OFF" : "ON", t->tm_hour, t->tm_min);
    }
  }
}

void loop()
{
  timer_tick();
  nightModeTick();
  ServerManager.tick();
  DisplayManager.tick();
  PeripheryManager.tick();
  if (ServerManager.isConnected)
  {
    MQTTManager.tick();
  }
}
