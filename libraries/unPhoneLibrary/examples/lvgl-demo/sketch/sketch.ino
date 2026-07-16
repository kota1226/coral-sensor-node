// sketch.ino ////////////////////////////////////////////////////////////////
// LVGL on unPhone demo //////////////////////////////////////////////////////
//
// derived from
// https://github.com/lvgl/lvgl/blob/master/examples/arduino/LVGL_Arduino/LVGL_Arduino.ino
// see also https://docs.lvgl.io/master/get-started/platforms/arduino.html

// we use the touchscreen (XPT2046) driver from the unPhone library (with the
// TFT driver from TFT_eSPI)
#include "unPhone.h"
#include <Adafruit_SPIFlash.h> // for LDF

#include <lvgl.h>                       // LVGL //////////////////////////////
#define CONFIG_IDF_TARGET_ESP32S3 1
#include <demos/lv_demos.h>
#include <demos/widgets/lv_demo_widgets.h>
#include <TFT_eSPI.h>
#include <examples/lv_examples.h>
#include <demos/lv_demos.h>
void lv_demo_widgets(void);             // encourage pio LDF...

// create an unPhone; add a custom version of Arduino's map command for
// translating from touchscreen coordinates to LCD coordinates
unPhone u = unPhone();
long my_mapper(long, long, long, long, long);

/*Change to your screen resolution*/
static const uint16_t screenWidth  = 480;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * 10 ];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); // the LCD screen

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(
  lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p
) {
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  tft.startWrite();
  tft.setAddrWindow( area->x1, area->y1, w, h );
  tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
  tft.endWrite();

  lv_disp_flush_ready( disp );
}

// map touch coords to lcd coords
// a version of map that never returns out of range values
long my_mapper(long x, long in_min, long in_max, long out_min, long out_max) {
  long probable =
  (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if(probable < out_min) return out_min;
  if(probable > out_max) return out_max;
  return probable;
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
  uint16_t touchX, touchY;

  // start of changes for unPhone ////////////////////////////////////////////
  bool touched = u.tsp->touched();

  if( !touched ) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    TS_Point p(-1, -1, -1);
    p = u.tsp->getPoint();

// filter the ghosting on version 9 boards (on USB power; ~300 pressure)
#if UNPHONE_SPIN >= 9
    if(p.z < 400) return;
//  D("probable ghost reject @ p.x(%04d), p.y(%04d) p.z(%04d)\n", p.x,p.y,p.z)
#endif

    Serial.printf("   p.x(%04d),  p.y(%04d) p.z(%04d)\n", p.x, p.y, p.z);
    if(p.x < 0 || p.y < 0) D("************* less than zero! *************\n")

    long xMin = 320;
    long xMax = 3945;
    long yMin = 420;
    long yMax = 3915;

    long xscld = my_mapper((long) p.x, xMin, xMax, 0, (long) screenWidth);
    long yscld = // Y is inverted on rotation 1 (landscape, buttons right)
      ((long) screenHeight) - 
      my_mapper((long) p.y, yMin, yMax, 0, (long) screenHeight);
    touchX = (uint16_t) xscld;
    touchY = (uint16_t) yscld;

    Serial.printf("touchX(%4d), touchY(%4d)\n", touchX, touchY);
    // end of changes for unPhone ////////////////////////////////////////////

    data->point.x = touchX;
    data->point.y = touchY;
    Serial.printf("Data x %u, Data y %u\n", touchX, touchY);
  }
}

void setup() {
  Serial.begin( 115200 ); /* prepare for possible serial debug */

  u.begin();
  u.tftp = (void*) &tft;
  u.tsp->setRotation(1);
  u.backlight(true);

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino +=
    String('V') + lv_version_major() + "." +
    lv_version_minor() + "." + lv_version_patch();

  Serial.println( LVGL_Arduino );
  Serial.println( "I am LVGL_Arduino" );

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb( my_print ); /* register print function for debugging */
#endif

  tft.begin();      /* TFT init */
  tft.setRotation( 1 ); /* Landscape orientation */

  /*Set the touchscreen calibration data,
   the actual data for your display can be acquired using
   the Generic -> Touch_calibrate example from the TFT_eSPI library*/
  uint16_t calData[5] = { 347, 3549, 419, 3352, 5 };
  tft.setTouch( calData );

  lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * 10 );

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );

#if 0
  /* Create simple label */
  lv_obj_t *label = lv_label_create( lv_scr_act() );
  lv_label_set_text( label, LVGL_Arduino.c_str() );
  lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );
#else
  /* Try an example from the lv_examples Arduino library
     make sure to include it as written above.
  lv_example_btn_1();
   */

  // uncomment one of these demos
  lv_demo_widgets();      // OK
  // lv_demo_benchmark();      // OK
  // lv_demo_keypad_encoder();   // works, but I haven't an encoder
  // lv_demo_music();        // NOK
  // lv_demo_printer();
  // lv_demo_stress();       // seems to be OK
#endif
  Serial.println( "Setup done" );
}

void loop() {
  lv_timer_handler(); /* let the GUI do its work */
  delay( 5 );
}
