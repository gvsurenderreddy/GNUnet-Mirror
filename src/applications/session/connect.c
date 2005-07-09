/*
     This file is part of GNUnet.
     (C) 2001, 2002, 2003, 2004, 2005 Christian Grothoff (and other contributing authors)

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
 * @file session/connect.c
 * @brief module responsible for the sessionkey exchange
 *   which establishes a session with another peer
 * @author Christian Grothoff
 */

#include "platform.h"
#include "gnunet_util.h"
#include "gnunet_protocols.h"
#include "gnunet_transport_service.h"
#include "gnunet_identity_service.h"
#include "gnunet_pingpong_service.h"
#include "gnunet_session_service.h"
#include "gnunet_stats_service.h"
#include "gnunet_topology_service.h"

#define HELO_HELPER_TABLE_START_SIZE 64

#define DEBUG_SESSION NO

#define EXTRA_CHECKS YES

static CoreAPIForApplication * coreAPI;

static Identity_ServiceAPI * identity;

static Transport_ServiceAPI * transport;

static Pingpong_ServiceAPI * pingpong;

static Topology_ServiceAPI * topology;

static Stats_ServiceAPI * stats;

static int stat_skeySent;

static int stat_skeyRejected;

static int stat_skeyAccepted;

static int stat_sessionEstablished;

/**
 * @brief message for session key exchange.
 */
typedef struct {
  p2p_HEADER header;
  /**
   * time when this key was created  (network byte order)
   * Must be the first field after the header since
   * the signature starts at this offset.
   */
  TIME_T creationTime;

  /**
   * The encrypted session key.  May ALSO contain
   * encrypted PINGs and PONGs.
   */
  RSAEncryptedData key;

  /**
   * Signature of the stuff above.
   */
  Signature signature;

} SKEY_Message;

#if 0
/**
 * Not thread-safe, only use for debugging!
 */
static const char * printSKEY(const SESSIONKEY * sk) {
  static char r[512];
  static char t[12];
  int i;

  strcpy(r, "");
  for (i=0;i<SESSIONKEY_LEN;i++) {
    SNPRINTF(t,
	     12,
	     "%02x",
	     sk->key[i]);
    strcat(r,t);
  }
  return r;
}
#endif

/**
 * We received a sign of life from this host.
 *
 * @param hostId the peer that gave a sign of live
 */
static void notifyPONG(PeerIdentity * hostId) {
#if DEBUG_SESSION
  EncName enc;
#endif

  GNUNET_ASSERT(hostId != NULL);
#if DEBUG_SESSION
  IFLOG(LOG_DEBUG,
	hash2enc(&hostId->hashPubKey,
		 &enc));
  LOG(LOG_DEBUG,
      "Received '%s' from '%s', marking session as up.\n",
      "PONG",
      &enc);
#endif
  GNUNET_ASSERT(hostId != NULL);
  if (stats != NULL)
    stats->change(stat_sessionEstablished,
		  1);
  coreAPI->confirmSessionUp(hostId);
  FREE(hostId);
}


/**
 * Check if the received session key is properly signed
 * and if connections to this peer are allowed according
 * to policy.
 *
 * @param hostId the sender of the key
 * @param sks the session key message
 * @return SYSERR if invalid, OK if valid
 */
static int verifySKS(const PeerIdentity * hostId,
		     SKEY_Message * sks) {
  char * limited;

  if ( (sks == NULL) ||
       (hostId == NULL) ) {
    BREAK();
    return SYSERR;
  }
  /* check if we are allowed to accept connections
     from that peer */
  limited = getConfigurationString("GNUNETD",
				   "LIMIT-ALLOW");
  if (limited != NULL) {
    EncName enc;
    hash2enc(&hostId->hashPubKey,
	     &enc);
    if (NULL == strstr(limited,
		       (char*) &enc)) {
#if DEBUG_SESSION
      LOG(LOG_DEBUG,
	  "Connection from peer '%s' was rejected.\n",
	  &enc);
#endif
      FREE(limited);
      return SYSERR;
    }
    FREE(limited);
  }
  limited = getConfigurationString("GNUNETD",
				   "LIMIT-DENY");
  if (limited != NULL) {
    EncName enc;
    hash2enc(&hostId->hashPubKey,
	     &enc);
    if (NULL != strstr(limited,
		       (char*) &enc)) {
#if DEBUG_SESSION
      LOG(LOG_DEBUG,
	  "Connection from peer '%s' was rejected.\n",
	  &enc);
#endif
      FREE(limited);
      return SYSERR;
    }
    FREE(limited);
  }

  if (OK != identity->verifyPeerSignature
      (hostId,
       sks,
       sizeof(SKEY_Message) - sizeof(Signature),
       &sks->signature)) {
    EncName enc;

    IFLOG(LOG_INFO,
	  hash2enc(&hostId->hashPubKey,
		   &enc));
    LOG(LOG_INFO,
	_("Session key from peer '%s' could not be verified.\n"),
	&enc);
    return SYSERR; /*reject!*/
  }
  return OK; /* ok */
}

