/*
 * Copyright (c) 2012, J. Coliz <maniacbug@ymail.com> 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is inteded to work with the uIP TCP/IP stack
 */

#include <stdio.h>
#include <string.h>

#include "uip.h"
#include "ntpc.h"
#include "timer.h"
#include "pt.h"

#define STATE_INITIAL         0
#define STATE_SENDING         1
#define STATE_RECEIVED        2

static struct ntpc_state s;

struct ntp_msg {
  u8_t LI:2;
  u8_t version:3;
  u8_t mode:3;
  u8_t stratum;
  int8_t poll;
  int8_t precision;
  u8_t root_delay[4];
  u8_t root_dispersion[4];
  u8_t reference_id[4];
  ntp_timestamp reference_timestamp;
  ntp_timestamp origin_timestamp;
  ntp_timestamp receive_timestamp;
  ntp_timestamp transmit_timestamp;
};

enum NTP_LI_e {
  NTP_LI_none,
  NTP_LI_61_sec,
  NTP_LI_59_sec,
  NTP_LI_unknown
};

enum NTP_mode_e { 
  NTP_mode_reserved,
  NTP_mode_symmetric_active,
  NTP_mode_symmetric_passive,
  NTP_mode_client,
  NTP_mode_server,
  NTP_mode_broadcast,
  NTP_mode_control_message,
  NTP_mode_private,
};

#define NTPC_SERVER_PORT  123
#define NTPC_CLIENT_PORT  124

/*---------------------------------------------------------------------------*/
static void
create_msg(register struct ntp_msg *m)
{
  memset(m,0,sizeof(ntp_msg));
  m->LI = NTP_LI_unknown;
  m->version = 4;
  m->mode = NTP_mode_client;
  m->poll = 6;
  m->precision = -2;  
  memcpy(m->reference_id,"ACTS",4);
}
/*---------------------------------------------------------------------------*/
static void
send_request(void)
{
  struct ntp_msg *m = (struct ntp_msg *)uip_appdata;

  create_msg(m);
  
  uip_send(uip_appdata, sizeof(ntp_msg)); 
}
/*---------------------------------------------------------------------------*/
static u8_t
parse_msg(void)
{
  struct ntp_msg *m = (struct ntp_msg *)uip_appdata;
  uint16_t* hisec = m->receive_timestamp.seconds;
  uint16_t* losec = m->receive_timestamp.seconds + 1;
  uint32_t seconds = ((uint32_t)(UIP_HTONS(*hisec)) << 16) | UIP_HTONS(*losec);
  s.unixtime = seconds - 2208988800UL; // Convert from NTP seconds to unix time
  return 1;
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_ntp(void))
{
  PT_BEGIN(&s.pt);

  /* try_again:*/
  s.state = STATE_SENDING;
  s.ticks = CLOCK_SECOND;

  do {
    send_request();
    /* Sending does not clear the NEWDATA flag.  The packet doesn't
       actually get sent until we yield at least once.  If we don't
       clear the flag ourselves, we will enter an infinite loop here.
       This is arguably a bug in uip.c and the uip_send() function
       should probably clear the NEWDATA flag. */
    uip_flags=uip_flags&(~UIP_NEWDATA);
    timer_set(&s.timer, s.ticks);
    PT_WAIT_UNTIL(&s.pt, uip_newdata() || timer_expired(&s.timer));

    if(uip_newdata() && parse_msg() ) {
      s.state = STATE_RECEIVED;
      break;
    }

    if(s.ticks < CLOCK_SECOND * 20) {
      s.ticks *= 2;
    }
  } while(s.state != STATE_RECEIVED);

  s.ticks = CLOCK_SECOND;

  ntpc_configured(&s);
  
  /*  timer_stop(&s.timer);*/

  /*
   * PT_END restarts the thread so we do this instead. Eventually we
   * should reacquire expired leases here.
   */
  while(1) {
    PT_YIELD(&s.pt);
  }

  PT_END(&s.pt);
}
/*---------------------------------------------------------------------------*/
int
ntpc_init(const uip_ipaddr_t* addr)
{
  s.state = STATE_INITIAL;
  s.conn = uip_udp_new(const_cast<uip_ipaddr_t*>(addr), UIP_HTONS(NTPC_SERVER_PORT),ntpc_appcall);
  if(s.conn != NULL) {
    uip_udp_bind(s.conn, UIP_HTONS(NTPC_CLIENT_PORT));
    PT_INIT(&s.pt);
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
void
ntpc_appcall(void)
{
  handle_ntp();
}
/*---------------------------------------------------------------------------*/
