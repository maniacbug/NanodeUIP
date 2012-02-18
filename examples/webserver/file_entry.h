/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
 
/**
 * @file file_entry.h 
 *
 * Contains the structure used to represent a single file in a
 * directory on a flash file system.
 *
 */

#ifndef __FILE_ENTRY_H__
#define __FILE_ENTRY_H__

struct file_entry_t
{
  const char* filename;
  const char* data;
  const unsigned int* len;
};

#endif // __FILE_ENTRY_H__
