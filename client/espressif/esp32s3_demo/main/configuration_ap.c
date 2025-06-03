#include "configuration_ap.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <lwip/ip_addr.h>

#include <stdio.h>
#include <stdlib.h>

#define TAG "CONFIGURATION_AP"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define WEB_SERVER_URL "http://192.168.4.1"

extern const char index_html_start[] asm("_binary_wifi_configuration_ap_html_start");
ap_t ap;

void start_access_point()
{
  // Get the SSID
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(ap.ssid, 32, "%s-%02X%02X%02X", "VolcRTC", mac[3], mac[4], mac[5]);
  // Initialize the TCP/IP stack
  ESP_ERROR_CHECK(esp_netif_init());

  // Create the default event loop
  esp_netif_t *netif = esp_netif_create_default_wifi_ap();

  // Set the router IP address to 192.168.4.1
  esp_netif_ip_info_t ip_info;
  IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
  IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
  IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
  esp_netif_dhcps_stop(netif);
  esp_netif_set_ip_info(netif, &ip_info);
  esp_netif_dhcps_start(netif);

  // Initialize the WiFi stack in Access Point mode
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Set the WiFi configuration
  wifi_config_t wifi_config = {};
  strcpy((char *)wifi_config.ap.ssid, ap.ssid);
  wifi_config.ap.ssid_len = strlen(ap.ssid);
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;

  // Start the WiFi Access Point
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "Access Point started with SSID %s", ap.ssid);
}

esp_err_t handler_index_html(httpd_req_t *r)
{
  httpd_resp_send(r, index_html_start, strlen(index_html_start));
  return ESP_OK;
}

esp_err_t handler_scan(httpd_req_t *r)
{
  esp_wifi_scan_start(NULL, true);
  uint16_t ap_num = 0;
  esp_wifi_scan_get_ap_num(&ap_num);
  wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(ap_num * sizeof(wifi_ap_record_t));
  esp_wifi_scan_get_ap_records(&ap_num, ap_records);

  // Send the scan results as JSON
  httpd_resp_set_type(r, "application/json");
  httpd_resp_sendstr_chunk(r, "[");
  for (int i = 0; i < ap_num; i++)
  {
    ESP_LOGI(TAG, "SSID: %s, RSSI: %d, Authmode: %d",
             (char *)ap_records[i].ssid, ap_records[i].rssi, ap_records[i].authmode);
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"rssi\":%d,\"authmode\":%d}",
             (char *)ap_records[i].ssid, ap_records[i].rssi, ap_records[i].authmode);
    httpd_resp_sendstr_chunk(r, buf);
    if (i < ap_num - 1)
    {
      httpd_resp_sendstr_chunk(r, ",");
    }
  }
  httpd_resp_sendstr_chunk(r, "]");
  httpd_resp_sendstr_chunk(r, NULL);
  free(ap_records);
  return ESP_OK;
}

void url_decode(char *url, char *decoded)
{
  int len = strlen(url);
  for (size_t i = 0; i < len; ++i)
  {
    if (url[i] == '%' && i + 2 < len)
    {
      char high = tolower(url[i + 1]);
      char low = tolower(url[i + 2]);

      if (isxdigit(high) && isxdigit(low))
      {
        unsigned char h_val = (high >= 'a') ? (high - 'a' + 10) : (high - '0');
        unsigned char l_val = (low >= 'a') ? (low - 'a' + 10) : (low - '0');
        *decoded++ = (h_val << 4) | l_val;
        i += 2;
      }
      else
      {
        *decoded++ = '\\';
        *decoded++ = url[i];
      }
    }
    else if (url[i] == '+')
    {
      *decoded = ' ';
      ++decoded;
    }
    else
    {
      *decoded = url[i];
      ++decoded;
    }
  }
  *decoded = '\0';
}

bool connect2wifi(char *ssid, char *password, EventGroupHandle_t event_group)
{
  // auto esp_netif = esp_netif_create_default_wifi_sta();

  wifi_config_t wifi_config;
  bzero(&wifi_config, sizeof(wifi_config));
  strcpy((char *)wifi_config.sta.ssid, ssid);
  strcpy((char *)wifi_config.sta.password, password);
  wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  wifi_config.sta.failure_retry_cnt = 1;

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  esp_err_t ret = esp_wifi_connect();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to connect to WiFi: %d", ret);
    return false;
  }
  ESP_LOGI(TAG, "Connecting to WiFi %s", ssid);

  // Wait for the connection to complete for 5 seconds
  EventBits_t bits = xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000000));
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "Connected to WiFi %s", ssid);
    return true;
  }
  else
  {
    ESP_LOGE(TAG, "Failed to connect to WiFi %s", ssid);
    return false;
  }
}

