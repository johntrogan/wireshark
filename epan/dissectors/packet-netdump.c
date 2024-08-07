/* packet-netdump.c
 * Routines for Netdump dissection
 * Copyright 2009, Neil Horman <nhorman@tuxdriver.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>

void proto_register_netdump(void);
void proto_reg_handoff_netdump(void);

static dissector_handle_t netdump_handle;

/* Initialize the protocol and registered fields */
static int proto_netdump;
static int hf_netdump_magic_number;
static int hf_netdump_seq_nr;
static int hf_netdump_command;
static int hf_netdump_from;
static int hf_netdump_to;
static int hf_netdump_payload;
static int hf_netdump_code;
static int hf_netdump_info;
static int hf_netdump_version;

/* Initialize the subtree pointers */
static int ett_netdump;

static const value_string command_names[] = {
	{ 0, "COMM_NONE" },
	{ 1, "COMM_SEND_MEM" },
	{ 2, "COMM_EXIT" },
	{ 3, "COMM_REBOOT" },
	{ 4, "COMM_HELLO" },
	{ 5, "COMM_GET_NR_PAGES" },
	{ 6, "COMM_GET_PAGE_SIZE" },
	{ 7, "COMM_START_NETDUMP_ACK" },
	{ 8, "COMM_GET_REGS" },
	{ 9, "COMM_SHOW_STATE" },
	{ 0, NULL }
};

static const value_string reply_code_names[] = {
	{ 0, "REPLY_NONE" },
	{ 1, "REPLY_ERROR" },
	{ 2, "REPLY_LOG" },
	{ 3, "REPLY_MEM" },
	{ 4, "REPLY_RESERVED" },
	{ 5, "REPLY_HELLO" },
	{ 6, "REPLY_NR_PAGES" },
	{ 7, "REPLY_PAGE_SIZE" },
	{ 8, "REPLY_START_NETDUMP" },
	{ 9, "REPLY_END_NETDUMP" },
	{ 10, "REPLY_REGS" },
	{ 11, "REPLY_MAGIC" },
	{ 12, "REPLY_SHOW_STATE" },
	{ 0, NULL }
};


/* Code to actually dissect the packets */
static int
dissect_netdump(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "Netdump");
	/* Clear out stuff in the info column */
	col_clear(pinfo->cinfo, COL_INFO);

	if (tree) { /* we are being asked for details */
		proto_item *ti = NULL;
		proto_tree *netdump_tree = NULL;
		ti = proto_tree_add_item(tree, proto_netdump, tvb, 0, -1, ENC_NA);
		netdump_tree = proto_item_add_subtree(ti, ett_netdump);
		if (tvb_reported_length(tvb) == 24) {
			/* Its a request format packet */
			proto_tree_add_item(netdump_tree, hf_netdump_magic_number, tvb, 0, 8, ENC_BIG_ENDIAN);
			proto_tree_add_item(netdump_tree, hf_netdump_seq_nr, tvb, 8, 4, ENC_BIG_ENDIAN);
			proto_tree_add_item(netdump_tree, hf_netdump_command, tvb, 12, 4, ENC_BIG_ENDIAN);
			proto_tree_add_item(netdump_tree, hf_netdump_from, tvb, 16, 4, ENC_BIG_ENDIAN);
			proto_tree_add_item(netdump_tree, hf_netdump_to, tvb, 20, 4, ENC_BIG_ENDIAN);
		} else {
			/* Its a reply packet */
			proto_tree_add_item(netdump_tree, hf_netdump_version, tvb, 0, 1, ENC_BIG_ENDIAN);
			proto_tree_add_item(netdump_tree, hf_netdump_seq_nr, tvb, 1, 4, ENC_BIG_ENDIAN);
			proto_tree_add_item(netdump_tree, hf_netdump_code, tvb, 5, 4, ENC_BIG_ENDIAN);
			proto_tree_add_item(netdump_tree, hf_netdump_info, tvb, 9, 4, ENC_LITTLE_ENDIAN);
			proto_tree_add_item(netdump_tree, hf_netdump_payload, tvb, 13, -1, ENC_NA);
		}
	}
	return tvb_captured_length(tvb);
}

void proto_register_netdump(void)
{
	/* Setup protocol subtree array */
	static int *ett[] = {
		&ett_netdump
	};

	static hf_register_info hf[] = {
		{ &hf_netdump_magic_number,
			{ "Netdump Magic Number", "netdump.magic",
			FT_UINT64, BASE_HEX,
			NULL, 0x0,
			NULL, HFILL }
		},
		{ &hf_netdump_seq_nr,
			{"Netdump seq number", "netdump.seq_nr",
			FT_UINT32, BASE_HEX,
			NULL, 0x0,
			NULL, HFILL}
		},
		{ &hf_netdump_command,
			{"Netdump command", "netdump.command",
			FT_UINT32, BASE_DEC,
			VALS(command_names), 0x0,
			NULL, HFILL}
		},
		{ &hf_netdump_from,
			{"Netdump from val", "netdump.from",
			FT_UINT32, BASE_HEX,
			NULL, 0x0,
			NULL, HFILL}
		},
		{ &hf_netdump_to,
			{"Netdump to val", "netdump.to",
			FT_UINT32, BASE_HEX,
			NULL, 0x0,
			NULL, HFILL}
		},
		{ &hf_netdump_code,
			{"Netdump code", "netdump.code",
			FT_UINT32, BASE_DEC,
			VALS(reply_code_names), 0x0,
			NULL, HFILL}
		},
		{ &hf_netdump_info,
			{"Netdump info", "netdump.info",
			FT_UINT32, BASE_HEX,
			NULL, 0x0,
			NULL, HFILL}
		},
		{ &hf_netdump_payload,
			{"Netdump payload", "netdump.payload",
			FT_BYTES, BASE_NONE,
			NULL, 0x0,
			NULL, HFILL}
		},
		{ &hf_netdump_version,
			{"Netdump version", "netdump.version",
			FT_UINT8, BASE_HEX,
			NULL, 0x0,
			NULL, HFILL}
		}
	};

	proto_netdump = proto_register_protocol ("Netdump Protocol", "Netdump", "netdump" );
	proto_register_field_array(proto_netdump, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	netdump_handle = register_dissector("netdump", dissect_netdump, proto_netdump);
}

void proto_reg_handoff_netdump(void)
{
	dissector_add_for_decode_as_with_preference("udp.port", netdump_handle);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
