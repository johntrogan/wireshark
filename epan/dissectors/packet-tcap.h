/* Do not modify this file. Changes will be overwritten.                      */
/* Generated automatically by the ASN.1 to Wireshark dissector compiler       */
/* packet-tcap.h                                                              */
/* asn2wrs.py -b -q -L -p tcap -c ./tcap.cnf -s ./packet-tcap-template -D . -O ../.. tcap.asn UnidialoguePDUs.asn DialoguePDUs.asn */

/* packet-tcap.h
 *
 * Copyright 2004, Tim Endean <endeant@hotmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef PACKET_tcap_H
#define PACKET_tcap_H

#include "ws_symbol_export.h"

/* TCAP component type */
#define TCAP_COMP_INVOKE	0xa1
#define TCAP_COMP_RRL		0xa2
#define TCAP_COMP_RE		0xa3
#define TCAP_COMP_REJECT	0xa4
#define TCAP_COMP_RRN		0xa7


#define ANSI_TC_INVOKE_L	0xe9
#define ANSI_TC_RRL		0xea
#define ANSI_TC_RE		0xeb
#define ANSI_TC_REJECT		0xec
#define ANSI_TC_INVOKE_N	0xed
#define ANSI_TC_RRN		0xee


#define	TCAP_SEQ_TAG		0x30
#define	TCAP_SET_TAG		0x31

#define TCAP_INVOKE_ID_TAG	0x02
#define TCAP_LINKED_ID_TAG	0x80

#define	TCAP_EOC_LEN		2

#define	TCAP_CONSTRUCTOR(TCtag)	(TCtag & 0x20)

#define TC_BEGIN 1
#define TC_CONT 2
#define TC_END 3
#define TC_ABORT 4
#define TC_ANSI_ABORT 5
#define TC_ANSI_ALL 6

struct tcap_private_t {
  bool acv; /* Is the Application Context Version present */
  const void * oid;
  uint32_t session_id;
  void * context;
  char *TransactionID_str;
  uint32_t src_tid;
  uint32_t dst_tid;
};

/** @file
 * lists and hash tables used in wireshark's tcap dissector
 * for calculation of delays in tcap-calls
 */

#define LENGTH_OID 23
struct tcaphash_context_t {
  struct tcaphash_context_key_t * key;
  uint32_t session_id;
  uint32_t first_frame;
  uint32_t last_frame;
  nstime_t begin_time;	/**< time of arrival of TC_BEGIN */
  nstime_t end_time;	/**< time of closing message */
  bool responded;	/**< true, if request has been responded */
  bool closed;
  bool upper_dissector;
  bool oid_present;
  char oid[LENGTH_OID+1];
  bool subdissector_present;
  dissector_handle_t subdissector_handle;
  void (* callback) (tvbuff_t *,packet_info *, proto_tree *, struct tcaphash_context_t *);
  struct tcaphash_begincall_t * begincall;
  struct tcaphash_contcall_t * contcall;
  struct tcaphash_endcall_t * endcall;
  struct tcaphash_ansicall_t * ansicall;
};

struct tcaphash_begincall_t {
  struct tcaphash_begin_info_key_t * beginkey;
  struct tcaphash_context_t * context;
  bool father;
  struct tcaphash_begincall_t * next_begincall;
  struct tcaphash_begincall_t * previous_begincall;
};

struct tcaphash_contcall_t {
  struct tcaphash_cont_info_key_t * contkey;
  struct tcaphash_context_t * context;
  bool father;
  struct tcaphash_contcall_t * next_contcall;
  struct tcaphash_contcall_t * previous_contcall;
};

struct tcaphash_endcall_t {
  struct tcaphash_end_info_key_t * endkey;
  struct tcaphash_context_t * context;
  bool father;
  struct tcaphash_endcall_t * next_endcall;
  struct tcaphash_endcall_t * previous_endcall;
};

struct tcaphash_ansicall_t {
  struct tcaphash_ansi_info_key_t * ansikey;
  struct tcaphash_context_t * context;
  bool father;
  struct tcaphash_ansicall_t * next_ansicall;
  struct tcaphash_ansicall_t * previous_ansicall;
};

/** The Key for the hash table is the TCAP origine transaction identifier
   of the TC_BEGIN containing the InitialDP */

struct tcaphash_context_key_t {
  uint32_t session_id;
};

struct tcaphash_begin_info_key_t {
  uint32_t hashKey;
  uint32_t tid;
  uint32_t pc_hash;
};

struct tcaphash_cont_info_key_t {
  uint32_t hashKey;
  uint32_t src_tid;
  uint32_t dst_tid;
  uint32_t opc_hash;
  uint32_t dpc_hash;
};

struct tcaphash_end_info_key_t {
  uint32_t hashKey;
  uint32_t tid;
  uint32_t opc_hash;
  uint32_t dpc_hash;
};

struct tcaphash_ansi_info_key_t {
  uint32_t hashKey;
  uint32_t tid;
  uint32_t opc_hash;
  uint32_t dpc_hash;
};


/** List of infos to store for the analyse */
struct tcapsrt_info_t {
  uint32_t tcap_session_id;
  uint32_t src_tid;
  uint32_t dst_tid;
  uint8_t ope;
};

/**
 * Initialize the Message Info used by the main dissector
 * Data are linked to a TCAP transaction
 */
struct tcapsrt_info_t * tcapsrt_razinfo(void);

void tcapsrt_close(struct tcaphash_context_t * p_tcaphash_context,
		   packet_info * pinfo _U_);

/**
 * Service Response Time analyze
 * Called just after dissector call
 * Associate a TCAP context to a tcap session and display session related infomations
 * like the first frame, the last, the session duration,
 * and a uniq session identifier for the filtering
 *
 * For ETSI tcap, the TCAP context can be reached through three keys
 * - a key (BEGIN) identifying the session according to the tcap source identifier
 * - a key (CONT) identifying the established session (src_id and dst_id)
 * - a key (END) identifying the session according to the tcap destination identifier
 *
 * For ANSI tcap, the TCAP context is reached through a uniq key
 * - a key (ANSI) identifying the session according to the tcap identifier
*/
struct tcaphash_context_t * tcapsrt_call_matching(tvbuff_t *tvb,
						  packet_info * pinfo _U_,
						  proto_tree *tree,
						  struct tcapsrt_info_t * p_tcap_info);

WS_DLL_PUBLIC bool gtcap_StatSRT;

extern int tcap_standard;

extern const value_string tcap_component_type_str[];
void proto_reg_handoff_tcap(void);
void proto_register_tcap(void);

extern dissector_handle_t get_itu_tcap_subdissector(uint32_t ssn);
dissector_handle_t get_ansi_tcap_subdissector(uint32_t ssn);

extern void add_ansi_tcap_subdissector(uint32_t ssn, dissector_handle_t dissector);
WS_DLL_PUBLIC void add_itu_tcap_subdissector(uint32_t ssn, dissector_handle_t dissector);

extern void delete_ansi_tcap_subdissector(uint32_t ssn, dissector_handle_t dissector);
WS_DLL_PUBLIC void delete_itu_tcap_subdissector(uint32_t ssn, dissector_handle_t dissector);

extern void call_tcap_dissector(dissector_handle_t, tvbuff_t*, packet_info*, proto_tree*);

extern const value_string tcap_UniDialoguePDU_vals[];
extern const value_string tcap_DialoguePDU_vals[];
int dissect_tcap_UniDialoguePDU(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);
int dissect_tcap_DialoguePDU(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);

#endif  /* PACKET_tcap_H */
