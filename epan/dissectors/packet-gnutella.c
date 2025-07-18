/* packet-gnutella.c
 * Routines for gnutella dissection
 * Copyright 2001, B. Johannessen <bob@havoq.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include "packet-tcp.h"

void proto_register_gnutella(void);
void proto_reg_handoff_gnutella(void);

static dissector_handle_t gnutella_handle;

/*
 * See
 *
 *	http://rfc-gnutella.sourceforge.net/developer/index.html
 */

static int proto_gnutella;

static int hf_gnutella_stream;

static int hf_gnutella_header;
static int hf_gnutella_header_id;
static int hf_gnutella_header_payload;
static int hf_gnutella_header_ttl;
static int hf_gnutella_header_hops;
static int hf_gnutella_header_size;

static int hf_gnutella_pong_payload;
static int hf_gnutella_pong_port;
static int hf_gnutella_pong_ip;
static int hf_gnutella_pong_files;
static int hf_gnutella_pong_kbytes;

static int hf_gnutella_query_payload;
static int hf_gnutella_query_min_speed;
static int hf_gnutella_query_search;

static int hf_gnutella_queryhit_payload;
static int hf_gnutella_queryhit_count;
static int hf_gnutella_queryhit_port;
static int hf_gnutella_queryhit_ip;
static int hf_gnutella_queryhit_speed;
static int hf_gnutella_queryhit_extra;
static int hf_gnutella_queryhit_servent_id;

static int hf_gnutella_queryhit_hit;
static int hf_gnutella_queryhit_hit_index;
static int hf_gnutella_queryhit_hit_size;
static int hf_gnutella_queryhit_hit_name;
static int hf_gnutella_queryhit_hit_extra;

static int hf_gnutella_push_payload;
static int hf_gnutella_push_servent_id;
static int hf_gnutella_push_index;
static int hf_gnutella_push_ip;
static int hf_gnutella_push_port;

static int ett_gnutella;

#define GNUTELLA_TCP_PORT	6346

/*
 * Used to determine whether a chunk of data looks like a Gnutella packet
 * or not - it might be a transfer stream, or it might be part of a
 * Gnutella packet that starts in an earlier missing TCP segment.
 *
 * One Gnutella spec says packets SHOULD be no bigger than 4K, although
 * that's SHOULD, not MUST.
 */
#define GNUTELLA_MAX_SNAP_SIZE	4096

#define GNUTELLA_UNKNOWN_NAME	"Unknown"
#define GNUTELLA_PING		0x00
#define GNUTELLA_PING_NAME	"Ping"
#define GNUTELLA_PONG		0x01
#define GNUTELLA_PONG_NAME	"Pong"
#define GNUTELLA_PUSH		0x40
#define GNUTELLA_PUSH_NAME	"Push"
#define GNUTELLA_QUERY		0x80
#define GNUTELLA_QUERY_NAME	"Query"
#define GNUTELLA_QUERYHIT	0x81
#define GNUTELLA_QUERYHIT_NAME	"QueryHit"

#define GNUTELLA_HEADER_LENGTH		23
#define GNUTELLA_SERVENT_ID_LENGTH	16
#define GNUTELLA_PORT_LENGTH		2
#define GNUTELLA_IP_LENGTH		4
#define GNUTELLA_LONG_LENGTH		4
#define GNUTELLA_SHORT_LENGTH		2
#define GNUTELLA_BYTE_LENGTH		1

#define GNUTELLA_PONG_LENGTH		14
#define GNUTELLA_PONG_PORT_OFFSET	0
#define GNUTELLA_PONG_IP_OFFSET		2
#define GNUTELLA_PONG_FILES_OFFSET	6
#define GNUTELLA_PONG_KBYTES_OFFSET	10

#define GNUTELLA_QUERY_SPEED_OFFSET	0
#define GNUTELLA_QUERY_SEARCH_OFFSET	2

