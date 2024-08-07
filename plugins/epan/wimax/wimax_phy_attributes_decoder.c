/* wimax_phy_attributes_decoder.c
 * WiMax PDU Burst Physical Attributes decoder
 *
 * Copyright (c) 2007 by Intel Corporation.
 *
 * Author: Lu Pan <lu.pan@intel.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1999 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Include files */

#include "config.h"

#include <epan/packet.h>
#include "wimax-int.h"

extern int proto_wimax;

static int proto_wimax_phy_attributes_decoder;
static int ett_wimax_phy_attributes_decoder;

static const value_string vals_subchannel_types[] =
{
	{ 0, "DL PUSC"},
	{ 1, "DL FUSC"},
	{16, "UL PUSC"},
	{0, NULL}
};

static const value_string vals_modulation_rates[] =
{
	{0, "BPSK R=1/2"},
	{1, "QPSK R=1/2"},
	{2, "QPSK R=3/4"},
	{3, "16-QAM R=1/2"},
	{4, "16-QAM R=3/4"},
	{5, "64-QAM R=1/2"},
	{6, "64-QAM R=2/3"},
	{7, "64-QAM R=3/4"},
	{8, "64-QAM R=5/6"},
	{0, NULL}
};

static const value_string vals_encoding_types[] =
{
	{0, "Tail biting convolutional coding (CCTB)"},
	{1, "Convolutional turbo coding (CTC)"},
	{0, NULL}
};

static int hf_phy_attributes_subchannelization_type;
static int hf_phy_attributes_permbase;
static int hf_phy_attributes_modulation_rate;
static int hf_phy_attributes_encoding_type;
static int hf_phy_attributes_num_repeat;
static int hf_phy_attributes_symbol_offset;
static int hf_phy_attributes_num_of_slots;
static int hf_phy_attributes_subchannel;



static int dissect_wimax_phy_attributes_decoder(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	unsigned offset = 0;
	unsigned tvb_len;
/*	unsigned num_of_slots;*/
	proto_item *phy_item = NULL;
	proto_tree *phy_tree = NULL;

	/* update the info column */
	/*col_append_str(pinfo->cinfo, COL_INFO, "PDU Burst Physical Attributes:");*/
	col_append_sep_str(pinfo->cinfo, COL_INFO, NULL, "PHY-attr");
	if (tree)
	{	/* we are being asked for details */
		/* get the tvb reported length */
		tvb_len = tvb_reported_length(tvb);
		/* display PDU Burst Physical Attributes dissector info */
		phy_item = proto_tree_add_protocol_format(tree, proto_wimax_phy_attributes_decoder, tvb, offset, tvb_len, "PDU Burst Physical Attributes (%u bytes)", tvb_len);
		/* add PDU Burst Physical Attributes subtree */
		phy_tree = proto_item_add_subtree(phy_item, ett_wimax_phy_attributes_decoder);
		/* display the subchannelization type */
		proto_tree_add_item(phy_tree, hf_phy_attributes_subchannelization_type, tvb, offset++, 1, ENC_BIG_ENDIAN);
		/* display the permbase */
		proto_tree_add_item(phy_tree, hf_phy_attributes_permbase, tvb, offset++, 1, ENC_BIG_ENDIAN);
		/* display the modulation rate */
		proto_tree_add_item(phy_tree, hf_phy_attributes_modulation_rate, tvb, offset++, 1, ENC_BIG_ENDIAN);
		/* display the encoding type */
		proto_tree_add_item(phy_tree, hf_phy_attributes_encoding_type, tvb, offset++, 1, ENC_BIG_ENDIAN);
		/* display the numRepeat */
		proto_tree_add_item(phy_tree, hf_phy_attributes_num_repeat, tvb, offset++, 1, ENC_BIG_ENDIAN);
		/* display the symbol offset */
		proto_tree_add_item(phy_tree, hf_phy_attributes_symbol_offset, tvb, offset++, 1, ENC_BIG_ENDIAN);
		/* display the number of slots */
		proto_tree_add_item(phy_tree, hf_phy_attributes_num_of_slots, tvb, offset, 2, ENC_BIG_ENDIAN);
		/* get the number of slots */
/*		num_of_slots =  tvb_get_uint16(tvb, offset);*/
		/* move to next field */
		offset += 2;
		/* display the physical subchannel list */
		while(offset < tvb_len)
		{
			proto_tree_add_item(phy_tree, hf_phy_attributes_subchannel, tvb, offset++, 1, ENC_BIG_ENDIAN);
		}
	}
	return tvb_captured_length(tvb);
}

/* Register Wimax PDU Burst Physical Attributes Protocol */
void wimax_proto_register_wimax_phy_attributes(void)
{
	/* Physical Attributes display */
	static hf_register_info hf[] =
	{
		{
			&hf_phy_attributes_subchannelization_type,
			{"Subchannelization Type", "wmx.phy_attributes.subchannelization_type", FT_UINT8, BASE_DEC, VALS(vals_subchannel_types), 0x0, NULL, HFILL}
		},
		{
			&hf_phy_attributes_permbase,
			{"Permbase", "wmx.phy_attributes.permbase", FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
		},
		{
			&hf_phy_attributes_modulation_rate,
			{"Modulation Rate", "wmx.phy_attributes.modulation_rate", FT_UINT8, BASE_DEC, VALS(vals_modulation_rates), 0x0, NULL, HFILL}
		},
		{
			&hf_phy_attributes_encoding_type,
			{"Encoding Type", "wmx.phy_attributes.encoding_type", FT_UINT8, BASE_DEC, VALS(vals_encoding_types), 0x0, NULL, HFILL}
		},
		{
			&hf_phy_attributes_num_repeat,
			{"numRepeat", "wmx.phy_attributes.num_repeat", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL}
		},
		{
			&hf_phy_attributes_symbol_offset,
			{"Symbol Offset", "wmx.phy_attributes.symbol_offset", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL}
		},
		{
			&hf_phy_attributes_num_of_slots,
			{"Number Of Slots", "wmx.phy_attributes.num_of_slots", FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
		},
		{
			&hf_phy_attributes_subchannel,
			{"Subchannel", "wmx.phy_attributes.subchannel", FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
		}
	};

	/* Setup protocol subtree array */
	static int *ett[] =
		{
			&ett_wimax_phy_attributes_decoder
		};

	proto_wimax_phy_attributes_decoder = proto_wimax;

	register_dissector("wimax_phy_attributes_burst_handler", dissect_wimax_phy_attributes_decoder, proto_wimax_phy_attributes_decoder);

	proto_register_field_array(proto_wimax_phy_attributes_decoder, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
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
