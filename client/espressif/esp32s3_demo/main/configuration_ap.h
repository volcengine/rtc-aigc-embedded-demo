#include <esp_http_server.h>

typedef struct
{
  httpd_handle_t server;
  EventGroupHandle_t event_group;
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  char ssid[32];
} ap_t;

void configure_ap();