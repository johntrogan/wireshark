/* packet-lapb.c
 * Routines for lapb frame disassembly
 * Olivier Abad <oabad@noos.fr>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <wiretap/wtap.h>
#include <epan/xdlc.h>
#include <epan/tfs.h>
#include <wsutil/array.h>

void proto_register_lapb(void);
void proto_reg_handoff_lapb(void);

static int proto_lapb;
static int hf_lapb_address;
static int hf_lapb_control;
static int hf_lapb_n_r;
static int hf_lapb_n_s;
static int hf_lapb_p;
static int hf_lapb_f;
static int hf_lapb_s_ftype;
static int hf_lapb_u_modifier_cmd;
static int hf_lapb_u_modifier_resp;
static int hf_lapb_ftype_i;
static int hf_lapb_ftype_s_u;

static int ett_lapb;
static int ett_lapb_control;

static dissector_handle_t x25_dir_handle;
static dissector_handle_t x25_handle;
static dissector_handle_t lapb_handle;

static const xdlc_cf_items lapb_cf_items = {
    &hf_lapb_n_r,
    &hf_lapb_n_s,
    &hf_lapb_p,
    &hf_lapb_f,
    &hf_lapb_s_ftype,
    &hf_lapb_u_modifier_cmd,
    &hf_lapb_u_modifier_resp,
    &hf_lapb_ftype_i,
    &hf_lapb_ftype_s_u
};

static int
dissect_lapb(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    proto_tree          *lapb_tree, *ti;
    uint16_t            control;
    int                 is_response;
    uint8_t             byte0;
    tvbuff_t            *next_tvb;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "LAPB");
    col_clear(pinfo->cinfo, COL_INFO);

    switch (pinfo->p2p_dir) {

    case P2P_DIR_SENT:
        col_set_str(pinfo->cinfo, COL_RES_DL_SRC, "DTE");
        col_set_str(pinfo->cinfo, COL_RES_DL_DST, "DCE");
        break;

    case P2P_DIR_RECV:
        col_set_str(pinfo->cinfo, COL_RES_DL_SRC, "DCE");
        col_set_str(pinfo->cinfo, COL_RES_DL_DST, "DTE");
        break;

    default:
        col_set_str(pinfo->cinfo, COL_RES_DL_SRC, "N/A");
        col_set_str(pinfo->cinfo, COL_RES_DL_DST, "N/A");
        break;
    }

    byte0 = tvb_get_uint8(tvb, 0);

    if (byte0 != 0x01 && byte0 != 0x03 && byte0 != 0x07 && byte0 != 0x0f) /* invalid LAPB frame */
    {
        col_set_str(pinfo->cinfo, COL_INFO, "Invalid LAPB frame");
        if (tree)
            proto_tree_add_protocol_format(tree, proto_lapb, tvb, 0, -1,
                            "Invalid LAPB frame");
        return 1;
    }

    switch (pinfo->p2p_dir) {

    case P2P_DIR_SENT:
        if (byte0 == 0x03)
            is_response = true;
        else
            is_response = false;
        break;

    case P2P_DIR_RECV:
        if (byte0 == 0x01)
            is_response = true;
        else
            is_response = false;
        break;

    default:
        /*
         * XXX - should we base this on the source and destination
         * addresses?  The problem is that we can tell one direction
         * from another with that, but we can't say which is DTE->DCE
         * and which is DCE->DTE.
         */
        is_response = false;
        break;
    }

    ti = proto_tree_add_protocol_format(tree, proto_lapb, tvb, 0, 2,
                                        "LAPB");
    lapb_tree = proto_item_add_subtree(ti, ett_lapb);
    proto_tree_add_uint(lapb_tree, hf_lapb_address, tvb, 0, 1, byte0);

    control = dissect_xdlc_control(tvb, 1, pinfo, lapb_tree, hf_lapb_control,
            ett_lapb_control, &lapb_cf_items, NULL, NULL, NULL,
            is_response, false, false);

    /* information frame ==> X.25 */
    if (XDLC_IS_INFORMATION(control)) {
        next_tvb = tvb_new_subset_remaining(tvb, 2);
        switch (pinfo->p2p_dir) {

        case P2P_DIR_SENT:
        case P2P_DIR_RECV:
            call_dissector(x25_dir_handle, next_tvb, pinfo, tree);
            break;

        default:
            call_dissector(x25_handle, next_tvb, pinfo, tree);
            break;
        }
    }
    return tvb_captured_length(tvb);
}

