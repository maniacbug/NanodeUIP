/*
 Copyright (C) 2012 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * @file sensor_gateway_rf24.ino
 *
 * This example shows how to use NanodeUIP to upload readings
 * to Pachube, after receiving them over the air using an
 * nRF24L01+ radio.  This can be used as a 'base station' for
 * a wireless sensor network, listening for readings and
 * uploading them to Pachube.
 *
 * REQUIRES the RF24 and RF24Network libraries
 * https://github.com/maniacbug/RF24
 * https://github.com/maniacbug/RF24Network
 *
 * HOW TO USE:  Load the 'sensornet' example from RF24Network onto another
 * node.  Give that node any address 1-5.  Load this example on your
 * Nanode or EtherShield-connected Arduino.  Turn them on, and watch
 * your sensor readings pile up on Pachube!
 */

//#define ETHERSHIELD // uncomment to run on EtherShield
#include <NanodeUIP.h>
#include <psock.h>
#include "webclient.h"
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include "printf.h"
#ifndef ETHERSHIELD
#include <NanodeUNIO.h>
#endif

// To ensure that uip-conf.h is set up correctly to accomodate webclient.
UIPASSERT(sizeof(struct webclient_state)<=TCP_APP_STATE_SIZE)

// To ensure that the correct uip-conf.h is being used
#if ! defined(__UIP_CONF_RF24NETWORK__) && ! defined(__AVR_ATmega1284P__)
#error This sketch requires a custom configuration.  Before compiling this sketch, copy uip-conf.h into the NanodeUIP directory
#endif

// Dear compiler, please be quiet about those uninitalized progmem
// warnings.
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] __attribute__ (( section (".progmem") )) = (s); &__c[0];}))

// If the required pins have not previously been defined, let's set the
// values now.
//
// This uses the 'default' pins based on the RF24 Getting Started
// board.  Note that if you use EtherSheild, you'll have a conflict on
// pin 10.  I recommend just moving the RF pins somewhere else.
#ifndef PINS_DEFINED 
const int rf_csn = 10;
const int rf_ce = 9;
const int red_led = 6;
#endif

extern uint16_t* __brkval;

/**************************************************************************/
/**
 * @defgroup radio Radio Details
 *
 * The instances which manage the radio.
 *
 * @{
 */
/**
 * Driver instance for the physical radio module
 */
RF24 radio(rf_ce,rf_csn);

/**
 * Network layer instance
 */
RF24Network network(radio);

/**************************************************************************/
/**
 * @defgroup message Message Handling
 *
 * The definition of the radio message passed between radios, and
 * functions to handle the contents of it.
 *
 * @{
 */
/**
 * Sensor reading message
 *
 * This is the message that we receive, containing a pair of sensor readings
 */
struct message_t
{
  uint16_t temp_reading;
  uint16_t voltage_reading;
  message_t(void): temp_reading(0), voltage_reading(0) {}
  void toCSV(char*,size_t,uint8_t);
};

/**
 * Translate a MCP9700 temperature reading into celcius.
 *
 * @param reading In the range of 0-65536, the reading which was taken from
 * an analog input.  Either take 64 readings or take one reading and shift it
 * up 6 bits.
 *
 * @return Temperature in celcius, in signed 8.8 bit.  0x1080 is 16.5 degrees C,
 * 0xFF00 is -1 degrees C.
 */
int16_t map_mcp9700(uint16_t reading)
{
  // The AREF was 3.3V on the sender
  const long V = 65536L/3.3; // 1V is this value

  // The MCP9700 reads 1/2V at 0C, and 1.75V at 125C
  return map(reading,0.5*V,1.75*V,0,125L*256L);
}

/**
 * Translate a MCP9701 temperature reading into celcius.
 *
 * @param reading In the range of 0-65536, the reading which was taken from
 * an analog input.  Either take 64 readings or take one reading and shift it
 * up 7 bits.
 *
 * @return Temperature in celcius, in signed 8.8 bit.  0x1080 is 16.5 degrees C,
 * 0xFF00 is -1 degrees C.
 */
int16_t map_mcp9701(uint16_t reading)
{
  // The AREF was 3.3V on the sender
  const long V = 65536L/3.3; // 1V is this value

  // The MCP9701 reads 0.4V at 0C, and 3.0V at 133C.
  return map(reading,0.4*V,3.0*V,0,133L*256L);
}

