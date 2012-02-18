/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * @file webserver.ino
 *
 * This example shows how to use NanodeUIP to run a web server.
 * It will show the current state of all digital and analog
 * pins, and allow you to change the state of digital pins
 * which are configured as outputs.
 *
 * Note that a minimum 500 byte packet buffer seems to be needed
 * to have pages load correctly in Firefox.
 */

//#define ETHERSHIELD // uncomment to run on EtherShield
#include <NanodeUIP.h>
#include <psock.h>
#include "httpd.h"
#include "printf.h"
#include "pins_cgi.h"

#ifndef ETHERSHIELD
#include <NanodeUNIO.h>
#endif

// If the required pins have not previously been defined, let's set the
// values now.
#ifndef PINS_DEFINED
const int led_red = 6;
const int led_yellow = 5;
const int led_green = 7;
const int button_a = 2;
const int button_b = 3;
const int button_c = 4;
#endif

// To ensure that uip-conf.h is set up correctly to accomodate webclient.
UIPASSERT(sizeof(struct httpd_state)<=TCP_APP_STATE_SIZE)

// We'll need this later to keep an eye on our memory
extern uint16_t* __brkval;

/**************************************************************************/
/**
 * @defgroup app_logic Application Logic
 *
 * The main stuff the sketch does is handled here.
 *
 * @{
 */

/**
 * All available application states handled by the loop()
 */
enum app_states_e { state_none = 0, state_needip, state_waiting, state_invalid };

/**
 * The current state the application is in
 */
static app_states_e app_state = state_needip;

/**
 * Flags related to the application state.  Generally, the application state
 * will transition when one of these flags changes state.
 */
struct app_flags_t
{
  uint8_t have_ip:1;
  uint8_t have_resolv:1;
};

/**
 * The current state of the application flags
 */
static app_flags_t app_flags;

/**
 * Callback for changes to DHCP status.
 *
 * Called by uIP when the DHCP status changes.  We use it to determine that
 * we now have a valid IP.
 *
 * @param s Current status
 * @param dnsaddr Pointer to the DNS server which the DHCP server gave us.
 */
static void dhcp_status(int s,const uint16_t *dnsaddr)
{
  char buf[20];
  if (s==DHCP_STATUS_OK)
  {
    resolv_conf(dnsaddr);
    uip.get_ip_addr_str(buf);
    printf_P(PSTR("%lu: IP:%s\r\n"),millis(),buf);
    app_flags.have_ip = 1;
  }
}
/** @} */

/**************************************************************************/
/**
 * @defgroup pins Pin Descriptions 
 *
 * The main purpose of this app is to control a set of pins from a web
 * server.  This sections defines which pins are controlled and what
 * they are called.
 *
 * @{
 */

/**
 * Easy-to-remember names of pins used in this sketch
 */
const char led_red_str[] PROGMEM = "Red Light";
const char led_yellow_str[] PROGMEM = "Yellow Light";
const char led_green_str[] PROGMEM = "Green Light";
const char button_a_str[] PROGMEM = "Button A";
const char button_b_str[] PROGMEM = "Button B";
const char button_c_str[] PROGMEM = "Button C";
const char analog_0_str[] PROGMEM = "Analog 0";
const char analog_1_str[] PROGMEM = "Analog 1";

/**
 * Lights monitored by the web server
 */
const pin_def_t lights[] PROGMEM =
{
  { led_red_str, led_red },
  { led_yellow_str, led_yellow },
  { led_green_str, led_green },
  { 0,0 } // terminator
};

/**
 * Buttons monitored by the web server
 */
const pin_def_t buttons[] PROGMEM =
{
  { button_a_str, button_a },
  { button_b_str, button_b },
  { button_c_str, button_c },
  { 0,0 } // terminator
};

/**
 * Sensors monitored by the web server
 */
const pin_def_t sensors[] PROGMEM =
{
  { analog_0_str, 0 },
  { analog_1_str, 1 },
  { 0,0 } // terminator
};

/**
 * Ensure the pins are set correctly, and register them with the
 * web server.
 */
void setup_pins()
{
  // led's are outputs
  pinMode(led_red,OUTPUT);
  pinMode(led_yellow,OUTPUT);
  pinMode(led_green,OUTPUT);

  // buttons are inputs
  pinMode(button_a,INPUT);
  digitalWrite(button_a,HIGH);
  pinMode(button_b,INPUT);
  digitalWrite(button_b,HIGH);
  pinMode(button_c,INPUT);
  digitalWrite(button_c,HIGH);

  // Tell the CGI which pins we care about
  cgi_register(buttons,lights,sensors);
}
/** @} */

/**************************************************************************/
/**
 * @defgroup app_control Primary Application Control
 * @{
 */

/**
 * Application setup
 */
void setup()
{
  //
  // Start networking
  //

#ifdef ETHERSHIELD // EtherShield
  byte macaddr[6] = { 0x2, 0x00, 0x00, 0x1, 0x2, 0x3 };
  uip.init(macaddr,SS);
#else  // Nanode
  byte macaddr[6];
  NanodeUNIO unio(NANODE_MAC_DEVICE);
  unio.read(macaddr,NANODE_MAC_ADDRESS,6);
  uip.init(macaddr);
#endif
  
  //
  // Prepare and register pins.
  //
  // Set pins first, so that later inits() can override, but
  // after NanodeUNIO, so pin 7 can be used.
  //

  setup_pins();

  //
  // Print Preamble
  //

  Serial.begin(38400);
  printf_begin();
  printf_P(PSTR(__FILE__"\r\n"));
  printf_P(PSTR("FREE: %u\r\n"),SP-(uint16_t)__brkval);

  //
  // Setup Ethernet networking
  //

  char buf[20];
  uip.get_mac_str(buf);
  Serial.println(buf);
  uip.wait_for_link();
  nanode_log_P(PSTR("Link is up"));
  uip.start_dhcp(dhcp_status);
}

/**
 * Application loop
 */
void loop()
{
  uip.poll();

  // State is pretty simple in this app, we just sit here and
  // listen for http connections
  switch (app_state)
  {
  case state_needip:
    if ( app_flags.have_ip )
    {
      app_state = state_waiting;
      httpd_init();
      printf_P(PSTR("+READY"));
    }
    break;
  case state_waiting:
  case state_none:
  case state_invalid:
    break;
  }
}
/** @} */

// vim:cin:ai:sts=2 sw=2 ft=cpp