void
proto_register_lapb(void)
{
    static hf_register_info hf[] = {
        { &hf_lapb_address,
          { "Address", "lapb.address", FT_UINT8, BASE_HEX, NULL, 0x0,
            NULL, HFILL }},

        { &hf_lapb_control,
          { "Control Field", "lapb.control", FT_UINT8, BASE_HEX, NULL, 0x0,
            NULL, HFILL }},

        { &hf_lapb_n_r,
          { "N(R)", "lapb.control.n_r", FT_UINT8, BASE_DEC,
            NULL, XDLC_N_R_MASK, NULL, HFILL }},

        { &hf_lapb_n_s,
          { "N(S)", "lapb.control.n_s", FT_UINT8, BASE_DEC,
            NULL, XDLC_N_S_MASK, NULL, HFILL }},

        { &hf_lapb_p,
          { "Poll", "lapb.control.p", FT_BOOLEAN, 8,
            TFS(&tfs_set_notset), XDLC_P_F, NULL, HFILL }},

        { &hf_lapb_f,
          { "Final", "lapb.control.f", FT_BOOLEAN, 8,
            TFS(&tfs_set_notset), XDLC_P_F, NULL, HFILL }},

        { &hf_lapb_s_ftype,
          { "Supervisory frame type", "lapb.control.s_ftype", FT_UINT8, BASE_HEX,
            VALS(stype_vals), XDLC_S_FTYPE_MASK, NULL, HFILL }},

        { &hf_lapb_u_modifier_cmd,
          { "Command", "lapb.control.u_modifier_cmd", FT_UINT8, BASE_HEX,
            VALS(modifier_vals_cmd), XDLC_U_MODIFIER_MASK, NULL, HFILL }},

        { &hf_lapb_u_modifier_resp,
          { "Response", "lapb.control.u_modifier_resp", FT_UINT8, BASE_HEX,
            VALS(modifier_vals_resp), XDLC_U_MODIFIER_MASK, NULL, HFILL }},

        { &hf_lapb_ftype_i,
          { "Frame type", "lapb.control.ftype", FT_UINT8, BASE_HEX,
            VALS(ftype_vals), XDLC_I_MASK, NULL, HFILL }},

        { &hf_lapb_ftype_s_u,
          { "Frame type", "lapb.control.ftype", FT_UINT8, BASE_HEX,
            VALS(ftype_vals), XDLC_S_U_MASK, NULL, HFILL }},
    };
    static int *ett[] = {
        &ett_lapb,
        &ett_lapb_control,
    };

    proto_lapb = proto_register_protocol("Link Access Procedure Balanced (LAPB)",
                                         "LAPB", "lapb");
    proto_register_field_array (proto_lapb, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));

    lapb_handle = register_dissector("lapb", dissect_lapb, proto_lapb);
}

void
proto_reg_handoff_lapb(void)
{
    /*
     * Get handles for the X.25 dissectors; we don't get an X.25
     * pseudo-header for LAPB-over-Ethernet, but we do get it
     * for raw LAPB.
     */
    x25_dir_handle = find_dissector_add_dependency("x.25_dir", proto_lapb);
    x25_handle = find_dissector_add_dependency("x.25", proto_lapb);

    dissector_add_uint("wtap_encap", WTAP_ENCAP_LAPB, lapb_handle);
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