/**
 * Translate a set of sensor readings into the CSV format needed to send
 * to Pachube.
 *
 * The format is tightly coupled to the datastreams in the
 * Pachube feed.  The output will look like this:
 *
 * @code
 * 1-temp,25.50
 * 1-voltage,4.85
 * @endcode
 *
 * This example shows a temp and voltage reading for node #1.  So there
 * must be datastreams named 1-temp and 1-voltage in the feed being
 * posted to.
 */
void message_t::toCSV(char* buf, size_t len, uint8_t node)
{
  // Convert the values from analog measurements to human-
  // readable values.

  // This assumes the 'voltage' input pin has a 1000/470 divider
  // circuit on a 3.3V AREF.  So the max voltage is 4.85.
  // We will use an 8.8-bit fixed point representation
  uint16_t voltage = map(voltage_reading,0,0xFFFF,0,4.85*256);

  // This assumes the 'temp' input pin is coming from a Microchip
  // MCP9701.  It assumes the AREF was 3.3V
  int16_t temp = map_mcp9701(temp_reading);

  snprintf_P(buf,len,PSTR("%o-temp,%i.%02u\r\n%o-voltage,%u.%02u\r\n"),
             node,
             temp>>8,
             (uint16_t)(temp&0xff)*100/256,  // XXX: This mishandles the decimal values when temp<0
             node,
             voltage>>8,
             (voltage&0xff)*100/256
            );
}

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
enum app_states_e
{
  state_none = 0, /**< No state has been set. Illegal */
  state_needip, /**< We are blocked until we get an IP address */
  state_needresolv, /**< We are blocked until we can resolve the host of our service */
  state_noconnection, /**< Everything is ready, webclient needs to be started */
  state_ready, /**< Ready to send */
  state_sending, /**< Ethernet transmission in progress */
  state_invalid  /**< An invalid state.  Illegal */
};

/**
 * The current state the application is in
 */
static app_states_e app_state;

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
 * The Pachube API key.  This gives us the permission to post.  This is a
 * public key to my personal testing feeds.  Feel free to use it in order
 * to try this out, but please don't abuse it!  It's easy enough to create
 * your own free Pachube account and make your own feeds!!
 */
static const char pachube_api_key[] __attribute__ (( section (".progmem") )) = "X-PachubeApiKey: 8pvNK_06BCBDXtRwq96si4ikFtKZn4rtDjmFoejHOG2iTDQpdXnu3jjMoDSk_E5_CRVMtjql79Jbz-4CT9HMR1Bs3LpqsV_sHKzmjuAM00Y574bHA3zGlarGhrmj9cFS";

/**
 * Callback for changes to DHCP status.
 *
 * Called by uIP when the DHCP status changes.  We use it to determine that
 * we now have a valid IP.
 *
 * @param s Current status
 * @param dnsaddr Pointer to the DNS server which the DHCP server gave us.
 */
static void dhcp_status_cb(int s,const uint16_t *dnsaddr)
{
  char buf[20];
  if (s==DHCP_STATUS_OK)
  {
    resolv_conf(dnsaddr);
    uip.get_ip_addr_str(buf);
    printf_P(PSTR("%lu: IP: %s\r\n"),millis(),buf);
    app_flags.have_ip = 1;
  }
}

/**
 * Callback for successful DNS resolutions
 *
 * Called by uIP when a name is successfully resolved.
 *
 * @param name Host name
 * @param addr IP address for that host
 */
static void resolv_found_cb(char *name,uint16_t *addr)
{
  char buf[20];
  uip.format_ipaddr(buf,addr);
  printf_P(PSTR("%lu: DNS: %s has address %s\r\n"),millis(),name,buf);
  app_flags.have_resolv = 1;
}

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
  // Print Preamble
  //

  Serial.begin(38400);
  printf_begin();
  printf_P(PSTR(__FILE__"\r\n"));
  printf_P(PSTR("FREE: %u\r\n"),SP-(uint16_t)__brkval);

  //
  // Bring up the RF network
  //

  SPI.begin();
  radio.begin();
  network.begin(/*channel*/ 92, /*node address*/ 0);

  //
  // Setup Ethernet networking
  //

  char buf[20];
  uip.get_mac_str(buf);
  printf_P(PSTR("MAC: %s\r\n"),buf);
  uip.wait_for_link();
  nanode_log_P(PSTR("Link is up"));
  uip.start_dhcp(dhcp_status_cb);
  uip.init_resolv(resolv_found_cb);
  app_state = state_needip;

  //
  // Setup the LED as a status
  //

  // Status LED will be 'LOW' only when we are state_ready
  pinMode(red_led,OUTPUT);
  digitalWrite(red_led,HIGH);
}

