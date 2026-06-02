#include <stdio.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
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

// ---------------------------------------------------------------------------
// HTTP example task — runs on core 1, independent of Blynk (core 0).
// Fetches http://httpbin.org/get every 10 s, logs status code + body snippet.
// Shows pattern: HTTP client coexisting with Blynk, zero locking needed.
// ---------------------------------------------------------------------------
static const char *HTTP_TAG = "http_task";

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client)) {
            // Print up to 128 chars so log isn't flooded
            int print_len = evt->data_len < 128 ? evt->data_len : 128;
            ESP_LOGI(HTTP_TAG, "Body[%d B]: %.*s%s",
                     evt->data_len, print_len, (char *)evt->data,
                     evt->data_len > 128 ? "..." : "");
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(HTTP_TAG, "Request complete");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(HTTP_TAG, "Disconnected");
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void http_task(void *)
{
    // Wait for network before first request
    vTaskDelay(pdMS_TO_TICKS(3000));

    esp_http_client_config_t config = {};
    config.url            = "http://httpbin.org/get";
    config.event_handler  = http_event_handler;
    config.timeout_ms     = 8000;

    for (;;) {
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            int64_t len = esp_http_client_get_content_length(client);
            ESP_LOGI(HTTP_TAG, "HTTP GET status=%d, content_length=%lld",
                     status, len);

            // Forward status to Blynk V7 — thread-safe, no lock needed
            Blynk.virtualWrite(V7, status);
        } else {
            ESP_LOGE(HTTP_TAG, "HTTP GET failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
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

  // HTTP task on core 1 — completely independent of Blynk task on core 0.
  xTaskCreatePinnedToCore(http_task, "http", 8192, NULL, 4, NULL, 1);

  vTaskDelete(NULL);
}
