/****************************************************************************
Adafruit GPS displayer v 1.0
Adapted from sample code by Allan Bowhill, 2020
Free for public use

For: PyGamer Board + GPS Feather Wing 1.8" 

To build, see:  https://learn.adafruit.com/adafruit-pygamer/arduino-libraries
Make sure to include other libraries as needed and delete duplicates 
from your ArduinoData folder if compipler complains about it.

Tested and developed using Deviot module for Sublime Text 3 and Platform.IO
****************************************************************************/

#include <Adafruit_Arcada.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_GPS.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_EPD.h>
#include <Adafruit_ZeroDMA.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <SPI.h>

// set to true to show copious serial output during development
#define DEBUG false

// Define GPS struct as link to its own serial connection
// no copy constuctor in this class, and only 1 instance allowed, so 
// we leave sleeping dogs lie and use the global GPS.xxx in all the classes
#define GPSSerial Serial1
Adafruit_GPS GPS(&GPSSerial);
#define GPSECHO false

// Define arcada library datatype
Adafruit_Arcada arcada;
extern Adafruit_SPIFlash Arcada_QSPI_Flash;

// event callback variable, populates every loop cycle
sensors_event_t event; 

const float pi = 3.14159265;

uint32_t buttons;
uint8_t j = 0;  // neopixel counter for rainbow

#define TFT_CS        44 // PyBadge/PyGamer display control pins: chip select
#define TFT_RST       46 // Display reset
#define TFT_DC        45 // Display data/command select
#define TFT_BACKLIGHT 47 // Display backlight pin

// https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7735.cpp
// OPTION 1 (recommended) is to use the HARDWARE SPI pins, which are unique
// to each board and not reassignable.
Adafruit_ST7735 tft = Adafruit_ST7735(&SPI1, TFT_CS, TFT_DC, TFT_RST);

// Check the timer callback, this function is called every millisecond!
// milliseconds is a 16-bit 255 to 0 countdown timer value
volatile uint8_t milliseconds = 0;

void timercallback() 
  {
  GPS.read(); // read new chars (one at a time) into GPS struct
  if (milliseconds == 0) 
    {
    milliseconds = 255;
    } 
  else 
    {
    milliseconds--;
    }
  }

// This is a 32-bit timer for less frequent display intervals
// needed to prevent display flicker and reduce data display frequency
uint32_t timer = millis();

// Free RAM check function
extern "C" char *sbrk(int i);
int FreeRam() 
  {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
  }

// GPS parser
void updateGPS()
  {
  // If we have collected a GPS sentence, process it.
  if (GPS.newNMEAreceived())
    {
    // Parse sentences that can be parsed: PMTK_SET_NMEA_OUTPUT_RMCGGA 
    // (RMC and GGA sentences)
    // this also sets newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) 
      {
      Serial.print(F("Coudln't Parse "));
      Serial.print(GPS.lastNMEA());
      return; // skip until we get a good sentence. 
      // most faulty sentences occur while serial baud rate is syncing 
      // shortly after boot
      }
    }
  }

/*************************************************************************/
/* TemplateDisplay class: Helper methods for displaying data on lines of */
/* display. This supports the main display                               */
/*************************************************************************/

class TemplatedDisplay
  {
  Adafruit_Arcada & arcada;
  
  public:

  TemplatedDisplay(Adafruit_Arcada & a) : arcada(a) { }
  ~TemplatedDisplay() {}

  void print(
    uint8_t upper_left_x, 
    uint8_t upper_left_y, 
    uint8_t pixel_cols, 
    uint8_t pixel_lines, 
    uint16_t color_bg, 
    uint16_t color_fg, 
    String display_string)
    {
    arcada.display->fillRect(upper_left_x, upper_left_y, pixel_cols, 
      pixel_lines, color_bg);
    arcada.display->setCursor(upper_left_x, upper_left_y);
    arcada.display->setTextColor(color_fg);
    arcada.display->print(display_string);
    arcada.display->print(" ");
    }

  void amend(
    uint8_t upper_left_x, 
    uint8_t upper_left_y, 
    uint16_t color_fg, 
    String display_string)
    {
    arcada.display->setCursor(upper_left_x, upper_left_y);
    arcada.display->setTextColor(color_fg);
    arcada.display->print(display_string);
    arcada.display->print(" ");
    }
  };

TemplatedDisplay tft_line = TemplatedDisplay(arcada);


/**********************************************************************/
/* SerialDisplay class: Writes GPS diagnostic messages to serial port */
/* depending on GPS fix status.                                       */
/**********************************************************************/

