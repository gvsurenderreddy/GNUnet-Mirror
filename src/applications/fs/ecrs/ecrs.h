/*
     This file is part of GNUnet.
     (C) 2003, 2004 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/

/**
 * @file applications/fs/ecrs/ecrs.h
 * @brief shared definitions for ECRS
 * @author Igor Wronsky, Christian Grothoff
 */

#ifndef ECRS_H
#define ECRS_H

#include "ecrs_core.h"
#include <extractor.h>


/**
 * Fixed SBlock updateInterval codes. Positive values 
 * are interpreted as durations (in seconds) for periodical 
 * updates.
 */
#define SBLOCK_UPDATE_SPORADIC  -1 
#define SBLOCK_UPDATE_NONE       0


#define BLOCK_ALIGN_SIZE (32 * 1024)

typedef struct Location {
  PeerIdentity peer;
  HashCode160 query;
  HashCode160 key;
  unsigned int type;
  unsigned long long size;
} Location;

enum uri_types { chk, sks, ksk, loc };

typedef struct ECRS_URI {
  enum uri_types type;
  union {
    struct {
      char ** keywords;
      unsigned int keywordCount;
    } ksk;
    struct {
      HashCode160 namespace;
      HashCode160 identifier;
    } sks;
    FileIdentifier chk;
    Location loc;
  } data;
} URI;

typedef struct {
  EXTRACTOR_KeywordType type;
  char * data;
} Item;

/**
 * Meta data to associate with a file, directory or namespace.
 */
typedef struct ECRS_MetaData {
  unsigned int itemCount;
  Item * items;
} MetaData;


void ECRS_encryptInPlace(const HashCode160 * hc,
			 void * data,
			 unsigned int len);

void ECRS_decryptInPlace(const HashCode160 * hc,
			 void * data,
			 unsigned int len);
 


#endif
