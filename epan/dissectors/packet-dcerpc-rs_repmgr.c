/* packet-dcerpc-rs_repmgr.c
 *
 * Routines for rs_repmgr dissection
 * Copyright 2004, Jaime Fournier <jaime.fournier@hush.com>
 * This information is based off the released idl files from opengroup.
 * ftp://ftp.opengroup.org/pub/dce122/dce/src/security.tar.gz security/idl/rs_repmgr.idl
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

void proto_register_rs_repmgr (void);
void proto_reg_handoff_rs_repmgr (void);

static int proto_rs_repmgr;
static int hf_rs_repmgr_opnum;


static int ett_rs_repmgr;
static e_guid_t uuid_rs_repmgr =
  { 0xb62dc198, 0xdfd4, 0x11ca, {0x94, 0x8f, 0x08, 0x00, 0x1e, 0x02, 0x59,
                                 0x4c} };

static uint16_t ver_rs_repmgr = 2;


static const dcerpc_sub_dissector rs_repmgr_dissectors[] = {
  {0, "get_info_and_creds",   NULL, NULL},
  {1, "init",                 NULL, NULL},
  {2, "init_done",            NULL, NULL},
  {3, "i_am_slave",           NULL, NULL},
  {4, "i_am_master",          NULL, NULL},
  {5, "become_master",        NULL, NULL},
  {6, "copy_all",             NULL, NULL},
  {7, "copy_propq",           NULL, NULL},
  {8, "stop_until_compat_sw", NULL, NULL},
  {0, NULL, NULL, NULL}
};

void
proto_register_rs_repmgr (void)
{
  static hf_register_info hf[] = {
    {&hf_rs_repmgr_opnum,
     {"Operation", "rs_repmgr.opnum", FT_UINT16, BASE_DEC, NULL, 0x0,
      NULL, HFILL}},
  };

  static int *ett[] = {
    &ett_rs_repmgr,
  };
  proto_rs_repmgr =
    proto_register_protocol
    ("DCE/RPC Operations between registry server replicas", "rs_repmgr",
     "rs_repmgr");
  proto_register_field_array (proto_rs_repmgr, hf, array_length (hf));
  proto_register_subtree_array (ett, array_length (ett));
}

void
proto_reg_handoff_rs_repmgr (void)
{
  /* Register the protocol as dcerpc */
  dcerpc_init_uuid (proto_rs_repmgr, ett_rs_repmgr, &uuid_rs_repmgr,
                    ver_rs_repmgr, rs_repmgr_dissectors, hf_rs_repmgr_opnum);
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
