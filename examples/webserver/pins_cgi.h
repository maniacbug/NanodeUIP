/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
 
/**
 * @file pins_cgi.h 
 *
 * Helpers to register pins with the CGI
 */

#ifndef __PINS_CGI_H__
#define __PINS_CGI_H__

/**
 * Definition for a single pin, as reported in an HTML list
 */
struct pin_def_t
{
  const char* name; /**< Easy-to-remember name of the pin, in progmem */
  uint8_t number;

  // Progmem accessors
  bool is_valid_P(void) const;
  const char* get_name_P(void) const;
  uint8_t get_number_P(void) const;
};

extern void cgi_register(const pin_def_t*,const pin_def_t*,const pin_def_t*);

#endif // __PINS_CGI_H__
