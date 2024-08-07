/* packet-dcerpc-llb.c
 *
 * Routines for llb dissection
 * Copyright 2004, Jaime Fournier <jaime.fournier@hush.com>
 * This information is based off the released idl files from opengroup.
 * ftp://ftp.opengroup.org/pub/dce122/dce/src/admin.tar.gz ./admin/dced/idl/llb.idl
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

void proto_register_llb (void);
void proto_reg_handoff_llb (void);

static int proto_llb;
static int hf_llb_opnum;

static int ett_llb;


static e_guid_t uuid_llb =
  { 0x333b33c3, 0x0000, 0x0000, {0x0d, 0x00, 0x00, 0x87, 0x84, 0x00, 0x00,
                                 0x00} };
static uint16_t ver_llb = 4;


static const dcerpc_sub_dissector llb_dissectors[] = {
  {0, "insert", NULL, NULL},
  {1, "delete", NULL, NULL},
  {2, "lookup", NULL, NULL},
  {0, NULL, NULL, NULL}
};

void
proto_register_llb (void)
{
  static hf_register_info hf[] = {
    {&hf_llb_opnum,
     {"Operation", "llb.opnum", FT_UINT16, BASE_DEC, NULL, 0x0, NULL,
      HFILL}},
  };

  static int *ett[] = {
    &ett_llb,
  };
  proto_llb =
    proto_register_protocol ("DCE/RPC NCS 1.5.1 Local Location Broker", "llb",
                             "llb");
  proto_register_field_array (proto_llb, hf, array_length (hf));
  proto_register_subtree_array (ett, array_length (ett));
}

void
proto_reg_handoff_llb (void)
{
  /* Register the protocol as dcerpc */
  dcerpc_init_uuid (proto_llb, ett_llb, &uuid_llb, ver_llb, llb_dissectors,
                    hf_llb_opnum);
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
