/* packet-vxlan.c
 *
 * Routines for Virtual eXtensible Local Area Network (VXLAN) packet dissection
 * RFC 7348 plus draft-smith-vxlan-group-policy-01
 *
 * (c) Copyright 2016, Sumit Kumar Jha <sjha3@ncsu.edu>
 * Support for VXLAN GPE (https://datatracker.ietf.org/doc/html/draft-ietf-nvo3-vxlan-gpe-02)
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/tfs.h>
#include "packet-vxlan.h"

#define UDP_PORT_VXLAN  "4789,8472" /* The IANA assigned port is 4789, but Linux default is 8472 for compatibility with early adopters */
#define UDP_PORT_VXLAN_GPE  4790

void proto_register_vxlan(void);
void proto_reg_handoff_vxlan(void);

static dissector_handle_t vxlan_handle;
static dissector_handle_t vxlan_gpe_handle;

static int proto_vxlan;
static int proto_vxlan_gpe;

static int hf_vxlan_flags;
static int hf_vxlan_gpe_flags;
static int hf_vxlan_flags_reserved;
static int hf_vxlan_reserved_8;
static int hf_vxlan_flag_a;
static int hf_vxlan_flag_d;
static int hf_vxlan_flag_i;
static int hf_vxlan_flag_g;
static int hf_vxlan_gbp;
static int hf_vxlan_vni;
static int hf_vxlan_gpe_flag_i;
static int hf_vxlan_gpe_flag_p;
static int hf_vxlan_gpe_flag_o;
static int hf_vxlan_gpe_flag_ver;
static int hf_vxlan_gpe_flag_reserved;
static int hf_vxlan_gpe_reserved_16;
static int hf_vxlan_next_proto;
static int ett_vxlan;
static int ett_vxlan_flags;

static int * const flags_fields[] = {
        &hf_vxlan_flag_g,
        &hf_vxlan_flag_i,
        &hf_vxlan_flag_d,
        &hf_vxlan_flag_a,
        &hf_vxlan_flags_reserved,
        NULL
    };

static int * const gpe_flags_fields[] = {
        &hf_vxlan_gpe_flag_ver,
        &hf_vxlan_gpe_flag_i,
        &hf_vxlan_gpe_flag_p,
        &hf_vxlan_gpe_flag_o,
        &hf_vxlan_gpe_flag_reserved,
        NULL

   };

static const value_string vxlan_next_protocols[] = {
        { VXLAN_IPV4, "IPv4" },
        { VXLAN_IPV6, "IPv6" },
        { VXLAN_ETHERNET, "Ethernet" },
        { VXLAN_NSH, "Network Service Header" },
        { VXLAN_MPLS, "MPLS"},
        { 0, NULL }
 };

static dissector_handle_t eth_handle;
static dissector_table_t vxlan_dissector_table;

