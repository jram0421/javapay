#include <pebble.h>

#include "card_window.h"
#include "defines.h"
#include "pdf417.h"
#include "settings_window.h"

static Window *s_window;
static char s_value[] = ZEROS ZEROS ZEROS ZEROS;
static GBitmap *s_bitmap_app_icon;
static BitmapLayer *s_bitmaplayer_app_icon;
static TextLayer *s_textlayer_logo;
static GBitmap *s_bitmap_barcode;
static Layer *s_layer_barcode;
static char s_text_card_number[20] = ZEROS " " ZEROS " " ZEROS " " ZEROS;
static TextLayer *s_textlayer_card_number;
static bool s_has_appeared = false;

static void handle_window_appear(Window *window);
static void handle_window_unload(Window *window);
static void initialize_ui(void);
static void click_config_provider(void *context);
static void handle_single_click(ClickRecognizerRef recognizer, void *context);
static void barcode_layer_update_proc(Layer *layer, GContext *ctx);

static int16_t prv_margin_for_bounds(GRect bounds) {
  return PBL_IF_ROUND_ELSE(bounds.size.w / 10, bounds.size.w / 18);
}

static GColor prv_panel_color(void) {
#if PBL_COLOR
  return GColorLightGray;
#else
  return GColorWhite;
#endif
}

static void prv_layout_layers(void) {
  Layer *root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer);
  int16_t margin = prv_margin_for_bounds(bounds);

  const int16_t icon_w = 119;
  const int16_t icon_h = 25;

  // Leave space for the JavaPay logo, the light-grey barcode panel, and the card number.
  int16_t barcode_w = bounds.size.w - (2 * margin);
  int16_t barcode_h = bounds.size.h / 3;
  if (barcode_h > barcode_w / 2) {
    barcode_h = barcode_w / 2;
  }

  int16_t icon_y = PBL_IF_ROUND_ELSE(bounds.size.h / 7, bounds.size.h / 9);
  GRect app_icon_frame = GRect((bounds.size.w - icon_w) / 2, icon_y, icon_w, icon_h);
  layer_set_frame(bitmap_layer_get_layer(s_bitmaplayer_app_icon), app_icon_frame);
  layer_set_frame(text_layer_get_layer(s_textlayer_logo), GRect(0, icon_y - 3, bounds.size.w, 34));

  int16_t barcode_y = icon_y + icon_h + PBL_IF_ROUND_ELSE(10, 8);
  int16_t max_barcode_bottom = bounds.size.h - 34;
  if (barcode_y + barcode_h > max_barcode_bottom) {
    barcode_y = max_barcode_bottom - barcode_h;
  }

  GRect barcode_frame = GRect((bounds.size.w - barcode_w) / 2,
                              barcode_y,
                              barcode_w,
                              barcode_h);
  layer_set_frame(s_layer_barcode, barcode_frame);

  GRect card_number_frame = GRect(margin,
                                  barcode_frame.origin.y + barcode_frame.size.h + 5,
                                  bounds.size.w - (2 * margin),
                                  22);
  layer_set_frame(text_layer_get_layer(s_textlayer_card_number), card_number_frame);
}

void barcode_window_push(bool animated) {
  initialize_ui();
  s_has_appeared = false;
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){
    .appear = handle_window_appear,
    .unload = handle_window_unload,
  });
  window_stack_push(s_window, animated);
}

void barcode_window_pop(bool animated) {
  window_stack_remove(s_window, animated);
}

static void initialize_ui(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
#if PBL_SDK_2
  window_set_fullscreen(s_window, true);
#endif

  Layer *root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer);

  s_layer_barcode = layer_create(bounds);
  layer_set_update_proc(s_layer_barcode, barcode_layer_update_proc);

  s_bitmap_app_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_JAVAPAY);
  s_bitmaplayer_app_icon = bitmap_layer_create(GRectZero);
  bitmap_layer_set_bitmap(s_bitmaplayer_app_icon, s_bitmap_app_icon);
  bitmap_layer_set_compositing_mode(s_bitmaplayer_app_icon, GCompOpAssignInverted);
  bitmap_layer_set_alignment(s_bitmaplayer_app_icon, GAlignCenter);

  // Text fallback keeps the JavaPay logo visible on color and B/W builds even
  // when bitmap compositing differs across emulators.
  s_textlayer_logo = text_layer_create(GRectZero);
  text_layer_set_background_color(s_textlayer_logo, GColorClear);
  text_layer_set_text_color(s_textlayer_logo, GColorBlack);
  text_layer_set_font(s_textlayer_logo, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_textlayer_logo, GTextAlignmentCenter);
  text_layer_set_text(s_textlayer_logo, "JavaPay");

  s_textlayer_card_number = text_layer_create(GRectZero);
  text_layer_set_background_color(s_textlayer_card_number, GColorWhite);
  text_layer_set_font(s_textlayer_card_number, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_textlayer_card_number, GTextAlignmentCenter);
  text_layer_set_text_color(s_textlayer_card_number, GColorBlack);

  layer_add_child(root_layer, s_layer_barcode);
  layer_add_child(root_layer, bitmap_layer_get_layer(s_bitmaplayer_app_icon));
  layer_add_child(root_layer, text_layer_get_layer(s_textlayer_logo));
  layer_add_child(root_layer, text_layer_get_layer(s_textlayer_card_number));

  prv_layout_layers();
}

static void barcode_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Light-grey panel requested behind/around the barcode.
  graphics_context_set_fill_color(ctx, prv_panel_color());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, bounds);

  GRect barcode_rect = grect_inset(bounds, GEdgeInsets(5));
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, barcode_rect, 0, GCornerNone);

  if (s_bitmap_barcode != NULL) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_bitmap_barcode, barcode_rect);
  }
}

static void persist_read_barcode(void) {
  if (E_DOES_NOT_EXIST == persist_read_string(STORAGE_CARD_NUMBER, s_value, sizeof(s_value))) {
    return;
  }

  s_bitmap_barcode = pdf417_create_bitmap(s_value);
  layer_mark_dirty(s_layer_barcode);

  for (int i = 0; i < 4; i++) {
    memcpy(s_text_card_number + 5 * i, s_value + 4 * i, 4);
  }
  text_layer_set_text(s_textlayer_card_number, s_text_card_number);
}

static void app_timer_callback(void *data) {
  card_window_push(true);
}

static void handle_window_appear(Window *window) {
  if (!persist_exists(STORAGE_CARD_NUMBER)) {
    if (s_has_appeared) {
      window_stack_pop_all(true);
    } else {
      app_timer_register(200, app_timer_callback, NULL);
    }
  }

  if (s_bitmap_barcode != NULL) {
    gbitmap_destroy(s_bitmap_barcode), s_bitmap_barcode = NULL;
  }

  persist_read_barcode();
  s_has_appeared = true;
}

static void handle_window_unload(Window *window) {
  window_destroy(window);
  gbitmap_destroy(s_bitmap_app_icon);
  bitmap_layer_destroy(s_bitmaplayer_app_icon);
  text_layer_destroy(s_textlayer_logo);
  layer_destroy(s_layer_barcode);
  text_layer_destroy(s_textlayer_card_number);

  if (s_bitmap_barcode != NULL) {
    gbitmap_destroy(s_bitmap_barcode), s_bitmap_barcode = NULL;
  }
}

static void click_config_provider(void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "barcode_window -> click_config_provider");
  window_single_click_subscribe(BUTTON_ID_SELECT, handle_single_click);
}

static void handle_single_click(ClickRecognizerRef recognizer, void *context) {
  settings_window_push(true);
}