/**
 * Force creation of a new Session key for the given host.
 *
 * @param hostId the identity of the other host
 * @param sk the SESSIONKEY to use
 * @param created the timestamp to use
 * @param ping optional PING to include (otherwise NULL)
 * @param pong optional PONG to include (otherwise NULL)
 * @param ret the address where to write the signed
 *        session key message
 * @return message on success, NULL on failure
 */
static SKEY_Message * makeSessionKeySigned(const PeerIdentity * hostId,
					   const SESSIONKEY * sk,
					   TIME_T created,
					   const p2p_HEADER * ping,
					   const p2p_HEADER * pong) {
  HELO_Message * foreignHelo;
  int size;
  SKEY_Message * msg;
  char * pt;

  GNUNET_ASSERT(sk != NULL);
  foreignHelo = identity->identity2Helo(hostId,
					ANY_PROTOCOL_NUMBER,
					YES);
  /* create and encrypt sessionkey */
  if (NULL == foreignHelo) {
    LOG(LOG_INFO,
	_("Cannot encrypt sessionkey, other peer not known!\n"));
    return NULL; /* other host not known */
  }

  size = sizeof(SKEY_Message);
  if (ping != NULL)
    size += ntohs(ping->size);
  if (pong != NULL)
    size += ntohs(pong->size);
  msg = MALLOC(size);

#if DEBUG_SESSION
  LOG(LOG_DEBUG,
      "Sending SKEY %u with %u bytes of data (%s, %s).\n",
      *(int*) sk,
      size,
      ping != NULL ? "ping":"",
      pong != NULL ? "pong":"");
#endif
  if (SYSERR == encryptPrivateKey(sk,
				  sizeof(SESSIONKEY),
				  &foreignHelo->publicKey,
				  &msg->key)) {
    BREAK();
    FREE(foreignHelo);
    FREE(msg);
    return NULL; /* encrypt failed */
  }
  FREE(foreignHelo);

  /* complete header */
  msg->header.size = htons(size);
  msg->header.type = htons(p2p_PROTO_SKEY);
  msg->creationTime = htonl(created);
  GNUNET_ASSERT(SYSERR != 
		identity->signData(msg,
				   sizeof(SKEY_Message)
				   - sizeof(Signature),
				   &msg->signature));
#if EXTRA_CHECKS
  /* verify signature/SKS */
  GNUNET_ASSERT(OK == verifySKS(coreAPI->myIdentity, msg));
#endif

  size = 0;
  if (ping != NULL)
    size += ntohs(ping->size);
  if (pong != NULL)
    size += ntohs(pong->size);
  if (size > 0) {
    pt = MALLOC(size);
    size = 0;
    if (ping != NULL) {
      memcpy(&pt[size], ping, ntohs(ping->size));
      size += ntohs(ping->size);
    }
    if (pong != NULL) {
      memcpy(&pt[size], pong, ntohs(pong->size));
      size += ntohs(pong->size);
    }
#if DEBUG_SESSION
    LOG(LOG_DEBUG,
	"Encrypting %d bytes of PINGPONG with key %u and IV %u\n",
	size,
	*(int*)sk,
	*(int*)&msg->signature);
#endif
    GNUNET_ASSERT(-1 != encryptBlock(pt,
				     size,
				     sk,
				     (const INITVECTOR*) &msg->signature,
				     &((char*)msg)[sizeof(SKEY_Message)]));
    FREE(pt);
  }
  return msg;
}

/**
 * Perform a session key exchange for entry be.  First sends a HELO
 * and then the new SKEY (in two plaintext packets). When called, the
 * semaphore of at the given index must already be down
 *
 * @param receiver peer to exchange a key with
 * @param tsession session to use for the exchange (maybe NULL)
 * @param pong pong to include (maybe NULL)
 */
