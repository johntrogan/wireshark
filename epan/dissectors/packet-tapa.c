/* packet-tapa.c
 * Routines for the disassembly of the Trapeze TAPA protocol
 *
 * Copyright 2007 Joerg Mayer (see AUTHORS file)
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
  TODO:

Specs:

	No specs available.
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/to_str.h>
#include <epan/ipproto.h>
#include <epan/expert.h>
#include "packet-ip.h"

void proto_reg_handoff_tapa(void);
void proto_register_tapa(void);

/* protocol handles */
static int proto_tapa;

/* ett handles */
static int ett_tapa_discover;
static int ett_tapa_discover_req;
static int ett_tapa_tunnel;

/* hf elements */
static int hf_tapa_discover_type;
static int hf_tapa_discover_flags;
static int hf_tapa_discover_length;
static int hf_tapa_discover_unknown;

static int hf_tapa_discover_req_type;
static int hf_tapa_discover_req_pad;
static int hf_tapa_discover_req_length;
static int hf_tapa_discover_req_value;

static int hf_tapa_discover_newtlv_type;
static int hf_tapa_discover_newtlv_pad;
static int hf_tapa_discover_newtlv_length;
static int hf_tapa_discover_newtlv_valuetext;
static int hf_tapa_discover_newtlv_valuehex;

static int hf_tapa_discover_reply_switchip;
static int hf_tapa_discover_reply_unused;
static int hf_tapa_discover_reply_bias;
static int hf_tapa_discover_reply_pad;

static int hf_tapa_tunnel_version;
static int hf_tapa_tunnel_five;
static int hf_tapa_tunnel_type;
static int hf_tapa_tunnel_zero;
static int hf_tapa_tunnel_dmac;
static int hf_tapa_tunnel_smac;
static int hf_tapa_tunnel_seqno;
static int hf_tapa_tunnel_length;
static int hf_tapa_tunnel_0804;
static int hf_tapa_tunnel_tagsetc;

static int hf_tapa_tunnel_remaining;

static expert_field ei_tapa_length_too_short;

static dissector_handle_t tapa_handle;

#define PROTO_SHORT_NAME "TAPA"
#define PROTO_LONG_NAME "Trapeze Access Point Access Protocol"

#define PORT_TAPA	5000 /* Not IANA registered */

typedef enum {
	TAPA_TYPE_REQUEST	= 0x01,
	TAPA_TYPE_REPLY		= 0x02,
	TAPA_TYPE_REQUEST_NEW	= 0x04,
	TAPA_TYPE_REPLY_NEW	= 0x05
} tapa_discover_type_t;

static const value_string tapa_discover_type_vals[] = {
	{ TAPA_TYPE_REQUEST,		"Request" },
	{ TAPA_TYPE_REPLY,		"Reply" },
	{ TAPA_TYPE_REQUEST_NEW,	"NewRequest" },
	{ TAPA_TYPE_REPLY_NEW,		"NewReply" },

	{ 0,	NULL }
};

typedef enum {
	TAPA_TUNNEL_TYPE_0	= 0x00,
	TAPA_TUNNEL_TYPE_1	= 0x01
} tapa_tunnel_type_t;

static const value_string tapa_tunnel_type_vals[] = {
	{ TAPA_TUNNEL_TYPE_0,	"Type 0" },
	{ TAPA_TUNNEL_TYPE_1,	"Type 1" },

	{ 0,	NULL }
};

typedef enum {
	TAPA_REQUEST_SERIAL	= 0x01,
	TAPA_REQUEST_MODEL	= 0x02
} tapa_discover_request_t;

static const value_string tapa_discover_request_vals[] = {
	{ TAPA_REQUEST_SERIAL,	"SerialNo" },
	{ TAPA_REQUEST_MODEL,	"Model" },

	{ 0,	NULL }
};

#if 0
static const value_string tapa_discover_unknown_vals[] = {

	{ 0,	NULL }
};
#endif

static bool
check_ascii(tvbuff_t *tvb, int offset, int length)
{
	int i;
	uint8_t buf;

	for (i = 0; i < length; i++) {
		buf = tvb_get_uint8(tvb, offset+i);
		if (buf < 0x20 || buf >= 0x80) {
			return false;
		}
	}
	return true;
}