/**
 * Application loop
 */
void loop()
{
  // Check the Ethernet
  uip.poll();

  // Check the radio
  network.update();
  while ( network.available() )
  {
    // If so, grab it and print it out
    RF24NetworkHeader header;
    message_t message;
    network.read(header,&message,sizeof(message));
    printf_P(PSTR("%lu: APP Received %x/%x from %u\n\r"),millis(),message.temp_reading,message.voltage_reading,header.from_node);

    // Validate that it's the expected sensors message 
    if ( header.type == 'S' )
    {
      // Make sure the Ethernet is ready, otherwise discard it
      if ( app_state == state_ready )
      {
	digitalWrite(red_led,HIGH);
        
	// Convert the readings to a pachube CSV
        char reading_buffer[40];
        message.toCSV(reading_buffer,sizeof(reading_buffer),header.from_node);

        // Send it to Pachube
        webclient_put_P(PSTR("api.pachube.com"), 80, PSTR("/v2/feeds/45038.csv"), pachube_api_key, reading_buffer);
        nanode_log_P(PSTR("Sending readings"));
        app_state = state_sending;
      }
      else
        nanode_log_P(PSTR("Ethernet not ready, radio payload discarded."));
    }
    else
      nanode_log_P(PSTR("Invalid radio payload, discarded."));
  }

  switch (app_state)
  {
  case state_needip:
    if ( app_flags.have_ip )
    {
      // launch resolver
      uip.query_name("api.pachube.com");
      app_state = state_needresolv;
    }
    break;
  case state_needresolv:
    if ( app_flags.have_resolv )
    {
      app_state = state_noconnection;
    }
    break;
  case state_noconnection:
  {
    printf_P(PSTR("+READY\r\n"));
    webclient_init();

    app_state = state_ready;
  }
  break;
  case state_ready:
    digitalWrite(red_led,LOW);
  case state_sending:
  case state_none:
  case state_invalid:
    break;
  }
}
/**@}*/

/****************************************************************************/
/**
 * @defgroup web_cb Webclient Callbacks
 *
 * Called by the webclient to signify various states.
 *
 * @{
 */
void webclient_datahandler(char *data, u16_t len)
{
  static uint32_t size_received = 0; /**< How much data have we received this transmission */
  static uint32_t started_at = 0; /**< When did we start receiving? */
  static uint32_t receives_needed = 2; /**< How many more do we need to get before we know it's working? */

  if ( len )
  {
    if ( ! started_at )
      started_at = millis();

    size_received += len;
#if 0
    // Dump out the text
    while(len--)
    {
      char c = *data++;
      if ( c == '\n' )
        Serial.print('\r');
      Serial.print(c);
    }
    Serial.println();
#else
    Serial.print('.');
#endif
  }

  // If data is NULL, we are done.  Print the stats
  if (!data && size_received)
  {
    Serial.println();
    printf_P(PSTR("%lu: DONE. Received %lu bytes in %lu msec.\r\n"),millis(),size_received,millis()-started_at);
    size_received = 0;
    started_at = 0;
    app_state = state_ready;

    // This is purely to enable an expect-based test.  See 'test.ex'.
    if (receives_needed)
    {
      if (!--receives_needed)
	printf_P(PSTR("+OK"));
    }
  }
}

void webclient_connected(void)
{
  uip_log_P(PSTR("webclient_connected"));
}

void webclient_timedout(void)
{
  uip_log_P(PSTR("webclient_timedout\r\n"));
  app_state = state_ready;
}

void webclient_aborted(void)
{
  uip_log_P(PSTR("webclient_aborted\r\n"));
  app_state = state_ready;
}

void webclient_closed(void)
{
  uip_log_P(PSTR("webclient_closed\r\n"));
  app_state = state_ready;
}

// vim:cin:ai:sts=2 sw=2 ft=cpp
