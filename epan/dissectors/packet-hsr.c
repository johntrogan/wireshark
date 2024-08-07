/* packet-hsr.c
 * Routines for HSR dissection (IEC62439 Part 3)
 * Copyright 2009, Florian Reichert <refl[AT]zhaw.ch>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald[AT]wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/etypes.h>

void proto_register_hsr(void);
void proto_reg_handoff_hsr(void);

static dissector_handle_t hsr_frame_handle;

/**********************************************************/
/* Lengths of fields within a HSR packet.                 */
/**********************************************************/
#define HSR_LSDU_PATH_LENGTH                 2
#define HSR_SEQUENZNR_LENGTH                 2

#define HSR_TOTAL_LENGTH                     4

/**********************************************************/
/* Offsets of fields within a HSR packet.                 */
/**********************************************************/
#define HSR_PATH_OFFSET                      0
#define HSR_SEQUENZNR_OFFSET                 2

static const value_string hsr_laneid_vals[] = {
    {0, "Lane A"},
    {1, "Lane B"},
    {0, NULL}
};

/**********************************************************/
/* Initialize the protocol and registered fields          */
/**********************************************************/

static int proto_hsr;

/* Initialize supervision frame fields */


static int hf_hsr_path;
static int hf_hsr_netid;
static int hf_hsr_laneid;
static int hf_hsr_lsdu_size;
static int hf_hsr_sequence_nr;
static int hf_type;

static dissector_table_t ethertype_subdissector_table;
/* Initialize the subtree pointers */
static int ett_hsr_frame;

/* Code to actually dissect the packets */
static int
dissect_hsr_frame(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    proto_item *ti;
    proto_tree *hsr_tree;
    tvbuff_t *next_tvb;
    uint16_t etype;
    uint16_t lsdu_size, lsdu_size_correct;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "HSR");

    col_set_str(pinfo->cinfo, COL_INFO, "HSR-Data Frame");

    /* create display subtree for the protocol */

    ti = proto_tree_add_item(tree, proto_hsr, tvb, 0, HSR_TOTAL_LENGTH, ENC_NA);

    hsr_tree = proto_item_add_subtree(ti, ett_hsr_frame);

    proto_tree_add_item(hsr_tree, hf_hsr_path,
                        tvb, HSR_PATH_OFFSET, HSR_LSDU_PATH_LENGTH , ENC_BIG_ENDIAN);

    proto_tree_add_item(hsr_tree, hf_hsr_netid,
                        tvb, HSR_PATH_OFFSET, HSR_LSDU_PATH_LENGTH , ENC_BIG_ENDIAN);

    proto_tree_add_item(hsr_tree, hf_hsr_laneid,
                        tvb, HSR_PATH_OFFSET, HSR_LSDU_PATH_LENGTH , ENC_BIG_ENDIAN);


    lsdu_size = tvb_get_ntohs(tvb, HSR_PATH_OFFSET) & 0x0fff;
    lsdu_size_correct = tvb_reported_length_remaining(tvb, 0);
    if (lsdu_size == lsdu_size_correct) {
        proto_tree_add_uint_format_value(hsr_tree, hf_hsr_lsdu_size,
                                   tvb, HSR_PATH_OFFSET, HSR_LSDU_PATH_LENGTH, lsdu_size,
                                   "%d [correct]", lsdu_size);
    } else {
        proto_tree_add_uint_format_value(hsr_tree, hf_hsr_lsdu_size,
                                   tvb, HSR_PATH_OFFSET, HSR_LSDU_PATH_LENGTH, lsdu_size,
                                   "%d [WRONG, should be %d]", lsdu_size, lsdu_size_correct);
    }

    proto_tree_add_item(hsr_tree, hf_hsr_sequence_nr,
                        tvb, HSR_SEQUENZNR_OFFSET,HSR_SEQUENZNR_LENGTH, ENC_BIG_ENDIAN);

    proto_tree_add_item(hsr_tree, hf_type,
                        tvb, HSR_TOTAL_LENGTH,2, ENC_BIG_ENDIAN);

    next_tvb = tvb_new_subset_remaining(tvb, HSR_TOTAL_LENGTH + 2);

    etype = tvb_get_ntohs(tvb, HSR_TOTAL_LENGTH);

    if (!dissector_try_uint(ethertype_subdissector_table, etype, next_tvb, pinfo, tree))
        call_data_dissector(next_tvb, pinfo, hsr_tree);

    return tvb_captured_length(tvb);
}



/* Register the protocol with Wireshark */
void proto_register_hsr(void)
{
    static hf_register_info hf[] = {
        { &hf_hsr_path,
          { "Path", "hsr.path",
            FT_UINT16, BASE_DEC, NULL, 0xf000,
            NULL, HFILL }
        },

        { &hf_hsr_netid,
          { "Network id", "hsr.netid",
            FT_UINT16, BASE_DEC, NULL, 0xe000,
            NULL, HFILL }
        },

        { &hf_hsr_laneid,
          { "Lane id", "hsr.laneid",
            FT_UINT16, BASE_DEC, VALS(hsr_laneid_vals), 0x1000,
            NULL, HFILL }
        },

        { &hf_hsr_lsdu_size,
          { "LSDU size", "hsr.lsdu_size",
            FT_UINT16, BASE_DEC, NULL, 0x0fff,
            NULL, HFILL }
        },

        { &hf_hsr_sequence_nr,
          { "Sequence number", "hsr.sequence_nr",
            FT_UINT16, BASE_DEC, NULL, 0x00,
            NULL, HFILL }
        },
        { &hf_type,
          { "Type", "hsr.type",
            FT_UINT16, BASE_HEX, VALS(etype_vals), 0x00,
            NULL, HFILL }
        }

    };

    static int *ett[] = {
        &ett_hsr_frame,
    };

    /* Register the protocol name and description */
    proto_hsr = proto_register_protocol("High-availability Seamless Redundancy (IEC62439 Part 3 Chapter 5)",
                                        "HSR", "hsr");

    /* Required function calls to register the header fields and subtree used */
    proto_register_field_array(proto_hsr, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));

    hsr_frame_handle = register_dissector("hsr", dissect_hsr_frame, proto_hsr);
}


void proto_reg_handoff_hsr(void)
{
    dissector_add_uint("ethertype", ETHERTYPE_HSR, hsr_frame_handle);

    ethertype_subdissector_table = find_dissector_table("ethertype");
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