#define GNUTELLA_QUERYHIT_HEADER_LENGTH		11
#define GNUTELLA_QUERYHIT_COUNT_OFFSET		0
#define GNUTELLA_QUERYHIT_PORT_OFFSET		1
#define GNUTELLA_QUERYHIT_IP_OFFSET		3
#define GNUTELLA_QUERYHIT_SPEED_OFFSET		7
#define GNUTELLA_QUERYHIT_FIRST_HIT_OFFSET	11
#define GNUTELLA_QUERYHIT_HIT_INDEX_OFFSET	0
#define GNUTELLA_QUERYHIT_HIT_SIZE_OFFSET	4
#define GNUTELLA_QUERYHIT_END_OF_STRING_LENGTH	2

#define GNUTELLA_PUSH_SERVENT_ID_OFFSET		0
#define GNUTELLA_PUSH_INDEX_OFFSET		16
#define GNUTELLA_PUSH_IP_OFFSET			20
#define GNUTELLA_PUSH_PORT_OFFSET		24

#define GNUTELLA_HEADER_ID_OFFSET		0
#define GNUTELLA_HEADER_PAYLOAD_OFFSET		16
#define GNUTELLA_HEADER_TTL_OFFSET		17
#define GNUTELLA_HEADER_HOPS_OFFSET		18
#define GNUTELLA_HEADER_SIZE_OFFSET		19

static void dissect_gnutella_pong(tvbuff_t *tvb, unsigned offset, proto_tree *tree) {

	proto_tree_add_item(tree,
		hf_gnutella_pong_port,
		tvb,
		offset + GNUTELLA_PONG_PORT_OFFSET,
		GNUTELLA_PORT_LENGTH,
		ENC_LITTLE_ENDIAN);

	proto_tree_add_item(tree,
		hf_gnutella_pong_ip,
		tvb,
		offset + GNUTELLA_PONG_IP_OFFSET,
		GNUTELLA_IP_LENGTH,
		ENC_BIG_ENDIAN);

	proto_tree_add_item(tree,
		hf_gnutella_pong_files,
		tvb,
		offset + GNUTELLA_PONG_FILES_OFFSET,
		GNUTELLA_LONG_LENGTH,
		ENC_LITTLE_ENDIAN);

	proto_tree_add_item(tree,
		hf_gnutella_pong_kbytes,
		tvb,
		offset + GNUTELLA_PONG_KBYTES_OFFSET,
		GNUTELLA_LONG_LENGTH,
		ENC_LITTLE_ENDIAN);

}

static void dissect_gnutella_query(tvbuff_t *tvb, unsigned offset, proto_tree *tree, unsigned size) {

	proto_tree_add_item(tree,
		hf_gnutella_query_min_speed,
		tvb,
		offset + GNUTELLA_QUERY_SPEED_OFFSET,
		GNUTELLA_SHORT_LENGTH,
		ENC_LITTLE_ENDIAN);

	if (size > GNUTELLA_SHORT_LENGTH) {
		/*
		 * XXX - the 0.4 spec says, of this field:
		 *
		 *   It SHOULD use an ASCII-compatible encoding and
		 *   charset. In this version of the protocol, no
		 *   encoding was specified, but most servents use
		 *   the ISO-8859-1 character set, but other encodings
		 *   such as UTF-8 MAY also be used (possibly in
		 *   conjonction with Query Data), as well as other
		 *   international character sets (ISO-8859-*, KOI-8,
		 *   S-JIS, Big5, ...).
		 *
		 * No obvious mechanism is provided to indicate what
		 * encoding is being used; perhaps this should be
		 * made a preference, defaulting to ISO 8859-1.
		 */
		proto_tree_add_item(tree,
			hf_gnutella_query_search,
			tvb,
			offset + GNUTELLA_QUERY_SEARCH_OFFSET,
			size - GNUTELLA_SHORT_LENGTH,
			ENC_ASCII);
	}
	else {
		proto_tree_add_string_format(tree,
			hf_gnutella_query_search, tvb,
			offset + GNUTELLA_QUERY_SEARCH_OFFSET,
			0, "", "Missing data for Query Search.");
	}
}