class SerialDisplay
  {
  public:
    // Adafruit_GPS has no default constructor so we use initializer list
    SerialDisplay() { }

    ~SerialDisplay() { }

    // shows data after a satellite fix
    void show_fix_data_serial(void)
      {
      Serial.print(F("Location: "));
      Serial.print(GPS.latitude, 4); 
      Serial.print(GPS.lat);
      Serial.print(", ");
      Serial.print(GPS.longitude, 4); 
      Serial.println(GPS.lon);
      Serial.print("Speed (mph): "); 
      Serial.println(GPS.speed * 1.15078);
      Serial.print("Speed (kph): "); 
      Serial.println(GPS.speed * 1.852);
      Serial.print("Speed (m/s): "); 
      Serial.println(GPS.speed * 0.514);
      Serial.print("Angle: "); 
      Serial.println(GPS.angle);
      Serial.print("Altitude: "); 
      Serial.println(GPS.altitude);
      Serial.print("Satellites: "); 
      Serial.println((int)GPS.satellites);
      }

    // shows pre-fix satellite data
    void show_active_data_serial(void)
      {
      Serial.print("\nTime: ");
        
      if (GPS.hour < 10) { Serial.print('0'); }
      Serial.print(GPS.hour, DEC); 
      Serial.print(':');
      
      if (GPS.minute < 10) { Serial.print('0'); }
      Serial.print(GPS.minute, DEC); 
      Serial.print(':');
      
      if (GPS.seconds < 10) { Serial.print('0'); }
      Serial.print(GPS.seconds, DEC);
      Serial.print(' ');
      
      Serial.print("Date: ");
      Serial.print(GPS.day, DEC); 
      Serial.print('/');
      Serial.print(GPS.month, DEC); 
      Serial.print("/20");
      Serial.println(GPS.year, DEC);
      
      Serial.print("Fix: "); 
      Serial.print((int)GPS.fix);
      
      Serial.print(" quality: "); 
      Serial.println((int)GPS.fixquality);
      }
  };

SerialDisplay serial_display = SerialDisplay();


/***************************************************************************/ 
/* BackLight class: Dims / raises display backlight based on ambient lignt */
/* does this gradually and smoothly, so it's hardly noticable.             */
/*                                                                         */
/* Ambient light reading 0-1023                                            */
/* Screen brightness: 0-255                                                */
/***************************************************************************/

#define ARYSZ 10

class BackLight
  {
  Adafruit_Arcada & arcada;   // Arcada library reference
  uint16_t backlight_minimum; // minimum acceptable screen brightness
  uint16_t current;           // current light reading
  uint16_t que[ARYSZ];        // array of ambient light readings

  public:

    // Constructor: initialize object with arcada instance.
    //
    // Scenario:
    //    BackLight a(arcada);
    //

    BackLight(Adafruit_Arcada & arc) : 
      arcada(arc), backlight_minimum(25), current(0)
      { 
      for(int i = 0; i < ARYSZ; ++i)
        {
        que[i] = 0;
        }
      }

    // Post-constructor: fully initialize object
    // returns reference to this object

    BackLight & post_init()
      {
      check();
      return *this;
      }

    // Copy Constructor
    //
    // Instantiate a new BackLight instance from an existing one.
    // Deep copies data members from one existing BackLight instance new one.
    //
    // Scenario:
    //   BackLight a(arcada);  
    //   BackLight b(a);  // Copy constructor called
    //   BackLight c = a; // Copy constructor called
    //

    BackLight(const BackLight & rhs) : arcada(rhs.arcada), 
      backlight_minimum( rhs.backlight_minimum ), 
      current (rhs.current)
      {
      for(int i = 0; i < ARYSZ; ++i)
        {
        que[i] = rhs.que[i];
        }
      }

    // Copy Assignment operator
    //
    // Deep copy data members from one existing BackLight instance to another
    // existing BackLight instance. In case of attempt at self-assignment, 
    // pass a reference of this instance through to next item in the chain.
    //
    // Scenarios:
    //
    //   BackLight a(arcada).post_init(); 
    //   BackLight d(arcada);
    //   d = a;     // Copy Assignment operator called, data copy made.
    //   a = a;     // Copy Assignment operator called but no data copy made.
    
    BackLight & operator=(const BackLight & rhs)
      {
      if (this != &rhs)
        {
        arcada = rhs.arcada;
        backlight_minimum = rhs.backlight_minimum;
        current = rhs.current;
      
        for(int i = 0; i < ARYSZ; ++i)
          {
          que[i] = rhs.que[i];
          }
        }

      return *this;
      }

    // Destructor: clean up open persistent data

    ~BackLight() 
      {
      }

    // print array of light values to Serial for diagnostics

    void show()
      {
      Serial.print("[");
      for(int i = 0; i < ARYSZ; ++i)
        {
        Serial.print(que[i]);
        Serial.print(',');
        }
      Serial.println("]");
      }

    // set backlight proportionally averaged to ambient light to save power
    // return average of all readings to prevent erratic changes in brightness

    uint16_t update_board()
      {
      check();
      if (DEBUG) show();

      uint16_t total = 0;

      // shift que left, popping lhs element off end, freeing rhs slot
      for(int i = 0; i < ARYSZ-1; i++)
        {
        que[i] = que[i+1];
        total += que[i];   // while we're at it, tally total so far
        }

      // set last item in que to current sensor reading
      que[ARYSZ-1] = current;

      // take average (mean) of que items (including current)
      uint16_t avg = (total + current) / ARYSZ;

      // remap light sensor's 0-1023 to screen's brightness 0-254
      // (1024.00 coerces division to occur, required)
      uint16_t converted_avg = (avg / 1024.00) * 255;

      // if converted average is below desired minimum brightness,
      // use the minimum. Otherwise, use the converted average to set
      // screen brightness.

      if (converted_avg <= backlight_minimum)
        write(backlight_minimum);
      else
        write(converted_avg);

      return converted_avg;
      }


  private:

    // read from light sensor library
    // returns 0 (darkest) to 1023 (brightest) or 0 if there is no sensor

    uint16_t read()
      {
      return arcada.readLightSensor();
      }

    // set backlight to input value.
    // return current value from board.

    uint16_t write(uint16_t lite)
      {
      arcada.setBacklight(lite);
      return read();
      }

    // update this object's current reading with backlight sensor value
    // return current value
    
    uint16_t check()
      {
      current = read();
      return current;
      }
  };

