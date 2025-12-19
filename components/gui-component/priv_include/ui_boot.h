#ifndef UI_BOOT_H
#define UI_BOOT_H

#ifdef __cplusplus
extern "C" {
#endif


// API
void ui_boot_init(void);
void ui_boot_load_screen();

void ui_boot_set_main_label(char* string);
void ui_boot_set_channel_info_label(char* string);
void ui_boot_set_discovery_info_label(char* string);

#ifdef __cplusplus
}
#endif

#endif
