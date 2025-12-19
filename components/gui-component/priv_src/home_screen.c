
#include "stdio.h"
#include "ui_defs.h"
#include "ui_home.h"
#include "ui_worker.h"
// Screen structure (auto-generated)
#include <stdint.h>
#include <stddef.h>
#include "wifi_icons.h"

//extern void lv_obj_t; /* forward-declare type for readability; include lvgl headers in your project */

// Step 3: Define the icon with pointers to the descriptors
ui_icon_t wifi_icon = {
    .total_states = 5,
    .state_src = {
        &wifi_0_img,           // Full signal
        &wifi_1_img,           // 2 bars
        &wifi_2_img,           // 1 bar
        &wifi_3_img,           // Just dot
        &wifi_disconnected_img // Disconnected
    }
};


ui_screen_t home_screen = {
    .name = "Home_Screen",
    .child_count = 4,
    .children = {
    {
        .type = UI_CHILD_LABEL,
        .id = "label_ap_ssid",
        .lv_obj = NULL,
        .x = 50, .y = 2,
        .w = 55, .h = 9,
        .icon = NULL,
        .current_state = 0,
        .text = "",
        .initial_value = 0
    },
    {
        .type = UI_CHILD_LABEL,
        .id = "label_main",
        .lv_obj = NULL,
        .x = 8, .y = 14,
        .w = 112, .h = 13,
        .icon = NULL,
        .current_state = 0,
        .text = "",
        .initial_value = 0
    },
    {
        .type = UI_CHILD_ICON,
        .id = "icon_wifi",
        .lv_obj = NULL,
        .x = 108, .y = -1,
        .w = 14, .h = 14,
        .icon = &wifi_icon,
        .current_state = 0,
        .text = "",
        .initial_value = 0
    },
    {
        .type = UI_CHILD_LABEL,
        .id = "label_discovery",
        .lv_obj = NULL,
        .x = 2, .y = 2,
        .w = 38, .h = 8,
        .icon = NULL,
        .current_state = 0,
        .text = "",
        .initial_value = 0
    }
    },
    .lv_screen = NULL
};



static void ui_home_load_screen_cb(void* args){

    printf("\nboot screen...");
    lv_scr_load(home_screen.lv_screen);
    printf("\nloaded...\n");

     //for(int i = 0; i < boot_screen.child_count; i++){
       // ui_child_t *c = &boot_screen.children[i];
           
}


void ui_home_load_screen(){

   ui_worker_process_job(ui_home_load_screen_cb, NULL);

}



static void ui_home_set_label_cb(void* args){
    
    uint8_t child_index=(int32_t) args;
    ui_child_t *c = &home_screen.children[child_index];

    char* text = c->text;
    
    lv_label_set_text(c->lv_obj, text);  // update LVGL object

     //for(int i = 0; i < home_screen.child_count; i++){
       // ui_child_t *c = &home_screen.children[i];
}


void ui_home_set_main_label(char* string){

    int n = snprintf(home_screen.children[1].text, sizeof(home_screen.children[1].text), "%s", string);
    //Send the id of the child label
    ui_worker_process_job(ui_home_set_label_cb, (void*)1);

}


void ui_home_set_wifi_ssid_label(char* string){

    int n = snprintf(home_screen.children[0].text, sizeof(home_screen.children[0].text), "%s", string);
    //Send the id of the child label
    ui_worker_process_job(ui_home_set_label_cb, (void*)0);

}


static void  ui_home_set_wifi_state_cb(void* args)
{
    
    uint8_t state=(uint32_t) args;
    ui_child_t *c = &home_screen.children[2];
    
    if(state >= c->icon->total_states) return;

    c->current_state = state;  // update internal state

    
    lv_img_set_src(c->lv_obj, c->icon->state_src[c->current_state]);  // update LVGL object
    
}


void  ui_home_set_wifi_state(uint8_t state){

    //third child
    ui_child_t *c = &home_screen.children[2];

    c->current_state=0;
    ui_worker_process_job(ui_home_set_wifi_state_cb, (void*)2);
      
}


// ------------------------------
// Home_Screen INIT
// ------------------------------
void ui_home_init(void)
{
    home_screen.lv_screen = lv_obj_create(NULL);

    for(int i = 0; i < home_screen.child_count; i++)
    {
        ui_child_t *c = &home_screen.children[i];

        switch(c->type)
        {
            case UI_CHILD_ICON:
                c->lv_obj = lv_img_create(home_screen.lv_screen);
                lv_obj_set_pos(c->lv_obj, c->x, c->y);

                /* enforce icon boundaries */
                lv_obj_set_size(c->lv_obj, c->w, c->h);
                lv_obj_set_style_clip_corner(
                    c->lv_obj,
                    true,
                    LV_PART_MAIN | LV_STATE_DEFAULT
                );

                lv_image_set_inner_align(c->lv_obj, LV_IMAGE_ALIGN_CENTER);

                // don't set src here; do it in separate loader
                break;


            case UI_CHILD_LABEL:
                c->lv_obj = lv_label_create(home_screen.lv_screen);
                lv_obj_set_pos(c->lv_obj, c->x, c->y);
                lv_obj_set_style_pad_all(c->lv_obj, 0, 0);
                lv_obj_set_width(c->lv_obj, c->w);
                lv_label_set_long_mode(c->lv_obj, LV_LABEL_LONG_CLIP);
                // text will be set separately at runtime
                break;

            case UI_CHILD_BAR:
                // Create a bar (progress) object
                c->lv_obj = lv_bar_create(home_screen.lv_screen);
                lv_obj_set_pos(c->lv_obj, c->x, c->y);
                lv_obj_set_size(c->lv_obj, c->w, c->h);
                lv_bar_set_value(c->lv_obj, c->initial_value, LV_ANIM_OFF);
                break;

            default:
                break;
        }
    }
}
