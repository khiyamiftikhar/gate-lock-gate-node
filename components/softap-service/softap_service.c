
/*
 * softap_with_exceptions.c
 *
 * SoftAP init that posts exceptions to an exception event loop via
 * system_context_t. Designed to co-exist with ESP-NOW (APSTA mode).
 *
 * Usage:
 *   - Create system_context_t in app_main()
 *   - Initialize softap_netifs.main_loop and softap_netifs.exception_loop before calling
 *   - Call wifi_init_softap_with_ctx(&sys_ctx);
 */

#include <string.h>
#include <assert.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "softap_event_adapter.h"
#include "softap_service.h"
#include "system_context.h"
/* -------------------- Configuration -------------------- */
#define WIFI_AP_SSID     "ESP32_Config"
#define WIFI_AP_PASS     "esp32pass"   // set empty for open AP
#define WIFI_AP_CHANNEL  1
#define WIFI_AP_MAX_CONN 3
//#define AP_STATIC_IP     (192),(168),(4),(1)
//#define AP_NETMASK       255,255,255,0
/* ------------------------------------------------------- */

static const char *TAG = "softap";

/* ---------- Local storage for the created netifs (returned to caller via ref) ---------- */
typedef struct {
    esp_netif_t *sta_netif;
    esp_netif_t *ap_netif;
} softap_netifs_t;


static softap_netifs_t softap_netifs={0};

//This is the default event loop handler, and not the system_context event loops
/* ---------- Wi-Fi event handler (logs connects/disconnects) ---------- */
static void softap_wifi_event_handler(void *arg,
                                      esp_event_base_t event_base,
                                      int32_t event_id,
                                      void *event_data)
{
    (void) arg;
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t *ev = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Station " MACSTR " joined (AID=%d)", MAC2STR(ev->mac), ev->aid);
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t *ev = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Station " MACSTR " left (AID=%d)", MAC2STR(ev->mac), ev->aid);
        }
    }
}




esp_err_t set_wifi_channel(uint8_t channel){


    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_set_channel failed: %s", esp_err_to_name(err));
            softap_post_exception(SOFTAP_EXCP_AP_CHANNEL_FAIL,(void*)&err,sizeof(esp_err_t));
            return ESP_FAIL;
        }
    return ESP_OK;
}

/*
 * Initialize SoftAP while preserving STA (for ESP-NOW).
 *
 * - ctx must be non-NULL and softap_netifs.main_loop and softap_netifs.exception_loop should be created already.
 * - out_netifs may be NULL; if non-NULL it will be populated with created netif handles (useful for ip queries).
 *
 * Returns ESP_OK on success; on recoverable failures returns error *and* posts an exception event.
 */
