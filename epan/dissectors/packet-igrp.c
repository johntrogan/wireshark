/* packet-igrp.c
 * Routines for IGRP dissection
 * Copyright 2000, Paul Ionescu <paul@acorp.ro>
 *
 * See
 *
 *   http://www.cisco.com/en/US/tech/tk365/technologies_white_paper09186a00800c8ae1.shtml
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * Copied from packet-syslog.c
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/expert.h>
#include <epan/to_str.h>
#include <epan/ipproto.h>
#include <epan/tfs.h>
#include <epan/unit_strings.h>

void proto_register_igrp(void);
void proto_reg_handoff_igrp(void);

#define IGRP_HEADER_LENGTH 12
#define IGRP_ENTRY_LENGTH 14

static dissector_handle_t igrp_handle;

static int proto_igrp;
static int hf_igrp_update;
static int hf_igrp_as;
/* Generated from convert_proto_tree_add_text.pl */
static int hf_igrp_load;
static int hf_igrp_bandwidth;
static int hf_igrp_command;
static int hf_igrp_reliability;
static int hf_igrp_network;
static int hf_igrp_version;
static int hf_igrp_interior_routes;
static int hf_igrp_mtu;
static int hf_igrp_hop_count;
static int hf_igrp_exterior_routes;
static int hf_igrp_delay;
static int hf_igrp_checksum;
static int hf_igrp_system_routes;
static int ett_igrp;
static int ett_igrp_vektor;
static int ett_igrp_net;

static expert_field ei_igrp_version;

static void dissect_vektor_igrp (packet_info *pinfo, tvbuff_t *tvb, proto_tree *igrp_vektor_tree, uint8_t network);

static int dissect_igrp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
  uint8_t ver_and_opcode,version,opcode,network;
  int offset=IGRP_HEADER_LENGTH;
  uint16_t ninterior,nsystem,nexterior;
  const uint8_t *ipsrc;
  proto_item *ti;
  proto_tree *igrp_tree, *igrp_vektor_tree;
  tvbuff_t   *next_tvb;

  col_set_str(pinfo->cinfo, COL_PROTOCOL, "IGRP");
  col_clear(pinfo->cinfo, COL_INFO);

  ver_and_opcode = tvb_get_uint8(tvb,0);

  switch (ver_and_opcode) {
  case 0x11:
    col_set_str(pinfo->cinfo, COL_INFO, "Response" );
    break;
  case 0x12:
    col_set_str(pinfo->cinfo, COL_INFO, "Request" );
    break;
  default:
    col_set_str(pinfo->cinfo, COL_INFO, "Unknown version or opcode");
  }


  if (tree) {
    ti = proto_tree_add_protocol_format(tree, proto_igrp, tvb, 0, -1,
                                        "Cisco IGRP");

    igrp_tree = proto_item_add_subtree(ti, ett_igrp);

    version = (ver_and_opcode&0xf0)>>4 ; /* version is the fist half of the byte */
    opcode = ver_and_opcode&0x0f ;       /* opcode is the last half of the byte */

    ti = proto_tree_add_item(igrp_tree, hf_igrp_version, tvb, 0, 1, ENC_BIG_ENDIAN);
    if (version != 1)
        expert_add_info(pinfo, ti, &ei_igrp_version);
    ti = proto_tree_add_item(igrp_tree, hf_igrp_command, tvb, 0, 1, ENC_BIG_ENDIAN);
    if (opcode==1)
        proto_item_append_text(ti, " (Response)");
    else
        proto_item_append_text(ti, " (Request)");
    proto_tree_add_item(igrp_tree, hf_igrp_update, tvb, 1,1, ENC_BIG_ENDIAN);
    proto_tree_add_item(igrp_tree, hf_igrp_as, tvb, 2,2, ENC_BIG_ENDIAN);

    ninterior = tvb_get_ntohs(tvb,4);
    nsystem = tvb_get_ntohs(tvb,6);
    nexterior = tvb_get_ntohs(tvb,8);

    /* this is a ugly hack to find the first byte of the IP source address */
    if (pinfo->net_src.type == AT_IPv4) {
      ipsrc = (const uint8_t *)pinfo->net_src.data;
      network = ipsrc[0];
    } else
      network = 0; /* XXX - shouldn't happen */

    ti = proto_tree_add_item(igrp_tree, hf_igrp_interior_routes, tvb, 4, 2, ENC_BIG_ENDIAN);
    for( ; ninterior>0 ; ninterior-- ) {
      igrp_vektor_tree =  proto_item_add_subtree(ti,ett_igrp_vektor);
      next_tvb = tvb_new_subset_length_caplen(tvb, offset, IGRP_ENTRY_LENGTH, -1);
      dissect_vektor_igrp (pinfo,next_tvb,igrp_vektor_tree,network);
      offset+=IGRP_ENTRY_LENGTH;
    }

    ti = proto_tree_add_item(igrp_tree, hf_igrp_system_routes, tvb, 6, 2, ENC_BIG_ENDIAN);
    for( ; nsystem>0 ; nsystem-- ) {
      igrp_vektor_tree =  proto_item_add_subtree(ti,ett_igrp_vektor);
      next_tvb = tvb_new_subset_length_caplen(tvb, offset, IGRP_ENTRY_LENGTH, -1);
      dissect_vektor_igrp (pinfo,next_tvb,igrp_vektor_tree,0);
      offset+=IGRP_ENTRY_LENGTH;
    }

    ti = proto_tree_add_item(igrp_tree, hf_igrp_exterior_routes, tvb, 8, 2, ENC_BIG_ENDIAN);
    for( ; nexterior>0 ; nexterior-- ) {
      igrp_vektor_tree =  proto_item_add_subtree(ti,ett_igrp_vektor);
      next_tvb = tvb_new_subset_length_caplen(tvb, offset, IGRP_ENTRY_LENGTH, -1);
      dissect_vektor_igrp (pinfo,next_tvb,igrp_vektor_tree,0);
      offset+=IGRP_ENTRY_LENGTH;
    }

    proto_tree_add_checksum(igrp_tree, tvb, 10, hf_igrp_checksum, -1, NULL, pinfo, 0, ENC_BIG_ENDIAN, PROTO_CHECKSUM_NO_FLAGS);
  }
  return tvb_captured_length(tvb);
}