BackLight backlight = BackLight(arcada).post_init();


/*******************************************************************/
/* DateTime class: provides UTC date and time based on GPS reading */
/* 
/* uses GPS global instance: Stateless!
/*******************************************************************/

class DateTime
  {
  public:
    DateTime()  { };
    ~DateTime() { };

    String get_the_time()
      {
      String the_time;
      String zero = String('0');
      String colon = String(':');

      if (GPS.hour < 10) 
        {
        the_time += zero;
        }
      the_time += String(GPS.hour, DEC); 
      the_time += colon;

      if (GPS.minute < 10) 
        {
        the_time += zero; 
        }
      the_time += String(GPS.minute, DEC); 
      the_time += colon;
      
      if (GPS.seconds < 10) 
        {
        the_time += zero; 
        }
      the_time += String(GPS.seconds, DEC);
      the_time += String(" UTC");

      return the_time;
      }

    String get_date()
      {
      String zero = String('0');

      String month    = String(GPS.month, DEC);
      String day      = String(GPS.day, DEC);
      String year_pfx = String("-20");
      String year     = String(GPS.year, DEC);

      String date = month;
      date += '-';
      date += day;
      date += year_pfx;
      date += year;
      
      return date;
      }
  };

DateTime timeline = DateTime();

/**************************************************/ 
/* Compass class: Displays heading on circle      */
/*                                                */
/* Compass 'rose' is a circle fixed at North (up) */
/* Heading indicator is ball whose position moves */
/* somewhere on that circle, representing angle   */
/* with respect to North you are heading.         */
/**************************************************/ 

class Compass
  {
  // settings for compass display
  int compass_radius;      // 'rose' circle radius
  int compass_x_offset;    // x-pos of cirle center
  int compass_y_offset;    // y-pos of circle center
  int compass_adjustment;  // offset in degrees for indicator ball
  
  // x,y position of indicator ball on compass circle
  float indicator_x;  // current x-pos of indicator ball
  float indicator_y;  // current y-pos of indicator ball

  public:

    // Constructor
    //
    // Constructs Compass object
    //
    // Usage:
    //   Compass c;               <-  use defaults, lower-left part of screen
    //   Compass c(30,30,80,-76); <- -76 degrees offset tweak to adjust
    //                                indicator ball position calibration
    //   

    Compass(
      int radius = 20, 
      int x_offset = 25, 
      int y_offset = 105, 
      int adjustmant = -76) :
        compass_radius(radius), 
        compass_x_offset(x_offset), 
        compass_y_offset(y_offset), 
        compass_adjustment(adjustmant),
        indicator_x(0.0),
        indicator_y(0.0)
        { }

    // Destructor
    ~Compass() {}

    // Draws heading indicator ball on circle, corrresponding to degrees
    void draw_heading_indicator()
      {
      // blot out last ball draw
      arcada.display->fillCircle(indicator_x, indicator_y, 3, ARCADA_BLACK);
      
      // draw new circle
      arcada.display->drawCircle(
        compass_x_offset, 
        compass_y_offset, 
        compass_radius, 
        ARCADA_YELLOW
        );
      
      // Draw white ball on heading circle
      // http://setosa.io/ev/sine-and-cosine/
      // theta is in radians, X/Y are polar coordinates
      float theta = 360 - (GPS.angle + compass_adjustment) * (pi / 180);
      indicator_x = compass_radius * sin(theta) + compass_x_offset;
      indicator_y = compass_radius * cos(theta) + compass_y_offset;
      arcada.display->fillCircle(indicator_x, indicator_y, 3, ARCADA_WHITE);
      }
  };

Compass diagnostic_compass = Compass(20,25,105,-76);


/*****************************************/
/* Unimplemented "features"              */
/*****************************************/

// Pretty, but just drains power

void update_neopixels()
  {
   /*
  for(int32_t i=0; i< arcada.pixels.numPixels(); i++) 
    {
    arcada.pixels.setPixelColor(i, Wheel(((i * 256 / arcada.pixels.numPixels()) + j*5) & 255));
    }
  arcada.pixels.show();
  j++;
  */
  }

// tap reading doesn't work very well, and it could pop the battery out of 
// the GPS wing in the back.

void read_taps()
  {
  /*
  bool playsound = false;
  uint8_t click = arcada.accel.getClick();
  if (click & 0x30) {
    Serial.print("Click detected (0x"); Serial.print(click, HEX); Serial.print("): ");
    if (click & 0x10) Serial.print(" single click");
    if (click & 0x20) Serial.print(" double click");
    //playsound = true;
    }
  */
  }


// Unused, can be activated by uncommenting update_neopixels(), but drains 
// power.
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.

