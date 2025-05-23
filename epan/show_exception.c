/* show_exception.c
 *
 * Routines to put exception information into the protocol tree
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2000 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define WS_LOG_DOMAIN LOG_DOMAIN_EPAN

#include <epan/packet.h>
#include <epan/exceptions.h>
#include <epan/expert.h>
#include <epan/prefs.h>
#include <epan/prefs-int.h>
#include <epan/show_exception.h>
#include <wsutil/ws_assert.h>
#include <wsutil/array.h>

#include <wsutil/wslog.h>

static int proto_short;
static int proto_dissector_bug;
static int proto_malformed;
static int proto_unreassembled;

static expert_field ei_dissector_bug;
static expert_field ei_malformed_reassembly;
static expert_field ei_malformed;
static expert_field ei_unreassembled;

void
register_show_exception(void)
{
	static ei_register_info ei_dissector_bug_set[] = {
		{ &ei_dissector_bug, { "_ws.dissector_bug.expert", PI_DISSECTOR_BUG, PI_ERROR, "Dissector bug", EXPFILL }},
	};
	static ei_register_info ei_malformed_set[] = {
		{ &ei_malformed_reassembly, { "_ws.malformed.reassembly", PI_MALFORMED, PI_ERROR, "Reassembly error", EXPFILL }},
		{ &ei_malformed, { "_ws.malformed.expert", PI_MALFORMED, PI_ERROR, "Malformed Packet (Exception occurred)", EXPFILL }},
	};
	static ei_register_info ei_unreassembled_set[] = {
		{ &ei_unreassembled, { "_ws.unreassembled.expert", PI_REASSEMBLE, PI_NOTE, "Unreassembled fragment (change preferences to enable reassembly)", EXPFILL }},
	};

	expert_module_t* expert_dissector_bug;
	expert_module_t* expert_malformed;
	expert_module_t* expert_unreassembled;

	proto_short = proto_register_protocol("Short Frame", "Short frame", "_ws.short");
	proto_dissector_bug = proto_register_protocol("Dissector Bug",
	    "Dissector bug", "_ws.dissector_bug");
	proto_malformed = proto_register_protocol("Malformed Packet",
	    "Malformed packet", "_ws.malformed");
	proto_unreassembled = proto_register_protocol(
	    "Unreassembled Fragmented Packet",
	    "Unreassembled fragmented packet", "_ws.unreassembled");

	expert_dissector_bug = expert_register_protocol(proto_dissector_bug);
	expert_register_field_array(expert_dissector_bug, ei_dissector_bug_set, array_length(ei_dissector_bug_set));
	expert_malformed = expert_register_protocol(proto_malformed);
	expert_register_field_array(expert_malformed, ei_malformed_set, array_length(ei_malformed_set));
	expert_unreassembled = expert_register_protocol(proto_unreassembled);
	expert_register_field_array(expert_unreassembled, ei_unreassembled_set, array_length(ei_unreassembled_set));

	/* "Short Frame", "Dissector Bug", "Malformed Packet", and
	   "Unreassembled Fragmented Packet" aren't really protocols,
	   they're error indications; disabling them makes no sense. */
	proto_set_cant_toggle(proto_short);
	proto_set_cant_toggle(proto_dissector_bug);
	proto_set_cant_toggle(proto_malformed);
	proto_set_cant_toggle(proto_unreassembled);
}