void restart_task(void *ctx)
{
  ESP_LOGI(TAG, "Restarting the ESP32 in 3 second");
  vTaskDelay(pdMS_TO_TICKS(3000));
  esp_restart();
}

void save(char *ssid, char *password)
{
  // Open the NVS flash
  nvs_handle_t nvs_handle;
  ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &nvs_handle));

  // Write the SSID and password to the NVS flash
  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid));
  ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "password", password));

  // Commit the changes
  ESP_ERROR_CHECK(nvs_commit(nvs_handle));

  // Close the NVS flash
  nvs_close(nvs_handle);

  ESP_LOGI(TAG, "WiFi configuration saved");
  // Use xTaskCreate to create a new task that restarts the ESP32
  xTaskCreate(restart_task, "restart_task", 4096, NULL, 5, NULL);
}

esp_err_t handler_form_submission(httpd_req_t *r)
{

  char buf[128];
  int ret = httpd_req_recv(r, buf, sizeof(buf));
  if (ret <= 0)
  {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_408(r);
    }
    return ESP_FAIL;
  }
  buf[ret] = '\0';
  ESP_LOGI(TAG, "Received form data: %s", buf);

  char decoded[128];
  url_decode(buf, decoded);
  ESP_LOGI(TAG, "Decoded form data: %s", decoded);

  // Parse the form data
  char ssid[32], password[64];
  if (sscanf(decoded, "ssid=%32[^&]&password=%64s", ssid, password) != 2)
  {
    httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid form data");
    return ESP_FAIL;
  }

  // Get this object from the user context
  if (!connect2wifi(ssid, password, ap.event_group))
  {
    char error[] = "Failed to connect to WiFi";
    char location[128];
    snprintf(location, sizeof(location), "/?error=%s&ssid=%s", error, ssid);

    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", location);
    httpd_resp_send(r, NULL, 0);
    return ESP_OK;
  }

  // Set HTML response
  httpd_resp_set_status(r, "200 OK");
  httpd_resp_set_type(r, "text/html");
  httpd_resp_send(r, "<h1>Done!</h1>", -1);

  save(ssid, password);
  return ESP_OK;
}

void start_web_server()
{
  // Start the web server
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  ESP_ERROR_CHECK(httpd_start(&(ap.server), &config));

  // Register the index.html file
  httpd_uri_t index_html = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = handler_index_html,
      .user_ctx = NULL};
  ESP_ERROR_CHECK(httpd_register_uri_handler(ap.server, &index_html));

  // Register the /scan URI
  httpd_uri_t scan = {
      .uri = "/scan",
      .method = HTTP_GET,
      .handler = handler_scan,
      .user_ctx = NULL};
  ESP_ERROR_CHECK(httpd_register_uri_handler(ap.server, &scan));

  // Register the form submission
  httpd_uri_t form_submit = {
      .uri = "/submit",
      .method = HTTP_POST,
      .handler = handler_form_submission,
      .user_ctx = NULL};
  ESP_ERROR_CHECK(httpd_register_uri_handler(ap.server, &form_submit));

  ESP_LOGI(TAG, "Web server started");
}

void handler_event_wifi_ap(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_id == WIFI_EVENT_AP_STACONNECTED)
  {
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d", MAC2STR(event->mac), event->aid);
  }
  else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
  {
    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d", MAC2STR(event->mac), event->aid);
  }
  else if (event_id == WIFI_EVENT_STA_CONNECTED)
  {
    xEventGroupSetBits(ap.event_group, WIFI_CONNECTED_BIT);
  }
  else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    xEventGroupSetBits(ap.event_group, WIFI_FAIL_BIT);
  }
}

void handler_event_ip_ap(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(ap.event_group, WIFI_CONNECTED_BIT);
  }
}

void configure_ap()
{
  ap.ssid[0] = '\0';
  ap.event_group = xEventGroupCreate();
  // Register event handlers
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &handler_event_wifi_ap,
                                                      NULL,
                                                      &(ap.instance_any_id)));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &handler_event_ip_ap,
                                                      NULL,
                                                      &(ap.instance_got_ip)));

  start_access_point(ap);
  start_web_server(ap);
  EventBits_t bits = xEventGroupWaitBits(ap.event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(60 * 1000));
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "Connected to WiFi");
    return;
  }
  else
  {
    ESP_LOGE(TAG, "Failed to connect to WiFi");
    return;
  }
}