uint32_t Wheel(byte WheelPos) 
  {
  if(WheelPos < 85) {
   return arcada.pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) 
    {
    WheelPos -= 85;
    return arcada.pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } 
  else 
    {
    WheelPos -= 170;
    return arcada.pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
  }

/***********************************************/
/* Loose Contract class: Parent to all screens */
/***********************************************/

class Screen
  {
  public:

    virtual ~Screen() = default;
    virtual void transition_screen() 
      {
      arcada.display->fillScreen(ARCADA_BLACK);
      }

    virtual void clear_screen() {}
    virtual void render() {}
  };

Screen * screen_ptr;

/************************************************/
/* MPH screen class: Manages MPH display screen */
/************************************************/

class MPH : public Screen
  {
  Adafruit_Arcada & arcada;
  Adafruit_ST7735 & tft;

  public:

    MPH(Adafruit_Arcada & arc, Adafruit_ST7735 & tf) :
      arcada(arc), tft(tf) 
      {}

    void transition_screen(void)
      {
      arcada.display->fillScreen(ARCADA_BLUE);
      }

    void clear_screen(void)
      {
      // arcada.display->fillRect(upper_left_x, upper_left_y, pixel_cols, 
      // pixel_lines, color_bg);
      // start filling from the far left because extra digits displayed while 
      // motionless will not show until we are moving. Clearing only the 
      // numeric part of the readout to reduce flicker.
      arcada.display->fillRect(0, 10, 160, 50, ARCADA_NAVY);
      }

    void render(void)
      {
      tft.setFont(&FreeSansBold24pt7b);
      tft.setCursor(10, 50);
      tft.setTextWrap(false);
      tft.setTextColor(ST7735_YELLOW);
      tft.setTextSize(1); // 1,2 or 3
      tft.println(GPS.speed * 1.15078);

      tft.setFont(&FreeSansBold18pt7b);
      tft.setCursor(10, 115);
      tft.setTextWrap(false);
      tft.setTextColor(ST7735_YELLOW);
      tft.setTextSize(1); // 1,2 or 3
      tft.println("MPH");
      }

    String name()
      {
      return "MPH";
      }

  };

MPH mph = MPH(arcada, tft);

/************************************************/
/* KPH screen class: Manages KPH display screen */
/************************************************/

class KPH : public Screen
  {
  Adafruit_Arcada & arcada;
  Adafruit_ST7735 & tft;

  public:

    KPH(Adafruit_Arcada & arc, Adafruit_ST7735 & tf) :
      arcada(arc), tft(tf) 
      {}

    void transition_screen(void)
      {
      arcada.display->fillScreen(ARCADA_GREEN);
      }

    void clear_screen(void)
      {
      // arcada.display->fillRect(upper_left_x, upper_left_y, pixel_cols, 
      // pixel_lines, color_bg);
      // start filling from the far left because extra digits displayed while 
      // motionless will not show until we are moving. Clearing only the 
      // numeric part of the readout to reduce flicker.
      arcada.display->fillRect(0, 10, 160, 50, ARCADA_YELLOW);
      }

    void render(void)
      {
      tft.setFont(&FreeSansBold24pt7b);
      tft.setCursor(10, 50);
      tft.setTextWrap(false);
      tft.setTextColor(ST7735_BLUE);
      tft.setTextSize(1); // 1,2 or 3
      tft.println(GPS.speed * 1.852);

      tft.setFont(&FreeSansBold18pt7b);
      tft.setCursor(10, 115);
      tft.setTextWrap(false);
      tft.setTextColor(ST7735_BLUE);
      tft.setTextSize(1); // 1,2 or 3
      tft.println("KPH");
      }

    String name()
      {
      return "KPH";
      }
  };

KPH kph = KPH(arcada, tft);

/*****************************************************/
/* ALTitude screen class: Manages ALT display screen */
/****************************************************/

class ALT : public Screen
  {
  Adafruit_Arcada & arcada;
  Adafruit_ST7735 & tft;

  public:

    ALT(Adafruit_Arcada & arc, Adafruit_ST7735 & tf) :
      arcada(arc), tft(tf) 
      {}

    void transition_screen(void)
      {
      arcada.display->fillScreen(ARCADA_WHITE);
      }

    void clear_screen(void)
      {
      // start filling from the far left because extra digits displayed while 
      // motionless will not show until we are moving. Clearing only the 
      // numeric part of the readout to reduce flicker.
      arcada.display->fillRect(0, 10, 160, 50, ARCADA_RED);
      }

    void render(void)
      {
      tft.setFont(&FreeSansBold24pt7b);
      tft.setCursor(10, 50);
      tft.setTextWrap(false);
      tft.setTextColor(ARCADA_BLACK);
      tft.setTextSize(1); // 1,2 or 3
      tft.println(GPS.altitude);

      tft.setFont(&FreeSansBold18pt7b);
      tft.setCursor(10,115);
      tft.setTextWrap(false);
      tft.setTextColor(ARCADA_BLACK);
      tft.setTextSize(1); // 1,2 or 3
      tft.println("ALT");
      }

    String name()
      {
      return "ALT";
      }

  };

ALT alt = ALT(arcada, tft);

/************************************************/
/* HDG screen class: Manages HDG display screen */
/************************************************/

class HDG : public Screen
  {
  Adafruit_Arcada & arcada;
  Adafruit_ST7735 & tft;

  public:

    HDG(Adafruit_Arcada & arc, Adafruit_ST7735 & tf) :
      arcada(arc), tft(tf) 
      {}

    void transition_screen(void)
      {
      arcada.display->fillScreen(ARCADA_WHITE);
      }

    void clear_screen(void)
      {
      arcada.display->fillRect(0, 10, 160, 50, ARCADA_NAVY);
      }

    void render(void)
      {
      tft.setFont(&FreeSansBold24pt7b);
      tft.setCursor(10, 50);
      tft.setTextWrap(false);
      tft.setTextColor(ST7735_GREEN);
      tft.setTextSize(1); // 1,2 or 3
      tft.println(GPS.angle);

      tft.setFont(&FreeSansBold18pt7b);
      tft.setCursor(10, 115);
      tft.setTextWrap(false);
      tft.setTextColor(ST7735_GREEN);
      tft.setTextSize(1); // 1,2 or 3
      tft.println("HDG");
      }

    String name()
      {
      return "HDG";
      }

  };

HDG hdg = HDG(arcada, tft);

/****************************************************/
/* DEFAULT screen class: Default screen display     */
/****************************************************/

class Default : public Screen
  {
  Adafruit_Arcada & arcada;
  Adafruit_ST7735 & tft;

  public:

    // constructor

    Default(Adafruit_Arcada & arc, Adafruit_ST7735 & tf) :
      arcada(arc), tft(tf) 
      {}

    // This method is called once before any display is attempted for this
    // screen. It sets background, and clears out graphics from last screen.

    void transition_screen(void)
      {
      arcada.display->fillScreen(ARCADA_BLACK);
      }

    // This method clears only part of the display that contains dynamic data

    void clear_screen(void)
      {
      // arcada.display->fillRect(upper_left_x, upper_left_y, pixel_cols, 
      // pixel_lines, color_bg);
      // start filling from the far left because extra digits displayed while 
      // motionless will not show until we are moving. Clearing only the 
      // numeric part of the readout to reduce flicker.
      arcada.display->fillRect(0, 10, 160, 50, ARCADA_BLACK);
      }

    // override of parent Screen class: renders data for this screen

    void render(void)
      {
      tft.setFont(&FreeSansBold24pt7b);
      tft.setCursor(10, 50);
      tft.setTextWrap(false);
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1); // 1,2 or 3
      tft.println("0");

      tft.setFont(&FreeSansBold18pt7b);
      tft.setCursor(10, 115);
      tft.setTextWrap(false);
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1); // 1,2 or 3
      tft.println("Default");
      }

    // reports name of screen for diagnostics

    String name()
      {
      return "Default";
      }

  };

