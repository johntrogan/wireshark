/* packet-rlc-lte.h
 *
 * Martin Mathieson
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PACKET_RLC_LTE_H
#define PACKET_RLC_LTE_H

/* rlcMode */
#define RLC_TM_MODE 1
#define RLC_UM_MODE 2
#define RLC_AM_MODE 4
#define RLC_PREDEF  8

/* direction */
#define DIRECTION_UPLINK 0
#define DIRECTION_DOWNLINK 1

/* priority ? */

/* channelType */
#define CHANNEL_TYPE_CCCH 1
#define CHANNEL_TYPE_BCCH_BCH 2
#define CHANNEL_TYPE_PCCH 3
#define CHANNEL_TYPE_SRB 4
#define CHANNEL_TYPE_DRB 5
#define CHANNEL_TYPE_BCCH_DL_SCH 6
#define CHANNEL_TYPE_MCCH 7
#define CHANNEL_TYPE_MTCH 8

/* sequenceNumberLength */
#define UM_SN_LENGTH_5_BITS 5
#define UM_SN_LENGTH_10_BITS 10
#define AM_SN_LENGTH_10_BITS 10
#define AM_SN_LENGTH_16_BITS 16


typedef enum rlc_lte_nb_mode {
    rlc_no_nb_mode = 0,
    rlc_nb_mode = 1
} rlc_lte_nb_mode;


/* Info attached to each LTE RLC frame */
typedef struct rlc_lte_info
{
    uint8_t         rlcMode;
    uint8_t         direction;
    uint8_t         priority;
    uint8_t         sequenceNumberLength;
    uint16_t        ueid;
    uint16_t        channelType;
    uint16_t        channelId; /* for SRB: 1=SRB1, 2=SRB2, 3=SRB1bis; for DRB: DRB ID */
    uint16_t        pduLength;
    bool            extendedLiField;
    rlc_lte_nb_mode nbMode;
} rlc_lte_info;


/* Configure number of PDCP SN bits to use for DRB channels. */
void set_rlc_lte_drb_pdcp_seqnum_length(packet_info *pinfo, uint16_t ueid, uint8_t drbid, uint8_t userplane_seqnum_length);

/* Configure LI field for AM DRB channels. */
void set_rlc_lte_drb_li_field(packet_info *pinfo, uint16_t ueid, uint8_t drbid, bool ul_ext_li_field, bool dl_ext_li_field);

/* Reset UE's bearers */
void rlc_lte_reset_ue_bearers(packet_info *pinfo, uint16_t ueid);

/**********************************************************************/
/* UDP framing format                                                 */
/* -----------------------                                            */
/* Several people have asked about dissecting RLC by framing          */
/* PDUs over IP.  A suggested format over UDP has been defined        */
/* and implemented by this dissector, using the definitions           */
/* below. A link to an example program showing you how to encode      */
/* these headers and send LTE RLC PDUs on a UDP socket is             */
/* provided at https://gitlab.com/wireshark/wireshark/-/wikis/RLC-LTE */
/*                                                                    */
/* A heuristic dissector (enabled by a preference) will               */
/* recognise a signature at the beginning of these frames.            */
/* Until someone is using this format, suggestions for changes        */
/* are welcome.                                                       */
/**********************************************************************/


/* Signature.  Rather than try to define a port for this, or make the
   port number a preference, frames will start with this string (with no
   terminating NULL */
#define RLC_LTE_START_STRING "rlc-lte"

/* Fixed field.  This is followed by the following 1 mandatory field:
   - rlcMode (1 byte)
   (where the allowed values are defined above */

/* Conditional field. This field is mandatory in case of RLC Unacknowledged mode.
   In case of RLC Acknowledged mode, the field is optional (assume 10 bits by default).
   The format is to have the tag, followed by the value (there is no length field,
   it's implicit from the tag). The allowed values are defined above. */

#define RLC_LTE_SN_LENGTH_TAG    0x02
/* 1 byte */

/* Optional fields. Attaching this info to frames will allow you
   to show you display/filter/plot/add-custom-columns on these fields, so should
   be added if available.
   The format is to have the tag, followed by the value (there is no length field,
   it's implicit from the tag) */

#define RLC_LTE_DIRECTION_TAG       0x03
/* 1 byte */

#define RLC_LTE_PRIORITY_TAG        0x04
/* 1 byte */

#define RLC_LTE_UEID_TAG            0x05
/* 2 bytes, network order */

#define RLC_LTE_CHANNEL_TYPE_TAG    0x06
/* 2 bytes, network order */

#define RLC_LTE_CHANNEL_ID_TAG      0x07
/* 2 bytes, network order */

#define RLC_LTE_EXT_LI_FIELD_TAG    0x08
/* 0 byte, tag presence indicates that AM DRB PDU is using an extended LI field of 15 bits */

#define RLC_LTE_NB_MODE_TAG         0x09
/* 1 byte containing rlc_lte_nb_mode enum value */

/* RLC PDU. Following this tag comes the actual RLC PDU (there is no length, the PDU
   continues until the end of the frame) */
#define RLC_LTE_PAYLOAD_TAG         0x01

#endif

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