static int
dissect_tapa_discover_reply(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tapa_discover_tree, uint32_t offset, int remaining)
{
	proto_tree_add_item(tapa_discover_tree, hf_tapa_discover_reply_switchip, tvb, offset, 4,
		ENC_BIG_ENDIAN);

	col_append_fstr(pinfo->cinfo, COL_INFO, ", Switch: %s",
			tvb_ip_to_str(pinfo->pool, tvb, offset));

	offset += 4;

	proto_tree_add_item(tapa_discover_tree, hf_tapa_discover_reply_unused, tvb, offset, 1,
		ENC_BIG_ENDIAN);
	offset += 1;

	proto_tree_add_item(tapa_discover_tree, hf_tapa_discover_reply_bias, tvb, offset, 1,
		ENC_BIG_ENDIAN);
	offset += 1;

	remaining -= 6;
	proto_tree_add_item(tapa_discover_tree, hf_tapa_discover_reply_pad, tvb, offset, remaining,
		ENC_NA);
	offset += remaining;

	return offset;
}

static int
dissect_tapa_discover_req(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tapa_discover_tree, uint32_t offset, int remaining)
{
	proto_tree	*tapa_discover_item_tree;
	uint8_t		 item_type;
	int		 item_length;
	char		*item_text;
	const char	*item_type_text;

	while (remaining > 0) {
		item_type = tvb_get_uint8(tvb, offset);
		item_type_text = val_to_str(item_type, tapa_discover_request_vals, "%d");
		item_length = tvb_get_ntohs(tvb, offset + 2);
		item_text = tvb_format_text(pinfo->pool, tvb, offset + 4, item_length);

		col_append_fstr(pinfo->cinfo, COL_INFO, ", %s: %s",
				item_type_text, item_text);

		tapa_discover_item_tree = proto_tree_add_subtree_format(tapa_discover_tree, tvb, offset, 4 + item_length,
			ett_tapa_discover_req, NULL, "Type %d = %s, length %d, value %s",
			item_type, item_type_text, item_length, item_text);

		proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_req_type, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		offset += 1;

		proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_req_pad, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		offset += 1;

		proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_req_length, tvb, offset, 2,
			ENC_BIG_ENDIAN);
		offset += 2;
		proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_req_value, tvb, offset, item_length,
			ENC_NA);
		offset += item_length;

		remaining -= (item_length + 4);
	}
	return offset;
}

static int
dissect_tapa_discover_unknown_new_tlv(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tapa_discover_tree, uint32_t offset, int remaining)
{
	proto_tree	*tapa_discover_item_tree;
	proto_item	*item, *discover_item;
	uint8_t		 item_type;
	int		 item_length;
	const char	*item_text;
	/*const char	*item_type_text;*/
	bool	 is_ascii;

	while (remaining > 3) {  /* type(1) + flags(1) + length(2) */
		item_type = tvb_get_uint8(tvb, offset);
		/*item_type_text = val_to_str(item_type, tapa_discover_unknown_vals, "%d");*/
		item_length = tvb_get_ntohs(tvb, offset + 2) - 4;

		tapa_discover_item_tree = proto_tree_add_subtree_format(tapa_discover_tree, tvb, offset, 4 + item_length,
			ett_tapa_discover_req, &discover_item, "Type %d, length %d", item_type, item_length);

		proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_newtlv_type, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		offset += 1;

		proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_newtlv_pad, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		offset += 1;

		item = proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_newtlv_length, tvb, offset, 2,
			ENC_BIG_ENDIAN);
		offset += 2;

		if (item_length <= 0) {
			expert_add_info(pinfo, item, &ei_tapa_length_too_short);
			return offset;
		}

		is_ascii = check_ascii(tvb, offset + 4, item_length);
		if (is_ascii)
			item_text = tvb_format_text(pinfo->pool, tvb, offset + 4, item_length);
		else
			item_text = "BINARY-DATA";

		col_append_fstr(pinfo->cinfo, COL_INFO, ", T=%d L=%d",
				item_type, item_length);
		proto_item_append_text(discover_item, ", value %s", item_text);

		if (is_ascii)
			proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_newtlv_valuetext,
				tvb, offset, item_length, ENC_ASCII);
		else
			proto_tree_add_item(tapa_discover_item_tree, hf_tapa_discover_newtlv_valuehex,
				tvb, offset, item_length, ENC_NA);
		offset += item_length;

		remaining -= (item_length + 4);
	}
	return offset;
}