static void dissect_gnutella_queryhit(tvbuff_t *tvb, unsigned offset, proto_tree *tree, unsigned size) {

	proto_tree *qhi, *hit_tree;
	int hit_count, i;
	int hit_offset;
	int name_length, extra_length;
	int idx_at_offset, size_at_offset;
	int servent_id_at_offset;
	int name_at_offset, extra_at_offset;
	int cur_char, remaining, used;

	hit_count = tvb_get_uint8(tvb, offset + GNUTELLA_QUERYHIT_COUNT_OFFSET);

	proto_tree_add_uint(tree,
		hf_gnutella_queryhit_count,
		tvb,
		offset + GNUTELLA_QUERYHIT_COUNT_OFFSET,
		GNUTELLA_BYTE_LENGTH,
		hit_count);

	proto_tree_add_item(tree,
		hf_gnutella_queryhit_port,
		tvb,
		offset + GNUTELLA_QUERYHIT_PORT_OFFSET,
		GNUTELLA_PORT_LENGTH,
		ENC_LITTLE_ENDIAN);

	proto_tree_add_item(tree,
		hf_gnutella_queryhit_ip,
		tvb,
		offset + GNUTELLA_QUERYHIT_IP_OFFSET,
		GNUTELLA_IP_LENGTH,
		ENC_BIG_ENDIAN);

	proto_tree_add_item(tree,
		hf_gnutella_queryhit_speed,
		tvb,
		offset + GNUTELLA_QUERYHIT_SPEED_OFFSET,
		GNUTELLA_LONG_LENGTH,
		ENC_LITTLE_ENDIAN);

	hit_offset = offset + GNUTELLA_QUERYHIT_FIRST_HIT_OFFSET;

	for(i = 0; i < hit_count; i++) {
		idx_at_offset  = hit_offset;
		size_at_offset = hit_offset + GNUTELLA_QUERYHIT_HIT_SIZE_OFFSET;

		hit_offset += (GNUTELLA_LONG_LENGTH * 2);

		name_length  = 0;
		extra_length = 0;

		name_at_offset = hit_offset;

		while(hit_offset - offset < size) {
			cur_char = tvb_get_uint8(tvb, hit_offset);
			if(cur_char == '\0')
				break;

			hit_offset++;
			name_length++;
		}

		hit_offset++;

		extra_at_offset = hit_offset;

		while(hit_offset - offset < size) {
			cur_char = tvb_get_uint8(tvb, hit_offset);
			if(cur_char == '\0')
				break;

			hit_offset++;
			extra_length++;
		}

		hit_offset++;

		qhi = proto_tree_add_item(tree,
			hf_gnutella_queryhit_hit,
			tvb,
			idx_at_offset,
			(GNUTELLA_LONG_LENGTH * 2) +
			name_length + extra_length +
			GNUTELLA_QUERYHIT_END_OF_STRING_LENGTH,
			ENC_NA);

		hit_tree = proto_item_add_subtree(qhi, ett_gnutella);

		proto_tree_add_item(hit_tree,
			hf_gnutella_queryhit_hit_index,
			tvb,
			idx_at_offset,
			GNUTELLA_LONG_LENGTH,
			ENC_LITTLE_ENDIAN);

		proto_tree_add_item(hit_tree,
			hf_gnutella_queryhit_hit_size,
			tvb,
			size_at_offset,
			GNUTELLA_LONG_LENGTH,
			ENC_LITTLE_ENDIAN);

		proto_tree_add_item(hit_tree,
			hf_gnutella_queryhit_hit_name,
			tvb,
			name_at_offset,
			name_length,
			ENC_ASCII);

		if(extra_length) {
			proto_tree_add_item(hit_tree,
				hf_gnutella_queryhit_hit_extra,
				tvb,
				extra_at_offset,
				extra_length,
				ENC_NA);
		}
	}

	used = hit_offset - offset;
	remaining = size - used;

	if(remaining > GNUTELLA_SERVENT_ID_LENGTH) {
		servent_id_at_offset = hit_offset + remaining - GNUTELLA_SERVENT_ID_LENGTH;

		proto_tree_add_item(tree,
			hf_gnutella_queryhit_extra,
			tvb,
			hit_offset,
			servent_id_at_offset - hit_offset,
			ENC_NA);
	}
	else {
		servent_id_at_offset = hit_offset;
	}

	proto_tree_add_item(tree,
		hf_gnutella_queryhit_servent_id,
		tvb,
		servent_id_at_offset,
		GNUTELLA_SERVENT_ID_LENGTH,
		ENC_NA);

}

