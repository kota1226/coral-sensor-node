// unPhoneLoRa.cpp
//
// derived from https://github.com/lnlp/LMIC-node via
// https://github.com/hamishcunningham/LMIC-node autumn 2022
//
// previously adapted from
// https://github.com/mcci-catena/arduino-lmic/tree/master/examples/ttn-otaa-feather-us915
// and later updated from https://github.com/mcci-catena/arduino-lorawan
// for unPhone 2022 by Gareth Coleman & Hamish Cunningham
// API details are here:
// https://github.com/mcci-catena/arduino-lmic/blob/master/doc/LMIC-v4.1.0.pdf
//
// another option: https://github.com/manuelbl/ttn-esp32

#include "unPhone.h"
#if __has_include("private.h")
  #include "private.h"
  // key data: copy these values from TTN, as lsb //////////////////////////////
  // (gareth's, test out v3, unphone-spin7-test) ///////////////////////////////
  // Optional: If DEVICEID is defined it will be used instead of the default defined in the BSF.
  // #define DEVICEID "<deviceid>"
  // Keys required for OTAA activation:
  // End-device Identifier (u1_t[8]) in lsb format
  #define OTAA_DEVEUI _LORA_DEV_EUI
  // Application Identifier (u1_t[8]) in lsb format
  #define OTAA_APPEUI _LORA_APP_EUI
  // Application Key (u1_t[16]) in msb format
  #define OTAA_APPKEY _LORA_APP_KEY
#else
  #define OTAA_DEVEUI 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  #define OTAA_APPEUI 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  #define OTAA_APPKEY 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#endif
char lora_payload[unPhone::LORA_PAYLOAD_LEN];
bool lora_payload_ready = false;

//////////////////////////////////////////////////////////////////////////////
// ttn-lora.cpp //////////////////////////////////////////////////////////////
// single-file version of LMIC-node unphone9-dev branch //////////////////////
// (prior to integration into unPhoneLibrary) ////////////////////////////////
// unPhone specifics (c) Hamish Cunningham 2022
//////////////////////////////////////////////////////////////////////////////
//
// File:         LMIC-node.h
// File:         LMIC-node.cpp
// (and File:    lorawan-keys.h, bsf_unPhone.h)
// Copyright:    Copyright (c) 2021 Leonel Lopes Parente
//               Portions Copyright (c) 2018 Terry Moore, MCCI
// Copyright:    Copyright (c) 2021 Leonel Lopes Parente
//               Copyright (c) 2018 Terry Moore, MCCI
//               Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
//
//               Permission is hereby granted, free of charge, to anyone 
//               obtaining a copy of this document and accompanying files to do, 
//               whatever they want with them without any restriction, including,
//               but not limited to, copying, modification and redistribution.
//               The above copyright notice and this permission notice shall be 
//               included in all copies or substantial portions of the Software.
//
//               THE SOFTWARE IS PROVIDED "AS IS", WITHOUT ANY WARRANTY.
//
// License:      MIT License. See accompanying LICENSE file.

//////////////////////////////////////////////////////////////////////////////
// LMIC-node.h ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include "lmic.h"
#include "hal/hal.h"
#ifdef USE_DISPLAY
    #include <Wire.h>
    #include "U8x8lib.h"
#endif
#ifdef USE_LED
    #include "EasyLed.h"
#endif

enum class InitType { Hardware, PostInitSerial };
enum class PrintTarget { All, Serial, Display };

const dr_t DefaultABPDataRate = DR_SF7;
const s1_t DefaultABPTxPower =  14;

// Forward declarations
static void doWorkCallback(osjob_t* job);
void processWork(ostime_t timestamp);
void processDownlink(ostime_t eventTimestamp, uint8_t fPort, uint8_t* data, uint8_t dataLength);
void onLmicEvent(void *pUserData, ev_t ev);
void displayTxSymbol(bool visible);

#ifndef DO_WORK_INTERVAL_SECONDS            // Should be set in platformio.ini
// HC changed to a min:
//   #define DO_WORK_INTERVAL_SECONDS 300   // Default 5 minutes if not set
     #define DO_WORK_INTERVAL_SECONDS 60    // Default to a min if not set
#endif    

#define TIMESTAMP_WIDTH 12 // Number of columns to display eventtime (zero-padded)
#define MESSAGE_INDENT TIMESTAMP_WIDTH + 3

// Determine which LMIC library is used
#ifdef _LMIC_CONFIG_PRECONDITIONS_H_   
    #define MCCI_LMIC
#else
    #define CLASSIC_LMIC
#endif    

#if !defined(ABP_ACTIVATION) && !defined(OTAA_ACTIVATION)
    #define OTAA_ACTIVATION
#endif

enum class ActivationMode {OTAA, ABP};
#ifdef OTAA_ACTIVATION
    const ActivationMode activationMode = ActivationMode::OTAA;
#else    
    const ActivationMode activationMode = ActivationMode::ABP;
#endif    


