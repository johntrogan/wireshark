/* packet-kingfisher.c
 * Routines for kingfisher packet dissection
 * By Rob Casey 2007
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * Copied from packet-pop.c
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/conversation.h>
#include <epan/expert.h>

void proto_register_kingfisher(void);
void proto_reg_handoff_kingfisher(void);

static dissector_handle_t kingfisher_handle;

#define SUPPORT_KINGFISHER_SERIES_2

#ifdef SUPPORT_KINGFISHER_SERIES_2
#define TCP_PORT_KINGFISHER_RANGE   "473,4058" /* 473 not IANA registered */
#define UDP_PORT_KINGFISHER_RANGE   "473,4058" /* 473 not IANA registered */
#else
#define TCP_PORT_KINGFISHER_RANGE   "4058"
#define UDP_PORT_KINGFISHER_RANGE   "4058"
#endif

static int proto_kingfisher;
static int hf_kingfisher_version;
static int hf_kingfisher_system;
static int hf_kingfisher_length;
static int hf_kingfisher_from;
static int hf_kingfisher_target;
static int hf_kingfisher_via;
static int hf_kingfisher_message;
static int hf_kingfisher_function;
static int hf_kingfisher_checksum;
static int hf_kingfisher_checksum_status;
static int hf_kingfisher_message_data;

static expert_field ei_kingfisher_checksum;

static dissector_handle_t kingfisher_conv_handle;


typedef struct _kingfisher_packet_t
{
    uint8_t     version;
    uint8_t     system_id;
    uint16_t    from;
    uint16_t    target;
    uint16_t    via;
    uint8_t     length;
    uint8_t     message;
    uint8_t     function;
    uint16_t    checksum;
} kingfisher_packet_t;

static int ett_kingfisher;

static const value_string function_code_vals[] =
{
    { 0x00, "Acknowledgement" },
    { 0x01, "Negative Acknowledgement" },
    { 0x02, "No Access" },
    { 0x03, "Message Buffer Full" },
    { 0x0a, "Get Data Frame" },
    { 0x0b, "Send Data Frame" },
    { 0x0c, "Get Data Blocks" },
    { 0x0d, "Send Data Blocks" },
    { 0x0e, "Check RTU Update" },
    { 0x0f, "Send RTU Update" },
    { 0x10, "Get Multiple Data" },
    { 0x11, "Send Multiple Data" },
    { 0x12, "Get Multiple Network Data" },
    { 0x13, "Send Multiple Network Data" },
    { 0x14, "Set Multiple Data" },
    { 0x15, "Get Multiple Data to Local Registers" },
    { 0x16, "Set Data Block" },
    { 0x17, "QSet Multiple Data" },
    { 0x18, "Set Digital Data" },
    { 0x1a, "Request RTU update" },
    { 0x1b, "QSet Digital Data" },
    { 0x1e, "Cold Start" },
    { 0x1f, "Warm Start" },
    { 0x21, "Program Control" },
    { 0x22, "Get RTU Status" },
    { 0x23, "Send RTU Status" },
    { 0x24, "Set RTC" },
    { 0x25, "Swap Master CPU" },
    { 0x26, "Send I/O Module Message" },
    { 0x28, "Get Diagnostic Information" },
    { 0x29, "Send Diagnostic Information" },
    { 0x2b, "Send Pager Information" },
    { 0x2c, "Get Pager Information" },
    { 0x2d, "Send Port Data Information" },
    { 0x2e, "Get Port Data Information" },
    { 0x2f, "Send RTU Data Information" },
    { 0x30, "Get RTU Data Information" },
    { 0x31, "Unlock Port" },
    { 0x33, "Carrier Test" },
    { 0x34, "Program Flash RAM" },
    { 0x35, "Get I/O Values" },
    { 0x36, "Send I/O Values" },
    { 0x37, "Synchronise Clock" },
    { 0x38, "Send Communications Module Message" },
    { 0x39, "Get Communications Module Message" },
    { 0x3a, "Get Driver Information" },
    { 0x3b, "Send Driver Information" },
    { 0x3c, "Communications Analyser" },
    { 0x41, "Dial Site" },
    { 0x42, "Hang-up Site" },
    { 0x46, "Send File" },
    { 0x47, "Get File" },
    { 0x50, "Get Event Logging" },
    { 0x51, "Send Event Logging" },
    { 0x80, "Acknowledgement" },
    { 0x81, "Negative Acknowledgement" },
    { 0x84, "Get Named Variable" },
    { 0x85, "Send Named Variable" },
    { 0x87, "Get Module Information" },
    { 0x88, "Send Module Information" },
    { 0x89, "Get I/O Values" },
    { 0x8a, "Send I/O Values" },
    { 0x9e, "Cold Start" },
    { 0x9f, "Warm Start" },
    { 0xa2, "Get RTU Status" },
    { 0xa3, "Send RTU Status" },
    { 0xa4, "Set RTC" },
    { 0xa8, "Get Diagnostic Information" },
    { 0xa9, "Send Diagnostic Information" },
    { 0xd1, "Set Event Log" },
    { 0xd2, "Clear Event Log" },
    { 0xd3, "Get Number of Events" },
    { 0xd4, "Send Number of Events" },
    { 0xd5, "Get Event Log" },
    { 0xd6, "Continue Event Log" },
    { 0xd7, "Send Event Log" },
    { 0xe0, "Send File Start" },
    { 0xe1, "Send File Start Acknowledgement" },
    { 0xe2, "Send File Data" },
    { 0xe3, "Send File Data Acknowledgement" },
    {0, NULL}
};


