#pragma once
#include <esp_http_server.h>

typedef struct
{
  EventGroupHandle_t event_group;

  char ssid[32];
  char password[64];
  int reconnect_count;
  char ip_address[16];
} network_t;

bool configure_network();