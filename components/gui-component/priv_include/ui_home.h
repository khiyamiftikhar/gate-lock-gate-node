#ifndef UI_HOME_H
#define UI_HOME_H

#ifdef __cplusplus
extern "C" {
#endif


// API

void ui_home_init(void);

void ui_home_load_screen();

/// @brief Set the main label
/// @param string 
void ui_home_set_main_label(char* string);


/// @brief Set the ssid name
void ui_home_set_wifi_ssid_label(char* string);

void  ui_home_set_wifi_state(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif
