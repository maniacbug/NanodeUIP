/**
 * \addtogroup apps
 * @{
 */

/**
 * \defgroup httpd Web server
 * @{
 * The uIP web server is a very simplistic implementation of an HTTP
 * server. It can serve web pages and files from a read-only ROM
 * filesystem, and provides a very small scripting language.

 */

/**
 * \file
 *         Web server
 * \author
 *         Adam Dunkels <adam@sics.se>
 */


/*
 * Copyright (c) 2004, Adam Dunkels.
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
 * This file is part of the uIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: httpd.c,v 1.2 2006/06/11 21:46:38 adam Exp $
 */

#include "uip.h"
#include "httpd.h"
#include "httpd_fs.h"
#include "httpd_cgi.h"
#include "webclient-strings.h"

#include <string.h>

#define STATE_WAITING 0
#define STATE_OUTPUT  1

#define ISO_nl      0x0a
#define ISO_space   0x20
#define ISO_bang    0x21
#define ISO_percent 0x25
#define ISO_period  0x2e
#define ISO_slash   0x2f
#define ISO_colon   0x3a

#if UIP_LOGGING == 1
#include <avr/pgmspace.h>
#include <stdio.h>
void uip_log_P(PGM_P msg);
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] __attribute__ (( section (".progmem") )) = (s); &__c[0];}))
#define UIP_LOG(m) uip_log_P(PSTR(m))
#else
#define UIP_LOG(m)
#endif /* UIP_LOGGING == 1 */

/*---------------------------------------------------------------------------*/
static unsigned short
generate_part_of_file(void *state)
{
  struct httpd_state *s = (struct httpd_state *)state;

  if(s->file.len > (int)uip_mss()) {
    s->len = (int)uip_mss();
  } else {
    s->len = s->file.len;
  }
  memcpy_P(uip_appdata, s->file.data, s->len );
  
  return s->len;
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_file(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sout);
  
  do {
    PSOCK_GENERATOR_SEND(&s->sout, generate_part_of_file, s);
    s->file.len -= s->len;
    s->file.data += s->len;
  } while(s->file.len > 0);
      
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_part_of_file(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sout);

#if UIP_LOGGING == 1
  printf_P(PSTR("send part %p %u\r\n"),(s->file.data), s->len );
#endif

  PSOCK_SEND_P(&s->sout, reinterpret_cast<uint8_t*>(s->file.data), s->len);
  
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static void
next_scriptstate(struct httpd_state *s)
{
  char *p;
  p = (char*)strchrnul_P(s->scriptptr, ISO_nl) + 1;
#if UIP_LOGGING == 1
  printf_P(PSTR("Script state ptr=%p p=%p\r\n"),s->scriptptr,p);
#endif
  s->scriptlen -= (unsigned short)(p - s->scriptptr);
  s->scriptptr = p;
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_script(struct httpd_state *s))
{
  char *ptr;
  UIP_LOG("httpd: processing script");
  
  PT_BEGIN(&s->scriptpt);


  while(s->file.len > 0) {

    /* Check if we should start executing a script. */
    if(pgm_read_byte(s->file.data) == ISO_percent &&
       pgm_read_byte(s->file.data + 1) == ISO_bang) {
      s->scriptptr = s->file.data + 3;
      s->scriptlen = s->file.len - 3;
      if(pgm_read_byte(s->scriptptr - 1) == ISO_colon) {
	UIP_LOG("httpd: sending file");
	httpd_fs_open_P(s->scriptptr + 1, &s->file);
	PT_WAIT_THREAD(&s->scriptpt, send_file(s));
      } else {
	UIP_LOG("httpd: calling cgi");
	PT_WAIT_THREAD(&s->scriptpt,
		       httpd_cgi(s->scriptptr)(s, s->scriptptr));
      }
      next_scriptstate(s);
      
      /* The script is over, so we reset the pointers and continue
	 sending the rest of the file. */
      s->file.data = s->scriptptr;
      s->file.len = s->scriptlen;
    } else {
      /* See if we find the start of script marker in the block of HTML
	 to be sent. */

      if(s->file.len > (int)uip_mss()) {
	s->len = (int)uip_mss();
      } else {
	s->len = s->file.len;
      }

      if(pgm_read_byte(s->file.data) == ISO_percent) {
	ptr = (char*)strchr_P(s->file.data + 1, ISO_percent);
      } else {
	ptr = (char*)strchr_P(s->file.data, ISO_percent);
      }
      if(ptr != NULL &&
	 ptr != s->file.data) {
	s->len = (int)(ptr - s->file.data);
	if(s->len >= (int)uip_mss()) {
	  s->len = (int)uip_mss();
	}
      }
      PT_WAIT_THREAD(&s->scriptpt, send_part_of_file(s));
      s->file.data += s->len;
      s->file.len -= s->len;
    }
  }
  
  PT_END(&s->scriptpt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_headers(struct httpd_state *s, const char *statushdr))
{
  char *ptr;

  PSOCK_BEGIN(&s->sout);

  PSOCK_SEND_P(&s->sout, (uint8_t*)statushdr,strlen_P(statushdr));

  ptr = strrchr(s->filename, ISO_period);
  if(ptr == NULL) {
    PSOCK_SEND_P(&s->sout, (uint8_t*)http_content_type_binary,sizeof(http_content_type_binary)-1);
  } else if(strncmp_P(ptr, http_html, sizeof(http_html)-1) == 0 ||
	    strncmp_P(ptr, http_shtml, sizeof(http_shtml)-1) == 0) {
    PSOCK_SEND_P(&s->sout, (uint8_t*)http_content_type_html, sizeof(http_content_type_html)-1 );
  } else if(strncmp_P(ptr, http_css, sizeof(http_css)-1 ) == 0) {
    PSOCK_SEND_P(&s->sout, (uint8_t*)http_content_type_css,sizeof(http_content_type_css)-1);
  } else if(strncmp_P(ptr, http_png, sizeof(http_png)-1 ) == 0) {
    PSOCK_SEND_P(&s->sout, (uint8_t*)http_content_type_png,sizeof(http_content_type_png)-1);
  } else if(strncmp_P(ptr, http_gif,  sizeof(http_gif)-1) == 0) {
    PSOCK_SEND_P(&s->sout, (uint8_t*)http_content_type_gif,sizeof(http_content_type_gif)-1);
  } else if(strncmp_P(ptr, http_jpg, sizeof(http_jpg)-1) == 0) {
    PSOCK_SEND_P(&s->sout, (uint8_t*)http_content_type_jpg,sizeof(http_content_type_jpg)-1);
  } else {
    PSOCK_SEND_P(&s->sout, (uint8_t*)http_content_type_plain,sizeof(http_content_type_plain)-1);
  }
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_output(struct httpd_state *s))
{
  char *ptr;
  
  PT_BEGIN(&s->outputpt);
 
  if(!httpd_fs_open(s->filename, &s->file)) {
    httpd_fs_open(http_404_html, &s->file);
    strcpy_P(s->filename, http_404_html);
    PT_WAIT_THREAD(&s->outputpt,
		   send_headers(s,
		   http_header_404));
    PT_WAIT_THREAD(&s->outputpt,
		   send_file(s));
  } else {
    PT_WAIT_THREAD(&s->outputpt,
		   send_headers(s,
		   http_header_200));
    ptr = strchr(s->filename, ISO_period);
    if(ptr != NULL && strncmp_P(ptr, http_shtml, 6) == 0) {
      PT_INIT(&s->scriptpt);
      PT_WAIT_THREAD(&s->outputpt, handle_script(s));
    } else {
      PT_WAIT_THREAD(&s->outputpt,
		     send_file(s));
    }
  }
  UIP_LOG("httpd: file sent");
  PSOCK_CLOSE(&s->sout);
  PT_END(&s->outputpt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_input(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sin);
  
  UIP_LOG("httpd: reading");

  PSOCK_READTO(&s->sin, ISO_space);
  if(strncmp_P(s->inputbuf, http_get, 4) != 0) {
    UIP_LOG("httpd: no get, closing");
    PSOCK_CLOSE_EXIT(&s->sin);
  }
  PSOCK_READTO(&s->sin, ISO_space);

  if(s->inputbuf[0] != ISO_slash) {
    UIP_LOG("httpd: no slash, closing");
    PSOCK_CLOSE_EXIT(&s->sin);
  }

  if(s->inputbuf[1] == ISO_space) {
    strncpy_P(s->filename, http_index_shtml, sizeof(s->filename));
  } else {
    s->inputbuf[PSOCK_DATALEN(&s->sin) - 1] = 0;
    strncpy(s->filename, &s->inputbuf[0], sizeof(s->filename));

  }

  s->state = STATE_OUTPUT;

  while(1) {
    PSOCK_READTO(&s->sin, ISO_nl);

    if(strncmp_P(s->inputbuf, http_referer, 8) == 0) {
      s->inputbuf[PSOCK_DATALEN(&s->sin) - 2] = 0;
    }
  }
  
  PSOCK_END(&s->sin);
}
/*---------------------------------------------------------------------------*/
static void
handle_connection(struct httpd_state *s)
{
  handle_input(s);
  if(s->state == STATE_OUTPUT) {
    handle_output(s);
  }
}
/*---------------------------------------------------------------------------*/
void
httpd_appcall(void)
{
  struct httpd_state *s = (struct httpd_state *)&(uip_conn->appstate);

  if(uip_closed() || uip_aborted() || uip_timedout()) {
    UIP_LOG("httpd: closed");
  } else if(uip_connected()) {
    PSOCK_INIT(&s->sin, reinterpret_cast<uint8_t*>(s->inputbuf), sizeof(s->inputbuf) - 1);
    PSOCK_INIT(&s->sout, reinterpret_cast<uint8_t*>(s->inputbuf), sizeof(s->inputbuf) - 1);
    PT_INIT(&s->outputpt);
    s->state = STATE_WAITING;
    UIP_LOG("httpd: connected");
    /*    timer_set(&s->timer, CLOCK_SECOND * 100);*/
    s->timer = 0;
    handle_connection(s);
  } else if(s != NULL) {
    if(uip_poll()) {
      ++s->timer;
      if(s->timer >= 20) {
	uip_abort();
        UIP_LOG("httpd: abort");
      }
    } else {
      s->timer = 0;
    }
    handle_connection(s);
  } else {
    uip_abort();
    UIP_LOG("httpd: abort");
  }
}
/*---------------------------------------------------------------------------*/
/**
 * \brief      Initialize the web server
 *
 *             This function initializes the web server and should be
 *             called at system boot-up.
 */
void
httpd_init(void)
{
  uip_listen(UIP_HTONS(80),httpd_appcall);
}
/*---------------------------------------------------------------------------*/
/** @} */
// vim:cin:ai:sts=2 sw=2 ft=cpp
