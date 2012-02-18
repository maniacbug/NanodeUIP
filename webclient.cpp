/**
 * \addtogroup apps
 * @{
 */

/**
 * \defgroup webclient Web client
 * @{
 *
 * This example shows a HTTP client that is able to download web pages
 * and files from web servers. It requires a number of callback
 * functions to be implemented by the module that utilizes the code:
 * webclient_datahandler(), webclient_connected(),
 * webclient_timedout(), webclient_aborted(), webclient_closed().
 */

/**
 * \file
 * Implementation of the HTTP client.
 * \author Adam Dunkels <adam@dunkels.com>
 */

/*
 * Copyright (c) 2002, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: webclient.c,v 1.2 2006/06/11 21:46:37 adam Exp $
 *
 */

#include <NanodeUIP.h>
#include <string.h>
#include <stdlib.h>

extern "C"
{
#include "uiplib.h"
}

#include "resolv.h"
#include "webclient.h"
#include "webclient-strings.h"

#define WEBCLIENT_TIMEOUT 20

#define WEBCLIENT_STATE_STATUSLINE 0
#define WEBCLIENT_STATE_HEADERS    1
#define WEBCLIENT_STATE_DATA       2
#define WEBCLIENT_STATE_CLOSE      3

#define HTTPFLAG_NONE   0
#define HTTPFLAG_OK     1
#define HTTPFLAG_MOVED  2
#define HTTPFLAG_ERROR  3

#define REQUEST_TYPE_GET	0
#define REQUEST_TYPE_PUT	1
#define REQUEST_TYPE_POST	2

#define ISO_nl       0x0a
#define ISO_cr       0x0d
#define ISO_space    0x20

#define DNS 1

#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] __attribute__ (( section (".progmem") )) = (s); &__c[0];}))

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
static struct webclient_state* ps;

