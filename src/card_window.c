#include <pebble.h>

#include "card_window.h"
#include "defines.h"

static Window *s_window;
static char s_value[] = ZEROS ZEROS ZEROS ZEROS;
static int8_t s_offset = 0;
static TextLayer *s_textlayer_prompt;
static char s_text_card_number[] = ZEROS " " ZEROS "\n" ZEROS " " ZEROS;
static TextLayer *s_textlayer_card_number;
static GBitmap *s_bitmap_down_arrow;
static GBitmap *s_bitmap_up_arrow;
static BitmapLayer *s_bitmaplayer_down_arrow;
static BitmapLayer *s_bitmaplayer_up_arrow;

#if PBL_SDK_2
static InverterLayer *s_inverterlayer;
#else
static Layer *s_layer_selection;
#endif

#define DIGIT_W 18
#define DIGIT_H 34
#define GROUP_GAP 10
#define ROW_GAP 6
#define DIGITS_PER_ROW 8

static GRect s_digit_frames[16];

static void handle_window_unload(Window *window);
static void initialize_ui(void);
static void click_config_provider(void *context);
static void handle_single_click(ClickRecognizerRef recognizer, void *context);
static void update_text(void);
static void update_frames(void);
static void rebuild_digit_frames(GRect bounds);
static void update_selection_frame(void);
static void update_arrow_frames(GRect bounds);

static int16_t prv_digit_width_for_bounds(GRect bounds) {
  int16_t max_row_width = bounds.size.w - PBL_IF_ROUND_ELSE(bounds.size.w / 4, 18);
  int16_t max_digit_w = (max_row_width - GROUP_GAP) / DIGITS_PER_ROW;
  return (max_digit_w < DIGIT_W) ? max_digit_w : DIGIT_W;
}

static GRect prv_card_number_frame(GRect bounds) {
  int16_t digit_w = prv_digit_width_for_bounds(bounds);
  int16_t row_width = (digit_w * DIGITS_PER_ROW) + GROUP_GAP;
  int16_t start_x = (bounds.size.w - row_width) / 2;
  int16_t start_y = bounds.size.h / 2 - DIGIT_H;
  return GRect(start_x - 4, start_y - 2, row_width + 8, (DIGIT_H * 2) + ROW_GAP + 8);
}

static void rebuild_digit_frames(GRect bounds) {
  int16_t digit_w = prv_digit_width_for_bounds(bounds);
  int16_t row_width = (digit_w * DIGITS_PER_ROW) + GROUP_GAP;
  int16_t start_x = (bounds.size.w - row_width) / 2;
  int16_t start_y = bounds.size.h / 2 - DIGIT_H;

  for (int i = 0; i < 16; i++) {
    int row = i / DIGITS_PER_ROW;
    int col = i % DIGITS_PER_ROW;

    int16_t x = start_x + (col * digit_w);
    if (col >= 4) {
      x += GROUP_GAP;
    }

    int16_t y = start_y + (row * (DIGIT_H + ROW_GAP));
    s_digit_frames[i] = GRect(x, y, digit_w, DIGIT_H);
  }
}

void card_window_push(bool animated) {
  s_offset = 0;
  strcpy(s_value, ZEROS ZEROS ZEROS ZEROS);
  initialize_ui();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){
    .unload = handle_window_unload,
  });
  window_stack_push(s_window, animated);
}

void card_window_pop(bool animated) {
  window_stack_remove(s_window, animated);
}

char *card_window_get_value(void) {
  return s_value;
}

void card_window_set_value(char *value) {
  strncpy(s_value, value, 16);
  if (s_window != NULL) {
    update_text();
  }
}

#if PBL_SDK_3
static void layer_selection_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, bounds, 3);
}
#endif

static void initialize_ui(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);

  Layer *root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer);
  int16_t margin = PBL_IF_ROUND_ELSE(bounds.size.w / 8, 5);

  GRect prompt_frame = GRect(margin,
                             PBL_IF_ROUND_ELSE(bounds.size.h / 12, bounds.size.h / 30),
                             bounds.size.w - (2 * margin),
                             PBL_IF_ROUND_ELSE(70, 44));
  s_textlayer_prompt = text_layer_create(prompt_frame);
  text_layer_set_background_color(s_textlayer_prompt, GColorWhite);
  text_layer_set_text_color(s_textlayer_prompt, GColorBlack);
  text_layer_set_font(s_textlayer_prompt, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(root_layer, text_layer_get_layer(s_textlayer_prompt));

#if PBL_ROUND
  text_layer_set_text(s_textlayer_prompt, "Enter\nyour 16-digit\ncard number:");
  text_layer_set_text_alignment(s_textlayer_prompt, GTextAlignmentCenter);
#else
  text_layer_set_text(s_textlayer_prompt, "Enter your 16-digit card number:");
  text_layer_set_text_alignment(s_textlayer_prompt, GTextAlignmentLeft);
#endif

  rebuild_digit_frames(bounds);
  s_textlayer_card_number = text_layer_create(prv_card_number_frame(bounds));
  text_layer_set_background_color(s_textlayer_card_number, GColorClear);
  text_layer_set_text_color(s_textlayer_card_number, GColorBlack);
  text_layer_set_font(s_textlayer_card_number, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(s_textlayer_card_number, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_textlayer_card_number, GTextOverflowModeWordWrap);
  layer_add_child(root_layer, text_layer_get_layer(s_textlayer_card_number));

#if PBL_SDK_2
  s_inverterlayer = inverter_layer_create(GRectZero);
  layer_add_child(root_layer, inverter_layer_get_layer(s_inverterlayer));
#else
  s_layer_selection = layer_create(GRectZero);
  layer_set_update_proc(s_layer_selection, layer_selection_update_proc);
  layer_add_child(root_layer, s_layer_selection);
#endif

  s_bitmap_down_arrow = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DOWN_ARROW);
  s_bitmaplayer_down_arrow = bitmap_layer_create(GRectZero);
  bitmap_layer_set_bitmap(s_bitmaplayer_down_arrow, s_bitmap_down_arrow);
#if PBL_COLOR
  bitmap_layer_set_compositing_mode(s_bitmaplayer_down_arrow, GCompOpSet);
#endif
  layer_add_child(root_layer, bitmap_layer_get_layer(s_bitmaplayer_down_arrow));

  s_bitmap_up_arrow = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_UP_ARROW);
  s_bitmaplayer_up_arrow = bitmap_layer_create(GRectZero);
  bitmap_layer_set_bitmap(s_bitmaplayer_up_arrow, s_bitmap_up_arrow);
