/* packet-dcerpc-rs_prop_pgo.c
 *
 * Routines for rs_prop_pgo dissection
 * Copyright 2004, Jaime Fournier <jaime.fournier@hush.com>
 * This information is based off the released idl files from opengroup.
 * ftp://ftp.opengroup.org/pub/dce122/dce/src/security.tar.gz security/idl/rs_prop_pgo.idl
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"


#include <epan/packet.h>
#include "packet-dcerpc.h"

void proto_register_rs_prop_pgo (void);
void proto_reg_handoff_rs_prop_pgo (void);

static int proto_rs_prop_pgo;
static int hf_rs_prop_pgo_opnum;


static int ett_rs_prop_pgo;
static e_guid_t uuid_rs_prop_pgo =
  { 0xc23626e8, 0xde34, 0x11ca, {0x8c, 0xbc, 0x08, 0x00, 0x1e, 0x03, 0x94,
                                 0xc7} };

static uint16_t ver_rs_prop_pgo = 1;


static const dcerpc_sub_dissector rs_prop_pgo_dissectors[] = {
  {0, "add",               NULL, NULL},
  {1, "rename",            NULL, NULL},
  {2, "replace",           NULL, NULL},
  {3, "add_member",        NULL, NULL},
  {4, "delete_member",     NULL, NULL},
  {5, "add_member_global", NULL, NULL},
  {0, NULL, NULL, NULL}
};

void
proto_register_rs_prop_pgo (void)
{
  static hf_register_info hf[] = {
    {&hf_rs_prop_pgo_opnum,
     {"Operation", "rs_prop_pgo.opnum", FT_UINT16, BASE_DEC, NULL, 0x0,
      NULL, HFILL}},
  };

  static int *ett[] = {
    &ett_rs_prop_pgo,
  };
  proto_rs_prop_pgo =
    proto_register_protocol
    ("DCE/RPC Registry server propagation interface - PGO items",
     "rs_prop_pgo", "rs_prop_pgo");
  proto_register_field_array (proto_rs_prop_pgo, hf, array_length (hf));
  proto_register_subtree_array (ett, array_length (ett));
}

void
proto_reg_handoff_rs_prop_pgo (void)
{
  /* Register the protocol as dcerpc */
  dcerpc_init_uuid (proto_rs_prop_pgo, ett_rs_prop_pgo, &uuid_rs_prop_pgo,
                    ver_rs_prop_pgo, rs_prop_pgo_dissectors,
                    hf_rs_prop_pgo_opnum);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local Variables:
 * c-basic-offset: 2
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=2 tabstop=8 expandtab:
 * :indentSize=2:tabSize=8:noTabs=true:
 */