static unsigned short
kingfisher_checksum(tvbuff_t *tvb, int offset)
{
    int c, i, j, len;
    unsigned short crc;

    crc = 0;
    len = tvb_reported_length_remaining(tvb, offset) - 2;
    for( i = 1; i < len; i++ )
    {
        c = ( ( unsigned char ) tvb_get_uint8( tvb, i ) ) & 0xff;
        for( j = 0; j < 8; ++j )
        {
            if( crc & 0x8000 )
            {
                crc <<= 1;
                crc += ( ( ( c <<= 1 ) & 0x100 ) != 0 );
                crc ^= 0x1021;
            }
            else
            {
                crc <<= 1;
                crc += ( ( ( c <<= 1 ) & 0x100 ) != 0 );
            }
        }
    }
    return crc;
}


static gboolean
dissect_kingfisher(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, gboolean is_conv_dissector)
{
    kingfisher_packet_t kfp;
    proto_tree *kingfisher_tree;
    proto_item *item=NULL;
    const char *func_string = NULL;
    unsigned short checksum;
    int message;


    /* There can be one byte reply packets. we only test for these when we
       are called from the conversation dissector since that is the only time
       we can be certain this is kingfisher
     */
    if(is_conv_dissector && (tvb_reported_length(tvb)==1)){
        /*
          Perform a check to see if the message is a single byte acknowledgement
          message - Note that in this instance there is no information in the packet
          with regard to source or destination RTU address which can be used in the
          population of dissector fields.
         */
        switch(tvb_get_uint8(tvb, 0)){
        case 0x00:
        case 0x01:
        case 0x80:
        case 0x81:
            col_set_str(pinfo->cinfo, COL_PROTOCOL, "Kingfisher");
            func_string = val_to_str_const(tvb_get_uint8(tvb, 0), function_code_vals, "Unknown function");
            col_add_fstr(pinfo->cinfo, COL_INFO, "(%s)", func_string);
            proto_tree_add_protocol_format(tree, proto_kingfisher, tvb, 0, -1, "Kingfisher Protocol, %s", func_string);
            return TRUE;
        }
        /* otherwise it is way too short to be kingfisher */
        return FALSE;
    }


    /* Verify that it looks like kingfisher */
    /* the packet must be at least 9 bytes */
    if(tvb_reported_length(tvb)<9){
        return FALSE;
    }

    /* the function code must be known */
    kfp.function = tvb_get_uint8( tvb, 6 );
    if (try_val_to_str(kfp.function, function_code_vals) == NULL) {
        /* This appears not to be a kingfisher packet */
        return FALSE;
    }

    /* verify the length */
    kfp.length = tvb_get_uint8(tvb, 2);
    if((kfp.length+1) != (uint8_t)tvb_captured_length(tvb)){
        return FALSE;
    }

    /* verify the checksum */
    kfp.checksum = tvb_get_ntohs(tvb, kfp.length - 1);
    checksum = kingfisher_checksum(tvb, 0);
    if(kfp.checksum!=checksum){
        return FALSE;
    }


    kfp.version = (kfp.function & 0x80)?3:2;
    kfp.system_id = tvb_get_uint8( tvb, 0 );
    kfp.message = tvb_get_uint8( tvb, 5 );

    kfp.target = tvb_get_uint8( tvb, 1 );
    kfp.from = tvb_get_uint8( tvb, 3 );
    kfp.via = tvb_get_uint8( tvb, 4 );

    if( kfp.version == 3 )
    {
        kfp.target |= ( tvb_get_uint8( tvb, 7 ) << 8 );
        kfp.from   |= ( tvb_get_uint8( tvb, 8 ) << 8 );
        kfp.via    |= ( tvb_get_uint8( tvb, 9 ) << 8 );
    }


    /* Ok  this does look like Kingfisher, so lets dissect it */
    func_string = val_to_str_const(kfp.function, function_code_vals, "Unknown function");

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "Kingfisher");
    col_add_fstr(pinfo->cinfo, COL_INFO, "%u > %u (%s)", kfp.from, kfp.target, func_string);


    message = (kfp.message & 0x0f) | ((kfp.message & 0xf0) >> 4);

    item = proto_tree_add_protocol_format(tree, proto_kingfisher, tvb, 0, -1, "Kingfisher Protocol, From RTU: %d, Target RTU: %d", kfp.from, kfp.target );
    kingfisher_tree = proto_item_add_subtree( item, ett_kingfisher );

    /* version */
    proto_tree_add_uint(kingfisher_tree, hf_kingfisher_version, tvb, 6, 1, kfp.version);

    /* system id */
    proto_tree_add_uint(kingfisher_tree, hf_kingfisher_system, tvb, 0, 1, kfp.system_id);

    /* target rtu */
    proto_tree_add_uint(kingfisher_tree, hf_kingfisher_target, tvb, 1, 1, kfp.target);

    /* length */
    proto_tree_add_uint(kingfisher_tree, hf_kingfisher_length, tvb, 2, 1, kfp.length);

    /* from rtu */
    proto_tree_add_uint(kingfisher_tree, hf_kingfisher_from, tvb, 3, 1, kfp.from);

    /* via rtu */
    proto_tree_add_uint(kingfisher_tree, hf_kingfisher_via, tvb, 4, 1, kfp.via);

    /* message number */
    proto_tree_add_uint_format_value(kingfisher_tree, hf_kingfisher_message, tvb, 5, 1, kfp.message, "%u (0x%02X, %s)", message, kfp.message, ((kfp.message & 0xf0)?"Response":"Request"));

    /* message function code */
    proto_tree_add_uint_format_value(kingfisher_tree, hf_kingfisher_function, tvb, 6, 1, kfp.function, "%u (0x%02X, %s)", kfp.function, kfp.function, func_string);

    /* message data */
    if(kfp.length > ((kfp.version==3)?11:8)){
        proto_tree_add_item(kingfisher_tree, hf_kingfisher_message_data, tvb, ((kfp.version==3)?10:7), kfp.length - ((kfp.version==3)?11:8), ENC_NA);
    }

    /* checksum */
    proto_tree_add_checksum(kingfisher_tree, tvb, kfp.length-1, hf_kingfisher_checksum, hf_kingfisher_checksum_status, &ei_kingfisher_checksum,
                            pinfo, checksum, ENC_BIG_ENDIAN, PROTO_CHECKSUM_VERIFY);

    return TRUE;
}


