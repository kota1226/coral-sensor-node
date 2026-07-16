// unPhoneUI0.cpp ////////////////////////////////////////////////////////////
// agglomerated default UI based on Adafruit's GFX and
// mostly derived from HarryEH's code
// and Mark Hepple's predictive text python code;
// thanks both!

// UIController.cpp //////////////////////////////////////////////////////////

#if UNPHONE_UI0 == 1
#include "unPhoneUI0.h"
#include <WiFi.h>                 // status for config scrn
#include <Adafruit_ImageReader.h> // image-reading for test card scrn

static unPhone *up;

// initialisation flag, not complete until parent has finished config
bool UIController::provisioned = false;

// the UI elements types (screens) /////////////////////////////////////////
const char *UIController::ui_mode_names[] = {
  "Menu",
  "Testcard: basic graphics",
  "Touchpaint",
  "Predictive text",
  "Etch-a-sketch",
  "Factory test rig",
  "Home",
};
uint8_t UIController::NUM_UI_ELEMENTS = 7;  // number of UI elements

// keep Arduino IDE compiler happy /////////////////////////////////////////
UIElement::UIElement(Adafruit_HX8357* tftp, XPT2046_Touchscreen* tsp, SdFat *sdp) {
  m_tft = tftp;
  m_ts = tsp;
  m_sd = sdp;
}
void UIElement::someFuncDummy() { }

// constructor for the main class ///////////////////////////////////////////
UIController::UIController(ui_modes_t start_mode) {
  m_mode = start_mode;
}

bool UIController::begin(unPhone& u) { ///////////////////////////////////////
  up = &u;
  begin(u, true);
  return true;
}
bool UIController::begin(unPhone& u, boolean doDraw) { ///////////////////////
  up = &u;
  D("UI.begin()\n")

  up->tftp->fillScreen(HX8357_GREEN);
  WAIT_MS(50)
  up->tftp->fillScreen(HX8357_BLACK);
  
  // define the menu element and the first m_element here 
  m_menu = new MenuUIElement(up->tftp, up->tsp, up->sdp);
  if(m_menu == NULL) {
    Serial.println("ERROR: no m_menu allocated");
    return false;
  }
  allocateUIElement(m_mode);

  if(doDraw)
    redraw();
  return true;
}

UIElement* UIController::allocateUIElement(ui_modes_t newMode) {
  // TODO trying to save memory here, but at the expense of possible
  // fragmentation; perhaps maintain an array of elements and never delete?
  if(m_element != 0 && m_element != m_menu) delete(m_element);

  switch(newMode) {
    case ui_menu:
      m_element = m_menu;                                               break;
    case ui_configure:
      m_element = new ConfigUIElement(up->tftp, up->tsp, up->sdp);      break;
    case ui_testcard:
      m_element = new TestCardUIElement(up->tftp, up->tsp, up->sdp);    break;
    case ui_touchpaint:
      m_element = new TouchpaintUIElement(up->tftp, up->tsp, up->sdp);  break;
    case ui_text:
      m_element = new TextPageUIElement(up->tftp, up->tsp, up->sdp);    break;
    case ui_etchasketch:
      m_element = new EtchASketchUIElement(up->tftp, up->tsp, up->sdp); break;
    case ui_testrig:
      m_element = new TestRigUIElement(up->tftp, up->tsp, up->sdp);     break;
    default:
      Serial.printf("invalid UI mode %d in allocateUIElement\n", newMode);
      m_element = m_menu;
  }

  return m_element;
}

// touch management code ////////////////////////////////////////////////////
TS_Point nowhere(-1, -1, -1);    // undefined coordinate
TS_Point firstTouch(0, 0, 0);    // the first touch defaults to 0,0,0
TS_Point p(-1, -1, -1);          // current point of interest (signal)
TS_Point prevSig(-1, -1, -1);    // the previous accepted touch signal
bool firstTimeThrough = true;    // first time through gotTouch() flag
uint16_t fromPrevSig = 0;        // distance from previous signal
unsigned long now = 0;           // millis
unsigned long prevSigMillis = 0; // previous signal acceptance time
unsigned long sincePrevSig = 0;  // time since previous signal acceptance
uint16_t DEFAULT_TIME_SENSITIVITY = 150; // min millis between touches
uint16_t TIME_SENSITIVITY = DEFAULT_TIME_SENSITIVITY;
uint16_t DEFAULT_DIST_SENSITIVITY = 200; // min distance between touches
uint16_t DIST_SENSITIVITY = DEFAULT_DIST_SENSITIVITY;
uint16_t TREAT_AS_NEW = 600;     // if no signal in this period treat as new
uint8_t MODE_CHANGE_TOUCHES = 1; // number of requests needed to switch mode
uint8_t modeChangeRequests = 0;  // number of current requests to switch mode
bool touchDBG = false;           // set true for diagnostics

