/*
     This file is part of GNUnet.
     (C) 2001, 2002, 2003, 2004 Christian Grothoff (and other contributing authors)

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
 * @file applications/datastore/datastore.c
 * @brief This module is responsible to manage content, in particular 
 *        it needs to decide what content to keep.  This module
 *        also uses the bloomfilter to reduce get operations on the
 *        database.
 * @author Christian Grothoff
 */

#include "platform.h"
#include "gnunet_util.h"
#include "gnunet_protocols.h"
#include "gnunet_datastore_service.h"
#include "gnunet_sqstore_service.h"
#include "filter.h"
#include "prefetch.h"

/**
 * Require 1 MB 'free' space.
 */
#define MIN_FREE 1024 * 1024

/**
 * SQ-store handle
 */
static SQstore_ServiceAPI * sq;

/**
 * Core API handle.
 */
static CoreAPIForApplication * coreAPI;

/**
 * Minimum priority in the DB.
 */
static unsigned int minPriority;

/**
 * Available space (maybe negative!)
 */
static long long available;

/**
 */
static unsigned long long getSize() {
  return sq->getSize();
}

static int get(const HashCode160 * query,
	       unsigned int type,
	       Datum_Iterator iter,
	       void * closure) {
  int ret;

  if (! testAvailable(query))
    return SYSERR;

  ret = sq->get(query,
		type,
		iter,
		closure);
  return ret;
}

/**
 * Explicitly remove some content from the database.
 */  
static int del(const HashCode160 * query,
	       const Datastore_Value * value) {
  int ok;

  ok = sq->del(query, value);
  if (OK == ok) {
    makeUnavailable(query); /* update filter! */
    available += ntohl(value->size);
  }
  return ok;
}

/**
 * Store an item in the datastore.  If the item is
 * already present, a second copy is created.
 *  
 * @return YES on success, NO if the datastore is
 *   full and the priority of the item is not high enough
 *   to justify removing something else, SYSERR on
 *   other serious error (i.e. IO permission denied)
 */
static int put(const HashCode160 * key,
	       const Datastore_Value * value) {
  int ok;

  /* check if we have enough space / priority */
  if ( (available < ntohl(value->size) ) &&
       (minPriority > ntohl(value->prio)) )
    return SYSERR; /* new content has such a low priority that
		      we should not even bother! */
  if (ntohl(value->prio) < minPriority)
    minPriority = ntohl(value->prio);

  /* add the content */
  ok = sq->put(key, 
	       value);
  if (ok == YES) {
    makeAvailable(key);
    available -= ntohl(value->size);
  }
  return ok;
}

typedef struct {
  int exists;
  const Datastore_Value * value;
  Datastore_Value * existing;
} CE;

static int checkExists(const HashCode160 * key,
		       const Datastore_Value * value,
		       CE * ce) {
  if ( (value->size != ce->value->size) ||
       (value->type != ce->value->type) )
    return OK;
  if (0 != memcmp(&((char*)value)[sizeof(Datastore_Value)],
		  &((char*)ce->value)[sizeof(Datastore_Value)],
		  ntohl(value->size) - sizeof(Datastore_Value)))
    return OK;
  ce->existing = MALLOC(ntohl(value->size));
  memcpy(ce->existing,
	 value,
	 ntohl(value->size));
  ce->exists = YES;
  return SYSERR; /* abort iteration! */
}

/**
 * Store an item in the datastore.  If the item is already present,
 * the priorities are summed up and the higher expiration time and
 * lower anonymity level is used.
 *  
 * @return YES on success, NO if the datastore is
 *   full and the priority of the item is not high enough
 *   to justify removing something else, SYSERR on
 *   other serious error (i.e. IO permission denied)
 */
static int putUpdate(const HashCode160 * key,
		     const Datastore_Value * value) {
  CE cls;
  int ok;

  /* check if it already exists... */
  cls.exists = NO;
  cls.existing = NULL;
  cls.value = value;
  sq->get(key,
	  ntohl(value->type),
	  (Datum_Iterator) &checkExists,
	  &cls);
  if (cls.exists) {
    if ( (0 == memcmp(cls.existing,
		      value,
		      sizeof(Datastore_Value))) &&
	 (htonl(value->prio) == 0) ) {
      FREE(cls.existing);
      return OK;
    }
    /* update prio */
    sq->update(key, 
	       value,
	       ntohl(value->prio));
    FREE(cls.existing);
    return OK;
  }
  
  /* check if we have enough space / priority */
  if ( (available < ntohl(value->size) ) &&
       (minPriority > ntohl(value->prio)) )
    return SYSERR; /* new content has such a low priority that
		      we should not even bother! */
  if (ntohl(value->prio) < minPriority)
    minPriority = ntohl(value->prio);

  /* add the content */
  ok = sq->put(key, 
	       value);
  if (ok == YES) {
    makeAvailable(key);
    available -= ntohl(value->size);
  }
  return ok;
}