/////////////////////////////////////////////////////////////////////////////
// BSF and keys //////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// bsf_unphone9.h ////////////////////////////////////////////////////////////
// 
// Function:     Board Support File for unPhone9 RFM95W LoRa.
// 
// Copyright:    Copyright (c) 2022 Hamish Cunningham
// 
// License:      MIT License. See accompanying LICENSE file.
// 
// Author:       Hamish Cunningham
// 
// Description:  This is an ESP32S3 board with onboard USB (provided by the MCU).
//               It supports automatic firmware upload and serial over USB.
//               Onboard display via HX8357
// 
//               CONNECTIONS AND PIN DEFINITIONS:
//
//               Indentifiers between parentheses are defined in the board's 
//               Board Support Package (BSP) which is part of the Arduino core. 
// 
//               Leds                GPIO 
//               ----                ----        
//               LED   <――――――――――>  13  (LED_BUILTIN) active-low
// 
//               I2C [display]       GPIO
//               ---                 ----
//               SDA   <――――――――――>   3  (SDA)
//               SCL   <――――――――――>   4  (SCL)
//
//               SPI/LoRa            GPIO
//               ---                 ----
//               MOSI  <――――――――――>  40  (MOSI)
//               MISO  <――――――――――>  41  (MISO)
//               SCK   <――――――――――>  39  (SCK)
//               NSS   <――――――――――>  44  (unPhone::LORA_CS)
//               RST   <――――――――――>  42  (unPhone::LORA_RESET)
//               DIO0  <――――――――――>  10  (unPhone::LMIC_DIO0)
//               DIO1  <――――――――――>  11  (unPhone::LMIC_DIO1)
//
// Docs:         https://unphone.net
//               https://iot.unphone.net
//               https://gitlab.com/hamishcunningham/unphone
//               https://gitlab.com/hamishcunningham/unPhoneLibrary
//
// Identifiers:  LMIC-node
//                   board-id:      unphone9_lora
//               PlatformIO
//                   board:         unphone9
//                   platform:      espressif32
//               Arduino
//                   board:         ARDUINO_UNPHONE9
//                   architecture:  ARDUINO_ARCH_ESP32
// 
//////////////////////////////////////////////////////////////////////////////

#define DEVICEID_DEFAULT "unPhone"  // Default deviceid value

// Wait for Serial
// Can be useful for boards with MCU with integrated USB support.
#define WAITFOR_SERIAL_SECONDS_DEFAULT 10   // -1 waits indefinitely  

// LMIC Clock Error
// This is only needed for slower 8-bit MCUs (e.g. 8MHz ATmega328 and ATmega32u4).
// Value is defined in parts per million (of MAX_CLOCK_ERROR).
// #ifndef LMIC_CLOCK_ERROR_PPM
//     #define LMIC_CLOCK_ERROR_PPM 0
// #endif   

// Pin mappings for LoRa tranceiver
// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss =   unPhone::LORA_CS,
    .rxtx =  LMIC_UNUSED_PIN,
    .rst =   unPhone::LORA_RESET,
    .dio = { unPhone::LMIC_DIO0, unPhone::LMIC_DIO1, LMIC_UNUSED_PIN },
#ifdef MCCI_LMIC
    .rxtx_rx_active = 0,
    .rssi_cal = 8, // note: in old unPhone code this was 0; 8 works here tho
    .spi_freq = 8000000     /* 8 MHz */
#endif    
};

#ifdef USE_SERIAL
#  define serial Serial // Serial_& serial = Serial; (creates compile errors)
#endif

#ifdef USE_LED
    EasyLed led(LED_BUILTIN, EasyLed::ActiveLevel::Low);
#endif

#ifdef USE_DISPLAY
    // TODO HX8357

    // Create U8x8 instance for SSD1306 OLED display (no reset) using hardware I2C.
    U8X8_SSD1306_128X64_NONAME_HW_I2C display(/*rst*/ U8X8_PIN_NONE, /*scl*/ SCL, /*sda*/ SDA);
#endif

bool boardInit(InitType initType)
{
    // This function is used to perform board specific initializations.
    // Required as part of standard template.

    // InitType::Hardware        Must be called at start of setup() before anything else.
    // InitType::PostInitSerial  Must be called after initSerial() before other initializations.    

    bool success = true;
    switch (initType)
    {
        case InitType::Hardware:
            // Note: Serial port and display are not yet initialized and cannot be used use here.
            // No actions required for this board.
            break;

        case InitType::PostInitSerial:
            // Note: If enabled Serial port and display are already initialized here.
            // No actions required for this board.
            break;           
    }
    return success;
}
/////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// lorawan-keys.h
//////////////////////////////////////////////////////////////////////////////

// moved to private.h if exists, see above

// -----------------------------------------------------------------------------

// Optional: If ABP_DEVICEID is defined it will be used for ABP instead of the default defined in the BSF.
// #define ABP_DEVICEID "<deviceid>"

// Keys required for ABP activation:

// End-device Address (u4_t) in uint32_t format. 
// Note: The value must start with 0x (current version of TTN Console does not provide this).
#define ABP_DEVADDR 0x00000000

// Network Session Key (u1_t[16]) in msb format
#define ABP_NWKSKEY 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

// Application Session K (u1_t[16]) in msb format
#define ABP_APPSKEY 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

//////////////////////////////////////////////////////////////////////////////

    
/////////////////////////////////////////////////////////////////////////////
// LMIC-node.h again /////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#if defined(ABP_ACTIVATION) && defined(OTAA_ACTIVATION)
    #error Only one of ABP_ACTIVATION and OTAA_ACTIVATION can be defined.
#endif

#if defined(ABP_ACTIVATION) && defined(ABP_DEVICEID)
    const char deviceId[] = ABP_DEVICEID;
#elif defined(DEVICEID)
    const char deviceId[] = DEVICEID;
#else
    const char deviceId[] = DEVICEID_DEFAULT;
#endif

// Allow WAITFOR_SERIAL_SECONDS to be defined in platformio.ini.
// If used it shall be defined in the [common] section.
// The common setting will only be used for boards that have 
// WAITFOR_SERIAL_SECONDS_DEFAULT defined (in BSP) with a value != 0
#if defined(WAITFOR_SERIAL_SECONDS_DEFAULT)  && WAITFOR_SERIAL_SECONDS_DEFAULT != 0
    #ifdef WAITFOR_SERIAL_SECONDS
        #define WAITFOR_SERIAL_S WAITFOR_SERIAL_SECONDS
    #else
        #define WAITFOR_SERIAL_S WAITFOR_SERIAL_SECONDS_DEFAULT
    #endif
#else
    #define WAITFOR_SERIAL_S 0
#endif 

#if defined(ABP_ACTIVATION) && defined(CLASSIC_LMIC)
    #error Do NOT use ABP activation when using the deprecated IBM LMIC framework library. \
           On The Things Network V3 this will cause a downlink message for EVERY uplink message \
           because it does properly handle MAC commands. 
#endif

#ifdef OTAA_ACTIVATION
    #if !defined(OTAA_DEVEUI) || !defined(OTAA_APPEUI) || !defined(OTAA_APPKEY)
        #error One or more LoRaWAN keys (OTAA_DEVEUI, OTAA_APPEUI, OTAA_APPKEY) are not defined.
    #endif 
#else
    // ABP activation
    #if !defined(ABP_DEVADDR) || !defined(ABP_NWKSKEY) || !defined(ABP_APPSKEY)
        #error One or more LoRaWAN keys (ABP_DEVADDR, ABP_NWKSKEY, ABP_APPSKEY) are not defined.
    #endif
#endif

// Determine if a valid region is defined.
// This actually has little effect because
// CLASSIC LMIC: defines CFG_eu868 by default,
// MCCI LMIC: if no region is defined it
// sets CFG_eu868 as default.
#if ( \
    ( defined(CLASSIC_LMIC) \
      && !( defined(CFG_eu868) \
            || defined(CFG_us915) ) ) \
    || \
    ( defined(MCCI_LMIC) \
      && !( defined(CFG_as923) \
            || defined(CFG_as923jp) \
            || defined(CFG_au915) \
            || defined(CFG_eu868) \
            || defined(CFG_in866) \
            || defined(CFG_kr920) \
            || defined(CFG_us915) ) ) \
)
    #Error No valid LoRaWAN region defined
#endif   

#ifndef MCCI_LMIC
    #define LMIC_ERROR_SUCCESS 0
    typedef int lmic_tx_error_t;

    // In MCCI LMIC these are already defined.
    // This macro can be used to initalize an array of event strings
    #define LEGACY_LMIC_EVENT_NAME_TABLE__INIT \
                "<<zero>>", \
                "EV_SCAN_TIMEOUT", "EV_BEACON_FOUND", \
                "EV_BEACON_MISSED", "EV_BEACON_TRACKED", "EV_JOINING", \
                "EV_JOINED", "EV_RFU1", "EV_JOIN_FAILED", "EV_REJOIN_FAILED", \
                "EV_TXCOMPLETE", "EV_LOST_TSYNC", "EV_RESET", \
                "EV_RXCOMPLETE", "EV_LINK_DEAD", "EV_LINK_ALIVE"

    // If working on an AVR (or worried about memory size), you can use this multi-zero
    // string and put this in a single const F() string to store it in program memory.
    // Index through this counting up from 0, until you get to the entry you want or 
    // to an entry that begins with a \0.
    #define LEGACY_LMIC_EVENT_NAME_MULTISZ__INIT \
                "<<zero>>\0" \                                                           \
                "EV_SCAN_TIMEOUT\0" "EV_BEACON_FOUND\0" \
                "EV_BEACON_MISSED\0" "EV_BEACON_TRACKED\0" "EV_JOINING\0" \
                "EV_JOINED\0" "EV_RFU1\0" "EV_JOIN_FAILED\0" "EV_REJOIN_FAILED\0" \
                "EV_TXCOMPLETE\0" "EV_LOST_TSYNC\0" "EV_RESET\0" \
                "EV_RXCOMPLETE\0" "EV_LINK_DEAD\0" "EV_LINK_ALIVE\0"   
#endif // LMIC_MCCI   


#if defined(USE_SERIAL) || defined(USE_DISPLAY)

    #ifdef MCCI_LMIC   
        static const char * const lmicEventNames[] = { LMIC_EVENT_NAME_TABLE__INIT };
        static const char * const lmicErrorNames[] = { LMIC_ERROR_NAME__INIT };
    #else
        static const char * const lmicEventNames[] = { LEGACY_LMIC_EVENT_NAME_TABLE__INIT };
    #endif
        

    void printChars(Print& printer, char ch, uint8_t count, bool linefeed = false)
    {
        for (uint8_t i = 0; i < count; ++i)
        {
            printer.print(ch);
        }
        if (linefeed)
        {
            printer.println();
        }
    }


    void printSpaces(Print& printer, uint8_t count, bool linefeed = false)
    {
        printChars(printer, ' ', count, linefeed);
    }


    void printHex(Print& printer, uint8_t* bytes, size_t length = 1, bool linefeed = false, char separator = 0)
    {
        for (size_t i = 0; i < length; ++i)
        {
            if (i > 0 && separator != 0)
            {
                printer.print(separator);
            }
            if (bytes[i] <= 0x0F)
            {
                printer.print('0');
            }
            printer.print(bytes[i], HEX);        
        }
        if (linefeed)
        {
            printer.println();
        }
    }


    void setTxIndicatorsOn(bool on = true)
    {
        if (on)
        {
            #ifdef USE_LED
                led.on();
            #endif
            #ifdef USE_DISPLAY
                displayTxSymbol(true);
            #endif           
        }
        else
        {
            #ifdef USE_LED
                led.off();
            #endif
            #ifdef USE_DISPLAY
                displayTxSymbol(false);
            #endif           
        }        
    }
    
#endif  // USE_SERIAL || USE_DISPLAY


#ifdef USE_DISPLAY 
    uint8_t transmitSymbol[8] = {0x18, 0x18, 0x00, 0x24, 0x99, 0x42, 0x3c, 0x00}; 
    #define ROW_0             0
    #define ROW_1             1
    #define ROW_2             2
    #define ROW_3             3
    #define ROW_4             4
    #define ROW_5             5
    #define ROW_6             6
    #define ROW_7             7    
    #define HEADER_ROW        ROW_0
    #define DEVICEID_ROW      ROW_1
    #define INTERVAL_ROW      ROW_2
    #define TIME_ROW          ROW_4
    #define EVENT_ROW         ROW_5
    #define STATUS_ROW        ROW_6
    #define FRMCNTRS_ROW      ROW_7
    #define COL_0             0
    #define ABPMODE_COL       10
    #define CLMICSYMBOL_COL   14
    #define TXSYMBOL_COL      15

    void initDisplay()
    {
        display.begin();
        display.setFont(u8x8_font_victoriamedium8_r); 
    }

    void displayTxSymbol(bool visible = true)
    {
        if (visible)
        {
            display.drawTile(TXSYMBOL_COL, ROW_0, 1, transmitSymbol);
        }
        else
        {
            display.drawGlyph(TXSYMBOL_COL, ROW_0, char(0x20));
        }
    }    
#endif // USE_DISPLAY


#ifdef USE_SERIAL
    bool initSerial(unsigned long speed = 115200, int16_t timeoutSeconds = 0)
    {
        // Initializes the serial port.
        // Optionally waits for serial port to be ready.
        // Will display status and progress on display (if enabled)
        // which can be useful for tracing (e.g. ATmega328u4) serial port issues.
        // A negative timeoutSeconds value will wait indefinitely.
        // A value of 0 (default) will not wait.
        // Returns: true when serial port ready,
        //          false when not ready.

        serial.begin(speed);

        #if WAITFOR_SERIAL_S != 0
            if (timeoutSeconds != 0)
            {   
                bool indefinite = (timeoutSeconds < 0);
                uint16_t secondsLeft = timeoutSeconds; 
                #ifdef USE_DISPLAY
                    display.setCursor(0, ROW_1);
                    display.print(F("Waiting for"));
                    display.setCursor(0,  ROW_2);                
                    display.print(F("serial port"));
                #endif

                while (!serial && (indefinite || secondsLeft > 0))
                {
                    if (!indefinite)
                    {
                        #ifdef USE_DISPLAY
                            display.clearLine(ROW_4);
                            display.setCursor(0, ROW_4);
                            display.print(F("timeout in "));
                            display.print(secondsLeft);
                            display.print('s');
                        #endif
                        --secondsLeft;
                    }
                    delay(1000);
                }  
                #ifdef USE_DISPLAY
                    display.setCursor(0, ROW_4);
                    if (serial)
                    {
                        display.print(F("Connected"));
                    }
                    else
                    {
                        display.print(F("NOT connected"));
                    }
                #endif
            }
        #endif

        return serial;
    }
#endif


//////////////////////////////////////////////////////////////////////////////
// LMIC-node.ccp /////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▄ █▀▀ █▀▀ ▀█▀ █▀█
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▄ █▀▀ █ █  █  █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀


const uint8_t payloadBufferLength = unPhone::LORA_PAYLOAD_LEN;


//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▀ █▀█ █▀▄
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▀ █ █ █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀▀ ▀ ▀ ▀▀ 


uint8_t payloadBuffer[payloadBufferLength];
static osjob_t doWorkJob;
uint32_t doWorkIntervalSeconds = DO_WORK_INTERVAL_SECONDS;  // Change value in platformio.ini

// Note: LoRa module pin mappings are defined in the Board Support Files.

// Set LoRaWAN keys defined in lorawan-keys.h.
#ifdef OTAA_ACTIVATION
    static const u1_t PROGMEM DEVEUI[8]  = { OTAA_DEVEUI } ;
    static const u1_t PROGMEM APPEUI[8]  = { OTAA_APPEUI };
    static const u1_t PROGMEM APPKEY[16] = { OTAA_APPKEY };
    // Below callbacks are used by LMIC for reading above values.
    void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
    void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
    void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }    
#else
    // ABP activation
    static const u4_t DEVADDR = ABP_DEVADDR ;
    static const PROGMEM u1_t NWKSKEY[16] = { ABP_NWKSKEY };
    static const u1_t PROGMEM APPSKEY[16] = { ABP_APPSKEY };
    // Below callbacks are not used be they must be defined.
    void os_getDevEui (u1_t* buf) { }
    void os_getArtEui (u1_t* buf) { }
    void os_getDevKey (u1_t* buf) { }
#endif


int16_t getSnrTenfold()
{
    // Returns ten times the SNR (dB) value of the last received packet.
    // Ten times to prevent the use of float but keep 1 decimal digit accuracy.
    // Calculation per SX1276 datasheet rev.7 §6.4, SX1276 datasheet rev.4 §6.4.
    // LMIC.snr contains value of PacketSnr, which is 4 times the actual SNR value.
    return (LMIC.snr * 10) / 4;
}


int16_t getRssi(int8_t snr)
{
    // Returns correct RSSI (dBm) value of the last received packet.
    // Calculation per SX1276 datasheet rev.7 §5.5.5, SX1272 datasheet rev.4 §5.5.5.

    #define RSSI_OFFSET            64
    #define SX1276_FREQ_LF_MAX     525000000     // per datasheet 6.3
    #define SX1272_RSSI_ADJUST     -139
    #define SX1276_RSSI_ADJUST_LF  -164
    #define SX1276_RSSI_ADJUST_HF  -157

    int16_t rssi;

    #ifdef MCCI_LMIC

        rssi = LMIC.rssi - RSSI_OFFSET;

    #else
        int16_t rssiAdjust;
        #ifdef CFG_sx1276_radio
            if (LMIC.freq > SX1276_FREQ_LF_MAX)
            {
                rssiAdjust = SX1276_RSSI_ADJUST_HF;
            }
            else
            {
                rssiAdjust = SX1276_RSSI_ADJUST_LF;   
            }
        #else
            // CFG_sx1272_radio    
            rssiAdjust = SX1272_RSSI_ADJUST;
        #endif    
        
        // Revert modification (applied in lmic/radio.c) to get PacketRssi.
        int16_t packetRssi = LMIC.rssi + 125 - RSSI_OFFSET;
        if (snr < 0)
        {
            rssi = rssiAdjust + packetRssi + snr;
        }
        else
        {
            rssi = rssiAdjust + (16 * packetRssi) / 15;
        }
    #endif

    return rssi;
}