void setTimeSensitivity(uint16_t s = DEFAULT_TIME_SENSITIVITY) { ////////////
  TIME_SENSITIVITY = s;
}
void setDistSensitivity(uint16_t d = DEFAULT_DIST_SENSITIVITY) { ////////////
  DIST_SENSITIVITY = d;
}
uint16_t distanceBetween(TS_Point a, TS_Point b) { // coord distance ////////
  uint32_t xpart = b.x - a.x, ypart = b.y - a.y;
  xpart *= xpart; ypart *= ypart;
  return sqrt(xpart + ypart);
}
void dbgTouch() { // print current state of touch model /////////////////////
  if(touchDBG) {
    D("p(x:%04d,y:%04d,z:%03d)", p.x, p.y, p.z)
    D(", now=%05lu, sincePrevSig=%05lu, prevSig=", now, sincePrevSig)
    D("p(x:%04d,y:%04d,z:%03d)", prevSig.x, prevSig.y, prevSig.z)
    D(", prevSigMillis=%05lu, fromPrevSig=%05u", prevSigMillis, fromPrevSig)
  }
}
const char *UIController::modeName(ui_modes_t m) {
  switch(m) {
    case ui_menu:               return "ui_menu";          break;
    case ui_configure:          return "ui_configure";     break;
    case ui_testcard:           return "ui_testcard";      break;
    case ui_touchpaint:         return "ui_touchpaint";    break;
    case ui_text:               return "ui_text";          break;
    case ui_etchasketch:        return "ui_etchasketch";   break;
    case ui_testrig:            return "ui_testrig";       break;
    default:
      D("invalid UI mode %d in allocateUIElement\n", m)
      return "invalid UI mode";
  }
}

// accept or reject touch signals ///////////////////////////////////////////
bool UIController::gotTouch() { 
  if(!up->tsp->touched()) {
    return false; // no touches
  }
    
  // set up timings
  now = millis();
  if(firstTimeThrough) {
    sincePrevSig = TIME_SENSITIVITY + 1;
  } else {
    sincePrevSig = now - prevSigMillis;
  }

  // retrieve a point
  p = up->tsp->getPoint();
  // add the following if want to dump the rest of the buffer:
  // while (! up->tsp->bufferEmpty()) {
  //   uint16_t x, y;
  //   uint8_t z;
  //   up->tsp->readData(&x, &y, &z);
  // }
  // delay(300);
  if(touchDBG)
    D("\n\np(x:%04d,y:%04d,z:%03d)\n\n", p.x, p.y, p.z)

  // if it is at 0,0,0 and we've just started then ignore it
  if(p == firstTouch && firstTimeThrough) {
    dbgTouch();
    if(touchDBG) D(", rejecting (0)\n\n")
    return false;
  }
  firstTimeThrough = false;
  
  // calculate distance from previous signal
  fromPrevSig = distanceBetween(p, prevSig);
  dbgTouch();

  if(touchDBG)
    D(", sincePrevSig<TIME_SENS.: %d...  ", sincePrevSig<TIME_SENSITIVITY)
  if(sincePrevSig < TIME_SENSITIVITY) { // ignore touches too recent
    if(touchDBG) D("rejecting (2)\n")
  } else if(
    fromPrevSig < DIST_SENSITIVITY && sincePrevSig < TREAT_AS_NEW
  ) {
    if(touchDBG) D("rejecting (3)\n")
#if UNPHONE_SPIN >= 9
  } else if(p.z < 400) { // ghost touches in 9 (on USB power) are ~300 pressure
    // or ignore: x > 1200 && x < 1700 && y > 2000 && y < 3000 && z < 450 ?
    if(touchDBG) D("rejecting (4)\n") // e.g. p(x:1703,y:2411,z:320)
#endif
  } else {
    prevSig = p;
    prevSigMillis = now;
    if(false) // delete this line to debug touch debounce
      D("decided this is a new touch: p(x:%04d,y:%04d,z:%03d)\n", p.x, p.y, p.z)
    return true;
  }
  return false;
}

/////////////////////////////////////////////////////////////////////////////
void UIController::changeMode() {
  D("changing mode from %d (%s) to...", m_mode, modeName(m_mode))
  up->tftp->fillScreen(HX8357_BLACK);
  setTimeSensitivity();         // set TIME_SENS to the default
  nextMode = (ui_modes_t) ((MenuUIElement *)m_menu)->getMenuItemSelected();
  if(nextMode == -1) nextMode = ui_menu;

  // allocate an element according to nextMode and 
  if(m_mode == ui_menu) {       // coming OUT of menu
    if(nextMode == ui_touchpaint)
      setTimeSensitivity(25);   // TODO make class member and move to TPUIE
    m_mode =    nextMode;
    m_element = allocateUIElement(nextMode);
  } else {                      // going INTO menu
    m_mode =    ui_menu;
    m_element = m_menu;
  }
  D("...%d (%s)\n", m_mode, modeName(m_mode))

  redraw();
  return;
}

/////////////////////////////////////////////////////////////////////////////
void UIController::handleTouch() {
  int temp = p.x;
  p.x = map(p.y, up->TS_MAXX, up->TS_MINX, 0, up->tftp->width());
  p.y = map(temp, up->TS_MAXY, up->TS_MINY, 0, up->tftp->height());
  // Serial.print("dbgTouch from handleTouch: "); dbgTouch(); Serial.flush();
  
  if(m_element->handleTouch(p.x, p.y)) {
    if(++modeChangeRequests >= MODE_CHANGE_TOUCHES) {
      changeMode();
      modeChangeRequests = 0;
    }
  } 
}

/////////////////////////////////////////////////////////////////////////////
void UIController::run() {
  if(gotTouch())
    handleTouch();
  m_element->runEachTurn();
}

////////////////////////////////////////////////////////////////////////////
void UIController::redraw() {
  up->tftp->fillScreen(HX8357_BLACK);
  m_element->draw();
}

////////////////////////////////////////////////////////////////////////////
void UIController::message(char *s) {
  up->tftp->setCursor(0, 465);
  up->tftp->setTextSize(2);
  up->tftp->setTextColor(HX8357_CYAN, HX8357_BLACK);
  up->tftp->print("                          ");
  up->tftp->setCursor(0, 465);
  up->tftp->print(s);
}