Default def = Default(arcada, tft);

/**************************************************/
/* DiagnosticScreen class: Main diagnostic screen */
/* Jam-packed full of information in tiny print   */
/*                                                */
/* - UTC Date and Time from GPS                   */
/* - Ambient light reading                        */
/* - Battery charge (voltage) level reading       */
/* - Acceleromer (x,y,z) readings                 */
/* - Free memory reading                          */
/* - Joystick position, button press detection    */
/* - GPS fix status (1 = have fix on satellites)  */
/* - GPS signal quality (1 or 2, 2 being high)    */
/* - GPS Satellite count                          */
/* - Latitude reading                             */
/* - Longitude reading                            */
/* - Velocity in MPH                              */
/* - Velocity in KPH                              */
/* - Velocity in m/s                              */
/* - Heading in degrees                           */
/* - Altitude in meters                           */
/* - Graphical heading compass                    */
/*                                                */
/* All data onscreen updated once per second      */
/* Screen brightness adjusted to ambient light    */
/*                                                */
/*                                                */
/**************************************************/

class DiagnosticScreen : public Screen 
  {
  public:

    Adafruit_Arcada & arcada;     // arcada library reference
    TemplatedDisplay & tft_line;  // helper class to print a line on display
    BackLight & backlight;        // class to control display backlight
    DateTime & timeline;          // time/date class 
    boolean debug;                // value of DEBUG flag

    // constructor

    DiagnosticScreen (
      Adafruit_Arcada & a, 
      TemplatedDisplay & t, 
      BackLight & b, 
      DateTime tl, boolean d
      ) : arcada(a), tft_line(t), backlight(b), timeline(tl), debug(d)  
        { }

    // reports name of current screen

    String name()
      {
      return "DiagnosticScreen";
      }

    // Overridden from parent Screen class, but nothing here.
    // This is because all graphical clearing of dynamic data display areas
    // are done inline below.

    void clear_screen(void)
      {
      }

    // Overridden from parent Screen class. 
    // Renders the display for this screen.

    void render()
      {
      // don't send debug data to serial unless it's dev activity
      if (DEBUG) serial_display.show_active_data_serial();

      // Display line 2: Read light sensor
      tft_line.print(0, 10, 160, 8, ARCADA_NAVY, ARCADA_YELLOW, "Light:");
      arcada.display->print(arcada.readLightSensor());
  
      // Line 2: Read battery
      tft_line.amend(90, 10, ARCADA_YELLOW, "Batt:");
      arcada.display->print(arcada.readBatterySensor()); 
      arcada.display->println("V");

      // Line 3: show accelarator data
      tft_line.print(0, 20, 160, 8, ARCADA_NAVY, ARCADA_YELLOW, "X:");
      arcada.display->print(event.acceleration.x, 1);
      tft_line.amend(60, 20, ARCADA_YELLOW, "Y:");
      arcada.display->print(event.acceleration.y, 1);
      tft_line.amend(115, 20, ARCADA_YELLOW, "Z:");
      arcada.display->print(event.acceleration.z, 1);

      // Line 4: memory
      tft_line.print(0, 30, 160, 8, ARCADA_NAVY,ARCADA_YELLOW,"Free Memory:");
      arcada.display->print(FreeRam());
    
      // Line 6: Show GPS quality data
      tft_line.print(0, 50, 160, 8, ARCADA_BLUE, ARCADA_WHITE, "Fix:");
      arcada.display->print((int)GPS.fix);
      tft_line.amend(55, 50, ARCADA_WHITE, "Qual:");
      arcada.display->println((int)GPS.fixquality);
      tft_line.amend(114, 50, ARCADA_WHITE, "SAT:");
      arcada.display->println((int)GPS.satellites);

      // if the GPS has a fix on satellite data
      if (GPS.fix) 
        {
        if (DEBUG) serial_display.show_fix_data_serial();

        // draws compass and heading indicator ball
        diagnostic_compass.draw_heading_indicator();

        // Line 1: set colors for top line of display
        tft_line.print(0, 0, 160, 8, ARCADA_NAVY, ARCADA_YELLOW, 
          timeline.get_date());
        tft_line.amend(85, 0, ARCADA_YELLOW, timeline.get_the_time());

        // Line 7 & 8: display LAT and LONG
        tft_line.print(0, 60, 160, 8, ARCADA_BLUE, ARCADA_WHITE, "LAT:");
        arcada.display->print(GPS.latitude,4);
        arcada.display->print(GPS.lat);
        tft_line.print(0, 70, 160, 8, ARCADA_BLUE, ARCADA_WHITE, "LONG:");
        arcada.display->print(GPS.longitude,4);
        arcada.display->print(GPS.lon);

        // Line 9-11: display VELOCITY in various scales
        tft_line.print(60, 80, 160, 8, ARCADA_RED, ARCADA_WHITE, "MPH:");
        arcada.display->print(GPS.speed * 1.15078);
      
        tft_line.print(60, 90, 160, 8, ARCADA_RED, ARCADA_WHITE, "KPH:");
        arcada.display->print(GPS.speed * 1.852);
      
        tft_line.print(60, 100, 160, 8, ARCADA_RED, ARCADA_WHITE, "m/s:");
        arcada.display->print(GPS.speed * 0.514);

        // Line 12 & 13: display HEADING and ALTITUDE
        tft_line.print(60, 110, 160, 8, ARCADA_RED, ARCADA_WHITE, "HDG:");
        arcada.display->print(GPS.angle);
      
        tft_line.print(60, 120, 160, 8, ARCADA_RED, ARCADA_WHITE, "ALT:");
        arcada.display->print(GPS.altitude);
        } // gps fix
      } // render
  };