static int
dissect_vxlan_common(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int is_gpe)
{
    proto_tree *vxlan_tree;
    proto_item *ti;
    tvbuff_t *next_tvb;
    int offset = 0;
    uint32_t vxlan_next_proto;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "VxLAN");
    col_clear(pinfo->cinfo, COL_INFO);


    ti = proto_tree_add_item(tree, proto_vxlan, tvb, offset, 8, ENC_NA);
    vxlan_tree = proto_item_add_subtree(ti, ett_vxlan);

    if(is_gpe) {
        proto_tree_add_bitmask(vxlan_tree, tvb, offset, hf_vxlan_gpe_flags, ett_vxlan_flags, gpe_flags_fields, ENC_BIG_ENDIAN);
        offset += 1;

        proto_tree_add_item(vxlan_tree, hf_vxlan_gpe_reserved_16, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;

        proto_tree_add_item_ret_uint(vxlan_tree, hf_vxlan_next_proto, tvb, offset, 1, ENC_BIG_ENDIAN, &vxlan_next_proto);
        offset += 1;
    } else {
        proto_tree_add_bitmask(vxlan_tree, tvb, offset, hf_vxlan_flags, ett_vxlan_flags, flags_fields, ENC_BIG_ENDIAN);
        offset += 2;

        proto_tree_add_item(vxlan_tree, hf_vxlan_gbp, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;
    }

    proto_tree_add_item(vxlan_tree, hf_vxlan_vni, tvb, offset, 3, ENC_BIG_ENDIAN);
    offset += 3;

    proto_tree_add_item(vxlan_tree, hf_vxlan_reserved_8, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    next_tvb = tvb_new_subset_remaining(tvb, offset);

    if(is_gpe){
        if(!dissector_try_uint(vxlan_dissector_table, vxlan_next_proto, next_tvb, pinfo, tree)) {
            call_data_dissector(next_tvb, pinfo, tree);
        }
    } else {
        call_dissector(eth_handle, next_tvb, pinfo, tree);
    }

    return tvb_captured_length(tvb);
}

static int
dissect_vxlan_gpe(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    return dissect_vxlan_common(tvb, pinfo, tree, true);
}


static int
dissect_vxlan(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    return dissect_vxlan_common(tvb, pinfo, tree, false);
}




/* Register VxLAN with Wireshark */
void
proto_register_vxlan(void)
{
    static hf_register_info hf[] = {
        { &hf_vxlan_flags,
          { "Flags", "vxlan.flags",
            FT_UINT16, BASE_HEX, NULL, 0x00,
            NULL, HFILL
          },
        },
        { &hf_vxlan_gpe_flags,
          { "Flags", "vxlan.flags",
            FT_UINT8, BASE_HEX, NULL, 0x00,
            NULL, HFILL
          },
        },
        { &hf_vxlan_flags_reserved,
          { "Reserved(R)", "vxlan.flags_reserved",
            FT_UINT16, BASE_HEX, NULL, 0x77b7,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_gpe_flag_reserved,
          { "Reserved(R)", "vxlan.flags_reserved",
            FT_UINT8, BASE_DEC, NULL, 0xC2,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_flag_g,
          { "GBP Extension", "vxlan.flag_g",
            FT_BOOLEAN, 16, TFS(&tfs_defined_not_defined), 0x8000,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_flag_i,
          { "VXLAN Network ID (VNI)", "vxlan.flag_i",
            FT_BOOLEAN, 16, NULL, 0x0800,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_flag_d,
          { "Don't Learn", "vxlan.flag_d",
            FT_BOOLEAN, 16, NULL, 0x0040,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_flag_a,
          { "Policy Applied", "vxlan.flag_a",
            FT_BOOLEAN, 16, NULL, 0x0008,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_gpe_flag_ver,
          { "Version", "vxlan.ver",
            FT_UINT8, BASE_DEC, NULL, 0x30,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_gpe_flag_i,
          { "Instance", "vxlan.i_bit",
            FT_UINT8, BASE_DEC, NULL, 0x08,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_gpe_flag_p,
          { "Next Protocol Bit", "vxlan.p_bit",
            FT_UINT8, BASE_DEC, NULL, 0x04,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_gpe_flag_o,
          { "OAM bit", "vxlan.o_bit",
            FT_UINT8, BASE_DEC, NULL, 0x01,
            NULL, HFILL,
          },
        },
        { &hf_vxlan_gbp,
          { "Group Policy ID", "vxlan.gbp",
            FT_UINT16, BASE_DEC, NULL, 0x0,
            NULL, HFILL
          },
        },
        { &hf_vxlan_vni,
          { "VXLAN Network Identifier (VNI)", "vxlan.vni",
            FT_UINT24, BASE_DEC, NULL, 0x0,
            NULL, HFILL
          },
        },
        { &hf_vxlan_reserved_8,
          { "Reserved", "vxlan.reserved8",
            FT_UINT8, BASE_DEC, NULL, 0x0,
            NULL, HFILL
          },
        },
        { &hf_vxlan_gpe_reserved_16,
          { "Reserved", "vxlan.reserved_16",
            FT_UINT16, BASE_DEC, NULL, 0x0,
            NULL, HFILL
          },
        },
        { &hf_vxlan_next_proto,
          { "Next Protocol", "vxlan.next_proto",
            FT_UINT8, BASE_DEC, VALS(vxlan_next_protocols), 0x0,
            NULL, HFILL
          },
        },
    };

    /* Setup protocol subtree array */
    static int *ett[] = {
        &ett_vxlan,
        &ett_vxlan_flags,
    };

    /* Register the protocol name and description */
    proto_vxlan = proto_register_protocol("Virtual eXtensible Local Area Network", "VXLAN", "vxlan");

    /* Protocol registered just for Decode As */
    proto_vxlan_gpe = proto_register_protocol_in_name_only("Virtual eXtensible Local Area Network (GPE)", "VXLAN (GPE)", "vxlan_gpe", proto_vxlan, FT_PROTOCOL);

    /* Required function calls to register the header fields and subtrees used */
    proto_register_field_array(proto_vxlan, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
    vxlan_dissector_table = register_dissector_table("vxlan.next_proto", "VXLAN Next Protocol", proto_vxlan, FT_UINT8, BASE_DEC);

    /* Register dissector handles */
    vxlan_handle = register_dissector("vxlan", dissect_vxlan, proto_vxlan);
    vxlan_gpe_handle = register_dissector("vxlan_gpe", dissect_vxlan_gpe, proto_vxlan_gpe);
}

void
proto_reg_handoff_vxlan(void)
{
    /*
     * RFC 7348 Figures 1 and 2, in the Payload section, say
     *
     * "(Note that the original Ethernet Frame's FCS is not included)"
     *
     * meaning that the inner Ethernet frame does *not* include an
     * FCS.
     */
    eth_handle = find_dissector_add_dependency("eth_withoutfcs", proto_vxlan);

    dissector_add_uint_range_with_preference("udp.port", UDP_PORT_VXLAN, vxlan_handle);
    dissector_add_uint_with_preference("udp.port", UDP_PORT_VXLAN_GPE, vxlan_gpe_handle);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