////////////////////////////////////////////////////////////////////////////
void UIElement::drawSwitcher(uint16_t xOrigin, uint16_t yOrigin) {
  uint16_t leftX = xOrigin;
  if(leftX == 0)
    leftX = (SWITCHER * BOXSIZE) + 8; // default is on right hand side
    m_tft->fillRect(leftX, 15 + yOrigin, BOXSIZE - 15, HALFBOX - 10, WHITE);
    m_tft->fillTriangle(
      leftX + 15, 35 + yOrigin,
      leftX + 15,  5 + yOrigin,
      leftX + 30, 20 + yOrigin,
      WHITE
    );
}

////////////////////////////////////////////////////////////////////////////
void UIElement::showLine(const char *buf, uint16_t *yCursor) {
  *yCursor += 20;
  m_tft->setCursor(0, *yCursor);
  m_tft->print(buf);
}


//////////////////////////////////////////////////////////////////////////////
// ConfigUIElement.cpp ///////////////////////////////////////////////////////

/**
 * Handle touches on this page
 * 
 * @param x - the x coordinate of the touch 
 * @param y - the y coordinate of the touch 
 * @returns bool - true if the touch is on the switcher
 */
bool ConfigUIElement::handleTouch(long x, long y) {
  return y < BOXSIZE && x > (BOXSIZE * SWITCHER);
}

// writes various things including mac address and wifi ssid ///////////////
void ConfigUIElement::draw() {
  // say hello
  m_tft->setTextColor(GREEN);
  m_tft->setTextSize(2);
  uint16_t yCursor = 0;
  m_tft->setCursor(0, yCursor);
  m_tft->print("Welcome to unPhone!");
  m_tft->setTextColor(BLUE);

  // note about switcher
  yCursor += 20;
  if(UIController::provisioned) {
    showLine("(where you see the arrow,", &yCursor);
    showLine("  press for menu)", &yCursor);
    drawSwitcher();
  } else {
    yCursor += 20;
  }

  // are we connected?
  yCursor += 40;
  m_tft->setCursor(0, yCursor);
  if (WiFi.status() == WL_CONNECTED) {
    yCursor += 20;
    m_tft->print("Connected to: ");
    m_tft->setTextColor(GREEN);
    m_tft->print(WiFi.SSID());
    m_tft->setTextColor(BLUE);
  } else {
    m_tft->setTextColor(RED);
    m_tft->print("Not connected to WiFi:");
    yCursor += 20;
    m_tft->setCursor(0, yCursor);
    m_tft->print("  trying to connect...");
    m_tft->setTextColor(BLUE);
  }

  // display the mac address
  char mac_buf[13];
  yCursor += 40;
  m_tft->setCursor(0, yCursor);
  m_tft->print("MAC addr: ");
  m_tft->print(up->getMAC());

  // firmware version
  showLine("Firmware date:", &yCursor);
  showLine("  ", &yCursor);
  m_tft->print(up->buildTime);

  // (used to be) AP details, now just unPhone.name
  showLine("Firmware name: ", &yCursor);
  showLine("  ", &yCursor);
  m_tft->print(up->appName);
  //m_tft->print("-");
  //m_tft->print(up->getMAC());

  // IP address
  showLine("IP: ", &yCursor);
  m_tft->print(WiFi.localIP());

  // battery voltage
  showLine("VBAT: ", &yCursor);
  m_tft->print(up->batteryVoltage());

  // battery voltage
  showLine("Hardware version: ", &yCursor);
  m_tft->print(UNPHONE_SPIN);

  // display the on-board temperature
  char buf[256];
  float onBoardTemp = temperatureRead();
  sprintf(buf, "MCU temp: %.2f C", onBoardTemp);
  showLine(buf, &yCursor);

  // web link
  yCursor += 60;
  showLine("An ", &yCursor);
  m_tft->setTextColor(MAGENTA);
  m_tft->print("IoT platform");
  m_tft->setTextColor(BLUE);
  m_tft->print(" from the");
  m_tft->setTextColor(MAGENTA);
  showLine("  University of Sheffield", &yCursor);
  m_tft->setTextColor(BLUE);
  showLine("Find out more at", &yCursor);
  m_tft->setTextColor(GREEN);
  showLine("              unphone.net", &yCursor);
}

//////////////////////////////////////////////////////////////////////////////
void ConfigUIElement::runEachTurn() {
  
}


//////////////////////////////////////////////////////////////////////////////
// MenuUIElement.cpp /////////////////////////////////////////////////////////

/**
 * Process touches.
 * @returns bool - true if the touch is a menu item
 */
bool MenuUIElement::handleTouch(long x, long y) {
  // D("text mode: responding to touch @ %d/%d/%d: ", x, y,-1)
  m_tft->setTextColor(WHITE, BLACK);
  int8_t menuItem = mapTextTouch(x, y);
  D("menuItem=%d, ", menuItem)
  if(menuItem == -1) D("ignoring\n")

  if(menuItem > 0 && menuItem < UIController::NUM_UI_ELEMENTS) {
    menuItemSelected = menuItem;
    return true;
  }
  return false;
}

// returns menu item number //////////////////////////////////////////////////
int8_t MenuUIElement::mapTextTouch(long xInput, long yInput) {
  for(
    int y = 30, i = 1;
    i < UIController::NUM_UI_ELEMENTS && y < 480;
    y += 48, i++
  ) {
    if(xInput > 270 && yInput > y && yInput < y + 48)
      return i;
  }
  return -1;
}

