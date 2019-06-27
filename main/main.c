#include <string.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_spiffs.h>
#include "macros.h"


static esp_err_t nvs_init() {
  printf("- Initialize NVS\n");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ERET( nvs_flash_erase() );
    ERET( nvs_flash_init() );
  }
  ERET( ret );
  return ESP_OK;
}


static esp_err_t spiffs_init() {
  printf("- Mount SPIFFS as VFS\n");
  printf("(VFS enables access though stdio)\n");
  esp_vfs_spiffs_conf_t config = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = false,
  };
  ERET( esp_vfs_spiffs_register(&config) );
  printf("- Get SPIFFS info total, used`bytes\n");
  size_t total, used;
  ERET( esp_spiffs_info(NULL, &total, &used) );
  printf("Total = %d, Used = %d\n", total, used);
  return ESP_OK;
}


static char* http_content_type(char *path) {
  char *ext = strrchr(path, '.');
  if (strcmp(ext, ".html") == 0) return "text/html";
  if (strcmp(ext, ".css") == 0) return "text/css";
  if (strcmp(ext, ".js") == 0) return "text/javascript";
  if (strcmp(ext, ".png") == 0) return "image/png";
  if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
  return "text/plain";
}


static esp_err_t httpd_static(httpd_req_t *req) {
  char buff[1024];
  size_t size;
  printf("Request URI = %s\n", req->uri);
  if(strcmp(req->uri, "/") == 0) strcpy((char*)req->uri, "/index.html");
  sprintf(buff, "/spiffs%s", req->uri);
  httpd_resp_set_type(req, http_content_type(buff));
  FILE *f = fopen(buff, "r");
  if (f == NULL) {
    printf("Cannot open file %s\n", buff);
    return ESP_FAIL;
  }
  do {
    size = fread(buff, 1, sizeof(buff), f);
    ERET( httpd_resp_send_chunk(req, buff, size) );
  } while (size == sizeof(buff));
  httpd_resp_sendstr_chunk(req, NULL);
  fclose(f);
  return ESP_OK;
}


static esp_err_t httpd_init() {
  httpd_handle_t handle = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  ERET( httpd_start(&handle, &config) );
  httpd_uri_t static_file = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = httpd_static,
    .user_ctx = NULL,
  };
  httpd_register_uri_handler(handle, &static_file);
  return ESP_OK;
}


static void on_wifi(void* arg, esp_event_base_t base, int32_t id, void* data) {
  if (id == WIFI_EVENT_AP_START) {
    ERETV( httpd_init() );
  }
  else if (id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *d = (wifi_event_ap_staconnected_t*) data;
    printf("Station " MACSTR " joined, AID = %d (event)\n", MAC2STR(d->mac), d->aid);
  } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t *d = (wifi_event_ap_stadisconnected_t*) data;
    printf("Station " MACSTR " left, AID = %d (event)\n", MAC2STR(d->mac), d->aid);
  }
}


static esp_err_t wifi_ap() {
  printf("- Initialize TCP/IP adapter\n");
  tcpip_adapter_init();
  printf("- Create default event loop\n");
  ERET( esp_event_loop_create_default() );
  printf("- Initialize WiFi with default config\n");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ERET( esp_wifi_init(&cfg) );
  printf("- Register WiFi event handler\n");
  ERET( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi, NULL) );
  printf("- Set WiFi mode as AP\n");
  ERET( esp_wifi_set_mode(WIFI_MODE_AP) );
  printf("- Set WiFi configuration\n");
  wifi_config_t wifi_config = {.ap = {
    .ssid = "charmender",
    .password = "charmender",
    .ssid_len = 0,
    .channel = 0,
    .authmode = WIFI_AUTH_WPA_PSK,
    .ssid_hidden = 0,
    .max_connection = 4,
    .beacon_interval = 100,
  }};
  ERET( esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
  printf("- Start WiFi\n");
  ERET( esp_wifi_start() );
  return ESP_OK;
}


void app_main() {
  ESP_ERROR_CHECK( nvs_init() );
  ESP_ERROR_CHECK( spiffs_init() );
  ESP_ERROR_CHECK( wifi_ap() );
}
