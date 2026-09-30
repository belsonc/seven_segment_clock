// ESP32-4848S040 Touch Display with LVGL and Relay Control

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP32_4848S040.h>
#include <lvgl.h>
#include "touch.h"

// Display backlight pin
#define GFX_BL 38

// Display objects
Arduino_ESP32SPI *bus;
Arduino_RGB_Display *gfx;

// Display dimensions (unused legacy variables)
int16_t w, h, text_size, banner_height, graph_baseline, graph_height, channel_width, signal_width;

// LVGL buffer definitions
#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))  // Will be 2 for RGB565
static uint8_t buf1[480 * 480 / 10 * BYTE_PER_PIXEL];
lv_display_t *display;

// LVGL display configuration
#define TFT_HOR_RES   480
#define TFT_VER_RES   480
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0
#define TFT_BRIGHTNESS 255  // Backlight brightness (0-255)

// LVGL draw buffer: 1/10 screen size usually works well (size is in bytes)
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// Relay GPIO configuration
#define GPIO_RELAY1  40
#define GPIO_RELAY2  2
#define GPIO_RELAY3  1


bool digit_format[10][7] = {{true, true, true, false, true, true, true}, //0
                            {false, false, true, false, false, true, false}, //1
                            {true, false, true, true, true, false, true}, //2
                            {true, false, true, true, false, true, true}, //3
                            {false, true, true, true, false, true, false}, //4
                            {true, true, false, true, false, true, true}, //5
                            {true, true, false, true, true, true, true}, //6
                            {true, false, true, false, false, true, false}, //7
                            {true, true, true, true, true, true, true}, //8
                            {true, true, true, true, false, true, true}}; //9

lv_obj_t *digits[4][7];  //there's 4 digits being displayed on the screen, 7 segments each



int x_coords[7] = {0, 0, 80, 0, 0, 80, 0};
int y_coords[7] = {20, 45, 45, 220, 245, 245, 420};

// Display flushing callback for LVGL
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#endif

  lv_disp_flush_ready(disp);
}

// Touchpad input callback for LVGL
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PRESSED;
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Tick callback for LVGL
static uint32_t my_tick(void)
{
  return millis();
}

// Process relay button events
void process_relay_action(lv_event_t *e, int relay)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * btn = (lv_obj_t*)lv_event_get_current_target(e);

  if(code == LV_EVENT_CLICKED) { // If it's a simple push button
    LV_LOG_USER("Clicked");
  }
  else if(code == LV_EVENT_VALUE_CHANGED) { // case of toggle button
    Serial.println("Toggled");
    int gpio = 0;
    switch (relay) {
      case 1 : gpio = GPIO_RELAY1; break;
      case 2 : gpio = GPIO_RELAY2; break;
      case 3 : gpio = GPIO_RELAY3; break;                 
    }    
    if (lv_obj_has_state(btn, LV_STATE_CHECKED) == true) {
      Serial.println("Is now checked");    
      pinMode(gpio, OUTPUT);
      digitalWrite(gpio, HIGH);
    }
    else {
      Serial.println("Is now released");
      pinMode(gpio, OUTPUT);
      digitalWrite(gpio, LOW);
    }
  }

}

// Event handler for relay 1 button
static void event_handler_relay1(lv_event_t *e)
{
  process_relay_action(e, 1);
}

// Event handler for relay 2 button
static void event_handler_relay2(lv_event_t *e)
{
  process_relay_action(e, 2);
}

// Event handler for relay 3 button
static void event_handler_relay3(lv_event_t *e)
{
  process_relay_action(e, 3);
}

// Create GUI with three relay control buttons
void relay_gui(void)
{
  int horiz_length = 90;
  int horiz_depth = 20;
  int vert_length = 20;
  int vert_depth = 170;



  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 7; j++) {
      digits[i][j] = lv_obj_create(lv_screen_active());
//      lv_obj_set_size(digits[i][j], 20, 100); // Set size for each segment
/*      if (j < 3) {
        lv_obj_set_size(digits[i][j], horiz_length, horiz_depth); // Horizontal segments
      } else {
        lv_obj_set_size(digits[i][j], vert_length, vert_depth); // Vertical segments
      }
*/
      if (j == 0 || j == 3 || j == 6) { //horizontal segments
          lv_obj_set_size(digits[i][j], horiz_length, horiz_depth);
      } else { //vertical segments
          lv_obj_set_size(digits[i][j], vert_length, vert_depth);
      }
      // lv_obj_set_pos(one_hour_right_bottom,200,245);


      if (j == 0 || j == 3 || j == 6) { //horizontal segments
          lv_obj_set_pos(digits[i][j], x_coords[j] + 25 + (i * 110), y_coords[j]);
      } else { //vertical segments
          lv_obj_set_pos(digits[i][j], x_coords[j] + 20 + (i * 110), y_coords[j]);
      }

//      lv_obj_set_pos(digits[i][j], x_coords[j] + 20 + (i * 110), y_coords[j]);
      lv_obj_set_scrollbar_mode(digits[i][j], LV_SCROLLBAR_MODE_OFF);
      lv_obj_set_style_bg_color(digits[i][j], lv_color_hex(0x000000), LV_PART_MAIN); // Default off color
      lv_obj_set_style_border_color(digits[i][j], lv_color_hex(0x000000), LV_PART_MAIN);
    }
  }

    for (int i = 0; i < 4; i++) {  //screen position
        for (int j = 0; j < 7; j++) {  //segments
            if (digit_format[i+5][j]) { 
                lv_obj_set_style_bg_color(digits[i][j], lv_color_hex(0x987654), LV_PART_MAIN); // On color
            } else {
                lv_obj_set_style_bg_color(digits[i][j], lv_color_hex(0x000000), LV_PART_MAIN); // Off color
            }
        }
    }

    int num_to_draw;
    for (int i=0; i<4; i++) {
      if (i == 0) {
        num_to_draw = 2;
      }
      else if (i == 1) {
        num_to_draw = 8;
      }
      else if (i == 2) {
        num_to_draw = 5;
      }
      else {
        num_to_draw = 7;
      }
      for (int j = 0; j<7; j++) {
        if(digit_format[num_to_draw][j]) {
            lv_obj_set_style_bg_color(digits[i][j], lv_color_hex(0x344378), LV_PART_MAIN); //on color
        } else {
            lv_obj_set_style_bg_color(digits[i][j], lv_color_hex(0x000000), LV_PART_MAIN); //off color
        }
      }
    }