esp_err_t wifi_init_softap()
{
    
    esp_err_t err=0;
    //assert(ctx != NULL);

    /* ---------- CRITICAL: Base initialization ---------- */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* ---------- CRITICAL: Create Wi-Fi driver ---------- */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* ---------- CRITICAL: Create network interfaces ---------- */
    softap_netifs.sta_netif = esp_netif_create_default_wifi_sta();
    softap_netifs.ap_netif  = esp_netif_create_default_wifi_ap();

    assert(softap_netifs.sta_netif && softap_netifs.ap_netif);  // cannot continue without both


    //Saving to the system context data structure
    set_ap_netif_obj(softap_netifs.sta_netif);
    set_station_netif_obj(softap_netifs.sta_netif);
    

    /* ---------- CRITICAL: Ensure AP+STA mode ---------- */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    /* ---------- CONFIGURE: SoftAP parameters ---------- */
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = (uint8_t)strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    if (strlen(WIFI_AP_PASS) == 0)
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

    err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(AP) failed: %s", esp_err_to_name(err));
        softap_post_exception(SOFTAP_EXCP_CONFIG_FAIL, (void*)&err,0);
        return err;  // AP cannot continue
    }

    /* ---------- CRITICAL: Start Wi-Fi ---------- */
    ESP_ERROR_CHECK(esp_wifi_start());

    /* ---------- OPTIONAL: Set channel for ESP-NOW ---------- */
    
    set_wifi_channel(1);        //just to start with, channel 1
    /* ---------- OPTIONAL: Assign static IP (recoverable) ---------- */
    esp_netif_ip_info_t ip_info;

    // Parse static IP
    if (!inet_aton(CONFIG_AP_STATIC_IP, &ip_info.ip)) {
        ESP_LOGE(TAG, "Invalid AP_STATIC_IP format in Kconfig: %s", CONFIG_AP_STATIC_IP);
        softap_post_exception(SOFTAP_EXCP_IPCFG_FAIL, NULL, 0);
        return ESP_FAIL;   // <--- stop here
    }

    // Parse gateway
    if (!inet_aton(CONFIG_AP_GATEWAY, &ip_info.gw)) {
        ESP_LOGE(TAG, "Invalid AP_GATEWAY format in Kconfig: %s", CONFIG_AP_GATEWAY);
        softap_post_exception(SOFTAP_EXCP_IPCFG_FAIL, NULL, 0);
        return ESP_FAIL;   // <--- stop here
    }

    // Parse netmask
    if (!inet_aton(CONFIG_AP_NETMASK, &ip_info.netmask)) {
        ESP_LOGE(TAG, "Invalid AP_NETMASK format in Kconfig: %s", CONFIG_AP_NETMASK);
        softap_post_exception(SOFTAP_EXCP_IPCFG_FAIL, NULL, 0);
        return ESP_FAIL;   // <--- stop here
    }

    // Now apply settings safely
    //esp_err_t err;

    err = esp_netif_dhcps_stop(softap_netifs.ap_netif);
    if (err != ESP_OK) {
        softap_post_exception(SOFTAP_EXCP_DNS_FAIL, NULL, 0);
        return err;  // <--- stop
    }

    err = esp_netif_set_ip_info(softap_netifs.ap_netif, &ip_info);
    if (err != ESP_OK) {
        softap_post_exception(SOFTAP_EXCP_IPCFG_FAIL, NULL, 0);
        return err;  // <--- stop
    }

    err = esp_netif_dhcps_start(softap_netifs.ap_netif);
    if (err != ESP_OK) {
        softap_post_exception(SOFTAP_EXCP_DHCP_FAIL, NULL, 0);
        return err;  // <--- stop
    }

    /* ---------- LOGGING ---------- */
    uint8_t mac_ap[6], mac_sta[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac_ap);
    esp_wifi_get_mac(WIFI_IF_STA, mac_sta);

    ESP_LOGI(TAG, "SoftAP started: SSID=%s  Channel=%d", WIFI_AP_SSID, WIFI_AP_CHANNEL);
    ESP_LOGI(TAG, "AP MAC:  " MACSTR, MAC2STR(mac_ap));
    ESP_LOGI(TAG, "STA MAC: " MACSTR, MAC2STR(mac_sta));

    return ESP_OK;

}

/* ---------------- Example app_main usage (illustrative) ----------------
void app_main(void)
{
    system_context_t sys_ctx = { 0 };
    esp_event_loop_args_t loop_args = {
        .queue_size = 16,
        .task_name = "main_loop",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY
    };
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &sys_ctx.main_loop));

    esp_event_loop_args_t exc_args = {
        .queue_size = 8,
        .task_name = "exc_loop",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 3072,
        .task_core_id = tskNO_AFFINITY
    };
    ESP_ERROR_CHECK(esp_event_loop_create(&exc_args, &sys_ctx.exception_loop));

    softap_netifs_t netifs;
    esp_err_t r = wifi_init_softap_with_ctx(&sys_ctx, &netifs);
    if (r != ESP_OK) {
        // The exception loop will receive details; handle or schedule retry here
        ESP_LOGW("MAIN", "SoftAP init returned %d; continuing with ESPNOW only", r);
    }

    // Continue to init ESPNOW, HTTP server, OTA components, etc.
}
------------------------------------------------------------------------ */