/*-----------------------------------------------------------------------------------*/
char *
webclient_mimetype(void)
{
  return ps->mimetype;
}
/*-----------------------------------------------------------------------------------*/
char *
webclient_filename(void)
{
  return ps->file;
}
/*-----------------------------------------------------------------------------------*/
char *
webclient_hostname(void)
{
  return ps->host;
}
/*-----------------------------------------------------------------------------------*/
unsigned short
webclient_port(void)
{
  return ps->port;
}
/*-----------------------------------------------------------------------------------*/
void
webclient_init(void)
{

}
/*-----------------------------------------------------------------------------------*/
static void
init_connection(void)
{
  ps->state = WEBCLIENT_STATE_STATUSLINE;

  ps->getrequestleft = sizeof(http_get) - 1 + 1 +
    sizeof(http_11) - 1 +
    sizeof(http_crnl) - 1 +
    sizeof(http_host) - 1 +
    sizeof(http_crnl) - 1 +
    strlen_P(http_user_agent_fields) +
    strlen(ps->file) + strlen(ps->host);

  if ( ps->request_type == REQUEST_TYPE_POST || ps->request_type == REQUEST_TYPE_PUT ) 
    ps->getrequestleft += sizeof(http_content_length) - 1 + 3 +
      sizeof(http_crnl) - 1 +
      sizeof(ps->body);

  if ( ps->progmem && pgm_read_byte(ps->extra_headers) )
    ps->getrequestleft += strlen_P(ps->extra_headers) +
    sizeof(http_crnl) - 1;
  else if ( ! ps->progmem && ps->extra_headers[0] )
    ps->getrequestleft += strlen(ps->extra_headers) +
    sizeof(http_crnl) - 1;

  ps->getrequestptr = 0;

  ps->httpheaderlineptr = 0;
}
/*-----------------------------------------------------------------------------------*/
void
webclient_close(void)
{
  ps->state = WEBCLIENT_STATE_CLOSE;
}
/*-----------------------------------------------------------------------------------*/
unsigned char
webclient_get(const char *host, u16_t port, const char *file)
{
  struct uip_conn *conn;
  uip_ipaddr_t *ipaddr;
  static uip_ipaddr_t addr;
  
  /* First check if the host is an IP addresps-> */
  ipaddr = &addr;
  if(uiplib_ipaddrconv(const_cast<char*>(host), (unsigned char *)addr) == 0) {
    ipaddr = 0;
#ifdef DNS
    ipaddr = (uip_ipaddr_t *)resolv_lookup((char*)host);  // cast away const unsafe!
#endif
    if(ipaddr == NULL) {
      UIP_LOG("Resolv lookup failed");
      return 0;
    }
  }
  
  conn = uip_connect(ipaddr, uip_htons(port),webclient_appcall);
  
  if(conn == NULL) {
    UIP_LOG("Open connection failed");
    return 0;
  }
  
  ps = reinterpret_cast<webclient_state*>(&(conn->appstate));
  ps->request_type = REQUEST_TYPE_GET; 
  ps->port = port;
  ps->body[0] = 0;
  ps->extra_headers = PSTR("");
  ps->progmem = 1;
  strncpy(ps->file, file, sizeof(ps->file));
  strncpy(ps->host, host, sizeof(ps->host));
  
  init_connection();
  uip_log_P(PSTR("Connection started."));
  return 1;
}
/*-----------------------------------------------------------------------------------*/
unsigned char
webclient_get_P(const char *host, u16_t port, const char *file)
{
  unsigned char result;

  char* buf_host = reinterpret_cast<char*>(malloc(strlen_P(host)+1));
  char* buf_file = reinterpret_cast<char*>(malloc(strlen_P(file)+1));

  strcpy_P(buf_host,host);
  strcpy_P(buf_file,file);

  result = webclient_get(buf_host,port,buf_file);

  free (buf_file);
  free (buf_host);

  return result;
}
/*-----------------------------------------------------------------------------------*/
unsigned char
webclient_put_P(const char * host, u16_t port, const char * file, const char* extra_headers, const char* body )
{
  static uip_ipaddr_t addr;

  // space for the host in RAM
  char host_buf[32];
  strncpy_P(host_buf,host,sizeof(host_buf));
  
  
  /* First check if the host is an IP address */
  uip_ipaddr_t *ipaddr = &addr;
  if(uiplib_ipaddrconv(host_buf, (unsigned char *)addr) == 0) {
    ipaddr = 0;
#ifdef DNS
    ipaddr = (uip_ipaddr_t *)resolv_lookup(host_buf);  // cast away const unsafe!
#endif
    if(ipaddr == NULL) {
      return 0;
    }
  }
  
  struct uip_conn *conn = uip_connect(ipaddr, uip_htons(port),webclient_appcall);
  if(conn == NULL) {
    return 0;
  }

  ps = reinterpret_cast<webclient_state*>(&(conn->appstate));
  strncpy_P(ps->host, host, sizeof(ps->host));
  strncpy_P(ps->file, file, sizeof(ps->file));
  
  ps->extra_headers = extra_headers;
  ps->progmem = 1;
  strncpy(ps->body, body, sizeof(ps->body));
  ps->body[sizeof(ps->body)-1] = 0;

  ps->request_type = REQUEST_TYPE_PUT; 
  ps->port = port;
  
  init_connection();
  uip_log_P(PSTR("Connection started."));
  return 1;
}
/*-----------------------------------------------------------------------------------*/
unsigned char
webclient_post(const char * host, u16_t port, const char * file, const char* extra_headers, const char* body )
{
  static uip_ipaddr_t addr;
  
  /* First check if the host is an IP address */
  uip_ipaddr_t *ipaddr = &addr;
  if(uiplib_ipaddrconv(const_cast<char*>(host), (unsigned char *)addr) == 0) {
    ipaddr = 0;
#ifdef DNS
    ipaddr = (uip_ipaddr_t *)resolv_lookup(host);  // cast away const unsafe!
#endif
    if(ipaddr == NULL) {
      return 0;
    }
  }
  
  struct uip_conn *conn = uip_connect(ipaddr, uip_htons(port),webclient_appcall);
  if(conn == NULL) {
    return 0;
  }

  ps = reinterpret_cast<webclient_state*>(&(conn->appstate));
  strncpy(ps->host, host, sizeof(ps->host));
  strncpy(ps->file, file, sizeof(ps->file));
  
  ps->extra_headers = extra_headers;
  ps->progmem = 0;
  strncpy(ps->body, body, sizeof(ps->body));
  ps->body[sizeof(ps->body)-1] = 0;

  ps->request_type = REQUEST_TYPE_POST; 
  ps->port = port;
  
  init_connection();
  uip_log_P(PSTR("Connection started."));
  return 1;
}
/*-----------------------------------------------------------------------------------*/
static char *
copy_string(char *dest,
	    const char *src, u16_t len)
{
  strncpy(dest, src, len);
  return dest + len;
}
/*-----------------------------------------------------------------------------------*/
static char *
copy_string_P(char *dest,
	    const char *src, u16_t len)
{
  strncpy_P(dest, src, len);
  return dest + len;
}
/*-----------------------------------------------------------------------------------*/
static void
senddata(void)
{
  u16_t len;
  char *getrequest;
  char *cptr;
  
  if(ps->getrequestleft > 0) {
    cptr = getrequest = (char *)uip_appdata;

    if ( ps->request_type == REQUEST_TYPE_GET )
      cptr = copy_string_P(cptr, http_get, sizeof(http_get) - 1);
    else if ( ps->request_type == REQUEST_TYPE_PUT )
      cptr = copy_string_P(cptr, http_put, sizeof(http_put) - 1);
    else if ( ps->request_type == REQUEST_TYPE_POST )
      cptr = copy_string_P(cptr, http_post, sizeof(http_post) - 1);
    
    cptr = copy_string(cptr, ps->file, strlen(ps->file));
    *cptr++ = ISO_space;
    cptr = copy_string_P(cptr, http_11, sizeof(http_11) - 1);

    cptr = copy_string_P(cptr, http_crnl, sizeof(http_crnl) - 1);
    
    cptr = copy_string_P(cptr, http_host, sizeof(http_host) - 1);
    cptr = copy_string(cptr, ps->host, strlen(ps->host));
    cptr = copy_string_P(cptr, http_crnl, sizeof(http_crnl) - 1);

    if ( ps->request_type == REQUEST_TYPE_POST || ps->request_type == REQUEST_TYPE_PUT ) {
      cptr = copy_string_P(cptr, http_content_length, sizeof(http_content_length) - 1);
      char buf[4];
      snprintf(buf,sizeof(buf),"%03u",strlen(ps->body));
      cptr = copy_string(cptr, buf, 3);
      cptr = copy_string_P(cptr, http_crnl, sizeof(http_crnl) - 1);
    } 

    if ( ps->progmem && pgm_read_byte(ps->extra_headers) ) {
      cptr = copy_string_P(cptr, ps->extra_headers, strlen_P(ps->extra_headers));
      cptr = copy_string_P(cptr, http_crnl, sizeof(http_crnl) - 1);
    }
    else if ( ! ps->progmem && ps->extra_headers[0] ) {
      cptr = copy_string(cptr, ps->extra_headers, strlen(ps->extra_headers));
      cptr = copy_string_P(cptr, http_crnl, sizeof(http_crnl) - 1);
    }

    cptr = copy_string_P(cptr, http_user_agent_fields,
		       strlen_P(http_user_agent_fields));
    
    if ( ps->body[0] ) {
      cptr = copy_string(cptr,ps->body,strlen(ps->body));
    }
    
    len = ps->getrequestleft > uip_mss()?
      uip_mss():
      ps->getrequestleft;
    uip_send(&(getrequest[ps->getrequestptr]), len);

    *cptr++ = 0;
    uip_log(getrequest);
  }
}
/*-----------------------------------------------------------------------------------*/
static void
acked(void)
{
  u16_t len;
  
  if(ps->getrequestleft > 0) {
    len = ps->getrequestleft > uip_mss()?
      uip_mss():
      ps->getrequestleft;
    ps->getrequestleft -= len;
    ps->getrequestptr += len;
  }
}
/*-----------------------------------------------------------------------------------*/
char *uip_appdata_ptr;
/*-----------------------------------------------------------------------------------*/
static u16_t
parse_statusline(u16_t len)
{
  char *cptr;
  uint8_t result_code_at;
  
  while(len > 0 && ps->httpheaderlineptr < sizeof(ps->httpheaderline)) {
    ps->httpheaderline[ps->httpheaderlineptr] = *uip_appdata_ptr++;
    --len;
    if(ps->httpheaderline[ps->httpheaderlineptr] == ISO_nl) {

      result_code_at = 0; 
      if((strncmp_P(ps->httpheaderline, http_10, sizeof(http_10) - 1) == 0))
        result_code_at = sizeof(http_10);
      else if((strncmp_P(ps->httpheaderline, http_11, sizeof(http_11) - 1) == 0))
        result_code_at = sizeof(http_11);
      else if((strncmp_P(ps->httpheaderline, http_icy, sizeof(http_icy) - 1) == 0))
        result_code_at = sizeof(http_icy);

      if(result_code_at) {
	cptr = &(ps->httpheaderline[result_code_at]);
	ps->httpflag = HTTPFLAG_NONE;
	if(strncmp_P(cptr, http_200, sizeof(http_200) - 1) == 0) {
	  /* 200 OK */
	  ps->httpflag = HTTPFLAG_OK;
	  uip_log_P(PSTR("HTTP OK"));
	} else if(strncmp_P(cptr, http_301, sizeof(http_301) - 1) == 0 ||
		  strncmp_P(cptr, http_302, sizeof(http_302) - 1) == 0) {
	  /* 301 Moved permanently or 302 Found. Location: header line
	     will contain thw new location. */
	  ps->httpflag = HTTPFLAG_MOVED;
	  uip_log_P(PSTR("HTTP MOVED"));
	} else {
	  ps->httpheaderline[ps->httpheaderlineptr - 1] = 0;
          uip_log(ps->httpheaderline);
	}
      } else {
	uip_log_P(PSTR("HTTP FAILED"));
        ps->httpheaderline[ps->httpheaderlineptr - 1] = 0;
        uip_log(ps->httpheaderline);
	uip_abort();
	webclient_aborted();
	return 0;
      }
      
      /* We're done parsing the status line, so we reset the pointer
	 and start parsing the HTTP headers*/
      ps->httpheaderlineptr = 0;
      ps->state = WEBCLIENT_STATE_HEADERS;
      break;
    } else {
      ++ps->httpheaderlineptr;
    }
  }
  return len;
}
/*-----------------------------------------------------------------------------------*/
#if 0
// defined but not used
static char
casecmp(char *str1, const char *str2, char len)
{
  static char c;
  
  while(len > 0) {
    c = *str1;
    /* Force lower-case characters */
    if(c & 0x40) {
      c |= 0x20;
    }
    if(*str2 != c) {
      return 1;
    }
    ++str1;
    ++str2;
    --len;
  }
  return 0;
}
#endif
/*-----------------------------------------------------------------------------------*/
static char
casecmp_P(char *str1, const char *str2, char len)
{
  static char c;
  
  while(len > 0) {
    c = *str1;
    /* Force lower-case characters */
    if(c & 0x40) {
      c |= 0x20;
    }
    if(pgm_read_byte(str2) != c) {
      return 1;
    }
    ++str1;
    ++str2;
    --len;
  }
  return 0;
}
/*-----------------------------------------------------------------------------------*/
static u16_t
parse_headers(u16_t len)
{
  char *cptr;
  static unsigned char i;

  while(len > 0 && ps->httpheaderlineptr < sizeof(ps->httpheaderline)) {
    ps->httpheaderline[ps->httpheaderlineptr] = *uip_appdata_ptr++;
    --len;
    if(ps->httpheaderline[ps->httpheaderlineptr] == ISO_nl) {
      /* We have an entire HTTP header line in ps->httpheaderline, so
	 we parse it. */
      if(ps->httpheaderline[0] == ISO_cr) {
	/* This was the last header line (i.e., and empty "\r\n"), so
	   we are done with the headers and proceed with the actual
	   data. */
	ps->state = WEBCLIENT_STATE_DATA;
        uip_log_P(PSTR("WEBCLIENT_STATE_DATA"));
	return len;
      }

      ps->httpheaderline[ps->httpheaderlineptr - 1] = 0;
      uip_log(ps->httpheaderline);
      /* Check for specific HTTP header fields. */
      if(casecmp_P(ps->httpheaderline, http_content_type,
		     sizeof(http_content_type) - 1) == 0) {
	/* Found Content-type field. */
	cptr = strchr(ps->httpheaderline, ';');
	if(cptr != NULL) {
	  *cptr = 0;
	}
	strncpy_P(ps->mimetype, ps->httpheaderline +
		sizeof(http_content_type) - 1, sizeof(ps->mimetype));
      } else if(casecmp_P(ps->httpheaderline, http_location,
			    sizeof(http_location) - 1) == 0) {
	cptr = ps->httpheaderline + sizeof(http_location) - 1;
	
	if(strncmp_P(cptr, http_http, 7) == 0) {
	  cptr += 7;
	  for(i = 0; i < ps->httpheaderlineptr - 7; ++i) {
	    if(*cptr == 0 ||
	       *cptr == '/' ||
	       *cptr == ' ' ||
	       *cptr == ':') {
	      ps->host[i] = 0;
	      break;
	    }
	    ps->host[i] = *cptr;
	    ++cptr;
	  }
	}
	strncpy(ps->file, cptr, sizeof(ps->file));
	/*	ps->file[ps->httpheaderlineptr - i] = 0;*/
      }


      /* We're done parsing, so we reset the pointer and start the
	 next line. */
      ps->httpheaderlineptr = 0;
    } else {
      ++ps->httpheaderlineptr;
    }
  }

  /* Handle header line overflow */
  if ( ps->httpheaderlineptr == sizeof(ps->httpheaderline) ) {
    ps->httpheaderline[ps->httpheaderlineptr - 1] = 0;
    uip_log(ps->httpheaderline);
    ps->httpheaderlineptr = 0;
  }

  return len;
}
/*-----------------------------------------------------------------------------------*/
static void
newdata(void)
{
  u16_t len;

  len = uip_datalen();
  uip_appdata_ptr = reinterpret_cast<char*>(uip_appdata);

  if(ps->state == WEBCLIENT_STATE_STATUSLINE) {
    len = parse_statusline(len);
  }
  
  if(ps->state == WEBCLIENT_STATE_HEADERS && len > 0) {
    len = parse_headers(len);
  }

  if(len > 0 && ps->state == WEBCLIENT_STATE_DATA &&
     ps->httpflag != HTTPFLAG_MOVED) {
    webclient_datahandler(uip_appdata_ptr, len);
  }
}
/*-----------------------------------------------------------------------------------*/
void
webclient_appcall(void)
{
  ps = reinterpret_cast<webclient_state*>(&(uip_conn->appstate));
  if(uip_connected()) {
    ps->timer = 0;
    ps->state = WEBCLIENT_STATE_STATUSLINE;
    senddata();
    webclient_connected();
    return;
  }

  if(ps->state == WEBCLIENT_STATE_CLOSE) {
    webclient_closed();
    uip_abort();
    return;
  }

  if(uip_aborted()) {
    webclient_aborted();
  }
  if(uip_timedout()) {
    webclient_timedout();
  }

  
  if(uip_acked()) {
    ps->timer = 0;
    acked();
  }
  if(uip_newdata()) {
    ps->timer = 0;
    newdata();
  }
  if(uip_rexmit() ||
     uip_newdata() ||
     uip_acked()) {
    senddata();
  } else if(uip_poll()) {
    ++ps->timer;
    if(ps->timer == WEBCLIENT_TIMEOUT) {
      webclient_timedout();
      uip_abort();
      return;
    }
        /*    senddata();*/
  }

  if(uip_closed()) {
    if(ps->httpflag != HTTPFLAG_MOVED) {
      /* Send NULL data to signal EOF. */
      webclient_datahandler(NULL, 0);
    } else {
#if DNS
      if(resolv_lookup(ps->host) == NULL) {
	resolv_query(ps->host);
      }
#endif
      webclient_get(ps->host, ps->port, ps->file);
    }
  }
}
/*-----------------------------------------------------------------------------------*/

/** @} */
/** @} */