void printEvent(ostime_t timestamp, 
                const char * const message, 
                PrintTarget target = PrintTarget::All,
                bool clearDisplayStatusRow = true,
                bool eventLabel = false)
{
    #ifdef USE_DISPLAY 
        if (target == PrintTarget::All || target == PrintTarget::Display)
        {
            display.clearLine(TIME_ROW);
            display.setCursor(COL_0, TIME_ROW);
            display.print(F("Time:"));                 
            display.print(timestamp); 
            display.clearLine(EVENT_ROW);
            if (clearDisplayStatusRow)
            {
                display.clearLine(STATUS_ROW);    
            }
            display.setCursor(COL_0, EVENT_ROW);               
            display.print(message);
        }
    #endif  
    
    #ifdef USE_SERIAL
        // Create padded/indented output without using printf().
        // printf() is not default supported/enabled in each Arduino core. 
        // Not using printf() will save memory for memory constrainted devices.
        String timeString(timestamp);
        uint8_t len = timeString.length();
        uint8_t zerosCount = TIMESTAMP_WIDTH > len ? TIMESTAMP_WIDTH - len : 0;

        if (target == PrintTarget::All || target == PrintTarget::Serial)
        {
            printChars(serial, '0', zerosCount);
            serial.print(timeString);
            serial.print(":  ");
            if (eventLabel)
            {
                serial.print(F("TTN LoRa Event: "));
            }
            serial.println(message);
        }
    #endif   
}           

void printEvent(ostime_t timestamp, 
                ev_t ev, 
                PrintTarget target = PrintTarget::All, 
                bool clearDisplayStatusRow = true)
{
    #if defined(USE_DISPLAY) || defined(USE_SERIAL)
        printEvent(timestamp, lmicEventNames[ev], target, clearDisplayStatusRow, true);
    #endif
}


void printFrameCounters(PrintTarget target = PrintTarget::All)
{
    #ifdef USE_DISPLAY
        if (target == PrintTarget::Display || target == PrintTarget::All)
        {
            display.clearLine(FRMCNTRS_ROW);
            display.setCursor(COL_0, FRMCNTRS_ROW);
            display.print(F("Up:"));
            display.print(LMIC.seqnoUp);
            display.print(F(" Dn:"));
            display.print(LMIC.seqnoDn);        
        }
    #endif

    #ifdef USE_SERIAL
        if (target == PrintTarget::Serial || target == PrintTarget::All)
        {
            printSpaces(serial, MESSAGE_INDENT);
            serial.print(F("Up: "));
            serial.print(LMIC.seqnoUp);
            serial.print(F(",  Down: "));
            serial.println(LMIC.seqnoDn);        
        }
    #endif        
}      


void printSessionKeys()
{    
    #if defined(USE_SERIAL) && defined(MCCI_LMIC)
        u4_t networkId = 0;
        devaddr_t deviceAddress = 0;
        u1_t networkSessionKey[16];
        u1_t applicationSessionKey[16];
        LMIC_getSessionKeys(&networkId, &deviceAddress, 
                            networkSessionKey, applicationSessionKey);

        printSpaces(serial, MESSAGE_INDENT);    
        serial.print(F("Network Id: "));
        serial.println(networkId, DEC);

        printSpaces(serial, MESSAGE_INDENT);    
        serial.print(F("Device Address: "));
        serial.println(deviceAddress, HEX);

        printSpaces(serial, MESSAGE_INDENT);    
        serial.print(F("Application Session Key: "));
        printHex(serial, applicationSessionKey, 16, true, '-');

        printSpaces(serial, MESSAGE_INDENT);    
        serial.print(F("Network Session Key:     "));
        printHex(serial, networkSessionKey, 16, true, '-');
    #endif
}


void printDownlinkInfo(void)
{
    #if defined(USE_SERIAL) || defined(USE_DISPLAY)

        uint8_t dataLength = LMIC.dataLen;
        // bool ackReceived = LMIC.txrxFlags & TXRX_ACK;

        int16_t snrTenfold = getSnrTenfold();
        int8_t snr = snrTenfold / 10;
        int8_t snrDecimalFraction = snrTenfold % 10;
        int16_t rssi = getRssi(snr);

        uint8_t fPort = 0;        
        if (LMIC.txrxFlags & TXRX_PORT)
        {
            fPort = LMIC.frame[LMIC.dataBeg -1];
        }        

        #ifdef USE_DISPLAY
            display.clearLine(EVENT_ROW);        
            display.setCursor(COL_0, EVENT_ROW);
            display.print(F("RX P:"));
            display.print(fPort);
            if (dataLength != 0)
            {
                display.print(" Len:");
                display.print(LMIC.dataLen);                       
            }
            display.clearLine(STATUS_ROW);        
            display.setCursor(COL_0, STATUS_ROW);
            display.print(F("RSSI"));
            display.print(rssi);
            display.print(F(" SNR"));
            display.print(snr);                
            display.print(".");                
            display.print(snrDecimalFraction);                      
        #endif

        #ifdef USE_SERIAL
            printSpaces(serial, MESSAGE_INDENT);    
            serial.println(F("Downlink received"));

            printSpaces(serial, MESSAGE_INDENT);
            serial.print(F("RSSI: "));
            serial.print(rssi);
            serial.print(F(" dBm,  SNR: "));
            serial.print(snr);                        
            serial.print(".");                        
            serial.print(snrDecimalFraction);                        
            serial.println(F(" dB"));

            printSpaces(serial, MESSAGE_INDENT);    
            serial.print(F("Port: "));
            serial.println(fPort);
   
            if (dataLength != 0)
            {
                printSpaces(serial, MESSAGE_INDENT);
                serial.print(F("Length: "));
                serial.println(LMIC.dataLen);                   
                printSpaces(serial, MESSAGE_INDENT);    
                serial.print(F("Data: "));
                printHex(serial, LMIC.frame+LMIC.dataBeg, LMIC.dataLen, true, ' ');
            }
        #endif
    #endif
} 