static gboolean
dissect_kingfisher_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
    gboolean was_kingfisher;


    was_kingfisher=dissect_kingfisher(tvb, pinfo, tree, FALSE);

    if(was_kingfisher){
        conversation_t *conversation;

        /* Ok this was a genuine kingfisher packet. Now create a conversation
           dissector for this tcp/udp socket and attach a conversation
           dissector to it.
         */
        conversation = find_or_create_conversation(pinfo);

        conversation_set_dissector(conversation, kingfisher_conv_handle);
    }

    return was_kingfisher;
}

static gboolean
dissect_kingfisher_conv(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
    return dissect_kingfisher(tvb, pinfo, tree, TRUE);
}

void
proto_register_kingfisher( void )
{
    static hf_register_info hf[] =
    {
            { &hf_kingfisher_version,       { "Version", "kingfisher.version", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL } },
            { &hf_kingfisher_system,        { "System Identifier", "kingfisher.system", FT_UINT8, BASE_DEC_HEX, NULL, 0x0, NULL, HFILL } },
            { &hf_kingfisher_length,        { "Length", "kingfisher.length", FT_UINT8, BASE_DEC_HEX, NULL, 0x0, NULL, HFILL } },
            { &hf_kingfisher_from,          { "From RTU", "kingfisher.from", FT_UINT16, BASE_DEC_HEX, NULL, 0x0, NULL, HFILL } },
            { &hf_kingfisher_target,        { "Target RTU", "kingfisher.target", FT_UINT16, BASE_DEC_HEX, NULL, 0x0, NULL, HFILL } },
            { &hf_kingfisher_via,           { "Via RTU", "kingfisher.via", FT_UINT16, BASE_DEC_HEX, NULL, 0x0, NULL, HFILL } },
            { &hf_kingfisher_message,       { "Message Number", "kingfisher.message", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL } },
            { &hf_kingfisher_function,      { "Message Function Code", "kingfisher.function", FT_UINT8, BASE_DEC, VALS( function_code_vals ), 0x0, NULL, HFILL } },
            { &hf_kingfisher_checksum,      { "Checksum", "kingfisher.checksum", FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL } },
            { &hf_kingfisher_checksum_status, { "Checksum Status", "kingfisher.checksum.status", FT_UINT8, BASE_NONE, VALS(proto_checksum_vals), 0x0, NULL, HFILL } },
            { &hf_kingfisher_message_data,  { "Message Data", "kingfisher.message_data", FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL } },
    };

    static int *ett[] = {
        &ett_kingfisher
    };

    static ei_register_info ei[] = {
        { &ei_kingfisher_checksum, { "kingfisher.bad_checksum", PI_CHECKSUM, PI_ERROR, "Bad checksum", EXPFILL }},
    };

    expert_module_t* expert_kingfisher;

    proto_kingfisher = proto_register_protocol( "Kingfisher", "Kingfisher", "kf" );
    proto_register_field_array( proto_kingfisher, hf, array_length( hf ) );
    proto_register_subtree_array( ett, array_length( ett ) );
    expert_kingfisher = expert_register_protocol(proto_kingfisher);
    expert_register_field_array(expert_kingfisher, ei, array_length(ei));

    kingfisher_handle = register_dissector("kf", dissect_kingfisher_heur, proto_kingfisher);
}


void
proto_reg_handoff_kingfisher( void )
{
    dissector_add_uint_range_with_preference("tcp.port", TCP_PORT_KINGFISHER_RANGE, kingfisher_handle);
    dissector_add_uint_range_with_preference("udp.port", UDP_PORT_KINGFISHER_RANGE, kingfisher_handle);

    kingfisher_conv_handle = create_dissector_handle(dissect_kingfisher_conv, proto_kingfisher);
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
