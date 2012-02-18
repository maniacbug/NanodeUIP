/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/*
 * Copyright (c) 2001-2006, Adam Dunkels.
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
 */

#include "uip.h"
#include "psock.h"
#include "httpd.h"
#include "httpd_cgi.h"
#include "httpd_fs.h"
#include "pins_cgi.h"

#include <stdio.h>
#include <string.h>

#include <NanodeUIP.h>
#include <Arduino.h>

HTTPD_CGI_CALL(cgi_toggle_pin, "toggle_pin", toggle_pin);
HTTPD_CGI_CALL(cgi_set_pin, "set_pin", set_pin);
HTTPD_CGI_CALL(cgi_buttons_list, "buttons_list", buttons_list);
HTTPD_CGI_CALL(cgi_sensors_list, "sensors_list", sensors_list);
HTTPD_CGI_CALL(cgi_lights_list, "lights_list", lights_list);

static const struct httpd_cgi_call *calls[] = { &cgi_toggle_pin, &cgi_set_pin, &cgi_buttons_list, &cgi_sensors_list, &cgi_lights_list, NULL };

static const pin_def_t* buttons;
static const pin_def_t* lights;
static const pin_def_t* sensors;

extern void cgi_register(const pin_def_t* _buttons,const pin_def_t* _lights,const pin_def_t* _sensors)
{
  buttons = _buttons;
  lights = _lights;
  sensors = _sensors;
}

// These methods assume THIS is a progmem pointer

bool pin_def_t::is_valid_P(void) const
{
  return pgm_read_word(&name) != 0;
}
const char* pin_def_t::get_name_P(void) const
{
  return reinterpret_cast<const char*>(pgm_read_word(&name));
}
uint8_t pin_def_t::get_number_P(void) const
{
  return pgm_read_byte(&number);
}

/*---------------------------------------------------------------------------*/
static
PT_THREAD(nullfunction(struct httpd_state *s, char *))
{
  PSOCK_BEGIN(&s->sout);
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
extern void log_Pcr(const char* str);

httpd_cgifunction
httpd_cgi(char *name)
{
  log_Pcr(name);

  const struct httpd_cgi_call **f;

  /* Find the matching name in the table, return the function. */
  for(f = calls; *f != NULL; ++f) {
    if(strncmp_P((*f)->name, name, strlen((*f)->name)) == 0) {
      return (*f)->function;
    }
  }
  return nullfunction;
}

/*---------------------------------------------------------------------------*/

static const char str_high[] PROGMEM = "HIGH";
static const char str_low[] PROGMEM = "LOW";
static const char str_notfound[] PROGMEM = "Not found";

/*---------------------------------------------------------------------------*/
static unsigned short
generate_buttons_list(void *arg)
{
  printf_P(PSTR("generate_buttons_list\r\n"));

  struct httpd_state *s = (struct httpd_state *)arg;

  return snprintf_P((char*)uip_appdata,UIP_APPDATA_SIZE,
    PSTR("<li><div class='ui-grid-a'><div class='ui-block-a'>%S</div><div class='ui-block-b'>%S</div></div></li>\r\n"),
    buttons[s->count].get_name_P(),
    digitalRead(buttons[s->count].get_number_P())?str_high:str_low
  );
}
static
PT_THREAD(buttons_list(struct httpd_state *s, char *))
{
  
  PSOCK_BEGIN(&s->sout);

  for(s->count = 0; buttons[s->count].is_valid_P(); ++s->count) {
    PSOCK_GENERATOR_SEND(&s->sout, generate_buttons_list, s);
  }

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static unsigned short
generate_sensors_list(void *arg)
{
  printf_P(PSTR("generate_sensors_list\r\n"));

  struct httpd_state *s = (struct httpd_state *)arg;

  return snprintf_P((char*)uip_appdata,UIP_APPDATA_SIZE,
    PSTR("<li><div class='ui-grid-a'><div class='ui-block-a'>%S</div><div class='ui-block-b'>%u</div></div></li>\r\n"),
    sensors[s->count].get_name_P(),
    analogRead(sensors[s->count].get_number_P())
  );
}
static
PT_THREAD(sensors_list(struct httpd_state *s, char *))
{
  
  PSOCK_BEGIN(&s->sout);

  for(s->count = 0; sensors[s->count].is_valid_P(); ++s->count) {
    PSOCK_GENERATOR_SEND(&s->sout, generate_sensors_list, s);
  }

  PSOCK_END(&s->sout);
}

/*---------------------------------------------------------------------------*/
static unsigned short
generate_lights_list(void *arg)
{
  printf_P(PSTR("generate_lights_list\r\n"));

  struct httpd_state *s = (struct httpd_state *)arg;

  uint8_t pin = lights[s->count].get_number_P();
  return snprintf_P((char*)uip_appdata,UIP_APPDATA_SIZE,
    PSTR("<div data-role='fieldcontain'>"
         "<label for='light-%u'>%S</label> <select class='light' name='%u' id='light-%u' data-role='slider'>"
         "<option value='0'>LOW</option><option value='1'>HIGH</option>"
         "</select>"
         "</div>\r\n"),
    pin,
    lights[s->count].get_name_P(),
    pin,
    pin
  );
}
static
PT_THREAD(lights_list(struct httpd_state *s, char *))
{
  
  PSOCK_BEGIN(&s->sout);

  for(s->count = 0; lights[s->count].is_valid_P(); ++s->count) {
    PSOCK_GENERATOR_SEND(&s->sout, generate_lights_list, s);
  }

  PSOCK_END(&s->sout);
}

/*---------------------------------------------------------------------------*/
static
PT_THREAD(toggle_pin(struct httpd_state *s, char *))
{
  char* pin_at;
  const char* response = str_notfound;

  PSOCK_BEGIN(&s->sout);
  // Take action

  nanode_log(s->filename);
  pin_at = strstr(s->filename,"pin=");
  if ( pin_at )
  {
    uint8_t pin = atoi(pin_at+4);
    digitalWrite(pin,digitalRead(pin)^HIGH);
    
    response = digitalRead(pin)?str_high:str_low;
  }
  
    // Send response
  PSOCK_SEND_P(&s->sout,(uint8_t*)response,strlen_P(response));

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(set_pin(struct httpd_state *s, char *))
{
  char* pin_at;
  char* val_at;
  const char* response = str_notfound;

  PSOCK_BEGIN(&s->sout);
  // Take action

  nanode_log(s->filename);
  pin_at = strstr(s->filename,"pin=");
  val_at = strstr(s->filename,"val=");
  if ( pin_at && val_at )
  {
    uint8_t pin = atoi(pin_at+4);
    uint8_t val = atoi(val_at+4);
    digitalWrite(pin,val);
    
    response = digitalRead(pin)?str_high:str_low;
  }
  
    // Send response
  PSOCK_SEND_P(&s->sout,(uint8_t*)response,strlen_P(response));

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
/** @} */
// vim:cin:ai:sts=2 sw=2 ft=cpp
