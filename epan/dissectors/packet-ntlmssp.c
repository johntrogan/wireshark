/* packet-ntlmssp.c
 * Add-on for better NTLM v1/v2 handling
 * Copyright 2009, 2012 Matthieu Patou <mat@matws.net>
 * Routines for NTLM Secure Service Provider
 * Devin Heitmueller <dheitmueller@netilla.com>
 * Copyright 2003, Tim Potter <tpot@samba.org>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define WS_LOG_DOMAIN "packet-ntlmssp"
#include "config.h"
#include <wireshark.h>
#include <string.h>

#include <epan/packet.h>
#include <epan/exceptions.h>
#include <epan/asn1.h>
#include <epan/prefs.h>
#include <epan/tap.h>
#include <epan/expert.h>
#include <epan/show_exception.h>
#include <epan/proto_data.h>
#include <epan/tfs.h>

#include <wsutil/array.h>
#include <wsutil/wsgcrypt.h>
#include <wsutil/crc32.h>
#include <wsutil/str_util.h>

#include "packet-windows-common.h"
#include "packet-kerberos.h"
#include "packet-dcerpc.h"
#include "packet-gssapi.h"

#include "read_keytab_file.h"

#include "packet-ntlmssp.h"

/*
 * See
 *
 *   https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/
 *
 * for Microsoft's MS-NLMP, NT LAN Manager (NTLM) Authentication Protocol
 * Specification.
 *
 * See also
 *
 *      http://davenport.sourceforge.net/ntlm.html
 *
 * which indicates that, in practice, some fields specified by MS-NLMP
 * may be absent; this has been seen in some captures.
 */

void proto_register_ntlmssp(void);
void proto_reg_handoff_ntlmssp(void);

static int ntlmssp_tap;

#define CLIENT_SIGN_TEXT "session key to client-to-server signing key magic constant"
#define CLIENT_SEAL_TEXT "session key to client-to-server sealing key magic constant"
#define SERVER_SIGN_TEXT "session key to server-to-client signing key magic constant"
#define SERVER_SEAL_TEXT "session key to server-to-client sealing key magic constant"

static const value_string ntlmssp_message_types[] = {
  { NTLMSSP_NEGOTIATE, "NTLMSSP_NEGOTIATE" },
  { NTLMSSP_CHALLENGE, "NTLMSSP_CHALLENGE" },
  { NTLMSSP_AUTH,      "NTLMSSP_AUTH" },
  { NTLMSSP_UNKNOWN,   "NTLMSSP_UNKNOWN" },
  { 0, NULL }
};

#define NTLMSSP_EK_IS_NT4HASH(ek) \
  (ek->fd_num == -1 && ek->keytype == 23 && ek->keylength == NTLMSSP_KEY_LEN)

static const unsigned char gbl_zeros[24] = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
static GHashTable* hash_packet;

/*
 * NTLMSSP negotiation flags
 * Taken from Samba
 *
 * See also the davenport.sourceforge.net document cited above,
 * although that document says that:
 *
 *      0x00010000 is "Target Type Domain";
 *      0x00020000 is "Target Type Server"
 *      0x00040000 is "Target Type Share";
 *
 * and that 0x00100000, 0x00200000, and 0x00400000 are
 * "Request Init Response", "Request Accept Response", and
 * "Request Non-NT Session Key", rather than those values shifted
 * right one having those interpretations.
 *
 * UPDATE: Further information obtained from [MS-NLMP] 2.2.2.5, added in comments
 * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/99d90ff4-957f-4c8a-80e4-5bfe5a9a9832
 */
#define NTLMSSP_NEGOTIATE_UNICODE                  0x00000001 // A
#define NTLMSSP_NEGOTIATE_OEM                      0x00000002 // B
#define NTLMSSP_REQUEST_TARGET                     0x00000004 // C
#define NTLMSSP_UNUSED_00000008                    0x00000008 // r10
#define NTLMSSP_NEGOTIATE_SIGN                     0x00000010 // D
#define NTLMSSP_NEGOTIATE_SEAL                     0x00000020 // E
#define NTLMSSP_NEGOTIATE_DATAGRAM                 0x00000040 // F
#define NTLMSSP_NEGOTIATE_LM_KEY                   0x00000080 // G, "requests LAN Manager (LM) session key computation", aka NTLMv1
#define NTLMSSP_UNUSED_00000100                    0x00000100 // r9
#define NTLMSSP_NEGOTIATE_NTLM                     0x00000200 // H, "requests usage of the NTLM v1 session security protocol"
#define NTLMSSP_UNUSED_00000400                    0x00000400 // r8
#define NTLMSSP_NEGOTIATE_ANONYMOUS                0x00000800 // J
#define NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED      0x00001000 // K
#define NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED 0x00002000 // L
#define NTLMSSP_UNUSED_00004000                    0x00004000 // r7
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN              0x00008000 // M
#define NTLMSSP_TARGET_TYPE_DOMAIN                 0x00010000 // N
#define NTLMSSP_TARGET_TYPE_SERVER                 0x00020000 // O
#define NTLMSSP_UNUSED_00040000                    0x00040000 // r6
#define NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY 0x00080000 // P, "requests usage of the NTLM v2 session security. NTLM v2 session security is a misnomer because it is not NTLM v2. It is NTLM v1 using the extended session security that is also in NTLM v2"
#define NTLMSSP_NEGOTIATE_IDENTIFY                 0x00100000 // Q
#define NTLMSSP_UNUSED_00200000                    0x00200000 // r5
#define NTLMSSP_REQUEST_NON_NT_SESSION_KEY         0x00400000 // R, "requests the usage of the LMOWF"
#define NTLMSSP_NEGOTIATE_TARGET_INFO              0x00800000 // S
#define NTLMSSP_UNUSED_01000000                    0x01000000 // r4
#define NTLMSSP_NEGOTIATE_VERSION                  0x02000000 // T
#define NTLMSSP_UNUSED_04000000                    0x04000000 // r3
#define NTLMSSP_UNUSED_08000000                    0x08000000 // r2
#define NTLMSSP_UNUSED_10000000                    0x10000000 // r1
#define NTLMSSP_NEGOTIATE_128                      0x20000000 // U
#define NTLMSSP_NEGOTIATE_KEY_EXCH                 0x40000000 // V
#define NTLMSSP_NEGOTIATE_56                       0x80000000 // W

static int proto_ntlmssp;
static int hf_ntlmssp_auth;
static int hf_ntlmssp_message_type;
static int hf_ntlmssp_negotiate_flags;
static int hf_ntlmssp_negotiate_flags_01;
static int hf_ntlmssp_negotiate_flags_02;
static int hf_ntlmssp_negotiate_flags_04;
static int hf_ntlmssp_negotiate_flags_08;
static int hf_ntlmssp_negotiate_flags_10;
static int hf_ntlmssp_negotiate_flags_20;
static int hf_ntlmssp_negotiate_flags_40;
static int hf_ntlmssp_negotiate_flags_80;
static int hf_ntlmssp_negotiate_flags_100;
static int hf_ntlmssp_negotiate_flags_200;
static int hf_ntlmssp_negotiate_flags_400;
static int hf_ntlmssp_negotiate_flags_800;
static int hf_ntlmssp_negotiate_flags_1000;
static int hf_ntlmssp_negotiate_flags_2000;
static int hf_ntlmssp_negotiate_flags_4000;
static int hf_ntlmssp_negotiate_flags_8000;
static int hf_ntlmssp_negotiate_flags_10000;
static int hf_ntlmssp_negotiate_flags_20000;
static int hf_ntlmssp_negotiate_flags_40000;
static int hf_ntlmssp_negotiate_flags_80000;
static int hf_ntlmssp_negotiate_flags_100000;
static int hf_ntlmssp_negotiate_flags_200000;
static int hf_ntlmssp_negotiate_flags_400000;
static int hf_ntlmssp_negotiate_flags_800000;
static int hf_ntlmssp_negotiate_flags_1000000;
static int hf_ntlmssp_negotiate_flags_2000000;
static int hf_ntlmssp_negotiate_flags_4000000;
static int hf_ntlmssp_negotiate_flags_8000000;
static int hf_ntlmssp_negotiate_flags_10000000;
static int hf_ntlmssp_negotiate_flags_20000000;
static int hf_ntlmssp_negotiate_flags_40000000;
static int hf_ntlmssp_negotiate_flags_80000000;
/* static int hf_ntlmssp_negotiate_workstation_strlen; */
/* static int hf_ntlmssp_negotiate_workstation_maxlen; */
/* static int hf_ntlmssp_negotiate_workstation_buffer; */
static int hf_ntlmssp_negotiate_workstation;
/* static int hf_ntlmssp_negotiate_domain_strlen; */
/* static int hf_ntlmssp_negotiate_domain_maxlen; */
/* static int hf_ntlmssp_negotiate_domain_buffer; */
static int hf_ntlmssp_negotiate_domain;
static int hf_ntlmssp_ntlm_server_challenge;
static int hf_ntlmssp_ntlm_client_challenge;
static int hf_ntlmssp_reserved;
static int hf_ntlmssp_challenge_target_name;
static int hf_ntlmssp_auth_username;
static int hf_ntlmssp_auth_domain;
static int hf_ntlmssp_auth_hostname;
static int hf_ntlmssp_auth_lmresponse;
static int hf_ntlmssp_auth_ntresponse;
static int hf_ntlmssp_auth_sesskey;
static int hf_ntlmssp_string_len;
static int hf_ntlmssp_string_maxlen;
static int hf_ntlmssp_string_offset;
static int hf_ntlmssp_blob_len;
static int hf_ntlmssp_blob_maxlen;
static int hf_ntlmssp_blob_offset;
static int hf_ntlmssp_version;
static int hf_ntlmssp_version_major;
static int hf_ntlmssp_version_minor;
static int hf_ntlmssp_version_build_number;
static int hf_ntlmssp_version_ntlm_current_revision;

static int hf_ntlmssp_challenge_target_info;
static int hf_ntlmssp_challenge_target_info_len;
static int hf_ntlmssp_challenge_target_info_maxlen;
static int hf_ntlmssp_challenge_target_info_offset;

static int hf_ntlmssp_challenge_target_info_item_type;
static int hf_ntlmssp_challenge_target_info_item_len;

static int hf_ntlmssp_challenge_target_info_end;
static int hf_ntlmssp_challenge_target_info_nb_computer_name;
static int hf_ntlmssp_challenge_target_info_nb_domain_name;
static int hf_ntlmssp_challenge_target_info_dns_computer_name;
static int hf_ntlmssp_challenge_target_info_dns_domain_name;
static int hf_ntlmssp_challenge_target_info_dns_tree_name;
static int hf_ntlmssp_challenge_target_info_flags;
static int hf_ntlmssp_challenge_target_info_timestamp;
static int hf_ntlmssp_challenge_target_info_restrictions;
static int hf_ntlmssp_challenge_target_info_target_name;
static int hf_ntlmssp_challenge_target_info_channel_bindings;

static int hf_ntlmssp_ntlmv2_response_item_type;
static int hf_ntlmssp_ntlmv2_response_item_len;

static int hf_ntlmssp_ntlmv2_response_end;
static int hf_ntlmssp_ntlmv2_response_nb_computer_name;
static int hf_ntlmssp_ntlmv2_response_nb_domain_name;
static int hf_ntlmssp_ntlmv2_response_dns_computer_name;
static int hf_ntlmssp_ntlmv2_response_dns_domain_name;
static int hf_ntlmssp_ntlmv2_response_dns_tree_name;
static int hf_ntlmssp_ntlmv2_response_flags;
static int hf_ntlmssp_ntlmv2_response_timestamp;
static int hf_ntlmssp_ntlmv2_response_restrictions;
static int hf_ntlmssp_ntlmv2_response_target_name;
static int hf_ntlmssp_ntlmv2_response_channel_bindings;

static int hf_ntlmssp_message_integrity_code;
static int hf_ntlmssp_verf;
static int hf_ntlmssp_verf_vers;
static int hf_ntlmssp_verf_body;
static int hf_ntlmssp_verf_randompad;
static int hf_ntlmssp_verf_hmacmd5;
static int hf_ntlmssp_verf_crc32;
static int hf_ntlmssp_verf_sequence;
/* static int hf_ntlmssp_decrypted_payload; */

static int hf_ntlmssp_ntlmv2_response;
static int hf_ntlmssp_ntlmv2_response_ntproofstr;
static int hf_ntlmssp_ntlmv2_response_rversion;
static int hf_ntlmssp_ntlmv2_response_hirversion;
static int hf_ntlmssp_ntlmv2_response_z;
static int hf_ntlmssp_ntlmv2_response_pad;
static int hf_ntlmssp_ntlmv2_response_time;
static int hf_ntlmssp_ntlmv2_response_chal;

static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_Version;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_Flags;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_LM_PRESENT;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_NT_PRESENT;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_REMOVED;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_CREDKEY_PRESENT;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_SHA_PRESENT;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_CredentialKey;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_CredentialKeyType;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_EncryptedCredsSize;
static int hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_EncryptedCreds;

static int ett_ntlmssp;
static int ett_ntlmssp_negotiate_flags;
static int ett_ntlmssp_string;
static int ett_ntlmssp_blob;
static int ett_ntlmssp_version;
static int ett_ntlmssp_challenge_target_info;
static int ett_ntlmssp_challenge_target_info_item;
static int ett_ntlmssp_ntlmv2_response;
static int ett_ntlmssp_ntlmv2_response_item;
static int ett_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL;

static expert_field ei_ntlmssp_v2_key_too_long;
static expert_field ei_ntlmssp_blob_len_too_long;
static expert_field ei_ntlmssp_target_info_attr;
static expert_field ei_ntlmssp_target_info_invalid;
static expert_field ei_ntlmssp_message_type;
static expert_field ei_ntlmssp_auth_nthash;
static expert_field ei_ntlmssp_sessionbasekey;
static expert_field ei_ntlmssp_sessionkey;

static dissector_handle_t ntlmssp_handle, ntlmssp_wrap_handle;

/* Configuration variables */
static const char *ntlmssp_option_nt_password;

#define NTLMSSP_CONV_INFO_KEY 0
/* Used in the conversation function */
typedef struct _ntlmssp_info {
  uint32_t         flags;
  bool             saw_challenge;
  gcry_cipher_hd_t rc4_handle_client;
  gcry_cipher_hd_t rc4_handle_server;
  uint8_t          sign_key_client[NTLMSSP_KEY_LEN];
  uint8_t          sign_key_server[NTLMSSP_KEY_LEN];
  uint32_t         server_dest_port;
  unsigned char    server_challenge[8];
  bool             rc4_state_initialized;
  ntlmssp_blob     ntlm_response;
  ntlmssp_blob     lm_response;
} ntlmssp_info;

#define NTLMSSP_PACKET_INFO_KEY 1
/* If this struct exists in the payload_decrypt, then we have already
   decrypted it once */
typedef struct _ntlmssp_packet_info {
  uint8_t  *decrypted_payload;
  uint8_t   payload_len;
  uint8_t   verifier[NTLMSSP_KEY_LEN];
  bool      payload_decrypted;
  bool      verifier_decrypted;
  int       verifier_offset;
  uint32_t  verifier_block_length;
} ntlmssp_packet_info;

static int
dissect_ntlmssp_verf(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_);

/*
 * GSlist of decrypted payloads.
 */
static GSList *decrypted_payloads;

#if 0
static int
LEBE_Convert(int value)
{
  char a, b, c, d;
  /* Get each byte */
  a = value&0x000000FF;
  b = (value&0x0000FF00) >> 8;
  c = (value&0x00FF0000) >> 16;
  d = (value&0xFF000000) >> 24;
  return (a << 24) | (b << 16) | (c << 8) | d;
}
#endif

static bool
ntlmssp_sessions_destroy_cb(wmem_allocator_t *allocator _U_, wmem_cb_event_t event _U_, void *user_data _U_)
{
  ntlmssp_info * conv_ntlmssp_info = (ntlmssp_info *) user_data;
  if (conv_ntlmssp_info->rc4_state_initialized) {
    gcry_cipher_close(conv_ntlmssp_info->rc4_handle_client);
    gcry_cipher_close(conv_ntlmssp_info->rc4_handle_server);
  }
  /* unregister this callback */
  return false;
}

/*
  Perform a DES encryption with a 16-byte key and 8-byte data item.
  It's in fact 3 susbsequent call to crypt_des_ecb with a 7-byte key.
  Missing bytes for the key are replaced by 0;
  Returns output in response, which is expected to be 24 bytes.
*/
static int
crypt_des_ecb_long(uint8_t *response,
                   const uint8_t *key,
                   const uint8_t *data)
{
  uint8_t pw21[21] = { 0 }; /* 21 bytes place for the needed key */

  memcpy(pw21, key, 16);

  memset(response, 0, 24);
  crypt_des_ecb(response, data, pw21);
  crypt_des_ecb(response + 8, data, pw21 + 7);
  crypt_des_ecb(response + 16, data, pw21 + 14);

  return 1;
}

/*
  Generate a challenge response, given an eight byte challenge and
  either the NT or the Lan Manager password hash (16 bytes).
  Returns output in response, which is expected to be 24 bytes.
*/
static int
ntlmssp_generate_challenge_response(uint8_t *response,
                                    const uint8_t *passhash,
                                    const uint8_t *challenge)
{
  uint8_t pw21[21]; /* Password hash padded to 21 bytes */

  memset(pw21, 0x0, sizeof(pw21));
  memcpy(pw21, passhash, 16);

  memset(response, 0, 24);

  crypt_des_ecb(response, challenge, pw21);
  crypt_des_ecb(response + 8, challenge, pw21 + 7);
  crypt_des_ecb(response + 16, challenge, pw21 + 14);

  return 1;
}


/* Ultra simple ANSI to unicode converter, will only work for ascii password...*/
static void
ansi_to_unicode(const char* ansi, char* unicode)
{
  size_t input_len;
  size_t i;

  input_len = strlen(ansi);
  if (unicode != NULL) {
    for (i = 0; i < (input_len); i++) {
      unicode[i * 2] = ansi[i];
      unicode[i * 2 + 1] = 0;
    }
    unicode[2 * input_len] = '\0';
  }
}

/* This function generate the Key Exchange Key (KXKEY)
 * Depending on the flags this key will either be used to encrypt the exported session key
 * or will be used directly as exported session key.
 * Exported session key is the key that will be used for sealing and signing communication
 * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/d86303b5-b29e-4fb9-b119-77579c761370
 */

static void
get_keyexchange_key(unsigned char keyexchangekey[NTLMSSP_KEY_LEN], const unsigned char sessionbasekey[NTLMSSP_KEY_LEN], const unsigned char lm_challenge_response[24], int flags)
{
  uint8_t basekey[NTLMSSP_KEY_LEN];
  uint8_t zeros[24] = { 0 };

  memset(keyexchangekey, 0, NTLMSSP_KEY_LEN);
  memset(basekey, 0, NTLMSSP_KEY_LEN);
  /* sessionbasekey is either derived from lm_hash or from nt_hash depending on the key type negotiated */
  memcpy(basekey, sessionbasekey, 8);
  memset(basekey+8, 0xBD, 8);
  if (flags&NTLMSSP_NEGOTIATE_LM_KEY) {
    /*data, key*/
    crypt_des_ecb(keyexchangekey, lm_challenge_response, basekey);
    crypt_des_ecb(keyexchangekey+8, lm_challenge_response, basekey+7);
  }
  else {
    if (flags&NTLMSSP_REQUEST_NON_NT_SESSION_KEY) {
      /*People from samba tends to use the same function in this case than in the previous one but with 0 data
       * it's not clear that it produce the good result
       * memcpy(keyexchangekey, lm_hash, 8);
       * Let's trust samba implementation it mights seem weird but they are more often right than the spec!
       */
      crypt_des_ecb(keyexchangekey, zeros, basekey);
      crypt_des_ecb(keyexchangekey+8, zeros, basekey+7);
    }
    else {
      /* it is stated page 65 of NTLM SSP spec: https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/d86303b5-b29e-4fb9-b119-77579c761370
       * that sessionbasekey should be encrypted with hmac_md5 using the concat of both challenge when it's NTLM v1 + extended session security but it turns out to be wrong!
       */
      memcpy(keyexchangekey, sessionbasekey, NTLMSSP_KEY_LEN);
    }
  }
}