static int
dissect_tapa_discover(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_item *ti;
	proto_tree *tapa_discover_tree = NULL;
	int offset = 0;
	uint8_t packet_type;
	int remaining;

	packet_type = tvb_get_uint8(tvb, 0);
	remaining = tvb_get_ntohs(tvb, 2) - 4;

	col_set_str(pinfo->cinfo, COL_PROTOCOL, PROTO_SHORT_NAME);
	col_add_fstr(pinfo->cinfo, COL_INFO, "Discover - %s",
			val_to_str(packet_type, tapa_discover_type_vals, "Unknown (%d)"));

	if (tree) {
		ti = proto_tree_add_item(tree, proto_tapa, tvb, offset, -1,
			ENC_NA);
		tapa_discover_tree = proto_item_add_subtree(ti, ett_tapa_discover);

		proto_tree_add_item(tapa_discover_tree, hf_tapa_discover_type, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		offset += 1;

		proto_tree_add_item(tapa_discover_tree, hf_tapa_discover_flags, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		offset += 1;

		ti = proto_tree_add_item(tapa_discover_tree, hf_tapa_discover_length, tvb, offset, 2,
			ENC_BIG_ENDIAN);
		offset += 2;

		if (remaining <= 0) {
			expert_add_info(pinfo, ti, &ei_tapa_length_too_short);
			return offset;
		}

		switch (packet_type) {
		case TAPA_TYPE_REQUEST:
			offset = dissect_tapa_discover_req(tvb, pinfo, tapa_discover_tree, offset, remaining);
			break;
		case TAPA_TYPE_REPLY:
			offset = dissect_tapa_discover_reply(tvb, pinfo, tapa_discover_tree, offset, remaining);
			break;
		case TAPA_TYPE_REQUEST_NEW:
		case TAPA_TYPE_REPLY_NEW:
			offset = dissect_tapa_discover_unknown_new_tlv(tvb, pinfo, tapa_discover_tree,
					offset, remaining);
			break;
		default:
			proto_tree_add_item(tapa_discover_tree, hf_tapa_discover_unknown, tvb, offset,
					remaining, ENC_NA);
		offset += 1;

			break;
		}
	}
	return offset;
}

static int
dissect_tapa_tunnel(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_item *ti;
	proto_tree *tapa_tunnel_tree = NULL;
	uint32_t offset = 0;
	uint8_t version;
	uint8_t type;
	unsigned remaining;

	version = tvb_get_uint8(tvb, 0) & 0xF0;
	type = tvb_get_uint8(tvb, 1);
	remaining = tvb_reported_length(tvb);

	col_set_str(pinfo->cinfo, COL_PROTOCOL, PROTO_SHORT_NAME);
	col_add_fstr(pinfo->cinfo, COL_INFO, "Tunnel - V=%d, T=%s", version >> 4,
			val_to_str(type, tapa_tunnel_type_vals, "Unknown (%d)"));

	if (tree) {
		ti = proto_tree_add_item(tree, proto_tapa, tvb, offset, -1,
			ENC_NA);
		tapa_tunnel_tree = proto_item_add_subtree(ti, ett_tapa_tunnel);

		proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_version, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_five, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		offset += 1;

		proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_type, tvb, offset, 1,
			ENC_BIG_ENDIAN);
		offset += 1;

		proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_zero, tvb, offset, 8,
			ENC_NA);
		offset += 8;

		proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_dmac, tvb, offset, 6,
			ENC_NA);
		offset += 6;

		proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_smac, tvb, offset, 6,
			ENC_NA);
		offset += 6;

		switch (type) {
		case TAPA_TUNNEL_TYPE_0:
			proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_0804, tvb, offset, 2,
				ENC_BIG_ENDIAN);
			offset += 2;

			proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_tagsetc, tvb, offset, 6,
				ENC_NA);
			offset += 6;

			break;
		case TAPA_TUNNEL_TYPE_1:
			proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_seqno, tvb, offset, 2,
				ENC_BIG_ENDIAN);
			offset += 2;

			proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_length, tvb, offset, 2,
				ENC_BIG_ENDIAN);
			offset += 2;
			break;
		default:
			break;
		}


		/* FIXME: This is just to help figuring out what the bytes mean */
		proto_tree_add_item(tapa_tunnel_tree, hf_tapa_tunnel_remaining, tvb,
			offset, remaining - offset, ENC_NA);
		offset = remaining;

	}
	return offset;
}