void
show_exception(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
	       unsigned long exception, const char *exception_message)
{
	static const char dissector_error_nomsg[] =
		"Dissector writer didn't bother saying what the error was";
	proto_item *item;

	if ((exception == ReportedBoundsError || exception == ContainedBoundsError) && pinfo->fragmented)
		exception = FragmentBoundsError;

	switch (exception) {

	case ScsiBoundsError:
		col_append_str(pinfo->cinfo, COL_INFO, "[SCSI transfer limited due to allocation_length too small]");
		proto_tree_add_protocol_format(tree, proto_short, tvb, 0, 0,
				"SCSI transfer limited due to allocation_length too small: %s truncated]", pinfo->current_proto);
		/* Don't record ScsiBoundsError exceptions as expert events - they merely
		 * reflect a normal SCSI condition.
		 * (any case where it's caused by something else is a bug). */
		break;

	case BoundsError:
		{
		bool display_info = true;
		module_t * frame_module = prefs_find_module("frame");
		if (frame_module != NULL)
		{
			pref_t *display_pref = prefs_find_preference(frame_module, "disable_packet_size_limited_in_summary");
			if (display_pref)
			{
				if (prefs_get_bool_value(display_pref, pref_current))
					display_info = false;
			}
		}

		if (display_info)
			col_append_str(pinfo->cinfo, COL_INFO, "[Packet size limited during capture]");
		proto_tree_add_protocol_format(tree, proto_short, tvb, 0, 0,
				"[Packet size limited during capture: %s truncated]", pinfo->current_proto);
		/* Don't record BoundsError exceptions as expert events - they merely
		 * reflect a capture done with a snapshot length too short to capture
		 * all of the packet
		 * (any case where it's caused by something else is a bug). */
        }
		break;

	case FragmentBoundsError:
		col_append_fstr(pinfo->cinfo, COL_INFO, "[BoundErrorUnreassembled Packet%s]", pinfo->noreassembly_reason);
		item = proto_tree_add_protocol_format(tree, proto_unreassembled,
		    tvb, 0, 0, "[BoundError Unreassembled Packet%s: %s]",
		    pinfo->noreassembly_reason, pinfo->current_proto);
		/* FragmentBoundsError merely reflect dissection done with
		 * reassembly turned off, so add a note to that effect
		 * (any case where it's caused by something else is a bug). */
		expert_add_info(pinfo, item, &ei_unreassembled);
		break;

	case ContainedBoundsError:
		col_append_fstr(pinfo->cinfo, COL_INFO, "[Malformed Packet: length of contained item exceeds length of containing item]");
		item = proto_tree_add_protocol_format(tree, proto_malformed,
		    tvb, 0, 0, "[Malformed Packet: %s: length of contained item exceeds length of containing item]",
		    pinfo->current_proto);
		expert_add_info(pinfo, item, &ei_malformed);
		break;

	case ReportedBoundsError:
		show_reported_bounds_error(tvb, pinfo, tree);
		break;

	case DissectorError:
		col_append_fstr(pinfo->cinfo, COL_INFO,
		    "[Dissector bug, protocol %s: %s]",
		    pinfo->current_proto,
		    exception_message == NULL ?
		        dissector_error_nomsg : exception_message);
		item = proto_tree_add_protocol_format(tree, proto_dissector_bug, tvb, 0, 0,
		    "[Dissector bug, protocol %s: %s]",
		    pinfo->current_proto,
		    exception_message == NULL ?
		        dissector_error_nomsg : exception_message);
		ws_log(WS_LOG_DOMAIN, LOG_LEVEL_WARNING,
		    "Dissector bug, protocol %s, in packet %u: %s",
		    pinfo->current_proto, pinfo->num,
		    exception_message == NULL ?
		        dissector_error_nomsg : exception_message);
		expert_add_info_format(pinfo, item, &ei_dissector_bug, "%s",
		    exception_message == NULL ?
		        dissector_error_nomsg : exception_message);
		break;

	case ReassemblyError:
		col_append_fstr(pinfo->cinfo, COL_INFO,
		    "[Reassembly error, protocol %s: %s]",
		    pinfo->current_proto,
		    exception_message == NULL ?
		        dissector_error_nomsg : exception_message);
		item = proto_tree_add_protocol_format(tree, proto_malformed, tvb, 0, 0,
		    "[Reassembly error, protocol %s: %s]",
		    pinfo->current_proto,
		    exception_message == NULL ?
		        dissector_error_nomsg : exception_message);
		expert_add_info_format(pinfo, item, &ei_malformed_reassembly, "%s",
		    exception_message == NULL ?
		        dissector_error_nomsg : exception_message);
		break;

	default:
		/* XXX - we want to know, if an unknown exception passed until here, don't we? */
		ws_assert_not_reached();
	}
}

void
show_reported_bounds_error(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_item *item;

	col_append_str(pinfo->cinfo, COL_INFO,
	    "[Malformed Packet]");
	item = proto_tree_add_protocol_format(tree, proto_malformed,
	    tvb, 0, 0, "[Malformed Packet: %s]", pinfo->current_proto);
	expert_add_info(pinfo, item, &ei_malformed);
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
