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
static char s_selection_value[] = "0";
static Layer *s_layer_selection;
static TextLayer *s_textlayer_selection_value;
#endif

static void handle_window_unload(Window *window);
static void initialize_ui(void);
static void click_config_provider(void *context);
static void handle_single_click(ClickRecognizerRef recognizer, void *context);
static void update_text(void);
static void update_frames(void);

static GRect prv_digit_grid_rect(void) {
  Layer *root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer);

  int16_t margin = PBL_IF_ROUND_ELSE(bounds.size.w / 5, bounds.size.w / 7);
  int16_t width = bounds.size.w - (2 * margin);
  int16_t height = 60;
  int16_t y = PBL_IF_ROUND_ELSE(bounds.size.h / 2 - 8, bounds.size.h / 2 - 26);

  return GRect(margin, y, width, height);
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
  graphics_context_set_fill_color(ctx, GColorWindsorTan);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 2, GCornersAll);
}
#endif

static void initialize_ui(void) {
  s_window = window_create();
  window_set_background_color(s_window, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));

  Layer *root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer);
  int16_t margin = PBL_IF_ROUND_ELSE(bounds.size.w / 8, 5);

  GRect prompt_frame = GRect(margin,
                             PBL_IF_ROUND_ELSE(bounds.size.h / 12, bounds.size.h / 30),
                             bounds.size.w - (2 * margin),
                             PBL_IF_ROUND_ELSE(70, 44));
  s_textlayer_prompt = text_layer_create(prompt_frame);
  text_layer_set_background_color(s_textlayer_prompt, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_text_color(s_textlayer_prompt, PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorWhite));
  text_layer_set_font(s_textlayer_prompt, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(root_layer, text_layer_get_layer(s_textlayer_prompt));

#if PBL_ROUND
  text_layer_set_text(s_textlayer_prompt, "Enter\nyour 16-digit\ncard number:");
  text_layer_set_text_alignment(s_textlayer_prompt, GTextAlignmentCenter);
#else
  text_layer_set_text(s_textlayer_prompt, "Enter your 16-digit card number:");
  text_layer_set_text_alignment(s_textlayer_prompt, GTextAlignmentLeft);
#endif

  s_textlayer_card_number = text_layer_create(prv_digit_grid_rect());
  text_layer_set_background_color(s_textlayer_card_number, PBL_IF_COLOR_ELSE(GColorClear, GColorBlack));
  text_layer_set_text_color(s_textlayer_card_number, PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorWhite));
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

  // Draw the selected digit inside the highlight. This layer covers the
  // original tan digit only when the highlight is exactly over it, avoiding
  // the doubled/offset Emery artifact from the old fixed-position math.
  s_textlayer_selection_value = text_layer_create(GRect(0, -6, 18, 30));
  text_layer_set_background_color(s_textlayer_selection_value, GColorClear);
  text_layer_set_text_color(s_textlayer_selection_value, GColorWhite);
  text_layer_set_font(s_textlayer_selection_value, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(s_textlayer_selection_value, GTextAlignmentCenter);
  text_layer_set_text(s_textlayer_selection_value, s_selection_value);
  layer_add_child(s_layer_selection, text_layer_get_layer(s_textlayer_selection_value));

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
  text_layer_destroy(s_textlayer_selection_value);
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
#if PBL_SDK_3
  s_selection_value[0] = s_value[s_offset];
  text_layer_set_text(s_textlayer_selection_value, s_selection_value);
#endif
  text_layer_set_text(s_textlayer_card_number, s_text_card_number);
}

#if PBL_SDK_3
static int16_t prv_text_width(const char *text) {
  GSize size = graphics_text_layout_get_content_size(text,
                                                      fonts_get_system_font(FONT_KEY_GOTHIC_28),
                                                      GRect(0, 0, 200, 40),
                                                      GTextOverflowModeTrailingEllipsis,
                                                      GTextAlignmentLeft);
  return size.w;
}
#endif

static void update_frames(void) {
  GRect grid = prv_digit_grid_rect();

  int16_t row = s_offset >= 8 ? 1 : 0;
  int16_t col = s_offset % 8;
  int16_t row_height = 28;

#if PBL_SDK_3
  // Emery's wider screen exposed the old hardcoded character-step math.
  // Measure the actual rendered text line and prefix so the highlight lands
  // on the selected digit for every platform/font width.
  const char *line = "0000 0000";
  char prefix[10] = {0};
  int16_t digit_index = (col < 4) ? col : col + 1; // account for the space
  strncpy(prefix, line, digit_index);

  int16_t line_width = prv_text_width(line);
  int16_t prefix_width = prv_text_width(prefix);
  int16_t digit_width = prv_text_width("0");
  int16_t line_start_x = grid.origin.x + (grid.size.w - line_width) / 2;

  GRect selection_frame = GRect(line_start_x + prefix_width - 2,
                                grid.origin.y + 6 + row * row_height,
                                digit_width + 4,
                                22);
#else
  int16_t char_step = grid.size.w / 9;
  if (char_step < 12) {
    char_step = 12;
  }
  int16_t start_x = grid.origin.x + (grid.size.w - ((8 * char_step) + 5)) / 2;

  GRect selection_frame = GRect(start_x + col * char_step + (col >= 4 ? 5 : 0),
                                grid.origin.y + 8 + row * row_height,
                                11,
                                20);
#endif

  GRect up_arrow_frame = GRect(selection_frame.origin.x + (selection_frame.size.w - 5) / 2,
                               selection_frame.origin.y - 5,
                               5,
                               3);
  GRect down_arrow_frame = GRect(selection_frame.origin.x + (selection_frame.size.w - 5) / 2,
                                 selection_frame.origin.y + selection_frame.size.h + 2,
                                 5,
                                 3);

#if PBL_SDK_2
  layer_set_frame(inverter_layer_get_layer(s_inverterlayer), selection_frame);
#else
  layer_set_frame(s_layer_selection, selection_frame);
  layer_set_frame(text_layer_get_layer(s_textlayer_selection_value),
                  GRect(0, -6, selection_frame.size.w, 30));
  s_selection_value[0] = s_value[s_offset];
  text_layer_set_text(s_textlayer_selection_value, s_selection_value);
#endif
  layer_set_frame(bitmap_layer_get_layer(s_bitmaplayer_down_arrow), down_arrow_frame);
  layer_set_frame(bitmap_layer_get_layer(s_bitmaplayer_up_arrow), up_arrow_frame);
}