uint32_t
get_md4pass_list(wmem_allocator_t *pool, md4_pass** p_pass_list)
{
#if defined(HAVE_HEIMDAL_KERBEROS) || defined(HAVE_MIT_KERBEROS)
  uint32_t       nb_pass = 0;
  enc_key_t     *ek;
  const char*    password = ntlmssp_option_nt_password;
  unsigned char  nt_hash[NTLMSSP_KEY_LEN];
  char           password_unicode[256];
  md4_pass*      pass_list;
  int            i;

  *p_pass_list = NULL;
  read_keytab_file_from_preferences();

  for (ek=enc_key_list; ek; ek=ek->next) {
    if (NTLMSSP_EK_IS_NT4HASH(ek)) {
      nb_pass++;
    }
  }
  memset(password_unicode, 0, sizeof(password_unicode));
  memset(nt_hash, 0, NTLMSSP_KEY_LEN);
  /* Compute the NT hash of the provided password, even if empty */
  if (strlen(password) < 129) {
    int password_len;
    nb_pass++;
    password_len = (int)strlen(password);
    ansi_to_unicode(password, password_unicode);
    gcry_md_hash_buffer(GCRY_MD_MD4, nt_hash, password_unicode, password_len*2);
  }
  if (nb_pass == 0) {
    /* Unable to calculate the session key without a valid password (128 chars or less) ......*/
    return 0;
  }
  i = 0;
  *p_pass_list = (md4_pass *)wmem_alloc0(pool, nb_pass*sizeof(md4_pass));
  pass_list = *p_pass_list;

  if (memcmp(nt_hash, gbl_zeros, NTLMSSP_KEY_LEN) != 0) {
    memcpy(pass_list[i].md4, nt_hash, NTLMSSP_KEY_LEN);
    snprintf(pass_list[i].key_origin, NTLMSSP_MAX_ORIG_LEN,
               "<Global NT Password>");
    i = 1;
  }
  for (ek=enc_key_list; ek; ek=ek->next) {
    if (NTLMSSP_EK_IS_NT4HASH(ek)) {
      memcpy(pass_list[i].md4, ek->keyvalue, NTLMSSP_KEY_LEN);
      memcpy(pass_list[i].key_origin, ek->key_origin,
             MIN(sizeof(pass_list[i].key_origin),sizeof(ek->key_origin)));
      i++;
    }
  }
  return nb_pass;
#else /* !(defined(HAVE_HEIMDAL_KERBEROS) || defined(HAVE_MIT_KERBEROS)) */
  (void) pool;
  *p_pass_list = NULL;
  return 0;
#endif /* !(defined(HAVE_HEIMDAL_KERBEROS) || defined(HAVE_MIT_KERBEROS)) */
}

/* Create an NTLMSSP version 2 key
 * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/5e550938-91d4-459f-b67d-75d70009e3f3
 */
static void
create_ntlmssp_v2_key(const uint8_t *serverchallenge, const uint8_t *clientchallenge,
                      uint8_t *sessionkey , const  uint8_t *encryptedsessionkey , int flags ,
                      const ntlmssp_blob *ntlm_response, const ntlmssp_blob *lm_response _U_, ntlmssp_header_t *ntlmssph,
                      packet_info *pinfo, proto_tree *ntlmssp_tree)
{
/* static const would be nicer, but -Werror=vla does not like it */
#define DOMAIN_NAME_BUF_SIZE 512
#define USER_BUF_SIZE 256
#define BUF_SIZE (DOMAIN_NAME_BUF_SIZE + USER_BUF_SIZE)
  char              domain_name_unicode[DOMAIN_NAME_BUF_SIZE];
  char              user_uppercase[USER_BUF_SIZE];
  char              buf[BUF_SIZE];
  /*uint8_t md4[NTLMSSP_KEY_LEN];*/
  unsigned char     nt_hash[NTLMSSP_KEY_LEN];
  unsigned char     nt_proof[NTLMSSP_KEY_LEN];
  unsigned char     ntowfv2[NTLMSSP_KEY_LEN];
  uint8_t           sessionbasekey[NTLMSSP_KEY_LEN];
  uint8_t           keyexchangekey[NTLMSSP_KEY_LEN];
  uint8_t           lm_challenge_response[24];
  uint32_t          i;
  uint32_t          j;
  gcry_cipher_hd_t  rc4_handle;
  size_t            user_len;
  size_t            domain_len;
  md4_pass         *pass_list = NULL;
  const md4_pass   *used_md4 = NULL;
  uint32_t          nb_pass = 0;
  bool              found = false;

  /* We are going to try password encrypted in keytab as well, it's an idea of Stefan Metzmacher <metze@samba.org>
   * The idea is to be able to test all the key of domain in once and to be able to decode the NTLM dialogs */

  memset(sessionkey, 0, NTLMSSP_KEY_LEN);
  nb_pass = get_md4pass_list(pinfo->pool, &pass_list);
  i = 0;
  memset(user_uppercase, 0, USER_BUF_SIZE);
  user_len = strlen(ntlmssph->acct_name);
  if (user_len < USER_BUF_SIZE / 2) {
    memset(buf, 0, BUF_SIZE);
    ansi_to_unicode(ntlmssph->acct_name, buf);
    for (j = 0; j < (2*user_len); j++) {
      if (buf[j] != '\0') {
        user_uppercase[j] = g_ascii_toupper(buf[j]);
      }
    }
  }
  else {
    /* Unable to calculate the session not enough space in buffer, note this is unlikely to happen but ......*/
    return;
  }
  domain_len = strlen(ntlmssph->domain_name);
  if (domain_len < DOMAIN_NAME_BUF_SIZE / 2) {
    ansi_to_unicode(ntlmssph->domain_name, domain_name_unicode);
  }
  else {
    /* Unable to calculate the session not enough space in buffer, note this is unlikely to happen but ......*/
    return;
  }
  while (i < nb_pass) {
    ws_debug("Turn %d", i);
    used_md4 = &pass_list[i];
    memcpy(nt_hash, pass_list[i].md4, NTLMSSP_KEY_LEN);
    ws_log_buffer(nt_hash, NTLMSSP_KEY_LEN, "Current NT hash");
    i++;
    /* NTOWFv2 computation */
    memset(buf, 0, BUF_SIZE);
    memcpy(buf, user_uppercase, user_len*2);
    memcpy(buf+user_len*2, domain_name_unicode, domain_len*2);
    if (ws_hmac_buffer(GCRY_MD_MD5, ntowfv2, buf, domain_len*2+user_len*2, nt_hash, NTLMSSP_KEY_LEN)) {
      return;
    }
    ws_log_buffer(ntowfv2, NTLMSSP_KEY_LEN, "NTOWFv2");

    /* LM response */
    memset(buf, 0, BUF_SIZE);
    memcpy(buf, serverchallenge, 8);
    memcpy(buf+8, clientchallenge, 8);
    if (ws_hmac_buffer(GCRY_MD_MD5, lm_challenge_response, buf, NTLMSSP_KEY_LEN, ntowfv2, NTLMSSP_KEY_LEN)) {
      return;
    }
    memcpy(lm_challenge_response+NTLMSSP_KEY_LEN, clientchallenge, 8);
    ws_log_buffer(lm_challenge_response, 24, "LM Response");

    /* NT proof = First NTLMSSP_KEY_LEN bytes of NT response */
    memset(buf, 0, BUF_SIZE);
    memcpy(buf, serverchallenge, 8);
    memcpy(buf+8, ntlm_response->contents+NTLMSSP_KEY_LEN, MIN(BUF_SIZE - 8, ntlm_response->length-NTLMSSP_KEY_LEN));
    if (ws_hmac_buffer(GCRY_MD_MD5, nt_proof, buf, ntlm_response->length-8, ntowfv2, NTLMSSP_KEY_LEN)) {
      return;
    }
    ws_log_buffer(nt_proof, NTLMSSP_KEY_LEN, "NT proof");
    if (!memcmp(nt_proof, ntlm_response->contents, NTLMSSP_KEY_LEN)) {
      found = true;
      break;
    }
  }
  if (!found) {
    return;
  }

  if (ws_hmac_buffer(GCRY_MD_MD5, sessionbasekey, nt_proof, NTLMSSP_KEY_LEN, ntowfv2, NTLMSSP_KEY_LEN)) {
    return;
  }

  get_keyexchange_key(keyexchangekey, sessionbasekey, lm_challenge_response, flags);
  /* now decrypt session key if needed and setup sessionkey for decrypting further communications */
  if (flags & NTLMSSP_NEGOTIATE_KEY_EXCH)
  {
    memcpy(sessionkey, encryptedsessionkey, NTLMSSP_KEY_LEN);
    if (!gcry_cipher_open(&rc4_handle, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0)) {
      if (!gcry_cipher_setkey(rc4_handle, keyexchangekey, NTLMSSP_KEY_LEN)) {
        gcry_cipher_decrypt(rc4_handle, sessionkey, NTLMSSP_KEY_LEN, NULL, 0);
      }
      gcry_cipher_close(rc4_handle);
    }
  }
  else
  {
    memcpy(sessionkey, keyexchangekey, NTLMSSP_KEY_LEN);
  }

  memcpy(ntlmssph->session_key, sessionkey, NTLMSSP_KEY_LEN);

  if (used_md4 == NULL) {
    return;
  }
  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_auth_nthash,
                         "NTLMv2 authenticated using %s (%02x%02x%02x%02x...)",
                         used_md4->key_origin,
                         used_md4->md4[0] & 0xFF, used_md4->md4[1] & 0xFF,
                         used_md4->md4[2] & 0xFF, used_md4->md4[3] & 0xFF);
  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_sessionbasekey,
                         "NTLMv2 BaseSessionKey ("
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         ")",
                         sessionbasekey[0] & 0xFF,  sessionbasekey[1] & 0xFF,
                         sessionbasekey[2] & 0xFF,  sessionbasekey[3] & 0xFF,
                         sessionbasekey[4] & 0xFF,  sessionbasekey[5] & 0xFF,
                         sessionbasekey[6] & 0xFF,  sessionbasekey[7] & 0xFF,
                         sessionbasekey[8] & 0xFF,  sessionbasekey[9] & 0xFF,
                         sessionbasekey[10] & 0xFF, sessionbasekey[11] & 0xFF,
                         sessionbasekey[12] & 0xFF, sessionbasekey[13] & 0xFF,
                         sessionbasekey[14] & 0xFF, sessionbasekey[15] & 0xFF);
  if (memcmp(sessionbasekey, sessionkey, NTLMSSP_KEY_LEN) == 0) {
    return;
  }
  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_sessionkey,
                         "NTLMSSP SessionKey ("
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         ")",
                         sessionkey[0] & 0xFF,  sessionkey[1] & 0xFF,
                         sessionkey[2] & 0xFF,  sessionkey[3] & 0xFF,
                         sessionkey[4] & 0xFF,  sessionkey[5] & 0xFF,
                         sessionkey[6] & 0xFF,  sessionkey[7] & 0xFF,
                         sessionkey[8] & 0xFF,  sessionkey[9] & 0xFF,
                         sessionkey[10] & 0xFF, sessionkey[11] & 0xFF,
                         sessionkey[12] & 0xFF, sessionkey[13] & 0xFF,
                         sessionkey[14] & 0xFF, sessionkey[15] & 0xFF);
}

 /* Create an NTLMSSP version 1 key
 * That is more complicated logic and methods and user challenge as well.
 * password points to the ANSI password to encrypt, challenge points to
 * the 8 octet challenge string
 * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/464551a8-9fc4-428e-b3d3-bc5bfb2e73a5
 */
static void
create_ntlmssp_v1_key(const uint8_t *serverchallenge, const uint8_t *clientchallenge,
                      uint8_t *sessionkey, const  uint8_t *encryptedsessionkey, int flags,
                      const uint8_t *ref_nt_challenge_response, const uint8_t *ref_lm_challenge_response,
                      ntlmssp_header_t *ntlmssph,
                      packet_info *pinfo, proto_tree *ntlmssp_tree)
{
  const char       *password = ntlmssp_option_nt_password;
  unsigned char     lm_password_upper[NTLMSSP_KEY_LEN];
  unsigned char     lm_hash[NTLMSSP_KEY_LEN];
  unsigned char     nt_hash[NTLMSSP_KEY_LEN];
  unsigned char     challenges_hash_first8[8];
  unsigned char     challenges[NTLMSSP_KEY_LEN];
  uint8_t           md4[NTLMSSP_KEY_LEN];
  uint8_t           nb_pass   = 0;
  uint8_t           sessionbasekey[NTLMSSP_KEY_LEN];
  uint8_t           keyexchangekey[NTLMSSP_KEY_LEN];
  uint8_t           lm_challenge_response[24];
  uint8_t           nt_challenge_response[24];
  gcry_cipher_hd_t  rc4_handle;
  gcry_md_hd_t      md5_handle;
  char              password_unicode[256];
  size_t            password_len;
  unsigned int      i;
  bool              found     = false;
  md4_pass         *pass_list = NULL;
  const md4_pass   *used_md4  = NULL;

  // "A Boolean setting that SHOULD<35> control using the NTLM response for the LM response to the server challenge when NTLMv1 authentication is used. The default value of this state variable is true."
  // "<35> Section 3.1.1.1: Windows NT Server 4.0 SP3 does not support providing NTLM instead of LM responses."
  // https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/f711d059-3983-4b9d-afbb-ff2f8c97ffbf
  static const bool NoLMResponseNTLMv1 = true;

  static const unsigned char lmhash_key[] =
    {0x4b, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25}; // "KGS!@#$%"

  memset(sessionkey, 0, NTLMSSP_KEY_LEN);
  /* Create a NT hash of the input password, even if empty */
  // NTOWFv1 as defined in https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/464551a8-9fc4-428e-b3d3-bc5bfb2e73a5
  password_len = strlen(password);
  /*Do not forget to free password*/
  ansi_to_unicode(password, password_unicode);
  gcry_md_hash_buffer(GCRY_MD_MD4, nt_hash, password_unicode, password_len*2);

  if ((flags & NTLMSSP_NEGOTIATE_LM_KEY && !(flags & NoLMResponseNTLMv1)) || !(flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY)  || !(flags & NTLMSSP_NEGOTIATE_NTLM)) {
    /* Create a LM hash of the input password, even if empty */
    // LMOWFv1 as defined in https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/464551a8-9fc4-428e-b3d3-bc5bfb2e73a5
    /* Truncate password if too long */
    if (password_len > NTLMSSP_KEY_LEN)
      password_len = NTLMSSP_KEY_LEN;

    memset(lm_password_upper, 0, sizeof(lm_password_upper));
    for (i = 0; i < password_len; i++) {
      lm_password_upper[i] = g_ascii_toupper(password[i]);
    }

    crypt_des_ecb(lm_hash, lmhash_key, lm_password_upper);
    crypt_des_ecb(lm_hash+8, lmhash_key, lm_password_upper+7);
    ntlmssp_generate_challenge_response(lm_challenge_response,
                                        lm_hash, serverchallenge);
    memcpy(sessionbasekey, lm_hash, NTLMSSP_KEY_LEN);
  }
  else {

    memset(lm_challenge_response, 0, 24);
    if (flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY) {
      nb_pass = get_md4pass_list(pinfo->pool, &pass_list);
      i = 0;
      while (i < nb_pass) {
        /*ws_debug("Turn %d", i);*/
        used_md4 = &pass_list[i];
        memcpy(nt_hash, pass_list[i].md4, NTLMSSP_KEY_LEN);
        /*ws_log_buffer(nt_hash, NTLMSSP_KEY_LEN, "Current NT hash");*/
        i++;
        if(clientchallenge){
          memcpy(lm_challenge_response, clientchallenge, 8);
        }
        if (gcry_md_open(&md5_handle, GCRY_MD_MD5, 0)) {
                break;
        }
        gcry_md_write(md5_handle, serverchallenge, 8);
        gcry_md_write(md5_handle, clientchallenge, 8);
        memcpy(challenges_hash_first8, gcry_md_read(md5_handle, 0), 8);
        gcry_md_close(md5_handle);
        crypt_des_ecb_long(nt_challenge_response, nt_hash, challenges_hash_first8);
        if (ref_nt_challenge_response && !memcmp(ref_nt_challenge_response, nt_challenge_response, 24)) {
          found = true;
          break;
        }
      }
    }
    else {
      crypt_des_ecb_long(nt_challenge_response, nt_hash, serverchallenge);
      if (NoLMResponseNTLMv1) {
        memcpy(lm_challenge_response, nt_challenge_response, 24);
      }
      else {
        crypt_des_ecb_long(lm_challenge_response, lm_hash, serverchallenge);
      }
      if (ref_nt_challenge_response &&
          !memcmp(ref_nt_challenge_response, nt_challenge_response, 24) &&
          ref_lm_challenge_response &&
          !memcmp(ref_lm_challenge_response, lm_challenge_response, 24))
      {
          found = true;
      }
    }
    /* So it's clearly not like this that's put into NTLMSSP doc but after some digging into samba code I'm quite confident
     * that sessionbasekey should be based md4(nt_hash) only in the case of some NT auth
     * Otherwise it should be lm_hash ...*/
    gcry_md_hash_buffer(GCRY_MD_MD4, md4, nt_hash, NTLMSSP_KEY_LEN);
    if (flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY) {
      memcpy(challenges, serverchallenge, 8);
      if(clientchallenge){
        memcpy(challenges+8, clientchallenge, 8);
      }
      if (ws_hmac_buffer(GCRY_MD_MD5, sessionbasekey, challenges, NTLMSSP_KEY_LEN, md4, NTLMSSP_KEY_LEN)) {
        return;
      }
    }
    else {
     memcpy(sessionbasekey, md4, NTLMSSP_KEY_LEN);
    }
  }

  if (!found) {
    return;
  }

  get_keyexchange_key(keyexchangekey, sessionbasekey, lm_challenge_response, flags);
  /*ws_log_buffer(nt_challenge_response, 24, "NT challenge response");
  ws_log_buffer(lm_challenge_response, 24, "LM challenge response");*/
  /* now decrypt session key if needed and setup sessionkey for decrypting further communications */
  if (flags & NTLMSSP_NEGOTIATE_KEY_EXCH)
  {
    if(encryptedsessionkey){
      memcpy(sessionkey, encryptedsessionkey, NTLMSSP_KEY_LEN);
    }
    if (!gcry_cipher_open(&rc4_handle, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0)) {
      if (!gcry_cipher_setkey(rc4_handle, keyexchangekey, NTLMSSP_KEY_LEN)) {
        gcry_cipher_decrypt(rc4_handle, sessionkey, NTLMSSP_KEY_LEN, NULL, 0);
      }
      gcry_cipher_close(rc4_handle);
    }
  }
  else
  {
    memcpy(sessionkey, keyexchangekey, NTLMSSP_KEY_LEN);
  }
  memcpy(ntlmssph->session_key, sessionkey, NTLMSSP_KEY_LEN);

  if (used_md4 == NULL) {
    return;
  }
  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_auth_nthash,
                         "NTLMv1 authenticated using %s (%02x%02x%02x%02x...)",
                         used_md4->key_origin,
                         used_md4->md4[0] & 0xFF, used_md4->md4[1] & 0xFF,
                         used_md4->md4[2] & 0xFF, used_md4->md4[3] & 0xFF);
  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_sessionbasekey,
                         "NTLMv1 BaseSessionKey ("
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         ")",
                         sessionbasekey[0] & 0xFF,  sessionbasekey[1] & 0xFF,
                         sessionbasekey[2] & 0xFF,  sessionbasekey[3] & 0xFF,
                         sessionbasekey[4] & 0xFF,  sessionbasekey[5] & 0xFF,
                         sessionbasekey[6] & 0xFF,  sessionbasekey[7] & 0xFF,
                         sessionbasekey[8] & 0xFF,  sessionbasekey[9] & 0xFF,
                         sessionbasekey[10] & 0xFF, sessionbasekey[11] & 0xFF,
                         sessionbasekey[12] & 0xFF, sessionbasekey[13] & 0xFF,
                         sessionbasekey[14] & 0xFF, sessionbasekey[15] & 0xFF);
  if (memcmp(sessionbasekey, sessionkey, NTLMSSP_KEY_LEN) == 0) {
    return;
  }
  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_sessionkey,
                         "NTLMSSP SessionKey ("
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         ")",
                         sessionkey[0] & 0xFF,  sessionkey[1] & 0xFF,
                         sessionkey[2] & 0xFF,  sessionkey[3] & 0xFF,
                         sessionkey[4] & 0xFF,  sessionkey[5] & 0xFF,
                         sessionkey[6] & 0xFF,  sessionkey[7] & 0xFF,
                         sessionkey[8] & 0xFF,  sessionkey[9] & 0xFF,
                         sessionkey[10] & 0xFF, sessionkey[11] & 0xFF,
                         sessionkey[12] & 0xFF, sessionkey[13] & 0xFF,
                         sessionkey[14] & 0xFF, sessionkey[15] & 0xFF);
}