static void dissect_gnutella_push(tvbuff_t *tvb, unsigned offset, proto_tree *tree) {

	proto_tree_add_item(tree,
		hf_gnutella_push_servent_id,
		tvb,
		offset + GNUTELLA_PUSH_SERVENT_ID_OFFSET,
		GNUTELLA_SERVENT_ID_LENGTH,
		ENC_NA);

	proto_tree_add_item(tree,
		hf_gnutella_push_index,
		tvb,
		offset + GNUTELLA_PUSH_INDEX_OFFSET,
		GNUTELLA_LONG_LENGTH,
		ENC_LITTLE_ENDIAN);

	proto_tree_add_item(tree,
		hf_gnutella_push_ip,
		tvb,
		offset + GNUTELLA_PUSH_IP_OFFSET,
		GNUTELLA_IP_LENGTH,
		ENC_BIG_ENDIAN);

	proto_tree_add_item(tree,
		hf_gnutella_push_port,
		tvb,
		offset + GNUTELLA_PUSH_PORT_OFFSET,
		GNUTELLA_PORT_LENGTH,
		ENC_LITTLE_ENDIAN);

}

static unsigned
get_gnutella_pdu_len(packet_info *pinfo _U_, tvbuff_t *tvb,
                     int offset, void *data _U_)
{
	uint32_t size;

	size = tvb_get_letohl(
		tvb,
		offset + GNUTELLA_HEADER_SIZE_OFFSET);
	if (size > GNUTELLA_MAX_SNAP_SIZE) {
		/*
		 * XXX - arbitrary limit, preventing overflows and
		 * attempts to reassemble 4GB of data.
		 */
		size = GNUTELLA_MAX_SNAP_SIZE;
	}

	/* The size doesn't include the header */
	return GNUTELLA_HEADER_LENGTH + size;
}