void printHeader(void)
{
    #ifdef USE_DISPLAY
        display.clear();
        display.setCursor(COL_0, HEADER_ROW);
        display.print(F("LMIC-node"));
        #ifdef ABP_ACTIVATION
            display.drawString(ABPMODE_COL, HEADER_ROW, "ABP");
        #endif
        #ifdef CLASSIC_LMIC
            display.drawString(CLMICSYMBOL_COL, HEADER_ROW, "*");
        #endif
        display.drawString(COL_0, DEVICEID_ROW, deviceId);
        display.setCursor(COL_0, INTERVAL_ROW);
        display.print(F("Interval:"));
        display.print(doWorkIntervalSeconds);
        display.print("s");
    #endif

    #ifdef USE_SERIAL
        serial.println(F("TTN LoRa LMIC-node"));
        serial.print(F("               Device-id:     "));
        serial.println(deviceId);            
        serial.print(F("               LMIC library:  "));
        #ifdef MCCI_LMIC  
            serial.println(F("MCCI"));
        #else
            serial.println(F("Classic [Deprecated]")); 
        #endif
        serial.print(F("               Activation:    "));
        #ifdef OTAA_ACTIVATION  
            serial.println(F("OTAA"));
        #else
            serial.println(F("ABP")); 
        #endif
        #if defined(LMIC_DEBUG_LEVEL) && LMIC_DEBUG_LEVEL > 0
            serial.print(F("LMIC debug:    "));  
            serial.println(LMIC_DEBUG_LEVEL);
        #endif
        serial.print(F("               Interval:      "));
        serial.print(doWorkIntervalSeconds);
        serial.println(F(" seconds"));
//      if (activationMode == ActivationMode::OTAA)
//      {
//          serial.println();
//      }
    #endif
}     


#ifdef ABP_ACTIVATION
    void setAbpParameters(dr_t dataRate = DefaultABPDataRate, s1_t txPower = DefaultABPTxPower) 
    {
        // Set static session parameters. Instead of dynamically establishing a session
        // by joining the network, precomputed session parameters are be provided.
        #ifdef PROGMEM
            // On AVR, these values are stored in flash and only copied to RAM
            // once. Copy them to a temporary buffer here, LMIC_setSession will
            // copy them into a buffer of its own again.
            uint8_t appskey[sizeof(APPSKEY)];
            uint8_t nwkskey[sizeof(NWKSKEY)];
            memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
            memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
            LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
        #else
            // If not running an AVR with PROGMEM, just use the arrays directly
            LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
        #endif

        #if defined(CFG_eu868)
            // Set up the channels used by the Things Network, which corresponds
            // to the defaults of most gateways. Without this, only three base
            // channels from the LoRaWAN specification are used, which certainly
            // works, so it is good for debugging, but can overload those
            // frequencies, so be sure to configure the full frequency range of
            // your network here (unless your network autoconfigures them).
            // Setting up channels should happen after LMIC_setSession, as that
            // configures the minimal channel set. The LMIC doesn't let you change
            // the three basic settings, but we show them here.
            LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
            LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
            // TTN defines an additional channel at 869.525Mhz using SF9 for class B
            // devices' ping slots. LMIC does not have an easy way to define set this
            // frequency and support for class B is spotty and untested, so this
            // frequency is not configured here.
        #elif defined(CFG_us915) || defined(CFG_au915)
            // NA-US and AU channels 0-71 are configured automatically
            // but only one group of 8 should (a subband) should be active
            // TTN recommends the second sub band, 1 in a zero based count.
            // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
            LMIC_selectSubBand(1);
        #elif defined(CFG_as923)
            // Set up the channels used in your country. Only two are defined by default,
            // and they cannot be changed.  Use BAND_CENTI to indicate 1% duty cycle.
            // LMIC_setupChannel(0, 923200000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
            // LMIC_setupChannel(1, 923400000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);

            // ... extra definitions for channels 2..n here
        #elif defined(CFG_kr920)
            // Set up the channels used in your country. Three are defined by default,
            // and they cannot be changed. Duty cycle doesn't matter, but is conventionally
            // BAND_MILLI.
            // LMIC_setupChannel(0, 922100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(1, 922300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(2, 922500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);

            // ... extra definitions for channels 3..n here.
        #elif defined(CFG_in866)
            // Set up the channels used in your country. Three are defined by default,
            // and they cannot be changed. Duty cycle doesn't matter, but is conventionally
            // BAND_MILLI.
            // LMIC_setupChannel(0, 865062500, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(1, 865402500, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(2, 865985000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);

            // ... extra definitions for channels 3..n here.
        #endif

        // Disable link check validation
        LMIC_setLinkCheckMode(0);

        // TTN uses SF9 for its RX2 window.
        LMIC.dn2Dr = DR_SF9;

        // Set data rate and transmit power (note: txpow is possibly ignored by the library)
        LMIC_setDrTxpow(dataRate, txPower);    
    }
#endif //ABP_ACTIVATION


void initLmic(bit_t adrEnabled = 1,
              dr_t abpDataRate = DefaultABPDataRate, 
              s1_t abpTxPower = DefaultABPTxPower) 
{
    // ostime_t timestamp = os_getTime();

    // Initialize LMIC runtime environment
    os_init();
    // Reset MAC state
    LMIC_reset();

    #ifdef ABP_ACTIVATION
        setAbpParameters(abpDataRate, abpTxPower);
    #endif

    // Enable or disable ADR (data rate adaptation). 
    // Should be turned off if the device is not stationary (mobile).
    // 1 is on, 0 is off.
    LMIC_setAdrMode(adrEnabled);

    if (activationMode == ActivationMode::OTAA)
    {
        #if defined(CFG_us915) || defined(CFG_au915)
            // NA-US and AU channels 0-71 are configured automatically
            // but only one group of 8 should (a subband) should be active
            // TTN recommends the second sub band, 1 in a zero based count.
            // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
            LMIC_selectSubBand(1); 
        #endif
    }

    // Relax LMIC timing if defined
    #if defined(LMIC_CLOCK_ERROR_PPM)
        uint32_t clockError = 0;
        #if LMIC_CLOCK_ERROR_PPM > 0
            #if defined(MCCI_LMIC) && LMIC_CLOCK_ERROR_PPM > 4000
                // Allow clock error percentage to be > 0.4%
                #define LMIC_ENABLE_arbitrary_clock_error 1
            #endif    
            clockError = (LMIC_CLOCK_ERROR_PPM / 100) * (MAX_CLOCK_ERROR / 100) / 100;
            LMIC_setClockError(clockError);
        #endif

        #ifdef USE_SERIAL
            serial.print(F("Clock Error:   "));
            serial.print(LMIC_CLOCK_ERROR_PPM);
            serial.print(" ppm (");
            serial.print(clockError);
            serial.println(")");            
        #endif
    #endif

    #ifdef MCCI_LMIC
        // Register a custom eventhandler and don't use default onEvent() to enable
        // additional features (e.g. make EV_RXSTART available). User data pointer is omitted.
        LMIC_registerEventCb(&onLmicEvent, nullptr);
    #endif
}