static bool
test_tapa_discover(tvbuff_t *tvb)
{
	uint8_t type, req_type;
	uint16_t length;

	if (tvb_captured_length(tvb) < 4)
		return false;

	/* Type(1 byte) <= 5, unknown(1 byte), length(2 bytes) */
	type = tvb_get_uint8(tvb, 0);
	/* unknown = tvb_get_uint8(tvb, 1); */
	length = tvb_get_ntohs(tvb, 2);
	req_type = tvb_get_uint8(tvb, 4);

	if (type < TAPA_TYPE_REQUEST		||
	    type > TAPA_TYPE_REPLY_NEW		||
	    length < 12				||
	    length > 1472			||
	    (type == TAPA_TYPE_REQUEST && (req_type < TAPA_REQUEST_SERIAL || req_type > TAPA_REQUEST_MODEL))) {
		return false;
	}

	return true;
}

static bool
test_tapa_tunnel(tvbuff_t *tvb)
{
	/* If it isn't IPv4, it's TAPA. IPv4: Version(1 byte) = 4,
		length(2 bytes) >= 20 */
	if (tvb_captured_length(tvb) < 4 ||
	    (tvb_get_uint8(tvb, 0) & 0xF0) >= 0x40 ||
	    tvb_get_ntohs(tvb, 2) > 0 ||
	    tvb_get_uint8(tvb, 1) > 1) {	/* Is tunnel type known? */
		return false;
	}
	return true;
}

static int
dissect_tapa_static(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
	if (test_tapa_discover(tvb)) {
		return dissect_tapa_discover(tvb, pinfo, tree);
	} else if (test_tapa_tunnel(tvb)) {
		return dissect_tapa_tunnel(tvb, pinfo, tree);
	} else
		return 0;
}

/* heuristic dissector */
static bool
dissect_tapa_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *iph)
{
	/* The TAPA protocol also uses IP protocol number 4 but it isn't really IPIP */
	if ((ws_ip_protocol(iph) == IP_PROTO_IPIP) && ((tvb_get_uint8(tvb, 0) & 0xF0) != 0x40) &&
	    (tvb_get_ntohs(tvb, 2)) < 20) {
		dissect_tapa_static(tvb, pinfo, tree, iph);
		return true;
	}

	return false;
}