// draw a textual menu ///////////////////////////////////////////////////////
void MenuUIElement::draw(){
  m_tft->setTextSize(2);
  m_tft->setTextColor(BLUE);

  m_tft->setCursor(230, 0);
  m_tft->print("MENU");

  uint16_t yCursor = 30;
  m_tft->drawFastHLine(0, yCursor, 320, MAGENTA);
  yCursor += 16;

  for(int i = 1; i < UIController::NUM_UI_ELEMENTS; i++) {
    m_tft->setCursor(0, yCursor);
    m_tft->print(UIController::ui_mode_names[i]);
    drawSwitcher(288, yCursor - 12);
    yCursor += 32;
    m_tft->drawFastHLine(0, yCursor, 320, MAGENTA);
    yCursor += 16;
  }
}

//////////////////////////////////////////////////////////////////////////////
void MenuUIElement::runEachTurn(){ // text page UI, run each turn
  // do nothing
}


//////////////////////////////////////////////////////////////////////////////
// TestCardUIElement.cpp /////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/**
 * Function that handles the touch on this page
 * 
 * @param x - the x coordinate of the touch 
 * @param y - the y coordinate of the touch 
 * @returns bool - true if the touch is on the switcher
 */
bool TestCardUIElement::handleTouch(long x, long y) {
  return y < BOXSIZE && x > (BOXSIZE * SWITCHER);
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/**
 * Function that controls the drawing on the test page
 */
void TestCardUIElement::draw(){
  m_tft->setTextColor(GREEN);
  m_tft->setTextSize(2);

  drawBBC();
  drawTestcard();

  WAIT_A_SEC;

  drawSwitcher();
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/**
 * Test page function that runs each turn 
 */
void TestCardUIElement::runEachTurn(){
  // do nothing 
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/**
 * draws the bbc test image
 */
void TestCardUIElement::drawBBC() { 
  Adafruit_ImageReader reader(*m_sd); // Image-reader object, pass in SD filesys

  // draw bmp
  Adafruit_Image       img;        // an image loaded into RAM
  ImageReturnCode      stat;       // status from image-reading functions
  stat = reader.drawBMP("/testcard.bmp", *m_tft, 0, 0); // draw it
  reader.printStatus(stat);        // how'd we do?

  m_tft->setTextSize(2);
  m_tft->setTextColor(BLUE);
  m_tft->setCursor(10, 360); m_tft->print("please wait"); WAIT_MS(100)
  m_tft->setCursor(10, 340); m_tft->print("Winding up elastic band:");
  WAIT_MS(600)
  for(int i = 0; i<12; i++) {
    m_tft->setCursor(150 + (i * 5), 360); m_tft->print(".");
    WAIT_MS(100)
  }
  WAIT_MS(300) WAIT_MS(300)
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/**
 * draw stuff to make screen dimensions obvious
 */
void TestCardUIElement::drawTestcard() {
  m_tft->fillScreen(BLACK);
  m_tft->setTextColor(GREEN);
  m_tft->setTextSize(1);
  m_tft->fillCircle(160, 240 + 100, 1, RED);
  m_tft->setCursor(160 + 8, 340);
  m_tft->setTextColor(RED);
  m_tft->print("X:160");

  m_tft->fillCircle(160 + 100, 240, 1, CYAN);
  m_tft->setCursor(260 + 8, 240);
  m_tft->setTextColor(CYAN);
  m_tft->print("Y:240");

  // (we're 320 wide and 480 tall in portrait mode)
  //            X    Y    W    H
  m_tft->drawRect(  0,   0,  60,  60, GREEN);
  m_tft->drawRect(130, 215,  60,  60, GREEN);
  m_tft->drawRect(260, 420,  60,  60, GREEN);
  m_tft->fillTriangle(5, 443, 10, 433, 15, 443, MAGENTA);
  m_tft->fillRoundRect(153, 435, 20, 10, 4, RED);
  m_tft->fillRect(295, 435, 10, 10, MAGENTA);

  // TODO the below causes a hang (bad radius?); use to set up task WDT
  // registration of this task with recovery?
  // m_tft->fillRoundRect(150, 430, 20, 10, 10, RED);

  // label the green boxes
  m_tft->setTextColor(BLUE);
  m_tft->setCursor(3, 50); m_tft->print("60x60:0,0");
  m_tft->setCursor(130, 277); m_tft->print("60x60:130,215");
  m_tft->setCursor(241, 410); m_tft->print("60x60:260,420");

  // significant positions, text
  m_tft->setTextSize(3);
  m_tft->setTextColor(YELLOW);
  m_tft->setCursor(135,  10); m_tft->print("the");
  m_tft->setCursor(135,  45); m_tft->print("test");
  m_tft->setCursor(135,  80); m_tft->print("card");
  m_tft->setTextSize(2);
  m_tft->setTextColor(WHITE);
  m_tft->setCursor(  0,   0);   m_tft->print("0,0");
  m_tft->setCursor(  0, 300);   m_tft->print("0,300");
  // m_tft->setCursor(260,   0); m_tft->print("260,0"); (obscured by switcher box)
  m_tft->setCursor(248,  60); m_tft->print("248,60");
  m_tft->setCursor(100, 300); m_tft->print("100,300");
  m_tft->setCursor(150, 240); m_tft->print("150,240");
  m_tft->setCursor(200, 300); m_tft->print("200,300");
  m_tft->setCursor(235, 465); m_tft->print("235,465");
  m_tft->setCursor(  0, 465);   m_tft->print("0,465");

  m_tft->drawFastHLine(150, 160, 120, MAGENTA);
  m_tft->drawFastVLine(270, 160,  50, MAGENTA);
  m_tft->drawLine(150, 160, 270, 210, MAGENTA);
  m_tft->fillCircle(230, 180, 5, CYAN);

  // horizontal and vertical counters
  m_tft->setTextColor(GREEN);
  for(int i=0, j=1; i<=460; i+=20, j++) {
    if(j == 7) continue;
    m_tft->setCursor(70, i);
    if(j < 10) m_tft->print(" ");
    if(i == 460) m_tft->setTextColor(GREEN);
    m_tft->print(j);
    WAIT_MS(50)
    if(i == 460) {
      m_tft->setCursor(98, 467);
      m_tft->setTextSize(1);
      m_tft->print("(X:70, Y:");
      m_tft->print(i);
      m_tft->print(")");
      m_tft->setTextSize(2);
    }
  }
  m_tft->setCursor(0, 120);
  m_tft->setTextColor(GREEN);
  m_tft->print("01234567890123456789012345");
}
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// TouchpaintUIElement.cpp ///////////////////////////////////////////////////

/**
 * 
 */
void TouchpaintUIElement::draw(){
  m_tft->fillScreen(BLACK);
  drawSelector();
  drawSwitcher();
}

/**
 * 
 */
void TouchpaintUIElement::runEachTurn(){
  // Do nothing 
}

/**
 * selection for point inside menu
 */
void TouchpaintUIElement::colourSelector(long x, long y) {
  oldcolour = currentcolour;

  uint16_t boxXOrigin;
  for(uint8_t i = 0; i < NUM_BOXES; i++) { // white border around selection
    boxXOrigin = i * BOXSIZE;
    if(x < ((i + 1) * BOXSIZE) ) {
      currentcolour = colour2box[i];
      if(i == 6) // white box: red border
        m_tft->drawRect(boxXOrigin, 0, BOXSIZE, BOXSIZE, RED);
      else       // white border
        m_tft->drawRect(boxXOrigin, 0, BOXSIZE, BOXSIZE, WHITE);
      break;
    }
  }

  if(oldcolour != currentcolour) { // refill previous selection (etc.)
    drawSelector();
    D("changed colour to %d\n", currentcolour)
  }
}

/**
 * 
 */
void TouchpaintUIElement::drawSelector() { 
  for(uint8_t i = 0; i < NUM_BOXES; i++) {
    m_tft->fillRect(i * BOXSIZE, 0, BOXSIZE, BOXSIZE, colour2box[i]);
  }
}

/**
 * Process touches.
 * @returns bool - true if the touch is on the switcher
 */
bool TouchpaintUIElement::handleTouch(long x, long y) {
  if(y < BOXSIZE && x > (BOXSIZE * SWITCHER)) {
    return true;
  } else if(y < BOXSIZE) { // we're in the control area
    D("in control area, calling selectColour\n")
    colourSelector(x, y);
  } else if(((y-PENRADIUS) > 0) && ((y+PENRADIUS) < m_tft->height())) {
    D("in drawing area, calling fillCircle\n")
    m_tft->fillCircle(x, y, PENRADIUS, currentcolour);
  }
  
  return false;
}


//////////////////////////////////////////////////////////////////////////////
// TextPageUIElement.cpp /////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// predictive text, and text input history ///////////////////////////////////
Predictor predictor;
class TextHistory {
public:
  static const uint8_t SIZE = 20; // set to 3 for testing
  char *buf[SIZE];    // the history buffer
  uint8_t cursor;     // next insertion point
  uint8_t start;      // first in the sequence
  int8_t iter;        // iterator index
  uint8_t members;    // number of stored values

  TextHistory() { clear(); }
  void store(const char *word);
  void remove();
  bool full() { return members == SIZE; }
  void clear();
  const char *first();
  const char *next();
  void debug();
  void test();
  uint8_t size() { return members; }
};
TextHistory textHistory;

//////////////////////////////////////////////////////////////////////////////
/**
 * Function that handles the touch on this page
 * 
 * @param x - the x coordinate of the touch 
 * @param y - the y coordinate of the touch 
 * @returns bool - true if the touch is on the switcher
 */
bool TextPageUIElement::handleTouch(long x, long y) {
  m_tft->setTextColor(WHITE, BLACK);
    // D("text mode: responding to touch @ %d/%d/%d: ", x, y,-1)

    int8_t symbol = mapTextTouch(x, y);
    D("sym=%d, ", symbol)

    if(symbol == 0) { // "ok"
      D("accepting\n")
      textHistory.store(predictor.first()); // textHistory.debug();
      predictor.reset();
      printHistory(0, 0);
    } else if(symbol >= 1 && symbol <= 8) { // next char
      D("suggesting for %c\n", ((symbol + 1) + '0'));
      if(predictor.suggest(symbol + 1) >= 0) {
        m_tft->setCursor(0, 80);
        int charsPrinted = 0;
        const char *cp = NULL;
        while( (cp = predictor.next()) != NULL ) {
          m_tft->print(cp);
          m_tft->print(" ");
          charsPrinted += strlen(cp) + 1;
        }
        for( ; charsPrinted < 100; charsPrinted++)
          m_tft->print(" ");
      }
    } else if(symbol ==  9) { // delete
      D("calling tH.remove(), (%d)\n", symbol);
      textHistory.remove(); // textHistory.debug();
      printHistory(0, 0);
    } else if(symbol == 10) { // "  _" / ?2
      // TODO
    } else if(symbol == 11) { // mode switcher arrow
      return true;
    }
  return false;
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/**
 * 
 */
void TextPageUIElement::printHistory(uint16_t x, uint16_t y) { 
  int charsPrinted = 0;
  const char *cp = NULL;
  m_tft->setCursor(x, y);
  for(cp = textHistory.first(); cp; cp = textHistory.next()) {
    m_tft->print(cp);
    m_tft->print(" ");
    charsPrinted += strlen(cp) + 1;
  }
  for( ; charsPrinted < 200; charsPrinted++)
    m_tft->print(" ");
}
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
/**
 * 
 */
uint16_t debugCursor = 0;
int8_t TextPageUIElement::mapTextTouch(long xInput, long yInput) { ///////////
  int8_t sym = -1; // symbol
  int8_t row = 0;  // rows (0 is above the text entry area)
  int8_t col = 1;  // columns

  for(int y = 160, i = 1; y < 480; y += 80, i++)
    if(yInput > y) row = i;
  for(int x = 107, i = 2; x < 480; x += 107, i++)
    if(xInput > x) col = i;
  if(row > 0 && col >= 1)
    sym = (( (row - 1) * 3) + col) - 1;

  // D("row=%d, col=%d, sym=%d\n", row, col, sym)

  m_tft->setTextColor(WHITE, BLACK);
  char s[5];
  sprintf(s, "%2d,", sym);
  m_tft->setCursor(debugCursor,150);
  debugCursor += 3;
  if(debugCursor > 280) debugCursor = 0;
  // uncomment for debug sym printing: m_tft->print(s);
  return sym;
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/**
 * 
 */
void TextPageUIElement::draw(){
  drawTextBoxes();
  drawSwitcher(255, 420);
}
//////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
/**
 * Divide up the screen into areas and creates the keyboard
 */
void TextPageUIElement::drawTextBoxes() {
  for(int y = 160; y < 480; y += 80)
    m_tft->drawFastHLine(0, y, 320, MAGENTA);
  m_tft->drawFastHLine(0, 479, 320, MAGENTA);
  for(int x = 0; x < 480; x += 107)
    m_tft->drawFastVLine(x, 160, 320, MAGENTA);
  m_tft->drawFastVLine(319, 160, 320, MAGENTA);

  m_tft->setTextSize(2);
  m_tft->setTextColor(BLUE);
  const uint8_t NUMLABELS = 12;
  const char *labels[NUMLABELS] = {
    " ok",
    " ABC", "DEF", " GHI", " JKL", "MNO", "PQRS", " TUV", "WXYZ",
    " del", "  _", ""
  };
  for(int i = 0, x = 30, y = 190; i < NUMLABELS; i++) {
    for(int j = 0; j < 3; j++, i++) {
      m_tft->setCursor(x, y);
      if(i == 0 || i == 9 || i == 10) m_tft->setTextColor(WHITE);
      m_tft->print(labels[i]);
      if(i == 0 || i == 9 || i == 10) m_tft->setTextColor(BLUE);
      x += 107;
      if(x > 344) x = 30;
    }
    i--;
    y += 80;
    if(y > 430) y = 190;
  }
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/**
 * Text page UI, run each turn
 */
void TextPageUIElement::runEachTurn(){
  // Do nothing
}
//////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
void TextHistory::clear() {
  members = cursor = start = 0;
  iter = -1;
  memset(buf, 0, SIZE);
}

void TextHistory::store(const char *word) {
  if(word == NULL)
    return;
  if(full() && ++start == SIZE)
    start = 0;
  if(members < SIZE)
    members++;

  buf[cursor++] = (char *) word;
  if(cursor == SIZE)
    cursor = 0;
}

// TODO validate this
void TextHistory::remove() { // remove last member
  uint8_t lastMemberIndex;
  if(members == 0) return;
  if(cursor == 0)
    lastMemberIndex = SIZE;
  else
    lastMemberIndex = cursor - 1;
  buf[lastMemberIndex] = NULL;
  cursor = lastMemberIndex;
  members--;
}

const char *TextHistory::first() {
  char *firstVal = NULL;
  if(members > 0) {
    firstVal = buf[start];
    iter = (start + 1) % SIZE;
    if(buf[iter] == 0)
      iter = -1;
  }
  return firstVal;
}

const char *TextHistory::next() {
  if(iter == -1) // first must be called before next
    return NULL;
  const char *nextVal = NULL;

  nextVal = buf[iter];
  iter = ( iter + 1 ) % SIZE;
  if(iter == cursor)
    iter = -1;

  return nextVal;
}

void TextHistory::debug() {
  D(
    "cursor=%d, iter=%d, start=%d, members=%d, ",
    cursor, iter, start, members
  )
  for(int i = 0; i<SIZE; i++)
    D("buf[%d]=%s ", i, buf[i] ? buf[i] : "NULL")
  D("\n")
}
void TextHistory::test() { // note this assumes SIZE of 3
  // assert(textHistory.size() == 0);
  // textHistory.store("1");
  // assert(textHistory.size() == 1);
  // textHistory.clear();
  // assert(textHistory.size() == 0);
  // textHistory.store("1");
  // assert(textHistory.first()[0] == '1');
  // textHistory.store("2");
  // assert(textHistory.next() == NULL);
  // assert(textHistory.first()[0] == '1');
  // assert(textHistory.next()[0] == '2');
  // assert(textHistory.next() == NULL);
  // textHistory.store("3");
  // assert(textHistory.first()[0] == '1');
  // assert(textHistory.next()[0] == '2');
  // assert(textHistory.next()[0] == '3');
  // assert(textHistory.next() == NULL);
  // textHistory.store("4");
  // assert(textHistory.next() == NULL);
  // assert(textHistory.first()[0] == '2');
  // assert(textHistory.next()[0] == '3');
  // assert(textHistory.next()[0] == '4');
  // assert(textHistory.next() == NULL);

  // D("\n")
  // textHistory.debug();
  // D("\n")

  // const char *w[] = { "a", "b", "c", "d" };
  // textHistory.debug();
  // for(int i = 0; i<4; i++) {
  //   textHistory.store(w[i]);
  //   textHistory.debug();
  // }

  // m_tft->fillScreen(BLACK);
  // m_tft->setTextSize(2);
  // m_tft->setTextColor(WHITE, BLACK);
  // m_tft->setCursor(0, 0);
  // //for(int i = 0; i<3; i++)
  // //  m_tft->print(textHistory.buf[i]);
  // for(const char *cp = textHistory.first(); cp; cp = textHistory.next())
  //   m_tft->print(cp ? cp : "NULL");
  // textHistory.debug();

  // D("textHistory.first()=%s\n", textHistory.first() ? "true" : "false")
  // if(textHistory.first())
  //   D("textHistory.first()=%s\n", textHistory.first())
  // const char *n = textHistory.next();
  // D("textHistory.next()=%s\n", n ? n : "NULL")
  // n = textHistory.next();
  // D("textHistory.next()=%s\n", n ? n : "NULL")
  // n = textHistory.next();
  // D("textHistory.next()=%s\n", n ? n : "NULL")
  // n = textHistory.next();
  // D("textHistory.next()=%s\n", n ? n : "NULL")

  // D("iterating: ");
  // for(const char *cp = textHistory.first(); cp; cp = textHistory.next())
  //   D(cp ? cp : "NULL");
  // D("\n");
  // textHistory.debug();

  // D("\niterating with e:\n");
  // textHistory.debug();
  // textHistory.store("e");
  // textHistory.debug();
  // for(const char *cp = textHistory.first(); cp; cp = textHistory.next()) {
  //   textHistory.debug();
  //   D(cp ? cp : "NULL");
  // }
  // D("\n");
  // textHistory.debug();

  /*
  m_tft->setCursor(0, 40);
  m_tft->print(textHistory.hasNext());
  m_tft->print(textHistory.next());
  m_tft->setCursor(0, 60);
  m_tft->print(textHistory.hasNext());
  m_tft->print(textHistory.next());
  m_tft->setCursor(0, 80);
  m_tft->print(textHistory.hasNext());
  // m_tft->print(textHistory.next());
  m_tft->print(textHistory.next());
  m_tft->print(textHistory.next());
  m_tft->print(textHistory.next());
  for(int y = 0; textHistory.hasNext(); y += 20 ) {
    m_tft->setCursor(0, y);
    m_tft->print(textHistory.next());
    m_tft->print(y);
  }
  */
}


//////////////////////////////////////////////////////////////////////////////
// EtchASketchUIElement.cpp //////////////////////////////////////////////////

#define XMID 160
#define YMID 240
int penx = XMID, peny = YMID; // x and y coords of the etching pen

/**
 * Show initial screen.
 */
void EtchASketchUIElement::draw(){
  penx = XMID; // put the pen in the ...
  peny = YMID; // ... middle of the screen

  m_tft->fillScreen(BLACK);
  m_tft->drawLine(0, 0, 319, 0, BLUE);
  m_tft->drawLine(319, 0, 319, 479, BLUE);
  m_tft->drawLine(319, 479, 0, 479, BLUE);
  m_tft->drawLine(0, 479, 0, 0, BLUE);
  drawSwitcher();
}

/**
 * Check the accelerometer, adjust the pen coords and draw a point.
 */
void EtchASketchUIElement::runEachTurn(){
  // get a new sensor event
  sensors_event_t event;
  up->getAccelEvent(&event);

#if UNPHONE_SPIN == 7
  if(event.acceleration.x >  2 && penx < 318) penx = penx + 1;
  if(event.acceleration.x < -2 && penx > 1)   penx = penx - 1;
  if(event.acceleration.y >  2 && peny < 478) peny = peny + 1;
  if(event.acceleration.y < -2 && peny > 1)   peny = peny - 1;
#elif UNPHONE_SPIN >= 9
  if(event.acceleration.x <  2 && penx < 318) penx = penx + 1;
  if(event.acceleration.x > -2 && penx > 1)   penx = penx - 1;
  if(event.acceleration.y >  2 && peny < 478) peny = peny + 1;
  if(event.acceleration.y < -2 && peny > 1)   peny = peny - 1;
#endif

  // draw
  m_tft->drawPixel(penx, peny, HX8357_GREEN);

  // display the results (acceleration is measured in m/s^2)
  /*
  Serial.print("X: "); Serial.print(event.acceleration.x); Serial.print("  ");
  Serial.print("Y: "); Serial.print(event.acceleration.y); Serial.print("  ");
  Serial.print("Z: "); Serial.print(event.acceleration.z); Serial.print("  ");
  Serial.println("m/s^2 ");
  */

  // delay before the next sample
  delay(20);
}

/**
 * Process touches.
 * @returns bool - true if the touch is on the switcher
 */
bool EtchASketchUIElement::handleTouch(long x, long y) {
  return y < BOXSIZE && x > (BOXSIZE * SWITCHER);
}


//////////////////////////////////////////////////////////////////////////////
// TestRigUIElement.cpp //////////////////////////////////////////////////////

/**
 * Process touches.
 * @returns bool - true if the touch is on the switcher
 */
bool TestRigUIElement::handleTouch(long x, long y) {
  // Serial.printf("test rig touch, x=%ld, y=%ld\n", x, y);
  if(x > 25 && x < 280 && y > 215 && y < 280) {
    m_tft->setTextSize(3);
    m_tft->setTextColor(RED);
    m_tft->setCursor(15, 300); m_tft->print("restart in 3...");

    delay(3000);
    ESP.restart();
  }
  return y < BOXSIZE && x > (BOXSIZE * SWITCHER);
}

void TestRigUIElement::draw() {
  m_tft->setTextSize(3);
  m_tft->setTextColor(YELLOW);
  m_tft->setCursor(15,  45); m_tft->print("to enter factory");
  m_tft->setCursor(15,  80); m_tft->print("test mode please");
  m_tft->setCursor(15, 115); m_tft->print("restart with all 3");
  m_tft->setCursor(15, 150); m_tft->print("buttons pressed");

  drawSwitcher();

  up->tftp->drawRect(35, 200, 250, 70, HX8357_MAGENTA);
  up->tftp->setTextSize(3);
  up->tftp->setCursor(58,220);
  up->tftp->setTextColor(HX8357_CYAN);
  up->tftp->print("restart now");
  return;
}

void TestRigUIElement::runEachTurn() { return; }


//////////////////////////////////////////////////////////////////////////////
// unPhonePredictor.cpp

#define printf(args...) // args

/* generate word set predictions for numeric input symbols;
   see main for usage; example word types:
static const uint16_t NUM_WORDS = 2000;
static const char *words[] = {  // in order of frequency
  "the",               //    0
  "to",                //    2
...
static const uint16_t NUM_SUGGS = 2446;
static const uint16_t suggestionSets[][NUM_SUGGS] = {
//arity wordnum(s)...                     index: symseq(s)...
  {  1,  1157, },                       //    0: 224 2245 22454 224548 2245489
  {  1,  333, },                        //    1: 2253
...
static const uint16_t NUM_STATES = 4746;
static const int16_t states[][NUM_STATES] = {
// sugset numds desc(s)[sym/state]....                             ... // index
  {    -1,  8,  2,    1,  3, 1081,  4, 1835,  5, 2311,  6, 2550, },    //    0
*/
Predictor::Predictor() {
}
void Predictor::print() {
  printf("predictor: state(%d) histlen(%d) sugiter(%d) descendants: ",
    state, histlen, sugiter);
  uint16_t numDescInts = states[state][STATE_NUM_DESCS] * 2;
  for(uint16_t i = 0; i < numDescInts; i += 1) {
    const char *bracket = (const char *) "|";
    if(i % 2) bracket = (const char *) " ";
    printf("%s%d%s", bracket, states[state][STATE_1ST_DESC + i], bracket);
  }
  printf("\n");
}
void Predictor::reset() {
  histlen = 0;
  state = 0;
  sugiter = 1;
}
int16_t Predictor::suggest(uint8_t symbolSeen) { // rtn sugset num or -1
  const int16_t *sp = states[state];
  uint16_t numDescendantInts = sp[STATE_NUM_DESCS] * 2;
  printf("symbolSeen(%d) numDescInts(%d)\n", symbolSeen, numDescendantInts);
  for(uint16_t i = 0; i < numDescendantInts; i += 2) {
    uint16_t consumedSymbol =  sp[STATE_1ST_DESC + i + 0];
    uint16_t descendantState = sp[STATE_1ST_DESC + i + 1];
    printf("consumedSymbol(%d) descendantState(%d) symbolSeen(%d)\n",
      consumedSymbol, descendantState, symbolSeen);
    if(symbolSeen == consumedSymbol) {
      history[histlen++] = symbolSeen;
      state = descendantState;
      return states[state][STATE_SUGGESTIONS];
    }
  }
  return -1;
}
const char *Predictor::next() { // pointer to a current suggestion word, or NULL
  const uint16_t *sugset =
    suggestionSets[states[state][STATE_SUGGESTIONS]];
  uint16_t numSuggs = sugset[0];
  uint16_t nextWord;
  if(sugiter <= numSuggs) {
    nextWord = sugset[sugiter++];
    return words[nextWord];
  } else 
    sugiter = 1;
  return NULL;
}
const char *Predictor::first() { // pointer to first suggestion, or NULL
  const uint16_t *sugset =
    suggestionSets[states[state][STATE_SUGGESTIONS]];
  if(sugset[0] > 0)          // there's at least 1 suggestion
    return words[sugset[1]]; // return it
  return NULL;               // there were no suggestions (at root?)
}
uint16_t Predictor::getState() { return state; }

#ifdef  PREDICTOR_MAIN // for testing
int main() {
  printf("using lexicon of %d words\n", (int) NUM_WORDS);
  Predictor p;                                    // *** usage example ***

  setbuf(stdout, NULL);
  setbuf(stdin, NULL);

  char c; 
  while(true) {
    p.print();                                    // *** usage example ***
    printf("input a symbol:\n");
    c = getchar();
    if(c == '\n') {
      printf("no input\n");
      continue;
    } else if(c == 'q') { // quit
      printf("quitting\n");
      exit(0);
    } else {
      getchar();
    }

    const char *selection = NULL;
    switch(c) {
      case ' ': // space or...
      case '0': // ...zero: select first candidate
        selection = p.first();                    // *** usage example ***
        if(selection)
          printf("selecting word %s\n", selection);
        else
          printf(
            "no word to select from state %d\n",
            p.getState()                          // *** usage example ***
          );
        p.reset();                                // *** usage example ***
        break;
      default:  // 1-9
        if(p.suggest(c - '0') >= 0) {             // *** usage example ***
          printf("suggestions: ");
          const char *suggested;
          while( (suggested = p.next()) != NULL ) // *** usage example ***
            printf("%s ", suggested);
          printf("\n");
        }
    }

    printf("\n");
  }

  return 0;
}
#endif // PREDICTOR_MAIN
#endif // UNPHONE_UI0 == 1
//////////////////////////////////////////////////////////////////////////////