static int dissect_gnutella_pdu(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_) {

	proto_item *ti, *hi, *pi;
	proto_tree *gnutella_tree = NULL;
	proto_tree *gnutella_header_tree, *gnutella_pong_tree;
	proto_tree *gnutella_queryhit_tree, *gnutella_push_tree;
	proto_tree *gnutella_query_tree;
	uint8_t payload_descriptor;
	uint32_t size = 0;
	const char *payload_descriptor_text;

	if (tree) {
		ti = proto_tree_add_item(tree,
			proto_gnutella,
			tvb,
			0,
			-1,
			ENC_NA);
		gnutella_tree = proto_item_add_subtree(ti, ett_gnutella);

		size = tvb_get_letohl(
			tvb,
			GNUTELLA_HEADER_SIZE_OFFSET);
	}

	payload_descriptor = tvb_get_uint8(
		tvb,
		GNUTELLA_HEADER_PAYLOAD_OFFSET);

	switch(payload_descriptor) {
		case GNUTELLA_PING:
			payload_descriptor_text = GNUTELLA_PING_NAME;
			break;
		case GNUTELLA_PONG:
			payload_descriptor_text = GNUTELLA_PONG_NAME;
			break;
		case GNUTELLA_PUSH:
			payload_descriptor_text = GNUTELLA_PUSH_NAME;
			break;
		case GNUTELLA_QUERY:
			payload_descriptor_text = GNUTELLA_QUERY_NAME;
			break;
		case GNUTELLA_QUERYHIT:
			payload_descriptor_text = GNUTELLA_QUERYHIT_NAME;
			break;
		default:
			payload_descriptor_text = GNUTELLA_UNKNOWN_NAME;
			break;
	}

	col_append_sep_str(pinfo->cinfo, COL_INFO, NULL,
		    payload_descriptor_text);

	if (tree) {
		hi = proto_tree_add_item(gnutella_tree,
			hf_gnutella_header,
			tvb,
			0,
			GNUTELLA_HEADER_LENGTH,
			ENC_NA);
		gnutella_header_tree = proto_item_add_subtree(hi, ett_gnutella);

		proto_tree_add_item(gnutella_header_tree,
			hf_gnutella_header_id,
			tvb,
			GNUTELLA_HEADER_ID_OFFSET,
			GNUTELLA_SERVENT_ID_LENGTH,
			ENC_NA);

		proto_tree_add_uint_format_value(gnutella_header_tree,
			hf_gnutella_header_payload,
			tvb,
			GNUTELLA_HEADER_PAYLOAD_OFFSET,
			GNUTELLA_BYTE_LENGTH,
			payload_descriptor,
			"%i (%s)",
			payload_descriptor,
			payload_descriptor_text);

		proto_tree_add_item(gnutella_header_tree,
			hf_gnutella_header_ttl,
			tvb,
			GNUTELLA_HEADER_TTL_OFFSET,
			GNUTELLA_BYTE_LENGTH,
			ENC_BIG_ENDIAN);

		proto_tree_add_item(gnutella_header_tree,
			hf_gnutella_header_hops,
			tvb,
			GNUTELLA_HEADER_HOPS_OFFSET,
			GNUTELLA_BYTE_LENGTH,
			ENC_BIG_ENDIAN);

		proto_tree_add_uint(gnutella_header_tree,
			hf_gnutella_header_size,
			tvb,
			GNUTELLA_HEADER_SIZE_OFFSET,
			GNUTELLA_LONG_LENGTH,
			size);

		if (size > 0) {
			switch(payload_descriptor) {
			case GNUTELLA_PONG:
				pi = proto_tree_add_item(
					gnutella_header_tree,
					hf_gnutella_pong_payload,
					tvb,
					GNUTELLA_HEADER_LENGTH,
					size,
					ENC_NA);
				gnutella_pong_tree = proto_item_add_subtree(
					pi,
					ett_gnutella);
				dissect_gnutella_pong(
					tvb,
					GNUTELLA_HEADER_LENGTH,
					gnutella_pong_tree);
				break;
			case GNUTELLA_PUSH:
				pi = proto_tree_add_item(
					gnutella_header_tree,
					hf_gnutella_push_payload,
					tvb,
					GNUTELLA_HEADER_LENGTH,
					size,
					ENC_NA);
				gnutella_push_tree = proto_item_add_subtree(
					pi,
					ett_gnutella);
				dissect_gnutella_push(
					tvb,
					GNUTELLA_HEADER_LENGTH,
					gnutella_push_tree);
				break;
			case GNUTELLA_QUERY:
				pi = proto_tree_add_item(
					gnutella_header_tree,
					hf_gnutella_query_payload,
					tvb,
					GNUTELLA_HEADER_LENGTH,
					size,
					ENC_NA);
				gnutella_query_tree = proto_item_add_subtree(
					pi,
					ett_gnutella);
				dissect_gnutella_query(
					tvb,
					GNUTELLA_HEADER_LENGTH,
					gnutella_query_tree,
					size);
				break;
			case GNUTELLA_QUERYHIT:
				pi = proto_tree_add_item(
					gnutella_header_tree,
					hf_gnutella_queryhit_payload,
					tvb,
					GNUTELLA_HEADER_LENGTH,
					size,
					ENC_NA);
				gnutella_queryhit_tree = proto_item_add_subtree(
					pi,
					ett_gnutella);
				dissect_gnutella_queryhit(
					tvb,
					GNUTELLA_HEADER_LENGTH,
					gnutella_queryhit_tree,
					size);
				break;
			}
		}
	}

	return tvb_captured_length(tvb);
}


static int dissect_gnutella(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data) {

	proto_item *ti;
	proto_tree *gnutella_tree = NULL;
	uint32_t size;

	col_set_str(pinfo->cinfo, COL_PROTOCOL, "Gnutella");

	col_clear(pinfo->cinfo, COL_INFO);

	/*
	 * OK, do we have enough data to determine whether this
	 * is Gnutella messages or just a transfer stream?
	 */
	if (tvb_bytes_exist(tvb, GNUTELLA_HEADER_SIZE_OFFSET, 4)) {
		/*
		 * Yes - fetch the length and see if it's bigger
		 * than GNUTELLA_MAX_SNAP_SIZE; if it is, we assume
		 * it's a transfer stream.
		 *
		 * Should we also check the payload descriptor?
		 */
		size = tvb_get_letohl(
			tvb,
			GNUTELLA_HEADER_SIZE_OFFSET);
		if (size > GNUTELLA_MAX_SNAP_SIZE) {
			if (tree) {
				ti = proto_tree_add_item(tree,
					proto_gnutella,
					tvb,
					0,
					-1,
					ENC_NA);
				gnutella_tree = proto_item_add_subtree(ti,
					ett_gnutella);

				proto_tree_add_item(gnutella_tree,
					hf_gnutella_stream,
					tvb,
					0,
					-1,
					ENC_NA);
			}
			return tvb_captured_length(tvb);
		}
	}

	tcp_dissect_pdus(tvb, pinfo, tree, true, GNUTELLA_HEADER_SIZE_OFFSET+4,
	    get_gnutella_pdu_len, dissect_gnutella_pdu, data);
	return tvb_captured_length(tvb);
}