static int freeSpaceExpired(const HashCode160 * key,
			 const Datastore_Value * value, 
			 void * closure) {
  int ret; 

  if (cronTime(NULL) < ntohll(value->expirationTime))
    return SYSERR; /* not expired */
  ret = sq->del(key, value);
  if (ret != SYSERR)
    available += ret * ntohl(value->size);
  if (available >= MIN_FREE)
    return SYSERR;
  return OK;
}

static int freeSpaceLow(const HashCode160 * key,
			const Datastore_Value * value, 
			void * closure) {
  int ret; 

  minPriority = ntohl(value->prio);
  ret = sq->del(key, value);
  if (ret != SYSERR)
    available += ret * ntohl(value->size);
  if (available >= MIN_FREE)
    return SYSERR;
  return OK;
}

/**
 * Cron-job that deletes low-priority/expired content
 * if we are about to run out of space.
 *
 * Also updates available and minPriority.
 */
static void cronMaintenance(void * unused) {
  long long tmpAvailable 
    = getConfigurationInt("AFS", "QUOTA") * 1024 * 1024; /* MB to bytes */
  available = tmpAvailable - sq->getSize();
  if (available < MIN_FREE) {
    sq->iterateExpirationTime(ANY_BLOCK,
			      &freeSpaceExpired,
			      NULL);
    if (available < MIN_FREE) {
      sq->iterateLowPriority(ANY_BLOCK,
			     &freeSpaceLow,
			     NULL);
    }
  } else {
    minPriority = 0;
  }  
}

/**
 * Initialize the manager-module.
 */ 
Datastore_ServiceAPI * 
provide_module_datastore(CoreAPIForApplication * capi) {
  static Datastore_ServiceAPI api;
  int quota;

  sq = capi->requestService("sqstore");
  if (sq == NULL) {
    BREAK();
    return NULL;
  }
  quota
    = htonl(getConfigurationInt("AFS", "QUOTA"));
  stateWriteContent("AFS-LAST-QUOTA",
		    sizeof(int),
		    &quota);

  coreAPI = capi;

  initPrefetch(sq);
  initFilters();
  cronMaintenance(NULL);
  addCronJob(&cronMaintenance,
	     10 * cronSECONDS,
	     10 * cronSECONDS,
	     NULL); 

  api.getSize = &getSize;
  api.put = &put;
  api.putUpdate = &putUpdate;
  api.get = &get;
  api.getRandom = &getRandom; /* in prefetch.c */
  api.del = &del;

  return &api;
}

/**
 * Shutdown the manager module.
 */
void release_module_datastore() {
  delCronJob(&cronMaintenance,
	     10 * cronSECONDS,
	     NULL); 
  donePrefetch();
  doneFilters();
  coreAPI->releaseService(sq);
  sq = NULL;
  coreAPI = NULL;
}

/**
 * Callback that adds all element of the SQStore to the
 * bloomfilter.
 */
static int filterAddAll(const HashCode160 * key,
			const Datastore_Value * value, 
			void * closure) {
  makeAvailable(key);
  return OK;
}

/**
 * Update Datastore.  Currently only re-builds the bloomfilter.
 * At some point we'll want to add code to convert data between
 * different sqstore's here, too.
 *
 * @return SYSERR on errors
 */
void update_module_datastore(UpdateAPI * uapi) {
  int quota;
  int * lq;
  int lastQuota;

  quota
    = getConfigurationInt("AFS", "QUOTA");
  lq = NULL;
  if (sizeof(int) != stateReadContent("AFS-LAST-QUOTA",
				      (void**)&lq)) 
    return; /* first start? */
  lastQuota = ntohl(*lq);
  FREE(lq);
  if (lastQuota == quota)
    return; /* unchanged */
  /* ok, need to convert! */
  deleteFilter();
  initFilters();  
  sq = uapi->requestService("sqstore");
  sq->get(NULL, ANY_BLOCK,
	  &filterAddAll,
	  NULL);  
  uapi->releaseService(sq);
  sq = NULL;
  doneFilters();
}


/* end of datastore.c */