static int exchangeKey(const PeerIdentity * receiver,
		       TSession * tsession,
		       p2p_HEADER * pong) {
  HELO_Message * helo;
  SKEY_Message * skey;
  char * sendBuffer;
  SESSIONKEY sk;
  TIME_T age;
  p2p_HEADER * ping;
  PeerIdentity * sndr;
  int size;
  EncName enc;

  GNUNET_ASSERT(receiver != NULL);
  if ( (topology != NULL) &&
       (topology->allowConnectionFrom(receiver) == SYSERR) )
    return SYSERR;
  hash2enc(&receiver->hashPubKey,
	   &enc);
  /* then try to connect on the transport level */
  if ( (tsession == NULL) ||
       (transport->associate(tsession) == SYSERR) ) {
    tsession = transport->connectFreely(receiver,
					YES);
    if (tsession == NULL) {
#if DEBUG_SESSION
      LOG(LOG_DEBUG,
	  "Key exchange with '%s' failed: could not connect.\n",
	  &enc);
#endif
      return SYSERR; /* failed to connect */
    }
  }

  /* create our ping */
  sndr = MALLOC(sizeof(PeerIdentity));
  *sndr = *receiver;
  ping = pingpong->pingUser(receiver,
			    (CronJob)&notifyPONG,
			    sndr,
			    NO);
  if (ping == NULL) {
    FREE(sndr);
    transport->disconnect(tsession);
    return SYSERR;
  }

  /* get or create out session key */
  if (OK != coreAPI->getCurrentSessionKey(receiver,
					  &sk,
					  &age,
					  YES)) {
    age = TIME(NULL);
    makeSessionkey(&sk);
#if DEBUG_SESSION
    LOG(LOG_DEBUG,
	"Created fresh sessionkey %u.\n",
	*(int*) &sk);
#endif
  }

  /* build SKEY message */
  skey = makeSessionKeySigned(receiver,
			      &sk,
			      age,
			      ping,
			      pong);
  if (skey == NULL) {
    FREE(ping);
    transport->disconnect(tsession);
    BREAK();
    return SYSERR;
  }

  /* create HELO */
  helo = transport->createHELO(ANY_PROTOCOL_NUMBER);
  if (NULL == helo) {
    LOG(LOG_INFO,
	"Could not create any HELO advertisement.  Not good.");
  }
  size = ntohs(skey->header.size);
  if (helo != NULL)
    size += HELO_Message_size(helo);
  sendBuffer = MALLOC(size);
  if (helo != NULL) {
    size = HELO_Message_size(helo);
    memcpy(sendBuffer,
	   helo,
	   HELO_Message_size(helo));
    FREE(helo);
    helo = NULL;
  } else {
    size = 0;
  }

  memcpy(&sendBuffer[size],
	 skey,
	 ntohs(skey->header.size));
  size += ntohs(skey->header.size);
  FREE(skey);
#if DEBUG_SESSION
  LOG(LOG_DEBUG,
      "Sending session key to peer '%s'.\n",
      &enc);
#endif
  if (stats != NULL)
    stats->change(stat_skeySent, 1);
  coreAPI->sendPlaintext(tsession,
			 sendBuffer,
			 size);
  FREE(sendBuffer);
  coreAPI->offerTSessionFor(receiver,
			    tsession);
  coreAPI->assignSessionKey(&sk,
			    receiver,
			    age,
			    YES);
  return OK;
}

/**
 * Accept a session-key that has been sent by another host.
 * The other host must be known (public key).  Notifies
 * the core about the new session key and possibly
 * triggers sending a session key ourselves (if not
 * already done).
 *
 * @param sender the identity of the sender host
 * @param tsession the transport session handle
 * @param msg message with the session key
 * @return SYSERR or OK
 */