DiagnosticScreen diag = 
  DiagnosticScreen(arcada, tft_line, backlight, timeline, DEBUG);

/*****************************************************/
/* ScreenGrid: Manages a 3 x 3 grid of all 9 screens */
/*****************************************************/

class ScreenGrid
  {
  static const uint8_t col_max = 3;
  static const uint8_t row_max = 3;

  Screen * screens[col_max][row_max];
  uint8_t current_row;
  uint8_t current_col;

  public:
    ScreenGrid(Screen & home) : current_row(1), current_col(1)
      {
      screens[1][1] = &home;
      }

    // copy constructor
    ScreenGrid(const ScreenGrid & rhs) :
      current_row(rhs.current_row),
      current_col(rhs.current_col)
      { 
      for (int row=0; row < row_max; row++)
        {
        for (int col=0; col < col_max; col++)
          {
          screens[row][col] = rhs.screens[row][col];
          }
        }
      }

    // copy assignment operator
    ScreenGrid & operator=(ScreenGrid & rhs)
      {
      if (this != &rhs)
        {
        current_row = rhs.current_row;
        current_col = rhs.current_col;
        
        for (int row=0; row < row_max; row++)
          {
          for (int col=0; col < col_max; col++)
            {
            screens[row][col] = rhs.screens[row][col];
            }
          }
        }
      return *this;
      }

    // destructcor
    ~ScreenGrid()
      {
      current_row = 0;
      current_col = 0;
      for (int row = 0; row < row_max; row++)
        {
        for (int col = 0; col < col_max; col++)
          {
          delete screens[row][col];
          }
        }      
      }

    // Add a screen object to a 3x3 grid location
    
    void insert(Screen & screen, uint8_t row, uint8_t col)
      {
      screens[row][col] = &screen;
      }

    // In current screen object, clear screen portion with dynamic data 
    
    void clear_current_screen()
      {
      screens[current_row][current_col]->clear_screen();
      }

    // in the current screen object, reset whole screen to erase last screen
    // because user chose another screen to display, and we are transitioning
    // to it

    void transition_current_screen()
      {
      screens[current_row][current_col]->transition_screen();
      }

    // sets current screen grid location as the one to display

    void set_current_screen(uint8_t row, uint8_t col)
      {
      current_row = row;
      current_col = col;
      }

    // returns current screen pointer to abstract Screen class, allowing
    // underlying screen derived class's methods to be called generically, 
    // like: ptr->method(); <- ploymorphic call

    Screen * get_current_screen()
      {
      return screens[current_row][current_col];
      }

    // return x-pos in grid of current screen
    
    uint8_t get_x()
      {
      return current_col;
      }

    // return y-pos in grid of current screen
    
    uint8_t get_y()
      {
      return current_row;
      }

    // return specified screen pointer from given row in grid

    Screen * get_screen(uint8_t row, uint8_t col)
      {
      return screens[row][col];
      }

    // change screen accordingly for an up-button press
    // resulting in a new screen to be displayed from grid

    void up()
      {
      if (current_row == 0)
        current_row = (row_max - 1);
      else
        current_row--;

      transition_current_screen();
      }

    // change screen accordingly for a down-button press

    void down()
      {
      if ( current_row == (row_max-1) )
        current_row = 0;
      else
        current_row++;

      transition_current_screen();
      }

    // change screen accordingly for left-button press

    void left()
      {
      if (current_col == 0)
        current_col = (col_max - 1);
      else
        current_col--;

      transition_current_screen();
      }

    // change screen accordingly for right-button press

    void right()
      {
      if ( current_col == (col_max - 1) )
        current_col = 0;
      else
        current_col++;

      transition_current_screen();
      }

    // diagnostic printout of all screen pointers in grid

    void print_grid()
      {
      for (int row=0; row < row_max; row++)
        {
        for (int col=0; col < col_max; col++)
          {
          Serial.print("[");
          Serial.print(row);
          Serial.print("]");
          Serial.print("[");
          Serial.print(col);
          Serial.print("] = ");

          screen_ptr = get_screen(row,col);

          Serial.println(int(screen_ptr), HEX);
          }
        }
      }
  };