/*
 * Create an NTLMSSP anonymous key
 */
static void
create_ntlmssp_anon_key(uint8_t *sessionkey, const  uint8_t *encryptedsessionkey, int flags,
                        ntlmssp_header_t *ntlmssph,
                        packet_info *pinfo, proto_tree *ntlmssp_tree)
{
  uint8_t           lm_challenge_response[24] = { 0, };
  uint8_t           sessionbasekey[NTLMSSP_KEY_LEN] = { 0, };
  uint8_t           keyexchangekey[NTLMSSP_KEY_LEN] = { 0, };
  gcry_cipher_hd_t  rc4_handle;

  memset(sessionkey, 0, NTLMSSP_KEY_LEN);

  get_keyexchange_key(keyexchangekey, sessionbasekey, lm_challenge_response, flags);
  if (flags & NTLMSSP_NEGOTIATE_KEY_EXCH)
  {
    if(encryptedsessionkey){
      memcpy(sessionkey, encryptedsessionkey, NTLMSSP_KEY_LEN);
    }
    if (!gcry_cipher_open(&rc4_handle, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0)) {
      if (!gcry_cipher_setkey(rc4_handle, keyexchangekey, NTLMSSP_KEY_LEN)) {
        gcry_cipher_decrypt(rc4_handle, sessionkey, NTLMSSP_KEY_LEN, NULL, 0);
      }
      gcry_cipher_close(rc4_handle);
    }
  }
  else
  {
    memcpy(sessionkey, keyexchangekey, NTLMSSP_KEY_LEN);
  }
  memcpy(ntlmssph->session_key, sessionkey, NTLMSSP_KEY_LEN);

  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_auth_nthash,
                         "NTLM authenticated using ANONYMOUS ZERO NTHASH");
  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_sessionbasekey,
                         "NTLM Anonymous BaseSessionKey ("
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         ")",
                         sessionbasekey[0] & 0xFF,  sessionbasekey[1] & 0xFF,
                         sessionbasekey[2] & 0xFF,  sessionbasekey[3] & 0xFF,
                         sessionbasekey[4] & 0xFF,  sessionbasekey[5] & 0xFF,
                         sessionbasekey[6] & 0xFF,  sessionbasekey[7] & 0xFF,
                         sessionbasekey[8] & 0xFF,  sessionbasekey[9] & 0xFF,
                         sessionbasekey[10] & 0xFF, sessionbasekey[11] & 0xFF,
                         sessionbasekey[12] & 0xFF, sessionbasekey[13] & 0xFF,
                         sessionbasekey[14] & 0xFF, sessionbasekey[15] & 0xFF);
  if (memcmp(sessionbasekey, sessionkey, NTLMSSP_KEY_LEN) == 0) {
    return;
  }
  expert_add_info_format(pinfo, proto_tree_get_parent(ntlmssp_tree),
                         &ei_ntlmssp_sessionkey,
                         "NTLMSSP SessionKey Anonymous ("
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         ")",
                         sessionkey[0] & 0xFF,  sessionkey[1] & 0xFF,
                         sessionkey[2] & 0xFF,  sessionkey[3] & 0xFF,
                         sessionkey[4] & 0xFF,  sessionkey[5] & 0xFF,
                         sessionkey[6] & 0xFF,  sessionkey[7] & 0xFF,
                         sessionkey[8] & 0xFF,  sessionkey[9] & 0xFF,
                         sessionkey[10] & 0xFF, sessionkey[11] & 0xFF,
                         sessionkey[12] & 0xFF, sessionkey[13] & 0xFF,
                         sessionkey[14] & 0xFF, sessionkey[15] & 0xFF);
}

void
ntlmssp_create_session_key(packet_info *pinfo,
                           proto_tree *tree,
                           ntlmssp_header_t *ntlmssph,
                           int flags,
                           const uint8_t *server_challenge,
                           const uint8_t *encryptedsessionkey,
                           const ntlmssp_blob *ntlm_response,
                           const ntlmssp_blob *lm_response)
{
  uint8_t client_challenge[8] = {0, };
  uint8_t sessionkey[NTLMSSP_KEY_LEN] = {0, };

  if (ntlm_response->length > 24)
  {
    /*
     * [MS-NLMP] 2.2.2.8 NTLM2 V2 Response: NTLMv2_RESPONSE has
     * the 2.2.2.7 "NTLM v2: NTLMv2_CLIENT_CHALLENGE" at offset 16.
     * Within that ChallengeFromClient is at offset 16, that means
     * it's at offset 32 in total.
     *
     * Note that value is only used for the LM_response of NTLMv2.
     */
    if (ntlm_response->length >= 40) {
      memcpy(client_challenge,
             ntlm_response->contents+32, 8);
    }
    create_ntlmssp_v2_key(server_challenge,
                          client_challenge,
                          sessionkey,
                          encryptedsessionkey,
                          flags,
                          ntlm_response,
                          lm_response,
                          ntlmssph,
                          pinfo,
                          tree);
  }
  else if (ntlm_response->length == 24 && lm_response->length == 24)
  {
    memcpy(client_challenge, lm_response->contents, 8);

    create_ntlmssp_v1_key(server_challenge,
                          client_challenge,
                          sessionkey,
                          encryptedsessionkey,
                          flags,
                          ntlm_response->contents,
                          lm_response->contents,
                          ntlmssph,
                          pinfo,
                          tree);
  }
  else if (ntlm_response->length == 0 && lm_response->length <= 1)
  {
    create_ntlmssp_anon_key(sessionkey,
                            encryptedsessionkey,
                            flags,
                            ntlmssph,
                            pinfo,
                            tree);
  }
}

static void
get_signing_key(uint8_t *sign_key_server, uint8_t* sign_key_client, const uint8_t key[NTLMSSP_KEY_LEN], int keylen)
{
  gcry_md_hd_t md5_handle;

  memset(sign_key_client, 0, NTLMSSP_KEY_LEN);
  memset(sign_key_server, 0, NTLMSSP_KEY_LEN);
  if (gcry_md_open(&md5_handle, GCRY_MD_MD5, 0)) {
    return;
  }
  gcry_md_write(md5_handle, key, keylen);
  gcry_md_write(md5_handle, CLIENT_SIGN_TEXT, strlen(CLIENT_SIGN_TEXT)+1); // +1 to get the final null-byte
  memcpy(sign_key_client, gcry_md_read(md5_handle, 0), NTLMSSP_KEY_LEN);
  gcry_md_reset(md5_handle);
  gcry_md_write(md5_handle, key, keylen);
  gcry_md_write(md5_handle, SERVER_SIGN_TEXT, strlen(SERVER_SIGN_TEXT)+1); // +1 to get the final null-byte
  memcpy(sign_key_server, gcry_md_read(md5_handle, 0), NTLMSSP_KEY_LEN);
  gcry_md_close(md5_handle);
}

/* We return either a 128 or 64 bit key
 */
static void
get_sealing_rc4key(const uint8_t exportedsessionkey[NTLMSSP_KEY_LEN] , const int flags , int *keylen ,
                   uint8_t *clientsealkey , uint8_t *serversealkey)
{
  gcry_md_hd_t md5_handle;

  memset(clientsealkey, 0, NTLMSSP_KEY_LEN);
  memset(serversealkey, 0, NTLMSSP_KEY_LEN);
  memcpy(clientsealkey, exportedsessionkey, NTLMSSP_KEY_LEN);
  if (flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY)
  {
    if (flags & NTLMSSP_NEGOTIATE_128)
    {
      /* The exportedsessionkey has already the good length just update the length*/
      *keylen = 16;
    }
    else
    {
      if (flags & NTLMSSP_NEGOTIATE_56)
      {
        memset(clientsealkey+7, 0, 9);
        *keylen = 7;
      }
      else
      {
        memset(clientsealkey+5, 0, 11);
        *keylen = 5;
      }
    }
    memcpy(serversealkey, clientsealkey, NTLMSSP_KEY_LEN);
    if (gcry_md_open(&md5_handle, GCRY_MD_MD5, 0)) {
      return;
    }
    gcry_md_write(md5_handle, clientsealkey, *keylen);
    gcry_md_write(md5_handle, CLIENT_SEAL_TEXT, strlen(CLIENT_SEAL_TEXT)+1); // +1 to get the final null-byte
    memcpy(clientsealkey, gcry_md_read(md5_handle, 0), NTLMSSP_KEY_LEN);
    gcry_md_reset(md5_handle);
    gcry_md_write(md5_handle, serversealkey, *keylen);
    gcry_md_write(md5_handle, SERVER_SEAL_TEXT, strlen(SERVER_SEAL_TEXT)+1); // +1 to get the final null-byte
    memcpy(serversealkey, gcry_md_read(md5_handle, 0), NTLMSSP_KEY_LEN);
    gcry_md_close(md5_handle);
  }
  else
  {
    if (flags & NTLMSSP_NEGOTIATE_128)
    {
      /* The exportedsessionkey has already the good length just update the length*/
      *keylen = 16;
    }
    else
    {
      *keylen = 8;
      if (flags & NTLMSSP_NEGOTIATE_56)
      {
        memset(clientsealkey+7, 0, 9);
      }
      else
      {
        memset(clientsealkey+5, 0, 11);
        clientsealkey[5]=0xe5;
        clientsealkey[6]=0x38;
        clientsealkey[7]=0xb0;
      }
    }
    memcpy(serversealkey, clientsealkey,*keylen);
  }
}
/* Create an NTLMSSP version 1 key.
 * password points to the ANSI password to encrypt, challenge points to
 * the 8 octet challenge string, key128 will do a 128 bit key if set to 1,
 * otherwise it will do a 40 bit key. The result is stored in
 * sspkey (expected to be NTLMSSP_KEY_LEN octets)
 */
/* dissect a string - header area contains:
     two byte len
     two byte maxlen
     four byte offset of string in data area
  The function returns the offset at the end of the string header,
  but the 'end' parameter returns the offset of the end of the string itself
  The 'start' parameter returns the offset of the beginning of the string
  If there's no string, just use the offset of the end of the tvb as start/end.
*/
static int
dissect_ntlmssp_string (tvbuff_t *tvb, wmem_allocator_t* allocator, int offset,
                        proto_tree *ntlmssp_tree,
                        bool unicode_strings,
                        int string_hf, int *start, int *end,
                        const uint8_t **stringp)
{
  proto_tree *tree          = NULL;
  proto_item *tf            = NULL;
  int16_t     string_length = tvb_get_letohs(tvb, offset);
  int16_t     string_maxlen = tvb_get_letohs(tvb, offset+2);
  int32_t     string_offset = tvb_get_letohl(tvb, offset+4);

  *start = (string_offset > offset+8 ? string_offset : (signed)tvb_reported_length(tvb));
  if (0 == string_length) {
    *end = *start;
    if (ntlmssp_tree)
      proto_tree_add_string(ntlmssp_tree, string_hf, tvb,
                            offset, 8, "NULL");
    if (stringp != NULL)
      *stringp = "";
    return offset+8;
  }

  if (unicode_strings) {
    /* UTF-16 string; must be 2-byte aligned */
    if ((string_offset & 1) != 0)
      string_offset++;
  }
  tf = proto_tree_add_item_ret_string(ntlmssp_tree, string_hf, tvb,
                           string_offset, string_length,
                           unicode_strings ? ENC_UTF_16|ENC_LITTLE_ENDIAN : ENC_ASCII|ENC_NA,
                           allocator, stringp);
  tree = proto_item_add_subtree(tf, ett_ntlmssp_string);
  proto_tree_add_uint(tree, hf_ntlmssp_string_len,
                      tvb, offset, 2, string_length);
  offset += 2;
  proto_tree_add_uint(tree, hf_ntlmssp_string_maxlen,
                      tvb, offset, 2, string_maxlen);
  offset += 2;
  proto_tree_add_uint(tree, hf_ntlmssp_string_offset,
                      tvb, offset, 4, string_offset);
  offset += 4;

  *end = string_offset + string_length;
  return offset;
}