static void dissect_vektor_igrp (packet_info *pinfo, tvbuff_t *tvb, proto_tree *igrp_vektor_tree, uint8_t network)
{
  union {
    uint8_t addr_bytes[4];
    uint32_t addr_word;
  } addr;
  address ip_addr;

  if (network != 0) {
    /*
     * Interior route; network is the high-order byte, and the three
     * bytes in the vector are the lower 3 bytes.
     */
    addr.addr_bytes[0]=network;
    addr.addr_bytes[1]=tvb_get_uint8(tvb,0);
    addr.addr_bytes[2]=tvb_get_uint8(tvb,1);
    addr.addr_bytes[3]=tvb_get_uint8(tvb,2);
  } else {
    /*
     * System or exterior route; the three bytes in the vector are
     * the three high-order bytes, and the low-order byte is 0.
     */
    addr.addr_bytes[0]=tvb_get_uint8(tvb,0);
    addr.addr_bytes[1]=tvb_get_uint8(tvb,1);
    addr.addr_bytes[2]=tvb_get_uint8(tvb,2);
    addr.addr_bytes[3]=0;
  }

  set_address(&ip_addr, AT_IPv4, 4, &addr);
  igrp_vektor_tree = proto_tree_add_subtree_format(igrp_vektor_tree, tvb, 0 ,14,
                                                   ett_igrp_net, NULL, "Entry for network %s", address_to_str(pinfo->pool, &ip_addr));
  proto_tree_add_ipv4(igrp_vektor_tree, hf_igrp_network, tvb, 0, 3, addr.addr_word);
  proto_tree_add_item(igrp_vektor_tree, hf_igrp_delay, tvb, 3, 3, ENC_BIG_ENDIAN);
  proto_tree_add_item(igrp_vektor_tree, hf_igrp_bandwidth, tvb, 6, 3, ENC_BIG_ENDIAN);
  proto_tree_add_item(igrp_vektor_tree, hf_igrp_mtu, tvb, 9, 2, ENC_BIG_ENDIAN);
  proto_tree_add_item(igrp_vektor_tree, hf_igrp_reliability, tvb, 11, 1, ENC_BIG_ENDIAN);
  proto_tree_add_item(igrp_vektor_tree, hf_igrp_load, tvb, 12, 1, ENC_BIG_ENDIAN);
  proto_tree_add_item(igrp_vektor_tree, hf_igrp_hop_count, tvb, 13, 1, ENC_BIG_ENDIAN);
}