/*
  // Create relay 1 button
  lv_obj_t *btn1 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn1, event_handler_relay1, LV_EVENT_ALL, NULL);
  lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -160);
  lv_obj_add_flag(btn1, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_size(btn1, 300, 75);
  label = lv_label_create(btn1);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
  lv_label_set_text(label, "Relay 1");
  lv_obj_center(label);

  // Create relay 2 button
  lv_obj_t *btn2 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn2, event_handler_relay2, LV_EVENT_ALL, NULL);
  lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_size(btn2, 300, 75);
  label = lv_label_create(btn2);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
  lv_label_set_text(label, "Relay 2");
  lv_obj_center(label);

  // Create relay 3 button
  lv_obj_t *btn3 = lv_button_create(lv_screen_active());
  lv_obj_add_event_cb(btn3, event_handler_relay3, LV_EVENT_ALL, NULL);
  lv_obj_align(btn3, LV_ALIGN_CENTER, 0, 160);
  lv_obj_add_flag(btn3, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_size(btn3, 300, 75);
  label = lv_label_create(btn3);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
  lv_label_set_text(label, "Relay 3");
  lv_obj_center(label);

  */
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Setup...");
  touch_init();

  // Initialize 9-bit mode SPI
  bus = new Arduino_ESP32SPI(
    GFX_NOT_DEFINED /* DC */, 39 /* CS */, 48 /* SCK */, 47 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

  // Initialize RGB panel (hardware specific)
  Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
#if 0
    4 /* R0 */, 5 /* R1 */, 6 /* R2 */, 7 /* R3 */, 15 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    11 /* B0 */, 12 /* B1 */, 13 /* B2 */, 14 /* B3 */, 0 /* B4 */,
#else
    11 /* R0 */, 12 /* R1 */, 13 /* R2 */, 14 /* R3 */, 0 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    4 /* B0 */, 5 /* B1 */, 6 /* B2 */, 7 /* B3 */, 15 /* B4 */,
#endif
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

  // Initialize display panel
  gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_4848s040_init_operations, sizeof(st7701_4848s040_init_operations));

  // Check if display initialization succeeded
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }

  // Turn on backlight
#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  analogWrite(GFX_BL, TFT_BRIGHTNESS);
#endif

  // Display startup screen
  gfx->setCursor(100, 200);
  gfx->displayOn();
  gfx->fillScreen(BLACK);
  gfx->setTextColor(BLUE);
  gfx->setTextSize(6 /* x scale */, 6 /* y scale */, 2 /* pixel_margin */);
  gfx->println("Starting...");
  gfx->setRotation((4 - TFT_ROTATION) % 4); // Adjust rotation for Arduino_GFX

  // Initialize LVGL
  lv_init();

  // Set tick source for LVGL timing
  lv_tick_set_cb(my_tick);

  // Create LVGL display
  lv_display_t *disp;
  disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_rotation(disp, TFT_ROTATION);

#if 1
  // Set default theme
  lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x000000),LV_PART_MAIN);
  lv_color_t color_primary = lv_color_hex(0x022681); //lv_palette_main(LV_PALETTE_BLUE);
  lv_color_t color_secondary = lv_color_hex(0xCAB226); //lv_palette_main(LV_PALETTE_RED);
  lv_theme_t * theme = lv_theme_default_init(NULL, color_primary, color_secondary, LV_THEME_DEFAULT_DARK, LV_FONT_DEFAULT);
  lv_disp_set_theme(disp, theme);
#endif

  // Initialize touch input
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  // Build GUI
  relay_gui();
}

void loop()
{
  // Let LVGL handle GUI tasks
  lv_timer_handler();
  delay(5);
}