ScreenGrid grid(diag);

/***************************************************/
/* ButtonController class dispatches button clicks */
/* to ScreenGrid.                                  */
/***************************************************/

enum class Direction : uint8_t { 
  UP    = 0,
  DOWN  = 1,
  LEFT  = 2,
  RIGHT = 4,
  NONE  = 5
};

class ButtonController
  {  
  uint8_t pressed_buttons;  // codes for buttons pressed
  Adafruit_Arcada arcada;   // graphics library
  ScreenGrid & screen_grid; // screen grid object
  Direction last_direction; // enum of last direction button clicked

  public:

  // constructor

  ButtonController(Adafruit_Arcada & arc, ScreenGrid & grid) :
    arcada(arc), 
    screen_grid(grid), 
    last_direction(Direction::NONE)
    {  }

  // destructor

  ~ButtonController() 
    { }
  
  // copy constructor

  ButtonController(const ButtonController & rhs) : 
    arcada(rhs.arcada),
    screen_grid(rhs.screen_grid),
    last_direction(rhs.last_direction)    
    { }

  // copy assignment operator

  ButtonController & operator=(ButtonController & rhs)
    {
    if (this != &rhs)
      {
      arcada = rhs.arcada;
      screen_grid = rhs.screen_grid;
      last_direction = rhs.last_direction;   
      }
    return *this;
    }

  // main update dispatcher for button presses.
  // reads for button presses, checks what was 
  // presssed if it finds something. This is called
  // periodically every second or so by main loop.

  void update()
    {
    pressed_buttons = arcada.readButtons();
    if ( pressed_buttons )
      check();
    }

  // determine which button was pressed, and change 
  // current_screen in ScreenGrid accordingly for 
  // up, down, left or right. Records last direction
  // selected, to prevent repeat keypresses. This value
  // is cleared-out every second or so by the main loop.

  void check()
    {
    if (pressed_buttons & ARCADA_BUTTONMASK_A) 
      {
      if (last_direction != Direction::UP)
        {
        screen_grid.up();
        last_direction = Direction::UP;
        }
      }
      
    if (pressed_buttons & ARCADA_BUTTONMASK_B) 
      {
      if (last_direction != Direction::DOWN)
        {
        screen_grid.down();
        last_direction = Direction::DOWN;
        }
       }
         
    if (pressed_buttons & ARCADA_BUTTONMASK_START) 
      {
      if (last_direction != Direction::RIGHT)
        {
        screen_grid.right();
        last_direction = Direction::RIGHT;
        }
      }

    if (pressed_buttons & ARCADA_BUTTONMASK_SELECT) 
      {
      if (last_direction != Direction::LEFT)
        {
        screen_grid.left();
        last_direction = Direction::LEFT;
        }
      }
    }

  // called by main loop every second or so to allow another button
  // press to activate a screen change.

  void clear()
    {
    last_direction = Direction::NONE;
    }
  };