/* Register the protocol with Wireshark */
void proto_register_igrp(void)
{

  /* Setup list of header fields */
  static hf_register_info hf[] = {

    { &hf_igrp_update,
      { "Update Release",           "igrp.update",
      FT_UINT8, BASE_DEC, NULL, 0x0 ,
      "Update Release number", HFILL }
    },
    { &hf_igrp_as,
      { "Autonomous System",           "igrp.as",
      FT_UINT16, BASE_DEC, NULL, 0x0 ,
      "Autonomous System number", HFILL }
    },

    /* Generated from convert_proto_tree_add_text.pl */
    { &hf_igrp_version, { "IGRP Version", "igrp.version", FT_UINT8, BASE_DEC, NULL, 0xF0, NULL, HFILL }},
    { &hf_igrp_command, { "Command", "igrp.command", FT_UINT8, BASE_DEC, NULL, 0x0F, NULL, HFILL }},
    { &hf_igrp_interior_routes, { "Interior routes", "igrp.interior_routes", FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_system_routes, { "System routes", "igrp.system_routes", FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_exterior_routes, { "Exterior routes", "igrp.exterior_routes", FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_checksum, { "Checksum", "igrp.checksum", FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_network, { "Network", "igrp.network", FT_IPv4, BASE_NONE, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_delay, { "Delay", "igrp.delay", FT_UINT24, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_bandwidth, { "Bandwidth", "igrp.bandwidth", FT_UINT24, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_mtu, { "MTU", "igrp.mtu", FT_UINT16, BASE_DEC|BASE_UNIT_STRING, UNS(&units_byte_bytes), 0x0, NULL, HFILL }},
    { &hf_igrp_reliability, { "Reliability", "igrp.reliability", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_load, { "Load", "igrp.load", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_igrp_hop_count, { "Hop count", "igrp.hop_count", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
  };

  /* Setup protocol subtree array */
  static int *ett[] = {
    &ett_igrp,
    &ett_igrp_vektor,
    &ett_igrp_net
  };

  static ei_register_info ei[] = {
    { &ei_igrp_version, { "igrp.version.invalid", PI_PROTOCOL, PI_WARN, "Unknown Version, The dissection may be inaccurate", EXPFILL }},
  };

  expert_module_t* expert_igrp;

  /* Register the protocol name and description */
  proto_igrp = proto_register_protocol("Cisco Interior Gateway Routing Protocol",
                                       "IGRP", "igrp");
  igrp_handle = register_dissector("igrp", dissect_igrp, proto_igrp);

  /* Required function calls to register the header fields and subtrees used */
  proto_register_field_array(proto_igrp, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));
  expert_igrp = expert_register_protocol(proto_igrp);
  expert_register_field_array(expert_igrp, ei, array_length(ei));
}

void
proto_reg_handoff_igrp(void)
{
  dissector_add_uint("ip.proto", IP_PROTO_IGRP, igrp_handle);
}

/*    IGRP Packet structure:

HEADER structure + k * VECTOR structure
where: k = (Number of Interior routes) + (Number of System routes) + (Number of Exterior routes)

HEADER structure is 12 bytes as follows :

4  bits         Version (only version 1 is defined)
4  bits         Opcode (1=Replay, 2=Request)
8  bits         Update Release
16 bits         Autonomous system number
16 bits         Number of Interior routes
16 bits         Number of System routes
16 bits         Number of Exterior routes
16 bits         Checksum
-------
12 bytes in header

VECTOR structure is 14 bytes as follows :
24 bits         Network
24 bits         Delay
24 bits         Bandwidth
16 bits         MTU
8  bits         Reliability
8  bits         Load
8  bits         Hop count
-------
14 bytes in 1 vector

It is interesting how is coded an ip network address in 3 bytes because IGRP is a classful routing protocol:
If it is a interior route then this 3 bytes are the final bytes, and the first one is taken from the source ip address of the ip packet
If it is a system route or a exterior route then this 3 bytes are the first three and the last byte is not important

If the Delay is 0xFFFFFF then the network is unreachable

*/

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
