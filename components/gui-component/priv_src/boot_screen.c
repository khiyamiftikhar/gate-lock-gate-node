
#include "ui_defs.h"
#include "ui_boot.h"
#include "ui_worker.h"
// Screen structure (auto-generated)
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

//extern void lv_obj_t; /* forward-declare type for readability; include lvgl headers in your project */

ui_screen_t boot_screen = {
    .name = "Boot_Screen",
    .child_count = 3,
    .children = {
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
        .type = UI_CHILD_LABEL,
        .id = "label_channel_no",
        .lv_obj = NULL,
        .x = 55, .y = 2,
        .w = 55, .h = 9,
        .icon = NULL,
        .current_state = 0,
        .text = "",
        .initial_value = 0
    },
    {
        .type = UI_CHILD_LABEL,
        .id = "label_discovery",
        .lv_obj = NULL,
        .x = 3, .y = 2,
        .w = 38, .h = 10,
        .icon = NULL,
        .current_state = 0,
        .text = "",
        .initial_value = 0
    }
    },
    .lv_screen = NULL
};


static void ui_boot_load_screen_cb(void* args){

    printf("\nboot screen...");
    lv_scr_load(boot_screen.lv_screen);
    printf("\nloaded...\n");

     //for(int i = 0; i < boot_screen.child_count; i++){
       // ui_child_t *c = &boot_screen.children[i];
           
}


void ui_boot_load_screen(){

   ui_worker_process_job(ui_boot_load_screen_cb, NULL);

}



static void ui_boot_set_label_cb(void* args){
    
    uint8_t child_index=(int32_t) args;
    ui_child_t *c = &boot_screen.children[child_index];

    char* text = c->text;
    
    lv_label_set_text(c->lv_obj, text);  // update LVGL object

     //for(int i = 0; i < boot_screen.child_count; i++){
       // ui_child_t *c = &boot_screen.children[i];
}


void ui_boot_set_main_label(char* string){

    int n = snprintf(boot_screen.children[0].text, sizeof(boot_screen.children[0].text), "%s", string);
    //Send the id of the child label
    ui_worker_process_job(ui_boot_set_label_cb, (void*)0);

}


void ui_boot_set_channel_info_label(char* string){

    int n = snprintf(boot_screen.children[1].text, sizeof(boot_screen.children[1].text), "%s", string);
    //Send the id of the child label
    ui_worker_process_job(ui_boot_set_label_cb, (void*)1);

}

void ui_boot_set_discovery_info_label(char* string){

    int n = snprintf(boot_screen.children[2].text, sizeof(boot_screen.children[2].text), "%s", string);
    //Send the id of the child label
    ui_worker_process_job(ui_boot_set_label_cb, (void*)2);

}


// ------------------------------
// Boot_Screen INIT
// ------------------------------
void ui_boot_init(void)
{
    boot_screen.lv_screen = lv_obj_create(NULL);

    for(int i = 0; i < boot_screen.child_count; i++)
    {
        ui_child_t *c = &boot_screen.children[i];

        switch(c->type)
        {
            case UI_CHILD_ICON:
                c->lv_obj = lv_img_create(boot_screen.lv_screen);
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
                c->lv_obj = lv_label_create(boot_screen.lv_screen);
                lv_obj_set_pos(c->lv_obj, c->x, c->y);
                lv_obj_set_style_pad_all(c->lv_obj, 0, 0);
                lv_obj_set_width(c->lv_obj, c->w);
                lv_label_set_long_mode(c->lv_obj, LV_LABEL_LONG_CLIP);
                // text will be set separately at runtime
                break;

            case UI_CHILD_BAR:
                // Create a bar (progress) object
                c->lv_obj = lv_bar_create(boot_screen.lv_screen);
                lv_obj_set_pos(c->lv_obj, c->x, c->y);
                lv_obj_set_size(c->lv_obj, c->w, c->h);
                lv_bar_set_value(c->lv_obj, c->initial_value, LV_ANIM_OFF);
                break;

            default:
                break;
        }
    }
}