/* dissect a generic blob - header area contains:
     two byte len
     two byte maxlen
     four byte offset of blob in data area
  The function returns the offset at the end of the blob header,
  but the 'end' parameter returns the offset of the end of the blob itself
*/
static int
dissect_ntlmssp_blob (tvbuff_t *tvb, packet_info *pinfo,
                      proto_tree *ntlmssp_tree, int offset,
                      int blob_hf, int *end, ntlmssp_blob *result)
{
  proto_item *tf          = NULL;
  proto_tree *tree        = NULL;
  uint16_t    blob_length = tvb_get_letohs(tvb, offset);
  uint16_t    blob_maxlen = tvb_get_letohs(tvb, offset+2);
  uint32_t    blob_offset = tvb_get_letohl(tvb, offset+4);

  if (0 == blob_length) {
    *end                  = (blob_offset > ((unsigned)offset)+8 ? blob_offset : ((unsigned)offset)+8);
    proto_tree_add_bytes_format_value(ntlmssp_tree, blob_hf, tvb, offset, 8, NULL, "Empty");
    result->length = 0;
    result->contents = NULL;
    return offset+8;
  }

  if (ntlmssp_tree) {
    tf = proto_tree_add_item (ntlmssp_tree, blob_hf, tvb,
                              blob_offset, blob_length, ENC_NA);
    tree = proto_item_add_subtree(tf, ett_ntlmssp_blob);
  }
  proto_tree_add_uint(tree, hf_ntlmssp_blob_len,
                      tvb, offset, 2, blob_length);
  offset += 2;
  proto_tree_add_uint(tree, hf_ntlmssp_blob_maxlen,
                      tvb, offset, 2, blob_maxlen);
  offset += 2;
  proto_tree_add_uint(tree, hf_ntlmssp_blob_offset,
                      tvb, offset, 4, blob_offset);
  offset += 4;

  *end = blob_offset + blob_length;

  if (blob_length < NTLMSSP_BLOB_MAX_SIZE) {
    result->length = blob_length;
    result->contents = (uint8_t *)tvb_memdup(wmem_file_scope(), tvb, blob_offset, blob_length);
  } else {
    expert_add_info_format(pinfo, tf, &ei_ntlmssp_v2_key_too_long,
                           "NTLM v2 key is %d bytes long, too big for our %d buffer", blob_length, NTLMSSP_BLOB_MAX_SIZE);
    result->length = 0;
    result->contents = NULL;
  }

  /*
   * XXX - for LmChallengeResponse (hf_ntlmssp_auth_lmresponse), should
   * we have a field for both Response (2.2.2.3 "LM_RESPONSE" and
   * 2.2.2.4 "LMv2_RESPONSE" in [MS-NLMP]) in addition to ClientChallenge
   * (only in 2.2.2.4 "LMv2_RESPONSE")?
   *
   * XXX - should we also dissect the fields of an NtChallengeResponse
   * (hf_ntlmssp_auth_ntresponse)?
   *
   * XXX - should we warn if the blob is too *small*?
   */
  if (blob_hf == hf_ntlmssp_auth_lmresponse) {
    /*
     * LMChallengeResponse.  It's either 2.2.2.3 "LM_RESPONSE" or
     * 2.2.2.4 "LMv2_RESPONSE", in [MS-NLMP].
     *
     * XXX - should we have a field for Response as well as
     * ClientChallenge?
     */
    if (tvb_memeql(tvb, blob_offset+8, (const uint8_t*)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", NTLMSSP_KEY_LEN) == 0) {
      /*
       * LMv2_RESPONSE.
       *
       * XXX - according to 2.2.2.4 "LMv2_RESPONSE", the ClientChallenge
       * is at an offset of 16 from the beginning of the blob; it's not
       * at the beginning of the blob.
       */
      proto_tree_add_item (ntlmssp_tree,
                           hf_ntlmssp_ntlm_client_challenge,
                           tvb, blob_offset, 8, ENC_NA);
    }
  } else if (blob_hf == hf_ntlmssp_auth_ntresponse) {
    /*
     * NTChallengeResponse.  It's either 2.2.2.6 "NTLM v1 Response:
     * NTLM_RESPONSE" or 2.2.2.8 "NTLM v2 Response: NTLMv2_RESPONSE"
     * in [MS-NLMP].
     */
    if (blob_length > 24) {
      /*
       * > 24 bytes, so it's "NTLM v2 Response: NTLMv2_RESPONSE".
       * An NTLMv2_RESPONSE has 16 bytes of Response followed
       * by an NTLMv2_CLIENT_CHALLENGE; an NTLMv2_CLIENT_CHALLENGE
       * is at least 32 bytes, so an NTLMv2_RESPONSE is at least
       * 48 bytes long.
       */
      dissect_ntlmv2_response(tvb, pinfo, tree, blob_offset, blob_length);
    }
  }

  return offset;
}

static int * const ntlmssp_negotiate_flags[] = {
    &hf_ntlmssp_negotiate_flags_80000000,
    &hf_ntlmssp_negotiate_flags_40000000,
    &hf_ntlmssp_negotiate_flags_20000000,
    &hf_ntlmssp_negotiate_flags_10000000,
    &hf_ntlmssp_negotiate_flags_8000000,
    &hf_ntlmssp_negotiate_flags_4000000,
    &hf_ntlmssp_negotiate_flags_2000000,
    &hf_ntlmssp_negotiate_flags_1000000,
    &hf_ntlmssp_negotiate_flags_800000,
    &hf_ntlmssp_negotiate_flags_400000,
    &hf_ntlmssp_negotiate_flags_200000,
    &hf_ntlmssp_negotiate_flags_100000,
    &hf_ntlmssp_negotiate_flags_80000,
    &hf_ntlmssp_negotiate_flags_40000,
    &hf_ntlmssp_negotiate_flags_20000,
    &hf_ntlmssp_negotiate_flags_10000,
    &hf_ntlmssp_negotiate_flags_8000,
    &hf_ntlmssp_negotiate_flags_4000,
    &hf_ntlmssp_negotiate_flags_2000,
    &hf_ntlmssp_negotiate_flags_1000,
    &hf_ntlmssp_negotiate_flags_800,
    &hf_ntlmssp_negotiate_flags_400,
    &hf_ntlmssp_negotiate_flags_200,
    &hf_ntlmssp_negotiate_flags_100,
    &hf_ntlmssp_negotiate_flags_80,
    &hf_ntlmssp_negotiate_flags_40,
    &hf_ntlmssp_negotiate_flags_20,
    &hf_ntlmssp_negotiate_flags_10,
    &hf_ntlmssp_negotiate_flags_08,
    &hf_ntlmssp_negotiate_flags_04,
    &hf_ntlmssp_negotiate_flags_02,
    &hf_ntlmssp_negotiate_flags_01,
    NULL
};

/* Dissect "version" */

/* From MS-NLMP:
    0   Major Version Number    1 byte
    1   Minor Version Number    1 byte
    2   Build Number            short(LE)
    3   (Reserved)              3 bytes
    4   NTLM Current Revision   1 byte
*/

static int
dissect_ntlmssp_version(tvbuff_t *tvb, int offset,
                        proto_tree *ntlmssp_tree)
{
  if (ntlmssp_tree) {
    proto_item *tf;
    proto_tree *version_tree;
    tf = proto_tree_add_none_format(ntlmssp_tree, hf_ntlmssp_version, tvb, offset, 8,
                                    "Version %u.%u (Build %u); NTLM Current Revision %u",
                                    tvb_get_uint8(tvb, offset),
                                    tvb_get_uint8(tvb, offset+1),
                                    tvb_get_letohs(tvb, offset+2),
                                    tvb_get_uint8(tvb, offset+7));
    version_tree = proto_item_add_subtree (tf, ett_ntlmssp_version);
    proto_tree_add_item(version_tree, hf_ntlmssp_version_major                , tvb, offset  , 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(version_tree, hf_ntlmssp_version_minor                , tvb, offset+1, 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(version_tree, hf_ntlmssp_version_build_number         , tvb, offset+2, 2, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(version_tree, hf_ntlmssp_version_ntlm_current_revision, tvb, offset+7, 1, ENC_LITTLE_ENDIAN);
  }
  return offset+8;
}

/* Dissect a NTLM response. This is documented at
   http://ubiqx.org/cifs/SMB.html#SMB.8, para 2.8.5.3 */

/* Attribute types */
/*
 * XXX - the davenport.sourceforge.net document cited above says that a
 * type of 5 has been seen, "apparently containing the 'parent' DNS
 * domain for servers in subdomains".
 * XXX: MS-NLMP info is newer than Davenport info;
 *      The attribute type list and the attribute names below are
 *      based upon MS-NLMP.
 */

#define NTLM_TARGET_INFO_END               0x0000
#define NTLM_TARGET_INFO_NB_COMPUTER_NAME  0x0001
#define NTLM_TARGET_INFO_NB_DOMAIN_NAME    0x0002
#define NTLM_TARGET_INFO_DNS_COMPUTER_NAME 0x0003
#define NTLM_TARGET_INFO_DNS_DOMAIN_NAME   0x0004
#define NTLM_TARGET_INFO_DNS_TREE_NAME     0x0005
#define NTLM_TARGET_INFO_FLAGS             0x0006
#define NTLM_TARGET_INFO_TIMESTAMP         0x0007
#define NTLM_TARGET_INFO_RESTRICTIONS      0x0008
#define NTLM_TARGET_INFO_TARGET_NAME       0x0009
#define NTLM_TARGET_INFO_CHANNEL_BINDINGS  0x000A

static const value_string ntlm_name_types[] = {
  { NTLM_TARGET_INFO_END,               "End of list" },
  { NTLM_TARGET_INFO_NB_COMPUTER_NAME,  "NetBIOS computer name" },
  { NTLM_TARGET_INFO_NB_DOMAIN_NAME,    "NetBIOS domain name" },
  { NTLM_TARGET_INFO_DNS_COMPUTER_NAME, "DNS computer name" },
  { NTLM_TARGET_INFO_DNS_DOMAIN_NAME,   "DNS domain name" },
  { NTLM_TARGET_INFO_DNS_TREE_NAME,     "DNS tree name" },
  { NTLM_TARGET_INFO_FLAGS,             "Flags" },
  { NTLM_TARGET_INFO_TIMESTAMP,         "Timestamp" },
  { NTLM_TARGET_INFO_RESTRICTIONS,      "Restrictions" },
  { NTLM_TARGET_INFO_TARGET_NAME,       "Target Name"},
  { NTLM_TARGET_INFO_CHANNEL_BINDINGS,  "Channel Bindings"},
  { 0, NULL }
};
static value_string_ext ntlm_name_types_ext = VALUE_STRING_EXT_INIT(ntlm_name_types);

/* The following *must* match the order of the list of attribute types   */
/*  Assumption: values in the list are a sequence starting with 0 and    */
/*  with no gaps allowing a direct access of the array by attribute type */
static int *ntlmssp_hf_challenge_target_info_hf_ptr_array[] = {
  &hf_ntlmssp_challenge_target_info_end,
  &hf_ntlmssp_challenge_target_info_nb_computer_name,
  &hf_ntlmssp_challenge_target_info_nb_domain_name,
  &hf_ntlmssp_challenge_target_info_dns_computer_name,
  &hf_ntlmssp_challenge_target_info_dns_domain_name,
  &hf_ntlmssp_challenge_target_info_dns_tree_name,
  &hf_ntlmssp_challenge_target_info_flags,
  &hf_ntlmssp_challenge_target_info_timestamp,
  &hf_ntlmssp_challenge_target_info_restrictions,
  &hf_ntlmssp_challenge_target_info_target_name,
  &hf_ntlmssp_challenge_target_info_channel_bindings
};

static int *ntlmssp_hf_ntlmv2_response_hf_ptr_array[] = {
  &hf_ntlmssp_ntlmv2_response_end,
  &hf_ntlmssp_ntlmv2_response_nb_computer_name,
  &hf_ntlmssp_ntlmv2_response_nb_domain_name,
  &hf_ntlmssp_ntlmv2_response_dns_computer_name,
  &hf_ntlmssp_ntlmv2_response_dns_domain_name,
  &hf_ntlmssp_ntlmv2_response_dns_tree_name,
  &hf_ntlmssp_ntlmv2_response_flags,
  &hf_ntlmssp_ntlmv2_response_timestamp,
  &hf_ntlmssp_ntlmv2_response_restrictions,
  &hf_ntlmssp_ntlmv2_response_target_name,
  &hf_ntlmssp_ntlmv2_response_channel_bindings
};

typedef struct _tif {
  int   *ett;
  int   *hf_item_type;
  int   *hf_item_length;
  int  **hf_attr_array_p;
} tif_t;

static tif_t ntlmssp_challenge_target_info_tif = {
  &ett_ntlmssp_challenge_target_info_item,
  &hf_ntlmssp_challenge_target_info_item_type,
  &hf_ntlmssp_challenge_target_info_item_len,
  ntlmssp_hf_challenge_target_info_hf_ptr_array
};

static tif_t ntlmssp_ntlmv2_response_tif = {
  &ett_ntlmssp_ntlmv2_response_item,
  &hf_ntlmssp_ntlmv2_response_item_type,
  &hf_ntlmssp_ntlmv2_response_item_len,
  ntlmssp_hf_ntlmv2_response_hf_ptr_array
};

/** See [MS-NLMP] 2.2.2.1 */
static int
dissect_ntlmssp_target_info_list(tvbuff_t *_tvb, packet_info *pinfo, proto_tree *tree,
                                 uint32_t target_info_offset, uint16_t target_info_length,
                                 tif_t *tif_p)
{
  tvbuff_t *tvb = tvb_new_subset_length(_tvb, target_info_offset, target_info_length);
  uint32_t item_offset = 0;
  uint16_t item_type = ~0;

  /* Now enumerate through the individual items in the list */

  while (tvb_bytes_exist(tvb, item_offset, 4) && (item_type != NTLM_TARGET_INFO_END)) {
    proto_item   *target_info_tf;
    proto_tree   *target_info_tree;
    uint32_t      content_offset;
    uint16_t      content_length;
    uint32_t      type_offset;
    uint32_t      len_offset;
    uint32_t      item_length;
    const uint8_t *text = NULL;

    int **hf_array_p = tif_p->hf_attr_array_p;

    /* Content type */
    type_offset = item_offset;
    item_type = tvb_get_letohs(tvb, type_offset);

    /* Content length */
    len_offset = type_offset + 2;
    content_length = tvb_get_letohs(tvb, len_offset);

    /* Content value */
    content_offset = len_offset + 2;
    item_length    = content_length + 4;

    if (!tvb_bytes_exist(tvb, item_offset, item_length)) {
        /* Mark the current item and all the rest as invalid */
        proto_tree_add_expert(tree, pinfo, &ei_ntlmssp_target_info_invalid,
                              tvb, item_offset, target_info_length - item_offset);
        return target_info_offset + target_info_length;
    }

    target_info_tree = proto_tree_add_subtree_format(tree, tvb, item_offset, item_length, *tif_p->ett, &target_info_tf,
                                  "Attribute: %s", val_to_str_ext(item_type, &ntlm_name_types_ext, "Unknown (%d)"));

    proto_tree_add_item (target_info_tree, *tif_p->hf_item_type,    tvb, type_offset, 2, ENC_LITTLE_ENDIAN);
    proto_tree_add_item (target_info_tree, *tif_p->hf_item_length,  tvb, len_offset,  2, ENC_LITTLE_ENDIAN);

    if (content_length > 0) {
      switch (item_type) {
      case NTLM_TARGET_INFO_NB_COMPUTER_NAME:
      case NTLM_TARGET_INFO_NB_DOMAIN_NAME:
      case NTLM_TARGET_INFO_DNS_COMPUTER_NAME:
      case NTLM_TARGET_INFO_DNS_DOMAIN_NAME:
      case NTLM_TARGET_INFO_DNS_TREE_NAME:
      case NTLM_TARGET_INFO_TARGET_NAME:
        proto_tree_add_item_ret_string(target_info_tree, *hf_array_p[item_type], tvb, content_offset, content_length, ENC_UTF_16|ENC_LITTLE_ENDIAN, pinfo->pool, &text);
        proto_item_append_text(target_info_tf, ": %s", text);
        break;

      case NTLM_TARGET_INFO_FLAGS:
        proto_tree_add_item(target_info_tree, *hf_array_p[item_type], tvb, content_offset, content_length, ENC_LITTLE_ENDIAN);
      break;

      case NTLM_TARGET_INFO_TIMESTAMP:
        dissect_nttime(tvb, target_info_tree, content_offset, *hf_array_p[item_type], ENC_LITTLE_ENDIAN);
        break;

      case NTLM_TARGET_INFO_RESTRICTIONS:
      case NTLM_TARGET_INFO_CHANNEL_BINDINGS:
        proto_tree_add_item(target_info_tree, *hf_array_p[item_type], tvb, content_offset, content_length, ENC_NA);
        break;

      default:
        proto_tree_add_expert(target_info_tree, pinfo, &ei_ntlmssp_target_info_attr,
                                   tvb, content_offset, content_length);
        break;
      }
    }

    item_offset += item_length;
  }

  return target_info_offset + item_offset;
}

/** See [MS-NLMP] 3.3.2 */
int
dissect_ntlmv2_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, int len)
{
  proto_item *ntlmv2_item = NULL;
  proto_tree *ntlmv2_tree = NULL;
  const int   orig_offset = offset;

  /* XXX - make sure we don't go past len? */
  if (tree) {
    ntlmv2_item = proto_tree_add_item(
      tree, hf_ntlmssp_ntlmv2_response, tvb,
      offset, len, ENC_NA);
    ntlmv2_tree = proto_item_add_subtree(
      ntlmv2_item, ett_ntlmssp_ntlmv2_response);
  }

  proto_tree_add_item(
    ntlmv2_tree, hf_ntlmssp_ntlmv2_response_ntproofstr, tvb,
    offset, 16, ENC_NA);
  offset += 16;

  proto_tree_add_item(ntlmv2_tree, hf_ntlmssp_ntlmv2_response_rversion, tvb, offset, 1, ENC_LITTLE_ENDIAN);
  offset += 1;

  proto_tree_add_item(ntlmv2_tree, hf_ntlmssp_ntlmv2_response_hirversion, tvb, offset, 1, ENC_LITTLE_ENDIAN);
  offset += 1;

  proto_tree_add_item(ntlmv2_tree, hf_ntlmssp_ntlmv2_response_z, tvb, offset, 6, ENC_NA);
  offset += 6;

  dissect_nttime(
    tvb, ntlmv2_tree, offset, hf_ntlmssp_ntlmv2_response_time, ENC_LITTLE_ENDIAN);
  offset += 8;
  proto_tree_add_item(
    ntlmv2_tree, hf_ntlmssp_ntlmv2_response_chal, tvb,
    offset, 8, ENC_NA);
  offset += 8;

  proto_tree_add_item(ntlmv2_tree, hf_ntlmssp_ntlmv2_response_z, tvb, offset, 4, ENC_NA);
  offset += 4;

  offset = dissect_ntlmssp_target_info_list(tvb, pinfo, ntlmv2_tree, offset, len - (offset - orig_offset), &ntlmssp_ntlmv2_response_tif);

  if ((offset - orig_offset) < len) {
    proto_tree_add_item(ntlmv2_tree, hf_ntlmssp_ntlmv2_response_pad, tvb, offset, len - (offset - orig_offset), ENC_NA);
  }

  return offset+len;
}

/* tapping into ntlmssph not yet implemented */
static int
dissect_ntlmssp_negotiate (tvbuff_t *tvb, packet_info* pinfo, int offset, proto_tree *ntlmssp_tree, ntlmssp_header_t *ntlmssph _U_)
{
  uint32_t negotiate_flags;
  int     data_start;
  int     data_end;
  int     item_start;
  int     item_end;

  /* NTLMSSP Negotiate Flags */
  negotiate_flags = tvb_get_letohl (tvb, offset);
  proto_tree_add_bitmask(ntlmssp_tree, tvb, offset, hf_ntlmssp_negotiate_flags, ett_ntlmssp_negotiate_flags, ntlmssp_negotiate_flags, ENC_LITTLE_ENDIAN);
  offset += 4;

  /*
   * XXX - the davenport document says that these might not be
   * sent at all, presumably meaning the length of the message
   * isn't enough to contain them.
   */
  offset = dissect_ntlmssp_string(tvb, pinfo->pool, offset, ntlmssp_tree, false,
                                  hf_ntlmssp_negotiate_domain,
                                  &data_start, &data_end, NULL);

  offset = dissect_ntlmssp_string(tvb, pinfo->pool, offset, ntlmssp_tree, false,
                                  hf_ntlmssp_negotiate_workstation,
                                  &item_start, &item_end, NULL);
  data_start = MIN(data_start, item_start);
  data_end   = MAX(data_end,   item_end);

  /* If there are more bytes before the data block dissect a version field
     if NTLMSSP_NEGOTIATE_VERSION is set in the flags (see MS-NLMP) */
  if (offset < data_start) {
    if (negotiate_flags & NTLMSSP_NEGOTIATE_VERSION)
      dissect_ntlmssp_version(tvb, offset, ntlmssp_tree);
  }
  return data_end;
}


static int
dissect_ntlmssp_challenge_target_info_blob (packet_info *pinfo, tvbuff_t *tvb, int offset,
                                            proto_tree *ntlmssp_tree,
                                            int *end)
{
  uint16_t challenge_target_info_length = tvb_get_letohs(tvb, offset);
  uint16_t challenge_target_info_maxlen = tvb_get_letohs(tvb, offset+2);
  uint32_t challenge_target_info_offset = tvb_get_letohl(tvb, offset+4);
  proto_item *tf = NULL;
  proto_tree *challenge_target_info_tree = NULL;

  /* the target info list is just a blob */
  if (0 == challenge_target_info_length) {
    *end = (challenge_target_info_offset > ((unsigned)offset)+8 ? challenge_target_info_offset : ((unsigned)offset)+8);
    proto_tree_add_none_format(ntlmssp_tree, hf_ntlmssp_challenge_target_info, tvb, offset, 8,
                          "Target Info List: Empty");
    return offset+8;
  }

  if (ntlmssp_tree) {
    tf = proto_tree_add_item (ntlmssp_tree, hf_ntlmssp_challenge_target_info, tvb,
                              challenge_target_info_offset, challenge_target_info_length, ENC_NA);
    challenge_target_info_tree = proto_item_add_subtree(tf, ett_ntlmssp_challenge_target_info);
  }
  proto_tree_add_uint(challenge_target_info_tree, hf_ntlmssp_challenge_target_info_len,
                      tvb, offset, 2, challenge_target_info_length);
  offset += 2;
  proto_tree_add_uint(challenge_target_info_tree, hf_ntlmssp_challenge_target_info_maxlen,
                      tvb, offset, 2, challenge_target_info_maxlen);
  offset += 2;
  proto_tree_add_uint(challenge_target_info_tree, hf_ntlmssp_challenge_target_info_offset,
                      tvb, offset, 4, challenge_target_info_offset);
  offset += 4;

  dissect_ntlmssp_target_info_list(tvb, pinfo, challenge_target_info_tree,
                                   challenge_target_info_offset, challenge_target_info_length,
                                   &ntlmssp_challenge_target_info_tif);

  *end = challenge_target_info_offset + challenge_target_info_length;
  return offset;
}

/* tapping into ntlmssph not yet implemented */
static int
dissect_ntlmssp_challenge (tvbuff_t *tvb, packet_info *pinfo, int offset,
                           proto_tree *ntlmssp_tree, ntlmssp_header_t *ntlmssph _U_)
{
  uint32_t        negotiate_flags = 0;
  int             item_start, item_end;
  int             data_start, data_end;       /* MIN and MAX seen */
  uint8_t         clientkey[NTLMSSP_KEY_LEN]; /* NTLMSSP cipher key for client */
  uint8_t         serverkey[NTLMSSP_KEY_LEN]; /* NTLMSSP cipher key for server*/
  ntlmssp_info   *conv_ntlmssp_info = NULL;
  conversation_t *conversation;
  bool            unicode_strings   = false;
  uint8_t         tmp[8];
  uint8_t         sspkey[NTLMSSP_KEY_LEN]; /* NTLMSSP cipher key */
  int             ssp_key_len;  /* Either 8 or 16 (40 bit or 128) */

  /*
   * Use the negotiate flags in this message, if they're present
   * in the capture, to determine whether strings are Unicode or
   * not.
   *
   * offset points at TargetNameFields; skip past it.
   */
  if (tvb_bytes_exist(tvb, offset+8, 4)) {
    negotiate_flags = tvb_get_letohl (tvb, offset+8);
    if (negotiate_flags & NTLMSSP_NEGOTIATE_UNICODE)
      unicode_strings = true;
  }

  /* Target name */
  /*
   * XXX - the davenport document (and MS-NLMP) calls this "Target Name",
   * presumably because non-domain targets are supported.
   * XXX - Original name "domain" changed to "target_name" to match MS-NLMP
   */
  offset = dissect_ntlmssp_string(tvb, pinfo->pool, offset, ntlmssp_tree, unicode_strings,
                                  hf_ntlmssp_challenge_target_name,
                                  &item_start, &item_end, NULL);
  data_start = item_start;
  data_end = item_end;

  /* NTLMSSP Negotiate Flags */
  proto_tree_add_bitmask(ntlmssp_tree, tvb, offset, hf_ntlmssp_negotiate_flags, ett_ntlmssp_negotiate_flags, ntlmssp_negotiate_flags, ENC_LITTLE_ENDIAN);
  offset += 4;

  /* NTLMSSP NT Lan Manager Challenge */
  proto_tree_add_item (ntlmssp_tree,
                       hf_ntlmssp_ntlm_server_challenge,
                       tvb, offset, 8, ENC_NA);

  /*
   * Store the flags and the RC4 state information with the conversation,
   * as they're needed in order to dissect subsequent messages.
   */
  conversation = find_or_create_conversation(pinfo);

  tvb_memcpy(tvb, tmp, offset, 8); /* challenge */
  /* We can face more than one NTLM exchange over the same couple of IP and ports ...*/
  conv_ntlmssp_info = (ntlmssp_info *)conversation_get_proto_data(conversation, proto_ntlmssp);
  /* XXX: The following code is (re)executed every time a particular frame is dissected
   *      (in whatever order). Thus it seems to me that "multiple exchanges" might not be
   *      handled well depending on the order that frames are visited after the initial dissection.
   */
  if (!conv_ntlmssp_info || memcmp(tmp, conv_ntlmssp_info->server_challenge, 8) != 0) {
    conv_ntlmssp_info = wmem_new0(wmem_file_scope(), ntlmssp_info);
    wmem_register_callback(wmem_file_scope(), ntlmssp_sessions_destroy_cb, conv_ntlmssp_info);
    /* Insert the flags into the conversation */
    conv_ntlmssp_info->flags = negotiate_flags;
    conv_ntlmssp_info->saw_challenge = true;
    /* Insert the RC4 state information into the conversation */
    tvb_memcpy(tvb, conv_ntlmssp_info->server_challenge, offset, 8);
    /* Between the challenge and the user provided password, we can build the
       NTLMSSP key and initialize the cipher if we are not in EXTENDED SESSION SECURITY
       in this case we need the client challenge as well*/
    /* BTW this is true just if we are in LM Authentication if not the logic is a bit different.
     * Right now it's not very clear what is LM Authentication it __seems__ to be when
     * NEGOTIATE NT ONLY is not set and NEGOTIATE EXTENDED SESSION SECURITY is not set as well*/
    if (!(conv_ntlmssp_info->flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY))
    {
      conv_ntlmssp_info->rc4_state_initialized = false;
      /* XXX - Make sure there is 24 bytes for the key */
      conv_ntlmssp_info->ntlm_response.contents = (uint8_t *)wmem_alloc0(wmem_file_scope(), 24);
      conv_ntlmssp_info->lm_response.contents = (uint8_t *)wmem_alloc0(wmem_file_scope(), 24);

      create_ntlmssp_v1_key(conv_ntlmssp_info->server_challenge,
                            NULL, sspkey, NULL, conv_ntlmssp_info->flags,
                            conv_ntlmssp_info->ntlm_response.contents,
                            conv_ntlmssp_info->lm_response.contents,
                            ntlmssph, pinfo, ntlmssp_tree);
      if (memcmp(sspkey, gbl_zeros, NTLMSSP_KEY_LEN) != 0) {
        get_sealing_rc4key(sspkey, conv_ntlmssp_info->flags, &ssp_key_len, clientkey, serverkey);
        if (!gcry_cipher_open(&conv_ntlmssp_info->rc4_handle_client, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0)) {
          if (gcry_cipher_setkey(conv_ntlmssp_info->rc4_handle_client, sspkey, ssp_key_len)) {
            gcry_cipher_close(conv_ntlmssp_info->rc4_handle_client);
            conv_ntlmssp_info->rc4_handle_client = NULL;
          }
        }
        if (!gcry_cipher_open(&conv_ntlmssp_info->rc4_handle_server, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0)) {
          if (gcry_cipher_setkey(conv_ntlmssp_info->rc4_handle_server, sspkey, ssp_key_len)) {
            gcry_cipher_close(conv_ntlmssp_info->rc4_handle_server);
            conv_ntlmssp_info->rc4_handle_server = NULL;
          }
        }
        if (conv_ntlmssp_info->rc4_handle_client && conv_ntlmssp_info->rc4_handle_server) {
          conv_ntlmssp_info->server_dest_port = pinfo->destport;
          conv_ntlmssp_info->rc4_state_initialized = true;
        }
      }
    }
    conversation_add_proto_data(conversation, proto_ntlmssp, conv_ntlmssp_info);
  }
  offset += 8;

  /* If no more bytes (ie: no "reserved", ...) before start of data block, then return */
  /* XXX: According to Davenport "This form is seen in older Win9x-based systems"      */
  /*      Also: I've seen a capture with an HTTP CONNECT proxy-authentication          */
  /*            message wherein the challenge from the proxy has this form.            */
  if (offset >= data_start) {
    return data_end;
  }

  /* Reserved (function not completely known) */
  /*
   * XXX - SSP key?  The davenport document says
   *
   *    The context field is typically populated when Negotiate Local
   *    Call is set. It contains an SSPI context handle, which allows
   *    the client to "short-circuit" authentication and effectively
   *    circumvent responding to the challenge. Physically, the context
   *    is two long values. This is covered in greater detail later,
   *    in the "Local Authentication" section.
   *
   * It also says that that information may be omitted.
   */
  proto_tree_add_item (ntlmssp_tree, hf_ntlmssp_reserved,
                       tvb, offset, 8, ENC_NA);
  offset += 8;

  /*
   * The presence or absence of this field is not obviously correlated
   * with any flags in the previous NEGOTIATE message or in this
   * message (other than the "Workstation Supplied" and "Domain
   * Supplied" flags in the NEGOTIATE message, at least in the capture
   * I've seen - but those also correlate with the presence of workstation
   * and domain name fields, so it doesn't seem to make sense that they
   * actually *indicate* whether the subsequent CHALLENGE has an
   * address list).
   */
  if (offset < data_start) {
    offset = dissect_ntlmssp_challenge_target_info_blob(pinfo, tvb, offset, ntlmssp_tree, &item_end);
    /* XXX: This code assumes that the address list in the data block */
    /*      is always after the target name. Is this OK ?             */
    data_end = MAX(data_end, item_end);
  }

  /* If there are more bytes before the data block dissect a version field
     if NTLMSSP_NEGOTIATE_VERSION is set in the flags (see MS-NLMP) */
  if (offset < data_start) {
    if (negotiate_flags & NTLMSSP_NEGOTIATE_VERSION)
      offset = dissect_ntlmssp_version(tvb, offset, ntlmssp_tree);
  }

  return MAX(offset, data_end);
}

static int
dissect_ntlmssp_auth (tvbuff_t *tvb, packet_info *pinfo, int offset,
                      proto_tree *ntlmssp_tree, ntlmssp_header_t *ntlmssph)
{
  int             item_start, item_end;
  int             data_start, data_end = 0;
  bool            have_negotiate_flags = false;
  uint32_t        negotiate_flags;
  uint8_t         sspkey[NTLMSSP_KEY_LEN];    /* exported session key */
  uint8_t         clientkey[NTLMSSP_KEY_LEN]; /* NTLMSSP cipher key for client */
  uint8_t         serverkey[NTLMSSP_KEY_LEN]; /* NTLMSSP cipher key for server*/
  uint8_t         encryptedsessionkey[NTLMSSP_KEY_LEN] = {0};
  ntlmssp_blob    sessionblob;
  bool            unicode_strings      = false;
  ntlmssp_info   *conv_ntlmssp_info;
  conversation_t *conversation;
  int             ssp_key_len;

  conv_ntlmssp_info = (ntlmssp_info *)p_get_proto_data(wmem_file_scope(), pinfo, proto_ntlmssp, NTLMSSP_CONV_INFO_KEY);
  if (conv_ntlmssp_info == NULL) {
    /*
     * There isn't any.  Is there any from this conversation?  If so,
     * it means this is the first time we've dissected this frame, so
     * we should give it flag info.
     */
    /* XXX: Create conv_ntlmssp_info & etc if no previous CHALLENGE seen */
    /*      so we'll have a place to store flags.                        */
    /*      This is a bit brute-force but looks like it will be OK.      */
    conversation = find_or_create_conversation(pinfo);
    conv_ntlmssp_info = (ntlmssp_info *)conversation_get_proto_data(conversation, proto_ntlmssp);
    if (conv_ntlmssp_info == NULL) {
      conv_ntlmssp_info = wmem_new0(wmem_file_scope(), ntlmssp_info);
      wmem_register_callback(wmem_file_scope(), ntlmssp_sessions_destroy_cb, conv_ntlmssp_info);
      conversation_add_proto_data(conversation, proto_ntlmssp, conv_ntlmssp_info);
    }
    /* XXX: The *conv_ntlmssp_info struct attached to the frame is the
            same as the one attached to the conversation. That is: *both* point to
            the exact same struct in memory.  Is this what is indended ?  */
    p_add_proto_data(wmem_file_scope(), pinfo, proto_ntlmssp, NTLMSSP_CONV_INFO_KEY, conv_ntlmssp_info);
  }

  /*
   * Get flag info from the original negotiate message, if any.
   * This is because the flag information is sometimes missing from
   * the AUTHENTICATE message, so we can't figure out whether
   * strings are Unicode or not by looking at *our* flags.
   *
   * MS-NLMP says:
   *
   * In 2.2.1.1 NEGOTIATE_MESSAGE:
   *
   *    NegotiateFlags (4 bytes): A NEGOTIATE structure that contains a set
   *    of flags, as defined in section 2.2.2.5. The client sets flags to
   *    indicate options it supports.
   *
   * In 2.2.1.2 CHALLENGE_MESSAGE:
   *
   *    NegotiateFlags (4 bytes): A NEGOTIATE structure that contains a set
   *    of flags, as defined by section 2.2.2.5. The server sets flags to
   *    indicate options it supports or, if there has been a NEGOTIATE_MESSAGE
   *    (section 2.2.1.1), the choices it has made from the options offered
   *    by the client.
   *
   * In 2.2.1.3 AUTHENTICATE_MESSAGE:
   *
   *    NegotiateFlags (4 bytes): In connectionless mode, a NEGOTIATE
   *    structure that contains a set of flags (section 2.2.2.5) and
   *    represents the conclusion of negotiation--the choices the client
   *    has made from the options the server offered in the CHALLENGE_MESSAGE.
   *    In connection-oriented mode, a NEGOTIATE structure that contains the
   *    set of bit flags (section 2.2.2.5) negotiated in the previous messages.
   *
   * As 1.3.1 NTLM Authentication Call Flow indicates, in connectionless
   * mode, there's no NEGOTIATE_MESSAGE, just a CHALLENGE_MESSAGE and
   * an AUTHENTICATE_MESSAGE.
   *
   * So, for connectionless mode, with no NEGOTIATE_MESSAGE, the flags
   * that are the result of negotiation are in the AUTHENTICATE_MESSAGE;
   * only at the time the AUTHENTICATE_MESSAGE is sent does the client
   * know what the server is offering, so, at that point, it can indicate
   * to the server which of those it supports, with the final result
   * specifying the capabilities offered by the server that are also
   * supported by the client.
   *
   * For connection-oriented mode, at the time of the CHALLENGE_MESSAGE,
   * the server knows what capabilities the client supports, as those
   * we specified in the NEGOTIATE_MESSAGE, so it returns the set of
   * capabilities, from the set that the client supports, that it also
   * supports, so the CHALLENGE_MESSAGE contains the final result.  The
   * AUTHENTICATE_MESSAGE "contains the set of bit flags ... negotiated
   * in the previous messages", so it should contain the same set of
   * bit flags that were in the CHALLENGE_MESSAGE.
   *
   * So we use the flags in this message, the AUTHENTICATE_MESSAGE, if
   * they're present; if this is connectionless mode, the flags in the
   * CHALLENGE_MESSAGE aren't sufficient, as they don't indicate what
   * the client supports, and if this is connection-oriented mode, the
   * flags here should match what's in the CHALLENGE_MESSAGE.
   *
   * The flags might be missing from this message; the message could
   * have been cut short by the snapshot length, and even if it's not,
   * some older protocol implementations omit it.  If they're missing,
   * we fall back on what's in the CHALLENGE_MESSAGE.
   *
   * XXX: I've seen a capture which does an HTTP CONNECT which:
   *      - has the NEGOTIATE & CHALLENGE messages in one TCP connection;
   *      - has the AUTHENTICATE message in a second TCP connection;
   *        (The authentication aparently succeeded).
   *      For that case, in order to get the flags from the CHALLENGE_MESSAGE,
   *      we'd somehow have to manage NTLMSSP exchanges that cross TCP
   *      connection boundaries.
   *
   * offset points at LmChallengeResponseFields; skip past
   * LmChallengeResponseFields, NtChallengeResponseFields,
   * DomainNameFields, UserNameFields, WorkstationFields,
   * and EncryptedRandomSessionKeyFields.
   */
  if (tvb_bytes_exist(tvb, offset+8+8+8+8+8+8, 4)) {
    /*
     * See where the Lan Manager response's blob begins;
     * the data area starts at, or before, that location.
     */
    data_start = tvb_get_letohl(tvb, offset+4);

    /*
     * See where the NTLM response's blob begins; the data area
     * starts at, or before, that location.
     */
    item_start = tvb_get_letohl(tvb, offset+8+4);
    data_start = MIN(data_start, item_start);

    /*
     * See where the domain name's blob begins; the data area
     * starts at, or before, that location.
     */
    item_start = tvb_get_letohl(tvb, offset+8+8+4);
    data_start = MIN(data_start, item_start);

    /*
     * See where the user name's blob begins; the data area
     * starts at, or before, that location.
     */
    item_start = tvb_get_letohl(tvb, offset+8+8+8+4);
    data_start = MIN(data_start, item_start);

    /*
     * See where the host name's blob begins; the data area
     * starts at, or before, that location.
     */
    item_start = tvb_get_letohl(tvb, offset+8+8+8+8+4);
    data_start = MIN(data_start, item_start);

    /*
     * See if we have a session key and flags.
     */
    if (offset+8+8+8+8+8 < data_start) {
      /*
       * We have a session key and flags.
       */
      negotiate_flags = tvb_get_letohl (tvb, offset+8+8+8+8+8+8);
      have_negotiate_flags = true;
      if (negotiate_flags & NTLMSSP_NEGOTIATE_UNICODE)
        unicode_strings = true;
    }
  }
  if (!have_negotiate_flags) {
    /*
     * The flags from this message aren't present; if we have the
     * flags from the CHALLENGE message, use them.
     */
    if (conv_ntlmssp_info != NULL && conv_ntlmssp_info->saw_challenge) {
      if (conv_ntlmssp_info->flags & NTLMSSP_NEGOTIATE_UNICODE)
        unicode_strings = true;
    }
  }

  /*
   * Sometimes the session key and flags are missing.
   * Sometimes the session key is present but the flags are missing.
   * XXX Who stay so ? Reading spec I would rather say the opposite: flags are
   * always present, session information are always there as well but sometime
   * session information could be null (in case of no session)
   * Sometimes they're both present.
   *
   * This does not correlate with any flags in the previous CHALLENGE
   * message, and only correlates with "Negotiate Unicode", "Workstation
   * Supplied", and "Domain Supplied" in the NEGOTIATE message - but
   * those don't make sense as flags to use to determine this.
   *
   * So we check all of the descriptors to figure out where the data
   * area begins, and if the session key or the flags would be in the
   * middle of the data area, we assume the field in question is
   * missing.
   *
   * XXX - Reading Davenport and MS-NLMP: as I see it the possibilities are:
   *       a. No session-key; no flags; no version ("Win9x")
   *       b. Session-key & flags.
   *       c. Session-key, flags & version.
   *    In cases b and c the session key may be "null".
   *
   */

  /* Lan Manager response */
  data_start = tvb_get_letohl(tvb, offset+4);
  offset = dissect_ntlmssp_blob(tvb, pinfo, ntlmssp_tree, offset,
                                hf_ntlmssp_auth_lmresponse,
                                &item_end,
                                conv_ntlmssp_info == NULL ? NULL :
                                &conv_ntlmssp_info->lm_response);
  data_end = MAX(data_end, item_end);

  /* NTLM response */
  item_start = tvb_get_letohl(tvb, offset+4);
  offset = dissect_ntlmssp_blob(tvb, pinfo, ntlmssp_tree, offset,
                                hf_ntlmssp_auth_ntresponse,
                                &item_end,
                                conv_ntlmssp_info == NULL ? NULL :
                                &conv_ntlmssp_info->ntlm_response);
  data_start = MIN(data_start, item_start);
  data_end = MAX(data_end, item_end);

  /* domain name */
  item_start = tvb_get_letohl(tvb, offset+4);
  offset = dissect_ntlmssp_string(tvb, pinfo->pool, offset, ntlmssp_tree,
                                  unicode_strings,
                                  hf_ntlmssp_auth_domain,
                                  &item_start, &item_end, &(ntlmssph->domain_name));
  /*ntlmssph->domain_name_len = item_end-item_start;*/
  data_start = MIN(data_start, item_start);
  data_end = MAX(data_end, item_end);

  /* user name */
  item_start = tvb_get_letohl(tvb, offset+4);
  offset = dissect_ntlmssp_string(tvb, pinfo->pool, offset, ntlmssp_tree,
                                  unicode_strings,
                                  hf_ntlmssp_auth_username,
                                  &item_start, &item_end, &(ntlmssph->acct_name));
  /*ntlmssph->acct_name_len = item_end-item_start;*/
  data_start = MIN(data_start, item_start);
  data_end = MAX(data_end, item_end);

  col_append_sep_fstr(pinfo->cinfo, COL_INFO, ", ", "User: %s\\%s",
                  ntlmssph->domain_name, ntlmssph->acct_name);

  /* hostname */
  item_start = tvb_get_letohl(tvb, offset+4);
  offset = dissect_ntlmssp_string(tvb, pinfo->pool, offset, ntlmssp_tree,
                                  unicode_strings,
                                  hf_ntlmssp_auth_hostname,
                                  &item_start, &item_end, &(ntlmssph->host_name));
  data_start = MIN(data_start, item_start);
  data_end = MAX(data_end, item_end);

  sessionblob.length = 0;
  if (offset < data_start) {
    /* Session Key */
    offset = dissect_ntlmssp_blob(tvb, pinfo, ntlmssp_tree, offset,
                                  hf_ntlmssp_auth_sesskey,
                                  &item_end, &sessionblob);
    data_end = MAX(data_end, item_end);
  }

  if (offset < data_start) {
    /* NTLMSSP Negotiate Flags */
    negotiate_flags = tvb_get_letohl (tvb, offset);
    proto_tree_add_bitmask(ntlmssp_tree, tvb, offset, hf_ntlmssp_negotiate_flags, ett_ntlmssp_negotiate_flags, ntlmssp_negotiate_flags, ENC_LITTLE_ENDIAN);
    offset += 4;

    /* If no previous flags seen (ie: no previous CHALLENGE) use flags
       from the AUTHENTICATE message).
       Assumption: (flags == 0) means flags not previously seen  */
    if ((conv_ntlmssp_info != NULL) && (conv_ntlmssp_info->flags == 0)) {
      conv_ntlmssp_info->flags = negotiate_flags;
    }
  } else
    negotiate_flags = 0;

  /* If there are more bytes before the data block dissect a version field
     if NTLMSSP_NEGOTIATE_VERSION is set in the flags (see MS-NLMP) */
  if (offset < data_start) {
    if (negotiate_flags & NTLMSSP_NEGOTIATE_VERSION) {
      offset = dissect_ntlmssp_version(tvb, offset, ntlmssp_tree);
    } else {
      proto_tree_add_item(ntlmssp_tree, hf_ntlmssp_ntlmv2_response_z, tvb, offset, 8, ENC_NA);
      offset += 8;
    }
  }

  /* If there are still more bytes before the data block dissect an MIC (message integrity_code) field */
  /*  (See MS-NLMP)                                                                    */
  if (offset < data_start) {
    proto_tree_add_item(ntlmssp_tree, hf_ntlmssp_message_integrity_code, tvb, offset, 16, ENC_NA);
    offset += 16;
  }

  if (sessionblob.length > NTLMSSP_KEY_LEN) {
    expert_add_info_format(pinfo, NULL, &ei_ntlmssp_blob_len_too_long, "Session blob length too long: %u", sessionblob.length);
  } else if (sessionblob.length != 0) {
    /* XXX - Is it a problem if sessionblob.length < NTLMSSP_KEY_LEN ? */
    memcpy(encryptedsessionkey, sessionblob.contents, sessionblob.length);
    /* Try to attach to an existing conversation if not then it's useless to try to do so
     * because we are missing important information (ie. server challenge)
     */
    if (conv_ntlmssp_info) {
      /* If we are in EXTENDED SESSION SECURITY then we can now initialize cipher */
      if ((conv_ntlmssp_info->flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY))
      {
        if (conv_ntlmssp_info->rc4_state_initialized) {
          /* XXX - Do we really need to reinitialize the cipher contexts? */
          gcry_cipher_close(conv_ntlmssp_info->rc4_handle_server);
          gcry_cipher_close(conv_ntlmssp_info->rc4_handle_client);
        }
        conv_ntlmssp_info->rc4_state_initialized = false;
        ntlmssp_create_session_key(pinfo,
                                   ntlmssp_tree,
                                   ntlmssph,
                                   conv_ntlmssp_info->flags,
                                   conv_ntlmssp_info->server_challenge,
                                   encryptedsessionkey,
                                   &conv_ntlmssp_info->ntlm_response,
                                   &conv_ntlmssp_info->lm_response);
        /* ssp is the exported session key */
        memcpy(sspkey, ntlmssph->session_key, NTLMSSP_KEY_LEN);
        if (memcmp(sspkey, gbl_zeros, NTLMSSP_KEY_LEN) != 0) {
          get_sealing_rc4key(sspkey, conv_ntlmssp_info->flags, &ssp_key_len, clientkey, serverkey);
          get_signing_key((uint8_t*)&conv_ntlmssp_info->sign_key_server, (uint8_t*)&conv_ntlmssp_info->sign_key_client, sspkey, ssp_key_len);
          if (!gcry_cipher_open (&conv_ntlmssp_info->rc4_handle_server, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0)) {
            if (gcry_cipher_setkey(conv_ntlmssp_info->rc4_handle_server, serverkey, ssp_key_len)) {
              gcry_cipher_close(conv_ntlmssp_info->rc4_handle_server);
              conv_ntlmssp_info->rc4_handle_server = NULL;
            }
          }
          if (!gcry_cipher_open (&conv_ntlmssp_info->rc4_handle_client, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0)) {
            if (gcry_cipher_setkey(conv_ntlmssp_info->rc4_handle_client, clientkey, ssp_key_len)) {
              gcry_cipher_close(conv_ntlmssp_info->rc4_handle_client);
              conv_ntlmssp_info->rc4_handle_client = NULL;
            }
          }
          if (conv_ntlmssp_info->rc4_handle_server && conv_ntlmssp_info->rc4_handle_client) {
            conv_ntlmssp_info->server_dest_port = pinfo->destport;
            conv_ntlmssp_info->rc4_state_initialized = true;
          }
        }
      }
     }
  }
  return MAX(offset, data_end);
}

static uint8_t*
get_sign_key(packet_info *pinfo, int cryptpeer)
{
  conversation_t *conversation;
  ntlmssp_info   *conv_ntlmssp_info;

  conversation = find_conversation_pinfo(pinfo, 0);
  if (conversation == NULL) {
    /* We don't have a conversation.  In this case, stop processing
       because we do not have enough info to decrypt the payload */
    return NULL;
  }
  else {
    /* We have a conversation, check for encryption state */
    conv_ntlmssp_info = (ntlmssp_info *)conversation_get_proto_data(conversation,
                                                    proto_ntlmssp);
    if (conv_ntlmssp_info == NULL) {
      /* No encryption state tied to the conversation.  Therefore, we
         cannot decrypt the payload */
      return NULL;
    }
    else {
      /* We have the encryption state in the conversation.  So return the
         crypt state tied to the requested peer
       */
      if (cryptpeer == 1) {
        return (uint8_t*)&conv_ntlmssp_info->sign_key_client;
      } else {
        return (uint8_t*)&conv_ntlmssp_info->sign_key_server;
      }
    }
  }
}

/*
 * Get the encryption state tied to this conversation.  cryptpeer indicates
 * whether to retrieve the client key (1) or the server key (0)
 */
static gcry_cipher_hd_t
get_encrypted_state(packet_info *pinfo, int cryptpeer)
{
  conversation_t *conversation;
  ntlmssp_info   *conv_ntlmssp_info;

  conversation = find_conversation_pinfo(pinfo, 0);
  if (conversation == NULL) {
    /* We don't have a conversation.  In this case, stop processing
       because we do not have enough info to decrypt the payload */
    return NULL;
  }
  else {
    /* We have a conversation, check for encryption state */
    conv_ntlmssp_info = (ntlmssp_info *)conversation_get_proto_data(conversation,
                                                    proto_ntlmssp);
    if (conv_ntlmssp_info == NULL) {
      /* No encryption state tied to the conversation.  Therefore, we
         cannot decrypt the payload */
      return NULL;
    }
    else {
      /* We have the encryption state in the conversation.  So return the
         crypt state tied to the requested peer
       */
      if (cryptpeer == 1) {
        return conv_ntlmssp_info->rc4_handle_client;
      } else {
        return conv_ntlmssp_info->rc4_handle_server;
      }
    }
  }
}

static tvbuff_t*
decrypt_data_payload(tvbuff_t *tvb, int offset, uint32_t encrypted_block_length,
                     packet_info *pinfo, proto_tree *tree _U_, void *key);
static void
store_verifier(tvbuff_t *tvb, int offset, uint32_t encrypted_block_length, packet_info *pinfo);

static void
decrypt_verifier(tvbuff_t *tvb, packet_info *pinfo);

static int
dissect_ntlmssp_payload(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
  volatile int          offset              = 0;
  proto_tree *volatile  ntlmssp_tree        = NULL;
  proto_item           *tf                  = NULL;
  uint32_t              length;
  uint32_t              encrypted_block_length;
  uint8_t               key[NTLMSSP_KEY_LEN];
  /* the magic ntlm is the identifier of a NTLMSSP packet that's 00 00 00 01 */
  uint32_t              ntlm_magic_size     = 4;
  uint32_t              ntlm_signature_size = 8;
  uint32_t              ntlm_seq_size       = 4;

  length = tvb_captured_length (tvb);
  /* signature + seq + real payload */
  encrypted_block_length = length - ntlm_magic_size;

  if (encrypted_block_length < (ntlm_signature_size + ntlm_seq_size)) {
    /* Don't know why this would happen, but if it does, don't even bother
       attempting decryption/dissection */
    return offset + length;
  }

  /* Setup a new tree for the NTLMSSP payload */
  if (tree) {
    tf = proto_tree_add_item (tree,
                              hf_ntlmssp_verf,
                              tvb, offset, -1, ENC_NA);

    ntlmssp_tree = proto_item_add_subtree (tf,
                                           ett_ntlmssp);
  }

  /*
   * Catch the ReportedBoundsError exception; the stuff we've been
   * handed doesn't necessarily run to the end of the packet, it's
   * an item inside a packet, so if it happens to be malformed (or
   * we, or a dissector we call, has a bug), so that an exception
   * is thrown, we want to report the error, but return and let
   * our caller dissect the rest of the packet.
   *
   * If it gets a BoundsError, we can stop, as there's nothing more
   * in the packet after our blob to see, so we just re-throw the
   * exception.
   */
  TRY {
    /* Version number */
    proto_tree_add_item (ntlmssp_tree, hf_ntlmssp_verf_vers,
                         tvb, offset, 4, ENC_LITTLE_ENDIAN);
    offset += 4;

    /* Encrypted body */
    proto_tree_add_item (ntlmssp_tree, hf_ntlmssp_verf_body,
                         tvb, offset, ntlm_signature_size + ntlm_seq_size, ENC_NA);
    memset(key, 0, sizeof(key));
    tvb_memcpy(tvb, key, offset, ntlm_signature_size + ntlm_seq_size);
    /* Try to decrypt */
    decrypt_data_payload (tvb, offset+(ntlm_signature_size + ntlm_seq_size), encrypted_block_length-(ntlm_signature_size + ntlm_seq_size), pinfo, ntlmssp_tree, key);
    store_verifier (tvb, offset, ntlm_signature_size + ntlm_seq_size, pinfo);
    decrypt_verifier (tvb, pinfo);
    /* let's try to hook ourselves here */

    offset += 12;
  } CATCH_NONFATAL_ERRORS {
    show_exception(tvb, pinfo, tree, EXCEPT_CODE, GET_MESSAGE);
  } ENDTRY;

  return offset;
}

static tvbuff_t*
decrypt_data_payload(tvbuff_t *tvb, int offset, uint32_t encrypted_block_length,
                     packet_info *pinfo, proto_tree *tree _U_, void *key)
{
  tvbuff_t            *decr_tvb; /* Used to display decrypted buffer */
  ntlmssp_packet_info *packet_ntlmssp_info;
  ntlmssp_packet_info *stored_packet_ntlmssp_info = NULL;

  /* Check to see if we already have state for this packet */
  packet_ntlmssp_info = (ntlmssp_packet_info *)p_get_proto_data(wmem_file_scope(), pinfo, proto_ntlmssp, NTLMSSP_PACKET_INFO_KEY);
  if (packet_ntlmssp_info == NULL) {
    /* We don't have any packet state, so create one */
    packet_ntlmssp_info = wmem_new0(wmem_file_scope(), ntlmssp_packet_info);
    p_add_proto_data(wmem_file_scope(), pinfo, proto_ntlmssp, NTLMSSP_PACKET_INFO_KEY, packet_ntlmssp_info);
  }
  if (!packet_ntlmssp_info->payload_decrypted) {
    conversation_t *conversation;
    ntlmssp_info   *conv_ntlmssp_info;

    /* Pull the challenge info from the conversation */
    conversation = find_conversation_pinfo(pinfo, 0);
    if (conversation == NULL) {
      /* There is no conversation, thus no encryption state */
      return NULL;
    }

    conv_ntlmssp_info = (ntlmssp_info *)conversation_get_proto_data(conversation,
                                                    proto_ntlmssp);
    if (conv_ntlmssp_info == NULL) {
      /* There is no NTLMSSP state tied to the conversation */
      return NULL;
    }
    if (!conv_ntlmssp_info->rc4_state_initialized) {
      /* The crypto sybsystem is not initialized.  This means that either
         the conversation did not include a challenge, or that we do not have the right password */
      return NULL;
    }
    if (key != NULL) {
      stored_packet_ntlmssp_info = (ntlmssp_packet_info *)g_hash_table_lookup(hash_packet, key);
    }
    if (stored_packet_ntlmssp_info != NULL && stored_packet_ntlmssp_info->payload_decrypted == true) {
      /* Mat TBD (stderr, "Found a already decrypted packet\n");*/
      memcpy(packet_ntlmssp_info, stored_packet_ntlmssp_info, sizeof(ntlmssp_packet_info));
      /* Mat TBD ws_log_buffer(packet_ntlmssp_info->decrypted_payload, encrypted_block_length, "Data");*/
    }
    else {
      gcry_cipher_hd_t rc4_handle;
      gcry_cipher_hd_t rc4_handle_peer;

      /* Get the pair of RC4 state structures.  One is used for to decrypt the
         payload.  The other is used to re-encrypt the payload to represent
         the peer */
      if (conv_ntlmssp_info->server_dest_port == pinfo->destport) {
        /* client */
        rc4_handle = get_encrypted_state(pinfo, 1);
        rc4_handle_peer = get_encrypted_state(pinfo, 0);
      } else {
        /* server */
        rc4_handle = get_encrypted_state(pinfo, 0);
        rc4_handle_peer = get_encrypted_state(pinfo, 1);
      }

      if (rc4_handle == NULL) {
        /* There is no encryption state, so we cannot decrypt */
        return NULL;
      }

      /* Store the decrypted contents in the packet state struct
         (of course at this point, they aren't decrypted yet) */
      packet_ntlmssp_info->decrypted_payload = (uint8_t *)tvb_memdup(wmem_file_scope(), tvb, offset,
                                                          encrypted_block_length);
      packet_ntlmssp_info->payload_len = encrypted_block_length;
      decrypted_payloads = g_slist_prepend(decrypted_payloads,
                                           packet_ntlmssp_info->decrypted_payload);
      if (key != NULL) {
        uint8_t *perm_key = g_new(uint8_t, NTLMSSP_KEY_LEN);
        memcpy(perm_key, key, NTLMSSP_KEY_LEN);
        g_hash_table_insert(hash_packet, perm_key, packet_ntlmssp_info);
      }

      /* Do the decryption of the payload */
      gcry_cipher_decrypt(rc4_handle, packet_ntlmssp_info->decrypted_payload, encrypted_block_length, NULL, 0);

      /* decrypt the verifier */
      /*ws_log_buffer(packet_ntlmssp_info->decrypted_payload, encrypted_block_length, "data");*/
      /* We setup a temporary buffer so we can re-encrypt the payload after
         decryption.  This is to update the opposite peer's RC4 state
         it's useful when we have only one key for both conversation
         in case of KEY_EXCH we have independent key so this is not needed*/
      if (!(NTLMSSP_NEGOTIATE_KEY_EXCH & conv_ntlmssp_info->flags)) {
        uint8_t *peer_block;
        peer_block = (uint8_t *)wmem_memdup(pinfo->pool, packet_ntlmssp_info->decrypted_payload, encrypted_block_length);
        gcry_cipher_decrypt(rc4_handle_peer, peer_block, encrypted_block_length, NULL, 0);
      }

      packet_ntlmssp_info->payload_decrypted = true;
    }
  }

 /* Show the decrypted buffer in a new window */
  decr_tvb = tvb_new_child_real_data(tvb, packet_ntlmssp_info->decrypted_payload,
                                     encrypted_block_length,
                                     encrypted_block_length);

  add_new_data_source(pinfo, decr_tvb,
                      "Decrypted data");
  return decr_tvb;
}

static int
dissect_ntlmssp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
  volatile int          offset       = 0;
  proto_tree *volatile  ntlmssp_tree = NULL;
  proto_item           *tf, *type_item;
  ntlmssp_header_t     *ntlmssph;

  /* Check if it is a signing signature */
  if (tvb_bytes_exist(tvb, offset, 16) &&
      tvb_reported_length_remaining(tvb, offset) == 16 &&
      tvb_get_uint8(tvb, offset) == 0x01)
  {
      tvbuff_t *verf_tvb = tvb_new_subset_length(tvb, offset, 16);
      offset += dissect_ntlmssp_verf(verf_tvb, pinfo, tree, NULL);
      return offset;
  }

  ntlmssph = wmem_new(pinfo->pool, ntlmssp_header_t);
  ntlmssph->type = 0;
  ntlmssph->domain_name = NULL;
  ntlmssph->acct_name = NULL;
  ntlmssph->host_name = NULL;
  memset(ntlmssph->session_key, 0, NTLMSSP_KEY_LEN);

  /* Setup a new tree for the NTLMSSP payload */
  tf = proto_tree_add_item (tree,
                              proto_ntlmssp,
                              tvb, offset, -1, ENC_NA);

  ntlmssp_tree = proto_item_add_subtree (tf, ett_ntlmssp);

  /*
   * Catch the ReportedBoundsError exception; the stuff we've been
   * handed doesn't necessarily run to the end of the packet, it's
   * an item inside a packet, so if it happens to be malformed (or
   * we, or a dissector we call, has a bug), so that an exception
   * is thrown, we want to report the error, but return and let
   * our caller dissect the rest of the packet.
   *
   * If it gets a BoundsError, we can stop, as there's nothing more
   * in the packet after our blob to see, so we just re-throw the
   * exception.
   */
  TRY {
    /* NTLMSSP constant */
    proto_tree_add_item (ntlmssp_tree, hf_ntlmssp_auth,
                         tvb, offset, 8, ENC_ASCII);
    offset += 8;

    /* NTLMSSP Message Type */
    type_item = proto_tree_add_item (ntlmssp_tree, hf_ntlmssp_message_type,
                         tvb, offset, 4, ENC_LITTLE_ENDIAN);
    ntlmssph->type = tvb_get_letohl (tvb, offset);
    offset += 4;

    col_append_sep_str(pinfo->cinfo, COL_INFO, ", ",
                    val_to_str_const(ntlmssph->type,
                                     ntlmssp_message_types,
                                     "Unknown NTLMSSP message type"));

    /* Call the appropriate dissector based on the Message Type */
    switch (ntlmssph->type) {

    case NTLMSSP_NEGOTIATE:
      dissect_ntlmssp_negotiate (tvb, pinfo, offset, ntlmssp_tree, ntlmssph);
      break;

    case NTLMSSP_CHALLENGE:
      dissect_ntlmssp_challenge (tvb, pinfo, offset, ntlmssp_tree, ntlmssph);
      break;

    case NTLMSSP_AUTH:
      dissect_ntlmssp_auth (tvb, pinfo, offset, ntlmssp_tree, ntlmssph);
      break;

    default:
      /* Unrecognized message type */
      expert_add_info(pinfo, type_item, &ei_ntlmssp_message_type);
      break;
    }
  } CATCH_NONFATAL_ERRORS {

    show_exception(tvb, pinfo, tree, EXCEPT_CODE, GET_MESSAGE);
  } ENDTRY;

  tap_queue_packet(ntlmssp_tap, pinfo, ntlmssph);
  return tvb_captured_length(tvb);
}

static void
store_verifier(tvbuff_t *tvb, int offset, uint32_t encrypted_block_length, packet_info *pinfo)
{
  ntlmssp_packet_info *packet_ntlmssp_info;

  packet_ntlmssp_info = (ntlmssp_packet_info*)p_get_proto_data(wmem_file_scope(), pinfo, proto_ntlmssp, NTLMSSP_PACKET_INFO_KEY);
  if (packet_ntlmssp_info == NULL) {
    /* We don't have any packet state, so create one */
    packet_ntlmssp_info = wmem_new0(wmem_file_scope(), ntlmssp_packet_info);
    p_add_proto_data(wmem_file_scope(), pinfo, proto_ntlmssp, NTLMSSP_PACKET_INFO_KEY, packet_ntlmssp_info);
  }

  if (!packet_ntlmssp_info->verifier_decrypted) {
    /* Store all necessary info for later decryption */
    packet_ntlmssp_info->verifier_offset = offset;
    packet_ntlmssp_info->verifier_block_length = encrypted_block_length;
    /* Setup the buffer to decrypt to */
    tvb_memcpy(tvb, packet_ntlmssp_info->verifier,
      offset, MIN(encrypted_block_length, sizeof(packet_ntlmssp_info->verifier)));
  }
}

/*
 * See page 45 of "DCE/RPC over SMB" by Luke Kenneth Casson Leighton.
 */
static void
decrypt_verifier(tvbuff_t *tvb, packet_info *pinfo)
{
  proto_tree          *decr_tree;
  conversation_t      *conversation;
  uint8_t*              sign_key;
  gcry_cipher_hd_t     rc4_handle;
  gcry_cipher_hd_t     rc4_handle_peer;
  tvbuff_t            *decr_tvb; /* Used to display decrypted buffer */
  uint8_t             *peer_block;
  uint8_t             *check_buf;
  uint8_t              calculated_md5[NTLMSSP_KEY_LEN];
  ntlmssp_info        *conv_ntlmssp_info;
  ntlmssp_packet_info *packet_ntlmssp_info;
  int                  decrypted_offset    = 0;
  int                  sequence            = 0;

  packet_ntlmssp_info = (ntlmssp_packet_info *)p_get_proto_data(wmem_file_scope(), pinfo, proto_ntlmssp, NTLMSSP_PACKET_INFO_KEY);
  if (packet_ntlmssp_info == NULL) {
    /* We don't have data for this packet */
    return;
  }
  conversation = find_conversation_pinfo(pinfo, 0);
  if (conversation == NULL) {
    /* There is no conversation, thus no encryption state */
    return;
  }
  conv_ntlmssp_info = (ntlmssp_info *)conversation_get_proto_data(conversation,
                                                  proto_ntlmssp);
  if (conv_ntlmssp_info == NULL) {
    /* There is no NTLMSSP state tied to the conversation */
    return;
  }

  if (!packet_ntlmssp_info->verifier_decrypted) {
    if (!conv_ntlmssp_info->rc4_state_initialized) {
      /* The crypto subsystem is not initialized.  This means that either
         the conversation did not include a challenge, or we are doing
         something other than NTLMSSP v1 */
      return;
    }
    if (conv_ntlmssp_info->server_dest_port == pinfo->destport) {
      /* client talk to server */
      rc4_handle = get_encrypted_state(pinfo, 1);
      sign_key = get_sign_key(pinfo, 1);
      rc4_handle_peer = get_encrypted_state(pinfo, 0);
    } else {
      rc4_handle = get_encrypted_state(pinfo, 0);
      sign_key = get_sign_key(pinfo, 0);
      rc4_handle_peer = get_encrypted_state(pinfo, 1);
    }

    if (rc4_handle == NULL || rc4_handle_peer == NULL) {
      /* There is no encryption state, so we cannot decrypt */
      return;
    }

    /*if (!(NTLMSSP_NEGOTIATE_KEY_EXCH & packet_ntlmssp_info->flags)) {*/
    if (conv_ntlmssp_info->flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY) {
      if ((NTLMSSP_NEGOTIATE_KEY_EXCH & conv_ntlmssp_info->flags)) {
        /* The spec says that if we have a key exchange then we have the signature that is encrypted
         * otherwise it's just a hmac_md5(keysign, concat(message, sequence))[0..7]
         */
        if (gcry_cipher_decrypt(rc4_handle, packet_ntlmssp_info->verifier, 8, NULL, 0)) {
          return;
        }
      }
      /*
       * Trying to check the HMAC MD5 of the message against the calculated one works great with LDAP payload but
       * don't with DCE/RPC calls.
       * TODO Some analysis needs to be done ...
       */
      if (sign_key != NULL) {
        check_buf = (uint8_t *)wmem_alloc(pinfo->pool, packet_ntlmssp_info->payload_len+4);
        tvb_memcpy(tvb, &sequence, packet_ntlmssp_info->verifier_offset+8, 4);
        memcpy(check_buf, &sequence, 4);
        memcpy(check_buf+4, packet_ntlmssp_info->decrypted_payload, packet_ntlmssp_info->payload_len);
        if (ws_hmac_buffer(GCRY_MD_MD5, calculated_md5, check_buf, (int)(packet_ntlmssp_info->payload_len+4), sign_key, NTLMSSP_KEY_LEN)) {
          return;
        }
        /*
        ws_log_buffer(packet_ntlmssp_info->verifier, 8, "HMAC from packet");
        ws_log_buffer(calculated_md5, 8, "HMAC");
        */
      }
    }
    else {
      /* The packet has a PAD then a checksum then a sequence and they are encoded in this order so we can decrypt all at once */
      /* Do the actual decryption of the verifier */
      if (gcry_cipher_decrypt(rc4_handle, packet_ntlmssp_info->verifier, packet_ntlmssp_info->verifier_block_length, NULL, 0)) {
        return;
      }
    }


    /* We setup a temporary buffer so we can re-encrypt the payload after
       decryption. This is to update the opposite peer's RC4 state
       This is not needed when we just have EXTENDED SESSION SECURITY because the signature is not encrypted
       and it's also not needed when we have key exchange because server and client have independent keys */
    if (!(NTLMSSP_NEGOTIATE_KEY_EXCH & conv_ntlmssp_info->flags) && !(NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY & conv_ntlmssp_info->flags)) {
      peer_block = (uint8_t *)wmem_memdup(pinfo->pool, packet_ntlmssp_info->verifier, packet_ntlmssp_info->verifier_block_length);
      if (gcry_cipher_decrypt(rc4_handle_peer, peer_block, packet_ntlmssp_info->verifier_block_length, NULL, 0)) {
        return;
      }
    }

    /* Mark the packet as decrypted so that subsequent attempts to dissect
       the packet use the already decrypted payload instead of attempting
       to decrypt again */
    packet_ntlmssp_info->verifier_decrypted = true;
  }

  /* Show the decrypted buffer in a new window */
  decr_tvb = tvb_new_child_real_data(tvb, packet_ntlmssp_info->verifier,
                                     packet_ntlmssp_info->verifier_block_length,
                                     packet_ntlmssp_info->verifier_block_length);
  add_new_data_source(pinfo, decr_tvb,
                      "Decrypted NTLMSSP Verifier");

  /* Show the decrypted payload in the tree */
  decr_tree = proto_tree_add_subtree_format(NULL, decr_tvb, 0, -1,
                           ett_ntlmssp, NULL,
                           "Decrypted Verifier (%d byte%s)",
                           packet_ntlmssp_info->verifier_block_length,
                           plurality(packet_ntlmssp_info->verifier_block_length, "", "s"));

  if (( conv_ntlmssp_info->flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY)) {
    proto_tree_add_item (decr_tree, hf_ntlmssp_verf_hmacmd5,
                         decr_tvb, decrypted_offset, 8, ENC_NA);
    decrypted_offset += 8;

    /* Incrementing sequence number of DCE conversation */
    proto_tree_add_item (decr_tree, hf_ntlmssp_verf_sequence,
                         decr_tvb, decrypted_offset, 4, ENC_NA);
  }
  else {
    /* RANDOM PAD usually it's 0 */
    proto_tree_add_item (decr_tree, hf_ntlmssp_verf_randompad,
                         decr_tvb, decrypted_offset, 4, ENC_LITTLE_ENDIAN);
    decrypted_offset += 4;

    /* CRC32 of the DCE fragment data */
    proto_tree_add_item (decr_tree, hf_ntlmssp_verf_crc32,
                         decr_tvb, decrypted_offset, 4, ENC_LITTLE_ENDIAN);
    decrypted_offset += 4;

    /* Incrementing sequence number of DCE conversation */
    proto_tree_add_item (decr_tree, hf_ntlmssp_verf_sequence,
                         decr_tvb, decrypted_offset, 4, ENC_NA);
  }
}

/* Used when NTLMSSP is done over DCE/RPC because in this case verifier and real payload are not contiguous*/
static int
dissect_ntlmssp_payload_only(tvbuff_t *tvb, packet_info *pinfo, _U_ proto_tree *tree, void *data)
{
  volatile int          offset       = 0;
  proto_tree *volatile  ntlmssp_tree = NULL;
  uint32_t              encrypted_block_length;
  tvbuff_t *volatile    decr_tvb;
  tvbuff_t**            ret_decr_tvb = (tvbuff_t**)data;

  if (ret_decr_tvb)
    *ret_decr_tvb = NULL;
  /* the magic ntlm is the identifier of a NTLMSSP packet that's 00 00 00 01
   */
  encrypted_block_length = tvb_captured_length (tvb);
  /* signature + seq + real payload */

  /* Setup a new tree for the NTLMSSP payload */
#if 0
  if (tree) {
    tf = proto_tree_add_item (tree,
                              hf_ntlmssp_verf,
                              tvb, offset, -1, ENC_NA);

    ntlmssp_tree = proto_item_add_subtree (tf,
                                           ett_ntlmssp);
  }
#endif
  /*
   * Catch the ReportedBoundsError exception; the stuff we've been
   * handed doesn't necessarily run to the end of the packet, it's
   * an item inside a packet, so if it happens to be malformed (or
   * we, or a dissector we call, has a bug), so that an exception
   * is thrown, we want to report the error, but return and let
   * our caller dissect the rest of the packet.
   *
   * If it gets a BoundsError, we can stop, as there's nothing more
   * in the packet after our blob to see, so we just re-throw the
   * exception.
   */
  TRY {
    /* Version number */

    /* Try to decrypt */
    decr_tvb = decrypt_data_payload (tvb, offset, encrypted_block_length, pinfo, ntlmssp_tree, NULL);
    if (ret_decr_tvb)
       *ret_decr_tvb = decr_tvb;
    /* let's try to hook ourselves here */

  } CATCH_NONFATAL_ERRORS {

    show_exception(tvb, pinfo, tree, EXCEPT_CODE, GET_MESSAGE);
  } ENDTRY;

  return offset;
}

/* Used when NTLMSSP is done over DCE/RPC because in this case verifier and real payload are not contiguous
 * But in fact this function could be merged with wrap_dissect_ntlmssp_verf because it's only used there
 */
static int
dissect_ntlmssp_verf(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
  volatile int          offset       = 0;
  proto_tree *volatile  ntlmssp_tree = NULL;
  proto_item           *tf           = NULL;
  uint32_t              verifier_length;
  uint32_t              encrypted_block_length;

  verifier_length = tvb_captured_length (tvb);
  encrypted_block_length = verifier_length - 4;

  if (encrypted_block_length < 12) {
    /* Don't know why this would happen, but if it does, don't even bother
       attempting decryption/dissection */
    return offset + verifier_length;
  }

  /* Setup a new tree for the NTLMSSP payload */
  if (tree) {
    tf = proto_tree_add_item (tree,
                              hf_ntlmssp_verf,
                              tvb, offset, -1, ENC_NA);

    ntlmssp_tree = proto_item_add_subtree (tf,
                                           ett_ntlmssp);
  }

  /*
   * Catch the ReportedBoundsError exception; the stuff we've been
   * handed doesn't necessarily run to the end of the packet, it's
   * an item inside a packet, so if it happens to be malformed (or
   * we, or a dissector we call, has a bug), so that an exception
   * is thrown, we want to report the error, but return and let
   * our caller dissect the rest of the packet.
   *
   * If it gets a BoundsError, we can stop, as there's nothing more
   * in the packet after our blob to see, so we just re-throw the
   * exception.
   */
  TRY {
    /* Version number */
    proto_tree_add_item (ntlmssp_tree, hf_ntlmssp_verf_vers,
                         tvb, offset, 4, ENC_LITTLE_ENDIAN);
    offset += 4;

    /* Encrypted body */
    proto_tree_add_item (ntlmssp_tree, hf_ntlmssp_verf_body,
                         tvb, offset, encrypted_block_length, ENC_NA);

    /* Extract and store the verifier for later decryption */
    store_verifier (tvb, offset, encrypted_block_length, pinfo);
    /* let's try to hook ourselves here */

    offset += 12;
    offset += encrypted_block_length;
  } CATCH_NONFATAL_ERRORS {

    show_exception(tvb, pinfo, tree, EXCEPT_CODE, GET_MESSAGE);
  } ENDTRY;

  return offset;
}

static tvbuff_t *
wrap_dissect_ntlmssp_payload_only(tvbuff_t *header_tvb _U_,
                                  tvbuff_t *payload_tvb,
                                  tvbuff_t *trailer_tvb _U_,
                                  tvbuff_t *auth_tvb _U_,
                                  packet_info *pinfo,
                                  dcerpc_auth_info *auth_info _U_)
{
  tvbuff_t *decrypted_tvb;

  dissect_ntlmssp_payload_only(payload_tvb, pinfo, NULL, &decrypted_tvb);
  /* Now the payload is decrypted, we can then decrypt the verifier which was stored earlier */
  decrypt_verifier(payload_tvb, pinfo);
  return decrypted_tvb;
}

static unsigned
header_hash(const void *pointer)
{
  uint32_t crc = ~crc32c_calculate(pointer, NTLMSSP_KEY_LEN, CRC32C_PRELOAD);
  /* Mat TBD ws_debug("Val: %u", crc);*/
  return crc;
}

static gboolean
header_equal(const void *pointer1, const void *pointer2)
{
  if (!memcmp(pointer1, pointer2, NTLMSSP_KEY_LEN)) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

static void
ntlmssp_init_protocol(void)
{
  hash_packet = g_hash_table_new_full(header_hash, header_equal, g_free, NULL);
}

static void
ntlmssp_cleanup_protocol(void)
{
  if (decrypted_payloads != NULL) {
    g_slist_free(decrypted_payloads);
    decrypted_payloads = NULL;
  }
  g_hash_table_destroy(hash_packet);
}



static int
wrap_dissect_ntlmssp(tvbuff_t *tvb, int offset, packet_info *pinfo,
                     proto_tree *tree, dcerpc_info *di _U_, uint8_t *drep _U_)
{
  tvbuff_t *auth_tvb;

  auth_tvb = tvb_new_subset_remaining(tvb, offset);

  dissect_ntlmssp(auth_tvb, pinfo, tree, NULL);

  return tvb_captured_length_remaining(tvb, offset);
}

static int
wrap_dissect_ntlmssp_verf(tvbuff_t *tvb, int offset, packet_info *pinfo,
                          proto_tree *tree, dcerpc_info *di _U_, uint8_t *drep _U_)
{
  tvbuff_t *auth_tvb;

  auth_tvb = tvb_new_subset_remaining(tvb, offset);
  return dissect_ntlmssp_verf(auth_tvb, pinfo, tree, NULL);
}

static dcerpc_auth_subdissector_fns ntlmssp_sign_fns = {
  wrap_dissect_ntlmssp,                 /* Bind */
  wrap_dissect_ntlmssp,                 /* Bind ACK */
  wrap_dissect_ntlmssp,                 /* AUTH3 */
  wrap_dissect_ntlmssp_verf,            /* Request verifier */
  wrap_dissect_ntlmssp_verf,            /* Response verifier */
  NULL,                                 /* Request data */
  NULL                                  /* Response data */
};

static dcerpc_auth_subdissector_fns ntlmssp_seal_fns = {
  wrap_dissect_ntlmssp,                 /* Bind */
  wrap_dissect_ntlmssp,                 /* Bind ACK */
  wrap_dissect_ntlmssp,                 /* AUTH3 */
  wrap_dissect_ntlmssp_verf,            /* Request verifier */
  wrap_dissect_ntlmssp_verf,            /* Response verifier */
  wrap_dissect_ntlmssp_payload_only,    /* Request data */
  wrap_dissect_ntlmssp_payload_only     /* Response data */
};

static const value_string MSV1_0_CRED_VERSION[] = {
    { 0x00000000, "MSV1_0_CRED_VERSION" },
    { 0x00000002, "MSV1_0_CRED_VERSION_V2" },
    { 0x00000004, "MSV1_0_CRED_VERSION_V3" },
    { 0xffff0001, "MSV1_0_CRED_VERSION_IUM" },
    { 0xffff0002, "MSV1_0_CRED_VERSION_REMOTE" },
    { 0xfffffffe, "MSV1_0_CRED_VERSION_RESERVED_1" },
    { 0xffffffff, "MSV1_0_CRED_VERSION_INVALID" },
    { 0, NULL }
};

#define MSV1_0_CRED_LM_PRESENT      0x00000001
#define MSV1_0_CRED_NT_PRESENT      0x00000002
#define MSV1_0_CRED_REMOVED         0x00000004
#define MSV1_0_CRED_CREDKEY_PRESENT 0x00000008
#define MSV1_0_CRED_SHA_PRESENT     0x00000010

static int* const MSV1_0_CRED_FLAGS_bits[] = {
	&hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_LM_PRESENT,
	&hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_NT_PRESENT,
	&hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_REMOVED,
	&hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_CREDKEY_PRESENT,
	&hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_SHA_PRESENT,
	NULL
};

static const value_string MSV1_0_CREDENTIAL_KEY_TYPE[] = {
    { 0, "InvalidCredKey" },
    { 1, "IUMCredKey" },
    { 2, "DomainUserCredKey" },
    { 3, "LocalUserCredKey" },
    { 4, "ExternallySuppliedCredKey" },
    { 0, NULL }
};

#define MSV1_0_CREDENTIAL_KEY_LENGTH 20

int
dissect_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL(tvbuff_t *tvb, int offset, proto_tree *tree)
{
	proto_item *item;
	proto_tree *subtree;
	uint32_t EncryptedCredsSize;

	if (tvb_captured_length(tvb) < 36)
		return offset;

	item = proto_tree_add_item(tree, hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL, tvb,
                                   offset, -1, ENC_NA);
	subtree = proto_item_add_subtree(item, ett_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL);

	proto_tree_add_item(subtree, hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_Version, tvb,
                            offset, 4, ENC_LITTLE_ENDIAN);
	offset+=4;

	proto_tree_add_bitmask(subtree, tvb, offset,
                               hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_Flags,
                               ett_ntlmssp, MSV1_0_CRED_FLAGS_bits, ENC_LITTLE_ENDIAN);
	offset+=4;

	proto_tree_add_item(subtree, hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_CredentialKey,
                            tvb, offset, MSV1_0_CREDENTIAL_KEY_LENGTH, ENC_NA);
	offset+=MSV1_0_CREDENTIAL_KEY_LENGTH;

	proto_tree_add_item(subtree, hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_CredentialKeyType,
                            tvb, offset, 4, ENC_LITTLE_ENDIAN);
	offset+=4;

	EncryptedCredsSize = tvb_get_letohl(tvb, offset);
	proto_tree_add_item(subtree, hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_EncryptedCredsSize,
                            tvb, offset, 4, ENC_LITTLE_ENDIAN);
	offset+=4;

	if (EncryptedCredsSize == 0)
		return offset;

	if (tvb_captured_length(tvb) < (36 + EncryptedCredsSize))
		return offset;

	proto_tree_add_item(subtree, hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_EncryptedCreds,
                            tvb, offset, EncryptedCredsSize, ENC_NA);
	offset+=EncryptedCredsSize;

	return offset;
}


void
proto_register_ntlmssp(void)
{

  static hf_register_info hf[] = {
    { &hf_ntlmssp_auth,
      { "NTLMSSP identifier", "ntlmssp.identifier",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_message_type,
      { "NTLM Message Type", "ntlmssp.messagetype",
        FT_UINT32, BASE_HEX, VALS(ntlmssp_message_types), 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags,
      { "Negotiate Flags", "ntlmssp.negotiateflags",
        FT_UINT32, BASE_HEX, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_01,
      { "Negotiate UNICODE", "ntlmssp.negotiateunicode",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_UNICODE,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_02,
      { "Negotiate OEM", "ntlmssp.negotiateoem",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_OEM,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_04,
      { "Request Target", "ntlmssp.requesttarget",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_REQUEST_TARGET,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_08,
      { "Request 0x00000008", "ntlmssp.unused00000008",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_00000008,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_10,
      { "Negotiate Sign", "ntlmssp.negotiatesign",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_SIGN,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_20,
      { "Negotiate Seal", "ntlmssp.negotiateseal",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_SEAL,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_40,
      { "Negotiate Datagram", "ntlmssp.negotiatedatagram",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_DATAGRAM,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_80,
      { "Negotiate Lan Manager Key", "ntlmssp.negotiatelmkey",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_LM_KEY,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_100,
      { "Negotiate 0x00000100", "ntlmssp.unused00000100",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_00000100,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_200,
      { "Negotiate NTLM key", "ntlmssp.negotiatentlm",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_NTLM,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_400,
      { "Negotiate 0x00000400", "ntlmssp.unused00000400",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_00000400,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_800,
      { "Negotiate Anonymous", "ntlmssp.negotiateanonymous",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_ANONYMOUS,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_1000,
      { "Negotiate OEM Domain Supplied", "ntlmssp.negotiateoemdomainsupplied",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_2000,
      { "Negotiate OEM Workstation Supplied", "ntlmssp.negotiateoemworkstationsupplied",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_4000,
      { "Negotiate 0x00004000", "ntlmssp.unused00004000",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_00004000,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_8000,
      { "Negotiate Always Sign", "ntlmssp.negotiatealwayssign",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_ALWAYS_SIGN,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_10000,
      { "Target Type Domain", "ntlmssp.targettypedomain",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_TARGET_TYPE_DOMAIN,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_20000,
      { "Target Type Server", "ntlmssp.targettypeserver",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_TARGET_TYPE_SERVER,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_40000,
      { "Negotiate 0x00040000", "ntlmssp.unused00040000",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_00040000,
        NULL, HFILL }
    },

/* Negotiate Flags */
    { &hf_ntlmssp_negotiate_flags_80000,
      { "Negotiate Extended Session Security", "ntlmssp.negotiateextendedsessionsecurity",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_100000,
      { "Negotiate Identify", "ntlmssp.negotiateidentify",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_IDENTIFY,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_200000,
      { "Negotiate 0x00200000", "ntlmssp.unused00200000",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_00200000,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_400000,
      { "Request Non-NT Session Key", "ntlmssp.requestnonntsessionkey",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_REQUEST_NON_NT_SESSION_KEY,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_800000,
      { "Negotiate Target Info", "ntlmssp.negotiatetargetinfo",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_TARGET_INFO,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_1000000,
      { "Negotiate 0x01000000", "ntlmssp.unused01000000",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_01000000,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_2000000,
      { "Negotiate Version", "ntlmssp.negotiateversion",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_VERSION,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_4000000,
      { "Negotiate 0x04000000", "ntlmssp.unused04000000",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_04000000,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_8000000,
      { "Negotiate 0x08000000", "ntlmssp.unused08000000",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_08000000,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_10000000,
      { "Negotiate 0x10000000", "ntlmssp.unused10000000",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_UNUSED_10000000,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_20000000,
      { "Negotiate 128", "ntlmssp.negotiate128",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_128,
        "128-bit encryption is supported", HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_40000000,
      { "Negotiate Key Exchange", "ntlmssp.negotiatekeyexch",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_KEY_EXCH,
        NULL, HFILL }
    },
    { &hf_ntlmssp_negotiate_flags_80000000,
      { "Negotiate 56", "ntlmssp.negotiate56",
        FT_BOOLEAN, 32, TFS (&tfs_set_notset), NTLMSSP_NEGOTIATE_56,
        "56-bit encryption is supported", HFILL }
    },
#if 0
    { &hf_ntlmssp_negotiate_workstation_strlen,
      { "Calling workstation name length", "ntlmssp.negotiate.callingworkstation.strlen",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL }
    },
#endif
#if 0
    { &hf_ntlmssp_negotiate_workstation_maxlen,
      { "Calling workstation name max length", "ntlmssp.negotiate.callingworkstation.maxlen",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL }
    },
#endif
#if 0
    { &hf_ntlmssp_negotiate_workstation_buffer,
      { "Calling workstation name buffer", "ntlmssp.negotiate.callingworkstation.buffer",
        FT_UINT32, BASE_HEX, NULL, 0x0,
        NULL, HFILL }
    },
#endif
    { &hf_ntlmssp_negotiate_workstation,
      { "Calling workstation name", "ntlmssp.negotiate.callingworkstation",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
#if 0
    { &hf_ntlmssp_negotiate_domain_strlen,
      { "Calling workstation domain length", "ntlmssp.negotiate.domain.strlen",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL }
    },
#endif
#if 0
    { &hf_ntlmssp_negotiate_domain_maxlen,
      { "Calling workstation domain max length", "ntlmssp.negotiate.domain.maxlen",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL }
    },
#endif
#if 0
    { &hf_ntlmssp_negotiate_domain_buffer,
      { "Calling workstation domain buffer", "ntlmssp.negotiate.domain.buffer",
        FT_UINT32, BASE_HEX, NULL, 0x0,
        NULL, HFILL }
    },
#endif
    { &hf_ntlmssp_negotiate_domain,
      { "Calling workstation domain", "ntlmssp.negotiate.domain",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlm_client_challenge,
      { "LMv2 Client Challenge", "ntlmssp.ntlmclientchallenge",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        "The 8-byte LMv2 challenge message generated by the client", HFILL }
    },
    { &hf_ntlmssp_ntlm_server_challenge,
      { "NTLM Server Challenge", "ntlmssp.ntlmserverchallenge",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_reserved,
      { "Reserved", "ntlmssp.reserved",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },

    { &hf_ntlmssp_challenge_target_name,
      { "Target Name", "ntlmssp.challenge.target_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_auth_domain,
      { "Domain name", "ntlmssp.auth.domain",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_auth_username,
      { "User name", "ntlmssp.auth.username",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_auth_hostname,
      { "Host name", "ntlmssp.auth.hostname",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_auth_lmresponse,
      { "Lan Manager Response", "ntlmssp.auth.lmresponse",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_auth_ntresponse,
      { "NTLM Response", "ntlmssp.auth.ntresponse",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_auth_sesskey,
      { "Session Key", "ntlmssp.auth.sesskey",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_string_len,
      { "Length", "ntlmssp.string.length",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_string_maxlen,
      { "Maxlen", "ntlmssp.string.maxlen",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_string_offset,
      { "Offset", "ntlmssp.string.offset",
        FT_UINT32, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_blob_len,
      { "Length", "ntlmssp.blob.length",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_blob_maxlen,
      { "Maxlen", "ntlmssp.blob.maxlen",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_blob_offset,
      { "Offset", "ntlmssp.blob.offset",
        FT_UINT32, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_version,
      { "Version", "ntlmssp.version",
        FT_NONE, BASE_NONE, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_version_major,
      { "Major Version", "ntlmssp.version.major",
        FT_UINT8, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_version_minor,
      { "Minor Version", "ntlmssp.version.minor",
        FT_UINT8, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_version_build_number,
      { "Build Number", "ntlmssp.version.build_number",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_version_ntlm_current_revision,
      { "NTLM Current Revision", "ntlmssp.version.ntlm_current_revision",
        FT_UINT8, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },

/* Target Info */
    { &hf_ntlmssp_challenge_target_info,
      { "Target Info", "ntlmssp.challenge.target_info",
        FT_NONE, BASE_NONE, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_challenge_target_info_len,
      { "Length", "ntlmssp.challenge.target_info.length",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_challenge_target_info_maxlen,
      { "Maxlen", "ntlmssp.challenge.target_info.maxlen",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },
    { &hf_ntlmssp_challenge_target_info_offset,
      { "Offset", "ntlmssp.challenge.target_info.offset",
        FT_UINT32, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },

    { &hf_ntlmssp_challenge_target_info_item_type,
      { "Target Info Item Type", "ntlmssp.challenge.target_info.item.type",
        FT_UINT16, BASE_HEX | BASE_EXT_STRING, &ntlm_name_types_ext, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_item_len,
      { "Target Info Item Length", "ntlmssp.challenge.target_info.item.length",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },

    { &hf_ntlmssp_challenge_target_info_end,
      { "List End", "ntlmssp.challenge.target_info.end",
        FT_NONE, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_nb_computer_name,
      { "NetBIOS Computer Name", "ntlmssp.challenge.target_info.nb_computer_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        "Server NetBIOS Computer Name", HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_nb_domain_name,
      { "NetBIOS Domain Name", "ntlmssp.challenge.target_info.nb_domain_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        "Server NetBIOS Domain Name", HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_dns_computer_name,
      { "DNS Computer Name", "ntlmssp.challenge.target_info.dns_computer_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_dns_domain_name,
      { "DNS Domain Name", "ntlmssp.challenge.target_info.dns_domain_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_dns_tree_name,
      { "DNS Tree Name", "ntlmssp.challenge.target_info.dns_tree_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_flags,
      { "Flags", "ntlmssp.challenge.target_info.flags",
        FT_UINT32, BASE_HEX, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_timestamp,
      { "Timestamp", "ntlmssp.challenge.target_info.timestamp",
        FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL, NULL, 0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_restrictions,
      { "Restrictions", "ntlmssp.challenge.target_info.restrictions",
        FT_BYTES, BASE_NONE, NULL, 0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_target_name,
      { "Target Name", "ntlmssp.challenge.target_info.target_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_challenge_target_info_channel_bindings,
      { "Channel Bindings", "ntlmssp.challenge.target_info.channel_bindings",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },

    { &hf_ntlmssp_ntlmv2_response_item_type,
      { "NTLMV2 Response Item Type", "ntlmssp.ntlmv2_response.item.type",
        FT_UINT16, BASE_HEX | BASE_EXT_STRING, &ntlm_name_types_ext, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_item_len,
      { "NTLMV2 Response Item Length", "ntlmssp.ntlmv2_response.item.length",
        FT_UINT16, BASE_DEC, NULL, 0x0,
        NULL, HFILL}
    },

    { &hf_ntlmssp_ntlmv2_response_end,
      { "List End", "ntlmssp.ntlmv2_response.end",
        FT_NONE, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_nb_computer_name,
      { "NetBIOS Computer Name", "ntlmssp.ntlmv2_response.nb_computer_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        "Server NetBIOS Computer Name", HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_nb_domain_name,
      { "NetBIOS Domain Name", "ntlmssp.ntlmv2_response.nb_domain_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        "Server NetBIOS Domain Name", HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_dns_computer_name,
      { "DNS Computer Name", "ntlmssp.ntlmv2_response.dns_computer_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_dns_domain_name,
      { "DNS Domain Name", "ntlmssp.ntlmv2_response.dns_domain_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_dns_tree_name,
      { "DNS Tree Name", "ntlmssp.ntlmv2_response.dns_tree_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_flags,
      { "Flags", "ntlmssp.ntlmv2_response.flags",
        FT_UINT32, BASE_HEX, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_timestamp,
      { "Timestamp", "ntlmssp.ntlmv2_response.timestamp",
        FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL, NULL, 0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_restrictions,
      { "Restrictions", "ntlmssp.ntlmv2_response.restrictions",
        FT_BYTES, BASE_NONE, NULL, 0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_target_name,
      { "Target Name", "ntlmssp.ntlmv2_response.target_name",
        FT_STRING, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_channel_bindings,
      { "Channel Bindings", "ntlmssp.ntlmv2_response.channel_bindings",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },

    { &hf_ntlmssp_message_integrity_code,
      { "MIC", "ntlmssp.authenticate.mic",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        "Message Integrity Code", HFILL}
    },
    { &hf_ntlmssp_verf,
      { "NTLMSSP Verifier", "ntlmssp.verf",
        FT_NONE, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_verf_vers,
      { "Version Number", "ntlmssp.verf.vers",
        FT_UINT32, BASE_DEC, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_verf_body,
      { "Verifier Body", "ntlmssp.verf.body",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
#if 0
    { &hf_ntlmssp_decrypted_payload,
      { "NTLM Decrypted Payload", "ntlmssp.decrypted_payload",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
#endif
    { &hf_ntlmssp_verf_randompad,
      { "Random Pad", "ntlmssp.verf.randompad",
        FT_UINT32, BASE_HEX, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_verf_crc32,
      { "Verifier CRC32", "ntlmssp.verf.crc32",
        FT_UINT32, BASE_HEX, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_verf_hmacmd5,
      { "HMAC MD5", "ntlmssp.verf.hmacmd5",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_verf_sequence,
      { "Sequence", "ntlmssp.verf.sequence",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },

    { &hf_ntlmssp_ntlmv2_response,
      { "NTLMv2 Response", "ntlmssp.ntlmv2_response",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_ntproofstr,
      { "NTProofStr", "ntlmssp.ntlmv2_response.ntproofstr",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        "The HMAC-MD5 of the challenge", HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_rversion,
      { "Response Version", "ntlmssp.ntlmv2_response.rversion",
        FT_UINT8, BASE_DEC, NULL, 0x0,
        "The 1-byte response version, currently set to 1", HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_hirversion,
      { "Hi Response Version", "ntlmssp.ntlmv2_response.hirversion",
        FT_UINT8, BASE_DEC, NULL, 0x0,
        "The 1-byte highest response version understood by the client, currently set to 1", HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_z,
      { "Z", "ntlmssp.ntlmv2_response.z",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        "byte array of zero bytes", HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_pad,
      { "padding", "ntlmssp.ntlmv2_response.pad",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        NULL, HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_time,
      { "Time", "ntlmssp.ntlmv2_response.time",
        FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC, NULL, 0,
        "The 8-byte little-endian time in UTC", HFILL }
    },
    { &hf_ntlmssp_ntlmv2_response_chal,
      { "NTLMv2 Client Challenge", "ntlmssp.ntlmv2_response.chal",
        FT_BYTES, BASE_NONE, NULL, 0x0,
        "The 8-byte NTLMv2 challenge message generated by the client", HFILL }
    },
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL,
      { "NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_Version,
      { "Version", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.Version",
        FT_UINT32, BASE_HEX, VALS(MSV1_0_CRED_VERSION), 0,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_Flags,
      { "Flags", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.Flags",
        FT_UINT32, BASE_HEX, NULL, 0,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_LM_PRESENT,
      { "lm_present", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.LM_PRESENT",
        FT_BOOLEAN, 32, NULL, MSV1_0_CRED_LM_PRESENT,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_NT_PRESENT,
      { "nt_present", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.NT_PRESENT",
        FT_BOOLEAN, 32, NULL, MSV1_0_CRED_NT_PRESENT,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_REMOVED,
      { "removed", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.REMOVED",
        FT_BOOLEAN, 32, NULL, MSV1_0_CRED_REMOVED,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_CREDKEY_PRESENT,
      { "credkey_present", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.CREDKEY_PRESENT",
        FT_BOOLEAN, 32, NULL, MSV1_0_CRED_CREDKEY_PRESENT,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_FLAG_SHA_PRESENT,
      { "sha_present", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.SHA_PRESENT",
        FT_BOOLEAN, 32, NULL, MSV1_0_CRED_SHA_PRESENT,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_CredentialKey,
      { "CredentialKey", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.CredentialKey",
        FT_BYTES, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_CredentialKeyType,
      { "CredentialKeyType", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.CredentialKeyType",
        FT_UINT32, BASE_DEC, VALS(MSV1_0_CREDENTIAL_KEY_TYPE), 0,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_EncryptedCredsSize,
      { "EncryptedCredsSize", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.EncryptedCredsSize",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL_EncryptedCreds,
      { "EncryptedCreds", "ntlmssp.NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL.EncryptedCreds",
        FT_BYTES, BASE_NONE, NULL, 0,
        NULL, HFILL }},
  };


  static int *ett[] = {
    &ett_ntlmssp,
    &ett_ntlmssp_negotiate_flags,
    &ett_ntlmssp_string,
    &ett_ntlmssp_blob,
    &ett_ntlmssp_version,
    &ett_ntlmssp_challenge_target_info,
    &ett_ntlmssp_challenge_target_info_item,
    &ett_ntlmssp_ntlmv2_response,
    &ett_ntlmssp_ntlmv2_response_item,
    &ett_ntlmssp_NTLM_REMOTE_SUPPLEMENTAL_CREDENTIAL,
  };
  static ei_register_info ei[] = {
     { &ei_ntlmssp_v2_key_too_long, { "ntlmssp.v2_key_too_long", PI_UNDECODED, PI_WARN, "NTLM v2 key is too long", EXPFILL }},
     { &ei_ntlmssp_blob_len_too_long, { "ntlmssp.blob.length.too_long", PI_UNDECODED, PI_WARN, "Session blob length too long", EXPFILL }},
     { &ei_ntlmssp_target_info_attr, { "ntlmssp.target_info_attr.unknown", PI_UNDECODED, PI_WARN, "Unknown NTLMSSP Target Info Attribute", EXPFILL }},
     { &ei_ntlmssp_target_info_invalid, { "ntlmssp.target_info_attr.invalid", PI_UNDECODED, PI_WARN, "Invalid NTLMSSP Target Info AvPairs", EXPFILL }},
     { &ei_ntlmssp_message_type, { "ntlmssp.messagetype.unknown", PI_PROTOCOL, PI_WARN, "Unrecognized NTLMSSP Message", EXPFILL }},
     { &ei_ntlmssp_auth_nthash, { "ntlmssp.authenticated", PI_SECURITY, PI_CHAT, "Authenticated NTHASH", EXPFILL }},
     { &ei_ntlmssp_sessionbasekey, { "ntlmssp.sessionbasekey", PI_SECURITY, PI_CHAT, "SessionBaseKey", EXPFILL }},
     { &ei_ntlmssp_sessionkey, { "ntlmssp.sessionkey", PI_SECURITY, PI_CHAT, "SessionKey", EXPFILL }},
  };
  module_t *ntlmssp_module;
  expert_module_t* expert_ntlmssp;

  proto_ntlmssp = proto_register_protocol (
    "NTLM Secure Service Provider", /* name */
    "NTLMSSP",  /* short name */
    "ntlmssp"   /* abbrev */
    );
  proto_register_field_array (proto_ntlmssp, hf, array_length (hf));
  proto_register_subtree_array (ett, array_length (ett));
  expert_ntlmssp = expert_register_protocol(proto_ntlmssp);
  expert_register_field_array(expert_ntlmssp, ei, array_length(ei));
  register_init_routine(&ntlmssp_init_protocol);
  register_cleanup_routine(&ntlmssp_cleanup_protocol);

  ntlmssp_module = prefs_register_protocol(proto_ntlmssp, NULL);

  prefs_register_string_preference(ntlmssp_module, "nt_password",
                                   "NT Password",
                                   "Cleartext NT Password (used to decrypt payloads, supports only ASCII passwords)",
                                   &ntlmssp_option_nt_password);

  ntlmssp_handle = register_dissector("ntlmssp", dissect_ntlmssp, proto_ntlmssp);
  ntlmssp_wrap_handle = register_dissector("ntlmssp_payload", dissect_ntlmssp_payload, proto_ntlmssp);
  register_dissector("ntlmssp_data_only", dissect_ntlmssp_payload_only, proto_ntlmssp);
  register_dissector("ntlmssp_verf", dissect_ntlmssp_verf, proto_ntlmssp);

  ntlmssp_tap = register_tap("ntlmssp");
}

void
proto_reg_handoff_ntlmssp(void)
{
  /* Register protocol with the GSS-API module */

  gssapi_init_oid("1.3.6.1.4.1.311.2.2.10", proto_ntlmssp, ett_ntlmssp,
                  ntlmssp_handle, ntlmssp_wrap_handle,
                  "NTLMSSP - Microsoft NTLM Security Support Provider");

  /* Register authenticated pipe dissector */

  /*
   * XXX - the verifiers here seem to have a version of 1 and a body of all
   * zeroes.
   *
   * XXX - DCE_C_AUTHN_LEVEL_CONNECT is, according to the DCE RPC 1.1
   * spec, upgraded to DCE_C_AUTHN_LEVEL_PKT.  Should we register
   * any other levels here?
   */
  register_dcerpc_auth_subdissector(DCE_C_AUTHN_LEVEL_CONNECT,
                                    DCE_C_RPC_AUTHN_PROTOCOL_NTLMSSP,
                                    &ntlmssp_sign_fns);

  register_dcerpc_auth_subdissector(DCE_C_AUTHN_LEVEL_PKT,
                                    DCE_C_RPC_AUTHN_PROTOCOL_NTLMSSP,
                                    &ntlmssp_sign_fns);

  register_dcerpc_auth_subdissector(DCE_C_AUTHN_LEVEL_PKT_INTEGRITY,
                                    DCE_C_RPC_AUTHN_PROTOCOL_NTLMSSP,
                                    &ntlmssp_sign_fns);

  register_dcerpc_auth_subdissector(DCE_C_AUTHN_LEVEL_PKT_PRIVACY,
                                    DCE_C_RPC_AUTHN_PROTOCOL_NTLMSSP,
                                    &ntlmssp_seal_fns);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 2
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=2 tabstop=8 expandtab:
 * :indentSize=2:tabSize=8:noTabs=true:
 */