static int acceptSessionKey(const PeerIdentity * sender,
			    const p2p_HEADER * msg,
			    TSession * tsession) {
  SESSIONKEY key;
  p2p_HEADER * ping;
  p2p_HEADER * pong;
  SKEY_Message * sessionkeySigned;
  int size;
  int pos;
  char * plaintext;
  EncName enc;

  if ( (topology != NULL) &&
       (topology->allowConnectionFrom(sender) == SYSERR) )
    return SYSERR;
  hash2enc(&sender->hashPubKey,
	   &enc);
#if DEBUG_SESSION
  LOG(LOG_DEBUG,
      "Received session key from peer '%s'.\n",
      &enc);
#endif
  if (ntohs(msg->size) < sizeof(SKEY_Message)) {
    LOG(LOG_WARNING,
	"Session key received from peer '%s' "
	"has invalid format (discarded).\n",
	&enc);
    return SYSERR;
  }
  sessionkeySigned = (SKEY_Message *) msg;
  if (SYSERR == verifySKS(sender,
			  sessionkeySigned)) {
    LOG(LOG_INFO,
	"Signature of session key from '%s' failed"
	" verification (discarded).\n",
	&enc);
    if (stats != NULL)
      stats->change(stat_skeyRejected, 
		    1);
    return SYSERR;  /* rejected */
  }
  size = identity->decryptData(&sessionkeySigned->key,
			       &key,
			       sizeof(SESSIONKEY));
  if (size != sizeof(SESSIONKEY)) {
    LOG(LOG_WARNING,
	_("Invalid '%s' message received from peer '%s'.\n"),
	"SKEY",
	&enc);
    return SYSERR;
  }
  if (key.crc32 !=
      htonl(crc32N(&key, SESSIONKEY_LEN))) {
    LOG(LOG_WARNING,
	_("SKEY from '%s' fails CRC check (have: %u, want %u).\n"),
	&enc,
	ntohl(key.crc32),
	crc32N(&key, SESSIONKEY_LEN));
    stats->change(stat_skeyRejected, 
		  1);
    return SYSERR;
  }

#if DEBUG_SESSION
  LOG(LOG_DEBUG,
      "Received SKEY message with %u bytes of data and key %u.\n",
      ntohs(sessionkeySigned->header.size),
      *(int*)&key);
#endif
  if (stats != NULL)
    stats->change(stat_skeyAccepted,
		  1);
  /* notify core about session key */
  coreAPI->assignSessionKey(&key,
			    sender,
			    ntohl(sessionkeySigned->creationTime),
			    NO);
  pos = sizeof(SKEY_Message);
  ping = NULL;
  pong = NULL;
  plaintext = NULL;
  size = ntohs(sessionkeySigned->header.size);
  if (sizeof(SKEY_Message) < size) {
    size -= sizeof(SKEY_Message);
    plaintext = MALLOC(size);
#if DEBUG_SESSION
    LOG(LOG_DEBUG,
	"Decrypting %d bytes of PINGPONG with key %u and IV %u\n",
	size,
	*(int*)&key,
	*(int*)&sessionkeySigned->signature);
#endif
    GNUNET_ASSERT(-1 != 
		  decryptBlock
		  (&key,
		   &((char*)sessionkeySigned)[sizeof(SKEY_Message)],
		   size,
		   (const INITVECTOR*) &sessionkeySigned->signature,
		   plaintext));
    pos = 0;
    /* find pings & pongs! */
    while (pos + sizeof(p2p_HEADER) < size) {
      p2p_HEADER * hdr;
      hdr = (p2p_HEADER*) &plaintext[pos];
      if (htons(hdr->size) + pos > size) {
	LOG(LOG_WARNING,
	    _("Error parsing encrypted session key, "
	      "given message part size is invalid.\n"));
	break;
      }
      if (htons(hdr->type) == p2p_PROTO_PING)
	ping = hdr;
      else if (htons(hdr->type) == p2p_PROTO_PONG)
	pong = hdr;
      else
	LOG(LOG_WARNING,
	    "Unknown type in embedded message: %u (size: %u)\n",
	    htons(hdr->type),
	    htons(hdr->size));
      pos += ntohs(hdr->size);
    }
  }
  if (pong != NULL) {
    /* we initiated, this is the response */
    /* notify ourselves about encapsulated pong */
#if DEBUG_SESSION
    LOG(LOG_DEBUG,
	"Received pong in session key, injecting!\n",
	&enc);
#endif
    coreAPI->injectMessage(sender,
			   (char*) pong,
			   ntohs(pong->size),
			   YES,
			   tsession);
    if (ping != NULL) { /* should always be true for well-behaved peers */
      /* pong can go out over ordinary channels */
#if DEBUG_SESSION
      LOG(LOG_DEBUG,
	  "Received ping in session key, "
	  "sending pong over normal encrypted session!\n",
	  &enc);
#endif
      ping->type = htons(p2p_PROTO_PONG);
      coreAPI->unicast(sender,
		       ping,
		       0,
		       0);
    }
  } else {
    if (ping != NULL) {
#if DEBUG_SESSION
      LOG(LOG_DEBUG,
	  "Received ping in session key, "
	  "sending pong together with my session key!\n",
	  &enc);
#endif
      ping->type = htons(p2p_PROTO_PONG);
      exchangeKey(sender,
		  tsession,
		  ping); /* ping is now pong */
    } else {
      BREAK();
      /* PING not included in SKEY - bug (in other peer!?) */
    }
  }
  FREENONNULL(plaintext);
  return OK;
}

