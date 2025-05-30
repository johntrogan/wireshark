/* packet-infiniband.c
 * Routines for Infiniband/ERF Dissection
 * Copyright 2008 Endace Technology Limited
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * Modified 2010 by Mellanox Technologies Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/exceptions.h>
#include <epan/conversation.h>
#include <epan/prefs.h>
#include <epan/etypes.h>
#include <epan/show_exception.h>
#include <epan/decode_as.h>
#include <epan/reassemble.h>
#include <epan/proto_data.h>
#include <wiretap/erf_record.h>
#include <wiretap/wtap.h>

#include "packet-infiniband.h"

void proto_register_infiniband(void);
void proto_reg_handoff_infiniband(void);

/*Default RRoce UDP port*/
#define DEFAULT_RROCE_UDP_PORT    4791

/* service id prefix 0x0000000001 is designated for
 * RDMA IP CM service as per Annex A11.2
 */
#define RDMA_IP_CM_SID_PREFIX_MASK 0xFFFFFFFFFF000000
#define RDMA_IP_CM_SID_PREFIX 0x0000000001000000

/* Wireshark ID */
static int proto_infiniband;
static int proto_infiniband_link;

/* Variables to hold expansion values between packets */
/* static int ett_infiniband;                */
static int ett_all_headers;
static int ett_lrh;
static int ett_grh;
static int ett_bth;
static int ett_rwh;
static int ett_rdeth;
static int ett_deth;
static int ett_reth;
static int ett_atomiceth;
static int ett_aeth;
static int ett_aeth_syndrome;
static int ett_atomicacketh;
static int ett_immdt;
static int ett_ieth;
static int ett_payload;
static int ett_vendor;
static int ett_subn_lid_routed;
static int ett_subn_directed_route;
static int ett_subnadmin;
static int ett_mad;
static int ett_cm;
static int ett_cm_sid;
static int ett_cm_ipcm;
static int ett_rmpp;
static int ett_subm_attribute;
static int ett_suba_attribute;
static int ett_datadetails;
static int ett_noticestraps;
/* static int ett_nodedesc;                  */
/* static int ett_nodeinfo;                  */
/* static int ett_switchinfo;                */
/* static int ett_guidinfo;                  */
/* static int ett_portinfo;                  */
static int ett_portinfo_capmask;
static int ett_pkeytable;
static int ett_sltovlmapping;
static int ett_vlarbitrationtable;
static int ett_linearforwardingtable;
static int ett_randomforwardingtable;
static int ett_multicastforwardingtable;
static int ett_sminfo;
static int ett_vendordiag;
static int ett_ledinfo;
static int ett_linkspeedwidthpairs;
static int ett_informinfo;
static int ett_linkrecord;
static int ett_servicerecord;
static int ett_pathrecord;
static int ett_mcmemberrecord;
static int ett_tracerecord;
static int ett_multipathrecord;
static int ett_serviceassocrecord;
static int ett_perfclass;
static int ett_link;

/* Dissector Declaration */
static dissector_handle_t ib_handle;
static dissector_handle_t ib_link_handle;

/* Subdissectors Declarations */
static dissector_handle_t ipv6_handle;
static dissector_handle_t eth_handle;
static dissector_table_t ethertype_dissector_table;

static dissector_table_t subdissector_table;

/* MAD_Data
* Structure to hold information from the common MAD header.
* This is necessary because the MAD header contains information which significantly changes the dissection algorithm. */
typedef struct {
    uint8_t managementClass;
    uint8_t classVersion;
    uint8_t method;
    uint8_t status;
    uint16_t classSpecific;
    uint64_t transactionID;
    uint16_t attributeID;
    uint32_t attributeModifier;
    char data[MAD_DATA_SIZE];
} MAD_Data;

typedef enum {
    IB_PACKET_STARTS_WITH_LRH, /* Regular IB packet */
    IB_PACKET_STARTS_WITH_GRH, /* ROCE packet */
    IB_PACKET_STARTS_WITH_BTH  /* RROCE packet */
} ib_packet_start_header;

/* Forward-declarations */

static void dissect_infiniband_common(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, ib_packet_start_header starts_with);
static int32_t find_next_header_sequence(struct infinibandinfo* ibInfo);
static bool contains(uint32_t value, uint32_t* arr, int length);
static void dissect_general_info(tvbuff_t *tvb, int offset, packet_info *pinfo, ib_packet_start_header starts_with);

/* Parsing Methods for specific IB headers. */

static void parse_VENDOR(proto_tree *, tvbuff_t *, int *);
static void parse_PAYLOAD(proto_tree *, packet_info *, struct infinibandinfo *, tvbuff_t *, int *, int length, int crclen, proto_tree *);
static void parse_IETH(proto_tree *, tvbuff_t *, int *);
static void parse_IMMDT(proto_tree *, tvbuff_t *, int *offset);
static void parse_ATOMICACKETH(proto_tree *, tvbuff_t *, int *offset);
static void parse_AETH(proto_tree *, tvbuff_t *, int *offset, packet_info *pinfo);
static void parse_ATOMICETH(proto_tree *, tvbuff_t *, int *offset);
static void parse_RETH(proto_tree *, tvbuff_t *, int *offset,
                       struct infinibandinfo *info);
static void parse_DETH(proto_tree *, packet_info *, tvbuff_t *, int *offset);
static void parse_RDETH(proto_tree *, tvbuff_t *, int *offset);
static void parse_IPvSix(proto_tree *, tvbuff_t *, int *offset, packet_info *);
static void parse_RWH(proto_tree *, tvbuff_t *, int *offset, packet_info *, proto_tree *);
static void parse_DCCETH(proto_tree *parentTree, tvbuff_t *tvb, int *offset);
static void parse_FETH(proto_tree *, tvbuff_t *, int *offset);

static void parse_SUBN_LID_ROUTED(proto_tree *, packet_info *, tvbuff_t *, int *offset);
static void parse_SUBN_DIRECTED_ROUTE(proto_tree *, packet_info *, tvbuff_t *, int *offset);
static void parse_SUBNADMN(proto_tree *, packet_info *, tvbuff_t *, int *offset);
static void parse_PERF(proto_tree *, tvbuff_t *, packet_info *, int *offset);
static void parse_BM(proto_tree *, tvbuff_t *, int *offset);
static void parse_DEV_MGT(proto_tree *, tvbuff_t *, int *offset);
static void parse_COM_MGT(proto_tree *parentTree, packet_info *pinfo, tvbuff_t *tvb, int *offset, proto_tree* top_tree);
static void parse_SNMP(proto_tree *, tvbuff_t *, int *offset);
static void parse_VENDOR_MANAGEMENT(proto_tree *, tvbuff_t *, int *offset);
static void parse_APPLICATION_MANAGEMENT(proto_tree *, tvbuff_t *, int *offset);
static void parse_RESERVED_MANAGEMENT(proto_tree *, tvbuff_t *, int *offset);

static bool parse_MAD_Common(proto_tree*, tvbuff_t*, int *offset, MAD_Data*);
static bool parse_RMPP(proto_tree* , tvbuff_t* , int *offset);
static void label_SUBM_Method(proto_item*, MAD_Data*, packet_info*);
static void label_SUBM_Attribute(proto_item*, MAD_Data*, packet_info*);
static void label_SUBA_Method(proto_item*, MAD_Data*, packet_info*);
static void label_SUBA_Attribute(proto_item*, MAD_Data*, packet_info*);

/* Class Attribute Parsing Routines */
static bool parse_SUBM_Attribute(proto_tree*, tvbuff_t*, int *offset, MAD_Data*);
static bool parse_SUBA_Attribute(proto_tree*, tvbuff_t*, int *offset, MAD_Data*);

/* These methods parse individual attributes
* Naming convention FunctionHandle = "parse_" + [Attribute Name];
* Where [Attribute Name] is the attribute identifier from chapter 14 of the IB Specification
* Subnet Management */
static void parse_NoticesAndTraps(proto_tree*, tvbuff_t*, int *offset);
static void parse_NodeDescription(proto_tree*, tvbuff_t*, int *offset);
static int parse_NodeInfo(proto_tree*, tvbuff_t*, int *offset);
static int parse_SwitchInfo(proto_tree*, tvbuff_t*, int *offset);
static int parse_GUIDInfo(proto_tree*, tvbuff_t*, int *offset);
static int parse_PortInfo(proto_tree*, tvbuff_t*, int *offset);
static void parse_P_KeyTable(proto_tree*, tvbuff_t*, int *offset);
static void parse_SLtoVLMappingTable(proto_tree*, tvbuff_t*, int *offset);
static void parse_VLArbitrationTable(proto_tree*, tvbuff_t*, int *offset);
static void parse_LinearForwardingTable(proto_tree*, tvbuff_t*, int *offset);
static void parse_RandomForwardingTable(proto_tree*, tvbuff_t*, int *offset);
static void parse_MulticastForwardingTable(proto_tree*, tvbuff_t*, int *offset);
static int parse_SMInfo(proto_tree*, tvbuff_t*, int *offset);
static int parse_VendorDiag(proto_tree*, tvbuff_t*, int *offset);
static void parse_LedInfo(proto_tree*, tvbuff_t*, int *offset);
static int parse_LinkSpeedWidthPairsTable(proto_tree*, tvbuff_t*, int *offset);

/* These methods parse individual attributes for specific MAD management classes.
* Naming convention FunctionHandle = "parse_" + [Management Class] + "_" + [Attribute Name];
* Where [Management Class] is the shorthand name for the management class as defined
* in the MAD Management Classes section below in this file, and [Attribute Name] is the
* attribute identifier from the corresponding chapter of the IB Specification */
static int parse_PERF_PortCounters(proto_tree* parentTree, tvbuff_t* tvb, packet_info *pinfo, int *offset);
static int parse_PERF_PortCountersExtended(proto_tree* parentTree, tvbuff_t* tvb, packet_info *pinfo, int *offset);

/* Subnet Administration */
static int parse_InformInfo(proto_tree*, tvbuff_t*, int *offset);
static int parse_LinkRecord(proto_tree*, tvbuff_t*, int *offset);
static int parse_ServiceRecord(proto_tree*, tvbuff_t*, int *offset);
static int parse_PathRecord(proto_tree*, tvbuff_t*, int *offset);
static int parse_MCMemberRecord(proto_tree*, tvbuff_t*, int *offset);
static int parse_TraceRecord(proto_tree*, tvbuff_t*, int *offset);
static int parse_MultiPathRecord(proto_tree*, tvbuff_t*, int *offset);
static int parse_ServiceAssociationRecord(proto_tree*, tvbuff_t*, int *offset);

/* Subnet Administration */
static void parse_RID(proto_tree*, tvbuff_t*, int *offset, MAD_Data*);

/* Common */
static int parse_ClassPortInfo(proto_tree*, tvbuff_t*, int *offset);

/* SM Methods */
static const value_string SUBM_Methods[] = {
    { 0x01, "SubnGet("},
    { 0x02, "SubnSet("},
    { 0x81, "SubnGetResp("},
    { 0x05, "SubnTrap("},
    { 0x07, "SubnTrapResp("},
    { 0, NULL}
};
/* SM Attributes */
static const value_string SUBM_Attributes[] = {
    { 0x0002, "Attribute (Notice)"},
    { 0x0010, "Attribute (NodeDescription)"},
    { 0x0011, "Attribute (NodeInfo)"},
    { 0x0012, "Attribute (SwitchInfo)"},
    { 0x0014, "Attribute (GUIDInfo)"},
    { 0x0015, "Attribute (PortInfo)"},
    { 0x0016, "Attribute (P_KeyTable)"},
    { 0x0017, "Attribute (SLtoVLMappingTable)"},
    { 0x0018, "Attribute (VLArbitrationTable)"},
    { 0x0019, "Attribute (LinearForwardingTable)"},
    { 0x001A, "Attribute (RandomForwardingTable)"},
    { 0x001B, "Attribute (MulticastForwardingTable)"},
    { 0x001C, "Attribute (LinkSpeedWidthPairsTable)"},
    { 0x0020, "Attribute (SMInfo)"},
    { 0x0030, "Attribute (VendorDiag)"},
    { 0x0031, "Attribute (LedInfo)"},
    { 0, NULL}
};

/* SA Methods */
static const value_string SUBA_Methods[] = {
    { 0x01, "SubnAdmGet("},
    { 0x81, "SubnAdmGetResp("},
    { 0x02, "SubnAdmSet("},
    { 0x06, "SubnAdmReport("},
    { 0x86, "SubnAdmReportResp("},
    { 0x12, "SubnAdmGetTable("},
    { 0x92, "SubnAdmGetTableResp("},
    { 0x13, "SubnAdmGetTraceTable("},
    { 0x14, "SubnAdmGetMulti("},
    { 0x94, "SubnAdmGetMultiResp("},
    { 0x15, "SubnAdmDelete("},
    { 0x95, "SubnAdmDeleteResp("},
    { 0, NULL}
};
/* SA Attributes */
static const value_string SUBA_Attributes[] = {
    { 0x0001, "Attribute (ClassPortInfo)"},
    { 0x0002, "Attribute (Notice)"},
    { 0x0003, "Attribute (InformInfo)"},
    { 0x0011, "Attribute (NodeRecord)"},
    { 0x0012, "Attribute (PortInfoRecord)"},
    { 0x0013, "Attribute (SLtoVLMappingTableRecord)"},
    { 0x0014, "Attribute (SwitchInfoRecord)"},
    { 0x0015, "Attribute (LinearForwardingTableRecord)"},
    { 0x0016, "Attribute (RandomForwardingTableRecord)"},
    { 0x0017, "Attribute (MulticastForwardingTableRecord)"},
    { 0x0018, "Attribute (SMInfoRecord)"},
    { 0x0019, "Attribute (LinkSpeedWidthPairsTableRecord)"},
    { 0x00F3, "Attribute (InformInfoRecord)"},
    { 0x0020, "Attribute (LinkRecord)"},
    { 0x0030, "Attribute (GuidInfoRecord)"},
    { 0x0031, "Attribute (ServiceRecord)"},
    { 0x0033, "Attribute (P_KeyTableRecord)"},
    { 0x0035, "Attribute (PathRecord)"},
    { 0x0036, "Attribute (VLArbitrationTableRecord)"},
    { 0x0038, "Attribute (MCMemberRecord)"},
    { 0x0039, "Attribute (TraceRecord)"},
    { 0x003A, "Attribute (MultiPathRecord)"},
    { 0x003B, "Attribute (ServiceAssociationRecord)"},
    { 0, NULL}
};

/* CM Attributes */
static const value_string CM_Attributes[] = {
    { 0x0001,       "ClassPortInfo"},
    { ATTR_CM_REQ,  "ConnectRequest"},
    { 0x0011,       "MsgRcptAck"},
    { ATTR_CM_REJ,  "ConnectReject"},
    { ATTR_CM_REP,  "ConnectReply"},
    { ATTR_CM_RTU,  "ReadyToUse"},
    { ATTR_CM_DREQ, "DisconnectRequest"},
    { ATTR_CM_DRSP, "DisconnectReply"},
    { 0x0017,       "ServiceIDResReq"},
    { 0x0018,       "ServiceIDResReqResp"},
    { 0x0019,       "LoadAlternatePath"},
    { 0x001A,       "AlternatePathResponse"},
    { 0, NULL}
};

/* RMPP Types */
#define RMPP_NOT_USED 0
#define RMPP_DATA   1
#define RMPP_ACK    2
#define RMPP_STOP   3
#define RMPP_ABORT  4

static const value_string RMPP_Packet_Types[] = {
    { RMPP_NOT_USED, " Not an RMPP Packet " },
    { RMPP_DATA,    "RMPP (DATA)" },
    { RMPP_ACK,     "RMPP (ACK)" },
    { RMPP_STOP,    "RMPP (STOP)" },
    { RMPP_ABORT,   "RMPP (ABORT)" },
    { 0, NULL}
};

static const value_string RMPP_Flags[] = {
    { 3, " (Transmission Sequence - First Packet)"},
    { 5, " (Transmission Sequence - Last Packet)"},
    { 7, " (Transmission Sequence - First and Last Packet)"},
    { 1, " (Transmission Sequence) " },
    { 0, NULL}
};

static const value_string RMPP_Status[]= {
    {   0, " (Normal)"},
    {   1, " (Resources Exhausted)"},
    { 118, " (Total Time Too Long)"},
    { 119, " (Inconsistent Last and PayloadLength)"},
    { 120, " (Inconsistent First and Segment Number)"},
    { 121, " (Bad RMPPType)"},
    { 122, " (NewWindowLast Too Small)"},
    { 123, " (SegmentNumber Too Big)"},
    { 124, " (Illegal Status)"},
    { 125, " (Unsupported Version)"},
    { 126, " (Too Many Retries)"},
    { 127, " (Unspecified - Unknown Error Code on ABORT)"},
    { 0, NULL}
};

static const value_string DiagCode[]= {
    {0x0000, "Function Ready"},
    {0x0001, "Performing Self Test"},
    {0x0002, "Initializing"},
    {0x0003, "Soft Error - Function has non-fatal error"},
    {0x0004, "Hard Error - Function has fatal error"},
    { 0, NULL}
};
static const value_string LinkWidthEnabled[]= {
    {0x0000, "No State Change"},
    {0x0001, "1x"},
    {0x0002, "4x"},
    {0x0003, "1x or 4x"},
    {0x0004, "8x"},
    {0x0005, "1x or 8x"},
    {0x0006, "4x or 8x"},
    {0x0007, "1x or 4x or 8x"},
    {0x0008, "12x"},
    {0x0009, "1x or 12x"},
    {0x000A, "4x or 12x"},
    {0x000B, "1x or 4x or 12x"},
    {0x000C, "8x or 12x"},
    {0x000D, "1x or 8x or 12x"},
    {0x000E, "4x or 8x or 12x"},
    {0x000F, "1x or 4x or 8x or 12x"},
    {0x00FF, "Set to LinkWidthSupported Value - Response contains actual LinkWidthSupported"},
    { 0, NULL}
};

static const value_string LinkWidthSupported[]= {
    {0x0001, "1x"},
    {0x0003, "1x or 4x"},
    {0x0007, "1x or 4x or 8x"},
    {0x000B, "1x or 4x or 12x"},
    {0x000F, "1x or 4x or 8x or 12x"},
    { 0, NULL}
};
static const value_string LinkWidthActive[]= {
    {0x0001, "1x"},
    {0x0002, "4x"},
    {0x0004, "8x"},
    {0x0008, "12x"},
    { 0, NULL}
};
static const value_string LinkSpeedSupported[]= {
    {0x0001, "2.5 Gbps"},
    {0x0003, "2.5 or 5.0 Gbps"},
    {0x0005, "2.5 or 10.0 Gbps"},
    {0x0007, "2.5 or 5.0 or 10.0 Gbps"},
    { 0, NULL}
};
static const value_string PortState[]= {
    {0x0000, "No State Change"},
    {0x0001, "Down (includes failed links)"},
    {0x0002, "Initialized"},
    {0x0003, "Armed"},
    {0x0004, "Active"},
    { 0, NULL}
};
static const value_string PortPhysicalState[]= {
    {0x0000, "No State Change"},
    {0x0001, "Sleep"},
    {0x0002, "Polling"},
    {0x0003, "Disabled"},
    {0x0004, "PortConfigurationTraining"},
    {0x0005, "LinkUp"},
    {0x0006, "LinkErrorRecovery"},
    {0x0007, "Phy Test"},
    { 0, NULL}
};
static const value_string LinkDownDefaultState[]= {
    {0x0000, "No State Change"},
    {0x0001, "Sleep"},
    {0x0002, "Polling"},
    { 0, NULL}
};
static const value_string LinkSpeedActive[]= {
    {0x0001, "2.5 Gbps"},
    {0x0002, "5.0 Gbps"},
    {0x0004, "10.0 Gbps"},
    { 0, NULL}
};
static const value_string LinkSpeedEnabled[]= {
    {0x0000, "No State Change"},
    {0x0001, "2.5 Gbps"},
    {0x0003, "2.5 or 5.0 Gbps"},
    {0x0005, "2.5 or 10.0 Gbps"},
    {0x0007, "2.5 or 5.0 or 10.0 Gbps"},
    {0x000F, "Set to LinkSpeedSupported value - response contains actual LinkSpeedSupported"},
    { 0, NULL}
};
static const value_string NeighborMTU[]= {
    {0x0001, "256"},
    {0x0002, "512"},
    {0x0003, "1024"},
    {0x0004, "2048"},
    {0x0005, "4096"},
    { 0, NULL}
};
static const value_string VLCap[]= {
    {0x0001, "VL0"},
    {0x0002, "VL0, VL1"},
    {0x0003, "VL0 - VL3"},
    {0x0004, "VL0 - VL7"},
    {0x0005, "VL0 - VL14"},
    { 0, NULL}
};
static const value_string MTUCap[]= {
    {0x0001, "256"},
    {0x0002, "512"},
    {0x0003, "1024"},
    {0x0004, "2048"},
    {0x0005, "4096"},
    { 0, NULL}
};
static const value_string OperationalVLs[]= {
    {0x0000, "No State Change"},
    {0x0001, "VL0"},
    {0x0002, "VL0, VL1"},
    {0x0003, "VL0 - VL3"},
    {0x0004, "VL0 - VL7"},
    {0x0005, "VL0 - VL14"},
    { 0, NULL}
};

/* For reserved fields in various packets */
static int hf_infiniband_reserved;
/* Local Route Header (LRH) */
static int hf_infiniband_LRH;
static int hf_infiniband_virtual_lane;
static int hf_infiniband_link_version;
static int hf_infiniband_service_level;
static int hf_infiniband_reserved2;
static int hf_infiniband_link_next_header;
static int hf_infiniband_destination_local_id;
static int hf_infiniband_reserved5;
static int hf_infiniband_packet_length;
static int hf_infiniband_source_local_id;
/* Global Route Header (GRH) */
static int hf_infiniband_GRH;
static int hf_infiniband_ip_version;
static int hf_infiniband_traffic_class;
static int hf_infiniband_flow_label;
static int hf_infiniband_payload_length;
static int hf_infiniband_next_header;
static int hf_infiniband_hop_limit;
static int hf_infiniband_source_gid;
static int hf_infiniband_destination_gid;
/* Base Transport Header (BTH) */
static int hf_infiniband_BTH;
static int hf_infiniband_opcode;
static int hf_infiniband_solicited_event;
static int hf_infiniband_migreq;
static int hf_infiniband_pad_count;
static int hf_infiniband_transport_header_version;
static int hf_infiniband_partition_key;
static int hf_infiniband_destination_qp;
static int hf_infiniband_acknowledge_request;
static int hf_infiniband_reserved7;
static int hf_infiniband_packet_sequence_number;
/* Raw Header (RWH) */
static int hf_infiniband_RWH;
static int hf_infiniband_etype;
/* Reliable Datagram Extended Transport Header (RDETH) */
static int hf_infiniband_RDETH;
static int hf_infiniband_ee_context;
/* Datagram Extended Transport Header (DETH) */
static int hf_infiniband_DETH;
static int hf_infiniband_queue_key;
static int hf_infiniband_source_qp;
/* RDMA Extended Transport Header (RETH) */
static int hf_infiniband_RETH;
static int hf_infiniband_virtual_address;
static int hf_infiniband_remote_key;
static int hf_infiniband_dma_length;
/* Atomic Extended Transport Header (AtomicETH) */
static int hf_infiniband_AtomicETH;
/* static int hf_infiniband_virtual_address_AtomicETH;                  */
/* static int hf_infiniband_remote_key_AtomicETH;                       */
static int hf_infiniband_swap_or_add_data;
static int hf_infiniband_compare_data;
/* ACK Extended Transport Header (AETH) */
static int hf_infiniband_AETH;
static int hf_infiniband_syndrome;
static int hf_infiniband_syndrome_reserved;
static int hf_infiniband_syndrome_opcode;
static int hf_infiniband_syndrome_credit_count;
static int hf_infiniband_syndrome_timer;
static int hf_infiniband_syndrome_reserved_value;
static int hf_infiniband_syndrome_error_code;
static int hf_infiniband_message_sequence_number;
/* Atomic ACK Extended Transport Header (AtomicAckETH) */
static int hf_infiniband_AtomicAckETH;
static int hf_infiniband_original_remote_data;
/* Immediate Extended Transport Header (ImmDt) */
static int hf_infiniband_IMMDT;
/* Invalidate Extended Transport Header (IETH) */
static int hf_infiniband_IETH;
/* FLUSH Extended Transport Header (FETH) */
static int hf_infiniband_FETH;
static int hf_infiniband_reserved27;
static int hf_infiniband_selectivity_level;
static int hf_infiniband_placement_type;

/* Payload */
static int hf_infiniband_payload;
static int hf_infiniband_invariant_crc;
static int hf_infiniband_variant_crc;
/* Unknown or Vendor Specific */
static int hf_infiniband_raw_data;
static int hf_infiniband_vendor;
/* CM REQ Header */
static int hf_cm_req_local_comm_id;
static int hf_cm_req_service_id;
static int hf_cm_req_service_id_prefix;
static int hf_cm_req_service_id_protocol;
static int hf_cm_req_service_id_dport;
static int hf_cm_req_local_ca_guid;
static int hf_cm_req_local_qkey;
static int hf_cm_req_local_qpn;
static int hf_cm_req_respo_res;
static int hf_cm_req_local_eecn;
static int hf_cm_req_init_depth;
static int hf_cm_req_remote_eecn;
static int hf_cm_req_remote_cm_resp_to;
static int hf_cm_req_transp_serv_type;
static int hf_cm_req_e2e_flow_ctrl;
static int hf_cm_req_start_psn;
static int hf_cm_req_local_cm_resp_to;
static int hf_cm_req_retry_count;
static int hf_cm_req_pkey;
static int hf_cm_req_path_pp_mtu;
static int hf_cm_req_rdc_exists;
static int hf_cm_req_rnr_retry_count;
static int hf_cm_req_max_cm_retries;
static int hf_cm_req_srq;
static int hf_cm_req_extended_transport;
static int hf_cm_req_primary_local_lid;
static int hf_cm_req_primary_remote_lid;
static int hf_cm_req_primary_local_gid;
static int hf_cm_req_primary_remote_gid;
static int hf_cm_req_primary_local_gid_ipv4;
static int hf_cm_req_primary_remote_gid_ipv4;
static int hf_cm_req_primary_flow_label;
static int hf_cm_req_primary_reserved0;
static int hf_cm_req_primary_packet_rate;
static int hf_cm_req_primary_traffic_class;
static int hf_cm_req_primary_hop_limit;
static int hf_cm_req_primary_sl;
static int hf_cm_req_primary_subnet_local;
static int hf_cm_req_primary_reserved1;
static int hf_cm_req_primary_local_ack_to;
static int hf_cm_req_primary_reserved2;
static int hf_cm_req_alt_local_lid;
static int hf_cm_req_alt_remote_lid;
static int hf_cm_req_alt_local_gid;
static int hf_cm_req_alt_remote_gid;
static int hf_cm_req_flow_label;
static int hf_cm_req_alt_reserved0;
static int hf_cm_req_packet_rate;
static int hf_cm_req_alt_traffic_class;
static int hf_cm_req_alt_hop_limit;
static int hf_cm_req_SL;
static int hf_cm_req_subnet_local;
static int hf_cm_req_alt_reserved1;
static int hf_cm_req_local_ACK_timeout;
static int hf_cm_req_alt_reserved2;
static int hf_cm_req_private_data;
static int hf_cm_req_ip_cm_req_msg;
static int hf_cm_req_ip_cm_majv;
static int hf_cm_req_ip_cm_minv;
static int hf_cm_req_ip_cm_ipv;
static int hf_cm_req_ip_cm_res;
static int hf_cm_req_ip_cm_sport;
static int hf_cm_req_ip_cm_sip6;
static int hf_cm_req_ip_cm_dip6;
static int hf_cm_req_ip_cm_sip4;
static int hf_cm_req_ip_cm_dip4;
static int hf_ip_cm_req_consumer_private_data;

/* CM REP Header */
static int hf_cm_rep_localcommid;
static int hf_cm_rep_remotecommid;
static int hf_cm_rep_localqkey;
static int hf_cm_rep_localqpn;
static int hf_cm_rep_localeecontnum;
static int hf_cm_rep_startingpsn;
static int hf_cm_rep_responderres;
static int hf_cm_rep_initiatordepth;
static int hf_cm_rep_tgtackdelay;
static int hf_cm_rep_failoveracc;
static int hf_cm_rep_e2eflowctl;
static int hf_cm_rep_rnrretrycount;
static int hf_cm_rep_srq;
static int hf_cm_rep_reserved;
static int hf_cm_rep_localcaguid;
static int hf_cm_rep_privatedata;
/* CM RTU Header */
static int hf_cm_rtu_localcommid;
static int hf_cm_rtu_remotecommid;
static int hf_cm_rtu_privatedata;
/* CM REJ Header */
static int hf_cm_rej_local_commid;
static int hf_cm_rej_remote_commid;
static int hf_cm_rej_msg_rej;
static int hf_cm_rej_msg_reserved0;
static int hf_cm_rej_rej_info_len;
static int hf_cm_rej_msg_reserved1;
static int hf_cm_rej_reason;
static int hf_cm_rej_add_rej_info;
static int hf_cm_rej_private_data;
/* CM DREQ Header */
static int hf_cm_dreq_localcommid;
static int hf_cm_dreq_remotecommid;
static int hf_cm_dreq_remote_qpn;
static int hf_cm_dreq_privatedata;
/* CM DRSP Header */
static int hf_cm_drsp_localcommid;
static int hf_cm_drsp_remotecommid;
static int hf_cm_drsp_privatedata;
/* MAD Base Header */
static int hf_infiniband_MAD;
static int hf_infiniband_base_version;
static int hf_infiniband_mgmt_class;
static int hf_infiniband_class_version;
static int hf_infiniband_method;
static int hf_infiniband_status;
static int hf_infiniband_class_specific;
static int hf_infiniband_transaction_id;
static int hf_infiniband_attribute_id;
static int hf_infiniband_attribute_modifier;
static int hf_infiniband_data;
/* RMPP Header */
static int hf_infiniband_RMPP;
static int hf_infiniband_rmpp_version;
static int hf_infiniband_rmpp_type;
static int hf_infiniband_r_resp_time;
static int hf_infiniband_rmpp_flags;
static int hf_infiniband_rmpp_status;
static int hf_infiniband_rmpp_data1;
static int hf_infiniband_rmpp_data2;
/* RMPP Data */
/* static int hf_infiniband_RMPP_DATA; */
static int hf_infiniband_segment_number;
static int hf_infiniband_payload_length32;
static int hf_infiniband_transferred_data;
/* RMPP ACK */
static int hf_infiniband_new_window_last;
/* RMPP ABORT and STOP */
static int hf_infiniband_optional_extended_error_data;
/* SMP Data LID Routed */
static int hf_infiniband_SMP_LID;
static int hf_infiniband_m_key;
static int hf_infiniband_smp_data;
/* SMP Data Directed Route */
static int hf_infiniband_SMP_DIRECTED;
static int hf_infiniband_smp_status;
static int hf_infiniband_hop_pointer;
static int hf_infiniband_hop_count;
static int hf_infiniband_dr_slid;
static int hf_infiniband_dr_dlid;
static int hf_infiniband_d;
static int hf_infiniband_initial_path;
static int hf_infiniband_return_path;
/* SA MAD Header */
static int hf_infiniband_SA;
static int hf_infiniband_sm_key;
static int hf_infiniband_attribute_offset;
static int hf_infiniband_component_mask;
static int hf_infiniband_subnet_admin_data;

/* Mellanox EoIB encapsulation header */
static int proto_mellanox_eoib;
static int hf_infiniband_ver;
static int hf_infiniband_tcp_chk;
static int hf_infiniband_ip_chk;
static int hf_infiniband_fcs;
static int hf_infiniband_ms;
static int hf_infiniband_seg_off;
static int hf_infiniband_seg_id;

static int ett_eoib;

#define MELLANOX_VERSION_FLAG           0x3000
#define MELLANOX_TCP_CHECKSUM_FLAG      0x0C00
#define MELLANOX_IP_CHECKSUM_FLAG       0x0300
#define MELLANOX_FCS_PRESENT_FLAG       0x0040
#define MELLANOX_MORE_SEGMENT_FLAG      0x0020
#define MELLANOX_SEGMENT_FLAG           0x001F

/* Attributes
* Additional Structures for individuala attribute decoding.
* Since they are not headers the naming convention is slightly modified
* Convention: hf_infiniband_[attribute name]_[field]
* This was not entirely necessary but I felt the previous convention
* did not provide adequate readability for the granularity of attribute/attribute fields. */

/* NodeDescription */
static int hf_infiniband_NodeDescription_NodeString;
/* NodeInfo */
static int hf_infiniband_NodeInfo_BaseVersion;
static int hf_infiniband_NodeInfo_ClassVersion;
static int hf_infiniband_NodeInfo_NodeType;
static int hf_infiniband_NodeInfo_NumPorts;
static int hf_infiniband_NodeInfo_SystemImageGUID;
static int hf_infiniband_NodeInfo_NodeGUID;
static int hf_infiniband_NodeInfo_PortGUID;
static int hf_infiniband_NodeInfo_PartitionCap;
static int hf_infiniband_NodeInfo_DeviceID;
static int hf_infiniband_NodeInfo_Revision;
static int hf_infiniband_NodeInfo_LocalPortNum;
static int hf_infiniband_NodeInfo_VendorID;
/* SwitchInfo */
static int hf_infiniband_SwitchInfo_LinearFDBCap;
static int hf_infiniband_SwitchInfo_RandomFDBCap;
static int hf_infiniband_SwitchInfo_MulticastFDBCap;
static int hf_infiniband_SwitchInfo_LinearFDBTop;
static int hf_infiniband_SwitchInfo_DefaultPort;
static int hf_infiniband_SwitchInfo_DefaultMulticastPrimaryPort;
static int hf_infiniband_SwitchInfo_DefaultMulticastNotPrimaryPort;
static int hf_infiniband_SwitchInfo_LifeTimeValue;
static int hf_infiniband_SwitchInfo_PortStateChange;
static int hf_infiniband_SwitchInfo_OptimizedSLtoVLMappingProgramming;
static int hf_infiniband_SwitchInfo_LIDsPerPort;
static int hf_infiniband_SwitchInfo_PartitionEnforcementCap;
static int hf_infiniband_SwitchInfo_InboundEnforcementCap;
static int hf_infiniband_SwitchInfo_OutboundEnforcementCap;
static int hf_infiniband_SwitchInfo_FilterRawInboundCap;
static int hf_infiniband_SwitchInfo_FilterRawOutboundCap;
static int hf_infiniband_SwitchInfo_EnhancedPortZero;
/* GUIDInfo */
/* static int hf_infiniband_GUIDInfo_GUIDBlock;                         */
static int hf_infiniband_GUIDInfo_GUID;
/* PortInfo */
static int hf_infiniband_PortInfo_GidPrefix;
static int hf_infiniband_PortInfo_LID;
static int hf_infiniband_PortInfo_MasterSMLID;
static int hf_infiniband_PortInfo_CapabilityMask;

/* Capability Mask Flags */
static int hf_infiniband_PortInfo_CapabilityMask_SM;
static int hf_infiniband_PortInfo_CapabilityMask_NoticeSupported;
static int hf_infiniband_PortInfo_CapabilityMask_TrapSupported;
static int hf_infiniband_PortInfo_CapabilityMask_OptionalIPDSupported;
static int hf_infiniband_PortInfo_CapabilityMask_AutomaticMigrationSupported;
static int hf_infiniband_PortInfo_CapabilityMask_SLMappingSupported;
static int hf_infiniband_PortInfo_CapabilityMask_MKeyNVRAM;
static int hf_infiniband_PortInfo_CapabilityMask_PKeyNVRAM;
static int hf_infiniband_PortInfo_CapabilityMask_LEDInfoSupported;
static int hf_infiniband_PortInfo_CapabilityMask_SMdisabled;
static int hf_infiniband_PortInfo_CapabilityMask_SystemImageGUIDSupported;
static int hf_infiniband_PortInfo_CapabilityMask_PKeySwitchExternalPortTrapSupported;
static int hf_infiniband_PortInfo_CapabilityMask_CommunicationManagementSupported;
static int hf_infiniband_PortInfo_CapabilityMask_SNMPTunnelingSupported;
static int hf_infiniband_PortInfo_CapabilityMask_ReinitSupported;
static int hf_infiniband_PortInfo_CapabilityMask_DeviceManagementSupported;
static int hf_infiniband_PortInfo_CapabilityMask_VendorClassSupported;
static int hf_infiniband_PortInfo_CapabilityMask_DRNoticeSupported;
static int hf_infiniband_PortInfo_CapabilityMask_CapabilityMaskNoticeSupported;
static int hf_infiniband_PortInfo_CapabilityMask_BootManagementSupported;
static int hf_infiniband_PortInfo_CapabilityMask_LinkRoundTripLatencySupported;
static int hf_infiniband_PortInfo_CapabilityMask_ClientRegistrationSupported;
static int hf_infiniband_PortInfo_CapabilityMask_OtherLocalChangesNoticeSupported;
static int hf_infiniband_PortInfo_CapabilityMask_LinkSpeedWIdthPairsTableSupported;
/* End Capability Mask Flags */


static int hf_infiniband_PortInfo_DiagCode;
static int hf_infiniband_PortInfo_M_KeyLeasePeriod;
static int hf_infiniband_PortInfo_LocalPortNum;
static int hf_infiniband_PortInfo_LinkWidthEnabled;
static int hf_infiniband_PortInfo_LinkWidthSupported;
static int hf_infiniband_PortInfo_LinkWidthActive;
static int hf_infiniband_PortInfo_LinkSpeedSupported;
static int hf_infiniband_PortInfo_PortState;
static int hf_infiniband_PortInfo_PortPhysicalState;
static int hf_infiniband_PortInfo_LinkDownDefaultState;
static int hf_infiniband_PortInfo_M_KeyProtectBits;
static int hf_infiniband_PortInfo_LMC;
static int hf_infiniband_PortInfo_LinkSpeedActive;
static int hf_infiniband_PortInfo_LinkSpeedEnabled;
static int hf_infiniband_PortInfo_NeighborMTU;
static int hf_infiniband_PortInfo_MasterSMSL;
static int hf_infiniband_PortInfo_VLCap;
static int hf_infiniband_PortInfo_M_Key;
static int hf_infiniband_PortInfo_InitType;
static int hf_infiniband_PortInfo_VLHighLimit;
static int hf_infiniband_PortInfo_VLArbitrationHighCap;
static int hf_infiniband_PortInfo_VLArbitrationLowCap;
static int hf_infiniband_PortInfo_InitTypeReply;
static int hf_infiniband_PortInfo_MTUCap;
static int hf_infiniband_PortInfo_VLStallCount;
static int hf_infiniband_PortInfo_HOQLife;
static int hf_infiniband_PortInfo_OperationalVLs;
static int hf_infiniband_PortInfo_PartitionEnforcementInbound;
static int hf_infiniband_PortInfo_PartitionEnforcementOutbound;
static int hf_infiniband_PortInfo_FilterRawInbound;
static int hf_infiniband_PortInfo_FilterRawOutbound;
static int hf_infiniband_PortInfo_M_KeyViolations;
static int hf_infiniband_PortInfo_P_KeyViolations;
static int hf_infiniband_PortInfo_Q_KeyViolations;
static int hf_infiniband_PortInfo_GUIDCap;
static int hf_infiniband_PortInfo_ClientReregister;
static int hf_infiniband_PortInfo_SubnetTimeOut;
static int hf_infiniband_PortInfo_RespTimeValue;
static int hf_infiniband_PortInfo_LocalPhyErrors;
static int hf_infiniband_PortInfo_OverrunErrors;
static int hf_infiniband_PortInfo_MaxCreditHint;
static int hf_infiniband_PortInfo_LinkRoundTripLatency;

/* P_KeyTable */
static int hf_infiniband_P_KeyTable_P_KeyTableBlock;
static int hf_infiniband_P_KeyTable_MembershipType;
static int hf_infiniband_P_KeyTable_P_KeyBase;

/* SLtoVLMappingTable */
static int hf_infiniband_SLtoVLMappingTable_SLtoVL_HighBits;
static int hf_infiniband_SLtoVLMappingTable_SLtoVL_LowBits;

/* VLArbitrationTable */
/* static int hf_infiniband_VLArbitrationTable_VLWeightPairs;           */
static int hf_infiniband_VLArbitrationTable_VL;
static int hf_infiniband_VLArbitrationTable_Weight;

/* LinearForwardingTable */
/* static int hf_infiniband_LinearForwardingTable_LinearForwardingTableBlock;  */
static int hf_infiniband_LinearForwardingTable_Port;

/* RandomForwardingTable */
/* static int hf_infiniband_RandomForwardingTable_RandomForwardingTableBlock;  */
static int hf_infiniband_RandomForwardingTable_LID;
static int hf_infiniband_RandomForwardingTable_Valid;
static int hf_infiniband_RandomForwardingTable_LMC;
static int hf_infiniband_RandomForwardingTable_Port;

/* MulticastForwardingTable */
/* static int hf_infiniband_MulticastForwardingTable_MulticastForwardingTableBlock;    */
static int hf_infiniband_MulticastForwardingTable_PortMask;

/* SMInfo */
static int hf_infiniband_SMInfo_GUID;
static int hf_infiniband_SMInfo_SM_Key;
static int hf_infiniband_SMInfo_ActCount;
static int hf_infiniband_SMInfo_Priority;
static int hf_infiniband_SMInfo_SMState;

/* VendorDiag */
static int hf_infiniband_VendorDiag_NextIndex;
static int hf_infiniband_VendorDiag_DiagData;

/* LedInfo */
static int hf_infiniband_LedInfo_LedMask;

/* LinkSpeedWidthPairsTable */
static int hf_infiniband_LinkSpeedWidthPairsTable_NumTables;
static int hf_infiniband_LinkSpeedWidthPairsTable_PortMask;
static int hf_infiniband_LinkSpeedWidthPairsTable_SpeedTwoFive;
static int hf_infiniband_LinkSpeedWidthPairsTable_SpeedFive;
static int hf_infiniband_LinkSpeedWidthPairsTable_SpeedTen;

/* Attributes for Subnet Administration.
* Mostly we have "Records" here which are just structures of SM attributes.
* There are some unique attributes though that we will want to have a structure for. */

/* NodeRecord */
/* PortInfoRecord */
/* SLtoVLMappingTableRecord */
/* SwitchInfoRecord */
/* LinearForwardingTableRecord */
/* RandomForwardingTableRecord */
/* MulticastForwardingTableRecord */
/* VLArbitrationTableRecord */

static int hf_infiniband_SA_LID;
static int hf_infiniband_SA_EndportLID;
static int hf_infiniband_SA_PortNum;
static int hf_infiniband_SA_InputPortNum;
static int hf_infiniband_SA_OutputPortNum;
static int hf_infiniband_SA_BlockNum_EightBit;
static int hf_infiniband_SA_BlockNum_NineBit;
static int hf_infiniband_SA_BlockNum_SixteenBit;
static int hf_infiniband_SA_Position;
/* static int hf_infiniband_SA_Index;                                   */

/* InformInfoRecord */
static int hf_infiniband_InformInfoRecord_SubscriberGID;
static int hf_infiniband_InformInfoRecord_Enum;

/* InformInfo */
static int hf_infiniband_InformInfo_GID;
static int hf_infiniband_InformInfo_LIDRangeBegin;
static int hf_infiniband_InformInfo_LIDRangeEnd;
static int hf_infiniband_InformInfo_IsGeneric;
static int hf_infiniband_InformInfo_Subscribe;
static int hf_infiniband_InformInfo_Type;
static int hf_infiniband_InformInfo_TrapNumberDeviceID;
static int hf_infiniband_InformInfo_QPN;
static int hf_infiniband_InformInfo_RespTimeValue;
static int hf_infiniband_InformInfo_ProducerTypeVendorID;

/* LinkRecord */
static int hf_infiniband_LinkRecord_FromLID;
static int hf_infiniband_LinkRecord_FromPort;
static int hf_infiniband_LinkRecord_ToPort;
static int hf_infiniband_LinkRecord_ToLID;

/* ServiceRecord */
static int hf_infiniband_ServiceRecord_ServiceID;
static int hf_infiniband_ServiceRecord_ServiceGID;
static int hf_infiniband_ServiceRecord_ServiceP_Key;
static int hf_infiniband_ServiceRecord_ServiceLease;
static int hf_infiniband_ServiceRecord_ServiceKey;
static int hf_infiniband_ServiceRecord_ServiceName;
static int hf_infiniband_ServiceRecord_ServiceData;

/* ServiceAssociationRecord */
static int hf_infiniband_ServiceAssociationRecord_ServiceKey;
static int hf_infiniband_ServiceAssociationRecord_ServiceName;

/* PathRecord */
static int hf_infiniband_PathRecord_DGID;
static int hf_infiniband_PathRecord_SGID;
static int hf_infiniband_PathRecord_DLID;
static int hf_infiniband_PathRecord_SLID;
static int hf_infiniband_PathRecord_RawTraffic;
static int hf_infiniband_PathRecord_FlowLabel;
static int hf_infiniband_PathRecord_HopLimit;
static int hf_infiniband_PathRecord_TClass;
static int hf_infiniband_PathRecord_Reversible;
static int hf_infiniband_PathRecord_NumbPath;
static int hf_infiniband_PathRecord_P_Key;
static int hf_infiniband_PathRecord_SL;
static int hf_infiniband_PathRecord_MTUSelector;
static int hf_infiniband_PathRecord_MTU;
static int hf_infiniband_PathRecord_RateSelector;
static int hf_infiniband_PathRecord_Rate;
static int hf_infiniband_PathRecord_PacketLifeTimeSelector;
static int hf_infiniband_PathRecord_PacketLifeTime;
static int hf_infiniband_PathRecord_Preference;

/* MCMemberRecord */
static int hf_infiniband_MCMemberRecord_MGID;
static int hf_infiniband_MCMemberRecord_PortGID;
static int hf_infiniband_MCMemberRecord_Q_Key;
static int hf_infiniband_MCMemberRecord_MLID;
static int hf_infiniband_MCMemberRecord_MTUSelector;
static int hf_infiniband_MCMemberRecord_MTU;
static int hf_infiniband_MCMemberRecord_TClass;
static int hf_infiniband_MCMemberRecord_P_Key;
static int hf_infiniband_MCMemberRecord_RateSelector;
static int hf_infiniband_MCMemberRecord_Rate;
static int hf_infiniband_MCMemberRecord_PacketLifeTimeSelector;
static int hf_infiniband_MCMemberRecord_PacketLifeTime;
static int hf_infiniband_MCMemberRecord_SL;
static int hf_infiniband_MCMemberRecord_FlowLabel;
static int hf_infiniband_MCMemberRecord_HopLimit;
static int hf_infiniband_MCMemberRecord_Scope;
static int hf_infiniband_MCMemberRecord_JoinState;
static int hf_infiniband_MCMemberRecord_ProxyJoin;

/* TraceRecord */
static int hf_infiniband_TraceRecord_GIDPrefix;
static int hf_infiniband_TraceRecord_IDGeneration;
static int hf_infiniband_TraceRecord_NodeType;
static int hf_infiniband_TraceRecord_NodeID;
static int hf_infiniband_TraceRecord_ChassisID;
static int hf_infiniband_TraceRecord_EntryPortID;
static int hf_infiniband_TraceRecord_ExitPortID;
static int hf_infiniband_TraceRecord_EntryPort;
static int hf_infiniband_TraceRecord_ExitPort;

/* MultiPathRecord */
static int hf_infiniband_MultiPathRecord_RawTraffic;
static int hf_infiniband_MultiPathRecord_FlowLabel;
static int hf_infiniband_MultiPathRecord_HopLimit;
static int hf_infiniband_MultiPathRecord_TClass;
static int hf_infiniband_MultiPathRecord_Reversible;
static int hf_infiniband_MultiPathRecord_NumbPath;
static int hf_infiniband_MultiPathRecord_P_Key;
static int hf_infiniband_MultiPathRecord_SL;
static int hf_infiniband_MultiPathRecord_MTUSelector;
static int hf_infiniband_MultiPathRecord_MTU;
static int hf_infiniband_MultiPathRecord_RateSelector;
static int hf_infiniband_MultiPathRecord_Rate;
static int hf_infiniband_MultiPathRecord_PacketLifeTimeSelector;
static int hf_infiniband_MultiPathRecord_PacketLifeTime;
static int hf_infiniband_MultiPathRecord_IndependenceSelector;
static int hf_infiniband_MultiPathRecord_GIDScope;
static int hf_infiniband_MultiPathRecord_SGIDCount;
static int hf_infiniband_MultiPathRecord_DGIDCount;
static int hf_infiniband_MultiPathRecord_SDGID;

/* ClassPortInfo */
static int hf_infiniband_ClassPortInfo_BaseVersion;
static int hf_infiniband_ClassPortInfo_ClassVersion;
static int hf_infiniband_ClassPortInfo_CapabilityMask;
static int hf_infiniband_ClassPortInfo_CapabilityMask2;
static int hf_infiniband_ClassPortInfo_RespTimeValue;
static int hf_infiniband_ClassPortInfo_RedirectGID;
static int hf_infiniband_ClassPortInfo_RedirectTC;
static int hf_infiniband_ClassPortInfo_RedirectSL;
static int hf_infiniband_ClassPortInfo_RedirectFL;
static int hf_infiniband_ClassPortInfo_RedirectLID;
static int hf_infiniband_ClassPortInfo_RedirectP_Key;
static int hf_infiniband_ClassPortInfo_Reserved;
static int hf_infiniband_ClassPortInfo_RedirectQP;
static int hf_infiniband_ClassPortInfo_RedirectQ_Key;
static int hf_infiniband_ClassPortInfo_TrapGID;
static int hf_infiniband_ClassPortInfo_TrapTC;
static int hf_infiniband_ClassPortInfo_TrapSL;
static int hf_infiniband_ClassPortInfo_TrapFL;
static int hf_infiniband_ClassPortInfo_TrapLID;
static int hf_infiniband_ClassPortInfo_TrapP_Key;
static int hf_infiniband_ClassPortInfo_TrapQP;
static int hf_infiniband_ClassPortInfo_TrapQ_Key;

/* Notice */
static int hf_infiniband_Notice_IsGeneric;
static int hf_infiniband_Notice_Type;
static int hf_infiniband_Notice_ProducerTypeVendorID;
static int hf_infiniband_Notice_TrapNumberDeviceID;
static int hf_infiniband_Notice_IssuerLID;
static int hf_infiniband_Notice_NoticeToggle;
static int hf_infiniband_Notice_NoticeCount;
static int hf_infiniband_Notice_DataDetails;
/* static int hf_infiniband_Notice_IssuerGID;             */
/* static int hf_infiniband_Notice_ClassTrapSpecificData; */

/* ClassPortInfo attribute in Performance class */
static int hf_infiniband_PerfMgt_ClassPortInfo;

/* PortCounters attribute in Performance class */
static int hf_infiniband_PortCounters;
static int hf_infiniband_PortCounters_PortSelect;
static int hf_infiniband_PortCounters_CounterSelect;
static int hf_infiniband_PortCounters_SymbolErrorCounter;
static int hf_infiniband_PortCounters_LinkErrorRecoveryCounter;
static int hf_infiniband_PortCounters_LinkDownedCounter;
static int hf_infiniband_PortCounters_PortRcvErrors;
static int hf_infiniband_PortCounters_PortRcvRemotePhysicalErrors;
static int hf_infiniband_PortCounters_PortRcvSwitchRelayErrors;
static int hf_infiniband_PortCounters_PortXmitDiscards;
static int hf_infiniband_PortCounters_PortXmitConstraintErrors;
static int hf_infiniband_PortCounters_PortRcvConstraintErrors;
static int hf_infiniband_PortCounters_LocalLinkIntegrityErrors;
static int hf_infiniband_PortCounters_ExcessiveBufferOverrunErrors;
static int hf_infiniband_PortCounters_VL15Dropped;
static int hf_infiniband_PortCounters_PortXmitData;
static int hf_infiniband_PortCounters_PortRcvData;
static int hf_infiniband_PortCounters_PortXmitPkts;
static int hf_infiniband_PortCounters_PortRcvPkts;

/* Extended PortCounters attribute in Performance class */
static int hf_infiniband_PortCountersExt;
static int hf_infiniband_PortCountersExt_PortSelect;
static int hf_infiniband_PortCountersExt_CounterSelect;
static int hf_infiniband_PortCountersExt_PortXmitData;
static int hf_infiniband_PortCountersExt_PortRcvData;
static int hf_infiniband_PortCountersExt_PortXmitPkts;
static int hf_infiniband_PortCountersExt_PortRcvPkts;
static int hf_infiniband_PortCountersExt_PortUnicastXmitPkts;
static int hf_infiniband_PortCountersExt_PortUnicastRcvPkts;
static int hf_infiniband_PortCountersExt_PortMulticastXmitPkts;
static int hf_infiniband_PortCountersExt_PortMulticastRcvPkts;

/* Notice DataDetails and ClassTrapSpecific Data for certain traps
* Note that traps reuse many fields, so they are only declared once under the first trap that they appear.
* There is no need to redeclare them for specific Traps (as with other SA Attributes) because they are uniform between Traps. */

/* Parse DataDetails for a given Trap */
static int parse_NoticeDataDetails(proto_tree*, tvbuff_t*, int *offset, uint16_t trapNumber);

/* Traps 64,65,66,67 */
static int hf_infiniband_Trap_GIDADDR;

/* Traps 68,69 */
/* DataDetails */
static int hf_infiniband_Trap_COMP_MASK;
static int hf_infiniband_Trap_WAIT_FOR_REPATH;
/* ClassTrapSpecificData */
/* static int hf_infiniband_Trap_PATH_REC;                              */

/* Trap 128 */
static int hf_infiniband_Trap_LIDADDR;

/* Trap 129, 130, 131 */
static int hf_infiniband_Trap_PORTNO;

/* Trap 144 */
static int hf_infiniband_Trap_OtherLocalChanges;
static int hf_infiniband_Trap_CAPABILITYMASK;
static int hf_infiniband_Trap_LinkSpeecEnabledChange;
static int hf_infiniband_Trap_LinkWidthEnabledChange;
static int hf_infiniband_Trap_NodeDescriptionChange;

/* Trap 145 */
static int hf_infiniband_Trap_SYSTEMIMAGEGUID;

/* Trap 256 */
static int hf_infiniband_Trap_DRSLID;
static int hf_infiniband_Trap_METHOD;
static int hf_infiniband_Trap_ATTRIBUTEID;
static int hf_infiniband_Trap_ATTRIBUTEMODIFIER;
static int hf_infiniband_Trap_MKEY;
static int hf_infiniband_Trap_DRNotice;
static int hf_infiniband_Trap_DRPathTruncated;
static int hf_infiniband_Trap_DRHopCount;
static int hf_infiniband_Trap_DRNoticeReturnPath;

/* Trap 257, 258 */
static int hf_infiniband_Trap_LIDADDR1;
static int hf_infiniband_Trap_LIDADDR2;
static int hf_infiniband_Trap_KEY;
static int hf_infiniband_Trap_SL;
static int hf_infiniband_Trap_QP1;
static int hf_infiniband_Trap_QP2;
static int hf_infiniband_Trap_GIDADDR1;
static int hf_infiniband_Trap_GIDADDR2;

/* Trap 259 */
static int hf_infiniband_Trap_DataValid;
static int hf_infiniband_Trap_PKEY;
static int hf_infiniband_Trap_SWLIDADDR;

/* Infiniband Link */
static int hf_infiniband_link_op;
static int hf_infiniband_link_fctbs;
static int hf_infiniband_link_vl;
static int hf_infiniband_link_fccl;
static int hf_infiniband_link_lpcrc;

/* RC Send reassembling */
static reassembly_table infiniband_rc_send_reassembly_table;
static int ett_infiniband_rc_send_fragment;
static int ett_infiniband_rc_send_fragments;
static int hf_infiniband_rc_send_fragments;
static int hf_infiniband_rc_send_fragment;
static int hf_infiniband_rc_send_fragment_overlap;
static int hf_infiniband_rc_send_fragment_overlap_conflict;
static int hf_infiniband_rc_send_fragment_multiple_tails;
static int hf_infiniband_rc_send_fragment_too_long_fragment;
static int hf_infiniband_rc_send_fragment_error;
static int hf_infiniband_rc_send_fragment_count;
static int hf_infiniband_rc_send_reassembled_in;
static int hf_infiniband_rc_send_reassembled_length;
static int hf_infiniband_rc_send_reassembled_data;
static const fragment_items infiniband_rc_send_frag_items = {
	&ett_infiniband_rc_send_fragment,
	&ett_infiniband_rc_send_fragments,
	&hf_infiniband_rc_send_fragments,
	&hf_infiniband_rc_send_fragment,
	&hf_infiniband_rc_send_fragment_overlap,
	&hf_infiniband_rc_send_fragment_overlap_conflict,
	&hf_infiniband_rc_send_fragment_multiple_tails,
	&hf_infiniband_rc_send_fragment_too_long_fragment,
	&hf_infiniband_rc_send_fragment_error,
	&hf_infiniband_rc_send_fragment_count,
	&hf_infiniband_rc_send_reassembled_in,
	&hf_infiniband_rc_send_reassembled_length,
	&hf_infiniband_rc_send_reassembled_data,
	"RC Send fragments"
};

/* Trap Type/Descriptions for dissection */
static const value_string Operand_Description[]= {
    { 0, " Normal Flow Control"},
    { 1, " Flow Control Init"},
    { 0, NULL}
};

/* Trap Type/Descriptions for dissection */
static const value_string Trap_Description[]= {
    { 64, " (Informational) <GIDADDR> is now in service"},
    { 65, " (Informational) <GIDADDR> is out of service"},
    { 66, " (Informational) New Multicast Group with multicast address <GIDADDR> is now created"},
    { 67, " (Informational) Multicast Group with multicast address <GIDADDR> is now deleted"},
    { 68, " (Informational) Paths indicated by <PATH_REC> and <COMP_MASK> are no longer valid"},
    { 69, " (Informational) Paths indicated by <PATH_REC> and <COMP_MASK> have been recomputed"},
    { 128, " (Urgent) Link State of at least one port of switch at <LIDADDR> has changed"},
    { 129, " (Urgent) Local Link Integrity threshold reached at <LIDADDR><PORTNO>"},
    { 130, " (Urgent) Excessive Buffer OVerrun threshold reached at <LIDADDR><PORTNO>"},
    { 131, " (Urgent) Flow Control Update watchdog timer expired at <LIDADDR><PORTNO>"},
    { 144, " (Informational) CapMask, NodeDesc, LinkWidthEnabled or LinkSpeedEnabled at <LIDADDR> has been modified"},
    { 145, " (Informational) SystemImageGUID at <LIDADDR> has been modified.  New value is <SYSTEMIMAGEGUID>"},
    { 256, " (Security) Bad M_Key, <M_KEY> from <LIDADDR> attempted <METHOD> with <ATTRIBUTEID> and <ATTRIBUTEMODIFIER>"},
    { 257, " (Security) Bad P_Key, <KEY> from <LIDADDR1><GIDADDR1><QP1> to <LIDADDR2><GIDADDR2><QP2> on <SL>"},
    { 258, " (Security) Bad Q_Key, <KEY> from <LIDADDR1><GIDADDR1><QP1> to <LIDADDR2><GIDADDR2><QP2> on <SL>"},
    { 259, " (Security) Bad P_Key, <KEY> from <LIDADDR1><GIDADDR1><QP1> to <LIDADDR2><GIDADDR2><QP2> on <SL> at switch <LIDADDR><PORTNO>"},
    { 0, NULL}
};


static const value_string bth_opcode_tbl[] = {
    { 0x0, "Reliable Connection (RC) - SEND First" },
    { 0x1, "Reliable Connection (RC) - SEND Middle" },
    { 0x2, "Reliable Connection (RC) - SEND Last" },
    { 0x3, "Reliable Connection (RC) - SEND Last with Immediate" },
    { 0x4, "Reliable Connection (RC) - SEND Only" },
    { 0x5, "Reliable Connection (RC) - SEND Only with Immediate" },
    { 0x6, "Reliable Connection (RC) - RDMA WRITE First" },
    { 0x7, "Reliable Connection (RC) - RDMA WRITE Middle" },
    { 0x8, "Reliable Connection (RC) - RDMA WRITE Last" },
    { 0x9, "Reliable Connection (RC) - RDMA WRITE Last with Immediate" },
    { 0xA, "Reliable Connection (RC) - RDMA WRITE Only" },
    { 0xB, "Reliable Connection (RC) - RDMA WRITE Only with Immediate" },
    { 0xC, "Reliable Connection (RC) - RDMA READ Request" },
    { 0xD, "Reliable Connection (RC) - RDMA READ response First" },
    { 0xE, "Reliable Connection (RC) - RDMA READ response Middle" },
    { 0xF, "Reliable Connection (RC) - RDMA READ response Last" },
    { 0x10, "Reliable Connection (RC) - RDMA READ response Only" },
    { 0x11, "Reliable Connection (RC) - Acknowledge" },
    { 0x12, "Reliable Connection (RC) - ATOMIC Acknowledge" },
    { 0x13, "Reliable Connection (RC) - CmpSwap" },
    { 0x14, "Reliable Connection (RC) - FetchAdd" },
    { 0x15, "Reliable Connection (RC) - Reserved" },
    { 0x16, "Reliable Connection (RC) - SEND Last with Invalidate" },
    { 0x17, "Reliable Connection (RC) - SEND Only with Invalidate" },
    { 0x1C, "Reliable Connection (RC) - FLUSH" },
    { 0x1D, "Reliable Connection (RC) - ATOMIC WRITE" },
    { 0x20, "Unreliable Connection (UC) - SEND First" },
    { 0x21, "Unreliable Connection (UC) - SEND Middle" },
    { 0x22, "Unreliable Connection (UC) - SEND Last" },
    { 0x23, "Unreliable Connection (UC) - SEND Last with Immediate" },
    { 0x24, "Unreliable Connection (UC) - SEND Only" },
    { 0x25, "Unreliable Connection (UC) - SEND Only with Immediate" },
    { 0x26, "Unreliable Connection (UC) - RDMA WRITE First" },
    { 0x27, "Unreliable Connection (UC) - RDMA WRITE Middle" },
    { 0x28, "Unreliable Connection (UC) - RDMA WRITE Last" },
    { 0x29, "Unreliable Connection (UC) - RDMA WRITE Last with Immediate" },
    { 0x2A, "Unreliable Connection (UC) - RDMA WRITE Only" },
    { 0x2B, "Unreliable Connection (UC) - RDMA WRITE Only with Immediate" },
    { 0x40, "Reliable Datagram (RD) - SEND First" },
    { 0x41, "Reliable Datagram (RD) - SEND Middle" },
    { 0x42, "Reliable Datagram (RD) - SEND Last" },
    { 0x43, "Reliable Datagram (RD) - SEND Last with Immediate" },
    { 0x44, "Reliable Datagram (RD) - SEND Only" },
    { 0x45, "Reliable Datagram (RD) - SEND Only with Immediate" },
    { 0x46, "Reliable Datagram (RD) - RDMA WRITE First" },
    { 0x47, "Reliable Datagram (RD) - RDMA WRITE Middle" },
    { 0x48, "Reliable Datagram (RD) - RDMA WRITE Last" },
    { 0x49, "Reliable Datagram (RD) - RDMA WRITE Last with Immediate" },
    { 0x4A, "Reliable Datagram (RD) - RDMA WRITE Only" },
    { 0x4B, "Reliable Datagram (RD) - RDMA WRITE Only with Immediate" },
    { 0x4C, "Reliable Datagram (RD) - RDMA READ Request" },
    { 0x4D, "Reliable Datagram (RD) - RDMA READ response First" },
    { 0x4E, "Reliable Datagram (RD) - RDMA READ response Middle" },
    { 0x4F, "Reliable Datagram (RD) - RDMA READ response Last" },
    { 0x50, "Reliable Datagram (RD) - RDMA READ response Only" },
    { 0x51, "Reliable Datagram (RD) - Acknowledge" },
    { 0x52, "Reliable Datagram (RD) - ATOMIC Acknowledge" },
    { 0x53, "Reliable Datagram (RD) - CmpSwap" },
    { 0x54, "Reliable Datagram (RD) - FetchAdd" },
    { 0x55, "Reliable Datagram (RD) - RESYNC" },
    { 0x5C, "Reliable Datagram (RD) - FLUSH" },
    { 0x5D, "Reliable Datagram (RD) - ATOMIC WRITE" },
    { 0x64, "Unreliable Datagram (UD) - SEND only" },
    { 0x65, "Unreliable Datagram (UD) - SEND only with Immediate" },
    { 0x80, "CNP" },
    { 0xA0, "Extended Reliable Connection (XRC) - SEND First" },
    { 0xA1, "Extended Reliable Connection (XRC) - SEND Middle" },
    { 0xA2, "Extended Reliable Connection (XRC) - SEND Last" },
    { 0xA3, "Extended Reliable Connection (XRC) - SEND Last with Immediate" },
    { 0xA4, "Extended Reliable Connection (XRC) - SEND Only" },
    { 0xA5, "Extended Reliable Connection (XRC) - SEND Only with Immediate" },
    { 0xA6, "Extended Reliable Connection (XRC) - RDMA WRITE First" },
    { 0xA7, "Extended Reliable Connection (XRC) - RDMA WRITE Middle" },
    { 0xA8, "Extended Reliable Connection (XRC) - RDMA WRITE Last" },
    { 0xA9, "Extended Reliable Connection (XRC) - RDMA WRITE Last with Immediate" },
    { 0xAA, "Extended Reliable Connection (XRC) - RDMA WRITE Only" },
    { 0xAB, "Extended Reliable Connection (XRC) - RDMA WRITE Only with Immediate" },
    { 0xAC, "Extended Reliable Connection (XRC) - RDMA READ Request" },
    { 0xAD, "Extended Reliable Connection (XRC) - RDMA READ response First" },
    { 0xAE, "Extended Reliable Connection (XRC) - RDMA READ response Middle" },
    { 0xAF, "Extended Reliable Connection (XRC) - RDMA READ response Last" },
    { 0xB0, "Extended Reliable Connection (XRC) - RDMA READ response Only" },
    { 0xB1, "Extended Reliable Connection (XRC) - Acknowledge" },
    { 0xB2, "Extended Reliable Connection (XRC) - ATOMIC Acknowledge" },
    { 0xB3, "Extended Reliable Connection (XRC) - CmpSwap" },
    { 0xB4, "Extended Reliable Connection (XRC) - FetchAdd" },
    { 0xB6, "Extended Reliable Connection (XRC) - SEND Last with Invalidate" },
    { 0xB7, "Extended Reliable Connection (XRC) - SEND Only with Invalidate" },
    { 0xBC, "Extended Reliable Connection (XRC) - FLUSH" },
    { 0xBD, "Extended Reliable Connection (XRC) - ATOMIC WRITE" },
    { 0, NULL}
};

#define AETH_SYNDROME_OPCODE_ACK 0
#define AETH_SYNDROME_OPCODE_RNR_NAK 1
#define AETH_SYNDROME_OPCODE_RES 2
#define AETH_SYNDROME_OPCODE_NAK 3

static const value_string aeth_syndrome_opcode_vals[]= {
    { AETH_SYNDROME_OPCODE_ACK, "Ack"},
    { AETH_SYNDROME_OPCODE_RNR_NAK, "RNR Nak"},
    { AETH_SYNDROME_OPCODE_RES, "Reserved"},
    { AETH_SYNDROME_OPCODE_NAK, "Nak"},
    { 0, NULL}
};

static const value_string aeth_syndrome_nak_error_code_vals[]= {
    { 0, "PSN Sequence Error"},
    { 1, "Invalid Request"},
    { 2, "Remote Access Error"},
    { 3, "Remote Operational Error"},
    { 4, "Invalid RD Request"},
    { 0, NULL}
};

static const value_string aeth_syndrome_timer_code_vals[]= {
    { 0, "655.36 ms"},
    { 1, "0.01 ms"},
    { 2, "0.02 ms"},
    { 3, "0.03 ms"},
    { 4, "0.04 ms"},
    { 5, "0.06 ms"},
    { 6, "0.08 ms"},
    { 7, "0.12 ms"},
    { 8, "0.16 ms"},
    { 9, "0.24 ms"},
    { 10, "0.32 ms"},
    { 11, "0.48 ms"},
    { 12, "0.64 ms"},
    { 13, "0.96 ms"},
    { 14, "1.28 ms"},
    { 15, "1.92 ms"},
    { 16, "2.56 ms"},
    { 17, "3.84 ms"},
    { 18, "5.12 ms"},
    { 19, "7.68 ms"},
    { 20, "10.24 ms"},
    { 21, "15.36 ms"},
    { 22, "20.48 ms"},
    { 23, "30.72 ms"},
    { 24, "40.96 ms"},
    { 25, "61.44 ms"},
    { 26, "81.92 ms"},
    { 27, "122.88 ms"},
    { 28, "163.84 ms"},
    { 29, "245.76 ms"},
    { 30, "327.68 ms"},
    { 31, "491.52 ms"},
    { 0, NULL}
};

/* MAD Management Classes
* Classes from the Common MAD Header
*
*      Management Class Name        Class Description
* ------------------------------------------------------------------------------------------------------------ */
#define SUBN_LID_ROUTED 0x01        /* Subnet Management LID Route */
#define SUBN_DIRECTED_ROUTE 0x81    /* Subnet Management Directed Route */
#define SUBNADMN 0x03               /* Subnet Administration */
#define PERF 0x04                   /* Performance Management */
#define BM 0x05                     /* Baseboard Management (Tunneling of IB-ML commands through the IBA subnet) */
#define DEV_MGT 0x06                /* Device Management */
#define COM_MGT 0x07                /* Communication Management */
#define SNMP 0x08                   /* SNMP Tunneling (tunneling of the SNMP protocol through the IBA fabric) */
#define VENDOR_1_START 0x09         /* Start of first Vendor Specific Range */
#define VENDOR_1_END 0x0F           /* End of first Vendor Specific Range */
#define VENDOR_2_START 0x30         /* Start of second Vendor Specific Range */
#define VENDOR_2_END 0x4F           /* End of the second Vendor Specific Range */
#define APPLICATION_START 0x10      /* Start of Application Specific Range */
#define APPLICATION_END 0x2F        /* End of Application Specific Range */

/* Performance class Attributes */
#define ATTR_PORT_COUNTERS      0x0012
#define ATTR_PORT_COUNTERS_EXT  0x001D

/* Link Next Header Values */
#define IBA_GLOBAL 3
#define IBA_LOCAL  2
#define IP_NON_IBA 1
#define RAW        0

static const value_string OpCodeMap[] =
{
    { RC_SEND_FIRST,                "RC Send First " },
    { RC_SEND_MIDDLE,               "RC Send Middle "},
    { RC_SEND_LAST,                 "RC Send Last " },
    { RC_SEND_LAST_IMM,             "RC Send Last Immediate "},
    { RC_SEND_ONLY,                 "RC Send Only "},
    { RC_SEND_ONLY_IMM,             "RC Send Only Immediate "},
    { RC_RDMA_WRITE_FIRST,          "RC RDMA Write First " },
    { RC_RDMA_WRITE_MIDDLE,         "RC RDMA Write Middle "},
    { RC_RDMA_WRITE_LAST,           "RC RDMA Write Last "},
    { RC_RDMA_WRITE_LAST_IMM,       "RC RDMA Write Last Immediate " },
    { RC_RDMA_WRITE_ONLY,           "RC RDMA Write Only " },
    { RC_RDMA_WRITE_ONLY_IMM,       "RC RDMA Write Only Immediate "},
    { RC_RDMA_READ_REQUEST,         "RC RDMA Read Request " },
    { RC_RDMA_READ_RESPONSE_FIRST,  "RC RDMA Read Response First " },
    { RC_RDMA_READ_RESPONSE_MIDDLE, "RC RDMA Read Response Middle "},
    { RC_RDMA_READ_RESPONSE_LAST,   "RC RDMA Read Response Last " },
    { RC_RDMA_READ_RESPONSE_ONLY,   "RC RDMA Read Response Only "},
    { RC_ACKNOWLEDGE,               "RC Acknowledge " },
    { RC_ATOMIC_ACKNOWLEDGE,        "RC Atomic Acknowledge " },
    { RC_CMP_SWAP,                  "RC Compare Swap " },
    { RC_FETCH_ADD,                 "RC Fetch Add "},
    { RC_SEND_LAST_INVAL,           "RC Send Last Invalidate "},
    { RC_SEND_ONLY_INVAL,           "RC Send Only Invalidate " },
    { RC_FLUSH,                     "RC Flush " },
    { RC_ATOMIC_WRITE,              "RC Atomic Write " },

    { RD_SEND_FIRST,                "RD Send First "},
    { RD_SEND_MIDDLE,               "RD Send Middle " },
    { RD_SEND_LAST,                 "RD Send Last "},
    { RD_SEND_LAST_IMM,             "RD Last Immediate " },
    { RD_SEND_ONLY,                 "RD Send Only "},
    { RD_SEND_ONLY_IMM,             "RD Send Only Immediate "},
    { RD_RDMA_WRITE_FIRST,          "RD RDMA Write First "},
    { RD_RDMA_WRITE_MIDDLE,         "RD RDMA Write Middle "},
    { RD_RDMA_WRITE_LAST,           "RD RDMA Write Last "},
    { RD_RDMA_WRITE_LAST_IMM,       "RD RDMA Write Last Immediate "},
    { RD_RDMA_WRITE_ONLY,           "RD RDMA Write Only "},
    { RD_RDMA_WRITE_ONLY_IMM,       "RD RDMA Write Only Immediate "},
    { RD_RDMA_READ_REQUEST,         "RD RDMA Read Request "},
    { RD_RDMA_READ_RESPONSE_FIRST,  "RD RDMA Read Response First "},
    { RD_RDMA_READ_RESPONSE_MIDDLE, "RD RDMA Read Response Middle "},
    { RD_RDMA_READ_RESPONSE_LAST,   "RD RDMA Read Response Last "},
    { RD_RDMA_READ_RESPONSE_ONLY,   "RD RDMA Read Response Only "},
    { RD_ACKNOWLEDGE,               "RD Acknowledge "},
    { RD_ATOMIC_ACKNOWLEDGE,        "RD Atomic Acknowledge "},
    { RD_CMP_SWAP,                  "RD Compare Swap "},
    { RD_FETCH_ADD,                 "RD Fetch Add "},
    { RD_RESYNC,                    "RD RESYNC "},
    { RD_FLUSH,                     "RD Flush "},
    { RD_ATOMIC_WRITE,              "RD Atomic Write " },

    { UD_SEND_ONLY,                 "UD Send Only "},
    { UD_SEND_ONLY_IMM,             "UD Send Only Immediate "},

    { UC_SEND_FIRST,                "UC Send First "},
    { UC_SEND_MIDDLE,               "UC Send Middle "},
    { UC_SEND_LAST,                 "UC Send Last "},
    { UC_SEND_LAST_IMM,             "UC Send Last Immediate "},
    { UC_SEND_ONLY,                 "UC Send Only "},
    { UC_SEND_ONLY_IMM,             "UC Send Only Immediate "},
    { UC_RDMA_WRITE_FIRST,          "UC RDMA Write First"},
    { UC_RDMA_WRITE_MIDDLE,         "UC RDMA Write Middle "},
    { UC_RDMA_WRITE_LAST,           "UC RDMA Write Last "},
    { UC_RDMA_WRITE_LAST_IMM,       "UC RDMA Write Last Immediate "},
    { UC_RDMA_WRITE_ONLY,           "UC RDMA Write Only "},
    { UC_RDMA_WRITE_ONLY_IMM,       "UC RDMA Write Only Immediate "},
    { 0, NULL}
};

/* Mellanox DCT has the same opcodes as RD so will use the same RD macros */
static const value_string DctOpCodeMap[] =
{
    { RD_SEND_FIRST,                "DC Send First "},
    { RD_SEND_MIDDLE,               "DC Send Middle " },
    { RD_SEND_LAST,                 "DC Send Last "},
    { RD_SEND_LAST_IMM,             "DC Last Immediate " },
    { RD_SEND_ONLY,                 "DC Send Only "},
    { RD_SEND_ONLY_IMM,             "DC Send Only Immediate "},
    { RD_RDMA_WRITE_FIRST,          "DC RDMA Write First "},
    { RD_RDMA_WRITE_MIDDLE,         "DC RDMA Write Middle "},
    { RD_RDMA_WRITE_LAST,           "DC RDMA Write Last "},
    { RD_RDMA_WRITE_LAST_IMM,       "DC RDMA Write Last Immediate "},
    { RD_RDMA_WRITE_ONLY,           "DC RDMA Write Only "},
    { RD_RDMA_WRITE_ONLY_IMM,       "DC RDMA Write Only Immediate "},
    { RD_RDMA_READ_REQUEST,         "DC RDMA Read Request "},
    { RD_RDMA_READ_RESPONSE_FIRST,  "DC RDMA Read Response First "},
    { RD_RDMA_READ_RESPONSE_MIDDLE, "DC RDMA Read Response Middle "},
    { RD_RDMA_READ_RESPONSE_LAST,   "DC RDMA Read Response Last "},
    { RD_RDMA_READ_RESPONSE_ONLY,   "DC RDMA Read Response Only "},
    { RD_ACKNOWLEDGE,               "DC Acknowledge "},
    { RD_ATOMIC_ACKNOWLEDGE,        "DC Atomic Acknowledge "},
    { RD_CMP_SWAP,                  "DC Compare Swap "},
    { RD_FETCH_ADD,                 "DC Fetch Add "},
    { RD_RESYNC,                    "DC Unknown Opcode "},
    { 0, NULL}
};

/* Header Ordering Based on OPCODES
* These are simply an enumeration of the possible header combinations defined by the IB Spec.
* These enumerations
* #DEFINE [HEADER_ORDER]         [ENUM]
* __________________________________ */
#define RDETH_DETH_PAYLD            0
/* __________________________________ */
#define RDETH_DETH_RETH_PAYLD       1
/* __________________________________ */
#define RDETH_DETH_IMMDT_PAYLD      2
/* __________________________________ */
#define RDETH_DETH_RETH_IMMDT_PAYLD 3
/* __________________________________ */
#define RDETH_DETH_RETH             4
/* __________________________________ */
#define RDETH_AETH_PAYLD            5
/* __________________________________ */
#define RDETH_PAYLD                 6
/* __________________________________ */
#define RDETH_AETH                  7
/* __________________________________ */
#define RDETH_AETH_ATOMICACKETH     8
/* __________________________________ */
#define RDETH_DETH_ATOMICETH        9
/* ___________________________________ */
#define RDETH_DETH                  10
/* ___________________________________ */
#define DETH_PAYLD                  11
/* ___________________________________ */
#define DETH_IMMDT_PAYLD            12
/* ___________________________________ */
#define PAYLD                       13
/* ___________________________________ */
#define IMMDT_PAYLD                 14
/* ___________________________________ */
#define RETH_PAYLD                  15
/* ___________________________________ */
#define RETH_IMMDT_PAYLD            16
/* ___________________________________ */
#define RETH                        17
/* ___________________________________ */
#define AETH_PAYLD                  18
/* ___________________________________ */
#define AETH                        19
/* ___________________________________ */
#define AETH_ATOMICACKETH           20
/* ___________________________________ */
#define ATOMICETH                   21
/* ___________________________________ */
#define IETH_PAYLD                  22
/* ___________________________________ */
#define DCCETH                      23
/* ___________________________________ */
#define FETH_RETH                   24
/* ___________________________________ */
#define RDETH_FETH_RETH             25
/* ___________________________________ */
#define RDETH_RETH_PAYLD            26
/* ___________________________________ */


/* Infiniband transport services
   These are an enumeration of the transport services over which an IB packet
   might be sent. The values match the corresponding 3 bits of the opCode field
   in the BTH  */
#define TRANSPORT_RC    0
#define TRANSPORT_UC    1
#define TRANSPORT_RD    2
#define TRANSPORT_UD    3

#define AETH_SYNDROME_RES 0x80
#define AETH_SYNDROME_OPCODE 0x60
#define AETH_SYNDROME_VALUE 0x1F


/* Array of all availavle OpCodes to make matching a bit easier.
* The OpCodes dictate the header sequence following in the packet.
* These arrays tell the dissector which headers must be decoded for the given OpCode. */
static uint32_t opCode_RDETH_DETH_ATOMICETH[] = {
 RD_CMP_SWAP,
 RD_FETCH_ADD
};
static uint32_t opCode_IETH_PAYLD[] = {
 RC_SEND_LAST_INVAL,
 RC_SEND_ONLY_INVAL
};
static uint32_t opCode_ATOMICETH[] = {
 RC_CMP_SWAP,
 RC_FETCH_ADD
};
static uint32_t opCode_RDETH_DETH_RETH_PAYLD[] = {
 RD_RDMA_WRITE_FIRST,
 RD_RDMA_WRITE_ONLY
};
static uint32_t opCode_RETH_IMMDT_PAYLD[] = {
 RC_RDMA_WRITE_ONLY_IMM,
 UC_RDMA_WRITE_ONLY_IMM
};
static uint32_t opCode_RDETH_DETH_IMMDT_PAYLD[] = {
 RD_SEND_LAST_IMM,
 RD_SEND_ONLY_IMM,
 RD_RDMA_WRITE_LAST_IMM
};

static uint32_t opCode_RDETH_AETH_PAYLD[] = {
 RD_RDMA_READ_RESPONSE_FIRST,
 RD_RDMA_READ_RESPONSE_LAST,
 RD_RDMA_READ_RESPONSE_ONLY
};
static uint32_t opCode_AETH_PAYLD[] = {
 RC_RDMA_READ_RESPONSE_FIRST,
 RC_RDMA_READ_RESPONSE_LAST,
 RC_RDMA_READ_RESPONSE_ONLY
};
static uint32_t opCode_RETH_PAYLD[] = {
 RC_RDMA_WRITE_FIRST,
 RC_RDMA_WRITE_ONLY,
 UC_RDMA_WRITE_FIRST,
 UC_RDMA_WRITE_ONLY
};

static uint32_t opCode_RDETH_DETH_PAYLD[] = {
 RD_SEND_FIRST,
 RD_SEND_MIDDLE,
 RD_SEND_LAST,
 RD_SEND_ONLY,
 RD_RDMA_WRITE_MIDDLE,
 RD_RDMA_WRITE_LAST
};

static uint32_t opCode_IMMDT_PAYLD[] = {
 RC_SEND_LAST_IMM,
 RC_SEND_ONLY_IMM,
 RC_RDMA_WRITE_LAST_IMM,
 UC_SEND_LAST_IMM,
 UC_SEND_ONLY_IMM,
 UC_RDMA_WRITE_LAST_IMM
};

static uint32_t opCode_PAYLD[] = {
 RC_SEND_FIRST,
 RC_SEND_MIDDLE,
 RC_SEND_LAST,
 RC_SEND_ONLY,
 RC_RDMA_WRITE_MIDDLE,
 RC_RDMA_WRITE_LAST,
 RC_RDMA_READ_RESPONSE_MIDDLE,
 UC_SEND_FIRST,
 UC_SEND_MIDDLE,
 UC_SEND_LAST,
 UC_SEND_ONLY,
 UC_RDMA_WRITE_MIDDLE,
 UC_RDMA_WRITE_LAST
};

/* It is not necessary to create arrays for these OpCodes since they indicate only one further header.
*  We can just decode it directly

* static uint32_t opCode_DETH_IMMDT_PAYLD[] = {
* UD_SEND_ONLY_IMM
* };
* static uint32_t opCode_DETH_PAYLD[] = {
* UD_SEND_ONLY
* };
* static uint32_t opCode_RDETH_DETH[] = {
* RD_RESYNC
* };
* static uint32_t opCode_RDETH_DETH_RETH[] = {
* RD_RDMA_READ_REQUEST
* };
* static uint32_t opCode_RDETH_DETH_RETH_IMMDT_PAYLD[] = {
* RD_RDMA_WRITE_ONLY_IMM
* };
* static uint32_t opCode_RDETH_AETH_ATOMICACKETH[] = {
* RD_ATOMIC_ACKNOWLEDGE
* };
* static uint32_t opCode_RDETH_AETH[] = {
* RD_ACKNOWLEDGE
* };
* static uint32_t opCode_RDETH_PAYLD[] = {
* RD_RDMA_READ_RESPONSE_MIDDLE
* };
* static uint32_t opCode_AETH_ATOMICACKETH[] = {
* RC_ATOMIC_ACKNOWLEDGE
* };
* static uint32_t opCode_RETH[] = {
* RC_RDMA_READ_REQUEST
* };
* static uint32_t opCode_AETH[] = {
* RC_ACKNOWLEDGE
* }; */

/* settings to be set by the user via the preferences dialog */
static unsigned pref_rroce_udp_port = DEFAULT_RROCE_UDP_PORT;
static bool try_heuristic_first = true;

/* saves information about connections that have been/are in the process of being
   negotiated via ConnectionManagement packets */
typedef struct {
    uint8_t req_gid[GID_SIZE],
           resp_gid[GID_SIZE];  /* GID of requester/responder, respectively */
    uint16_t req_lid,
            resp_lid;           /* LID of requester/responder, respectively */
    uint32_t req_qp,
            resp_qp;            /* QP number of requester/responder, respectively */
    uint64_t service_id;         /* service id for this connection */
} connection_context;

/* holds a table of connection contexts being negotiated by CM. the key is a obtained
   using add_address_to_hash64(initiator address, TransactionID) */
static GHashTable *CM_context_table;

/* heuristics sub-dissectors list for dissecting the data payload of IB packets */
static heur_dissector_list_t heur_dissectors_payload;
/* heuristics sub-dissectors list for dissecting the PrivateData of CM packets */
static heur_dissector_list_t heur_dissectors_cm_private;

/* ----- This sections contains various utility functions indirectly related to Infiniband dissection ---- */
static void infiniband_payload_prompt(packet_info *pinfo _U_, char* result)
{
    snprintf(result, MAX_DECODE_AS_PROMPT_LEN, "Dissect Infiniband payload as");
}

static void table_destroy_notify(void *data) {
    g_free(data);
}

/* --------------------------------------------------------------------------------------------------------*/
/* Helper dissector for correctly dissecting RRoCE packets (encapsulated within an IP */
/* frame). The only difference from regular IB packets is that RRoCE packets do not contain */
/* a LRH, and always start with a BTH.                                                      */
static int
dissect_rroce(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    /* this is a RRoCE packet, so signal the IB dissector not to look for LRH/GRH */
    dissect_infiniband_common(tvb, pinfo, tree, IB_PACKET_STARTS_WITH_BTH);
    return tvb_captured_length(tvb);
}

/* Helper dissector for correctly dissecting RoCE packets (encapsulated within an Ethernet */
/* frame). The only difference from regular IB packets is that RoCE packets do not contain */
/* a LRH, and always start with a GRH.                                                      */
static int
dissect_roce(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    /* this is a RoCE packet, so signal the IB dissector not to look for LRH */
    dissect_infiniband_common(tvb, pinfo, tree, IB_PACKET_STARTS_WITH_GRH);
    return tvb_captured_length(tvb);
}

static int
dissect_infiniband(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    dissect_infiniband_common(tvb, pinfo, tree, IB_PACKET_STARTS_WITH_LRH);
    return tvb_captured_length(tvb);
}

/* Common Dissector for both InfiniBand and RoCE packets
 * IN:
 *       tvb - The tvbbuff of packet data
 *       pinfo - The packet info structure with column information
 *       tree - The tree structure under which field nodes are to be added
 *       starts_with - regular IB packet starts with LRH, ROCE starts with GRH and RROCE starts with BTH,
 *                     this tells the parser what headers of (LRH/GRH) to skip.
 * Notes:
 * 1.) Floating "offset+=" statements should probably be "functionized" but they are inline
 * Offset is only passed by reference in specific places, so do not be confused when following code
 * In any code path, adding up "offset+=" statements will tell you what byte you are at */
static void
dissect_infiniband_common(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, ib_packet_start_header starts_with)
{
    /* Top Level Item */
    proto_item *infiniband_packet;

    /* The Headers Subtree */
    proto_tree *all_headers_tree;

    /* BTH - Base Transport Header */
    bool dctBthHeader = false;
    int bthSize = 12;

    /* LRH - Local Route Header */
    proto_item *local_route_header_item;
    proto_tree *local_route_header_tree;

    /* Raw Data */
    proto_item *RAWDATA_header_item;
    uint8_t lnh_val;                 /* Link Next Header Value */
    int offset = 0;                /* Current Offset */

    /* General Variables */
    bool bthFollows = false;    /* Tracks if we are parsing a BTH.  This is a significant decision point */
    struct infinibandinfo info = { NULL, 0, 0, 0, 0, 0, 0, 0, false, false};
    int32_t nextHeaderSequence = -1; /* defined by this dissector. #define which indicates the upcoming header sequence from OpCode */
    uint8_t nxtHdr = 0;              /* Keyed off for header dissection order */
    uint16_t packetLength = 0;       /* Packet Length.  We track this as tvb_length - offset.   */
                                    /*  It provides the parsing methods a known size            */
                                    /*   that must be available for that header.                */
    int crc_length = 0;
    int crclen = 6;

    void *src_addr,                 /* the address to be displayed in the source/destination columns */
         *dst_addr;                 /* (lid/gid number) will be stored here */

    pinfo->srcport = pinfo->destport = 0xffffffff;  /* set the src/dest QPN to something impossible instead of the default 0,
                                                       so we don't mistake it for a MAD. (QP is only 24bit, so can't be 0xffffffff)*/

    pinfo->ptype = PT_IBQP;     /* set the port-type for this packet to be Infiniband QP number */

    /* Mark the Packet type as Infiniband in the wireshark UI */
    /* Clear other columns */
    col_set_str(pinfo->cinfo, COL_PROTOCOL, "InfiniBand");
    col_clear(pinfo->cinfo, COL_INFO);

    /* Top Level Packet */
    infiniband_packet = proto_tree_add_item(tree, proto_infiniband, tvb, offset, -1, ENC_NA);

    /* Headers Level Tree */
    all_headers_tree = proto_item_add_subtree(infiniband_packet, ett_all_headers);

    if (starts_with == IB_PACKET_STARTS_WITH_GRH) {
        /* this is a RoCE packet, skip LRH parsing */
        col_set_str(pinfo->cinfo, COL_PROTOCOL, "RoCE");
        lnh_val = IBA_GLOBAL;
        packetLength = tvb_reported_length_remaining(tvb, offset);
        crclen = 4;
        goto skip_lrh;
    }
     else if (starts_with == IB_PACKET_STARTS_WITH_BTH) {
         /* this is a RRoCE packet, skip LRH/GRH parsing and go directly to BTH */
         col_set_str(pinfo->cinfo, COL_PROTOCOL, "RRoCE");
         lnh_val = IBA_LOCAL;
         packetLength = tvb_reported_length_remaining(tvb, offset);
         crclen = 4;
         goto skip_lrh;
     }

    /* Local Route Header Subtree */
    local_route_header_item = proto_tree_add_item(all_headers_tree, hf_infiniband_LRH, tvb, offset, 8, ENC_NA);
    proto_item_set_text(local_route_header_item, "%s", "Local Route Header");
    local_route_header_tree = proto_item_add_subtree(local_route_header_item, ett_lrh);

    proto_tree_add_item(local_route_header_tree, hf_infiniband_virtual_lane, tvb, offset, 1, ENC_BIG_ENDIAN);

    proto_tree_add_item(local_route_header_tree, hf_infiniband_link_version, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;
    proto_tree_add_item(local_route_header_tree, hf_infiniband_service_level, tvb, offset, 1, ENC_BIG_ENDIAN);

    proto_tree_add_item(local_route_header_tree, hf_infiniband_reserved2, tvb, offset, 1, ENC_NA);
    proto_tree_add_item(local_route_header_tree, hf_infiniband_link_next_header, tvb, offset, 1, ENC_BIG_ENDIAN);


    /* Save Link Next Header... This tells us what the next header is. */
    lnh_val =  tvb_get_uint8(tvb, offset);
    lnh_val = lnh_val & 0x03;
    offset += 1;


    proto_tree_add_item(local_route_header_tree, hf_infiniband_destination_local_id, tvb, offset, 2, ENC_BIG_ENDIAN);


    /* Set destination in packet view. */
    dst_addr = wmem_alloc(pinfo->pool, sizeof(uint16_t));
    *((uint16_t*) dst_addr) = tvb_get_ntohs(tvb, offset);
    set_address(&pinfo->dst, AT_IB, sizeof(uint16_t), dst_addr);

    offset += 2;

    proto_tree_add_item(local_route_header_tree, hf_infiniband_reserved5, tvb, offset, 2, ENC_BIG_ENDIAN);

    packetLength = tvb_get_ntohs(tvb, offset); /* Get the Packet Length. This will determine payload size later on. */
    packetLength = packetLength & 0x07FF;      /* Mask off top 5 bits, they are reserved */
    packetLength = packetLength * 4;           /* Multiply by 4 to get true byte length. This is by specification.  */
                                               /*   PktLen is size in 4 byte words (byteSize /4). */

    proto_tree_add_item(local_route_header_tree, hf_infiniband_packet_length, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;
    proto_tree_add_item(local_route_header_tree, hf_infiniband_source_local_id, tvb, offset, 2, ENC_BIG_ENDIAN);

    /* Set Source in packet view. */
    src_addr = wmem_alloc(pinfo->pool, sizeof(uint16_t));
    *((uint16_t*) src_addr) = tvb_get_ntohs(tvb, offset);
    set_address(&pinfo->src, AT_IB, sizeof(uint16_t), src_addr);

    offset += 2;
    packetLength -= 8; /* Shave 8 bytes for the LRH. */

skip_lrh:

    /* Key off Link Next Header.  This tells us what High Level Data Format we have */
    switch (lnh_val)
    {
        case IBA_GLOBAL: {
            proto_item *global_route_header_item;
            proto_tree *global_route_header_tree;

            global_route_header_item = proto_tree_add_item(all_headers_tree, hf_infiniband_GRH, tvb, offset, 40, ENC_NA);
            proto_item_set_text(global_route_header_item, "%s", "Global Route Header");
            global_route_header_tree = proto_item_add_subtree(global_route_header_item, ett_grh);

            proto_tree_add_item(global_route_header_tree, hf_infiniband_ip_version, tvb, offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(global_route_header_tree, hf_infiniband_traffic_class, tvb, offset, 2, ENC_BIG_ENDIAN);
            proto_tree_add_item(global_route_header_tree, hf_infiniband_flow_label, tvb, offset, 4, ENC_BIG_ENDIAN);
            offset += 4;

            proto_tree_add_item(global_route_header_tree, hf_infiniband_payload_length, tvb, offset, 2, ENC_BIG_ENDIAN);
            offset += 2;

            nxtHdr = tvb_get_uint8(tvb, offset);

            proto_tree_add_item(global_route_header_tree, hf_infiniband_next_header, tvb, offset, 1, ENC_BIG_ENDIAN);
            offset += 1;
            proto_tree_add_item(global_route_header_tree, hf_infiniband_hop_limit, tvb, offset, 1, ENC_BIG_ENDIAN);
            offset += 1;
            proto_tree_add_item(global_route_header_tree, hf_infiniband_source_gid, tvb, offset, 16, ENC_NA);

            /* set source GID in packet view*/
            set_address_tvb(&pinfo->src, AT_IB, GID_SIZE, tvb, offset);
            offset += 16;

            proto_tree_add_item(global_route_header_tree, hf_infiniband_destination_gid, tvb, offset, 16, ENC_NA);
            /* set destination GID in packet view*/
            set_address_tvb(&pinfo->dst, AT_IB, GID_SIZE, tvb, offset);

            offset += 16;
            packetLength -= 40; /* Shave 40 bytes for GRH */

            if (nxtHdr != 0x1B)
            {
                /* Some kind of packet being transported globally with IBA, but locally it is not IBA - no BTH following. */
                break;
            }
        }
            /* otherwise fall through and start parsing BTH */
        /* FALL THROUGH */
        case IBA_LOCAL: {
            proto_item *base_transport_header_item;
            proto_tree *base_transport_header_tree;
            bthFollows = true;
            /* Get the OpCode - this tells us what headers are following */
            info.opCode = tvb_get_uint8(tvb, offset);
            info.pad_count = (tvb_get_uint8(tvb, offset+1) & 0x30) >> 4;

            if ((info.opCode >> 5) == 0x2) {
                info.dctConnect = !(tvb_get_uint8(tvb, offset + 1) & 0x80);
                dctBthHeader = true;
                bthSize += 8;
            }

            base_transport_header_item = proto_tree_add_item(all_headers_tree, hf_infiniband_BTH, tvb, offset, bthSize, ENC_NA);
            proto_item_set_text(base_transport_header_item, "%s", "Base Transport Header");
            base_transport_header_tree = proto_item_add_subtree(base_transport_header_item, ett_bth);
            proto_tree_add_item(base_transport_header_tree, hf_infiniband_opcode, tvb, offset, 1, ENC_BIG_ENDIAN);

            if (dctBthHeader) {
                /* since DCT uses the same opcodes as RD we will use another name mapping */
                col_append_str(pinfo->cinfo, COL_INFO, val_to_str_const((uint32_t)info.opCode, DctOpCodeMap, "Unknown OpCode "));
            }
            else {
                col_append_str(pinfo->cinfo, COL_INFO, val_to_str_const((uint32_t)info.opCode, OpCodeMap, "Unknown OpCode "));
            }
            offset += 1;

            proto_tree_add_item(base_transport_header_tree, hf_infiniband_solicited_event, tvb, offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(base_transport_header_tree, hf_infiniband_migreq, tvb, offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(base_transport_header_tree, hf_infiniband_pad_count, tvb, offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(base_transport_header_tree, hf_infiniband_transport_header_version, tvb, offset, 1, ENC_BIG_ENDIAN);
            offset += 1;
            proto_tree_add_item(base_transport_header_tree, hf_infiniband_partition_key, tvb, offset, 2, ENC_BIG_ENDIAN);
            offset += 2;
            proto_tree_add_item(base_transport_header_tree, hf_infiniband_reserved, tvb, offset, 1, ENC_NA);
            offset += 1;
            proto_tree_add_item_ret_uint(base_transport_header_tree, hf_infiniband_destination_qp, tvb, offset, 3, ENC_BIG_ENDIAN, &pinfo->destport);
            col_append_fstr(pinfo->cinfo, COL_INFO, "QP=0x%06x ", pinfo->destport);
            offset += 3;
            proto_tree_add_item(base_transport_header_tree, hf_infiniband_acknowledge_request, tvb, offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(base_transport_header_tree, hf_infiniband_reserved7, tvb, offset, 1, ENC_BIG_ENDIAN);
            offset += 1;
            proto_tree_add_item_ret_uint(base_transport_header_tree, hf_infiniband_packet_sequence_number, tvb, offset, 3, ENC_BIG_ENDIAN, &info.packet_seq_num);
            offset += 3;
            offset += bthSize - 12;
            packetLength -= bthSize; /* Shave bthSize for Base Transport Header */
        }
            break;
        case IP_NON_IBA:
            /* Raw IPv6 Packet */
            dst_addr = wmem_strdup(pinfo->pool, "IPv6 over IB Packet");
            set_address(&pinfo->dst,  AT_STRINGZ, (int)strlen((char *)dst_addr)+1, dst_addr);

            parse_IPvSix(all_headers_tree, tvb, &offset, pinfo);
            break;
        case RAW:
            parse_RWH(all_headers_tree, tvb, &offset, pinfo, tree);
            break;
        default:
            /* Unknown Packet */
            RAWDATA_header_item = proto_tree_add_item(all_headers_tree, hf_infiniband_raw_data, tvb, offset, -1, ENC_NA);
            proto_item_set_text(RAWDATA_header_item, "%s", "Unknown Raw Data - IB Encapsulated");
            break;
    }

    /* Base Transport header is hit quite often, however it is alone since it is the exception not the rule */
    /* Only IBA Local packets use it */
    if (bthFollows)
    {
        /* Find our next header sequence based on the Opcode
        * Each case decrements the packetLength by the amount of bytes consumed by each header.
        * The find_next_header_sequence method could be used to automate this.
        * We need to keep track of this so we know much data to mark as payload/ICRC/VCRC values. */

        nextHeaderSequence = find_next_header_sequence(&info);

        /* find_next_header_sequence gives us the DEFINE value corresponding to the header order following */
        /* Enumerations are named intuitively, e.g. RDETH DETH PAYLOAD means there is an RDETH Header, DETH Header, and a packet payload */
        switch (nextHeaderSequence)
        {
            case RDETH_DETH_PAYLD:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);

                packetLength -= 4; /* RDETH */
                packetLength -= 8; /* DETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RDETH_DETH_RETH_PAYLD:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);
                parse_RETH(all_headers_tree, tvb, &offset, &info);

                packetLength -= 4; /* RDETH */
                packetLength -= 8; /* DETH */
                packetLength -= 16; /* RETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RDETH_DETH_IMMDT_PAYLD:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);
                parse_IMMDT(all_headers_tree, tvb, &offset);

                packetLength -= 4; /* RDETH */
                packetLength -= 8; /* DETH */
                packetLength -= 4; /* IMMDT */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RDETH_DETH_RETH_IMMDT_PAYLD:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);
                parse_RETH(all_headers_tree, tvb, &offset, &info);
                parse_IMMDT(all_headers_tree, tvb, &offset);

                packetLength -= 4;  /* RDETH */
                packetLength -= 8;  /* DETH */
                packetLength -= 16; /* RETH */
                packetLength -= 4;  /* IMMDT */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RDETH_DETH_RETH:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);
                parse_RETH(all_headers_tree, tvb, &offset, &info);

                /*packetLength -= 4;*/  /* RDETH */
                /*packetLength -= 8;*/  /* DETH */
                /*packetLength -= 16;*/ /* RETH */

                break;
            case RDETH_AETH_PAYLD:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_AETH(all_headers_tree, tvb, &offset, pinfo);

                packetLength -= 4; /* RDETH */
                packetLength -= 4; /* AETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RDETH_PAYLD:
                parse_RDETH(all_headers_tree, tvb, &offset);

                packetLength -= 4; /* RDETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RDETH_AETH:
                parse_AETH(all_headers_tree, tvb, &offset, pinfo);

                /*packetLength -= 4;*/ /* RDETH */
                /*packetLength -= 4;*/ /* AETH */


                break;
            case RDETH_AETH_ATOMICACKETH:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_AETH(all_headers_tree, tvb, &offset, pinfo);
                parse_ATOMICACKETH(all_headers_tree, tvb, &offset);

                /*packetLength -= 4;*/ /* RDETH */
                /*packetLength -= 4;*/ /* AETH */
                /*packetLength -= 8;*/ /* AtomicAckETH */


                break;
            case RDETH_DETH_ATOMICETH:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);
                parse_ATOMICETH(all_headers_tree, tvb, &offset);

                /*packetLength -= 4;*/  /* RDETH */
                /*packetLength -= 8;*/  /* DETH */
                /*packetLength -= 28;*/ /* AtomicETH */

                break;
            case RDETH_DETH:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);

                /*packetLength -= 4;*/ /* RDETH */
                /*packetLength -= 8;*/ /* DETH */

                break;
            case DETH_PAYLD:
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);

                packetLength -= 8; /* DETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case PAYLD:

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case IMMDT_PAYLD:
                parse_IMMDT(all_headers_tree, tvb, &offset);

                packetLength -= 4; /* IMMDT */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RETH_IMMDT_PAYLD:
                parse_RETH(all_headers_tree, tvb, &offset, &info);
                parse_IMMDT(all_headers_tree, tvb, &offset);

                packetLength -= 16; /* RETH */
                packetLength -= 4; /* IMMDT */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RETH_PAYLD:
                parse_RETH(all_headers_tree, tvb, &offset, &info);

                packetLength -= 16; /* RETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case RETH:
                parse_RETH(all_headers_tree, tvb, &offset, &info);

                packetLength -= 16; /* RETH */
                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);

                break;
            case AETH_PAYLD:
                parse_AETH(all_headers_tree, tvb, &offset, pinfo);

                packetLength -= 4; /* AETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case AETH:
                parse_AETH(all_headers_tree, tvb, &offset, pinfo);

                /*packetLength -= 4;*/ /* AETH */

                break;
            case AETH_ATOMICACKETH:
                parse_AETH(all_headers_tree, tvb, &offset, pinfo);
                parse_ATOMICACKETH(all_headers_tree, tvb, &offset);

                /*packetLength -= 4;*/ /* AETH */
                /*packetLength -= 8;*/ /* AtomicAckETH */

                break;
            case ATOMICETH:
                parse_ATOMICETH(all_headers_tree, tvb, &offset);

                /*packetLength -= 28;*/ /* AtomicETH */

                break;
            case IETH_PAYLD:
                parse_IETH(all_headers_tree, tvb, &offset);

                packetLength -= 4; /* IETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case DETH_IMMDT_PAYLD:
                parse_DETH(all_headers_tree, pinfo, tvb, &offset);
                parse_IMMDT(all_headers_tree, tvb, &offset);

                packetLength -= 8; /* DETH */
                packetLength -= 4; /* IMMDT */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case DCCETH:
                parse_DCCETH(all_headers_tree, tvb, &offset);
                packetLength -= 16; /* DCCETH */

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            case FETH_RETH:
                parse_FETH(all_headers_tree, tvb, &offset);
                parse_RETH(all_headers_tree, tvb, &offset, &info);

                /*packetLength -= 4;*/ /* FETH */
                /*packetLength -= 16;*/ /* RETH */
                break;
            case RDETH_FETH_RETH:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_FETH(all_headers_tree, tvb, &offset);
                parse_RETH(all_headers_tree, tvb, &offset, &info);

                /*packetLength -= 4;*/ /* RDETH */
                /*packetLength -= 4;*/ /* FETH */
                /*packetLength -= 16;*/ /* RETH */
                break;
            case RDETH_RETH_PAYLD:
                parse_RDETH(all_headers_tree, tvb, &offset);
                parse_RETH(all_headers_tree, tvb, &offset, &info);

                packetLength -= 4;
                packetLength -= 16;

                parse_PAYLOAD(all_headers_tree, pinfo, &info, tvb, &offset, packetLength, crclen, tree);
                break;
            default:
                parse_VENDOR(all_headers_tree, tvb, &offset);
                break;

        }

    }
    /* Display the ICRC/VCRC */
    /* Doing it this way rather than in a variety of places according to the specific packet */
    /* If we've already displayed it crc_length comes out 0 */
    crc_length = tvb_reported_length_remaining(tvb, offset);
    if (crc_length == 6)
    {
        proto_tree_add_item(all_headers_tree, hf_infiniband_invariant_crc, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
        proto_tree_add_item(all_headers_tree, hf_infiniband_variant_crc, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;
    }
    else if (crc_length == 4)
    {
        proto_tree_add_item(all_headers_tree, hf_infiniband_invariant_crc, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }
    else if (crc_length == 2)
    {
        proto_tree_add_item(all_headers_tree, hf_infiniband_variant_crc, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;
    }

}

static int
dissect_infiniband_link(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    /* Top Level Item */
    proto_item *infiniband_link_packet;

    /* The Link Subtree */
    proto_tree *link_tree;

    proto_item *operand_item;
    int         offset = 0;     /* Current Offset */
    uint8_t     operand;        /* Link packet Operand */

    operand =  tvb_get_uint8(tvb, offset);
    operand = (operand & 0xF0) >> 4;

    /* Mark the Packet type as Infiniband in the wireshark UI */
    /* Clear other columns */
    col_set_str(pinfo->cinfo, COL_PROTOCOL, "InfiniBand Link");
    col_add_str(pinfo->cinfo, COL_INFO,
             val_to_str(operand, Operand_Description, "Unknown (0x%1x)"));

    /* Assigns column values */
    dissect_general_info(tvb, offset, pinfo, IB_PACKET_STARTS_WITH_LRH);

    /* Top Level Packet */
    infiniband_link_packet = proto_tree_add_item(tree, proto_infiniband_link, tvb, offset, -1, ENC_NA);

    /* Headers Level Tree */
    link_tree = proto_item_add_subtree(infiniband_link_packet, ett_link);

    operand_item = proto_tree_add_item(link_tree, hf_infiniband_link_op, tvb, offset, 2, ENC_BIG_ENDIAN);

    if (operand > 1) {
        proto_item_set_text(operand_item, "%s", "Reserved");
        call_data_dissector(tvb, pinfo, link_tree);
    } else {
        proto_tree_add_item(link_tree, hf_infiniband_link_fctbs, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;

        proto_tree_add_item(link_tree, hf_infiniband_link_vl, tvb, offset, 2, ENC_BIG_ENDIAN);
        proto_tree_add_item(link_tree, hf_infiniband_link_fccl, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;

        proto_tree_add_item(link_tree, hf_infiniband_link_lpcrc, tvb, offset, 2, ENC_BIG_ENDIAN);
    }

    return tvb_captured_length(tvb);
}


/* Description: Finds the header sequence that follows the Base Transport Header.
* Somwhat inefficient (should be using a single key,value pair data structure)
* But uses pure probablity to take a stab at better efficiency.
* Searches largest header sequence groups first, and then finally resorts to single matches for unique header sequences
* IN: OpCode: The OpCode from the Base Transport Header.
* OUT: The Header Sequence enumeration.  See Declarations for #defines from (0-22) */
static int32_t
find_next_header_sequence(struct infinibandinfo* ibInfo)
{
    if (ibInfo->opCode == 0x55)
        return ibInfo->dctConnect ? DCCETH : PAYLD;

    if (contains(ibInfo->opCode, &opCode_PAYLD[0], (int32_t)array_length(opCode_PAYLD)))
        return PAYLD;

    if (contains(ibInfo->opCode, &opCode_IMMDT_PAYLD[0], (int32_t)array_length(opCode_IMMDT_PAYLD)))
        return IMMDT_PAYLD;

    if (contains(ibInfo->opCode, &opCode_RDETH_DETH_PAYLD[0], (int32_t)array_length(opCode_RDETH_DETH_PAYLD)))
        return RDETH_DETH_PAYLD;

    if (contains(ibInfo->opCode, &opCode_RETH_PAYLD[0], (int32_t)array_length(opCode_RETH_PAYLD)))
        return RETH_PAYLD;

    if (contains(ibInfo->opCode, &opCode_RDETH_AETH_PAYLD[0], (int32_t)array_length(opCode_RDETH_AETH_PAYLD)))
        return RDETH_AETH_PAYLD;

    if (contains(ibInfo->opCode, &opCode_AETH_PAYLD[0], (int32_t)array_length(opCode_AETH_PAYLD)))
        return AETH_PAYLD;

    if (contains(ibInfo->opCode, &opCode_RDETH_DETH_IMMDT_PAYLD[0], (int32_t)array_length(opCode_RDETH_DETH_IMMDT_PAYLD)))
        return RDETH_DETH_IMMDT_PAYLD;

    if (contains(ibInfo->opCode, &opCode_RETH_IMMDT_PAYLD[0], (int32_t)array_length(opCode_RETH_IMMDT_PAYLD)))
        return RETH_IMMDT_PAYLD;

    if (contains(ibInfo->opCode, &opCode_RDETH_DETH_RETH_PAYLD[0], (int32_t)array_length(opCode_RDETH_DETH_RETH_PAYLD)))
        return RDETH_DETH_RETH_PAYLD;

    if (contains(ibInfo->opCode, &opCode_ATOMICETH[0], (int32_t)array_length(opCode_ATOMICETH)))
        return ATOMICETH;

    if (contains(ibInfo->opCode, &opCode_IETH_PAYLD[0], (int32_t)array_length(opCode_IETH_PAYLD)))
        return IETH_PAYLD;

    if (contains(ibInfo->opCode, &opCode_RDETH_DETH_ATOMICETH[0], (int32_t)array_length(opCode_RDETH_DETH_ATOMICETH)))
        return RDETH_DETH_ATOMICETH;

    if ((ibInfo->opCode ^ RC_ACKNOWLEDGE) == 0)
        return AETH;

    if ((ibInfo->opCode ^ RC_RDMA_READ_REQUEST) == 0)
        return RETH;

    if ((ibInfo->opCode ^ RC_ATOMIC_ACKNOWLEDGE) == 0)
        return AETH_ATOMICACKETH;

    if ((ibInfo->opCode ^ RD_RDMA_READ_RESPONSE_MIDDLE) == 0)
        return RDETH_PAYLD;

    if ((ibInfo->opCode ^ RD_ACKNOWLEDGE) == 0)
        return RDETH_AETH;

    if ((ibInfo->opCode ^ RD_ATOMIC_ACKNOWLEDGE) == 0)
        return RDETH_AETH_ATOMICACKETH;

    if ((ibInfo->opCode ^ RD_RDMA_WRITE_ONLY_IMM) == 0)
        return RDETH_DETH_RETH_IMMDT_PAYLD;

    if ((ibInfo->opCode ^ RD_RDMA_READ_REQUEST) == 0)
        return RDETH_DETH_RETH;

    if ((ibInfo->opCode ^ RD_RESYNC) == 0)
        return RDETH_DETH;

    if ((ibInfo->opCode ^ UD_SEND_ONLY) == 0)
        return DETH_PAYLD;

    if ((ibInfo->opCode ^ UD_SEND_ONLY_IMM) == 0)
        return DETH_IMMDT_PAYLD;

    if ((ibInfo->opCode ^ RC_FLUSH) == 0)
        return FETH_RETH;

    if ((ibInfo->opCode ^ RD_FLUSH) == 0)
        return RDETH_FETH_RETH;

    if ((ibInfo->opCode ^ RC_ATOMIC_WRITE) == 0)
        return RETH_PAYLD;

    if ((ibInfo->opCode ^ RD_ATOMIC_WRITE) == 0)
        return RDETH_RETH_PAYLD;

    return -1;
}

/* Description: Finds if a given value is present in an array. This is probably in a standard library somewhere,
* But I'd rather define my own.
* IN: OpCode: The OpCode you are looking for
* IN: Codes: The organized array of OpCodes to look through
* IN: Array length, because we're in C++...
* OUT: Boolean indicating if that OpCode was found in OpCodes */
static bool
contains(uint32_t OpCode, uint32_t* Codes, int32_t length)
{
    int32_t i;
    for (i = 0; i < length; i++)
    {
        if ((OpCode ^ Codes[i]) == 0)
            return true;
    }
    return false;
}

/* Parse RDETH - Reliable Datagram Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void
parse_RDETH(proto_tree * parentTree, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    /* RDETH - Reliable Datagram Extended Transport Header */
    proto_item *RDETH_header_item;
    proto_tree *RDETH_header_tree;

    RDETH_header_item = proto_tree_add_item(parentTree, hf_infiniband_RDETH, tvb, local_offset, 4, ENC_NA);
    proto_item_set_text(RDETH_header_item, "%s", "RDETH - Reliable Datagram Extended Transport Header");
    RDETH_header_tree = proto_item_add_subtree(RDETH_header_item, ett_rdeth);

    proto_tree_add_item(RDETH_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(RDETH_header_tree, hf_infiniband_ee_context, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    *offset = local_offset;
}

/* Parse DETH - Datagram Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset  */
static void
parse_DETH(proto_tree *parentTree, packet_info *pinfo, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    /* DETH - Datagram Extended Transport Header */
    proto_item *DETH_header_item;
    proto_tree *DETH_header_tree;

    DETH_header_item = proto_tree_add_item(parentTree, hf_infiniband_DETH, tvb, local_offset, 8, ENC_NA);
    proto_item_set_text(DETH_header_item, "%s", "DETH - Datagram Extended Transport Header");
    DETH_header_tree = proto_item_add_subtree(DETH_header_item, ett_deth);

    proto_tree_add_item(DETH_header_tree, hf_infiniband_queue_key, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(DETH_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(DETH_header_tree, hf_infiniband_source_qp, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    pinfo->srcport = tvb_get_ntoh24(tvb, local_offset);
    local_offset += 3;

    *offset = local_offset;
}

/* Parse DCCETH - DC Connected Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: dctConnect - True if this is a DCT-Connect packet.
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset  */
static void
parse_DCCETH(proto_tree *parentTree _U_, tvbuff_t *tvb _U_, int *offset)
{
    /* Do nothing just skip the header size */
    *offset += 16;
}

/* Parse RETH - RDMA Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset
* OUT: Updated info->reth_remote_key
* OUT: Updated info->reth_dma_length */
static void
parse_RETH(proto_tree * parentTree, tvbuff_t *tvb, int *offset,
           struct infinibandinfo *info)
{
    int         local_offset = *offset;
    /* RETH - RDMA Extended Transport Header */
    proto_item *RETH_header_item;
    proto_tree *RETH_header_tree;

    RETH_header_item = proto_tree_add_item(parentTree, hf_infiniband_RETH, tvb, local_offset, 16, ENC_NA);
    proto_item_set_text(RETH_header_item, "%s", "RETH - RDMA Extended Transport Header");
    RETH_header_tree = proto_item_add_subtree(RETH_header_item, ett_reth);

    proto_tree_add_item_ret_uint64(RETH_header_tree, hf_infiniband_virtual_address, tvb, local_offset, 8, ENC_BIG_ENDIAN, &info->reth_remote_address);
    local_offset += 8;
    proto_tree_add_item_ret_uint(RETH_header_tree, hf_infiniband_remote_key, tvb, local_offset, 4, ENC_BIG_ENDIAN, &info->reth_remote_key);
    local_offset += 4;
    proto_tree_add_item_ret_uint(RETH_header_tree, hf_infiniband_dma_length, tvb, local_offset, 4, ENC_BIG_ENDIAN, &info->reth_dma_length);
    local_offset += 4;

    *offset = local_offset;
}

/* Parse AtomicETH - Atomic Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void
parse_ATOMICETH(proto_tree * parentTree, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    /* AtomicETH - Atomic Extended Transport Header */
    proto_item *ATOMICETH_header_item;
    proto_tree *ATOMICETH_header_tree;

    ATOMICETH_header_item = proto_tree_add_item(parentTree, hf_infiniband_AtomicETH, tvb, local_offset, 28, ENC_NA);
    proto_item_set_text(ATOMICETH_header_item, "%s", "AtomicETH - Atomic Extended Transport Header");
    ATOMICETH_header_tree = proto_item_add_subtree(ATOMICETH_header_item, ett_atomiceth);

    proto_tree_add_item(ATOMICETH_header_tree, hf_infiniband_virtual_address, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(ATOMICETH_header_tree, hf_infiniband_remote_key, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(ATOMICETH_header_tree, hf_infiniband_swap_or_add_data, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(ATOMICETH_header_tree, hf_infiniband_compare_data, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    *offset = local_offset;
}

/* Parse AETH - ACK Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void
parse_AETH(proto_tree * parentTree, tvbuff_t *tvb, int *offset, packet_info *pinfo)
{
    int         local_offset = *offset;
    /* AETH - ACK Extended Transport Header */
    proto_item *AETH_header_item;
    proto_tree *AETH_header_tree;
    proto_item *AETH_syndrome_item;
    proto_tree *AETH_syndrome_tree;
    uint8_t     opcode;
    uint32_t    nak_error;

    AETH_header_item = proto_tree_add_item(parentTree, hf_infiniband_AETH, tvb, local_offset, 4, ENC_NA);
    proto_item_set_text(AETH_header_item, "%s", "AETH - ACK Extended Transport Header");
    AETH_header_tree = proto_item_add_subtree(AETH_header_item, ett_aeth);

    AETH_syndrome_item = proto_tree_add_item(AETH_header_tree, hf_infiniband_syndrome, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    AETH_syndrome_tree = proto_item_add_subtree(AETH_syndrome_item, ett_aeth_syndrome);
    proto_tree_add_item(AETH_syndrome_tree, hf_infiniband_syndrome_reserved, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(AETH_syndrome_tree, hf_infiniband_syndrome_opcode, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    opcode = ((tvb_get_uint8(tvb, local_offset) & AETH_SYNDROME_OPCODE) >> 5);
    proto_item_append_text(AETH_syndrome_item, ", %s", val_to_str_const(opcode, aeth_syndrome_opcode_vals, "Unknown"));
    switch (opcode)
    {
        case AETH_SYNDROME_OPCODE_ACK:
            proto_tree_add_item(AETH_syndrome_tree, hf_infiniband_syndrome_credit_count, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            break;
        case AETH_SYNDROME_OPCODE_RNR_NAK:
            proto_tree_add_item(AETH_syndrome_tree, hf_infiniband_syndrome_timer, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            break;
        case AETH_SYNDROME_OPCODE_RES:
            proto_tree_add_item(AETH_syndrome_tree, hf_infiniband_syndrome_reserved_value, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            break;
        case AETH_SYNDROME_OPCODE_NAK:
            proto_tree_add_item_ret_uint(AETH_syndrome_tree, hf_infiniband_syndrome_error_code, tvb, local_offset, 1, ENC_BIG_ENDIAN, &nak_error);
            col_append_fstr(pinfo->cinfo, COL_INFO, "[%s] ", val_to_str(nak_error, aeth_syndrome_nak_error_code_vals, "Unknown (%d)"));
            break;
    }

    local_offset += 1;
    proto_tree_add_item(AETH_header_tree, hf_infiniband_message_sequence_number, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;

    *offset = local_offset;
}

/* Parse AtomicAckEth - Atomic ACK Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void
parse_ATOMICACKETH(proto_tree * parentTree, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    /* AtomicAckEth - Atomic ACK Extended Transport Header */
    proto_item *ATOMICACKETH_header_item;
    proto_tree *ATOMICACKETH_header_tree;

    ATOMICACKETH_header_item = proto_tree_add_item(parentTree, hf_infiniband_AtomicAckETH, tvb, local_offset, 8, ENC_NA);
    proto_item_set_text(ATOMICACKETH_header_item, "%s", "ATOMICACKETH - Atomic ACK Extended Transport Header");
    ATOMICACKETH_header_tree = proto_item_add_subtree(ATOMICACKETH_header_item, ett_atomicacketh);
    proto_tree_add_item(ATOMICACKETH_header_tree, hf_infiniband_original_remote_data, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    *offset = local_offset;
}

/* Parse IMMDT - Immediate Data Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void
parse_IMMDT(proto_tree * parentTree, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    /* IMMDT - Immediate Data Extended Transport Header */
    proto_item *IMMDT_header_item;
    proto_tree *IMMDT_header_tree;

    IMMDT_header_item = proto_tree_add_item(parentTree, hf_infiniband_IMMDT, tvb, local_offset, 4, ENC_NA);
    proto_item_set_text(IMMDT_header_item, "%s", "IMMDT - Immediate Data Extended Transport Header");
    IMMDT_header_tree = proto_item_add_subtree(IMMDT_header_item, ett_immdt);
    proto_tree_add_item(IMMDT_header_tree, hf_infiniband_IMMDT, tvb, local_offset, 4, ENC_NA);
    local_offset += 4;
    *offset = local_offset;
}

/* Parse IETH - Invalidate Extended Transport Header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void
parse_IETH(proto_tree * parentTree, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    /* IETH - Invalidate Extended Transport Header */
    proto_item *IETH_header_item;
    proto_tree *IETH_header_tree;

    IETH_header_item = proto_tree_add_item(parentTree, hf_infiniband_IETH, tvb, local_offset, 4, ENC_NA);
    proto_item_set_text(IETH_header_item, "%s", "IETH - Invalidate Extended Transport Header");
    IETH_header_tree = proto_item_add_subtree(IETH_header_item, ett_ieth);

    proto_tree_add_item(IETH_header_tree, hf_infiniband_IETH, tvb, local_offset, 4, ENC_NA);
    local_offset += 4;

    *offset = local_offset;
}

/* Parse FETH - FLUSH extended transport header
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void
parse_FETH(proto_tree * parentTree, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    /* FETH - FLUSH Extended Transport Header */
    proto_item *FETH_header_item;
    proto_tree *FETH_header_tree;

    FETH_header_item = proto_tree_add_item(parentTree, hf_infiniband_FETH, tvb, local_offset, 4, ENC_NA);
    proto_item_set_text(FETH_header_item, "%s", "FETH - FLUSH Extended Transport Header");
    FETH_header_tree = proto_item_add_subtree(FETH_header_item, ett_ieth);

    proto_tree_add_item(FETH_header_tree, hf_infiniband_reserved27, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(FETH_header_tree, hf_infiniband_selectivity_level, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(FETH_header_tree, hf_infiniband_placement_type, tvb, local_offset, 1, ENC_BIG_ENDIAN);

    local_offset += 1;
    *offset = local_offset;
}

static void update_sport(packet_info *pinfo)
{
    conversation_t *conv;
    conversation_infiniband_data *conv_data;

    conv = find_conversation(pinfo->num, &pinfo->dst, &pinfo->dst,
                             CONVERSATION_IBQP, pinfo->destport, pinfo->destport, NO_ADDR_B|NO_PORT_B);
    if (!conv)
        return;

    conv_data = (conversation_infiniband_data *)conversation_get_proto_data(conv, proto_infiniband);
    if (!conv_data)
        return;

    pinfo->srcport = conv_data->src_qp;
}

static bool parse_PAYLOAD_do_rc_send_reassembling(packet_info *pinfo,
                                                  struct infinibandinfo *info)
{
    conversation_t *conversation = NULL;
    conversation_infiniband_data *proto_data = NULL;

    switch (info->opCode) {
    case RC_SEND_FIRST:
    case RC_SEND_MIDDLE:
    case RC_SEND_ONLY:
    case RC_SEND_ONLY_IMM:
    case RC_SEND_ONLY_INVAL:
    case RC_SEND_LAST:
    case RC_SEND_LAST_IMM:
    case RC_SEND_LAST_INVAL:
        break;
    default:
        /* not a fragmented RC Send */
        return false;
    }

    /*
     * Check if RC Send reassembling was used before in
     * this conversation
     */
    conversation = find_conversation_pinfo(pinfo, 0);
    if (conversation == NULL) {
        return false;
    }
    proto_data = conversation_get_proto_data(conversation, proto_infiniband);
    if (proto_data == NULL) {
        return false;
    }

    return proto_data->do_rc_send_reassembling;

    return true;
}

static tvbuff_t *parse_PAYLOAD_reassemble_tvb(packet_info *pinfo,
                                              struct infinibandinfo *info,
                                              tvbuff_t *tvb,
                                              proto_tree *top_tree)
{
    static const uint32_t rc_send_id = 99;
    conversation_t *conversation = NULL;
    conversation_infiniband_data *proto_data = NULL;
    bool more_frags = false;
    fragment_head *fd_head = NULL;
    bool fd_head_not_cached = false;

    switch (info->opCode) {
    case RC_SEND_FIRST:
    case RC_SEND_MIDDLE:
        more_frags = true;
        break;
    case RC_SEND_ONLY:
    case RC_SEND_ONLY_IMM:
    case RC_SEND_ONLY_INVAL:
    case RC_SEND_LAST:
    case RC_SEND_LAST_IMM:
    case RC_SEND_LAST_INVAL:
        break;
    default:
        /* not a fragmented RC Send */
        return tvb;
    }

    conversation = find_or_create_conversation(pinfo);
    proto_data = conversation_get_proto_data(conversation, proto_infiniband);
    if (proto_data == NULL) {
        /*
         * Remember that RC Send reassembling was requested
         * for this conversation
         */
        proto_data = wmem_new0(wmem_file_scope(), conversation_infiniband_data);
        proto_data->do_rc_send_reassembling = true;
        conversation_add_proto_data(conversation,
                                    proto_infiniband,
                                    proto_data);
    }

    fd_head = (fragment_head *)p_get_proto_data(wmem_file_scope(),
                                                pinfo,
                                                proto_infiniband,
                                                rc_send_id);
    if (fd_head == NULL) {
            fd_head_not_cached = true;

            pinfo->fd->visited = 0;
            fd_head = fragment_add_seq_next(&infiniband_rc_send_reassembly_table,
                                            tvb, 0, pinfo,
                                            conversation->conv_index,
                                            NULL, tvb_captured_length(tvb),
                                            more_frags);
    }

    if (fd_head == NULL) {
            /*
             * We really want the fd_head and pass it to
             * process_reassembled_data()
             *
             * So that individual fragments gets the
             * reassembled in field.
             */
            fd_head = fragment_get_reassembled_id(&infiniband_rc_send_reassembly_table,
                                                  pinfo,
                                                  conversation->conv_index);
    }

    if (fd_head == NULL) {
            /*
             * we need more data...
             */
            return NULL;
    }

    if (fd_head_not_cached) {
            p_add_proto_data(wmem_file_scope(), pinfo,
                             proto_infiniband,
                             rc_send_id,
                             fd_head);
    }

    return process_reassembled_data(tvb, 0, pinfo,
                                    "Reassembled Infiniband RC Send fragments",
                                    fd_head,
                                    &infiniband_rc_send_frag_items,
                                    NULL, /* update_col_info*/
                                    top_tree);
}

/* Parse Payload - Packet Payload / Invariant CRC / maybe Variant CRC
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: pinfo - packet info from wireshark
* IN: info - infiniband info passed to subdissectors
* IN: tvb - the data buffer from wireshark
* IN/OUT: offset - The current and updated offset
* IN: length - Length of Payload
* IN: top_tree - parent tree of Infiniband dissector */
static void parse_PAYLOAD(proto_tree *parentTree,
                          packet_info *pinfo, struct infinibandinfo *info,
                          tvbuff_t *tvb, int *offset, int length, int crclen, proto_tree *top_tree)
{
    int                 local_offset    = *offset;
    /* Payload - Packet Payload */
    uint8_t             management_class;
    tvbuff_t *volatile  next_tvb;
    int                 reported_length;
    heur_dtbl_entry_t  *hdtbl_entry;
    bool                dissector_found = false;

    if (!tvb_bytes_exist(tvb, *offset, length)) /* previously consumed bytes + offset was all the data - none or corrupt payload */
    {
        col_set_str(pinfo->cinfo, COL_INFO, "Invalid Packet Length from LRH! [Malformed Packet]");
        col_set_fence(pinfo->cinfo, COL_INFO);
        return;
    }

    /* management datagrams are determined by the source/destination QPs */
    if (pinfo->srcport == 0 || pinfo->srcport == 1 || pinfo->destport == 0 || pinfo->destport == 1)    /* management datagram */
    {
        management_class =  tvb_get_uint8(tvb, (*offset) + 1);

        if (((management_class >= (uint8_t)VENDOR_1_START) && (management_class <= (uint8_t)VENDOR_1_END))
            || ((management_class >= (uint8_t)VENDOR_2_START) && (management_class <= (uint8_t)VENDOR_2_END)))
        {
            /* parse vendor specific */
            col_set_str(pinfo->cinfo, COL_INFO, "VENDOR (Unknown Attribute)");
            parse_VENDOR_MANAGEMENT(parentTree, tvb, offset);
        }
        else if ((management_class >= (uint8_t)APPLICATION_START) && (management_class <= (uint8_t)APPLICATION_END))
        {
            /* parse application specific */
            col_set_str(pinfo->cinfo, COL_INFO, "APP (Unknown Attribute)");
            parse_APPLICATION_MANAGEMENT(parentTree, tvb, offset);
        }
        else if (((management_class == (uint8_t)0x00) || (management_class == (uint8_t)0x02))
                 || ((management_class >= (uint8_t)0x50) && (management_class <= (uint8_t)0x80))
                 || ((management_class >= (uint8_t)0x82)))
        {
            /* parse reserved classes */
            col_set_str(pinfo->cinfo, COL_INFO, "RESERVED (Unknown Attribute)");
            parse_RESERVED_MANAGEMENT(parentTree, tvb, offset);
        }
        else /* we have a normal management_class */
        {
            switch (management_class)
            {
                case SUBN_LID_ROUTED:
                    /* parse subn man lid routed */
                    parse_SUBN_LID_ROUTED(parentTree, pinfo, tvb, &local_offset);
                break;
                case SUBN_DIRECTED_ROUTE:
                    /* parse subn directed route */
                    parse_SUBN_DIRECTED_ROUTE(parentTree, pinfo, tvb, &local_offset);
                break;
                case SUBNADMN:
                    /* parse sub admin */
                    parse_SUBNADMN(parentTree, pinfo, tvb, &local_offset);
                break;
                case PERF:
                    /* parse performance */
                    parse_PERF(parentTree, tvb, pinfo, &local_offset);
                break;
                case BM:
                    /* parse baseboard mgmt */
                    col_set_str(pinfo->cinfo, COL_INFO, "BM (Unknown Attribute)");
                    parse_BM(parentTree, tvb, &local_offset);
                break;
                case DEV_MGT:
                    /* parse device management */
                    col_set_str(pinfo->cinfo, COL_INFO, "DEV_MGT (Unknown Attribute)");
                    parse_DEV_MGT(parentTree, tvb, &local_offset);
                break;
                case COM_MGT:
                    /* parse communication management */
                    parse_COM_MGT(parentTree, pinfo, tvb, &local_offset, top_tree);
                break;
                case SNMP:
                    /* parse snmp tunneling */
                    col_set_str(pinfo->cinfo, COL_INFO, "SNMP (Unknown Attribute)");
                    parse_SNMP(parentTree, tvb, &local_offset);
                break;
                default:
                    break;
            }
        }
    }
    else /* Normal Data Packet - Parse as such */
    {
        bool allow_reassembling = true;
        /* update sport for the packet, for dissectors that performs
         * exact match on saddr, dadr, sport, dport tuple.
         */
        update_sport(pinfo);

        info->payload_tree = parentTree;

        /* Calculation for Payload:
        * (tvb_length) Length of entire packet - (local_offset) Starting byte of Payload Data
        * offset addition is more complex for the payload.
        * We need the total length of the packet, - length of previous headers, + offset where payload started.
        * We also need  to reserve 6 bytes for the CRCs which are not actually part of the payload.  */
        reported_length = tvb_reported_length_remaining(tvb, local_offset);
        if (reported_length >= crclen)
            reported_length -= crclen;
        next_tvb = tvb_new_subset_length(tvb, local_offset, reported_length);

        info->do_rc_send_reassembling = parse_PAYLOAD_do_rc_send_reassembling(pinfo, info);

reassemble:

        if (info->do_rc_send_reassembling) {
            allow_reassembling = false;
            next_tvb = parse_PAYLOAD_reassemble_tvb(pinfo, info, next_tvb, top_tree);
            if (next_tvb == NULL) {
                /*
                 * we need more data...
                 */
                goto skip_dissector;
            }
        }

        if (try_heuristic_first)
        {
            if (dissector_try_heuristic(heur_dissectors_payload, next_tvb, pinfo, top_tree, &hdtbl_entry, info))
                dissector_found = true;
        }

        if (dissector_found == false)
        {
            if (dissector_try_payload_with_data(subdissector_table, next_tvb, pinfo, top_tree, true, info))
            {
                dissector_found = true;
            }
            else
            {
                if (!try_heuristic_first)
                {
                    if (dissector_try_heuristic(heur_dissectors_payload, next_tvb, pinfo, top_tree, &hdtbl_entry, info))
                        dissector_found = true;
                }
            }
        }

        if (dissector_found == false)
        {
            /* No sub-dissector found.
               Label rest of packet as "Data" */
            call_data_dissector(next_tvb, pinfo, top_tree);
        }

        if (allow_reassembling && info->do_rc_send_reassembling) {
                goto reassemble;
        }

skip_dissector:
        /* Will contain ICRC <and maybe VCRC> = 4 or 4+2 (crclen) */
        local_offset = tvb_reported_length(tvb) - crclen;
    }

    *offset = local_offset;
}

/* Parse VENDOR - Parse a vendor specific or unknown header sequence
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_VENDOR(proto_tree * parentTree, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *VENDOR_header_item;
    proto_tree *VENDOR_header_tree;
    int         VENDOR_header_length;

    VENDOR_header_item = proto_tree_add_item(parentTree, hf_infiniband_vendor, tvb, local_offset, 4, ENC_NA);
    proto_item_set_text(VENDOR_header_item, "%s", "Vendor Specific or Unknown Header Sequence");
    VENDOR_header_tree = proto_item_add_subtree(VENDOR_header_item, ett_vendor);
    proto_tree_add_item_ret_length(VENDOR_header_tree, hf_infiniband_vendor, tvb, local_offset, -1, ENC_NA, &VENDOR_header_length);
    local_offset += VENDOR_header_length;
    *offset = local_offset;
}

/* Parse IPv6 - Parse an IPv6 Packet
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset
* IN: pinfo - packet info from wireshark */
static void parse_IPvSix(proto_tree *parentTree, tvbuff_t *tvb, int *offset, packet_info *pinfo)
{
    tvbuff_t *ipv6_tvb;

    /* (- 2) for VCRC which lives at the end of the packet   */
    ipv6_tvb = tvb_new_subset_length(tvb, *offset,
                  tvb_reported_length_remaining(tvb, *offset) - 2);
    call_dissector(ipv6_handle, ipv6_tvb, pinfo, parentTree);
    *offset = tvb_reported_length(tvb) - 2;

    /* Display the VCRC */
    proto_tree_add_item(parentTree, hf_infiniband_variant_crc, tvb, *offset, 2, ENC_BIG_ENDIAN);
}

/* Parse EtherType - Parse a generic IP packaet with an EtherType of IP or ARP
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset
* IN: pinfo - packet info from wireshark
* IN: top_tree - parent tree of Infiniband dissector */
static void parse_RWH(proto_tree *ah_tree, tvbuff_t *tvb, int *offset, packet_info *pinfo, proto_tree *top_tree)
{
    uint16_t  ether_type;
    tvbuff_t *next_tvb;

    /* RWH - Raw Header */
    proto_item *RWH_header_item;
    proto_tree *RWH_header_tree;

    int captured_length, reported_length;

    RWH_header_item = proto_tree_add_item(ah_tree, hf_infiniband_RWH, tvb, *offset, 4, ENC_NA);
    proto_item_set_text(RWH_header_item, "%s", "RWH - Raw Header");
    RWH_header_tree = proto_item_add_subtree(RWH_header_item, ett_rwh);

    proto_tree_add_item(RWH_header_tree, hf_infiniband_reserved, tvb,
            *offset, 2, ENC_NA);

    *offset += 2;

    ether_type = tvb_get_ntohs(tvb, *offset);
    proto_tree_add_uint(RWH_header_tree, hf_infiniband_etype, tvb, *offset, 2,
                        ether_type);
    *offset += 2;

    /* Get the captured length and reported length of the data
     * after the Ethernet type. */
    captured_length = tvb_captured_length_remaining(tvb, *offset);
    reported_length = tvb_reported_length_remaining(tvb, *offset);

    /* Construct a tvbuff for the payload after the Ethernet type,
     * not including the FCS. */
    if ((captured_length >= 0) && (reported_length >= 0)) {
        if (reported_length >= 2)
            reported_length -= 2;
    }

    next_tvb = tvb_new_subset_length(tvb, *offset, reported_length);
    if (!dissector_try_uint(ethertype_dissector_table, ether_type,
            next_tvb, pinfo, top_tree))
       call_data_dissector(next_tvb, pinfo, top_tree);

    *offset = tvb_reported_length(tvb) - 2;
    /* Display the VCRC */
    proto_tree_add_item(ah_tree, hf_infiniband_variant_crc, tvb, *offset, 2, ENC_BIG_ENDIAN);
}


/* Parse a Mellanox EoIB Encapsulation Header and the associated Ethernet frame
* IN: parentTree to add the dissection to - in this code the all_headers_tree
* IN: tvb - the data buffer from wireshark
* IN: The current offset
* IN: pinfo - packet info from wireshark
* IN: top_tree - parent tree of Infiniband dissector */
static bool dissect_mellanox_eoib(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    proto_item *header_item;
    proto_tree *header_subtree;
    tvbuff_t   *encap_tvb;
    int         offset = 0;
    bool        more_segments;
    struct infinibandinfo *info = (struct infinibandinfo *)data;

    if (((info->opCode & 0xE0) >> 5) != TRANSPORT_UD)
        return false;

    if ((tvb_get_uint8(tvb, offset) & 0xF0) != 0xC0)
        return false;

    if (tvb_reported_length(tvb) < 4) {
        /* not even large enough to contain the eoib encap header. error! */
        return false;
    }

    header_item = proto_tree_add_item(tree, proto_mellanox_eoib, tvb, offset, 4, ENC_NA);
    header_subtree = proto_item_add_subtree(header_item, ett_eoib);

    proto_tree_add_item(header_subtree, hf_infiniband_ver, tvb, offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(header_subtree, hf_infiniband_tcp_chk, tvb, offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(header_subtree, hf_infiniband_ip_chk, tvb, offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(header_subtree, hf_infiniband_fcs, tvb, offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(header_subtree, hf_infiniband_ms, tvb, offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(header_subtree, hf_infiniband_seg_off, tvb, offset, 2, ENC_BIG_ENDIAN);
    more_segments = tvb_get_ntohs(tvb, offset) & (MELLANOX_MORE_SEGMENT_FLAG | MELLANOX_SEGMENT_FLAG);
    offset += 2;
    proto_tree_add_item(header_subtree, hf_infiniband_seg_id, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    encap_tvb = tvb_new_subset_remaining(tvb, offset);

    if (more_segments) {
        /* this is a fragment of an encapsulated Ethernet jumbo frame, parse as data */
        call_data_dissector(encap_tvb, pinfo, tree);
    } else {
        /* non-fragmented frames can be fully parsed */
        call_dissector(eth_handle, encap_tvb, pinfo, tree);
    }

    return true;
}

/* IBA packet data could be anything in principle, however it is common
 * practice to carry non-IBA data encapsulated with an EtherType header,
 * similar to the RWH header. There is no way to identify these frames
 * positively.
 */
static bool dissect_eth_over_ib(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
    uint16_t etype, reserved;
    const char *saved_proto;
    tvbuff_t   *next_tvb;
    struct infinibandinfo *info = (struct infinibandinfo *)data;
    volatile bool   dissector_found = false;

    if (tvb_reported_length(tvb) < 4) {
        /* not even large enough to contain the eoib encap header. error! */
        return false;
    }

    etype    = tvb_get_ntohs(tvb, 0);
    reserved = tvb_get_ntohs(tvb, 2);

    if (reserved != 0)
        return false;

    next_tvb = tvb_new_subset_remaining(tvb, 4);

    /* Look for sub-dissector, and call it if found.
        Catch exceptions, so that if the reported length of "next_tvb"
        was reduced by some dissector before an exception was thrown,
        we can still put in an item for the trailer. */
    saved_proto = pinfo->current_proto;

    TRY {
        dissector_found = (bool)dissector_try_uint(ethertype_dissector_table,
                                etype, next_tvb, pinfo, tree);
    }
    CATCH_NONFATAL_ERRORS {
        /* Somebody threw an exception that means that there
            was a problem dissecting the payload; that means
            that a dissector was found, so we don't need to
            dissect the payload as data or update the protocol
            or info columns.

            Just show the exception and then drive on to show
            the trailer, after noting that a dissector was found
            and restoring the protocol value that was in effect
            before we called the subdissector.
        */

        show_exception(next_tvb, pinfo, tree, EXCEPT_CODE, GET_MESSAGE);
        dissector_found = true;
        pinfo->current_proto = saved_proto;
    }
    ENDTRY;

    if (dissector_found) {
        proto_item *PAYLOAD_header_item;
        proto_tree *PAYLOAD_header_tree;

        /* now create payload entry to show Ethertype */
        PAYLOAD_header_item = proto_tree_add_item(info->payload_tree, hf_infiniband_payload, tvb, 0,
            tvb_reported_length(tvb) - 6, ENC_NA);
        proto_item_set_text(PAYLOAD_header_item, "%s", "IBA Payload - appears to be EtherType encapsulated");
        PAYLOAD_header_tree = proto_item_add_subtree(PAYLOAD_header_item, ett_payload);

        proto_tree_add_uint(PAYLOAD_header_tree, hf_infiniband_etype, tvb, 0, 2, etype);
        proto_tree_add_item(PAYLOAD_header_tree, hf_infiniband_reserved, tvb, 2, 2, ENC_NA);
    }

    return dissector_found;
}

/* Parse Subnet Management (LID Routed)
* IN: parentTree to add the dissection to
* IN: pinfo - packet info from wireshark
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_SUBN_LID_ROUTED(proto_tree *parentTree, packet_info *pinfo, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *SUBN_LID_ROUTED_header_item;
    proto_tree *SUBN_LID_ROUTED_header_tree;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }

    local_offset = *offset;

    /* local_offset - 24 here because when we come out of parse_MAD_Common, the offset it sitting at the data section. */
    SUBN_LID_ROUTED_header_item = proto_tree_add_item(parentTree, hf_infiniband_SMP_LID, tvb, local_offset - 24, 256, ENC_NA);
    proto_item_set_text(SUBN_LID_ROUTED_header_item, "%s", "SMP (LID Routed) ");
    SUBN_LID_ROUTED_header_tree = proto_item_add_subtree(SUBN_LID_ROUTED_header_item, ett_subn_lid_routed);
    proto_tree_add_item(SUBN_LID_ROUTED_header_tree, hf_infiniband_m_key, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(SUBN_LID_ROUTED_header_tree, hf_infiniband_reserved, tvb, local_offset, 32, ENC_NA);
    local_offset += 32;

    label_SUBM_Method(SUBN_LID_ROUTED_header_item, &MadData, pinfo);
    label_SUBM_Attribute(SUBN_LID_ROUTED_header_item, &MadData, pinfo);

    /* Try to do the detail parse of the attribute.  If there is an error, or the attribute is unknown, we'll just highlight the generic data. */
    if (!parse_SUBM_Attribute(SUBN_LID_ROUTED_header_tree, tvb, &local_offset, &MadData))
    {
        proto_tree_add_item(SUBN_LID_ROUTED_header_tree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    local_offset += 64;
    }

    proto_tree_add_item(SUBN_LID_ROUTED_header_tree, hf_infiniband_reserved, tvb, local_offset, 128, ENC_NA);
    local_offset += 128;
    *offset = local_offset;
}

/* Parse Subnet Management (Directed Route)
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_SUBN_DIRECTED_ROUTE(proto_tree *parentTree, packet_info *pinfo, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *SUBN_DIRECTED_ROUTE_header_item;
    proto_tree *SUBN_DIRECTED_ROUTE_header_tree;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }

    local_offset = *offset;

    /* local_offset - 24 here because when we come out of parse_MAD_Common, the offset it sitting at the data section.
    * We need to go backwards because this particular SMP uses the class specific portion of the Common MAD Header */
    SUBN_DIRECTED_ROUTE_header_item = proto_tree_add_item(parentTree, hf_infiniband_SMP_DIRECTED, tvb, local_offset - 24, 256, ENC_NA);
    proto_item_set_text(SUBN_DIRECTED_ROUTE_header_item, "%s", "SMP (Directed Route) ");
    SUBN_DIRECTED_ROUTE_header_tree = proto_item_add_subtree(SUBN_DIRECTED_ROUTE_header_item, ett_subn_directed_route);

    label_SUBM_Method(SUBN_DIRECTED_ROUTE_header_item, &MadData, pinfo);
    label_SUBM_Attribute(SUBN_DIRECTED_ROUTE_header_item, &MadData, pinfo);

    /* Place us at offset 4, the "D" Bit (Direction bit for Directed Route SMPs) */
    local_offset -= 20;
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_d, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_smp_status, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_hop_pointer, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_hop_count, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    local_offset += 16; /* Skip over the rest of the Common MAD Header... It's already dissected by parse_MAD_Common */
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_m_key, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_dr_slid, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_dr_dlid, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_reserved, tvb, local_offset, 28, ENC_NA);
    local_offset += 28;

    /* Try to do the detail parse of the attribute.  If there is an error, or the attribute is unknown, we'll just highlight the generic data. */
    if (!parse_SUBM_Attribute(SUBN_DIRECTED_ROUTE_header_tree, tvb, &local_offset, &MadData))
    {
        proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    local_offset += 64;
    }

    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_initial_path, tvb, local_offset, 64, ENC_NA);
    local_offset += 64;
    proto_tree_add_item(SUBN_DIRECTED_ROUTE_header_tree, hf_infiniband_return_path, tvb, local_offset, 64, ENC_NA);
    local_offset += 64;
    *offset = local_offset;
}

/* Parse Subnet Administration
* IN: parentTree to add the dissection to
* IN: pinfo - packet info from wireshark
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_SUBNADMN(proto_tree *parentTree, packet_info *pinfo, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *SUBNADMN_header_item;
    proto_tree *SUBNADMN_header_tree;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }
    if (!parse_RMPP(parentTree, tvb, offset))
    {
        /* TODO: Mark Corrupt Packet */
        return;
    }
    local_offset = *offset;

    SUBNADMN_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset - 36, 256, ENC_NA);
    proto_item_set_text(SUBNADMN_header_item, "%s", "SA ");
    SUBNADMN_header_tree = proto_item_add_subtree(SUBNADMN_header_item, ett_subnadmin);

    proto_tree_add_item(SUBNADMN_header_tree, hf_infiniband_sm_key, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(SUBNADMN_header_tree, hf_infiniband_attribute_offset, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SUBNADMN_header_tree, hf_infiniband_reserved, tvb, local_offset, 2, ENC_NA);
    local_offset += 2;
    proto_tree_add_item(SUBNADMN_header_tree, hf_infiniband_component_mask, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;

    label_SUBA_Method(SUBNADMN_header_item, &MadData, pinfo);
    label_SUBA_Attribute(SUBNADMN_header_item, &MadData, pinfo);

    if (!parse_SUBA_Attribute(SUBNADMN_header_tree, tvb, &local_offset, &MadData))
    {
        proto_tree_add_item(SUBNADMN_header_tree, hf_infiniband_subnet_admin_data, tvb, local_offset, 200, ENC_NA);
    local_offset += 200;
    }
    *offset = local_offset;
}

/* Parse Performance Management
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN: pinfo - the pinfo struct from wireshark
* IN/OUT: The current and updated offset */
static void parse_PERF(proto_tree *parentTree, tvbuff_t *tvb, packet_info *pinfo, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *PERF_header_item;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }

    local_offset = *offset; /* offset now points to the start of the MAD data field */

    switch (MadData.attributeID) {
        case 0x0001: /* (ClassPortInfo) */
            col_set_str(pinfo->cinfo, COL_INFO, "PERF (ClassPortInfo)");
            proto_tree_add_item(parentTree, hf_infiniband_PerfMgt_ClassPortInfo, tvb, local_offset, 40, ENC_NA);
            local_offset += 40;
            *offset = local_offset;
            parse_ClassPortInfo(parentTree, tvb, offset);
            break;
        case ATTR_PORT_COUNTERS:
            parse_PERF_PortCounters(parentTree, tvb, pinfo, &local_offset);
            break;
        case ATTR_PORT_COUNTERS_EXT:
            parse_PERF_PortCountersExtended(parentTree, tvb, pinfo, &local_offset);
            break;
        default:
            col_set_str(pinfo->cinfo, COL_INFO, "PERF (Unknown Attribute)");
            PERF_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, MAD_DATA_SIZE, ENC_NA);
            local_offset += MAD_DATA_SIZE;
            proto_item_set_text(PERF_header_item, "%s", "PERF - Performance Management MAD (Dissector Not Implemented)");
            break;
    }

    *offset = local_offset;
}

/* Parse Baseboard Management
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_BM(proto_tree *parentTree, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *BM_header_item;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }
    local_offset = *offset;

    BM_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, MAD_DATA_SIZE, ENC_NA);
    local_offset += MAD_DATA_SIZE;
    proto_item_set_text(BM_header_item, "%s", "BM - Baseboard Management MAD (Dissector Not Implemented)");
    *offset = local_offset;
}

/* Parse Device Management
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_DEV_MGT(proto_tree *parentTree, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *DEVM_header_item;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }
    local_offset = *offset;
    DEVM_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, MAD_DATA_SIZE, ENC_NA);
    local_offset += MAD_DATA_SIZE;
    proto_item_set_text(DEVM_header_item, "%s", "DEV_MGT - Device Management MAD (Dissector Not Implemented)");
    *offset = local_offset;
}

static bool parse_CM_Req_ServiceID(proto_tree *parent_tree, tvbuff_t *tvb, int *offset, uint64_t serviceid)
{
    proto_item *service_id_item;
    proto_tree *service_id_tree;
    int         local_offset = *offset;
    bool ip_cm_sid;

    if ((serviceid & RDMA_IP_CM_SID_PREFIX_MASK) == RDMA_IP_CM_SID_PREFIX) {
        service_id_item = proto_tree_add_item(parent_tree, hf_cm_req_service_id, tvb, local_offset, 8, ENC_BIG_ENDIAN);
        proto_item_set_text(service_id_item, "%s", "IP CM ServiceID");
        service_id_tree = proto_item_add_subtree(service_id_item, ett_cm_sid);

        proto_tree_add_item(service_id_tree, hf_cm_req_service_id_prefix, tvb, local_offset, 5, ENC_NA);
        local_offset += 5;
        proto_tree_add_item(service_id_tree, hf_cm_req_service_id_protocol, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        local_offset += 1;
        proto_tree_add_item(service_id_tree, hf_cm_req_service_id_dport, tvb, local_offset, 2, ENC_BIG_ENDIAN);
        local_offset += 2;
        ip_cm_sid = true;
    } else {
        proto_tree_add_item(parent_tree, hf_cm_req_service_id, tvb, local_offset, 8, ENC_BIG_ENDIAN);
        local_offset += 8;
        ip_cm_sid = false;
    }
    *offset = local_offset;
    return ip_cm_sid;
}

static uint64_t make_hash_key(uint64_t transcationID, address *addr)
{
    uint64_t hash_key;

    hash_key = transcationID;
    hash_key = add_address_to_hash64(hash_key, addr);
    return hash_key;
}

static connection_context* lookup_connection(uint64_t transcationID, address *addr)
{
    connection_context *connection;
    uint64_t hash_key;

    hash_key = make_hash_key(transcationID, addr);

    connection = (connection_context *)g_hash_table_lookup(CM_context_table, &hash_key);
    return connection;
}

static void remove_connection(uint64_t transcationID, address *addr)
{
    uint64_t hash_key;

    hash_key = make_hash_key(transcationID, addr);

    g_hash_table_remove(CM_context_table, &hash_key);
}

static void
create_conv_and_add_proto_data(packet_info *pinfo, uint64_t service_id,
                               bool client_to_server,
                               address *addr, const uint16_t lid,
                               const uint32_t port, const uint32_t src_port,
                               const unsigned options, uint8_t *mad_data)
{
    conversation_t *conv;
    conversation_infiniband_data *proto_data;

    proto_data = wmem_new0(wmem_file_scope(), conversation_infiniband_data);
    proto_data->service_id = service_id;
    proto_data->client_to_server = client_to_server;
    proto_data->src_qp = src_port;
    memcpy(&proto_data->mad_private_data[0], mad_data, MAD_DATA_SIZE);
    conv = conversation_new(pinfo->num, addr, addr,
                            CONVERSATION_IBQP, port, port, options);
    conversation_add_proto_data(conv, proto_infiniband, proto_data);

    /* next, register the conversation using the LIDs */
    set_address(addr, AT_IB, sizeof(uint16_t), wmem_memdup(pinfo->pool, &lid, sizeof lid));
    conv = conversation_new(pinfo->num, addr, addr,
                            CONVERSATION_IBQP, port, port, options);
    conversation_add_proto_data(conv, proto_infiniband, proto_data);
}

static void save_conversation_info(packet_info *pinfo, uint8_t *local_gid, uint8_t *remote_gid,
                                   uint32_t local_qpn, uint32_t local_lid, uint32_t remote_lid,
                                   uint64_t serviceid, MAD_Data *MadData)
{
    /* the following saves information about the conversation this packet defines,
       so there's no point in doing it more than once per packet */
    if (!pinfo->fd->visited)
    {
        connection_context *connection;
        conversation_infiniband_data *proto_data;
        conversation_t *conv;
        uint64_t *hash_key = g_new(uint64_t, 1);

        /* create a new connection context and store it in the hash table */
        connection = g_new(connection_context, 1);

        if (pinfo->dst.type == AT_IPv4) {
            memcpy(&(connection->req_gid), local_gid, 4);
            memcpy(&(connection->resp_gid), remote_gid, 4);
        } else {
            memcpy(&(connection->req_gid), local_gid, GID_SIZE);
            memcpy(&(connection->resp_gid), remote_gid, GID_SIZE);
        }
        connection->req_lid = local_lid;
        connection->resp_lid = remote_lid;
        connection->req_qp = local_qpn;
        connection->resp_qp = 0;   /* not currently known. we'll fill this in later */
        connection->service_id = serviceid;

        /* save the context to the context hash table, for retrieval when the corresponding
           CM REP message arrives */
        *hash_key = make_hash_key(MadData->transactionID, &pinfo->src);
        g_hash_table_replace(CM_context_table, hash_key, connection);

        /* Now we create a conversation for the CM exchange. This uses both
           sides of the conversation since CM packets also include the source
           QPN */
        proto_data = wmem_new0(wmem_file_scope(), conversation_infiniband_data);
        proto_data->service_id = connection->service_id;
        proto_data->client_to_server = true;

        conv = conversation_new(pinfo->num, &pinfo->src, &pinfo->dst,
                                CONVERSATION_IBQP, pinfo->srcport, pinfo->destport, 0);
        conversation_add_proto_data(conv, proto_infiniband, proto_data);

        /* create unidirection conversation for packets that will flow from
         * server to client.
         */
        create_conv_and_add_proto_data(pinfo, connection->service_id, false,
                                       &pinfo->src, connection->req_lid,
                                       connection->req_qp, 0, NO_ADDR2|NO_PORT2,
                                       &MadData->data[0]);
    }
}

static void parse_IP_CM_Req_Msg(proto_tree *parent_tree, tvbuff_t *tvb, int local_offset)
{
    proto_item *private_data_item;
    proto_tree *private_data_tree;
    uint8_t ipv;

    private_data_item = proto_tree_add_item(parent_tree, hf_cm_req_ip_cm_req_msg, tvb, local_offset, 92, ENC_NA);
    proto_item_set_text(private_data_item, "%s", "IP CM Private Data");
    private_data_tree = proto_item_add_subtree(private_data_item, ett_cm_ipcm);

    proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_majv, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_minv, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    ipv = (tvb_get_uint8(tvb, local_offset) & 0xf0) >> 4;

    proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_ipv, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_res, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_sport, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;

    if (ipv == 4) {
        /* skip first 12 bytes of zero for sip */
        proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_sip4, tvb, local_offset + 12, 4, ENC_BIG_ENDIAN);
        local_offset += 16;
        /* skip first 12 bytes of zero for dip */
        proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_dip4, tvb, local_offset + 12, 4, ENC_BIG_ENDIAN);
    } else {
        proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_sip6, tvb, local_offset, 16, ENC_NA);
        local_offset += 16;
        proto_tree_add_item(private_data_tree, hf_cm_req_ip_cm_dip6, tvb, local_offset, 16, ENC_NA);
    }
    local_offset += 16;

    /* finally add the consumer specific private data as undecoded data */
    proto_tree_add_item(private_data_tree, hf_ip_cm_req_consumer_private_data,
 tvb, local_offset, 56, ENC_NA);
}

static void parse_CM_Req(proto_tree *top_tree, packet_info *pinfo, tvbuff_t *tvb, int *offset,
                         MAD_Data *MadData, proto_tree *CM_header_tree, struct infinibandinfo *info)
{
    tvbuff_t *next_tvb;
    heur_dtbl_entry_t *hdtbl_entry;
    uint8_t    *local_gid, *remote_gid;
    uint64_t serviceid;
    int     local_offset;
    uint32_t local_qpn;
    uint32_t local_lid;
    uint32_t remote_lid;
    bool ip_cm_sid;

    local_offset = *offset;

    proto_tree_add_item(CM_header_tree, hf_cm_req_local_comm_id, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
    local_offset += 4;

    serviceid = tvb_get_ntoh64(tvb, local_offset);
    ip_cm_sid = parse_CM_Req_ServiceID(CM_header_tree, tvb, &local_offset, serviceid);

    proto_tree_add_item(CM_header_tree, hf_cm_req_local_ca_guid, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(CM_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_req_local_qkey, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_req_local_qpn, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_qpn = tvb_get_ntoh24(tvb, local_offset);
    local_offset += 3;
    proto_tree_add_item(CM_header_tree, hf_cm_req_respo_res, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_local_eecn, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(CM_header_tree, hf_cm_req_init_depth, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_remote_eecn, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(CM_header_tree, hf_cm_req_remote_cm_resp_to, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_transp_serv_type, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_e2e_flow_ctrl, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_start_psn, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(CM_header_tree, hf_cm_req_local_cm_resp_to, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_retry_count, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_pkey, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(CM_header_tree, hf_cm_req_path_pp_mtu, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_rdc_exists, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_rnr_retry_count, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_max_cm_retries, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_srq, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_extended_transport, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_local_lid, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_lid = tvb_get_ntohs(tvb, local_offset);
    local_offset += 2;
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_remote_lid, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    remote_lid = tvb_get_ntohs(tvb, local_offset);
    local_offset += 2;

    if (pinfo->dst.type == AT_IPv4) {
        local_gid  = (uint8_t *)wmem_alloc(pinfo->pool, 4);
        proto_tree_add_item(CM_header_tree, hf_cm_req_primary_local_gid_ipv4, tvb, local_offset + 12, 4, ENC_BIG_ENDIAN);
        (*(uint32_t*)local_gid) = tvb_get_ipv4(tvb, local_offset + 12);
        local_offset += 16;

        remote_gid = (uint8_t *)wmem_alloc(pinfo->pool, 4);
        proto_tree_add_item(CM_header_tree, hf_cm_req_primary_remote_gid_ipv4, tvb, local_offset + 12, 4, ENC_BIG_ENDIAN);
        (*(uint32_t*)remote_gid) = tvb_get_ipv4(tvb, local_offset + 12);
    } else {
        local_gid = (uint8_t *)wmem_alloc(pinfo->pool, GID_SIZE);
        proto_tree_add_item(CM_header_tree, hf_cm_req_primary_local_gid, tvb, local_offset, 16, ENC_NA);
        tvb_get_ipv6(tvb, local_offset, (ws_in6_addr*)local_gid);
        local_offset += 16;

        remote_gid = (uint8_t *)wmem_alloc(pinfo->pool, GID_SIZE);
        proto_tree_add_item(CM_header_tree, hf_cm_req_primary_remote_gid, tvb, local_offset, 16, ENC_NA);
        tvb_get_ipv6(tvb, local_offset, (ws_in6_addr*)remote_gid);
    }
    local_offset += 16;

    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_flow_label, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_reserved0, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_packet_rate, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_traffic_class, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_hop_limit, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_sl, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_subnet_local, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_reserved1, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_local_ack_to, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_primary_reserved2, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_local_lid, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_remote_lid, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_local_gid, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_remote_gid, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(CM_header_tree, hf_cm_req_flow_label, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_reserved0, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_packet_rate, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_traffic_class, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_hop_limit, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_SL, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_subnet_local, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_reserved1, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_req_local_ACK_timeout, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_req_alt_reserved2, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    save_conversation_info(pinfo, local_gid, remote_gid, local_qpn, local_lid, remote_lid, serviceid, MadData);

    if (ip_cm_sid) {
        /* decode IP CM service specific private data */
        parse_IP_CM_Req_Msg(CM_header_tree, tvb, local_offset);
        /* ip_cm.req is 36 in length */
        next_tvb = tvb_new_subset_length(tvb, local_offset+36, 56);
    } else {
        /* Add the undecoded private data anyway as RDMA CM private data */
        proto_tree_add_item(CM_header_tree, hf_cm_req_private_data, tvb, local_offset, 92, ENC_NA);
        next_tvb = tvb_new_subset_length(tvb, local_offset, 92);
    }

    /* give a chance for subdissectors to analyze the private data */
    dissector_try_heuristic(heur_dissectors_cm_private, next_tvb, pinfo, top_tree, &hdtbl_entry, info);

    local_offset += 92;
    *offset = local_offset;
}

static void create_bidi_conv(packet_info *pinfo, connection_context *connection)
{
    conversation_t *conv;
    conversation_infiniband_data *proto_data;

    proto_data = wmem_new0(wmem_file_scope(), conversation_infiniband_data);
    proto_data->service_id = connection->service_id;
    proto_data->client_to_server = false;
    memset(&proto_data->mad_private_data[0], 0, MAD_DATA_SIZE);
    conv = conversation_new(pinfo->num, &pinfo->src, &pinfo->dst,
                            CONVERSATION_IBQP, connection->resp_qp,
                            connection->req_qp, 0);
    conversation_add_proto_data(conv, proto_infiniband, proto_data);
}

static void
attach_connection_to_pinfo(packet_info *pinfo, connection_context *connection,
                           MAD_Data *MadData)
{
    address req_addr,
            resp_addr;  /* we'll fill these in and pass them to conversation_new */

    /* RC traffic never(?) includes a field indicating the source QPN, so
       the destination host knows it only from previous context (a single
       QPN on the host that is part of an RC can only receive traffic from
       that RC). For this reason we do not register conversations with both
       sides, but rather we register the same conversation twice - once for
       each side of the Reliable Connection. */

    if (pinfo->dst.type == AT_IPv4) {
        set_address(&req_addr, AT_IPv4, 4, connection->req_gid);
        set_address(&resp_addr, AT_IPv4, 4, connection->resp_gid);
    } else if (pinfo->dst.type == AT_IPv6) {
        set_address(&req_addr, AT_IPv6, GID_SIZE, connection->req_gid);
        set_address(&resp_addr, AT_IPv6, GID_SIZE, connection->resp_gid);
    } else {
        set_address(&req_addr, AT_IB, GID_SIZE, connection->req_gid);
        set_address(&resp_addr, AT_IB, GID_SIZE, connection->resp_gid);
    }

    /* create conversations:
     * first for client to server direction so that upper level protocols
     * can do appropriate dissection depending on the message direction.
     */
    create_conv_and_add_proto_data(pinfo, connection->service_id, true,
                                   &resp_addr, connection->resp_lid,
                                   connection->resp_qp, connection->req_qp,
                                   NO_ADDR2|NO_PORT2, &MadData->data[0]);
    /* now create bidirectional connection that can be looked upon
     * by ULP for stateful context in both directions.
     */
    create_bidi_conv(pinfo, connection);
}

static void update_passive_conv_info(packet_info *pinfo,
                                     connection_context *connection)
{
    conversation_t *conv;
    conversation_infiniband_data *conv_data;

    conv = find_conversation(pinfo->num, &pinfo->dst, &pinfo->dst,
                             CONVERSATION_IBQP, connection->req_qp, connection->req_qp,
                             NO_ADDR_B|NO_PORT_B);
    if (!conv)
        return;   /* nothing to do with no conversation context */

    conv_data = (conversation_infiniband_data *)conversation_get_proto_data(conv, proto_infiniband);
    if (!conv_data)
        return;
    conv_data->src_qp = connection->resp_qp;
}

static void update_conversation_info(packet_info *pinfo,
                                     uint32_t remote_qpn,
                                     MAD_Data *MadData)
{
    /* the following saves information about the conversation this packet defines,
       so there's no point in doing it more than once per packet */
    if (!pinfo->fd->visited) {
        /* get the previously saved context for this connection */
        connection_context *connection;

        connection = lookup_connection(MadData->transactionID, &pinfo->dst);

        /* if an appropriate connection was not found there's something wrong, but nothing we can
           do about it here - so just skip saving the context */
        if (connection) {
            connection->resp_qp = remote_qpn;
            update_passive_conv_info(pinfo, connection);
            attach_connection_to_pinfo(pinfo, connection, MadData);
        }
    }
}

static void parse_CM_Rsp(proto_tree *top_tree, packet_info *pinfo, tvbuff_t *tvb, int *offset,
                         MAD_Data *MadData, proto_tree *CM_header_tree, struct infinibandinfo *info)
{
    tvbuff_t *next_tvb;
    heur_dtbl_entry_t *hdtbl_entry;
    uint32_t remote_qpn;
    int      local_offset;

    local_offset = *offset;

    proto_tree_add_item(CM_header_tree, hf_cm_rep_localcommid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_remotecommid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_localqkey, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_localqpn, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    remote_qpn = tvb_get_ntoh24(tvb, local_offset);
    local_offset += 3;
    proto_tree_add_item(CM_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_localeecontnum, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(CM_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_startingpsn, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(CM_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_responderres, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_initiatordepth, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_tgtackdelay, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_rep_failoveracc, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_rep_e2eflowctl, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_rnrretrycount, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_rep_srq, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_rep_reserved, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_localcaguid, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(CM_header_tree, hf_cm_rep_privatedata, tvb, local_offset, 196, ENC_NA);

    update_conversation_info(pinfo, remote_qpn, MadData);

    /* give a chance for subdissectors to get the private data */
    next_tvb = tvb_new_subset_length(tvb, local_offset, 196);
    dissector_try_heuristic(heur_dissectors_cm_private, next_tvb, pinfo, top_tree, &hdtbl_entry, info);

    local_offset += 196;
    *offset = local_offset;
}

static connection_context*
try_connection_dissectors(proto_tree *top_tree, packet_info *pinfo, tvbuff_t *tvb,
                          address *addr, MAD_Data *MadData, struct infinibandinfo *info,
                          int pdata_offset, int pdata_length)
{
    tvbuff_t *next_tvb;
    heur_dtbl_entry_t *hdtbl_entry;
    connection_context *connection;

    connection = lookup_connection(MadData->transactionID, addr);

    next_tvb = tvb_new_subset_length(tvb, pdata_offset, pdata_length);
    dissector_try_heuristic(heur_dissectors_cm_private, next_tvb, pinfo, top_tree,
                            &hdtbl_entry, info);
    return connection;
}

static void parse_CM_Rtu(proto_tree *top_tree, packet_info *pinfo, tvbuff_t *tvb, int *offset,
                         MAD_Data *MadData, proto_tree *CM_header_tree,
                         struct infinibandinfo *info)
{
    int      local_offset;

    local_offset = *offset;
    proto_tree_add_item(CM_header_tree, hf_cm_rtu_localcommid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_rtu_remotecommid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_rtu_privatedata, tvb, local_offset, 224, ENC_NA);
    try_connection_dissectors(top_tree, pinfo, tvb, &pinfo->src, MadData, info, local_offset, 224);
    local_offset += 224;
    *offset = local_offset;
}

static void parse_CM_Rej(proto_tree *top_tree, packet_info *pinfo, tvbuff_t *tvb, int *offset,
                         MAD_Data *MadData, proto_tree *CM_header_tree,
                         struct infinibandinfo *info)
{
    int      local_offset;

    local_offset = *offset;
    proto_tree_add_item(CM_header_tree, hf_cm_rej_local_commid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_rej_remote_commid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_rej_msg_rej, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_rej_msg_reserved0, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rej_rej_info_len, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(CM_header_tree, hf_cm_rej_msg_reserved1, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_rej_reason, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(CM_header_tree, hf_cm_rej_add_rej_info, tvb, local_offset, 72, ENC_NA);
    local_offset += 72;
    proto_tree_add_item(CM_header_tree, hf_cm_rej_private_data, tvb, local_offset, 148, ENC_NA);

    try_connection_dissectors(top_tree, pinfo, tvb, &pinfo->dst, MadData, info, local_offset, 148);
    local_offset += 148;
    *offset = local_offset;
}

static void parse_CM_DReq(proto_tree *top_tree, packet_info *pinfo, tvbuff_t *tvb, int *offset,
                         MAD_Data *MadData, proto_tree *CM_header_tree,
                         struct infinibandinfo *info)
{
    int      local_offset;

    local_offset = *offset;
    proto_tree_add_item(CM_header_tree, hf_cm_dreq_localcommid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_dreq_remotecommid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_dreq_remote_qpn, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(CM_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(CM_header_tree, hf_cm_dreq_privatedata, tvb, local_offset, 220, ENC_NA);
    try_connection_dissectors(top_tree, pinfo, tvb, &pinfo->src, MadData, info, local_offset, 220);
    local_offset += 220;
    *offset = local_offset;
}

static void parse_CM_DRsp(proto_tree *top_tree, packet_info *pinfo, tvbuff_t *tvb, int *offset,
                          MAD_Data *MadData, proto_tree *CM_header_tree,
                          struct infinibandinfo *info)
{
    connection_context *connection;
    int      local_offset;

    local_offset = *offset;
    proto_tree_add_item(CM_header_tree, hf_cm_drsp_localcommid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_drsp_remotecommid, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(CM_header_tree, hf_cm_drsp_privatedata, tvb, local_offset, 224, ENC_NA);

    /* connection is closing so remove entry from the connection table */
    connection = try_connection_dissectors(top_tree, pinfo, tvb, &pinfo->dst, MadData, info, local_offset, 224);
    if (connection)
        remove_connection(MadData->transactionID, &pinfo->dst);

    local_offset += 224;
    *offset = local_offset;
}

/* Parse Communication Management
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_COM_MGT(proto_tree *parentTree, packet_info *pinfo, tvbuff_t *tvb, int *offset, proto_tree* top_tree)
{
    MAD_Data    MadData;
    struct infinibandinfo info = { NULL, 0, 0, 0, 0, 0, 0, 0, false, false};
    int         local_offset;
    const char *label;
    proto_item *CM_header_item;
    proto_tree *CM_header_tree;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }
    local_offset = *offset;

    CM_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, MAD_DATA_SIZE, ENC_NA);

    label = val_to_str_const(MadData.attributeID, CM_Attributes, "(Unknown CM Attribute)");

    proto_item_set_text(CM_header_item, "CM %s", label);
    col_add_fstr(pinfo->cinfo, COL_INFO, "CM: %s", label);

    CM_header_tree = proto_item_add_subtree(CM_header_item, ett_cm);

    info.payload_tree = parentTree;
    switch (MadData.attributeID) {
        case ATTR_CM_REQ:
            info.cm_attribute_id = ATTR_CM_REQ;
            parse_CM_Req(top_tree, pinfo, tvb, &local_offset, &MadData, CM_header_tree, &info);
            break;
        case ATTR_CM_REP:
            info.cm_attribute_id = ATTR_CM_REP;
            parse_CM_Rsp(top_tree, pinfo, tvb, &local_offset, &MadData, CM_header_tree, &info);
            break;
        case ATTR_CM_RTU:
            info.cm_attribute_id = ATTR_CM_RTU;
            parse_CM_Rtu(top_tree, pinfo, tvb, &local_offset, &MadData, CM_header_tree, &info);
            break;
        case ATTR_CM_REJ:
            info.cm_attribute_id = ATTR_CM_REJ;
            parse_CM_Rej(top_tree, pinfo, tvb, &local_offset, &MadData, CM_header_tree, &info);
            break;
        case ATTR_CM_DREQ:
            info.cm_attribute_id = ATTR_CM_DREQ;
            parse_CM_DReq(top_tree, pinfo, tvb, &local_offset, &MadData, CM_header_tree, &info);
            break;
        case ATTR_CM_DRSP:
            info.cm_attribute_id = ATTR_CM_DRSP;
            parse_CM_DRsp(top_tree, pinfo, tvb, &local_offset, &MadData, CM_header_tree, &info);
            break;
        default:
            proto_item_append_text(CM_header_item, " (Dissector Not Implemented)");
            local_offset += MAD_DATA_SIZE;
            break;
    }

    *offset = local_offset;
}

/* Parse SNMP Tunneling
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_SNMP(proto_tree *parentTree, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *SNMP_header_item;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }
    local_offset = *offset;

    SNMP_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, MAD_DATA_SIZE, ENC_NA);
    local_offset += MAD_DATA_SIZE;
    proto_item_set_text(SNMP_header_item, "%s", "SNMP - SNMP Tunneling MAD (Dissector Not Implemented)");
    *offset = local_offset;
}

/* Parse Vendor Specific Management Packets
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_VENDOR_MANAGEMENT(proto_tree *parentTree, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *VENDOR_header_item;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }
    local_offset = *offset;

    VENDOR_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, MAD_DATA_SIZE, ENC_NA);
    local_offset += MAD_DATA_SIZE;
    proto_item_set_text(VENDOR_header_item, "%s", "VENDOR - Vendor Specific Management MAD (Dissector Not Implemented)");
    *offset = local_offset;
}

/* Parse Application Specific Management Packets
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_APPLICATION_MANAGEMENT(proto_tree *parentTree, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *APP_header_item;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }
    local_offset = *offset;
    APP_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, MAD_DATA_SIZE, ENC_NA);
    local_offset += MAD_DATA_SIZE;
    proto_item_set_text(APP_header_item, "%s", "APP - Application Specific MAD (Dissector Not Implemented)");
    *offset = local_offset;
}

/* Parse Reserved Management Packets.

* This is an !ERROR CONDITION!
* It means that the Management Class value used was defined as a reserved value for furture use.
* This method is here since we will want to report this information directly to the UI without blowing up Wireshark.

* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static void parse_RESERVED_MANAGEMENT(proto_tree *parentTree, tvbuff_t *tvb, int *offset)
{
    /* Parse the Common MAD Header */
    MAD_Data    MadData;
    int         local_offset;
    proto_item *RESV_header_item;

    if (!parse_MAD_Common(parentTree, tvb, offset, &MadData))
    {
        /* TODO: Mark Corrupt Packet - Not enough bytes exist for at least the Common MAD header which is present in all MAD packets */
        return;
    }
    local_offset = *offset;
    RESV_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 256, ENC_NA);
    local_offset += 256;
    proto_item_set_text(RESV_header_item, "%s", "RESERVED - Reserved MAD Type (Possible Device Error)");
    *offset = local_offset;
}

/* Parse the common MAD Header
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset
* IN/OUT: MadData - the data from the MAD header */
static bool parse_MAD_Common(proto_tree *parentTree, tvbuff_t *tvb, int *offset, MAD_Data* MadData)
{
    int         local_offset = *offset;
    proto_item *MAD_header_item;
    proto_tree *MAD_header_tree;

    if (MadData == NULL)
        return false;
    if (!tvb_bytes_exist(tvb, *offset, 256))
        return false;

    /* Get the Management Class to decide between LID Routed and Direct Route */
    MadData->managementClass =      tvb_get_uint8(tvb, local_offset + 1);
    MadData->classVersion =         tvb_get_uint8(tvb, local_offset + 2);
    MadData->method =               tvb_get_uint8(tvb, local_offset + 3);
    MadData->status =               tvb_get_uint8(tvb, local_offset + 4);
    MadData->classSpecific =        tvb_get_ntohs(tvb, local_offset + 6);
    MadData->transactionID =        tvb_get_ntoh64(tvb, local_offset + 8);
    MadData->attributeID =          tvb_get_ntohs(tvb, local_offset + 16);
    MadData->attributeModifier =    tvb_get_ntohl(tvb, local_offset + 20);
    tvb_memcpy(tvb, MadData->data, local_offset + 24, MAD_DATA_SIZE);

    /* Populate the Dissector Tree */

    MAD_header_item = proto_tree_add_item(parentTree, hf_infiniband_MAD, tvb, local_offset, 256, ENC_NA);
    proto_item_set_text(MAD_header_item, "%s", "MAD Header - Common Management Datagram");
    MAD_header_tree = proto_item_add_subtree(MAD_header_item, ett_mad);

    proto_tree_add_item(MAD_header_tree, hf_infiniband_base_version, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_mgmt_class, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_class_version, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_method, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_status, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_class_specific, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_transaction_id, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_attribute_id, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_reserved, tvb, local_offset, 2, ENC_NA);
    local_offset += 2;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_attribute_modifier, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(MAD_header_tree, hf_infiniband_data, tvb, local_offset, MAD_DATA_SIZE, ENC_NA);
    *offset = local_offset; /* Move the offset to the start of the Data field - this will be where the other parsers start. */

    return true;
}

/* Parse the RMPP (Reliable Multi-Packet Transaction Protocol
* IN: parentTree to add the dissection to
* IN: tvb - the data buffer from wireshark
* IN/OUT: The current and updated offset */
static bool parse_RMPP(proto_tree *parentTree, tvbuff_t *tvb, int *offset)
{
    int         local_offset = *offset;
    uint8_t     RMPP_Type    = tvb_get_uint8(tvb, local_offset + 1);
    proto_item *RMPP_header_item;
    proto_tree *RMPP_header_tree;

    RMPP_header_item = proto_tree_add_item(parentTree, hf_infiniband_RMPP, tvb, local_offset, 12, ENC_NA);
    proto_item_set_text(RMPP_header_item, "%s", val_to_str(RMPP_Type, RMPP_Packet_Types, "Reserved RMPP Type! (0x%02x)"));
    RMPP_header_tree = proto_item_add_subtree(RMPP_header_item, ett_rmpp);

    proto_tree_add_item(RMPP_header_tree, hf_infiniband_rmpp_version, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(RMPP_header_tree, hf_infiniband_rmpp_type, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(RMPP_header_tree, hf_infiniband_r_resp_time, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(RMPP_header_tree, hf_infiniband_rmpp_flags, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(RMPP_header_tree, hf_infiniband_rmpp_status, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    switch (RMPP_Type)
    {
        case RMPP_NOT_USED:
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_rmpp_data1, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_rmpp_data2, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            break;
        case RMPP_DATA:
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_segment_number, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_payload_length32, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_transferred_data, tvb, local_offset, 220, ENC_NA);
            break;
        case RMPP_ACK:
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_segment_number, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_new_window_last, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_reserved, tvb, local_offset, 220, ENC_NA);
            break;
        case RMPP_STOP:
        case RMPP_ABORT:
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
            local_offset += 4;
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
            local_offset += 4;
            proto_tree_add_item(RMPP_header_tree, hf_infiniband_optional_extended_error_data, tvb, local_offset, 220, ENC_NA);
            break;
        default:
            break;
    }
    *offset = local_offset;
    return true;
}

/* Parse the Method from the MAD Common Header.
* Simply used to generate the identifier.
* IN: SubMItem - the item to append the method label to.
* IN: MadHeader - the MadData structure that contains the information from the Common MAD header.
* IN: pinfo - packet info from wireshark. */
static void label_SUBM_Method(proto_item *SubMItem, MAD_Data *MadHeader, packet_info *pinfo)
{
    const char *label = val_to_str_const(MadHeader->method, SUBM_Methods, "(Unknown SubManagement Method!)");

    proto_item_append_text(SubMItem, "%s", label);
    col_append_str(pinfo->cinfo, COL_INFO, label);
}

/* Parse the SA Method from the MAD Common Header.
* Simply used to generate the identifier.
* IN: SubAItem - the item to append the method label to.
* IN: MadHeader - the MadData structure that contains the information from the Common MAD header.
* IN: pinfo - packet info from wireshark. */
static void label_SUBA_Method(proto_item *SubAItem, MAD_Data *MadHeader, packet_info *pinfo)
{
    const char *label = val_to_str_const(MadHeader->method, SUBA_Methods, "(Unknown SubAdministration Method!)");

    proto_item_append_text(SubAItem, "%s", label);
    col_append_str(pinfo->cinfo, COL_INFO, label);
}

/* Parse the Attribute from the MAD Common Header
* Simply used to generate the identifier.
* IN: SubMItem - the item to append the Attribute label to.
* IN: MadHeader - the MadData structure that contains the information from the Common MAD header.
* IN: pinfo - packet info from wireshark. */
static void label_SUBM_Attribute(proto_item *SubMItem, MAD_Data *MadHeader, packet_info *pinfo)
{
    const char *label = val_to_str_const(MadHeader->attributeID, SUBM_Attributes, "(Unknown SubManagement Attribute!)");

    proto_item_append_text(SubMItem, "%s", &label[11]);
    col_append_str(pinfo->cinfo, COL_INFO, &label[11]);
}

/* Parse the SA Attribute from the MAD Common Header
* Simply used to generate the identifier.
* IN: SubAItem - the item to append the Attribute label to.
* IN: MadHeader - the MadData structure that contains the information from the Common MAD header.
* IN: pinfo - packet info from wireshark. */
static void label_SUBA_Attribute(proto_item *SubAItem, MAD_Data *MadHeader, packet_info *pinfo)
{
    const char *label = val_to_str_const(MadHeader->attributeID, SUBA_Attributes, "(Unknown SubAdministration Attribute!)");

    proto_item_append_text(SubAItem, "%s", &label[11]);
    col_append_str(pinfo->cinfo, COL_INFO, &label[11]);
}

/* Parse the attribute from a Subnet Management Packet.
* IN: Parent Tree to add the item to in the dissection tree
* IN: tvbuff, offset - the data and where it is.
* IN: MAD_Data the data from the Common MAD Header that provides the information we need */
static bool parse_SUBM_Attribute(proto_tree *parentTree, tvbuff_t *tvb, int *offset, MAD_Data *MadHeader)
{
    uint16_t    attributeID = MadHeader->attributeID;
    proto_item *SUBM_Attribute_header_item;
    proto_tree *SUBM_Attribute_header_tree;

    SUBM_Attribute_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, *offset, 64, ENC_NA);
    proto_item_set_text(SUBM_Attribute_header_item, "%s", val_to_str(attributeID, SUBM_Attributes, "Unknown Attribute Type! (0x%02x)"));
    SUBM_Attribute_header_tree = proto_item_add_subtree(SUBM_Attribute_header_item, ett_subm_attribute);


    switch (attributeID)
    {
        case 0x0002:
            parse_NoticesAndTraps(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0010:
             parse_NodeDescription(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0011:
            parse_NodeInfo(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0012:
            parse_SwitchInfo(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0014:
            parse_GUIDInfo(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0015:
            parse_PortInfo(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0016:
            parse_P_KeyTable(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0017:
            parse_SLtoVLMappingTable(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0018:
            parse_VLArbitrationTable(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0019:
            parse_LinearForwardingTable(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x001A:
            parse_RandomForwardingTable(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x001B:
            parse_MulticastForwardingTable(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x001C:
            parse_LinkSpeedWidthPairsTable(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0020:
            parse_SMInfo(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0030:
            parse_VendorDiag(SUBM_Attribute_header_tree , tvb, offset);
            break;
        case 0x0031:
            parse_LedInfo(SUBM_Attribute_header_tree , tvb, offset);
            break;
        default:
            break;
    }


    *offset += 64;
    return true;

}
/* Parse the attribute from a Subnet Administration Packet.
* IN: Parent Tree to add the item to in the dissection tree
* IN: tvbuff, offset - the data and where it is.
* IN: MAD_Data the data from the Common MAD Header that provides the information we need */
static bool parse_SUBA_Attribute(proto_tree *parentTree, tvbuff_t *tvb, int *offset, MAD_Data *MadHeader)
{
    uint16_t    attributeID = MadHeader->attributeID;
    proto_item *SUBA_Attribute_header_item;
    proto_tree *SUBA_Attribute_header_tree;

    SUBA_Attribute_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, *offset, 200, ENC_NA);
    proto_item_set_text(SUBA_Attribute_header_item, "%s", val_to_str(attributeID, SUBA_Attributes, "Unknown Attribute Type! (0x%02x)"));
    SUBA_Attribute_header_tree = proto_item_add_subtree(SUBA_Attribute_header_item, ett_suba_attribute);

    /* Skim off the RID fields should they be present */
    parse_RID(SUBA_Attribute_header_tree, tvb, offset, MadHeader);

    /* Parse the rest of the attributes */
    switch (MadHeader->attributeID)
    {
        case 0x0001: /* (ClassPortInfo) */
            parse_ClassPortInfo(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0002: /* (Notice) */
            parse_NoticesAndTraps(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0003: /* (InformInfo) */
            parse_InformInfo(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0011: /* (NodeRecord) */
            parse_NodeInfo(SUBA_Attribute_header_tree, tvb, offset);
            *offset += 40;
            parse_NodeDescription(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0012: /* (PortInfoRecord) */
            parse_PortInfo(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0013: /* (SLtoVLMappingTableRecord) */
            parse_SLtoVLMappingTable(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0014: /* (SwitchInfoRecord) */
            parse_SwitchInfo(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0015: /*(LinearForwardingTableRecord) */
            parse_LinearForwardingTable(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0016: /* (RandomForwardingTableRecord) */
            parse_RandomForwardingTable(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0017: /* (MulticastForwardingTableRecord) */
            parse_MulticastForwardingTable(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0018: /* (SMInfoRecord) */
            parse_SMInfo(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0019: /* (LinkSpeedWidthPairsTableRecord) */
            parse_LinkSpeedWidthPairsTable(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x00F3: /*(InformInfoRecord) */
            parse_InformInfo(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0020: /* (LinkRecord) */
            parse_LinkRecord(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0030: /* (GuidInfoRecord) */
            parse_GUIDInfo(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0031: /*(ServiceRecord) */
            parse_ServiceRecord(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0033: /* (P_KeyTableRecord) */
            parse_P_KeyTable(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0035: /* (PathRecord) */
            parse_PathRecord(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0036: /* (VLArbitrationTableRecord) */
            parse_VLArbitrationTable(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0038: /* (MCMemberRecord) */
            parse_MCMemberRecord(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x0039: /* (TraceRecord) */
            parse_TraceRecord(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x003A: /* (MultiPathRecord) */
            parse_MultiPathRecord(SUBA_Attribute_header_tree, tvb, offset);
            break;
        case 0x003B: /* (ServiceAssociationRecord) */
            parse_ServiceAssociationRecord(SUBA_Attribute_header_tree, tvb, offset);
            break;
        default: /* (Unknown SubAdministration Attribute!) */
            /* We've already labeled as unknown in item construction */
            break;
    }

    *offset += 200;
    return true;
}

/* Subnet Management Attribute Parsing Methods.
*  Also Parsing for Attributes common to both SM/SA.
* The Subnet Admin Parsing methods will call some of these methods when an attribute is present within an SA MAD
*/


/* Parse ClassPortInfo Attribute Field
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins      */
static int parse_ClassPortInfo(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_tree *ClassPortInfo_header_tree;

    if (!parentTree)
        return *offset;

    ClassPortInfo_header_tree = parentTree;

    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_BaseVersion, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_ClassVersion, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_CapabilityMask, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_CapabilityMask2, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 3;

    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RespTimeValue, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RedirectGID, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RedirectTC, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RedirectSL, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RedirectFL, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RedirectLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RedirectP_Key, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_Reserved, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RedirectQP, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_RedirectQ_Key, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;

    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_TrapGID, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_TrapTC, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_TrapSL, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_TrapFL, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_TrapLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_TrapP_Key, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_Reserved, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_TrapQP, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(ClassPortInfo_header_tree, hf_infiniband_ClassPortInfo_TrapQ_Key, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;

    return local_offset;
}

/* Parse NoticeDataDetails Attribute Field
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins
*       trapNumber - The Trap ID of the Trap Data being Dissected  */

static int parse_NoticeDataDetails(proto_tree* parentTree, tvbuff_t* tvb, int *offset, uint16_t trapNumber)
{
    int         local_offset = *offset;
    proto_item *DataDetails_header_item;
    proto_tree *DataDetails_header_tree;

    if (!parentTree)
        return 0;

    DataDetails_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 54, ENC_NA);
    DataDetails_header_tree = proto_item_add_subtree(DataDetails_header_item, ett_datadetails);


    switch (trapNumber)
    {
        case 64:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 64 DataDetails");
            local_offset += 6;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
        break;
        case 65:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 65 DataDetails");
            local_offset += 6;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
        break;
        case 66:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 66 DataDetails");
            local_offset += 6;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
        break;
        case 67:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 67 DataDetails");
            local_offset += 6;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
        break;
        case 68:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 68 DataDetails");
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_COMP_MASK, tvb, local_offset, 8, ENC_BIG_ENDIAN);
            local_offset += 8;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_WAIT_FOR_REPATH, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        break;
        case 69:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 69 DataDetails");
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_COMP_MASK, tvb, local_offset, 8, ENC_BIG_ENDIAN);
            local_offset += 8;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_WAIT_FOR_REPATH, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        break;
        case 128:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 128 DataDetails");
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
        break;
        case 129:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 129 DataDetails");
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_PORTNO, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
        break;
        case 130:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 130 DataDetails");
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_PORTNO, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
        break;
        case 131:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 131 DataDetails");
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_PORTNO, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
        break;
        case 144:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 144 DataDetails");
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_OtherLocalChanges, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_CAPABILITYMASK, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LinkSpeecEnabledChange, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LinkWidthEnabledChange, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_NodeDescriptionChange, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        break;
        case 145:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 145 DataDetails");
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_SYSTEMIMAGEGUID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
            local_offset += 8;
        break;
        case 256:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 256 DataDetails");
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_DRSLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_METHOD, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_ATTRIBUTEID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_ATTRIBUTEMODIFIER, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_MKEY, tvb, local_offset, 8, ENC_BIG_ENDIAN);
            local_offset += 8;
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_DRNotice, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_DRPathTruncated, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_DRHopCount, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_DRNoticeReturnPath, tvb, local_offset, 30, ENC_NA);
            local_offset += 30;
        break;
        case 257:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 257 DataDetails");
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR1, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR2, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_KEY, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_SL, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_QP1, tvb, local_offset, 3, ENC_BIG_ENDIAN);
            local_offset += 3;
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_QP2, tvb, local_offset, 3, ENC_BIG_ENDIAN);
            local_offset += 3;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR1, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR2, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
        break;
        case 258:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 258 DataDetails");
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR1, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR2, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_KEY, tvb, local_offset, 4, ENC_BIG_ENDIAN);
            local_offset += 4;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_SL, tvb, local_offset, 1, ENC_BIG_ENDIAN);  local_offset  += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_QP1, tvb, local_offset, 3, ENC_BIG_ENDIAN);
            local_offset += 3;
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_QP2, tvb, local_offset, 3, ENC_BIG_ENDIAN);
            local_offset += 3;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR1, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR2, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
        break;
        case 259:
            proto_item_set_text(DataDetails_header_item, "%s", "Trap 259 DataDetails");
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_DataValid, tvb, local_offset, 2, ENC_NA);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR1, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_LIDADDR2, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_PKEY, tvb, local_offset, 2, ENC_BIG_ENDIAN);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_SL, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_QP1, tvb, local_offset, 3, ENC_BIG_ENDIAN);
            local_offset += 3;
            local_offset += 1;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_QP2, tvb, local_offset, 3, ENC_BIG_ENDIAN);
            local_offset += 3;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR1, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_GIDADDR2, tvb, local_offset, 16, ENC_NA);
            local_offset += 16;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_SWLIDADDR, tvb, local_offset, 2, ENC_NA);
            local_offset += 2;
            proto_tree_add_item(DataDetails_header_tree, hf_infiniband_Trap_PORTNO, tvb, local_offset, 1, ENC_BIG_ENDIAN);
            local_offset += 1;
        break;
        default:
            proto_item_set_text(DataDetails_header_item, "%s", "Vendor Specific Subnet Management Trap");
            local_offset += 54;
            break;
    }

    return local_offset;
}

/* Parse NoticesAndTraps Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_NoticesAndTraps(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *NoticesAndTraps_header_item;
    proto_tree *NoticesAndTraps_header_tree;
    uint16_t    trapNumber   = tvb_get_ntohs(tvb, local_offset + 4);

    if (!parentTree)
        return;

    NoticesAndTraps_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(NoticesAndTraps_header_item, "%s", val_to_str(trapNumber, Trap_Description, "Unknown or Vendor Specific Trap Number! (0x%02x)"));
    NoticesAndTraps_header_tree = proto_item_add_subtree(NoticesAndTraps_header_item, ett_noticestraps);

    proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_IsGeneric, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_Type, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_ProducerTypeVendorID, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_TrapNumberDeviceID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_IssuerLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_NoticeToggle, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_NoticeCount, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;

    parse_NoticeDataDetails(NoticesAndTraps_header_tree, tvb, &local_offset, trapNumber);
    proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_DataDetails, tvb, local_offset, 54, ENC_NA);
    local_offset += 54;

#if 0    /* Only Defined For GMPs not SMPs which is not part of this dissector phase */
    *proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_IssuerGID, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    *proto_tree_add_item(NoticesAndTraps_header_tree, hf_infiniband_Notice_ClassTrapSpecificData, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
#endif

}

/* Parse NodeDescription Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_NodeDescription(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_tree *NodeDescription_header_tree;

    if (!parentTree)
        return;

    NodeDescription_header_tree = parentTree;
    proto_tree_add_item(NodeDescription_header_tree, hf_infiniband_NodeDescription_NodeString, tvb, local_offset, 64, ENC_ASCII);
}

/* Parse NodeInfo Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_NodeInfo(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_tree *NodeInfo_header_tree;

    if (!parentTree)
        return *offset;

    NodeInfo_header_tree = parentTree;

    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_BaseVersion, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_ClassVersion, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_NodeType, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_NumPorts, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_SystemImageGUID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_NodeGUID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_PortGUID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_PartitionCap, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_DeviceID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_Revision, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_LocalPortNum, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(NodeInfo_header_tree, hf_infiniband_NodeInfo_VendorID, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;

    return local_offset;

}

/* Parse SwitchInfo Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_SwitchInfo(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_tree *SwitchInfo_header_tree;

    if (!parentTree)
        return *offset;

    SwitchInfo_header_tree = parentTree;

    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_LinearFDBCap, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_RandomFDBCap, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_MulticastFDBCap, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_LinearFDBTop, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_DefaultPort, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_DefaultMulticastPrimaryPort, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_DefaultMulticastNotPrimaryPort, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_LifeTimeValue, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_PortStateChange, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_OptimizedSLtoVLMappingProgramming, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_LIDsPerPort, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_PartitionEnforcementCap, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_InboundEnforcementCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_OutboundEnforcementCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_FilterRawInboundCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_FilterRawOutboundCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(SwitchInfo_header_tree, hf_infiniband_SwitchInfo_EnhancedPortZero, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    return local_offset;
}

/* Parse GUIDInfo Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_GUIDInfo(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_tree *GUIDInfo_header_tree;
    proto_item *tempItemLow;
    int         i;

    if (!parentTree)
        return *offset;

    GUIDInfo_header_tree = parentTree;

    for (i = 0; i < 8; i++)
    {
        tempItemLow = proto_tree_add_item(GUIDInfo_header_tree, hf_infiniband_GUIDInfo_GUID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
        proto_item_append_text(tempItemLow, "(%u)", i);
    }
    return local_offset;
}

/* Parse PortInfo Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_PortInfo(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_tree *PortInfo_header_tree;
    proto_item *PortInfo_CapabilityMask_item;
    proto_tree *PortInfo_CapabilityMask_tree;
    proto_item *temp_item;
    uint16_t    temp_val;

    if (!parentTree)
        return *offset;

    PortInfo_header_tree = parentTree;

    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_M_Key, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_GidPrefix, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_MasterSMLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;

    /* Capability Mask Flags */
    PortInfo_CapabilityMask_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_CapabilityMask, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    PortInfo_CapabilityMask_tree = proto_item_add_subtree(PortInfo_CapabilityMask_item, ett_portinfo_capmask);

    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_SM, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_NoticeSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_TrapSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_OptionalIPDSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_AutomaticMigrationSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_SLMappingSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_MKeyNVRAM, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_PKeyNVRAM, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_LEDInfoSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_SMdisabled, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_SystemImageGUIDSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_PKeySwitchExternalPortTrapSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_CommunicationManagementSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_SNMPTunnelingSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_ReinitSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_DeviceManagementSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_VendorClassSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_DRNoticeSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_CapabilityMaskNoticeSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_BootManagementSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_LinkRoundTripLatencySupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_ClientRegistrationSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_OtherLocalChangesNoticeSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_CapabilityMask_tree, hf_infiniband_PortInfo_CapabilityMask_LinkSpeedWIdthPairsTableSupported, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    /* End Capability Mask Flags */

    /* Diag Code */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_DiagCode, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    temp_val = tvb_get_ntohs(tvb, local_offset);

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, DiagCode, "Reserved DiagCode! Possible Error"));
    local_offset += 2;
    /* End Diag Code */

    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_M_KeyLeasePeriod, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LocalPortNum, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    /* LinkWidthEnabled */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LinkWidthEnabled, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, LinkWidthEnabled, "Reserved LinkWidthEnabled Value! Possible Error"));
    local_offset += 1;
    /* End LinkWidthEnabled */

    /* LinkWidthSupported */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LinkWidthSupported, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, LinkWidthSupported, "Reserved LinkWidthSupported Value! Possible Error"));
    local_offset += 1;
    /* End LinkWidthSupported */

    /* LinkWidthActive */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LinkWidthActive, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, LinkWidthActive, "Reserved LinkWidthActive Value! Possible Error"));
    local_offset += 1;
    /* End LinkWidthActive */

    /* LinkSpeedSupported */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LinkSpeedSupported, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x00F0;
    temp_val = temp_val >> 4;

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, LinkSpeedSupported, "Reserved LinkWidthSupported Value! Possible Error"));
    /* End LinkSpeedSupported */

    /* PortState */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_PortState, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x000F;
    /*temp_val = temp_val >> 4 */

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, PortState, "Reserved PortState Value! Possible Error"));
    local_offset += 1;
    /* End PortState */

    /* PortPhysicalState */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_PortPhysicalState, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x00F0;
    temp_val = temp_val >> 4;

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, PortPhysicalState, "Reserved PortPhysicalState Value! Possible Error"));
    /* End PortPhysicalState */

    /* LinkDownDefaultState */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LinkDownDefaultState, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x000F;
    /*temp_val = temp_val >> 4 */

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, LinkDownDefaultState, "Reserved LinkDownDefaultState Value! Possible Error"));
    local_offset += 1;
    /* End LinkDownDefaultState */

    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_M_KeyProtectBits, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LMC, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    /* LinkSpeedActive */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LinkSpeedActive, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x00F0;
    temp_val = temp_val >> 4;

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, LinkSpeedActive, "Reserved LinkSpeedActive Value! Possible Error"));
    /* End LinkSpeedActive */

    /* LinkSpeedEnabled */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LinkSpeedEnabled, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x000F;
    /*temp_val = temp_val >> 4 */

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, LinkSpeedEnabled, "Reserved LinkSpeedEnabled Value! Possible Error"));
    local_offset += 1;
    /* End LinkSpeedEnabled */

    /* NeighborMTU */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_NeighborMTU, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x00F0;
    temp_val = temp_val >> 4;

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, NeighborMTU, "Reserved NeighborMTU Value! Possible Error"));

    /* End NeighborMTU */

    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_MasterSMSL, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    /* VLCap */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_VLCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x00F0;
    temp_val = temp_val >> 4;

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, VLCap, "Reserved VLCap Value! Possible Error"));

    /* End VLCap */

    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_InitType, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_VLHighLimit, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_VLArbitrationHighCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_VLArbitrationLowCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_InitTypeReply, tvb, local_offset, 1, ENC_BIG_ENDIAN);

    /* MTUCap */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_MTUCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x000F;
    /*temp_val = temp_val >> 4 */

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, MTUCap, "Reserved MTUCap Value! Possible Error"));
    local_offset += 1;
    /* End MTUCap */

    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_VLStallCount, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_HOQLife, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    /* OperationalVLs */
    temp_item = proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_OperationalVLs, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    temp_val = (uint16_t)tvb_get_uint8(tvb, local_offset);

    /* 4 bit values = mask and shift */
    temp_val = temp_val & 0x00F0;
    temp_val = temp_val >> 4;

    proto_item_append_text(temp_item, ", %s", val_to_str_const(temp_val, OperationalVLs, "Reserved OperationalVLs Value! Possible Error"));
    /* End OperationalVLs */

    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_PartitionEnforcementInbound, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_PartitionEnforcementOutbound, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_FilterRawInbound, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_FilterRawOutbound, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_M_KeyViolations, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_P_KeyViolations, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_Q_KeyViolations, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_GUIDCap, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_ClientReregister, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_SubnetTimeOut, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_RespTimeValue, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LocalPhyErrors, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_OverrunErrors, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_MaxCreditHint, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 3; /* 2 + 1 Reserved */
    proto_tree_add_item(PortInfo_header_tree, hf_infiniband_PortInfo_LinkRoundTripLatency, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;

    return local_offset;
}

/* Parse P_KeyTable Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_P_KeyTable(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    int         i;
    proto_item *P_KeyTable_header_item;
    proto_tree *P_KeyTable_header_tree;
    proto_item *tempItemLow;
    proto_item *tempItemHigh;

    if (!parentTree)
        return;

    P_KeyTable_header_item = proto_tree_add_item(parentTree, hf_infiniband_P_KeyTable_P_KeyTableBlock, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(P_KeyTable_header_item, "%s", "P_KeyTable");
    P_KeyTable_header_tree = proto_item_add_subtree(P_KeyTable_header_item, ett_pkeytable);

    for (i = 0; i < 32; i++)
    {
        tempItemLow = proto_tree_add_item(P_KeyTable_header_tree, hf_infiniband_P_KeyTable_MembershipType, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        tempItemHigh = proto_tree_add_item(P_KeyTable_header_tree, hf_infiniband_P_KeyTable_P_KeyBase, tvb, local_offset, 2, ENC_BIG_ENDIAN);
        local_offset += 2;
        proto_item_append_text(tempItemLow,  "(%u)", i);
        proto_item_append_text(tempItemHigh, "(%u)", i+1);
    }
}

/* Parse SLtoVLMappingTable Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_SLtoVLMappingTable(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *SLtoVLMappingTable_header_item;
    proto_tree *SLtoVLMappingTable_header_tree;
    proto_item *tempItemLow;
    proto_item *tempItemHigh;
    int         i;

    if (!parentTree)
        return;

    SLtoVLMappingTable_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(SLtoVLMappingTable_header_item, "%s", "SLtoVLMappingTable");
    SLtoVLMappingTable_header_tree = proto_item_add_subtree(SLtoVLMappingTable_header_item, ett_sltovlmapping);

    for (i = 0; i < 8; i++)
    {
        tempItemLow = proto_tree_add_item(SLtoVLMappingTable_header_tree, hf_infiniband_SLtoVLMappingTable_SLtoVL_HighBits, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        tempItemHigh = proto_tree_add_item(SLtoVLMappingTable_header_tree, hf_infiniband_SLtoVLMappingTable_SLtoVL_LowBits, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        local_offset += 1;
        proto_item_append_text(tempItemLow,  "(%u)", i);
        proto_item_append_text(tempItemHigh, "(%u)", i+1);
    }
}

/* Parse VLArbitrationTable Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_VLArbitrationTable(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    int         i;
    proto_item *VLArbitrationTable_header_item;
    proto_tree *VLArbitrationTable_header_tree;
    proto_item *tempItemLow;
    proto_item *tempItemHigh;

    if (!parentTree)
        return;

    VLArbitrationTable_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(VLArbitrationTable_header_item, "%s", "VLArbitrationTable");
    VLArbitrationTable_header_tree = proto_item_add_subtree(VLArbitrationTable_header_item, ett_vlarbitrationtable);

    for (i = 0; i < 32; i++)
    {
        tempItemLow = proto_tree_add_item(VLArbitrationTable_header_tree, hf_infiniband_VLArbitrationTable_VL, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        local_offset += 1;
        tempItemHigh = proto_tree_add_item(VLArbitrationTable_header_tree, hf_infiniband_VLArbitrationTable_Weight, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        local_offset += 1;
        proto_item_append_text(tempItemLow,  "(%u)", i);
        proto_item_append_text(tempItemHigh, "(%u)", i);
    }
}

/* Parse LinearForwardingTable Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_LinearForwardingTable(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         i;
    int         local_offset = *offset;
    proto_item *LinearForwardingTable_header_item;
    proto_tree *LinearForwardingTable_header_tree;
    proto_item *tempItemLow;

    if (!parentTree)
        return;

    LinearForwardingTable_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(LinearForwardingTable_header_item, "%s", "LinearForwardingTable");
    LinearForwardingTable_header_tree = proto_item_add_subtree(LinearForwardingTable_header_item, ett_linearforwardingtable);

    for (i = 0; i < 64; i++)
    {
        tempItemLow = proto_tree_add_item(LinearForwardingTable_header_tree, hf_infiniband_LinearForwardingTable_Port, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        local_offset += 1;
        proto_item_append_text(tempItemLow, "(%u)", i);
    }
}

/* Parse RandomForwardingTable Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_RandomForwardingTable(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         i;
    int         local_offset = *offset;
    proto_item *RandomForwardingTable_header_item;
    proto_tree *RandomForwardingTable_header_tree;
    proto_item *tempItemLow;

    if (!parentTree)
        return;

    RandomForwardingTable_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(RandomForwardingTable_header_item, "%s", "RandomForwardingTable");
    RandomForwardingTable_header_tree = proto_item_add_subtree(RandomForwardingTable_header_item, ett_randomforwardingtable);

    for (i = 0; i < 16; i++)
    {
        tempItemLow = proto_tree_add_item(RandomForwardingTable_header_tree, hf_infiniband_RandomForwardingTable_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
        local_offset += 2;
        proto_item_append_text(tempItemLow, "(%u)", i);
        tempItemLow = proto_tree_add_item(RandomForwardingTable_header_tree, hf_infiniband_RandomForwardingTable_Valid, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        proto_item_append_text(tempItemLow, "(%u)", i);
        tempItemLow = proto_tree_add_item(RandomForwardingTable_header_tree, hf_infiniband_RandomForwardingTable_LMC, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        local_offset += 1;
        proto_item_append_text(tempItemLow, "(%u)", i);
        tempItemLow = proto_tree_add_item(RandomForwardingTable_header_tree, hf_infiniband_RandomForwardingTable_Port, tvb, local_offset, 1, ENC_BIG_ENDIAN);
        local_offset += 1;
        proto_item_append_text(tempItemLow, "(%u)", i);
    }
}

/* Parse NoticesAndTraps Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_MulticastForwardingTable(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         i;
    int         local_offset = *offset;
    proto_item *MulticastForwardingTable_header_item;
    proto_tree *MulticastForwardingTable_header_tree;
    proto_item *tempItemLow;

    if (!parentTree)
        return;

    MulticastForwardingTable_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(MulticastForwardingTable_header_item, "%s", "MulticastForwardingTable");
    MulticastForwardingTable_header_tree = proto_item_add_subtree(MulticastForwardingTable_header_item, ett_multicastforwardingtable);

    for (i = 0; i < 16; i++)
    {
        tempItemLow = proto_tree_add_item(MulticastForwardingTable_header_tree, hf_infiniband_MulticastForwardingTable_PortMask, tvb, local_offset, 2, ENC_BIG_ENDIAN);
        local_offset += 2;
        proto_item_append_text(tempItemLow, "(%u)", i);
    }

}

/* Parse SMInfo Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_SMInfo(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *SMInfo_header_item;
    proto_tree *SMInfo_header_tree;

    if (!parentTree)
        return *offset;

    SMInfo_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(SMInfo_header_item, "%s", "SMInfo");
    SMInfo_header_tree = proto_item_add_subtree(SMInfo_header_item, ett_sminfo);

    proto_tree_add_item(SMInfo_header_tree, hf_infiniband_SMInfo_GUID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(SMInfo_header_tree, hf_infiniband_SMInfo_SM_Key, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(SMInfo_header_tree, hf_infiniband_SMInfo_ActCount, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(SMInfo_header_tree, hf_infiniband_SMInfo_Priority, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(SMInfo_header_tree, hf_infiniband_SMInfo_SMState, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    return local_offset;
}

/* Parse VendorDiag Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_VendorDiag(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *VendorDiag_header_item;
    proto_tree *VendorDiag_header_tree;

    if (!parentTree)
        return *offset;

    VendorDiag_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(VendorDiag_header_item, "%s", "VendorDiag");
    VendorDiag_header_tree = proto_item_add_subtree(VendorDiag_header_item, ett_vendordiag);

    proto_tree_add_item(VendorDiag_header_tree, hf_infiniband_VendorDiag_NextIndex, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(VendorDiag_header_tree, hf_infiniband_VendorDiag_DiagData, tvb, local_offset, 62, ENC_NA);
    local_offset += 62;

    return local_offset;
}

/* Parse LedInfo Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static void parse_LedInfo(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *LedInfo_header_item;
    proto_tree *LedInfo_header_tree;

    if (!parentTree)
        return;

    LedInfo_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(LedInfo_header_item, "%s", "LedInfo");
    LedInfo_header_tree = proto_item_add_subtree(LedInfo_header_item, ett_ledinfo);

    proto_tree_add_item(LedInfo_header_tree, hf_infiniband_LedInfo_LedMask, tvb, local_offset, 1, ENC_BIG_ENDIAN);
}

/* Parse LinkSpeedWidthPairsTable Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_LinkSpeedWidthPairsTable(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *LinkSpeedWidthPairsTable_header_item;
    proto_tree *LinkSpeedWidthPairsTable_header_tree;

    if (!parentTree)
        return *offset;

    LinkSpeedWidthPairsTable_header_item = proto_tree_add_item(parentTree, hf_infiniband_smp_data, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(LinkSpeedWidthPairsTable_header_item, "%s", "LinkSpeedWidthPairsTable");
    LinkSpeedWidthPairsTable_header_tree = proto_item_add_subtree(LinkSpeedWidthPairsTable_header_item, ett_linkspeedwidthpairs);

    proto_tree_add_item(LinkSpeedWidthPairsTable_header_tree, hf_infiniband_LinkSpeedWidthPairsTable_NumTables, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(LinkSpeedWidthPairsTable_header_tree, hf_infiniband_LinkSpeedWidthPairsTable_PortMask, tvb, local_offset, 32, ENC_NA);
    local_offset += 32;
    proto_tree_add_item(LinkSpeedWidthPairsTable_header_tree, hf_infiniband_LinkSpeedWidthPairsTable_SpeedTwoFive, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(LinkSpeedWidthPairsTable_header_tree, hf_infiniband_LinkSpeedWidthPairsTable_SpeedFive, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(LinkSpeedWidthPairsTable_header_tree, hf_infiniband_LinkSpeedWidthPairsTable_SpeedTen, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

   return local_offset;
}

/* Parse RID Field from Subnet Administration Packets.
* IN: SA_header_tree - the dissection tree of the subnet admin attribute.
*     tvb - the packet buffer
*     MadHeader - the Common MAD header from this packet.
* IN/OUT:  offset - the current and updated offset in the packet buffer */
static void parse_RID(proto_tree* SA_header_tree, tvbuff_t* tvb, int *offset, MAD_Data* MadHeader)
{
    int local_offset = *offset;

    if (!SA_header_tree)
    {
        return;
    }
        switch (MadHeader->attributeID)
        {
            case 0x0011:
                /* NodeRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 2, ENC_NA);
                local_offset += 2;
                break;
            case 0x0012:
                /* PortInfoRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_EndportLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_PortNum, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                local_offset += 1;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
                local_offset += 1;
                break;
            case 0x0013:
                /* SLtoVLMappingTableRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_InputPortNum, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                local_offset += 1;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_OutputPortNum, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                local_offset += 1;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
                local_offset += 4;
                break;
            case 0x0014:
                /* SwitchInfoRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 2, ENC_NA);
                local_offset += 2;
                break;
            case 0x0015:
                /* LinearForwardingTableRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_BlockNum_SixteenBit, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
                local_offset += 4;
                break;
            case 0x0016:
                /* RandomForwardingTableRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_BlockNum_SixteenBit, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
                local_offset += 4;
                break;
            case 0x0017:
                /* MulticastForwardingTableRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_Position, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_BlockNum_NineBit, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
                local_offset += 4;
                break;
            case 0x0036:
                /* VLArbitrationTableRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_OutputPortNum, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                local_offset += 1;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_BlockNum_EightBit, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                local_offset += 1;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
                local_offset += 4;
                break;
            case 0x0018:
                /* SMInfoRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 2, ENC_NA);
                local_offset += 2;
                break;
            case 0x0033:
                /* P_KeyTableRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_BlockNum_SixteenBit, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_PortNum, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                local_offset += 1;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 3, ENC_NA);
                local_offset += 3;
                break;
            case 0x00F3:
                /* InformInfoRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_InformInfoRecord_SubscriberGID, tvb, local_offset, 16, ENC_NA);
                local_offset += 16;
                proto_tree_add_item(SA_header_tree, hf_infiniband_InformInfoRecord_Enum, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 6, ENC_NA);
                local_offset += 6;
                break;
            case 0x0020:
                /* LinkRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_LinkRecord_FromLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_LinkRecord_FromPort, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                local_offset += 1;
                break;
            case 0x0031:
                /* ServiceRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_ServiceRecord_ServiceID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
                local_offset += 8;
                proto_tree_add_item(SA_header_tree, hf_infiniband_ServiceRecord_ServiceGID, tvb, local_offset, 16, ENC_NA);
                local_offset += 16;
                proto_tree_add_item(SA_header_tree, hf_infiniband_ServiceRecord_ServiceP_Key, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                local_offset += 2;
                break;
            case 0x0038:
                /* MCMemberRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_MCMemberRecord_MGID, tvb, local_offset, 16, ENC_NA);
                local_offset += 16;
                proto_tree_add_item(SA_header_tree, hf_infiniband_MCMemberRecord_PortGID, tvb, local_offset, 16, ENC_NA);
                local_offset += 16;
                break;
            case 0x0030:
                /* GuidInfoRecord */
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_LID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_SA_BlockNum_EightBit, tvb, local_offset, 1, ENC_BIG_ENDIAN);
                local_offset += 2;
                proto_tree_add_item(SA_header_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
                local_offset += 4;
                break;
            default:
                break;
        }

    *offset = local_offset;
}

/* Parse InformInfo Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_InformInfo(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *InformInfo_header_item;
    proto_tree *InformInfo_header_tree;

    if (!parentTree)
    {
        return *offset;
    }
    InformInfo_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset, 36, ENC_NA);
    proto_item_set_text(InformInfo_header_item, "%s", "InformInfo");
    InformInfo_header_tree = proto_item_add_subtree(InformInfo_header_item, ett_informinfo);

    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_GID, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_LIDRangeBegin, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_LIDRangeEnd, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_reserved, tvb, local_offset, 2, ENC_NA);
    local_offset += 2;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_IsGeneric, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_Subscribe, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_Type, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_TrapNumberDeviceID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_QPN, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_RespTimeValue, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(InformInfo_header_tree, hf_infiniband_InformInfo_ProducerTypeVendorID, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;

   return local_offset;
}
/* Parse LinkRecord Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_LinkRecord(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *LinkRecord_header_item;
    proto_tree *LinkRecord_header_tree;

    if (!parentTree)
    {
        return *offset;
    }

    LinkRecord_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset, 3, ENC_NA);
    proto_item_set_text(LinkRecord_header_item, "%s", "LinkRecord");
    LinkRecord_header_tree = proto_item_add_subtree(LinkRecord_header_item, ett_linkrecord);

    proto_tree_add_item(LinkRecord_header_tree, hf_infiniband_LinkRecord_ToPort, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(LinkRecord_header_tree, hf_infiniband_LinkRecord_ToLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;

   return local_offset;

}
/* Parse ServiceRecord Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_ServiceRecord(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *ServiceRecord_header_item;
    proto_tree *ServiceRecord_header_tree;
    proto_item *tempData;

    if (!parentTree)
    {
        return *offset;
    }

    ServiceRecord_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset, 176, ENC_NA);
    proto_item_set_text(ServiceRecord_header_item, "%s", "ServiceRecord");
    ServiceRecord_header_tree = proto_item_add_subtree(ServiceRecord_header_item, ett_servicerecord);

    proto_tree_add_item(ServiceRecord_header_tree, hf_infiniband_ServiceRecord_ServiceLease, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(ServiceRecord_header_tree, hf_infiniband_ServiceRecord_ServiceKey, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(ServiceRecord_header_tree, hf_infiniband_ServiceRecord_ServiceName, tvb, local_offset, 64, ENC_NA);
    local_offset += 64;

    tempData = proto_tree_add_item(ServiceRecord_header_tree, hf_infiniband_ServiceRecord_ServiceData, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_item_append_text(tempData, "%s", "(ServiceData 8.1, 8.16)");
    tempData = proto_tree_add_item(ServiceRecord_header_tree, hf_infiniband_ServiceRecord_ServiceData, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_item_append_text(tempData, "%s", "(ServiceData 16.1, 16.8)");
    tempData = proto_tree_add_item(ServiceRecord_header_tree, hf_infiniband_ServiceRecord_ServiceData, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_item_append_text(tempData, "%s", "(ServiceData 32.1, 32.4)");
    tempData = proto_tree_add_item(ServiceRecord_header_tree, hf_infiniband_ServiceRecord_ServiceData, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_item_append_text(tempData, "%s", "(ServiceData 64.1, 64.2)");

    return local_offset;

}
/* Parse PathRecord Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_PathRecord(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *PathRecord_header_item;
    proto_tree *PathRecord_header_tree;

    if (!parentTree)
    {
        return *offset;
    }

    PathRecord_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(PathRecord_header_item, "%s", "PathRecord");
    PathRecord_header_tree = proto_item_add_subtree(PathRecord_header_item, ett_pathrecord);
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_reserved, tvb, local_offset, 8, ENC_NA);
    local_offset += 8;

    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_DGID, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_SGID, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_DLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_SLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_RawTraffic, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_FlowLabel, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_HopLimit, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_TClass, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_Reversible, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_NumbPath, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_P_Key, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_SL, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_MTUSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_MTU, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_RateSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_Rate, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_PacketLifeTimeSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_PacketLifeTime, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(PathRecord_header_tree, hf_infiniband_PathRecord_Preference, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    return local_offset;
}
/* Parse MCMemberRecord Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins   */
static int parse_MCMemberRecord(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *MCMemberRecord_header_item;
    proto_tree *MCMemberRecord_header_tree;

    if (!parentTree)
    {
        return *offset;
    }

    MCMemberRecord_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset, 64, ENC_NA);
    proto_item_set_text(MCMemberRecord_header_item, "%s", "MCMemberRecord");
    MCMemberRecord_header_tree = proto_item_add_subtree(MCMemberRecord_header_item, ett_mcmemberrecord);

    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_Q_Key, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_MLID, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_MTUSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_MTU, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_TClass, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_P_Key, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_RateSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_Rate, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_PacketLifeTimeSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_PacketLifeTime, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_SL, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_FlowLabel, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_HopLimit, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_Scope, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_JoinState, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MCMemberRecord_header_tree, hf_infiniband_MCMemberRecord_ProxyJoin, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 3;

    return local_offset;

}
/* Parse TraceRecord Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_TraceRecord(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *TraceRecord_header_item;
    proto_tree *TraceRecord_header_tree;

    if (!parentTree)
    {
        return *offset;
    }

    TraceRecord_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset, 46, ENC_NA);
    proto_item_set_text(TraceRecord_header_item, "%s", "TraceRecord");
    TraceRecord_header_tree = proto_item_add_subtree(TraceRecord_header_item, ett_tracerecord);

    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_GIDPrefix, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_IDGeneration, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_NodeType, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_NodeID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_ChassisID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_EntryPortID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_ExitPortID, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_EntryPort, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(TraceRecord_header_tree, hf_infiniband_TraceRecord_ExitPort, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    return local_offset;
}
/* Parse MultiPathRecord Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_MultiPathRecord(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *MultiPathRecord_header_item;
    proto_tree *MultiPathRecord_header_tree;
    proto_item *SDGID;
    uint8_t     SDGIDCount;
    uint8_t     DGIDCount;
    uint32_t    i;

    if (!parentTree)
    {
        return *offset;
    }

    MultiPathRecord_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset, 200, ENC_NA);
    proto_item_set_text(MultiPathRecord_header_item, "%s", "MultiPathRecord");
    MultiPathRecord_header_tree = proto_item_add_subtree(MultiPathRecord_header_item, ett_multipathrecord);

    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_RawTraffic, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_FlowLabel, tvb, local_offset, 3, ENC_BIG_ENDIAN);
    local_offset += 3;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_HopLimit, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_TClass, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_Reversible, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_NumbPath, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_P_Key, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_SL, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_MTUSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_MTU, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_RateSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_Rate, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_PacketLifeTimeSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_PacketLifeTime, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_IndependenceSelector, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_GIDScope, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;

    SDGIDCount = tvb_get_uint8(tvb, local_offset);
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_SGIDCount, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    DGIDCount = tvb_get_uint8(tvb, local_offset);
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_DGIDCount, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_reserved, tvb, local_offset, 7, ENC_NA);
    local_offset += 7;

    for (i = 0; i < SDGIDCount; i++)
    {
        SDGID = proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_SDGID, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
        proto_item_set_text(SDGID, "(%s%u)", "SGID", i);
    }
    for (i = 0; i < DGIDCount; i++)
    {
        SDGID = proto_tree_add_item(MultiPathRecord_header_tree, hf_infiniband_MultiPathRecord_SDGID, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
        proto_item_set_text(SDGID, "(%s%u)", "DGID", i);
    }

    return local_offset;
}
/* Parse ServiceAssociationRecord Attribute
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins     */
static int parse_ServiceAssociationRecord(proto_tree* parentTree, tvbuff_t* tvb, int *offset)
{
    int         local_offset = *offset;
    proto_item *ServiceAssociationRecord_header_item;
    proto_tree *ServiceAssociationRecord_header_tree;

    if (!parentTree)
    {
        return *offset;
    }

    ServiceAssociationRecord_header_item = proto_tree_add_item(parentTree, hf_infiniband_SA, tvb, local_offset, 80, ENC_NA);
    proto_item_set_text(ServiceAssociationRecord_header_item, "%s", "ServiceAssociationRecord");
    ServiceAssociationRecord_header_tree = proto_item_add_subtree(ServiceAssociationRecord_header_item, ett_serviceassocrecord);

    proto_tree_add_item(ServiceAssociationRecord_header_tree, hf_infiniband_ServiceAssociationRecord_ServiceKey, tvb, local_offset, 16, ENC_NA);
    local_offset += 16;
    proto_tree_add_item(ServiceAssociationRecord_header_tree, hf_infiniband_ServiceAssociationRecord_ServiceName, tvb, local_offset, 64, ENC_ASCII);
    local_offset += 64;

    return local_offset;
}

/* Parse PortCounters MAD from the Performance management class.
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins
*       pinfo - The packet info structure with column information  */
static int parse_PERF_PortCounters(proto_tree* parentTree, tvbuff_t* tvb, packet_info *pinfo, int *offset)
{
    proto_item *perf_item;
    proto_tree *perf_tree;
    int         local_offset = *offset;

    col_set_str(pinfo->cinfo, COL_INFO, "PERF (PortCounters)");

    perf_item = proto_tree_add_item(parentTree, hf_infiniband_PortCounters, tvb, local_offset, 40, ENC_NA);
    perf_tree = proto_item_add_subtree(perf_item, ett_perfclass);

    proto_tree_add_item(perf_tree, hf_infiniband_reserved, tvb, local_offset, 40, ENC_NA);
    local_offset += 40;
    proto_tree_add_item(perf_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortSelect, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_CounterSelect, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_SymbolErrorCounter, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_LinkErrorRecoveryCounter, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_LinkDownedCounter, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortRcvErrors, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortRcvRemotePhysicalErrors, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortRcvSwitchRelayErrors, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortXmitDiscards, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortXmitConstraintErrors, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortRcvConstraintErrors, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_bits_item(perf_tree, hf_infiniband_PortCounters_LocalLinkIntegrityErrors, tvb, local_offset*8, 4, ENC_BIG_ENDIAN);
    proto_tree_add_bits_item(perf_tree, hf_infiniband_PortCounters_ExcessiveBufferOverrunErrors, tvb, local_offset*8 + 4, 4, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_reserved, tvb, local_offset, 2, ENC_NA);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_VL15Dropped, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortXmitData, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortRcvData, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortXmitPkts, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCounters_PortRcvPkts, tvb, local_offset, 4, ENC_BIG_ENDIAN);
    local_offset += 4;

    *offset = local_offset; /* update caller's offset to point to end of the PortCounters payload */
    return local_offset;
}

/* Parse PortCountersExtended MAD from the Performance management class.
* IN:   parentTree - The tree to add the dissection to
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins
*       pinfo - The packet info structure with column information  */
static int parse_PERF_PortCountersExtended(proto_tree* parentTree, tvbuff_t* tvb, packet_info *pinfo, int *offset)
{
    proto_item *perf_item;
    proto_tree *perf_tree;
    int         local_offset = *offset;

    col_set_str(pinfo->cinfo, COL_INFO, "PERF (PortCountersExtended)");

    perf_item = proto_tree_add_item(parentTree, hf_infiniband_PortCountersExt, tvb, local_offset, 72, ENC_NA);
    perf_tree = proto_item_add_subtree(perf_item, ett_perfclass);

    proto_tree_add_item(perf_tree, hf_infiniband_reserved, tvb, local_offset, 40, ENC_NA);
    local_offset += 40;
    proto_tree_add_item(perf_tree, hf_infiniband_reserved, tvb, local_offset, 1, ENC_NA);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortSelect, tvb, local_offset, 1, ENC_BIG_ENDIAN);
    local_offset += 1;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_CounterSelect, tvb, local_offset, 2, ENC_BIG_ENDIAN);
    local_offset += 2;
    proto_tree_add_item(perf_tree, hf_infiniband_reserved, tvb, local_offset, 4, ENC_NA);
    local_offset += 4;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortXmitData, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortRcvData, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortXmitPkts, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortRcvPkts, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortUnicastXmitPkts, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortUnicastRcvPkts, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortMulticastXmitPkts, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;
    proto_tree_add_item(perf_tree, hf_infiniband_PortCountersExt_PortMulticastRcvPkts, tvb, local_offset, 8, ENC_BIG_ENDIAN);
    local_offset += 8;

    *offset = local_offset; /* update caller's offset to point to end of the PortCountersExt payload */
    return local_offset;
}

/* dissect_general_info
* Used to extract very few values from the packet in the case that full dissection is disabled by the user.
* IN:
*       tvb - The tvbbuff of packet data
*       offset - The offset in TVB where the attribute begins
*       pinfo - The packet info structure with column information
*       starts_with - regular IB packet starts with LRH, ROCE starts with GRH and RROCE starts with BTH,
*                     this tells the parser what headers of (LRH/GRH) to skip. */
static void dissect_general_info(tvbuff_t *tvb, int offset, packet_info *pinfo, ib_packet_start_header starts_with)
{
    uint8_t           lnh_val            = 0; /* The Link Next Header Value.  Tells us which headers are coming */
    bool              bthFollows         = false; /* Tracks if we are parsing a BTH.  This is a significant decision point */
    uint8_t           virtualLane        = 0; /* The Virtual Lane of the current Packet */
    int32_t           nextHeaderSequence = -1; /* defined by this dissector. #define which indicates the upcoming header sequence from OpCode */
    uint8_t           nxtHdr             = 0; /* that must be available for that header. */
    uint8_t           management_class   = 0;
    MAD_Data          MadData;

    /* BTH - Base Transport Header */
    struct infinibandinfo info = { NULL, 0, 0, 0, 0, 0, 0, 0, false, false};
    int bthSize = 12;
    void *src_addr,                 /* the address to be displayed in the source/destination columns */
         *dst_addr;                 /* (lid/gid number) will be stored here */

    if (starts_with == IB_PACKET_STARTS_WITH_GRH) {
        /* this is a RoCE packet, skip LRH parsing */
        lnh_val = IBA_GLOBAL;
        goto skip_lrh;
    }
    else if (starts_with == IB_PACKET_STARTS_WITH_BTH) {
        /* this is a RRoCE packet, skip LRH/GRH parsing and go directly to BTH */
        lnh_val = IBA_LOCAL;
        goto skip_lrh;
    }

    virtualLane =  tvb_get_uint8(tvb, offset);
    virtualLane = virtualLane & 0xF0;
    offset += 1;

    /* Save Link Next Header... This tells us what the next header is. */
    lnh_val =  tvb_get_uint8(tvb, offset);
    lnh_val = lnh_val & 0x03;
    offset += 1;

    /* Set destination in packet view. */
    dst_addr = wmem_alloc(pinfo->pool, sizeof(uint16_t));
    *((uint16_t*) dst_addr) = tvb_get_ntohs(tvb, offset);
    set_address(&pinfo->dst, AT_IB, sizeof(uint16_t), dst_addr);

    offset += 4;

    /* Set Source in packet view. */
    src_addr = wmem_alloc(pinfo->pool, sizeof(uint16_t));
    *((uint16_t*) src_addr) = tvb_get_ntohs(tvb, offset);
    set_address(&pinfo->src, AT_IB, sizeof(uint16_t), src_addr);

    offset += 2;

skip_lrh:

    switch (lnh_val)
    {
        case IBA_GLOBAL:
            offset += 6;
            nxtHdr = tvb_get_uint8(tvb, offset);
            offset += 2;

            /* Set source GID in packet view. */
            set_address_tvb(&pinfo->src, AT_IB, GID_SIZE, tvb, offset);
            offset += 16;

            /* Set destination GID in packet view. */
            set_address_tvb(&pinfo->dst, AT_IB, GID_SIZE, tvb, offset);
            offset += 16;

            if (nxtHdr != 0x1B)
            {
                /* Some kind of packet being transported globally with IBA, but locally it is not IBA - no BTH following. */
                break;
            }
            /* else
             * {
             *      Fall through switch and start parsing Local Headers and BTH
             * }
             */
        /* FALL THROUGH */
        case IBA_LOCAL:
            bthFollows = true;

            /* Get the OpCode - this tells us what headers are following */
            info.opCode = tvb_get_uint8(tvb, offset);
            if ((info.opCode >> 5) == 0x2) {
                info.dctConnect = !(tvb_get_uint8(tvb, offset + 1) & 0x80);
                bthSize += 8;
            }
            col_append_str(pinfo->cinfo, COL_INFO, val_to_str_const((uint32_t)info.opCode, OpCodeMap, "Unknown OpCode "));
            offset += bthSize;
            break;
        case IP_NON_IBA:
            /* Raw IPv6 Packet */
            dst_addr = wmem_strdup(pinfo->pool, "IPv6 over IB Packet");
            set_address(&pinfo->dst,  AT_STRINGZ, (int)strlen((char *)dst_addr)+1, dst_addr);
            break;
        case RAW:
            break;
        default:
            break;
    }

    if (bthFollows)
    {
        /* Find our next header sequence based on the Opcode
         * Since we're not doing dissection here, we just need the proper offsets to get our labels in packet view */

        nextHeaderSequence = find_next_header_sequence(&info);
        switch (nextHeaderSequence)
        {
            case RDETH_DETH_PAYLD:
                offset += 4; /* RDETH */
                offset += 8; /* DETH */
                break;
            case RETH_IMMDT_PAYLD:
                offset += 16; /* RETH */
                offset += 4; /* IMMDT */
                break;
            case RDETH_DETH_RETH_PAYLD:
                offset += 4; /* RDETH */
                offset += 8; /* DETH */
                offset += 16; /* RETH */
                break;
            case RDETH_DETH_IMMDT_PAYLD:
                offset += 4; /* RDETH */
                offset += 8; /* DETH */
                offset += 4; /* IMMDT */
                break;
            case RDETH_DETH_RETH_IMMDT_PAYLD:
                offset += 4; /* RDETH */
                offset += 8; /* DETH */
                offset += 16; /* RETH */
                offset += 4; /* IMMDT */
                break;
            case RDETH_DETH_RETH:
                offset += 4; /* RDETH */
                offset += 8; /* DETH */
                offset += 16; /* RETH */
                break;
            case RDETH_AETH_PAYLD:
                offset += 4; /* RDETH */
                offset += 4; /* AETH */
                break;
            case RDETH_PAYLD:
                offset += 4; /* RDETH */
                break;
            case RDETH_AETH:
                offset += 4; /* RDETH */
                offset += 4; /* AETH */
                break;
            case RDETH_AETH_ATOMICACKETH:
                offset += 4; /* RDETH */
                offset += 4; /* AETH */
                offset += 8; /* AtomicAckETH */
                break;
            case RDETH_DETH_ATOMICETH:
                offset += 4; /* RDETH */
                offset += 8; /* DETH */
                offset += 28; /* AtomicETH */
                break;
            case RDETH_DETH:
                offset += 4; /* RDETH */
                offset += 8; /* DETH */
                break;
            case DETH_PAYLD:
                offset += 8; /* DETH */
                break;
            case PAYLD:
                break;
            case IMMDT_PAYLD:
                offset += 4; /* IMMDT */
                break;
            case RETH_PAYLD:
                offset += 16; /* RETH */
                break;
            case RETH:
                offset += 16; /* RETH */
                break;
            case AETH_PAYLD:
                offset += 4; /* AETH */
                break;
            case AETH:
                offset += 4; /* AETH */
                break;
            case AETH_ATOMICACKETH:
                offset += 4; /* AETH */
                offset += 8; /* AtomicAckETH */
                break;
            case ATOMICETH:
                offset += 28; /* AtomicETH */
                break;
            case IETH_PAYLD:
                offset += 4; /* IETH */
                break;
            case DETH_IMMDT_PAYLD:
                offset += 8; /* DETH */
                offset += 4; /* IMMDT */
                break;
            case DCCETH:
                offset += 16; /* DCCETH */
                break;
            case FETH_RETH:
                offset += 4; /* FETH */
                offset += 16; /* RETH */
                break;
            case RDETH_FETH_RETH:
                offset += 4; /* RDETH */
                offset += 4; /* FETH */
                offset += 16; /* RETH */
                break;
            case  RDETH_RETH_PAYLD:
                offset += 4; /* RDETH */
                offset += 16; /* RETH */
            default:
                break;
        }
    }
    if (virtualLane == 0xF0)
    {
        management_class =  tvb_get_uint8(tvb, offset + 1);
        if (((management_class >= (uint8_t)VENDOR_1_START) && (management_class <= (uint8_t)VENDOR_1_END))
            || ((management_class >= (uint8_t)VENDOR_2_START) && (management_class <= (uint8_t)VENDOR_2_END)))
        {
            return;
        }
        else if ((management_class >= (uint8_t)APPLICATION_START) && (management_class <= (uint8_t)APPLICATION_END))
        {
            return;
        }
        else if (((management_class == (uint8_t)0x00) || (management_class == (uint8_t)0x02))
                 || ((management_class >= (uint8_t)0x50) && (management_class <= (uint8_t)0x80))
                 || ((management_class >= (uint8_t)0x82)))
        {
            return;
        }
        else /* we have a normal management_class */
        {
            if (parse_MAD_Common(NULL, tvb, &offset, &MadData)) {
                label_SUBM_Method(NULL, &MadData, pinfo);
                label_SUBM_Attribute(NULL, &MadData, pinfo);
            }
        }
    }

    return;
}

static void
infiniband_shutdown(void)
{
    g_hash_table_destroy(CM_context_table);
}

/* Protocol Registration */
void proto_register_infiniband(void)
{
    module_t *infiniband_module;

    /* Field dissector structures.
    * For reserved fields, reservedX denotes the reserved field is X bits in length.
    * e.g. reserved2 is a reserved field 2 bits in length.
    * The third parameter is a filter string associated for this field.
    * So for instance, to filter packets for a given virtual lane,
    * The filter (infiniband.LRH.vl == 3) or something similar would be used. */

    /* XXX: ToDo: Verify against Infiniband 1.2.1 Specification                           */
    /*            Fields verified/corrected: Those after comment "XX: All following ..."  */

    /* meanings for MAD method field */
    static const value_string mad_method_str[] = {
        { 0x01, "Get()" },
        { 0x02, "Set()" },
        { 0x81, "GetResp()" },
        { 0x03, "Send()" },
        { 0x05, "Trap()" },
        { 0x06, "Report()" },
        { 0x86, "ReportResp()" },
        { 0x07, "TrapRepress()" },
        { 0x12, "GetTable()" },
        { 0x92, "GetTableResp()" },
        { 0x13, "GetTraceTable()" },
        { 0x14, "GetMulti()" },
        { 0x94, "GetMultiResp()" },
        { 0x15, "Delete()" },
        { 0x95, "DeleteResp()" },
        { 0,    NULL }
    };

    static hf_register_info hf[] = {
        /* Local Route Header (LRH) */
        { &hf_infiniband_LRH, {
                "Local Route Header", "infiniband.lrh",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_virtual_lane, {
                "Virtual Lane", "infiniband.lrh.vl",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_link_version, {
                "Link Version", "infiniband.lrh.lver",
                FT_UINT8, BASE_DEC, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_service_level, {
                "Service Level", "infiniband.lrh.sl",
                FT_UINT8, BASE_DEC, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_reserved2, {
                "Reserved (2 bits)", "infiniband.lrh.reserved2",
                FT_UINT8, BASE_DEC, NULL, 0x0C, NULL, HFILL}
        },
        { &hf_infiniband_link_next_header, {
                "Link Next Header", "infiniband.lrh.lnh",
                FT_UINT8, BASE_HEX, NULL, 0x03, NULL, HFILL}
        },
        { &hf_infiniband_destination_local_id, {
                "Destination Local ID", "infiniband.lrh.dlid",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_reserved5, {
                "Reserved (5 bits)", "infiniband.lrh.reserved5",
                FT_UINT16, BASE_DEC, NULL, 0xF800, NULL, HFILL}
        },
        { &hf_infiniband_packet_length, {
                "Packet Length", "infiniband.lrh.pktlen",
                FT_UINT16, BASE_DEC, NULL, 0x07FF, NULL, HFILL}
        },
        { &hf_infiniband_source_local_id, {
                "Source Local ID", "infiniband.lrh.slid",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },

        /* Global Route Header (GRH) */
        { &hf_infiniband_GRH, {
                "Global Route Header", "infiniband.grh",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ip_version, {
                "IP Version", "infiniband.grh.ipver",
                FT_UINT8, BASE_DEC, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_traffic_class, {
                "Traffic Class", "infiniband.grh.tclass",
                FT_UINT16, BASE_DEC, NULL, 0x0FF0, NULL, HFILL}
        },
        { &hf_infiniband_flow_label, {
                "Flow Label", "infiniband.grh.flowlabel",
                FT_UINT32, BASE_DEC, NULL, 0x000FFFFF, NULL, HFILL}
        },
        { &hf_infiniband_payload_length, {
                "Payload Length", "infiniband.grh.paylen",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_next_header, {
                "Next Header", "infiniband.grh.nxthdr",
                FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_hop_limit, {
                "Hop Limit", "infiniband.grh.hoplmt",
                FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_source_gid, {
                "Source GID", "infiniband.grh.sgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_destination_gid, {
                "Destination GID", "infiniband.grh.dgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* Base Transport Header (BTH) */
        { &hf_infiniband_BTH, {
                "Base Transport Header", "infiniband.bth",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_opcode, {
                "Opcode", "infiniband.bth.opcode",
                FT_UINT8, BASE_DEC, VALS(bth_opcode_tbl), 0x0, NULL, HFILL}
        },
        { &hf_infiniband_solicited_event, {
                "Solicited Event", "infiniband.bth.se",
                FT_BOOLEAN, 8, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_migreq, {
                "MigReq", "infiniband.bth.m",
                FT_BOOLEAN, 8, NULL, 0x40, NULL, HFILL}
        },
        { &hf_infiniband_pad_count, {
                "Pad Count", "infiniband.bth.padcnt",
                FT_UINT8, BASE_DEC, NULL, 0x30, NULL, HFILL}
        },
        { &hf_infiniband_transport_header_version, {
                "Header Version", "infiniband.bth.tver",
                FT_UINT8, BASE_DEC, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_partition_key, {
                "Partition Key", "infiniband.bth.p_key",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_destination_qp, {
                "Destination Queue Pair", "infiniband.bth.destqp",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_acknowledge_request, {
                "Acknowledge Request", "infiniband.bth.a",
                FT_BOOLEAN, 8, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_reserved7, {
                "Reserved (7 bits)", "infiniband.bth.reserved7",
                FT_UINT8, BASE_DEC, NULL, 0x7F, NULL, HFILL}
        },
        { &hf_infiniband_packet_sequence_number, {
                "Packet Sequence Number", "infiniband.bth.psn",
                FT_UINT24, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },

        /* Raw Header (RWH) */
        { &hf_infiniband_RWH, {
                "Raw Header", "infiniband.rwh",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_etype, {
                "Ethertype", "infiniband.rwh.etype",
                FT_UINT16, BASE_HEX, VALS(etype_vals), 0x0, "Type", HFILL }
        },

        /* Reliable Datagram Extended Transport Header (RDETH) */
        { &hf_infiniband_RDETH, {
                "Reliable Datagram Extended Transport Header", "infiniband.rdeth",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ee_context, {
                "E2E Context", "infiniband.rdeth.eecnxt",
                FT_UINT24, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },

        /* Datagram Extended Transport Header (DETH) */
        { &hf_infiniband_DETH, {
                "Datagram Extended Transport Header", "infiniband.deth",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_queue_key, {
                "Queue Key", "infiniband.deth.q_key",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_source_qp, {
                "Source Queue Pair", "infiniband.deth.srcqp",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* RDMA Extended Transport Header (RETH) */
        { &hf_infiniband_RETH, {
                "RDMA Extended Transport Header", "infiniband.reth",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_virtual_address, {
                "Virtual Address", "infiniband.reth.va",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_remote_key, {
                "Remote Key", "infiniband.reth.r_key",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_dma_length, {
                "DMA Length", "infiniband.reth.dmalen",
                FT_UINT32, BASE_DEC_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* Atomic Extended Transport Header (AtomicETH) */
        { &hf_infiniband_AtomicETH, {
                "Atomic Extended Transport Header", "infiniband.atomiceth",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#if 0
        { &hf_infiniband_virtual_address_AtomicETH, {
                "Virtual Address", "infiniband.atomiceth.va",
                FT_UINT64, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_remote_key_AtomicETH, {
                "Remote Key", "infiniband.atomiceth.r_key",
                FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
#endif
        { &hf_infiniband_swap_or_add_data, {
                "Swap (Or Add) Data", "infiniband.atomiceth.swapdt",
                FT_UINT64, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_compare_data, {
                "Compare Data", "infiniband.atomiceth.cmpdt",
                FT_UINT64, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },

        /* ACK Extended Transport Header (AETH) */
        { &hf_infiniband_AETH, {
                "ACK Extended Transport Header", "infiniband.aeth",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_syndrome, {
                "Syndrome", "infiniband.aeth.syndrome",
                FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_syndrome_reserved, {
                "Reserved", "infiniband.aeth.syndrome.reserved",
                FT_UINT8, BASE_DEC, NULL, AETH_SYNDROME_RES, NULL, HFILL}
        },
        { &hf_infiniband_syndrome_opcode, {
                "OpCode", "infiniband.aeth.syndrome.opcode",
                FT_UINT8, BASE_DEC, VALS(aeth_syndrome_opcode_vals), AETH_SYNDROME_OPCODE, NULL, HFILL}
        },
        { &hf_infiniband_syndrome_credit_count, {
                "Credit Count", "infiniband.aeth.syndrome.credit_count",
                FT_UINT8, BASE_DEC, NULL, AETH_SYNDROME_VALUE, NULL, HFILL}
        },
        { &hf_infiniband_syndrome_timer, {
                "Timer", "infiniband.aeth.syndrome.timer",
                FT_UINT8, BASE_DEC, VALS(aeth_syndrome_timer_code_vals), AETH_SYNDROME_VALUE, NULL, HFILL}
        },
        { &hf_infiniband_syndrome_reserved_value, {
                "Reserved", "infiniband.aeth.syndrome.reserved_value",
                FT_UINT8, BASE_DEC, NULL, AETH_SYNDROME_VALUE, NULL, HFILL}
        },
        { &hf_infiniband_syndrome_error_code, {
                "Error Code", "infiniband.aeth.syndrome.error_code",
                FT_UINT8, BASE_DEC, VALS(aeth_syndrome_nak_error_code_vals), AETH_SYNDROME_VALUE, NULL, HFILL}
        },
        { &hf_infiniband_message_sequence_number, {
                "Message Sequence Number", "infiniband.aeth.msn",
                FT_UINT24, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },

        /* Atomic ACK Extended Transport Header (AtomicAckETH) */
        { &hf_infiniband_AtomicAckETH, {
                "Atomic ACK Extended Transport Header", "infiniband.atomicacketh",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_original_remote_data, {
                "Original Remote Data", "infiniband.atomicacketh.origremdt",
                FT_UINT64, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },

        /* Immediate Extended Transport Header (ImmDT) */
        { &hf_infiniband_IMMDT, {
                "Immediate Data", "infiniband.immdt",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* Invalidate Extended Transport Header (IETH) */
        { &hf_infiniband_IETH, {
                "RKey", "infiniband.ieth",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* FLUSH Extended Transport Header (FETH) */
        { &hf_infiniband_FETH, {
                "FLUSH Extended Transport Header", "infiniband.feth",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_reserved27, {
                "Reserved (27bits)", "infiniband.feth.reserved27",
                FT_UINT32, BASE_DEC, NULL, 0xFFFFFFE0, NULL, HFILL}
        },
        { &hf_infiniband_selectivity_level, {
                "Selectivity Level", "infiniband.feth.sel",
                FT_UINT8, BASE_DEC, NULL, 0x18, NULL, HFILL}
        },
        { &hf_infiniband_placement_type, {
                "Placement Type", "infiniband.feth.plt",
                FT_UINT8, BASE_DEC, NULL, 0x07, NULL, HFILL}
        },

        /* Payload */
        { &hf_infiniband_payload, {
                "Payload", "infiniband.payload",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_invariant_crc, {
                "Invariant CRC", "infiniband.invariant.crc",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_variant_crc, {
                "Variant CRC", "infiniband.variant.crc",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_raw_data, {
                "Raw Data", "infiniband.rawdata",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* Unknown or Vendor Specific */
        { &hf_infiniband_vendor, {
                "Unknown/Vendor Specific Data", "infiniband.vendor",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* Common Reserved fields */
        { &hf_infiniband_reserved, {
                "Reserved", "infiniband.reserved",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* CM REQ Header */
        {&hf_cm_req_local_comm_id, {
                "Local Communication ID", "infiniband.cm.req",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_service_id, {
                "ServiceID", "infiniband.cm.req.serviceid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_service_id_prefix, {
                "Prefix", "infiniband.cm.req.serviceid.prefix",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_service_id_protocol, {
                "Protocol", "infiniband.cm.req.serviceid.protocol",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_service_id_dport, {
                "Destination Port", "infiniband.cm.req.serviceid.dport",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_local_ca_guid, {
                "Local CA GUID", "infiniband.cm.req.localcaguid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_local_qkey, {
                "Local Q_Key", "infiniband.cm.req.localqkey",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_local_qpn, {
                "Local QPN", "infiniband.cm.req.localqpn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_respo_res, {
                "Responder Resources", "infiniband.cm.req.responderres",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_local_eecn, {
                "Local EECN", "infiniband.cm.req.localeecn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_init_depth, {
                "Initiator Depth", "infiniband.cm.req.initdepth",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_remote_eecn, {
                "Remote EECN", "infiniband.cm.req.remoteeecn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_remote_cm_resp_to, {
                "Remote CM Response Timeout", "infiniband.cm.req.remoteresptout",
                FT_UINT8, BASE_HEX, NULL, 0xF8, NULL, HFILL}
        },
        {&hf_cm_req_transp_serv_type, {
                "Transport Service Type", "infiniband.cm.req.transpsvctype",
                FT_UINT8, BASE_HEX, NULL, 0x06, NULL, HFILL}
        },
        {&hf_cm_req_e2e_flow_ctrl, {
                "End-to-End Flow Control", "infiniband.cm.req.e2eflowctrl",
                FT_UINT8, BASE_HEX, NULL, 0x1, NULL, HFILL}
        },
        {&hf_cm_req_start_psn, {
                "Starting PSN", "infiniband.cm.req.startpsn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_local_cm_resp_to, {
                "Local CM Response Timeout", "infiniband.cm.req.localresptout",
                FT_UINT8, BASE_HEX, NULL, 0xf8, NULL, HFILL}
        },
        {&hf_cm_req_retry_count, {
                "Retry Count", "infiniband.cm.req.retrcount",
                FT_UINT8, BASE_HEX, NULL, 0x7, NULL, HFILL}
        },
        {&hf_cm_req_pkey, {
                "Partition Key", "infiniband.cm.req.pkey",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_path_pp_mtu, {
                "Path Packet Payload MTU", "infiniband.cm.req.pppmtu",
                FT_UINT8, BASE_HEX, NULL, 0xf0, NULL, HFILL}
        },
        {&hf_cm_req_rdc_exists, {
                "RDC Exists", "infiniband.cm.req.rdcexist",
                FT_UINT8, BASE_HEX, NULL, 0x8, NULL, HFILL}
        },
        {&hf_cm_req_rnr_retry_count, {
                "RNR Retry Count", "infiniband.cm.req.rnrretrcount",
                FT_UINT8, BASE_HEX, NULL, 0x7, NULL, HFILL}
        },
        {&hf_cm_req_max_cm_retries, {
                "Max CM Retries", "infiniband.cm.req.maxcmretr",
                FT_UINT8, BASE_HEX, NULL, 0xf0, NULL, HFILL}
        },
        {&hf_cm_req_srq, {
                "SRQ", "infiniband.cm.req.srq",
                FT_UINT8, BASE_HEX, NULL, 0x8, NULL, HFILL}
        },
        {&hf_cm_req_extended_transport, {
                "Extended Transport", "infiniband.cm.req.ext_transport",
                FT_UINT8, BASE_HEX, NULL, 0x7, NULL, HFILL}
        },
        {&hf_cm_req_primary_local_lid, {
                "Primary Local Port LID", "infiniband.cm.req.prim_locallid",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_primary_remote_lid, {
                "Primary Remote Port LID", "infiniband.cm.req.prim_remotelid",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_primary_local_gid, {
                "Primary Local Port GID", "infiniband.cm.req.prim_localgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_primary_remote_gid, {
                "Primary Remote Port GID", "infiniband.cm.req.prim_remotegid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_primary_local_gid_ipv4, {
                "Primary Local Port GID", "infiniband.cm.req.prim_localgid_ipv4",
                FT_IPv4, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_primary_remote_gid_ipv4, {
                "Primary Remote Port GID", "infiniband.cm.req.prim_remotegid_ipv4",
                FT_IPv4, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_primary_flow_label, {
                "Primary Flow Label", "infiniband.cm.req.prim_flowlabel",
                FT_UINT32, BASE_HEX, NULL, 0xfffff000, NULL, HFILL}
        },
        {&hf_cm_req_primary_reserved0, {
                "Reserved", "infiniband.cm.req.prim_reserved0",
                FT_UINT32, BASE_HEX, NULL, 0x0fc0, NULL, HFILL}
        },
        {&hf_cm_req_primary_packet_rate, {
                "Primary Packet Rate", "infiniband.cm.req.prim_pktrate",
                FT_UINT32, BASE_HEX, NULL, 0x3f, NULL, HFILL}
        },
        {&hf_cm_req_primary_traffic_class, {
                "Primary Traffic Class", "infiniband.cm.req.prim_tfcclass",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_primary_hop_limit, {
                "Primary Hop Limit", "infiniband.cm.req.prim_hoplim",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_primary_sl, {
                "Primary SL", "infiniband.cm.req.prim_sl",
                FT_UINT8, BASE_HEX, NULL, 0xf0, NULL, HFILL}
        },
        {&hf_cm_req_primary_subnet_local, {
                "Primary Subnet Local", "infiniband.cm.req.prim_subnetlocal",
                FT_UINT8, BASE_HEX, NULL, 0x8, NULL, HFILL}
        },
        {&hf_cm_req_primary_reserved1, {
                "Reserved", "infiniband.cm.req.prim_reserved1",
                FT_UINT8, BASE_HEX, NULL, 0x7, NULL, HFILL}
        },
        {&hf_cm_req_primary_local_ack_to, {
                "Primary Local ACK Timeout", "infiniband.cm.req.prim_localacktout",
                FT_UINT8, BASE_HEX, NULL, 0xf8, NULL, HFILL}
        },
        {&hf_cm_req_primary_reserved2, {
                "Reserved", "infiniband.cm.req.prim_reserved2",
                FT_UINT8, BASE_HEX, NULL, 0x07, NULL, HFILL}
        },
        {&hf_cm_req_alt_local_lid, {
                "Alternate Local Port LID", "infiniband.cm.req.alt_locallid",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_alt_remote_lid, {
                "Alternate Remote Port LID", "infiniband.cm.req.alt_remotelid",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_alt_local_gid, {
                "Alternate Local Port GID", "infiniband.cm.req.alt_localgid",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_alt_remote_gid, {
                "Alternate Remote Port GID", "infiniband.cm.req.alt_remotegid",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_flow_label, {
                "Alternate Flow Label", "infiniband.cm.req.alt_flowlabel",
                FT_UINT32, BASE_HEX, NULL, 0xfffff000, NULL, HFILL}
        },
        {&hf_cm_req_alt_reserved0, {
                "Reserved", "infiniband.cm.req.alt_reserved0",
                FT_UINT32, BASE_HEX, NULL, 0x0fc0, NULL, HFILL}
        },
        {&hf_cm_req_packet_rate, {
                "Alternate Packet Rate", "infiniband.cm.req.alt_pktrate",
                FT_UINT32, BASE_HEX, NULL, 0x3f, NULL, HFILL}
        },
        {&hf_cm_req_alt_traffic_class, {
                "Alternate Traffic Class", "infiniband.cm.req.alt_tfcclass",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_alt_hop_limit, {
                "Alternate Hop Limit", "infiniband.cm.req.alt_hoplim",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_SL, {
                "Alternate SL", "infiniband.cm.req.alt_sl",
                FT_UINT8, BASE_HEX, NULL, 0xf0, NULL, HFILL}
        },
        {&hf_cm_req_subnet_local, {
                "Alternate Subnet Local", "infiniband.cm.req.alt_subnetlocal",
                FT_UINT8, BASE_HEX, NULL, 0x8, NULL, HFILL}
        },
        {&hf_cm_req_alt_reserved1, {
                "Reserved", "infiniband.cm.req.alt_reserved1",
                FT_UINT8, BASE_HEX, NULL, 0x7, NULL, HFILL}
        },
        {&hf_cm_req_local_ACK_timeout, {
                "Alternate Local ACK Timeout", "infiniband.cm.req.alt_localacktout",
                FT_UINT8, BASE_HEX, NULL, 0xf8, NULL, HFILL}
        },
        {&hf_cm_req_alt_reserved2, {
                "Reserved", "infiniband.cm.req.alt_reserved1",
                FT_UINT8, BASE_HEX, NULL, 0x07, NULL, HFILL}
        },
        {&hf_cm_req_private_data, {
                "PrivateData", "infiniband.cm.req.private",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_req_msg, {
                "IP CM Request Msg", "infiniband.cm.req.ip_cm",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_majv, {
                "IP CM Major Version", "infiniband.cm.req.ip_cm.majv",
                FT_UINT8, BASE_HEX, NULL, 0xf0, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_minv, {
                "IP CM Minor Version", "infiniband.cm.req.ip_cm.minv",
                FT_UINT8, BASE_HEX, NULL, 0x0f, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_ipv, {
                "IP CM IP Version", "infiniband.cm.req.ip_cm.ipv",
                FT_UINT8, BASE_HEX, NULL, 0xf0, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_res, {
                "IP CM Reserved", "infiniband.cm.req.ip_cm.reserved",
                FT_UINT8, BASE_HEX, NULL, 0x0f, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_sport, {
                "IP CM Source Port", "infiniband.cm.req.ip_cm.sport",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_sip6, {
                "IP CM Source IP", "infiniband.cm.req.ip_cm.sip6",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_dip6, {
                "IP CM Destination IP", "infiniband.cm.req.ip_cm.dip6",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_sip4, {
                "IP CM Source IP", "infiniband.cm.req.ip_cm.sip4",
                FT_IPv4, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_req_ip_cm_dip4, {
                "IP CM Destination IP", "infiniband.cm.req.ip_cm.dip4",
                FT_IPv4, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_ip_cm_req_consumer_private_data, {
                "IP CM Consumer PrivateData", "infiniband.cm.req.ip_cm.private",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* CM REP Header */
        {&hf_cm_rep_localcommid, {
                "Local Communication ID", "infiniband.cm.rep",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_remotecommid, {
                "Remote Communication ID", "infiniband.cm.rep.remotecommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_localqkey, {
                "Local Q_Key", "infiniband.cm.rep.localqkey",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_localqpn, {
                "Local QPN", "infiniband.cm.rep.localqpn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_localeecontnum, {
                "Local EE Context Number", "infiniband.cm.rep.localeecn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_startingpsn, {
                "Starting PSN", "infiniband.cm.rep.startpsn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_responderres, {
                "Responder Resources", "infiniband.cm.rep.respres",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_initiatordepth, {
                "Initiator Depth", "infiniband.cm.rep.initdepth",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_tgtackdelay, {
                "Target ACK Delay", "infiniband.cm.rep.tgtackdelay",
                FT_UINT8, BASE_HEX, NULL, 0xf8, NULL, HFILL}
        },
        {&hf_cm_rep_failoveracc, {
                "Failover Accepted", "infiniband.cm.rep.failoveracc",
                FT_UINT8, BASE_HEX, NULL, 0x6, NULL, HFILL}
        },
        {&hf_cm_rep_e2eflowctl, {
                "End-To-End Flow Control", "infiniband.cm.rep.e2eflowctrl",
                FT_UINT8, BASE_HEX, NULL, 0x1, NULL, HFILL}
        },
        {&hf_cm_rep_rnrretrycount, {
                "RNR Retry Count", "infiniband.cm.rep.rnrretrcount",
                FT_UINT8, BASE_HEX, NULL, 0xe0, NULL, HFILL}
        },
        {&hf_cm_rep_srq, {
                "SRQ", "infiniband.cm.rep.srq",
                FT_UINT8, BASE_HEX, NULL, 0x10, NULL, HFILL}
        },
        {&hf_cm_rep_reserved, {
                "Reserved", "infiniband.cm.rep.reserved",
                FT_UINT8, BASE_HEX, NULL, 0x0f, NULL, HFILL}
        },
        {&hf_cm_rep_localcaguid, {
                "Local CA GUID", "infiniband.cm.rep.localcaguid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rep_privatedata, {
                "PrivateData", "infiniband.cm.rep.private",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* IB CM RTU Header */
        {&hf_cm_rtu_localcommid, {
                "Local Communication ID", "infiniband.cm.rtu.localcommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rtu_remotecommid, {
                "Remote Communication ID", "infiniband.cm.rtu.remotecommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rtu_privatedata, {
                "PrivateData", "infiniband.cm.rtu.private",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* CM REJ Header */
        {&hf_cm_rej_local_commid, {
                "Local Communication ID", "infiniband.cm.rej.localcommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rej_remote_commid, {
                "Remote Communication ID", "infiniband.cm.rej.remotecommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rej_msg_rej, {
                "Message REJected", "infiniband.cm.rej.msgrej",
                FT_UINT8, BASE_HEX, NULL, 0xc0, NULL, HFILL}
        },
        {&hf_cm_rej_msg_reserved0, {
                "Reserved", "infiniband.cm.rej.reserved0",
                FT_UINT8, BASE_HEX, NULL, 0x03, NULL, HFILL}
        },
        {&hf_cm_rej_rej_info_len, {
                "Reject Info Length", "infiniband.cm.rej.rejinfolen",
                FT_UINT8, BASE_HEX, NULL, 0xfe, NULL, HFILL}
        },
        {&hf_cm_rej_msg_reserved1, {
                "Reserved", "infiniband.cm.rej.reserved1",
                FT_UINT8, BASE_HEX, NULL, 0x01, NULL, HFILL}
        },
        {&hf_cm_rej_reason, {
                "Reason", "infiniband.cm.rej.reason",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rej_add_rej_info, {
                "Additional Reject Information (ARI)", "infiniband.cm.rej.ari",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_rej_private_data, {
                "PrivateData", "infiniband.cm.rej.private",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* IB CM DREQ Header */
        {&hf_cm_dreq_localcommid, {
                "Local Communication ID", "infiniband.cm.dreq.localcommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_dreq_remotecommid, {
                "Remote Communication ID", "infiniband.cm.dreq.remotecommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_dreq_remote_qpn, {
                "Remote QPN/EECN", "infiniband.cm.req.remoteqpneecn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_dreq_privatedata, {
                "PrivateData", "infiniband.cm.dreq.private",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* IB CM DRSP Header */
        {&hf_cm_drsp_localcommid, {
                "Local Communication ID", "infiniband.cm.drsp.localcommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_drsp_remotecommid, {
                "Remote Communication ID", "infiniband.cm.drsp.remotecommid",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        {&hf_cm_drsp_privatedata, {
                "PrivateData", "infiniband.cm.drsp.private",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* MAD Base Header */
        { &hf_infiniband_MAD, {
                "MAD (Management Datagram) Common Header", "infiniband.mad",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_base_version, {
                "Base Version", "infiniband.mad.baseversion",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_mgmt_class, {
                "Management Class", "infiniband.mad.mgmtclass",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_class_version, {
                "Class Version", "infiniband.mad.classversion",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_method, {
                "Method", "infiniband.mad.method",
                FT_UINT8, BASE_HEX, VALS(mad_method_str), 0x0, NULL, HFILL}
        },
        { &hf_infiniband_status, {
                "Status", "infiniband.mad.status",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_class_specific, {
                "Class Specific", "infiniband.mad.classspecific",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_transaction_id, {
                "Transaction ID", "infiniband.mad.transactionid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_attribute_id, {
                "Attribute ID", "infiniband.mad.attributeid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_attribute_modifier, {
                "Attribute Modifier", "infiniband.mad.attributemodifier",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_data, {
                "MAD Data Payload", "infiniband.mad.data",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* RMPP Header */
        { &hf_infiniband_RMPP, {
                "RMPP (Reliable Multi-Packet Transaction Protocol)", "infiniband.rmpp",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_rmpp_version, {
                "RMPP Type", "infiniband.rmpp.rmppversion",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_rmpp_type, {
                "RMPP Type", "infiniband.rmpp.rmpptype",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_r_resp_time, {
                "R Resp Time", "infiniband.rmpp.rresptime",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_rmpp_flags, {
                "RMPP Flags", "infiniband.rmpp.rmppflags",
                FT_UINT8, BASE_HEX, VALS(RMPP_Flags), 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_rmpp_status, {
                "RMPP Status", "infiniband.rmpp.rmppstatus",
                FT_UINT8, BASE_HEX, VALS(RMPP_Status), 0x0, NULL, HFILL}
        },
        { &hf_infiniband_rmpp_data1, {
                "RMPP Data 1", "infiniband.rmpp.data1",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_rmpp_data2, {
                "RMPP Data 2", "infiniband.rmpp.data2",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

    /* RMPP Data */
#if 0
        { &hf_infiniband_RMPP_DATA, {
                "RMPP Data (Reliable Multi-Packet Transaction Protocol)", "infiniband.rmpp.data",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#endif
        { &hf_infiniband_segment_number, {
                "Segment Number", "infiniband.rmpp.segmentnumber",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_payload_length32, {
                "Payload Length", "infiniband.rmpp.payloadlength",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_transferred_data, {
                "Transferred Data", "infiniband.rmpp.transferreddata",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* RMPP ACK */
        { &hf_infiniband_new_window_last, {
                "New Window Last", "infiniband.rmpp.newwindowlast",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* RMPP ABORT/STOP */
        { &hf_infiniband_optional_extended_error_data, {
                "Optional Extended Error Data", "infiniband.rmpp.extendederrordata",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* SMP Data (LID Routed) */
        { &hf_infiniband_SMP_LID, {
                "Subnet Management Packet (LID Routed)", "infiniband.smplid",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_m_key, {
                "M_Key", "infiniband.smplid.mkey",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_smp_data, {
                "SMP Data", "infiniband.smplid.smpdata",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

    /* XX: All following verified/corrected against Infiniband 1.2.1 Specification */
        /* SMP Data Directed Route */
        { &hf_infiniband_SMP_DIRECTED, {
                "Subnet Management Packet (Directed Route)", "infiniband.smpdirected",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_smp_status, {
                "Status", "infiniband.smpdirected.smpstatus",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_hop_pointer, {
                "Hop Pointer", "infiniband.smpdirected.hoppointer",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_hop_count, {
                "Hop Count", "infiniband.smpdirected.hopcount",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_dr_slid, {
                "DrSLID", "infiniband.smpdirected.drslid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_dr_dlid, {
                "DrDLID", "infiniband.smpdirected.drdlid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_d, {
                "D (Direction Bit)", "infiniband.smpdirected.d",
                FT_UINT64, BASE_HEX, NULL, 0x8000, NULL, HFILL}
        },
        { &hf_infiniband_initial_path, {
                "Initial Path", "infiniband.smpdirected.initialpath",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_return_path, {
                "Return Path", "infiniband.smpdirected.returnpath",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* SA MAD Header */
        { &hf_infiniband_SA, {
                "SA Packet (Subnet Administration)", "infiniband.sa.drdlid",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_sm_key, {
                "SM_Key (Verification Key)", "infiniband.sa.smkey",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_attribute_offset, {
                "Attribute Offset", "infiniband.sa.attributeoffset",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_component_mask, {
                "Component Mask", "infiniband.sa.componentmask",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_subnet_admin_data, {
                "Subnet Admin Data", "infiniband.sa.subnetadmindata",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* NodeDescription */
        { &hf_infiniband_NodeDescription_NodeString, {
                "NodeString", "infiniband.nodedescription.nodestring",
                FT_STRING, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* NodeInfo */
        { &hf_infiniband_NodeInfo_BaseVersion, {
                "BaseVersion", "infiniband.nodeinfo.baseversion",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_ClassVersion, {
                "ClassVersion", "infiniband.nodeinfo.classversion",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_NodeType, {
                "NodeType", "infiniband.nodeinfo.nodetype",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_NumPorts, {
                "NumPorts", "infiniband.nodeinfo.numports",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_SystemImageGUID, {
                "SystemImageGUID", "infiniband.nodeinfo.systemimageguid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_NodeGUID, {
                "NodeGUID", "infiniband.nodeinfo.nodeguid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_PortGUID, {
                "PortGUID", "infiniband.nodeinfo.portguid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_PartitionCap, {
                "PartitionCap", "infiniband.nodeinfo.partitioncap",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_DeviceID, {
                "DeviceID", "infiniband.nodeinfo.deviceid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_Revision, {
                "Revision", "infiniband.nodeinfo.revision",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_LocalPortNum, {
                "LocalPortNum", "infiniband.nodeinfo.localportnum",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_NodeInfo_VendorID, {
                "VendorID", "infiniband.nodeinfo.vendorid",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* SwitchInfo */
        { &hf_infiniband_SwitchInfo_LinearFDBCap, {
                "LinearFDBCap", "infiniband.switchinfo.linearfdbcap",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_RandomFDBCap, {
                "RandomFDBCap", "infiniband.switchinfo.randomfdbcap",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_MulticastFDBCap, {
                "MulticastFDBCap", "infiniband.switchinfo.multicastfdbcap",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_LinearFDBTop, {
                "LinearFDBTop", "infiniband.switchinfo.linearfdbtop",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_DefaultPort, {
                "DefaultPort", "infiniband.switchinfo.defaultport",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_DefaultMulticastPrimaryPort, {
                "DefaultMulticastPrimaryPort", "infiniband.switchinfo.defaultmulticastprimaryport",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_DefaultMulticastNotPrimaryPort, {
                "DefaultMulticastNotPrimaryPort", "infiniband.switchinfo.defaultmulticastnotprimaryport",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_LifeTimeValue, {
                "LifeTimeValue", "infiniband.switchinfo.lifetimevalue",
                FT_UINT8, BASE_HEX, NULL, 0xF8, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_PortStateChange, {
                "PortStateChange", "infiniband.switchinfo.portstatechange",
                FT_UINT8, BASE_HEX, NULL, 0x04, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_OptimizedSLtoVLMappingProgramming, {
                "OptimizedSLtoVLMappingProgramming", "infiniband.switchinfo.optimizedsltovlmappingprogramming",
                FT_UINT8, BASE_HEX, NULL, 0x03, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_LIDsPerPort, {
                "LIDsPerPort", "infiniband.switchinfo.lidsperport",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_PartitionEnforcementCap, {
                "PartitionEnforcementCap", "infiniband.switchinfo.partitionenforcementcap",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_InboundEnforcementCap, {
                "InboundEnforcementCap", "infiniband.switchinfo.inboundenforcementcap",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_OutboundEnforcementCap, {
                "OutboundEnforcementCap", "infiniband.switchinfo.outboundenforcementcap",
                FT_UINT8, BASE_HEX, NULL, 0x40, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_FilterRawInboundCap, {
                "FilterRawInboundCap", "infiniband.switchinfo.filterrawinboundcap",
                FT_UINT8, BASE_HEX, NULL, 0x20, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_FilterRawOutboundCap, {
                "FilterRawOutboundCap", "infiniband.switchinfo.filterrawoutboundcap",
                FT_UINT8, BASE_HEX, NULL, 0x10, NULL, HFILL}
        },
        { &hf_infiniband_SwitchInfo_EnhancedPortZero, {
                "EnhancedPortZero", "infiniband.switchinfo.enhancedportzero",
                FT_UINT8, BASE_HEX, NULL, 0x08, NULL, HFILL}
        },

        /* GUIDInfo */
#if 0
        { &hf_infiniband_GUIDInfo_GUIDBlock, {
                "GUIDBlock", "infiniband.switchinfo.guidblock",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#endif
        { &hf_infiniband_GUIDInfo_GUID, {
                "GUID", "infiniband.switchinfo.guid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* PortInfo */
        { &hf_infiniband_PortInfo_M_Key, {
                "M_Key", "infiniband.portinfo.m_key",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_GidPrefix, {
                "GidPrefix", "infiniband.portinfo.guid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LID, {
                "LID", "infiniband.portinfo.lid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_MasterSMLID, {
                "MasterSMLID", "infiniband.portinfo.mastersmlid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask, {
                "CapabilityMask", "infiniband.portinfo.capabilitymask",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* Capability Mask Flags */
        { &hf_infiniband_PortInfo_CapabilityMask_SM, {
                "SM", "infiniband.portinfo.capabilitymask.issm",
                FT_UINT32, BASE_HEX, NULL, 0x00000002, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_NoticeSupported, {
                "NoticeSupported", "infiniband.portinfo.capabilitymask.noticesupported",
                FT_UINT32, BASE_HEX, NULL, 0x00000004, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_TrapSupported, {
                "TrapSupported", "infiniband.portinfo.capabilitymask.trapsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00000008, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_OptionalIPDSupported, {
                "OptionalIPDSupported", "infiniband.portinfo.capabilitymask.optionalipdsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00000010, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_AutomaticMigrationSupported, {
                "AutomaticMigrationSupported", "infiniband.portinfo.capabilitymask.automaticmigrationsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00000020, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_SLMappingSupported, {
                "SLMappingSupported", "infiniband.portinfo.capabilitymask.slmappingsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00000040, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_MKeyNVRAM, {
                "MKeyNVRAM", "infiniband.portinfo.capabilitymask.mkeynvram",
                FT_UINT32, BASE_HEX, NULL, 0x00000080, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_PKeyNVRAM, {
                "PKeyNVRAM", "infiniband.portinfo.capabilitymask.pkeynvram",
                FT_UINT32, BASE_HEX, NULL, 0x00000100, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_LEDInfoSupported, {
                "LEDInfoSupported", "infiniband.portinfo.capabilitymask.ledinfosupported",
                FT_UINT32, BASE_HEX, NULL, 0x00000200, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_SMdisabled, {
                "SMdisabled", "infiniband.portinfo.capabilitymask.smdisabled",
                FT_UINT32, BASE_HEX, NULL, 0x00000400, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_SystemImageGUIDSupported, {
                "SystemImageGUIDSupported", "infiniband.portinfo.capabilitymask.systemimageguidsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00000800, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_PKeySwitchExternalPortTrapSupported, {
                "PKeySwitchExternalPortTrapSupported", "infiniband.portinfo.capabilitymask.pkeyswitchexternalporttrapsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00001000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_CommunicationManagementSupported, {
                "CommunicationManagementSupported", "infiniband.portinfo.capabilitymask.communicationmanagementsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00010000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_SNMPTunnelingSupported, {
                "SNMPTunnelingSupported", "infiniband.portinfo.capabilitymask.snmptunnelingsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00020000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_ReinitSupported, {
                "ReinitSupported", "infiniband.portinfo.capabilitymask.reinitsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00040000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_DeviceManagementSupported, {
                "DeviceManagementSupported", "infiniband.portinfo.capabilitymask.devicemanagementsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00080000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_VendorClassSupported, {
                "VendorClassSupported", "infiniband.portinfo.capabilitymask.vendorclasssupported",
                FT_UINT32, BASE_HEX, NULL, 0x00100000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_DRNoticeSupported, {
                "DRNoticeSupported", "infiniband.portinfo.capabilitymask.drnoticesupported",
                FT_UINT32, BASE_HEX, NULL, 0x00200000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_CapabilityMaskNoticeSupported, {
                "CapabilityMaskNoticeSupported", "infiniband.portinfo.capabilitymask.capabilitymasknoticesupported",
                FT_UINT32, BASE_HEX, NULL, 0x00400000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_BootManagementSupported, {
                "BootManagementSupported", "infiniband.portinfo.capabilitymask.bootmanagementsupported",
                FT_UINT32, BASE_HEX, NULL, 0x00800000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_LinkRoundTripLatencySupported, {
                "LinkRoundTripLatencySupported", "infiniband.portinfo.capabilitymask.linkroundtriplatencysupported",
                FT_UINT32, BASE_HEX, NULL, 0x01000000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_ClientRegistrationSupported, {
                "ClientRegistrationSupported", "infiniband.portinfo.capabilitymask.clientregistrationsupported",
                FT_UINT32, BASE_HEX, NULL, 0x02000000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_OtherLocalChangesNoticeSupported, {
                "OtherLocalChangesNoticeSupported", "infiniband.portinfo.capabilitymask.otherlocalchangesnoticesupported",
                FT_UINT32, BASE_HEX, NULL, 0x04000000, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_CapabilityMask_LinkSpeedWIdthPairsTableSupported, {
                "LinkSpeedWIdthPairsTableSupported", "infiniband.portinfo.capabilitymask.linkspeedwidthpairstablesupported",
                FT_UINT32, BASE_HEX, NULL, 0x08000000, NULL, HFILL}
        },
        /* End Capability Mask Flags */

        /* PortInfo */
        { &hf_infiniband_PortInfo_DiagCode, {
                "DiagCode", "infiniband.portinfo.diagcode",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_M_KeyLeasePeriod, {
                "M_KeyLeasePeriod", "infiniband.portinfo.m_keyleaseperiod",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LocalPortNum, {
                "LocalPortNum", "infiniband.portinfo.localportnum",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LinkWidthEnabled, {
                "LinkWidthEnabled", "infiniband.portinfo.linkwidthenabled",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LinkWidthSupported, {
                "LinkWidthSupported", "infiniband.portinfo.linkwidthsupported",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LinkWidthActive, {
                "LinkWidthActive", "infiniband.portinfo.linkwidthactive",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LinkSpeedSupported, {
                "LinkSpeedSupported", "infiniband.portinfo.linkspeedsupported",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_PortState, {
                "PortState", "infiniband.portinfo.portstate",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_PortPhysicalState, {
                "PortPhysicalState", "infiniband.portinfo.portphysicalstate",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LinkDownDefaultState, {
                "LinkDownDefaultState", "infiniband.portinfo.linkdowndefaultstate",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_M_KeyProtectBits, {
                "M_KeyProtectBits", "infiniband.portinfo.m_keyprotectbits",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LMC, {
                "LMC", "infiniband.portinfo.lmc",
                FT_UINT8, BASE_HEX, NULL, 0x07, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LinkSpeedActive, {
                "LinkSpeedActive", "infiniband.portinfo.linkspeedactive",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LinkSpeedEnabled, {
                "LinkSpeedEnabled", "infiniband.portinfo.linkspeedenabled",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_NeighborMTU, {
                "NeighborMTU", "infiniband.portinfo.neighbormtu",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_MasterSMSL, {
                "MasterSMSL", "infiniband.portinfo.mastersmsl",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_VLCap, {
                "VLCap", "infiniband.portinfo.vlcap",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_InitType, {
                "InitType", "infiniband.portinfo.inittype",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_VLHighLimit, {
                "VLHighLimit", "infiniband.portinfo.vlhighlimit",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_VLArbitrationHighCap, {
                "VLArbitrationHighCap", "infiniband.portinfo.vlarbitrationhighcap",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_VLArbitrationLowCap, {
                "VLArbitrationLowCap", "infiniband.portinfo.vlarbitrationlowcap",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_InitTypeReply, {
                "InitTypeReply", "infiniband.portinfo.inittypereply",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_MTUCap, {
                "MTUCap", "infiniband.portinfo.mtucap",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_VLStallCount, {
                "VLStallCount", "infiniband.portinfo.vlstallcount",
                FT_UINT8, BASE_HEX, NULL, 0xE0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_HOQLife, {
                "HOQLife", "infiniband.portinfo.hoqlife",
                FT_UINT8, BASE_HEX, NULL, 0x1F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_OperationalVLs, {
                "OperationalVLs", "infiniband.portinfo.operationalvls",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_PartitionEnforcementInbound, {
                "PartitionEnforcementInbound", "infiniband.portinfo.partitionenforcementinbound",
                FT_UINT8, BASE_HEX, NULL, 0x08, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_PartitionEnforcementOutbound, {
                "PartitionEnforcementOutbound", "infiniband.portinfo.partitionenforcementoutbound",
                FT_UINT8, BASE_HEX, NULL, 0x04, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_FilterRawInbound, {
                "FilterRawInbound", "infiniband.portinfo.filterrawinbound",
                FT_UINT8, BASE_HEX, NULL, 0x02, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_FilterRawOutbound, {
                "FilterRawOutbound", "infiniband.portinfo.filterrawoutbound",
                FT_UINT8, BASE_HEX, NULL, 0x01, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_M_KeyViolations, {
                "M_KeyViolations", "infiniband.portinfo.m_keyviolations",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_P_KeyViolations, {
                "P_KeyViolations", "infiniband.portinfo.p_keyviolations",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_Q_KeyViolations, {
                "Q_KeyViolations", "infiniband.portinfo.q_keyviolations",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_GUIDCap, {
                "GUIDCap", "infiniband.portinfo.guidcap",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_ClientReregister, {
                "ClientReregister", "infiniband.portinfo.clientreregister",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_SubnetTimeOut, {
                "SubnetTimeOut", "infiniband.portinfo.subnettimeout",
                FT_UINT8, BASE_HEX, NULL, 0x1F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_RespTimeValue, {
                "RespTimeValue", "infiniband.portinfo.resptimevalue",
                FT_UINT8, BASE_HEX, NULL, 0x1F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LocalPhyErrors, {
                "LocalPhyErrors", "infiniband.portinfo.localphyerrors",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_OverrunErrors, {
                "OverrunErrors", "infiniband.portinfo.overrunerrors",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_MaxCreditHint, {
                "MaxCreditHint", "infiniband.portinfo.maxcredithint",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PortInfo_LinkRoundTripLatency, {
                "LinkRoundTripLatency", "infiniband.portinfo.linkroundtriplatency",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* P_KeyTable */
        { &hf_infiniband_P_KeyTable_P_KeyTableBlock, {
                "P_KeyTableBlock", "infiniband.p_keytable.p_keytableblock",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_P_KeyTable_MembershipType, {
                "MembershipType", "infiniband.p_keytable.membershiptype",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_P_KeyTable_P_KeyBase, {
                "P_KeyBase", "infiniband.p_keytable.p_keybase",
                FT_UINT16, BASE_HEX, NULL, 0x7FFF, NULL, HFILL}
        },

        /* SLtoVLMappingTable */
        { &hf_infiniband_SLtoVLMappingTable_SLtoVL_HighBits, {
                "SL(x)toVL", "infiniband.sltovlmappingtable.sltovlhighbits",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_SLtoVLMappingTable_SLtoVL_LowBits, {
                "SL(x)toVL", "infiniband.sltovlmappingtable.sltovllowbits",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },

        /* VLArbitrationTable */
#if 0
        { &hf_infiniband_VLArbitrationTable_VLWeightPairs, {
                "VLWeightPairs", "infiniband.vlarbitrationtable.vlweightpairs",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#endif
        { &hf_infiniband_VLArbitrationTable_VL, {
                "VL", "infiniband.vlarbitrationtable.vl",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_VLArbitrationTable_Weight, {
                "Weight", "infiniband.vlarbitrationtable.weight",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* LinearForwardingTable */
#if 0
        { &hf_infiniband_LinearForwardingTable_LinearForwardingTableBlock, {
                "LinearForwardingTableBlock", "infiniband.linearforwardingtable.linearforwardingtableblock",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#endif
        { &hf_infiniband_LinearForwardingTable_Port, {
                "Port", "infiniband.linearforwardingtable.port",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* RandomForwardingTable */
#if 0
        { &hf_infiniband_RandomForwardingTable_RandomForwardingTableBlock, {
                "RandomForwardingTableBlock", "infiniband.randomforwardingtable.randomforwardingtableblock",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#endif
        { &hf_infiniband_RandomForwardingTable_LID, {
                "LID", "infiniband.randomforwardingtable.lid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_RandomForwardingTable_Valid, {
                "Valid", "infiniband.randomforwardingtable.valid",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_RandomForwardingTable_LMC, {
                "LMC", "infiniband.randomforwardingtable.lmc",
                FT_UINT8, BASE_HEX, NULL, 0x70, NULL, HFILL}
        },
        { &hf_infiniband_RandomForwardingTable_Port, {
                "Port", "infiniband.randomforwardingtable.port",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* MulticastForwardingTable */
#if 0
        { &hf_infiniband_MulticastForwardingTable_MulticastForwardingTableBlock , {
                "MulticastForwardingTableBlock", "infiniband.multicastforwardingtable.multicastforwardingtableblock",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#endif
        { &hf_infiniband_MulticastForwardingTable_PortMask, {
                "PortMask", "infiniband.multicastforwardingtable.portmask",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* SMInfo */
        { &hf_infiniband_SMInfo_GUID, {
                "GUID", "infiniband.sminfo.guid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SMInfo_SM_Key, {
                "SM_Key", "infiniband.sminfo.sm_key",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SMInfo_ActCount, {
                "ActCount", "infiniband.sminfo.actcount",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SMInfo_Priority, {
                "Priority", "infiniband.sminfo.priority",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_SMInfo_SMState, {
                "SMState", "infiniband.sminfo.smstate",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },

        /* VendorDiag */
        { &hf_infiniband_VendorDiag_NextIndex, {
                "NextIndex", "infiniband.vendordiag.nextindex",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_VendorDiag_DiagData, {
                "DiagData", "infiniband.vendordiag.diagdata",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* LedInfo */
        { &hf_infiniband_LedInfo_LedMask, {
                "LedMask", "infiniband.ledinfo.ledmask",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },

        /* LinkSpeedWidthPairsTable */
        { &hf_infiniband_LinkSpeedWidthPairsTable_NumTables, {
                "NumTables", "infiniband.linkspeedwidthpairstable.numtables",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_LinkSpeedWidthPairsTable_PortMask, {
                "PortMask", "infiniband.linkspeedwidthpairstable.portmask",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_LinkSpeedWidthPairsTable_SpeedTwoFive, {
                "Speed 2.5 Gbps", "infiniband.linkspeedwidthpairstable.speedtwofive",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_LinkSpeedWidthPairsTable_SpeedFive, {
                "Speed 5 Gbps", "infiniband.linkspeedwidthpairstable.speedfive",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_LinkSpeedWidthPairsTable_SpeedTen, {
                "Speed 10 Gbps", "infiniband.linkspeedwidthpairstable.speedten",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },

        /* NodeRecord */
        /* PortInfoRecord */
        /* SLtoVLMappingTableRecord */
        /* SwitchInfoRecord */
        /* LinearForwardingTableRecord */
        /* RandomForwardingTableRecord */
        /* MulticastForwardingTableRecord */
        /* VLArbitrationTableRecord */
        { &hf_infiniband_SA_LID, {
                "LID", "infiniband.sa.lid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SA_EndportLID, {
                "EndportLID", "infiniband.sa.endportlid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SA_PortNum, {
                "PortNum", "infiniband.sa.portnum",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SA_InputPortNum , {
                "InputPortNum", "infiniband.sa.inputportnum",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SA_OutputPortNum, {
                "OutputPortNum", "infiniband.sa.outputportnum",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SA_BlockNum_EightBit, {
                "BlockNum_EightBit", "infiniband.sa.blocknum_eightbit",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SA_BlockNum_NineBit, {
                "BlockNum_NineBit", "infiniband.sa.blocknum_ninebit",
                FT_UINT16, BASE_HEX, NULL, 0x01FF, NULL, HFILL}
        },
        { &hf_infiniband_SA_BlockNum_SixteenBit, {
                "BlockNum_SixteenBit", "infiniband.sa.blocknum_sixteenbit",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_SA_Position, {
                "Position", "infiniband.sa.position",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
#if 0
        { &hf_infiniband_SA_Index, {
                "Index", "infiniband.sa.index",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
#endif

        /* InformInfoRecord */
        { &hf_infiniband_InformInfoRecord_SubscriberGID, {
                "SubscriberGID", "infiniband.informinforecord.subscribergid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfoRecord_Enum, {
                "Enum", "infiniband.informinforecord.enum",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* InformInfo */
        { &hf_infiniband_InformInfo_GID, {
                "GID", "infiniband.informinfo.gid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_LIDRangeBegin, {
                "LIDRangeBegin", "infiniband.informinfo.lidrangebegin",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_LIDRangeEnd, {
                "LIDRangeEnd", "infiniband.informinfo.lidrangeend",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_IsGeneric, {
                "IsGeneric", "infiniband.informinfo.isgeneric",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_Subscribe, {
                "Subscribe", "infiniband.informinfo.subscribe",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_Type, {
                "Type", "infiniband.informinfo.type",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_TrapNumberDeviceID, {
                "TrapNumberDeviceID", "infiniband.informinfo.trapnumberdeviceid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_QPN, {
                "QPN", "infiniband.informinfo.qpn",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_RespTimeValue, {
                "RespTimeValue", "infiniband.informinfo.resptimevalue",
                FT_UINT8, BASE_HEX, NULL, 0x1F, NULL, HFILL}
        },
        { &hf_infiniband_InformInfo_ProducerTypeVendorID, {
                "ProducerTypeVendorID", "infiniband.informinfo.producertypevendorid",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* LinkRecord */
        { &hf_infiniband_LinkRecord_FromLID, {
                "FromLID", "infiniband.linkrecord.fromlid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_LinkRecord_FromPort, {
                "FromPort", "infiniband.linkrecord.fromport",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_LinkRecord_ToPort, {
                "ToPort", "infiniband.linkrecord.toport",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_LinkRecord_ToLID, {
                "ToLID", "infiniband.linkrecord.tolid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* ServiceRecord */
        { &hf_infiniband_ServiceRecord_ServiceID, {
                "ServiceID", "infiniband.linkrecord.serviceid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ServiceRecord_ServiceGID, {
                "ServiceGID", "infiniband.linkrecord.servicegid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ServiceRecord_ServiceP_Key, {
                "ServiceP_Key", "infiniband.linkrecord.servicep_key",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ServiceRecord_ServiceLease, {
                "ServiceLease", "infiniband.linkrecord.servicelease",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ServiceRecord_ServiceKey, {
                "ServiceKey", "infiniband.linkrecord.servicekey",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ServiceRecord_ServiceName, {
                "ServiceName", "infiniband.linkrecord.servicename",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ServiceRecord_ServiceData, {
                "ServiceData", "infiniband.linkrecord.servicedata",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* ServiceAssociationRecord */
        { &hf_infiniband_ServiceAssociationRecord_ServiceKey, {
                "ServiceKey", "infiniband.serviceassociationrecord.servicekey",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ServiceAssociationRecord_ServiceName, {
                "ServiceName", "infiniband.serviceassociationrecord.servicename",
                FT_STRING, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* PathRecord */
        { &hf_infiniband_PathRecord_DGID, {
                "DGID", "infiniband.pathrecord.dgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_SGID, {
                "SGID", "infiniband.pathrecord.sgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_DLID, {
                "DLID", "infiniband.pathrecord.dlid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_SLID, {
                "SLID", "infiniband.pathrecord.slid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_RawTraffic, {
                "RawTraffic", "infiniband.pathrecord.rawtraffic",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_FlowLabel, {
                "FlowLabel", "infiniband.pathrecord.flowlabel",
                FT_UINT24, BASE_HEX, NULL, 0x0FFFFF, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_HopLimit, {
                "HopLimit", "infiniband.pathrecord.hoplimit",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_TClass, {
                "TClass", "infiniband.pathrecord.tclass",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_Reversible, {
                "Reversible", "infiniband.pathrecord.reversible",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_NumbPath, {
                "NumbPath", "infiniband.pathrecord.numbpath",
                FT_UINT8, BASE_HEX, NULL, 0x7F, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_P_Key, {
                "P_Key", "infiniband.pathrecord.p_key",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_SL, {
                "SL", "infiniband.pathrecord.sl",
                FT_UINT16, BASE_HEX, NULL, 0x000F, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_MTUSelector, {
                "MTUSelector", "infiniband.pathrecord.mtuselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_MTU, {
                "MTU", "infiniband.pathrecord.mtu",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_RateSelector, {
                "RateSelector", "infiniband.pathrecord.rateselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_Rate, {
                "Rate", "infiniband.pathrecord.rate",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_PacketLifeTimeSelector, {
                "PacketLifeTimeSelector", "infiniband.pathrecord.packetlifetimeselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_PacketLifeTime, {
                "PacketLifeTime", "infiniband.pathrecord.packetlifetime",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_PathRecord_Preference, {
                "Preference", "infiniband.pathrecord.preference",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* MCMemberRecord */
        { &hf_infiniband_MCMemberRecord_MGID, {
                "MGID", "infiniband.mcmemberrecord.mgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_PortGID, {
                "PortGID", "infiniband.mcmemberrecord.portgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_Q_Key, {
                "Q_Key", "infiniband.mcmemberrecord.q_key",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_MLID, {
                "MLID", "infiniband.mcmemberrecord.mlid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_MTUSelector, {
                "MTUSelector", "infiniband.mcmemberrecord.mtuselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_MTU, {
                "MTU", "infiniband.mcmemberrecord.mtu",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_TClass, {
                "TClass", "infiniband.mcmemberrecord.tclass",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_P_Key, {
                "P_Key", "infiniband.mcmemberrecord.p_key",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_RateSelector, {
                "RateSelector", "infiniband.mcmemberrecord.rateselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_Rate, {
                "Rate", "infiniband.mcmemberrecord.rate",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_PacketLifeTimeSelector, {
                "PacketLifeTimeSelector", "infiniband.mcmemberrecord.packetlifetimeselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_PacketLifeTime, {
                "PacketLifeTime", "infiniband.mcmemberrecord.packetlifetime",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_SL, {
                "SL", "infiniband.mcmemberrecord.sl",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_FlowLabel, {
                "FlowLabel", "infiniband.mcmemberrecord.flowlabel",
                FT_UINT24, BASE_HEX, NULL, 0x0FFFFF, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_HopLimit, {
                "HopLimit", "infiniband.mcmemberrecord.hoplimit",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_Scope, {
                "Scope", "infiniband.mcmemberrecord.scope",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_JoinState, {
                "JoinState", "infiniband.mcmemberrecord.joinstate",
                FT_UINT8, BASE_HEX, NULL, 0x0F, NULL, HFILL}
        },
        { &hf_infiniband_MCMemberRecord_ProxyJoin, {
                "ProxyJoin", "infiniband.mcmemberrecord.proxyjoin",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },

        /* TraceRecord */
        { &hf_infiniband_TraceRecord_GIDPrefix, {
                "GidPrefix", "infiniband.tracerecord.gidprefix",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_TraceRecord_IDGeneration, {
                "IDGeneration", "infiniband.tracerecord.idgeneration",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_TraceRecord_NodeType, {
                "NodeType", "infiniband.tracerecord.nodetype",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_TraceRecord_NodeID, {
                "NodeID", "infiniband.tracerecord.nodeid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_TraceRecord_ChassisID, {
                "ChassisID", "infiniband.tracerecord.chassisid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_TraceRecord_EntryPortID, {
                "EntryPortID", "infiniband.tracerecord.entryportid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_TraceRecord_ExitPortID, {
                "ExitPortID", "infiniband.tracerecord.exitportid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_TraceRecord_EntryPort, {
                "EntryPort", "infiniband.tracerecord.entryport",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_TraceRecord_ExitPort, {
                "ExitPort", "infiniband.tracerecord.exitport",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* MultiPathRecord */
        { &hf_infiniband_MultiPathRecord_RawTraffic, {
                "RawTraffic", "infiniband.multipathrecord.rawtraffic",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_FlowLabel, {
                "FlowLabel", "infiniband.multipathrecord.flowlabel",
                FT_UINT24, BASE_HEX, NULL, 0x0FFFFF, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_HopLimit, {
                "HopLimit", "infiniband.multipathrecord.hoplimit",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_TClass, {
                "TClass", "infiniband.multipathrecord.tclass",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_Reversible, {
                "Reversible", "infiniband.multipathrecord.reversible",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_NumbPath, {
                "NumbPath", "infiniband.multipathrecord.numbpath",
                FT_UINT8, BASE_HEX, NULL, 0x7F, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_P_Key, {
                "P_Key", "infiniband.multipathrecord.p_key",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_SL, {
                "SL", "infiniband.multipathrecord.sl",
                FT_UINT16, BASE_HEX, NULL, 0x000F, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_MTUSelector, {
                "MTUSelector", "infiniband.multipathrecord.mtuselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_MTU, {
                "MTU", "infiniband.multipathrecord.mtu",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_RateSelector, {
                "RateSelector", "infiniband.multipathrecord.rateselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_Rate, {
                "Rate", "infiniband.multipathrecord.rate",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_PacketLifeTimeSelector, {
                "PacketLifeTimeSelector", "infiniband.multipathrecord.packetlifetimeselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_PacketLifeTime, {
                "PacketLifeTime", "infiniband.multipathrecord.packetlifetime",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_IndependenceSelector, {
                "IndependenceSelector", "infiniband.multipathrecord.independenceselector",
                FT_UINT8, BASE_HEX, NULL, 0xC0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_GIDScope, {
                "GIDScope", "infiniband.multipathrecord.gidscope",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_SGIDCount, {
                "SGIDCount", "infiniband.multipathrecord.sgidcount",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_DGIDCount, {
                "DGIDCount", "infiniband.multipathrecord.dgidcount",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_MultiPathRecord_SDGID, {
                "SDGID", "infiniband.multipathrecord.sdgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* ClassPortInfo */
        { &hf_infiniband_ClassPortInfo_BaseVersion, {
                "BaseVersion", "infiniband.classportinfo.baseversion",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_ClassVersion, {
                "ClassVersion", "infiniband.classportinfo.classversion",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_CapabilityMask, {
                "CapabilityMask", "infiniband.classportinfo.capabilitymask",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_CapabilityMask2, {
                "CapabilityMask2", "infiniband.classportinfo.capabilitymask2",
                FT_UINT32, BASE_HEX, NULL, 0xFFFFFFE0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RespTimeValue, {
                "RespTimeValue", "infiniband.classportinfo.resptimevalue",
                FT_UINT8, BASE_HEX, NULL, 0x1F, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RedirectGID, {
                "RedirectGID", "infiniband.classportinfo.redirectgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RedirectTC, {
                "RedirectTC", "infiniband.classportinfo.redirecttc",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RedirectSL, {
                "RedirectSL", "infiniband.classportinfo.redirectsl",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RedirectFL, {
                "RedirectFL", "infiniband.classportinfo.redirectfl",
                FT_UINT24, BASE_HEX, NULL, 0x0FFFFF, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RedirectLID, {
                "RedirectLID", "infiniband.classportinfo.redirectlid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RedirectP_Key, {
                "RedirectP_Key", "infiniband.classportinfo.redirectpkey",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_Reserved, {
                "Reserved", "infiniband.classportinfo.reserved",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RedirectQP, {
                "RedirectQP", "infiniband.classportinfo.redirectqp",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_RedirectQ_Key, {
                "RedirectQ_Key", "infiniband.classportinfo.redirectqkey",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_TrapGID, {
                "TrapGID", "infiniband.classportinfo.trapgid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_TrapTC, {
                "TrapTC", "infiniband.classportinfo.traptc",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_TrapSL, {
                "TrapSL", "infiniband.classportinfo.trapsl",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_TrapFL, {
                "TrapFL", "infiniband.classportinfo.trapfl",
                FT_UINT24, BASE_HEX, NULL, 0x0FFFFF, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_TrapLID, {
                "TrapLID", "infiniband.classportinfo.traplid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_TrapP_Key, {
                "TrapP_Key", "infiniband.classportinfo.trappkey",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_TrapQP, {
                "TrapQP", "infiniband.classportinfo.trapqp",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_ClassPortInfo_TrapQ_Key, {
                "TrapQ_Key", "infiniband.classportinfo.trapqkey",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* Notice */
        { &hf_infiniband_Notice_IsGeneric, {
                "IsGeneric", "infiniband.notice.isgeneric",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_Notice_Type, {
                "Type", "infiniband.notice.type",
                FT_UINT8, BASE_HEX, NULL, 0x7F, NULL, HFILL}
        },
        { &hf_infiniband_Notice_ProducerTypeVendorID, {
                "ProducerTypeVendorID", "infiniband.notice.producertypevendorid",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Notice_TrapNumberDeviceID, {
                "TrapNumberDeviceID", "infiniband.notice.trapnumberdeviceid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Notice_IssuerLID, {
                "IssuerLID", "infiniband.notice.issuerlid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Notice_NoticeToggle, {
                "NoticeToggle", "infiniband.notice.noticetoggle",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_Notice_NoticeCount, {
                "NoticeCount", "infiniband.notice.noticecount",
                FT_UINT16, BASE_HEX, NULL, 0x7FFF, NULL, HFILL}
        },
        { &hf_infiniband_Notice_DataDetails, {
                "DataDetails", "infiniband.notice.datadetails",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#if 0
        { &hf_infiniband_Notice_IssuerGID, {
                "IssuerGID", "infiniband.notice.issuergid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Notice_ClassTrapSpecificData, {
                "ClassTrapSpecificData", "infiniband.notice.classtrapspecificdata",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#endif

        /* Traps 64,65,66,67 */
        { &hf_infiniband_Trap_GIDADDR, {
                "GIDADDR", "infiniband.trap.gidaddr",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* Traps 68,69 */
        { &hf_infiniband_Trap_COMP_MASK, {
                "COMP_MASK", "infiniband.trap.comp_mask",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_WAIT_FOR_REPATH, {
                "WAIT_FOR_REPATH", "infiniband.trap.wait_for_repath",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
#if 0
        { &hf_infiniband_Trap_PATH_REC, {
                "PATH_REC", "infiniband.trap.path_rec",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
#endif

        /* Trap 128 */
        { &hf_infiniband_Trap_LIDADDR, {
                "LIDADDR", "infiniband.trap.lidaddr",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* Trap 129, 130, 131 */
        { &hf_infiniband_Trap_PORTNO, {
                "PORTNO", "infiniband.trap.portno",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* Trap 144 */
        { &hf_infiniband_Trap_OtherLocalChanges, {
                "OtherLocalChanges", "infiniband.trap.otherlocalchanges",
                FT_UINT8, BASE_HEX, NULL, 0x01, NULL, HFILL}
        },
        { &hf_infiniband_Trap_CAPABILITYMASK, {
                "CAPABILITYMASK", "infiniband.trap.capabilitymask",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_LinkSpeecEnabledChange, {
                "LinkSpeecEnabledChange", "infiniband.trap.linkspeecenabledchange",
                FT_UINT8, BASE_HEX, NULL, 0x04, NULL, HFILL}
        },
        { &hf_infiniband_Trap_LinkWidthEnabledChange, {
                "LinkWidthEnabledChange", "infiniband.trap.linkwidthenabledchange",
                FT_UINT8, BASE_HEX, NULL, 0x02, NULL, HFILL}
        },
        { &hf_infiniband_Trap_NodeDescriptionChange, {
                "NodeDescriptionChange", "infiniband.trap.nodedescriptionchange",
                FT_UINT8, BASE_HEX, NULL, 0x01, NULL, HFILL}
        },

        /* Trap 145 */
        { &hf_infiniband_Trap_SYSTEMIMAGEGUID, {
                "SYSTEMIMAGEGUID", "infiniband.trap.systemimageguid",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },

        /* Trap 256 */
        { &hf_infiniband_Trap_DRSLID, {
                "DRSLID", "infiniband.trap.drslid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_METHOD, {
                "METHOD", "infiniband.trap.method",
                FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_ATTRIBUTEID, {
                "ATTRIBUTEID", "infiniband.trap.attributeid",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_ATTRIBUTEMODIFIER, {
                "ATTRIBUTEMODIFIER", "infiniband.trap.attributemodifier",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_MKEY, {
                "MKEY", "infiniband.trap.mkey",
                FT_UINT64, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_DRNotice, {
                "DRNotice", "infiniband.trap.drnotice",
                FT_UINT8, BASE_HEX, NULL, 0x80, NULL, HFILL}
        },
        { &hf_infiniband_Trap_DRPathTruncated, {
                "DRPathTruncated", "infiniband.trap.drpathtruncated",
                FT_UINT8, BASE_HEX, NULL, 0x40, NULL, HFILL}
        },
        { &hf_infiniband_Trap_DRHopCount, {
                "DRHopCount", "infiniband.trap.drhopcount",
                FT_UINT8, BASE_HEX, NULL, 0x3F, NULL, HFILL}
        },
        { &hf_infiniband_Trap_DRNoticeReturnPath, {
                "DRNoticeReturnPath", "infiniband.trap.drnoticereturnpath",
                FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* Trap 257, 258 */
        { &hf_infiniband_Trap_LIDADDR1, {
                "LIDADDR1", "infiniband.trap.lidaddr1",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_LIDADDR2, {
                "LIDADDR2", "infiniband.trap.lidaddr2",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_KEY, {
                "KEY", "infiniband.trap.key",
                FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_SL, {
                "SL", "infiniband.trap.sl",
                FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_QP1, {
                "QP1", "infiniband.trap.qp1",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_QP2, {
                "QP2", "infiniband.trap.qp2",
                FT_UINT24, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_GIDADDR1, {
                "GIDADDR1", "infiniband.trap.gidaddr1",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_GIDADDR2, {
                "GIDADDR2", "infiniband.trap.gidaddr2",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },

        /* Trap 259 */
        { &hf_infiniband_Trap_DataValid, {
                "DataValid", "infiniband.trap.datavalid",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_PKEY, {
                "PKEY", "infiniband.trap.pkey",
                FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
        },
        { &hf_infiniband_Trap_SWLIDADDR, {
                "SWLIDADDR", "infiniband.trap.swlidaddr",
                FT_IPv6, BASE_NONE, NULL, 0x0, NULL, HFILL}
        },
        /* ClassPortInfo in Performance class */
        { &hf_infiniband_PerfMgt_ClassPortInfo, {
                "ClassPortInfo (Performance Management MAD)", "infiniband.classportinfo",
               FT_NONE, BASE_NONE, NULL, 0x0,
                "Performance class ClassPortInfo packet", HFILL}
        },
        /* PortCounters in Performance class */
        { &hf_infiniband_PortCounters, {
                "Port Counters (Performance Management MAD)", "infiniband.portcounters",
                FT_NONE, BASE_NONE, NULL, 0x0,
                "Performance class PortCounters packet", HFILL}
        },
        { &hf_infiniband_PortCounters_PortSelect, {
                "PortSelect", "infiniband.portcounters.portselect",
                FT_UINT8, BASE_HEX, NULL, 0x0,
                "Selects the port that will be accessed", HFILL}
        },
        { &hf_infiniband_PortCounters_CounterSelect, {
                "CounterSelect", "infiniband.portcounters.counterselect",
                FT_UINT16, BASE_HEX, NULL, 0x0,
                "When writing, selects which counters are affected by the operation", HFILL}
        },
        { &hf_infiniband_PortCounters_SymbolErrorCounter, {
                "SymbolErrorCounter", "infiniband.portcounters.symbolerrorcounter",
                FT_UINT16, BASE_DEC, NULL, 0x0,
                "Total number of minor link errors", HFILL}
        },
        { &hf_infiniband_PortCounters_LinkErrorRecoveryCounter, {
                "LinkErrorRecoveryCounter", "infiniband.portcounters.linkerrorrecoverycounter",
                FT_UINT8, BASE_DEC, NULL, 0x0,
                "Total number of times successfully completed link error recovery process", HFILL}
        },
        { &hf_infiniband_PortCounters_LinkDownedCounter, {
                "LinkDownedCounter", "infiniband.portcounters.linkdownedcounter",
                FT_UINT8, BASE_DEC, NULL, 0x0,
                "Total number of times failed link error recovery process", HFILL}
        },
        { &hf_infiniband_PortCounters_PortRcvErrors, {
                "PortRcvErrors", "infiniband.portcounters.portrcverrors",
                FT_UINT16, BASE_DEC, NULL, 0x0,
                "Total number of packets containing an error received", HFILL}
        },
        { &hf_infiniband_PortCounters_PortRcvRemotePhysicalErrors, {
                "PortRcvRemotePhysicalErrors", "infiniband.portcounters.portrcvremotephysicalerrors",
                FT_UINT16, BASE_DEC, NULL, 0x0,
                "Total number of packets marked with EBP delimiter received", HFILL}
        },
        { &hf_infiniband_PortCounters_PortRcvSwitchRelayErrors, {
                "PortRcvSwitchRelayErrors", "infiniband.portcounters.portrcvswitchrelayerrors",
                FT_UINT16, BASE_DEC, NULL, 0x0,
                "Total number of packets discarded because they could not be forwarded by switch relay",
                HFILL}
        },
        { &hf_infiniband_PortCounters_PortXmitDiscards, {
                "PortXmitDiscards", "infiniband.portcounters.portxmitdiscards",
                FT_UINT16, BASE_DEC, NULL, 0x0,
                "Total number of outbound packets discarded", HFILL}
        },
        { &hf_infiniband_PortCounters_PortXmitConstraintErrors, {
                "PortXmitConstraintErrors", "infiniband.portcounters.portxmitconstrainterrors",
                FT_UINT8, BASE_DEC, NULL, 0x0,
                "Total number of packets not transmitted from the switch physical port", HFILL}
        },
        { &hf_infiniband_PortCounters_PortRcvConstraintErrors, {
                "PortRcvConstraintErrors", "infiniband.portcounters.portrcvconstrainterrors",
                FT_UINT8, BASE_DEC, NULL, 0x0,
                "Total number of packets received on the switch physical port that are discarded", HFILL}
        },
        { &hf_infiniband_PortCounters_LocalLinkIntegrityErrors, {
                "LocalLinkIntegrityErrors", "infiniband.portcounters.locallinkintegrityerrors",
                FT_UINT8, BASE_DEC, NULL, 0x0,
                "The number of times the count of local physical errors exceeded the threshold specified by LocalPhyErrors",
                HFILL}
        },
        { &hf_infiniband_PortCounters_ExcessiveBufferOverrunErrors, {
                "ExcessiveBufferOverrunErrors", "infiniband.portcounters.excessivebufferoverrunerrors",
                FT_UINT8, BASE_DEC, NULL, 0x0,
                "The number of times that OverrunErrors consecutive flow control update periods occurred",
                HFILL}
        },
        { &hf_infiniband_PortCounters_VL15Dropped, {
                "VL15Dropped", "infiniband.portcounters.vl15dropped",
                FT_UINT16, BASE_DEC, NULL, 0x0,
                "Number of incoming VL15 packets dropped", HFILL}
        },
        { &hf_infiniband_PortCounters_PortXmitData, {
                "PortXmitData", "infiniband.portcounters.portxmitdata",
                FT_UINT32, BASE_DEC, NULL, 0x0,
                "Total number of data octets, divided by 4, transmitted on all VLs from the port", HFILL}
        },
        { &hf_infiniband_PortCounters_PortRcvData, {
                "PortRcvData", "infiniband.portcounters.portrcvdata",
                FT_UINT32, BASE_DEC, NULL, 0x0,
                "Total number of data octets, divided by 4, received on all VLs at the port", HFILL}
        },
        { &hf_infiniband_PortCounters_PortXmitPkts, {
                "PortXmitPkts", "infiniband.portcounters.portxmitpkts",
                FT_UINT32, BASE_DEC, NULL, 0x0,
                "Total number of packets transmitted on all VLs from the port", HFILL}
        },
        { &hf_infiniband_PortCounters_PortRcvPkts, {
                "PortRcvPkts", "infiniband.portcounters.portrcvpkts",
                FT_UINT32, BASE_DEC, NULL, 0x0,
                "Total number of packets received from all VLs on the port", HFILL}
        },
        /* PortCountersExtended in Performance class */
        { &hf_infiniband_PortCountersExt, {
                "Port Counters Extended (Performance Management MAD)", "infiniband.portcounters_ext",
                FT_NONE, BASE_NONE, NULL, 0x0,
                "Performance class PortCountersExtended packet", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortSelect, {
                "PortSelect", "infiniband.portcounters_ext.portselect",
                FT_UINT8, BASE_HEX, NULL, 0x0,
                "Selects the port that will be accessed", HFILL}
        },
        { &hf_infiniband_PortCountersExt_CounterSelect, {
                "CounterSelect", "infiniband.portcounters_ext.counterselect",
                FT_UINT16, BASE_HEX, NULL, 0x0,
                "When writing, selects which counters are affected by the operation", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortXmitData, {
                "PortXmitData", "infiniband.portcounters_ext.portxmitdata",
                FT_UINT64, BASE_DEC, NULL, 0x0,
                "Total number of data octets, divided by 4, transmitted on all VLs from the port", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortRcvData, {
                "PortRcvData", "infiniband.portcounters_ext.portrcvdata",
                FT_UINT64, BASE_DEC, NULL, 0x0,
                "Total number of data octets, divided by 4, received on all VLs at the port", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortXmitPkts, {
                "PortXmitPkts", "infiniband.portcounters_ext.portxmitpkts",
                FT_UINT64, BASE_DEC, NULL, 0x0,
                "Total number of packets transmitted on all VLs from the port", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortRcvPkts, {
                "PortRcvPkts", "infiniband.portcounters_ext.portrcvpkts",
                FT_UINT64, BASE_DEC, NULL, 0x0,
                "Total number of packets received from all VLs on the port", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortUnicastXmitPkts, {
                "PortUnicastXmitPkts", "infiniband.portcounters_ext.portunicastxmitpkts",
                FT_UINT64, BASE_DEC, NULL, 0x0,
                "Total number of unicast packets transmitted on all VLs from the port", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortUnicastRcvPkts, {
                "PortUnicastRcvPkts", "infiniband.portcounters_ext.portunicastrcvpkts",
                FT_UINT64, BASE_DEC, NULL, 0x0,
                "Total number of unicast packets received from all VLs on the port", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortMulticastXmitPkts, {
                "PortMulticastXmitPkts", "infiniband.portcounters_ext.portmulticastxmitpkts",
                FT_UINT64, BASE_DEC, NULL, 0x0,
                "Total number of multicast packets transmitted on all VLs from the port", HFILL}
        },
        { &hf_infiniband_PortCountersExt_PortMulticastRcvPkts, {
                "PortMulticastRcvPkts", "infiniband.portcounters_ext.portmulticastrcvpkts",
                FT_UINT64, BASE_DEC, NULL, 0x0,
                "Total number of multicast packets received from all VLs on the port", HFILL}
        }
    };

    /* Array to hold expansion options between dissections */
    static int *ett[] = {
    /*  &ett_infiniband,       */
        &ett_all_headers,
        &ett_lrh,
        &ett_grh,
        &ett_bth,
        &ett_rwh,
        &ett_rdeth,
        &ett_deth,
        &ett_reth,
        &ett_atomiceth,
        &ett_aeth,
        &ett_aeth_syndrome,
        &ett_atomicacketh,
        &ett_immdt,
        &ett_ieth,
        &ett_payload,
        &ett_vendor,
        &ett_subn_lid_routed,
        &ett_subn_directed_route,
        &ett_subnadmin,
        &ett_cm,
        &ett_cm_sid,
        &ett_cm_ipcm,
        &ett_mad,
        &ett_rmpp,
        &ett_subm_attribute,
        &ett_suba_attribute,
        &ett_datadetails,
        &ett_noticestraps,
    /*  &ett_nodedesc,         */
    /*  &ett_nodeinfo,         */
    /*  &ett_switchinfo,       */
    /*  &ett_guidinfo,         */
    /*  &ett_portinfo,         */
        &ett_portinfo_capmask,
        &ett_pkeytable,
        &ett_sltovlmapping,
        &ett_vlarbitrationtable,
        &ett_linearforwardingtable,
        &ett_randomforwardingtable,
        &ett_multicastforwardingtable,
        &ett_sminfo,
        &ett_vendordiag,
        &ett_ledinfo,
        &ett_linkspeedwidthpairs,
        &ett_informinfo,
        &ett_linkrecord,
        &ett_servicerecord,
        &ett_pathrecord,
        &ett_mcmemberrecord,
        &ett_tracerecord,
        &ett_multipathrecord,
        &ett_serviceassocrecord,
        &ett_perfclass,
    };

    static hf_register_info hf_link[] = {
        { &hf_infiniband_link_op, {
                "Operand", "infiniband_link.op",
                FT_UINT16, BASE_DEC, VALS(Operand_Description), 0xF000, NULL, HFILL}
        },
        { &hf_infiniband_link_fctbs, {
                "Flow Control Total Blocks Sent", "infiniband_link.fctbs",
                FT_UINT16, BASE_DEC, NULL, 0x0FFF, NULL, HFILL}
        },
        { &hf_infiniband_link_vl, {
                "Virtual Lane", "infiniband_link.vl",
                FT_UINT16, BASE_DEC, NULL, 0xF000, NULL, HFILL}
        },
        { &hf_infiniband_link_fccl, {
                "Flow Control Credit Limit", "infiniband_link.fccl",
                FT_UINT16, BASE_DEC, NULL, 0x0FFF, NULL, HFILL}
        },
        { &hf_infiniband_link_lpcrc, {
                "Link Packet CRC", "infiniband_link.lpcrc",
                FT_UINT16, BASE_HEX, NULL, 0, NULL, HFILL}
        }
    };

    static int *ett_link_array[] = {
        &ett_link
    };

    static hf_register_info hf_eoib[] = {
        /* Mellanox EoIB encapsulation header */
        { &hf_infiniband_ver, {
                "Version", "infiniband.eoib.version",
                FT_UINT16, BASE_HEX, NULL, MELLANOX_VERSION_FLAG, NULL, HFILL}
        },
        { &hf_infiniband_tcp_chk, {
                "TCP Checksum", "infiniband.eoib.tcp_chk",
                FT_UINT16, BASE_HEX, NULL, MELLANOX_TCP_CHECKSUM_FLAG, NULL, HFILL}
        },
        { &hf_infiniband_ip_chk, {
                "IP Checksum", "infiniband.eoib.ip_chk",
                FT_UINT16, BASE_HEX, NULL, MELLANOX_IP_CHECKSUM_FLAG, NULL, HFILL}
        },
        { &hf_infiniband_fcs, {
                "FCS Field Present", "infiniband.eoib.fcs",
                FT_BOOLEAN, 16, NULL, MELLANOX_FCS_PRESENT_FLAG, NULL, HFILL}
        },
        { &hf_infiniband_ms, {
                "More Segments to Follow", "infiniband.eoib.ms",
                FT_BOOLEAN, 16, NULL, MELLANOX_MORE_SEGMENT_FLAG, NULL, HFILL}
        },
        { &hf_infiniband_seg_off, {
                "Segment Offset", "infiniband.eoib.ip_seg_offset",
                FT_UINT16, BASE_DEC, NULL, MELLANOX_SEGMENT_FLAG, NULL, HFILL}
        },
        { &hf_infiniband_seg_id, {
                "Segment ID", "infiniband.eoib.ip_seg_id",
                FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL}
        },
    };

    static int *ett_eoib_array[] = {
        &ett_eoib
    };

    static hf_register_info hf_rc_send[] = {
        { &hf_infiniband_rc_send_fragments,
                { "Reassembled Infiniband RC Send Fragments", "infiniband.rc_send.fragments",
                FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL }},

        { &hf_infiniband_rc_send_fragment,
                { "Infiniband RC Send Fragment", "infiniband.rc_send.fragment",
                FT_FRAMENUM, BASE_NONE, NULL, 0, NULL, HFILL }},
        { &hf_infiniband_rc_send_fragment_overlap,
                { "Fragment overlap", "infiniband.rc_send.fragment.overlap",
                FT_BOOLEAN, BASE_NONE, NULL, 0, NULL, HFILL }},

        { &hf_infiniband_rc_send_fragment_overlap_conflict,
                { "Conflicting data in fragment overlap", "infiniband.rc_send.fragment.overlap.conflict",
                FT_BOOLEAN, BASE_NONE, NULL, 0, NULL, HFILL }},

        { &hf_infiniband_rc_send_fragment_multiple_tails,
                { "Multiple tail fragments found", "infiniband.rc_send.fragment.multipletails",
                FT_BOOLEAN, BASE_NONE, NULL, 0, NULL, HFILL }},

        { &hf_infiniband_rc_send_fragment_too_long_fragment,
                { "Fragment too long", "infiniband.rc_send.fragment.toolongfragment",
                FT_BOOLEAN, BASE_NONE, NULL, 0, NULL, HFILL }},

        { &hf_infiniband_rc_send_fragment_error,
                { "Defragmentation error", "infiniband.rc_send.fragment.error",
                FT_FRAMENUM, BASE_NONE, NULL, 0, NULL, HFILL }},

        { &hf_infiniband_rc_send_fragment_count,
                { "Fragment count", "infiniband.rc_send.fragment.count",
                FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL }},

        { &hf_infiniband_rc_send_reassembled_in,
                { "Reassembled PDU in frame", "infiniband.rc_send.reassembled_in",
                FT_FRAMENUM, BASE_NONE, NULL, 0, NULL, HFILL }},

        { &hf_infiniband_rc_send_reassembled_length,
                { "Reassembled Infiniband RC Send length", "infiniband.rc_send.reassembled.length",
                FT_UINT32, BASE_DEC, NULL, 0, NULL, HFILL }},

        { &hf_infiniband_rc_send_reassembled_data,
                { "Reassembled Infiniband RC Send data", "infiniband.rc_send.reassembled.data",
                FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL }},
    };

    static int *ett_rc_send_array[] = {
        &ett_infiniband_rc_send_fragment,
        &ett_infiniband_rc_send_fragments,
    };

    proto_infiniband = proto_register_protocol("InfiniBand", "IB", "infiniband");
    ib_handle = register_dissector("infiniband", dissect_infiniband, proto_infiniband);

    proto_register_field_array(proto_infiniband, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));

    /* register the subdissector tables */
    heur_dissectors_payload = register_heur_dissector_list_with_description("infiniband.payload", "InfiniBand payload", proto_infiniband);
    heur_dissectors_cm_private = register_heur_dissector_list_with_description("infiniband.mad.cm.private", "InfiniBand CM private data", proto_infiniband);

    /* register dissection preferences */
    infiniband_module = prefs_register_protocol(proto_infiniband, proto_reg_handoff_infiniband);

    prefs_register_obsolete_preference(infiniband_module, "identify_payload");
    prefs_register_obsolete_preference(infiniband_module, "dissect_eoib");
    prefs_register_uint_preference(infiniband_module, "rroce.port", "RRoce UDP Port",
                                   "The UDP port for RROCE messages (default " G_STRINGIFY(DEFAULT_RROCE_UDP_PORT) ")",
                                    10, &pref_rroce_udp_port);
    prefs_register_bool_preference(infiniband_module, "try_heuristic_first",
        "Try heuristic sub-dissectors first",
        "Try to decode a packet using an heuristic sub-dissector before using Decode As",
        &try_heuristic_first);

    proto_infiniband_link = proto_register_protocol("InfiniBand Link", "InfiniBand Link", "infiniband_link");
    ib_link_handle = register_dissector("infiniband_link", dissect_infiniband_link, proto_infiniband_link);

    proto_register_field_array(proto_infiniband_link, hf_link, array_length(hf_link));
    proto_register_subtree_array(ett_link_array, array_length(ett_link_array));

    proto_mellanox_eoib = proto_register_protocol("Mellanox EoIB Encapsulation Header", "Mellanox EoIB", "infiniband.eoib");
    proto_register_field_array(proto_infiniband, hf_eoib, array_length(hf_eoib));
    proto_register_subtree_array(ett_eoib_array, array_length(ett_eoib_array));

    /* initialize the hash table */
    CM_context_table = g_hash_table_new_full(g_int64_hash, g_int64_equal,
                                             table_destroy_notify, table_destroy_notify);

    subdissector_table = register_decode_as_next_proto(proto_infiniband, "infiniband", "Infiniband Payload",
                                                       infiniband_payload_prompt);

    /* RC Send reassembling */
    proto_register_field_array(proto_infiniband, hf_rc_send, array_length(hf_rc_send));
    proto_register_subtree_array(ett_rc_send_array, array_length(ett_rc_send_array));
    reassembly_table_register(&infiniband_rc_send_reassembly_table,
        &addresses_ports_reassembly_table_functions);

    register_shutdown_routine(infiniband_shutdown);
}

/* Reg Handoff.  Register dissectors we'll need for IPoIB and RoCE */
void proto_reg_handoff_infiniband(void)
{
    static bool initialized = false;
    static unsigned prev_rroce_udp_port;
    static dissector_handle_t rroce_handle;

    if (!initialized)
    {
        dissector_handle_t roce_handle;
        int proto_ethertype;

        ipv6_handle               = find_dissector_add_dependency("ipv6", proto_infiniband);

        /*
        * I haven't found an official spec for EoIB, but slide 10
        * of
        *
        *    http://downloads.openfabrics.org/Media/Sonoma2009/Sonoma_2009_Tues_converged-net-bridging.pdf
        *
        * shows the "Eth Payload" following the "Eth Header" and optional
        * "Vlan tag", and doesn't show an FCS; "Payload" generally
        * refers to the data transported by the protocol, which wouldn't
        * include the FCS.
        *
        * In addition, the capture attached to bug 5061 includes no
        * Ethernet FCS.
        *
        * So we assume the Ethernet frames carried by EoIB don't include
        * the Ethernet FCS.
        */
        eth_handle                = find_dissector("eth_withoutfcs");
        ethertype_dissector_table = find_dissector_table("ethertype");

        /* announce an anonymous Infiniband dissector */
        dissector_add_uint("erf.types.type", ERF_TYPE_INFINIBAND, ib_handle);

        /* announce an anonymous Infiniband dissector */
        dissector_add_uint("erf.types.type", ERF_TYPE_INFINIBAND_LINK, ib_link_handle);

        /* create and announce an anonymous RoCE dissector */
        roce_handle = create_dissector_handle(dissect_roce, proto_infiniband);
        dissector_add_uint("ethertype", ETHERTYPE_ROCE, roce_handle);

        /* create and announce an anonymous RRoCE dissector */
        rroce_handle = create_dissector_handle(dissect_rroce, proto_infiniband);

        /* RROCE over IPv4 isn't standardized but it's been seen in the wild */
        dissector_add_for_decode_as("ip.proto", rroce_handle);

        dissector_add_uint("wtap_encap", WTAP_ENCAP_INFINIBAND, ib_handle);
        heur_dissector_add("infiniband.payload", dissect_mellanox_eoib, "Mellanox EoIB", "mellanox_eoib", proto_mellanox_eoib, HEURISTIC_ENABLE);

        /* This could be put in the ethernet dissector file, but since there are a few Infiniband fields in the encapsulation,
           keep the dissector here, but associate it with ethernet */
        proto_ethertype = proto_get_id_by_filter_name("ethertype");
        heur_dissector_add("infiniband.payload", dissect_eth_over_ib, "Ethernet over IB", "eth_over_ib", proto_ethertype, HEURISTIC_ENABLE);

        initialized = true;
    }
    else
    {
        dissector_delete_uint("udp.port", prev_rroce_udp_port, rroce_handle);
    }
    /* We are saving the previous value of rroce udp port so we will be able to remove the dissector
     * the next time user pref is updated and we get called back to proto_reg_handoff_infiniband.
     * "Auto" preference not used because port isn't for the Infiniband protocol itself.
     */
    prev_rroce_udp_port = pref_rroce_udp_port;
    dissector_add_uint("udp.port", pref_rroce_udp_port, rroce_handle);
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
