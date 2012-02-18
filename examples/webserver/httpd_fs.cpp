/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#include <NanodeUIP.h>
#include "uip.h"
#include "file_entry.h"
#include "httpd_fs.h"

// These generated headers contain the references to the files in
// the file system.
#include "files.h"
#include "dir.h"

int strncmp_PP(const char *ch1, const char *ch2, size_t len)
{
  char c1=0,c2=0;
  while (len--)
  {
    c1 = pgm_read_byte(ch1++);
    c2 = pgm_read_byte(ch2++);
    if ( c1 != c2 )
      break;
    if(!c1 && !c2)
      return (0);
    if(!c1 || !c2)
      break;
  } 
  return (c1 -c2);
}

struct entry
{
  char* filename;
  char* data;
  unsigned int* len;
  entry(const void* data)
  {
    memcpy_P(this,data,sizeof(file_entry_t));
  }
  bool matches(const char* name) const
  {
    return ( name && filename && !strncmp_P(name,filename,strlen_P(filename)) );
  }
  bool matches_P(const char* name) const
  {
    return ( name && filename && !strncmp_PP(name,filename,strlen_P(filename)) );
  }
  void result(struct httpd_fs_file *file) const
  {
    file->data = data;
    file->len = pgm_read_word(len);

    printf_P(PSTR("result %p %i\n\r"),data,*len);
  }
  bool isvalid(void) const
  {
    return filename != 0;
  }
};

// Note that 'filename' might not be zero-terminated!
int httpd_fs_open(const char *filename, struct httpd_fs_file *file)
{
  nanode_log_P(PSTR("http: file open"));
  nanode_log((char*)filename);
  
  int result = 0;
  const file_entry_t* cur = dir;
  bool done = false;
  while (!done)
  {
    entry e(cur);
    if (e.matches(filename))
    {
      e.result(file);
      done = 1;
      result = 1;
    }
    if (!e.isvalid())
      done = 1;
    ++cur;
  }

  return result; 
}

void log_Pcr(const char* str)
{
  char c = pgm_read_byte(str++);
  while (c && c != '\r' && c != '\n')
  {
    Serial.print(c);
    c = pgm_read_byte(str++);
  }
  Serial.println();
}

int httpd_fs_open_P(const char *filename, struct httpd_fs_file *file)
{
  nanode_log_P(PSTR("http: file open _P"));
  log_Pcr(filename);
  
  int result = 0;
  const file_entry_t* cur = dir;
  bool done = false;
  while (!done)
  {
    entry e(cur);
    if (e.matches_P(filename))
    {
      e.result(file);
      done = 1;
      result = 1;
    }
    if (!e.isvalid())
      done = 1;
    ++cur;
  }

  return result; 
}
// vim:cin:ai:sts=2 sw=2 ft=cpp