void proto_register_gnutella(void) {

	static hf_register_info hf[] = {
		{ &hf_gnutella_header,
			{ "Descriptor Header", "gnutella.header",
			FT_NONE, BASE_NONE, NULL, 0,
			"Gnutella Descriptor Header", HFILL }
		},
		{ &hf_gnutella_pong_payload,
			{ "Pong", "gnutella.pong.payload",
			FT_NONE, BASE_NONE, NULL, 0,
			"Gnutella Pong Payload", HFILL }
		},
		{ &hf_gnutella_push_payload,
			{ "Push", "gnutella.push.payload",
			FT_NONE, BASE_NONE, NULL, 0,
			"Gnutella Push Payload", HFILL }
		},
		{ &hf_gnutella_query_payload,
			{ "Query", "gnutella.query.payload",
			FT_NONE, BASE_NONE, NULL, 0,
			"Gnutella Query Payload", HFILL }
		},
		{ &hf_gnutella_queryhit_payload,
			{ "QueryHit", "gnutella.queryhit.payload",
			FT_NONE, BASE_NONE, NULL, 0,
			"Gnutella QueryHit Payload", HFILL }
		},
		{ &hf_gnutella_stream,
			{ "Gnutella Upload / Download Stream", "gnutella.stream",
			FT_NONE, BASE_NONE, NULL, 0,
			NULL, HFILL }
		},
		{ &hf_gnutella_header_id,
			{ "ID", "gnutella.header.id",
			FT_BYTES, BASE_NONE, NULL, 0,
			"Gnutella Descriptor ID", HFILL }
		},
		{ &hf_gnutella_header_payload,
			{ "Payload", "gnutella.header.payload",
			FT_UINT8, BASE_DEC, NULL, 0,
			"Gnutella Descriptor Payload", HFILL }
		},
		{ &hf_gnutella_header_ttl,
			{ "TTL", "gnutella.header.ttl",
			FT_UINT8, BASE_DEC, NULL, 0,
			"Gnutella Descriptor Time To Live", HFILL }
		},
		{ &hf_gnutella_header_hops,
			{ "Hops", "gnutella.header.hops",
			FT_UINT8, BASE_DEC, NULL, 0,
			"Gnutella Descriptor Hop Count", HFILL }
		},
		{ &hf_gnutella_header_size,
			{ "Length", "gnutella.header.size",
			FT_UINT8, BASE_DEC, NULL, 0,
			"Gnutella Descriptor Payload Length", HFILL }
		},
		{ &hf_gnutella_pong_port,
			{ "Port", "gnutella.pong.port",
			FT_UINT16, BASE_PT_TCP, NULL, 0,
			"Gnutella Pong TCP Port", HFILL }
		},
		{ &hf_gnutella_pong_ip,
			{ "IP", "gnutella.pong.ip",
			FT_IPv4, BASE_NONE, NULL, 0,
			"Gnutella Pong IP Address", HFILL }
		},
		{ &hf_gnutella_pong_files,
			{ "Files Shared", "gnutella.pong.files",
			FT_UINT32, BASE_DEC, NULL, 0,
			"Gnutella Pong Files Shared", HFILL }
		},
		{ &hf_gnutella_pong_kbytes,
			{ "KBytes Shared", "gnutella.pong.kbytes",
			FT_UINT32, BASE_DEC, NULL, 0,
			"Gnutella Pong KBytes Shared", HFILL }
		},
		{ &hf_gnutella_query_min_speed,
			{ "Min Speed", "gnutella.query.min_speed",
			FT_UINT32, BASE_DEC, NULL, 0,
			"Gnutella Query Minimum Speed", HFILL }
		},
		{ &hf_gnutella_query_search,
			{ "Search", "gnutella.query.search",
			FT_STRINGZ, BASE_NONE, NULL, 0,
			"Gnutella Query Search", HFILL }
		},
		{ &hf_gnutella_queryhit_hit,
			{ "Hit", "gnutella.queryhit.hit",
			FT_NONE, BASE_NONE, NULL, 0,
			"Gnutella QueryHit", HFILL }
		},
		{ &hf_gnutella_queryhit_hit_index,
			{ "Index", "gnutella.queryhit.hit.index",
			FT_UINT32, BASE_DEC, NULL, 0,
			"Gnutella QueryHit Index", HFILL }
		},
		{ &hf_gnutella_queryhit_hit_size,
			{ "Size", "gnutella.queryhit.hit.size",
			FT_UINT32, BASE_DEC, NULL, 0,
			"Gnutella QueryHit Size", HFILL }
		},
		{ &hf_gnutella_queryhit_hit_name,
			{ "Name", "gnutella.queryhit.hit.name",
			FT_STRING, BASE_NONE, NULL, 0,
			"Gnutella Query Name", HFILL }
		},
		{ &hf_gnutella_queryhit_hit_extra,
			{ "Extra", "gnutella.queryhit.hit.extra",
			FT_BYTES, BASE_NONE, NULL, 0,
			"Gnutella Query Extra", HFILL }
		},
		{ &hf_gnutella_queryhit_count,
			{ "Count", "gnutella.queryhit.count",
			FT_UINT8, BASE_DEC, NULL, 0,
			"Gnutella QueryHit Count", HFILL }
		},
		{ &hf_gnutella_queryhit_port,
			{ "Port", "gnutella.queryhit.port",
			FT_UINT16, BASE_PT_TCP, NULL, 0,
			"Gnutella QueryHit Port", HFILL }
		},
		{ &hf_gnutella_queryhit_ip,
			{ "IP", "gnutella.queryhit.ip",
			FT_IPv4, BASE_NONE, NULL, 0,
			"Gnutella QueryHit IP Address", HFILL }
		},
		{ &hf_gnutella_queryhit_speed,
			{ "Speed", "gnutella.queryhit.speed",
			FT_UINT32, BASE_DEC, NULL, 0,
			"Gnutella QueryHit Speed", HFILL }
		},
		{ &hf_gnutella_queryhit_extra,
			{ "Extra", "gnutella.queryhit.extra",
			FT_BYTES, BASE_NONE, NULL, 0,
			"Gnutella QueryHit Extra", HFILL }
		},
		{ &hf_gnutella_queryhit_servent_id,
			{ "Servent ID", "gnutella.queryhit.servent_id",
			FT_BYTES, BASE_NONE, NULL, 0,
			"Gnutella QueryHit Servent ID", HFILL }
		},
		{ &hf_gnutella_push_servent_id,
			{ "Servent ID", "gnutella.push.servent_id",
			FT_BYTES, BASE_NONE, NULL, 0,
			"Gnutella Push Servent ID", HFILL }
		},
		{ &hf_gnutella_push_ip,
			{ "IP", "gnutella.push.ip",
			FT_IPv4, BASE_NONE, NULL, 0,
			"Gnutella Push IP Address", HFILL }
		},
		{ &hf_gnutella_push_index,
			{ "Index", "gnutella.push.index",
			FT_UINT32, BASE_DEC, NULL, 0,
			"Gnutella Push Index", HFILL }
		},
		{ &hf_gnutella_push_port,
			{ "Port", "gnutella.push.port",
			FT_UINT16, BASE_DEC, NULL, 0,
			"Gnutella Push Port", HFILL }
		},
	};

	static int *ett[] = {
		&ett_gnutella,
	};

	proto_gnutella = proto_register_protocol("Gnutella Protocol", "GNUTELLA", "gnutella");

	proto_register_field_array(proto_gnutella, hf, array_length(hf));

	proto_register_subtree_array(ett, array_length(ett));

	gnutella_handle = register_dissector("gnutella", dissect_gnutella, proto_gnutella);
}

void proto_reg_handoff_gnutella(void) {
	dissector_add_uint_with_preference("tcp.port", GNUTELLA_TCP_PORT, gnutella_handle);
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