#ifdef MCCI_LMIC 
void onLmicEvent(void *pUserData, ev_t ev)
#else
void onEvent(ev_t ev) 
#endif
{
    // LMIC event handler
    ostime_t timestamp = os_getTime(); 

    switch (ev) 
    {
#ifdef MCCI_LMIC
        // Only supported in MCCI LMIC library:
        case EV_RXSTART:
            // Do not print anything for this event or it will mess up timing.
            break;

        case EV_TXSTART:
            setTxIndicatorsOn();
            printEvent(timestamp, ev);            
            break;               

        case EV_JOIN_TXCOMPLETE:
        case EV_TXCANCELED:
            setTxIndicatorsOn(false);
            printEvent(timestamp, ev);
            break;               
#endif
        case EV_JOINED:
            setTxIndicatorsOn(false);
            printEvent(timestamp, ev);
            printSessionKeys();

            // Disable link check validation.
            // Link check validation is automatically enabled
            // during join, but because slow data rates change
            // max TX size, it is not used in this example.                    
            LMIC_setLinkCheckMode(0);

            // The doWork job has probably run already (while
            // the node was still joining) and have rescheduled itself.
            // Cancel the next scheduled doWork job and re-schedule
            // for immediate execution to prevent that any uplink will
            // have to wait until the current doWork interval ends.
            os_clearCallback(&doWorkJob);
            os_setCallback(&doWorkJob, doWorkCallback);
            break;

        case EV_TXCOMPLETE:
            // Transmit completed, includes waiting for RX windows.
            setTxIndicatorsOn(false);   
            printEvent(timestamp, ev);
            printFrameCounters();

            // Check if downlink was received
            if (LMIC.dataLen != 0 || LMIC.dataBeg != 0)
            {
                uint8_t fPort = 0;
                if (LMIC.txrxFlags & TXRX_PORT)
                {
                    fPort = LMIC.frame[LMIC.dataBeg -1];
                }
                printDownlinkInfo();
                processDownlink(timestamp, fPort, LMIC.frame + LMIC.dataBeg, LMIC.dataLen);                
            }
            break;     
          
        // Below events are printed only.
        case EV_SCAN_TIMEOUT:
        case EV_BEACON_FOUND:
        case EV_BEACON_MISSED:
        case EV_BEACON_TRACKED:
        case EV_RFU1:                    // This event is defined but not used in code
        case EV_JOINING:        
        case EV_JOIN_FAILED:           
        case EV_REJOIN_FAILED:
        case EV_LOST_TSYNC:
        case EV_RESET:
        case EV_RXCOMPLETE:
        case EV_LINK_DEAD:
        case EV_LINK_ALIVE:
#ifdef MCCI_LMIC
        // Only supported in MCCI LMIC library:
        case EV_SCAN_FOUND:              // This event is defined but not used in code 
#endif
            printEvent(timestamp, ev);    
            break;

        default: 
            printEvent(timestamp, "Unknown Event");    
            break;
    }
}


static void doWorkCallback(osjob_t* job)
{
    // Event hander for doWorkJob. Gets called by the LMIC scheduler.
    // The actual work is performed in function processWork() which is called below.

    ostime_t timestamp = os_getTime();
    #ifdef USE_SERIAL
//      serial.println();
        printEvent(timestamp, "TTN LoRa doWork job started", PrintTarget::Serial);
    #endif    

    // Do the work that needs to be performed.
    processWork(timestamp);

    // This job must explicitly reschedule itself for the next run.
    ostime_t startAt = timestamp + sec2osticks((int64_t)doWorkIntervalSeconds);
    os_setTimedCallback(&doWorkJob, startAt, doWorkCallback);    
}


lmic_tx_error_t scheduleUplink(uint8_t fPort, uint8_t* data, uint8_t dataLength, bool confirmed = false)
{
    // This function is called from the processWork() function to schedule
    // transmission of an uplink message that was prepared by processWork().
    // Transmission will be performed at the next possible time

    ostime_t timestamp = os_getTime();
    printEvent(timestamp, "Packet queued");

    lmic_tx_error_t retval = LMIC_setTxData2(fPort, data, dataLength, confirmed ? 1 : 0);
    timestamp = os_getTime();

    if (retval == LMIC_ERROR_SUCCESS)
    {
        #ifdef CLASSIC_LMIC
            // For MCCI_LMIC this will be handled in EV_TXSTART        
            setTxIndicatorsOn();  
        #endif        
    }
    else
    {
        String errmsg; 
        #ifdef USE_SERIAL
            errmsg = "LMIC Error: ";
            #ifdef MCCI_LMIC
                errmsg.concat(lmicErrorNames[abs(retval)]);
            #else
                errmsg.concat(retval);
            #endif
            printEvent(timestamp, errmsg.c_str(), PrintTarget::Serial);
        #endif
        #ifdef USE_DISPLAY
            errmsg = "LMIC Err: ";
            errmsg.concat(retval);
            printEvent(timestamp, errmsg.c_str(), PrintTarget::Display);
        #endif         
    }
    return retval;    
}


//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▄ █▀▀ █▀▀ ▀█▀ █▀█
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▄ █▀▀ █ █  █  █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀

static volatile uint16_t counter_ = 0;
uint16_t getCounterValue() { // Increments counter and returns the new value.
    delay(50); // Fake this takes some time
    return ++counter_;
}
void resetCounter() { counter_ = 0; } // Reset counter to 0

// This function is called from the doWorkCallback() callback function when
// the doWork job is executed. Uses globals: payloadBuffer and LMIC data
// structure. This is where the main work is performed like reading sensor and
// GPS data and schedule uplink messages if anything needs to be transmitted.
void processWork(ostime_t doWorkJobTimeStamp) {
    // Skip processWork if using OTAA and still joining.
    if (LMIC.devaddr != 0) {
        // Collect input data.
        // For simplicity LMIC-node uses a counter to simulate a sensor.
        // The counter is increased automatically by getCounterValue()
        // and can be reset with a 'reset counter' command downlink message.

        uint16_t counterValue = getCounterValue();
        ostime_t timestamp = os_getTime();

        // if there's no payload do nothing
        if(! lora_payload_ready) {
          #ifdef USE_DISPLAY
            printEvent(timestamp, "no pyld, UL !scheduled", PrintTarget::Display);
          #endif
          #ifdef USE_SERIAL
            printEvent(
              timestamp, "no payload, uplink not scheduled", PrintTarget::Serial
            );
          #endif
          return;
        }

        #ifdef USE_DISPLAY
            // Interval and Counter values are combined on a single row. This
            // allows to keep the 3rd row empty which makes the information
            // better readable on the small display.
            display.clearLine(INTERVAL_ROW);
            display.setCursor(COL_0, INTERVAL_ROW);
            display.print("I:");
            display.print(doWorkIntervalSeconds);
            display.print("s");
            display.print(" Ctr:");
            display.print(counterValue);
        #endif
        #ifdef USE_SERIAL
            printEvent(timestamp, "Input data collected", PrintTarget::Serial);
            printSpaces(serial, MESSAGE_INDENT);
            serial.print(F("COUNTER value: "));
            serial.println(counterValue);
        #endif

        // For simplicity LMIC-node will try to send an uplink message every
        // time processWork() is executed.

        // Schedule uplink message if possible
        if (LMIC.opmode & OP_TXRXPEND) {
            // TxRx is currently pending, do not send.
            #ifdef USE_SERIAL
                printEvent(timestamp,
                  "Uplink not scheduled because TxRx pending",
                  PrintTarget::Serial);
            #endif
            #ifdef USE_DISPLAY
                printEvent(timestamp, "UL not scheduled", PrintTarget::Display);
            #endif
        } else { // Prepare uplink payload.
            // copy payload from (lora_send) caller
            uint8_t payloadLength =
              min((uint8_t) strlen(lora_payload), unPhone::LORA_PAYLOAD_LEN);
            strncpy((char *) payloadBuffer, lora_payload, payloadLength);
            payloadBuffer[unPhone::LORA_PAYLOAD_LEN - 1] = '\0'; // no overrun

            // schedule an uplink
            uint8_t fPort = 10;
//          payloadBuffer[0] = counterValue >> 8;
//          payloadBuffer[1] = counterValue & 0xFF;
//          uint8_t payloadLength = 2;
            scheduleUplink(fPort, payloadBuffer, payloadLength);
            lora_payload_ready = false;
        }
    }
}

// This function is called from the onEvent() event handler on EV_TXCOMPLETE
// when a downlink message was received. Implements a 'reset counter' command
// that can be sent via a downlink message. To send the reset counter command
// to the node, send a downlink message (e.g. from the TTN Console) with
// single byte value resetCmd on port cmdPort.
void processDownlink(
  ostime_t txCompleteTimestamp, uint8_t fPort, uint8_t* data, uint8_t dataLength
) {
    const uint8_t cmdPort = 100;
    const uint8_t resetCmd= 0xC0;

    if (fPort == cmdPort && dataLength == 1 && data[0] == resetCmd) {
        #ifdef USE_SERIAL
            printSpaces(serial, MESSAGE_INDENT);
            serial.println(F("Reset cmd received"));
        #endif
        ostime_t timestamp = os_getTime();
        resetCounter();
        printEvent(timestamp, "Counter reset", PrintTarget::All, false);
    }
}

// ("user code" setup and loop merged into exported API) /////////////////////


//////////////////////////////////////////////////////////////////////////////
// exported API for unPhone //////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void unphone_lora_setup() {
  // boardInit() must be called at start of setup() before anything else
  bool hardwareInitSucceeded = boardInit(InitType::Hardware); // TODO delete?

  #ifdef USE_DISPLAY 
    initDisplay();
  #endif
// HC this is done elsewhere
//#ifdef USE_SERIAL
//  initSerial(MONITOR_SPEED, WAITFOR_SERIAL_S);
//#endif    

  boardInit(InitType::PostInitSerial);

  #if defined(USE_SERIAL) || defined(USE_DISPLAY)
    printHeader();
  #endif

  if (!hardwareInitSucceeded) {   
    #ifdef USE_SERIAL
      serial.println(F("Error: hardware init failed."));
      serial.flush();            
    #endif
    #ifdef USE_DISPLAY
      // Following mesage shown only if failure was unrelated to I2C.
      display.setCursor(COL_0, FRMCNTRS_ROW);
      display.print(F("HW init failed"));
    #endif
    abort();
  }

  initLmic();

  //  "user code" begin: place code for initializing sensors etc. here.
  resetCounter();
  //  "user code" end

  if (activationMode == ActivationMode::OTAA)
    LMIC_startJoining();

  // schedule initial doWork job for immediate execution.
  os_setCallback(&doWorkJob, doWorkCallback);
}

void unphone_lora_loop() {
  os_runloop_once();
}

// ttn msg (vsprintf style)
void unphone_lora_send(const char *fmt, va_list arglist) {
  vsprintf(lora_payload, fmt, arglist);
  lora_payload_ready = true;
}

void unphone_lora_shutdown() { LMIC_shutdown(); }