ButtonController button_controller = ButtonController(arcada, grid);


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
void setup() 
  {
  // Systems check and setup of board components
  
  // Init serial console
  Serial.begin(115200);
  //while (!Serial);
  
  // Init GPIO, NeoPixels, Speaker
  if (!arcada.arcadaBegin()) 
    {
    Serial.println(F("Arcada library failed to initialize board."));
    while (1);
    }

  Serial.print(F("Free RAM: "));
  Serial.println(FreeRam());

  // Chack storage of const, in this case, what we defined as pi
  // If the address is $2000000 or larger, its in SRAM. 
  // If the address is between $0000 and $3FFFF Then it is in FLASH
  Serial.print(F("Address of pi $")); 
  Serial.println((int)&pi, HEX);
    
  // Init display
  arcada.displayBegin();
  Serial.println(F("Arcada display initialized."));

  // Bring up backlight incrementally
  arcada.display->fillScreen(ARCADA_BLUE);
  for (int i=0; i<150; i++) 
    {
    arcada.setBacklight(i);
    delay(1);
    }
  Serial.print(F("Backlight initialized: "));
  Serial.println(arcada.getBacklight());

  arcada.display->setTextWrap(true);

  // Check for presence built-in flash drive
  if (!Arcada_QSPI_Flash.begin())
    {
    Serial.println(F("Could not find flash on QSPI bus!"));
    arcada.display->setTextColor(ARCADA_RED);
    arcada.display->println(F("QSPI Flash FAIL"));
    while(1);
    }
 
  // Filesystem Check
  Arcada_FilesystemType foundFS = arcada.filesysBegin();
  if (foundFS == ARCADA_FILESYS_NONE) 
    {
    Serial.println(F("Failed to load filesys"));
    while(1);
    }
  else 
    {
    if (foundFS == ARCADA_FILESYS_SD) Serial.print(F("SD: "));
    if (foundFS == ARCADA_FILESYS_QSPI) Serial.print(F("QSPI: "));
    if (foundFS == ARCADA_FILESYS_SD_AND_QSPI) Serial.print(F("SD & QSPI: "));
    Serial.println(F("OK!"));
    Serial.print(arcada.filesysListFiles("/"));
    Serial.println(F(" files found."));
    }

  // Check for presence of LIS3DH accelerometer and initialize it
  if (!arcada.hasAccel()) 
    {
    Serial.println("No accelerometer found");
    while(1);
    } 
  else 
    {
    arcada.accel.setRange(LIS3DH_RANGE_4_G);   // 2, 4, 8 or 16 G!
    arcada.accel.setClick(1, 80);
    Serial.println(F("LIS3DH Accelerometer initialized."));
    }

  // Init GPS serial
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  GPS.sendCommand(PGCMD_NOANTENNA);
  delay(1000);
  GPSSerial.println(PMTK_Q_RELEASE);
  Serial.println(F("GPS initialized."));

  buttons = 0;

  arcada.timerCallback(750, timercallback);
  Serial.print(F("Timer callback frequency set to: "));
  Serial.println(arcada.getTimerCallbackFreq());

  Serial.print(F("Free RAM: "));
  Serial.println(FreeRam());
  
  // More screen setup: cursor position
  arcada.display->setCursor(0, 0);

  // finish initialization of BackLight object.
  backlight.post_init();

  /*********************************************************************/
  /* Assign new screens below, placing Screen instance into 3 x 3 grid */
  /* All screen slots in grid must have a screen assigned. Use default */
  /* screen class if nothing else is defined for a slot.               */
  /*********************************************************************/
  /*   
     Virtual screen layout:
     The starting screen in the middle at 1,1 and is fixed to diagnostics
     screen. Others screens appear at coordinates defined below.

    +-----+-----+-----+
    | 0,0 | 0,1 | 0,2 |
    +-----+-----+-----+
    | 1,0 | 1,1 | 1,2 |
    +-----+-----+-----+
    | 2,0 | 2,1 | 2,2 |
    +-----+-----+-----+

          Adafruit PyGamer Board
    /+-----------------------------+\   Buttons (screen navigation):
    |    joy   +----------+     B1  |    B1 = up
    |   +---+  |          |      o  |    B2 = down
    |   | o |  |  screen  |  B2     |    B3 = right
    |   +---+  |          |   o     |    B4 = left
    |          +----------+         |
    \        B4           B3        /   Joy: unused
     \        o            o       /
      +---------------------------+
  */

  // attach screens to display grid 
  grid.insert(hdg,0,0); // heading readout
  grid.insert(alt,0,1); // altimiter readout
  grid.insert(def,0,2); // (default screen)

  grid.insert(kph,1,0);     // Kilometers Per Second readout
  // grid.insert(diag,1,1); // Diagnostic screen (omit, initialized by class)
  grid.insert(mph,1,2);     // Miles Per Hour readout
  
  grid.insert(def,2,0); // (default screen)
  grid.insert(def,2,1); // (default screen)
  grid.insert(def,2,2); // (default screen)

  // Display splash screen, indicating end of setup phase
  arcada.display->fillScreen(ARCADA_RED);
  delay(100);
  arcada.display->fillScreen(ARCADA_GREEN);
  delay(100);
  arcada.display->fillScreen(ARCADA_BLUE);
  delay(100);
  arcada.display->fillScreen(ARCADA_BLACK);
  delay(100);
  }


void loop() 
  {
  // acceleratorometer events
  arcada.accel.getEvent(&event);

  updateGPS();                  // update global GPS data object
  update_neopixels();           // update neopixels (unimplemented, power)
  button_controller.update();   // update button detectors and act on presses
   
  // if millis() or 32-bit timer wraps around, we'll just reset it
  if (timer > millis()) timer = millis();

  // approximately every second or so, render screen. Eliminates most flicker.
  if (millis() - timer > 1000)
    {    
    button_controller.clear();  // remove repeated keypress
    timer = millis();           // reset the timer
    backlight.update_board();   // adjust dynamic backlight to save power
    screen_ptr = grid.get_current_screen(); // get pointer to current screen
    screen_ptr->clear_screen(); // clear dynamic part of screen (readout)
    screen_ptr->render();       // read and display new value
    }
  }
