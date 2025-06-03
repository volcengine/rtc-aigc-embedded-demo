// Copy and modify from https://github.com/78/esp-wifi-connect

#include "network.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "configuration_ap.h"

#define TAG "NETWORK"

#define WIFI_EVENT_CONNECTED BIT0
#define WIFI_EVENT_FAILED BIT1
#define MAX_RECONNECT_COUNT 5

static network_t network;

int8_t get_rssi()
{
  // Get station info
  wifi_ap_record_t ap_info;
  ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
  return ap_info.rssi;
}

uint8_t get_channel()
{
  // Get station info
  wifi_ap_record_t ap_info;
  ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
  return ap_info.primary;
}

void handler_event_wifi_st(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    xEventGroupClearBits(network.event_group, WIFI_EVENT_CONNECTED);
    if (network.reconnect_count < MAX_RECONNECT_COUNT)
    {
      esp_wifi_connect();
      network.reconnect_count++;
      ESP_LOGI(TAG, "Reconnecting WiFi (attempt %d)", network.reconnect_count);
    }
    else
    {
      xEventGroupSetBits(network.event_group, WIFI_EVENT_FAILED);
      ESP_LOGI(TAG, "WiFi connection failed");
    }
  }
}

void handler_event_ip_st(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

  esp_ip4addr_ntoa(&event->ip_info.ip, network.ip_address, sizeof(network.ip_address));
  ESP_LOGI(TAG, "Got IP: %s", network.ip_address);
  xEventGroupSetBits(network.event_group, WIFI_EVENT_CONNECTED);
}

bool is_connected()
{
  return xEventGroupGetBits(network.event_group) & WIFI_EVENT_CONNECTED;
}

bool configure_network()
{

  // Create the event group
  network.ssid[0] = '\0';
  network.password[0] = '\0';
  network.reconnect_count = 0;
  network.ip_address[0] = '\0';
  network.event_group = xEventGroupCreate();

  // Get ssid and password from NVS
  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open("wifi", NVS_READONLY, &nvs_handle);
  if (ret == ESP_OK)
  {
    size_t length = sizeof(network.ssid);
    nvs_get_str(nvs_handle, "ssid", network.ssid, &length);
    length = sizeof(network.password);
    nvs_get_str(nvs_handle, "password", network.password, &length);
    nvs_close(nvs_handle);
  }
  // Try to connect to WiFi, if failed, launch the WiFi configuration AP
  if (network.ssid[0] != '\0')
  {
    ESP_LOGI(TAG, "Connecting to WiFi %s", network.ssid);
    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &handler_event_wifi_st,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &handler_event_ip_st,
                                                        NULL,
                                                        &instance_got_ip));

    // Create the default event loop
    esp_netif_create_default_wifi_sta();

    // Initialize the WiFi stack in station mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_LOGI(TAG, "Connecting to WiFi ssid=%s password=%s", network.ssid, network.password);
    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, network.ssid);
    strcpy((char *)wifi_config.sta.password, network.password);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start the WiFi stack
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for the WiFi stack to start
    EventBits_t bits = xEventGroupWaitBits(network.event_group, WIFI_EVENT_CONNECTED | WIFI_EVENT_FAILED, pdFALSE, pdFALSE, pdMS_TO_TICKS(30 * 1000));
    if (bits & WIFI_EVENT_FAILED)
    {
      ESP_LOGE(TAG, "WifiStation failed");
      // Reset the WiFi stack
      ESP_ERROR_CHECK(esp_wifi_stop());
      ESP_ERROR_CHECK(esp_wifi_deinit());

      // 取消注册事件处理程序
      ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
      ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
      // nvs_flash_erase();
      // return false;
    } else {
        ESP_LOGI(TAG, "Connected to %s rssi=%d channel=%d", network.ssid, get_rssi(), get_channel());
    }
  }

  if (!is_connected(network))
  {
    configure_ap();
    ESP_LOGI(TAG, "WiFi not connected, starting AP");
    return false;
  }

  return true;
}