void
proto_register_tapa(void)
{
	expert_module_t* expert_tapa;

	static hf_register_info hf[] = {

		/* TAPA discover header */
		{ &hf_tapa_discover_type,
		  { "Type",	"tapa.discover.type", FT_UINT8, BASE_DEC, VALS(tapa_discover_type_vals),
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_flags,
		  { "Flags",	"tapa.discover.flags", FT_UINT8, BASE_HEX, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_length,
		  { "Length",	"tapa.discover.length", FT_UINT16, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},

		/* TAPA discover request */
		{ &hf_tapa_discover_req_type,
		  { "Req type",	"tapa.discover.req.type", FT_UINT8, BASE_DEC, VALS(tapa_discover_request_vals),
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_req_pad,
		  { "Req padding",	"tapa.discover.req.pad", FT_UINT8, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_req_length,
		  { "Req length",	"tapa.discover.req.length", FT_UINT16, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_req_value,
		  { "Req value",   "tapa.discover.req.value", FT_BYTES, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		/* TAPA discover reply */
		{ &hf_tapa_discover_reply_switchip,
		  { "Switch Ip",   "tapa.discover.reply.switchip", FT_IPv4, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_reply_unused,
		  { "Reply unused",	"tapa.discover.reply.unused", FT_UINT8, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_reply_bias,
		  { "Reply bias",	"tapa.discover.reply.bias", FT_UINT8, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_reply_pad,
		  { "Reply pad",   "tapa.discover.reply.pad", FT_BYTES, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		/* TAPA discover new request/reply tlv */
		{ &hf_tapa_discover_newtlv_type,
		  { "New tlv type",	"tapa.discover.newtlv.type", FT_UINT8, BASE_DEC, VALS(tapa_discover_request_vals),
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_newtlv_pad,
		  { "New tlv padding",	"tapa.discover.newtlv.pad", FT_UINT8, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_newtlv_length,
		  { "New tlv length",	"tapa.discover.newtlv.length", FT_UINT16, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_newtlv_valuetext,
		  { "New tlv value",   "tapa.discover.newtlv.valuetext", FT_STRING, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_discover_newtlv_valuehex,
		  { "New tlv value",   "tapa.discover.newtlv.valuehex", FT_BYTES, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		/* TAPA discover unknown packet */
		{ &hf_tapa_discover_unknown,
		  { "Tapa unknown packet",   "tapa.discover.unknown", FT_BYTES, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		/* TAPA tunnel */
		{ &hf_tapa_tunnel_version,
		  { "Tapa tunnel version",   "tapa.tunnel.version", FT_UINT8, BASE_HEX, NULL,
		    0xF0, NULL, HFILL }},

		{ &hf_tapa_tunnel_five,
		  { "Tapa tunnel five",   "tapa.tunnel.five", FT_UINT8, BASE_HEX, NULL,
		    0x0F, NULL, HFILL }},

		{ &hf_tapa_tunnel_type,
		  { "Tapa tunnel type",   "tapa.tunnel.type", FT_UINT8, BASE_HEX, VALS(tapa_tunnel_type_vals),
		    0x0, NULL, HFILL }},

		{ &hf_tapa_tunnel_zero,
		  { "Tapa tunnel zeroes",   "tapa.tunnel.zero", FT_BYTES, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_tunnel_dmac,
		  { "Tapa tunnel dest mac",   "tapa.tunnel.dmac", FT_ETHER, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_tunnel_smac,
		  { "Tapa tunnel src mac",   "tapa.tunnel.smac", FT_ETHER, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		/* TAPA tunnel type 0 */
		{ &hf_tapa_tunnel_0804,
		  { "Tapa tunnel 0804",   "tapa.tunnel.0804", FT_UINT16, BASE_HEX, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_tunnel_tagsetc,
		  { "Tapa tunnel tags, seqno, pad",   "tapa.tunnel.tags", FT_BYTES, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

		/* TAPA tunnel type 1 */
		{ &hf_tapa_tunnel_seqno,
		  { "Tapa tunnel seqno",   "tapa.tunnel.seqno", FT_UINT16, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},

		{ &hf_tapa_tunnel_length,
		  { "Tapa tunnel length",   "tapa.tunnel.length", FT_UINT16, BASE_DEC, NULL,
		    0x0, NULL, HFILL }},


		/* TAPA tunnel remaining stuff */
		{ &hf_tapa_tunnel_remaining,
		  { "Tapa tunnel all data",   "tapa.tunnel.remaining", FT_BYTES, BASE_NONE, NULL,
		    0x0, NULL, HFILL }},

	};
	static int *ett[] = {
		&ett_tapa_discover,
		&ett_tapa_discover_req,
		&ett_tapa_tunnel,
	};

	static ei_register_info ei[] = {
		{ &ei_tapa_length_too_short,
		  { "tapa.length_too_short", PI_MALFORMED, PI_ERROR,
	            "Length is too short (<= 4)", EXPFILL }}
	};

	proto_tapa = proto_register_protocol(PROTO_LONG_NAME, PROTO_SHORT_NAME, "tapa");
	proto_register_field_array(proto_tapa, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
	expert_tapa = expert_register_protocol(proto_tapa);
	expert_register_field_array(expert_tapa, ei, array_length(ei));

	tapa_handle = register_dissector("tapa", dissect_tapa_static, proto_tapa);

}

void
proto_reg_handoff_tapa(void)
{
	dissector_add_uint_with_preference("udp.port", PORT_TAPA, tapa_handle);
	heur_dissector_add( "ip", dissect_tapa_heur, "TAPA over IP", "tapa_ip", proto_tapa, HEURISTIC_ENABLE);
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
