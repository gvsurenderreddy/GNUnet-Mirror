/*
      This file is part of GNUnet

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

#include "platform.h"
#include "gnunet_util.h"
#include "gnunet_protocols.h"
#include "gnunet_getoption_lib.h"

#include "getoption.h"

/**
 * @file getoption/clientapi.c
 * @brief library to make it easy for clients to obtain
 *        options from the GNUnet server (if it supports
 *        the getoption protocol)
 * @author Christian Grothoff
 */

/**
 * Obtain option from a peer.
 * @return NULL on error
 */   
char * getConfigurationOptionValue(GNUNET_TCP_SOCKET * sock,
				   const char * section,
				   const char * option) {
  CS_GET_OPTION_REQUEST req;
  CS_GET_OPTION_REPLY * reply;
  int res;
  char * ret;
  
  memset(&req,
	 0,
	 sizeof(CS_GET_OPTION_REQUEST));
  req.header.type = htons(CS_PROTO_GET_OPTION_REQUEST);
  req.header.size = htons(sizeof(CS_GET_OPTION_REQUEST));
  if ( (strlen(section) >= CS_GET_OPTION_REQUEST_OPT_LEN) ||
       (strlen(option) >= CS_GET_OPTION_REQUEST_OPT_LEN) ) 
    return NULL;
  strcpy(&req.section[0],
	 section);
  strcpy(&req.option[0],
	 option);
  res = writeToSocket(sock,
		      &req.header);
  if (res != OK) 
    return NULL;
  reply = NULL;
  res = readFromSocket(sock,
		       (CS_HEADER**)&reply);
  if (res != OK) 
    return NULL;
  ret = MALLOC(ntohs(reply->header.size) - sizeof(CS_HEADER) + 1);
  memcpy(ret,
	 &reply->value[0],
	 ntohs(reply->header.size) - sizeof(CS_HEADER));
  ret[ntohs(reply->header.size) - sizeof(CS_HEADER)] = '\0';
  FREE(reply);
  return ret;
}
