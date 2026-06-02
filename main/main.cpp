#include <stdio.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "WiFi.h"
#include "Ethernet.h"
#include "WString.h"
#include "config_b.h"

// #define BLYNK_DEBUG
#define BLYNK_PRINT stdout
#define BLYNK_TOKEN BLYNK_AUTH_TOKEN
#define BLYNK_SERVER BLYNK_CONFIG_SERVER

#include "BlynkEspIDF.h"
#include <BlynkWidgets.h>

BlynkTimer timer;
WidgetTerminal terminal(V11);

// BLYNK_WRITE handlers fire from inside Blynk task with the mutex already held.
BLYNK_WRITE(V11)
{
  terminal.clear();
  terminal.flush();
  String receivedCommand = param.asString();
  receivedCommand.trim();
  receivedCommand.toLowerCase();

  if (receivedCommand == "help")
  {
    terminal.println("Available Commands:");
    terminal.println("- restart: Restart the device");
    terminal.println("- clear: Clear the terminal");
  }
  else if (receivedCommand == "restart")
  {
    terminal.println("System Restarting");
    terminal.flush();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
  }
  else if (receivedCommand == "clear")
  {
    terminal.clear();
  }
  else
  {
    terminal.println("Unknown Command. Type 'help' for assistance.");
  }

  terminal.flush();
}

BLYNK_WRITE(V1)
{
  printf("Got a value: %s\n", param.asString());
}

BLYNK_WRITE(V2)
{
  GpsParam gps(param);
  printf("Latitude: %f\n", gps.getLat());
  printf("Longitude: %f\n", gps.getLon());
  printf("Speed: %f\n", gps.getSpeed());
  printf("Altitude: %f\n", gps.getAltitude());
}

BLYNK_WRITE(V3)
{
  float x = param[0].asFloat();
  float y = param[1].asFloat();
  float z = param[2].asFloat();
  printf("Acceleration: X=%f, Y=%f, Z=%f\n", x, y, z);
}

void cb()
{
  Blynk.virtualWrite(13, "hello");
}

extern "C" void app_main(void)
{
  esp_log_level_set("wifi", ESP_LOG_ERROR);
  esp_log_level_set("wifi_init", ESP_LOG_ERROR);
  esp_log_level_set("phy_init", ESP_LOG_ERROR);
  esp_log_level_set("wifi_prov_scheme_ble", ESP_LOG_ERROR);
  esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);
  esp_log_level_set("gpio", ESP_LOG_ERROR);

  Eth.begin();

  // Spawn dedicated Blynk task. timer.run() also driven inside it.
  timer.setInterval(1000UL, cb);
  if (!BlynkTaskStart(BLYNK_TOKEN, BLYNK_SERVER, 8080, &timer,
                      /*stack=*/8192, /*prio=*/5, /*core=*/0)) {
    printf("Failed to start Blynk task\n");
    return;
  }

  // Wait until connected before issuing API calls from app_main.
  for (int i = 0; i < 200 && !Blynk.connected(); ++i) {
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  // Blynk API is internally thread-safe — call directly from any task.
  Blynk.setProperty(V1, "onLabel", "Y");
  Blynk.setProperty(V1, "offLabel", "N");

  terminal.clear();
  terminal.println("Hello from ESP32-IDF ");
  terminal.println("This is a test message.");
  terminal.flush();

  // app_main can now do other networking / sensor work without starving Blynk.
  vTaskDelete(NULL);
}