#if PBL_COLOR
  bitmap_layer_set_compositing_mode(s_bitmaplayer_up_arrow, GCompOpSet);
#endif
  layer_add_child(root_layer, bitmap_layer_get_layer(s_bitmaplayer_up_arrow));

  update_text();
  update_frames();
}

static void handle_window_unload(Window *window) {
  window_destroy(window);
  text_layer_destroy(s_textlayer_prompt);
  text_layer_destroy(s_textlayer_card_number);
#if PBL_SDK_2
  inverter_layer_destroy(s_inverterlayer);
#else
  layer_destroy(s_layer_selection);
#endif
  gbitmap_destroy(s_bitmap_down_arrow);
  gbitmap_destroy(s_bitmap_up_arrow);
  bitmap_layer_destroy(s_bitmaplayer_down_arrow);
  bitmap_layer_destroy(s_bitmaplayer_up_arrow);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, handle_single_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, handle_single_click);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 250, handle_single_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 250, handle_single_click);
}

static void handle_single_click(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "card_window -> handle_single_click");

  switch (click_recognizer_get_button_id(recognizer)) {
  case BUTTON_ID_BACK:
    if (s_offset > 0) {
      s_offset--;
      update_frames();
    } else {
      card_window_pop(true);
    }
    break;
  case BUTTON_ID_UP:
    s_value[s_offset] = (s_value[s_offset] == '9') ? '0' : s_value[s_offset] + 1;
    update_text();
    break;
  case BUTTON_ID_SELECT:
    if (s_offset < 15) {
      s_offset++;
      update_frames();
    } else {
      persist_write_string(STORAGE_CARD_NUMBER, s_value);
      card_window_pop(true);
    }
    break;
  case BUTTON_ID_DOWN:
    s_value[s_offset] = (s_value[s_offset] == '0') ? '9' : s_value[s_offset] - 1;
    update_text();
    break;
  default:
    break;
  }
}

static void update_text(void) {
  for (size_t i = 0; i < 4; i++) {
    memcpy(s_text_card_number + 5 * i, s_value + 4 * i, 4);
  }
  text_layer_set_text(s_textlayer_card_number, s_text_card_number);
}

static void update_selection_frame(void) {
  GRect f = s_digit_frames[s_offset];
  GRect selection_frame = GRect(f.origin.x - 2,
                                f.origin.y - 2,
                                f.size.w + 4,
                                f.size.h + 4);
#if PBL_SDK_2
  layer_set_frame(inverter_layer_get_layer(s_inverterlayer), selection_frame);
#else
  layer_set_frame(s_layer_selection, selection_frame);
  layer_mark_dirty(s_layer_selection);
#endif
}

static void prv_set_bitmap_frame_centered(BitmapLayer *bitmap_layer, GBitmap *bitmap, GPoint center) {
  GRect bitmap_bounds = gbitmap_get_bounds(bitmap);
  if (bitmap_bounds.size.w <= 0) {
    bitmap_bounds.size.w = 9;
  }
  if (bitmap_bounds.size.h <= 0) {
    bitmap_bounds.size.h = 6;
  }

  layer_set_frame(bitmap_layer_get_layer(bitmap_layer),
                  GRect(center.x - (bitmap_bounds.size.w / 2),
                        center.y - (bitmap_bounds.size.h / 2),
                        bitmap_bounds.size.w,
                        bitmap_bounds.size.h));
}

static void update_arrow_frames(GRect bounds) {
  GRect f = s_digit_frames[s_offset];
  int16_t center_x = f.origin.x + (f.size.w / 2);

  // Keep the arrow layers large enough for the full PNGs and pull them away
  // from the highlight box so they do not clip on Emery.
  prv_set_bitmap_frame_centered(s_bitmaplayer_up_arrow,
                                s_bitmap_up_arrow,
                                GPoint(center_x, f.origin.y - 8));
  prv_set_bitmap_frame_centered(s_bitmaplayer_down_arrow,
                                s_bitmap_down_arrow,
                                GPoint(center_x, f.origin.y + f.size.h + 8));
}

static void update_frames(void) {
  Layer *root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer);

  rebuild_digit_frames(bounds);
  layer_set_frame(text_layer_get_layer(s_textlayer_card_number), prv_card_number_frame(bounds));
  update_selection_frame();
  update_arrow_frames(bounds);
}