/**
 * Try to connect to the given peer.
 *
 * @return SYSERR if that is impossible,
 *         YES if a connection is established upon return,
 *         NO if we're going to try to establish one asynchronously
 */
static int tryConnect(const PeerIdentity * peer) {
  EncName enc;

  IFLOG(LOG_DEBUG,
	hash2enc(&peer->hashPubKey,
		 &enc));
  if ( (topology != NULL) &&
       (topology->allowConnectionFrom(peer) == SYSERR) ) {
    LOG(LOG_DEBUG,
	"Topology rejected connecting to '%s'.\n",
	&enc);
    return SYSERR;
  }
  if (coreAPI->queryBPMfromPeer(peer) != 0) {
    LOG(LOG_DEBUG,
	"Connection to '%s' already up (have BPM limit)\n",
	&enc);
    return YES; /* trivial case */
  }
  LOG(LOG_DEBUG,
      "Trying to exchange key with '%s'.\n",
      &enc);
  if (OK == exchangeKey(peer, NULL, NULL))
    return NO;
  else
    return SYSERR;
}

/**
 * We have received an (encrypted) SKEY message.
 * The reaction is to update our key to the new
 * value.  (Rekeying).
 */
static int acceptSessionKeyUpdate(const PeerIdentity * sender,
				  const p2p_HEADER * msg) {
  acceptSessionKey(sender,
		   msg,
		   NULL);
  return OK;
}


/**
 * Initialize the module.
 */
Session_ServiceAPI *
provide_module_session(CoreAPIForApplication * capi) {
  static Session_ServiceAPI ret;

  coreAPI = capi;
  identity = capi->requestService("identity");
  if (identity == NULL) {
    BREAK();
    return NULL;
  }
  transport = capi->requestService("transport");
  if (transport == NULL) {
    BREAK();
    coreAPI->releaseService(identity);
    identity = NULL;
    return NULL;
  }
  pingpong = capi->requestService("pingpong");
  if (pingpong == NULL) {
    BREAK();
    coreAPI->releaseService(transport);
    transport = NULL;
    coreAPI->releaseService(identity);
    identity = NULL;
    return NULL;
  }
  topology = capi->requestService("topology");
  stats = capi->requestService("stats");
  if (stats != NULL) {
    stat_skeySent
      = stats->create(_("# session keys sent"));
    stat_skeyRejected
      = stats->create(_("# session keys rejected"));
    stat_skeyAccepted
      = stats->create(_("# session keys accepted"));
    stat_sessionEstablished
      = stats->create(_("# sessions established"));
  }

  LOG(LOG_DEBUG,
      _("'%s' registering handler %d (plaintext and ciphertext)\n"),
      "session",
      p2p_PROTO_SKEY);
  coreAPI->registerPlaintextHandler(p2p_PROTO_SKEY,
				    &acceptSessionKey);
  coreAPI->registerHandler(p2p_PROTO_SKEY,
			   &acceptSessionKeyUpdate);
  ret.tryConnect = &tryConnect;
  return &ret;
}

/**
 * Shutdown the module.
 */
int release_module_session() {
  coreAPI->unregisterPlaintextHandler(p2p_PROTO_SKEY,
				      &acceptSessionKey);
  coreAPI->unregisterHandler(p2p_PROTO_SKEY,
			     &acceptSessionKeyUpdate);
  if (topology != NULL) {
    coreAPI->releaseService(topology);
    topology = NULL;
  }
  coreAPI->releaseService(stats);
  stats = NULL;
  coreAPI->releaseService(identity);
  identity = NULL;
  coreAPI->releaseService(transport);
  transport = NULL;
  coreAPI->releaseService(pingpong);
  pingpong = NULL;
  coreAPI = NULL;
  return OK;
}

/* end of connect.c */
