/* packet-llt.c
 * Routines for Veritas Low Latency Transport (LLT) dissection
 * Copyright 2006, Stephen Fisher (see AUTHORS file)
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/prefs.h>
#include <epan/etypes.h>

void proto_register_llt(void);
void proto_reg_handoff_llt(void);

static dissector_handle_t llt_handle;

static const value_string message_type_vs[] = {
	{ 0x0a, "heartbeat" },
	{ 0, NULL}
};

/* Variables for our preferences */
static unsigned preference_alternate_ethertype = 0x0;

/* Initialize the protocol and registered fields */
static int proto_llt;

static int hf_llt_cluster_num;
static int hf_llt_node_id;
static int hf_llt_message_type;
static int hf_llt_sequence_num;
static int hf_llt_message_time;
static int hf_llt_dst_node_id;
static int hf_llt_src_node_id;

/* Initialize the subtree pointers */
static int ett_llt;

/* Code to actually dissect the packets */
static int
dissect_llt(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	/* Set up structures needed to add the protocol subtree and manage it */
	proto_item *ti;
	proto_tree *llt_tree;
	uint8_t message_type;
        uint16_t magic;

	/* Make entries in Protocol column and Info column on summary display */
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "LLT");

	magic = tvb_get_uint16(tvb, 0, ENC_BIG_ENDIAN);
	if(magic == 0x0602){ /* v2 ? */

		ti = proto_tree_add_item(tree, proto_llt, tvb, 0, -1, ENC_NA);
		llt_tree = proto_item_add_subtree(ti, ett_llt);

		proto_tree_add_item(llt_tree, hf_llt_cluster_num, tvb, 2, 2, ENC_LITTLE_ENDIAN);
		proto_tree_add_item(llt_tree, hf_llt_dst_node_id, tvb, 6, 1, ENC_LITTLE_ENDIAN);
		proto_tree_add_item(llt_tree, hf_llt_src_node_id, tvb, 8, 1, ENC_LITTLE_ENDIAN);

	} else {
		message_type = tvb_get_uint8(tvb, 3);

		col_add_fstr(pinfo->cinfo, COL_INFO, "Message type: %s", val_to_str(message_type, message_type_vs, "Unknown (0x%02x)"));

		ti = proto_tree_add_item(tree, proto_llt, tvb, 0, -1, ENC_NA);
		llt_tree = proto_item_add_subtree(ti, ett_llt);

		proto_tree_add_item(llt_tree, hf_llt_cluster_num, tvb, 2, 1, ENC_BIG_ENDIAN);
		proto_tree_add_item(llt_tree, hf_llt_message_type, tvb, 3, 1, ENC_BIG_ENDIAN);
		proto_tree_add_item(llt_tree, hf_llt_node_id, tvb, 7, 1, ENC_BIG_ENDIAN);
		proto_tree_add_item(llt_tree, hf_llt_sequence_num, tvb, 24, 4, ENC_BIG_ENDIAN);
		proto_tree_add_item(llt_tree, hf_llt_message_time, tvb, 40, 4, ENC_BIG_ENDIAN);
	}
	return tvb_captured_length(tvb);
}

/* Register the protocol with Wireshark */
void
proto_register_llt(void)
{
	module_t *llt_module;

	static hf_register_info hf[] = {

		{ &hf_llt_cluster_num,  { "Cluster number", "llt.cluster_num",
					  FT_UINT16, BASE_DEC, NULL, 0,
					  "Cluster number that this node belongs to", HFILL } },

		{ &hf_llt_message_type, { "Message type", "llt.message_type",
					  FT_UINT8, BASE_HEX, VALS(message_type_vs), 0,
					  "Type of LLT message contained in this frame", HFILL } },

		{ &hf_llt_node_id,      { "Node ID", "llt.node_id",
					  FT_UINT8, BASE_DEC, NULL, 0,
					  "Number identifying this node within the cluster", HFILL } },

		{ &hf_llt_sequence_num, { "Sequence number", "llt.sequence_num",
					  FT_UINT32, BASE_DEC, NULL, 0,
					  "Sequence number of this frame", HFILL } },

		{ &hf_llt_message_time, { "Message time", "llt.message_time",
					  FT_UINT32, BASE_DEC, NULL, 0,
					  "Number of ticks since this node was last rebooted", HFILL } },

		{ &hf_llt_dst_node_id,  { "Destination Node ID", "llt.dst.node_id",
					  FT_UINT8, BASE_DEC, NULL, 0,
					  "Number identifying destination node within the cluster", HFILL } },

		{ &hf_llt_src_node_id,  { "Source Node ID", "llt.src.node_id",
					  FT_UINT8, BASE_DEC, NULL, 0,
					  "Number identifying source node within the cluster", HFILL } },

	};

	/* Setup protocol subtree array */
	static int *ett[] = {
		&ett_llt,
	};

	/* Register the protocol name and description */
	proto_llt = proto_register_protocol("Veritas Low Latency Transport (LLT)", "LLT", "llt");

	/* Required function calls to register the header fields and subtrees used */
	proto_register_field_array(proto_llt, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	/* Register preferences module */
	llt_module = prefs_register_protocol(proto_llt, proto_reg_handoff_llt);

	/* Register our preferences */
	prefs_register_uint_preference(llt_module, "alternate_ethertype", "Alternate ethertype value (in hex)",
				       "Dissect this ethertype as LLT traffic in addition to the default, 0xCAFE.",
				       16, &preference_alternate_ethertype); /* A base-16 (hexadecimal) value */

	/* Register our dissector */
	llt_handle = register_dissector("llt", dissect_llt, proto_llt);
}


void
proto_reg_handoff_llt(void)
{
	static bool initialized = false;
	static unsigned preference_alternate_ethertype_last;

	if (!initialized) {
		dissector_add_uint("ethertype", ETHERTYPE_LLT, llt_handle);
		initialized = true;
	} else {
		if (preference_alternate_ethertype_last != 0x0) {
			dissector_delete_uint("ethertype", preference_alternate_ethertype_last, llt_handle);
		}
	}

	/* Save the setting to see if it has changed later */
	preference_alternate_ethertype_last = preference_alternate_ethertype;

	if (preference_alternate_ethertype != 0x0) {
 		/* Register the new ethertype setting */
		dissector_add_uint("ethertype", preference_alternate_ethertype, llt_handle);
	}
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
