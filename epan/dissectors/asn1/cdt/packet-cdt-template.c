/* packet-cdt.c
 *
 * Routines for Compressed Data Type packet dissection.
 *
 * Copyright 2005, Stig Bjorlykke <stig@bjorlykke.org>, Thales Norway AS
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Ref: STANAG 4406 Annex E
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/oids.h>
#include <epan/expert.h>
#include <epan/asn1.h>
#include <wsutil/array.h>
#include "packet-ber.h"
#include "packet-p1.h"

#include "packet-cdt.h"

#define CDT_UNDEFINED  0
#define CDT_EXTERNAL   1
#define CDT_P1         2
#define CDT_P3         3
#define CDT_P7         4

#define PNAME  "Compressed Data Type"
#define PSNAME "CDT"
#define PFNAME "cdt"

void proto_register_cdt(void);
void proto_reg_handoff_cdt(void);

static proto_tree *top_tree;
static proto_item *cdt_item;

static uint32_t content_type;

/* Initialize the protocol and registered fields */
static int proto_cdt;
#include "packet-cdt-hf.c"

/* Initialize the subtree pointers */
#include "packet-cdt-ett.c"

static expert_field ei_cdt_unable_compress_content;
static expert_field ei_cdt_unable_uncompress_content;

#include "packet-cdt-fn.c"


/*--- proto_register_cdt -------------------------------------------*/

/*
** Dissect Compressed Data Type
*/
void dissect_cdt (tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree)
{
  proto_tree *tree = NULL;

  /* save parent_tree so subdissectors can create new top nodes */
  top_tree = parent_tree;

  if (parent_tree) {
    cdt_item = proto_tree_add_item (parent_tree, proto_cdt, tvb, 0, -1, ENC_NA);
    tree = proto_item_add_subtree (cdt_item, ett_cdt_CompressedData);
  } else {
    cdt_item = NULL;
  }

  col_set_str (pinfo->cinfo, COL_PROTOCOL, "CDT");
  col_clear (pinfo->cinfo, COL_INFO);

  dissect_CompressedData_PDU (tvb, pinfo, tree, NULL);
}

void proto_register_cdt (void) {

  /* List of fields */
  static hf_register_info hf[] = {
#include "packet-cdt-hfarr.c"
  };

  /* List of subtrees */
  static int *ett[] = {
#include "packet-cdt-ettarr.c"
  };

  static ei_register_info ei[] = {
     { &ei_cdt_unable_compress_content, { "cdt.unable_compress_content", PI_UNDECODED, PI_ERROR, "Unable to get compressed content", EXPFILL }},
     { &ei_cdt_unable_uncompress_content, { "cdt.unable_uncompress_content", PI_UNDECODED, PI_ERROR, "Unable to get uncompressed content", EXPFILL }},
  };

  expert_module_t* expert_cdt;

  /* Register protocol */
  proto_cdt = proto_register_protocol (PNAME, PSNAME, PFNAME);

  /* Register fields and subtrees */
  proto_register_field_array (proto_cdt, hf, array_length(hf));
  proto_register_subtree_array (ett, array_length(ett));
  expert_cdt = expert_register_protocol(proto_cdt);
  expert_register_field_array(expert_cdt, ei, array_length(ei));
}


/*--- proto_reg_handoff_cdt ---------------------------------------*/
void proto_reg_handoff_cdt (void) {
#include "packet-cdt-dis-tab.c"
}
