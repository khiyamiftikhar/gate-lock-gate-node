
#include "ui_defs.h"
#include "ui_home.h"
// Screen structure (auto-generated)
#include <stdint.h>
#include <stddef.h>

extern void lv_obj_t; /* forward-declare type for readability; include lvgl headers in your project */

ui_screen_t home_screen = {
    .name = "Home_Screen",
    .child_count = 4,
    .children = {
    {
        .type = UI_CHILD_LABEL,
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
        .lv_obj = NULL,
        .x = 108, .y = -1,
        .w = 14, .h = 14,
        .icon = NULL,
        .current_state = 0,
        .text = "",
        .initial_value = 0
    },
    {
        .type = UI_CHILD_LABEL,
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
                lv_image_set_inner_align(c->lv_obj, LV_IMAGE_ALIGN_TOP_MID);
                // don't set src here; do it in separate loader
                break;

            case UI_CHILD_LABEL:
                c->lv_obj = lv_label_create(home_screen.lv_screen);
                lv_obj_set_pos(c->lv_obj, c->x, c->y);
                lv_obj_set_style_pad_all(c->lv_obj, 0, 0);
                lv_obj_set_size(c->lv_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
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
