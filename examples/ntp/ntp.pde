/*
 Copyright (C) 2012 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * @file ntp.pde 
 *
 * This example shows how to use the network time protocol.
 *
 * See http://www.faqs.org/rfcs/rfc1305.html
 */

//#define ETHERSHIELD // uncomment to run on EtherShield
#include <NanodeUIP.h>
#include <psock.h>
#include <ntpc.h>
#include "webclient.h"
#include "printf.h"
#include "_rtclib.h"
#ifndef ETHERSHIELD
#include <NanodeUNIO.h>
#endif

// To ensure that uip-conf.h is set up correctly to accomodate webclient.
UIPASSERT(sizeof(struct webclient_state)<=TCP_APP_STATE_SIZE)

// Dear compiler, please be quiet about those uninitalized progmem 
// warnings.
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] __attribute__ (( section (".progmem") )) = (s); &__c[0];}))

enum app_states_e { state_none = 0, state_needip, state_needtime, 
  state_done, state_invalid };

static app_states_e app_state;

struct app_flags_t
{
  uint8_t have_ip:1;
  uint8_t have_time:1;
};

static app_flags_t app_flags;

RTC_Millis rtc;

static void dhcp_status(int s,const uint16_t *) {
  char buf[20];
  if (s==DHCP_STATUS_OK) {
    uip.get_ip_addr_str(buf);
    printf_P(PSTR("%lu: IP: %s\r\n"),millis(),buf);
    app_flags.have_ip = 1;
  }
}

void ntpc_configured(const ntpc_state* s)
{
  rtc.adjust(DateTime(s->unixtime));
  char buf[32];
  rtc.now().toString(buf,sizeof(buf));
  printf_P(PSTR("%lu: Got time %s\r\n"),millis(),buf);
  app_flags.have_time = 1;
}

extern uint16_t* __brkval;

uip_ipaddr_t ntpserver;

void setup() {

#ifdef ETHERSHIELD // EtherShield
  byte macaddr[6] = { 0x2, 0x00, 0x00, 0x1, 0x2, 0x3 };
  uip.init(macaddr,SS);
#else  // Nanode
  byte macaddr[6];
  NanodeUNIO unio(NANODE_MAC_DEVICE);
  unio.read(macaddr,NANODE_MAC_ADDRESS,6);
  uip.init(macaddr);
#endif

  Serial.begin(38400);
  printf_begin();
  printf_P(PSTR(__FILE__"\r\n"));
  printf_P(PSTR("FREE: %u\r\n"),SP-(uint16_t)__brkval);

  char buf[20];
  uip.get_mac_str(buf);
  printf_P(PSTR("MAC: %s\r\n"),buf);
  uip.wait_for_link();
  nanode_log_P(PSTR("Link is up"));
  uip.start_dhcp(dhcp_status);
  app_state = state_needip;
  uip_ipaddr(ntpserver,192, 43, 244, 18);
  
  printf_P(PSTR("+READY\r\n"));
}

void loop() {
  uip.poll();

  switch (app_state)
  {
    case state_needip:
      if ( app_flags.have_ip )
      {
	// launch time client
	ntpc_init(&ntpserver);
	app_state = state_needtime;
      }
      break;
    case state_needtime:
      if ( app_flags.have_time )
      {
	app_state = state_done;
      }
      break;
    case state_done:
      printf_P(PSTR("+OK\r\n"));
      app_state = state_none;
      break;
    case state_none:
    case state_invalid:
      break;
  }
}

// Stats
uint32_t size_received = 0;
uint32_t started_at = 0;

/****************************************************************************/
void webclient_datahandler(char *data, u16_t len)
{
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
  }
}

// vim:cin:ai:sts=2 sw=2 ft=cpp
// vim:cin:ai:sts=2 sw=2 ft=cpp
