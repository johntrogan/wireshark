/* Do not modify this file. Changes will be overwritten.                      */
/* Generated automatically by the ASN.1 to Wireshark dissector compiler       */
/* packet-p7.c                                                                */
/* asn2wrs.py -b -C -q -L -p p7 -c ./p7.cnf -s ./packet-p7-template -D . -O ../.. MSAbstractService.asn MSGeneralAttributeTypes.asn MSAccessProtocol.asn MSUpperBounds.asn */

/* packet-p7.c
 * Routines for X.413 (P7) packet dissection
 * Graeme Lunt 2007
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
#include <epan/oids.h>
#include <epan/asn1.h>
#include <epan/proto_data.h>
#include <wsutil/array.h>

#include "packet-ber.h"
#include "packet-acse.h"
#include "packet-ros.h"
#include "packet-rtse.h"
#include "packet-p7.h"

#include "packet-p1.h"
#include <epan/strutil.h>

#define PNAME  "X.413 Message Store Service"
#define PSNAME "P7"
#define PFNAME "p7"

void proto_register_p7(void);
void proto_reg_handoff_p7(void);

static int seqno;

/* Initialize the protocol and registered fields */
static int proto_p7;

#define op_ms_submission_control       2
#define op_ms_message_submission       3
#define op_ms_probe_submission         4
#define op_ms_cancel_deferred_delivery 7
#define op_summarize                   20
#define op_list                        21
#define op_fetch                       22
#define op_delete                      23
#define op_register_ms                 24
#define op_alert                       25
#define op_modify                      26
#define err_attribute_error            21
#define err_auto_action_request_error  22
#define err_delete_error               23
#define err_fetch_restriction_error    24
#define err_range_error                25
#define err_ub_security_error          26
#define err_service_error              27
#define err_sequence_number_error      28
#define err_invalid_parameters_error   29
#define err_message_group_error        30
#define err_ms_extension_error         31
#define err_register_ms_error          32
#define err_modify_error               33
#define err_entry_class_error          34
#define ub_alert_addresses             16
#define ub_attribute_values            32767
#define ub_attributes_supported        1024
#define ub_auto_action_errors          32767
#define ub_auto_actions                128
#define ub_auto_registrations          1024
#define ub_default_registrations       1024
#define ub_entry_classes               128
#define ub_entry_types                 16
#define ub_error_reasons               16
#define ub_extensions                  32
#define ub_group_depth                 64
#define ub_group_descriptor_length     256
#define ub_group_part_length           128
#define ub_information_bases           16
#define ub_matching_rules              1024
#define ub_message_groups              8192
#define ub_message_notes_length        1024
#define ub_messages                    2147483647
#define ub_modifications               32767
#define ub_msstring_match              512
#define ub_per_auto_action             32767
#define ub_per_entry                   1024
#define ub_service_information_length  2048
#define ub_summaries                   16
#define ub_supplementary_info_length   256
#define ub_ua_registration_identifier_length 32
#define ub_ua_registrations            128
#define ub_ua_restrictions             16

static int hf_p7_AutoActionType_PDU;              /* AutoActionType */
static int hf_p7_AutoActionError_PDU;             /* AutoActionError */
static int hf_p7_EntryType_PDU;                   /* EntryType */
static int hf_p7_SequenceNumber_PDU;              /* SequenceNumber */
static int hf_p7_RetrievalStatus_PDU;             /* RetrievalStatus */
static int hf_p7_MessageGroupName_PDU;            /* MessageGroupName */
static int hf_p7_MSBindArgument_PDU;              /* MSBindArgument */
static int hf_p7_MSBindResult_PDU;                /* MSBindResult */
static int hf_p7_MS_EIT_PDU;                      /* MS_EIT */
static int hf_p7_ChangeCredentialsAlgorithms_PDU;  /* ChangeCredentialsAlgorithms */
static int hf_p7_PAR_ms_bind_error_PDU;           /* PAR_ms_bind_error */
static int hf_p7_CreationTime_PDU;                /* CreationTime */
static int hf_p7_OriginatorToken_PDU;             /* OriginatorToken */
static int hf_p7_SummarizeArgument_PDU;           /* SummarizeArgument */
static int hf_p7_SummarizeResult_PDU;             /* SummarizeResult */
static int hf_p7_ListArgument_PDU;                /* ListArgument */
static int hf_p7_ListResult_PDU;                  /* ListResult */
static int hf_p7_FetchArgument_PDU;               /* FetchArgument */
static int hf_p7_FetchResult_PDU;                 /* FetchResult */
static int hf_p7_DeleteArgument_PDU;              /* DeleteArgument */
static int hf_p7_DeleteResult_PDU;                /* DeleteResult */
static int hf_p7_Register_MSArgument_PDU;         /* Register_MSArgument */
static int hf_p7_Register_MSResult_PDU;           /* Register_MSResult */
static int hf_p7_ProtectedChangeCredentials_PDU;  /* ProtectedChangeCredentials */
static int hf_p7_AlertArgument_PDU;               /* AlertArgument */
static int hf_p7_AlertResult_PDU;                 /* AlertResult */
static int hf_p7_ModifyArgument_PDU;              /* ModifyArgument */
static int hf_p7_ModifyResult_PDU;                /* ModifyResult */
static int hf_p7_MSMessageSubmissionArgument_PDU;  /* MSMessageSubmissionArgument */
static int hf_p7_MSMessageSubmissionResult_PDU;   /* MSMessageSubmissionResult */
static int hf_p7_MSProbeSubmissionArgument_PDU;   /* MSProbeSubmissionArgument */
static int hf_p7_MSProbeSubmissionResult_PDU;     /* MSProbeSubmissionResult */
static int hf_p7_PAR_attribute_error_PDU;         /* PAR_attribute_error */
static int hf_p7_PAR_auto_action_request_error_PDU;  /* PAR_auto_action_request_error */
static int hf_p7_PAR_delete_error_PDU;            /* PAR_delete_error */
static int hf_p7_PAR_fetch_restriction_error_PDU;  /* PAR_fetch_restriction_error */
static int hf_p7_PAR_invalid_parameters_error_PDU;  /* PAR_invalid_parameters_error */
static int hf_p7_PAR_range_error_PDU;             /* PAR_range_error */
static int hf_p7_PAR_sequence_number_error_PDU;   /* PAR_sequence_number_error */
static int hf_p7_ServiceErrorParameter_PDU;       /* ServiceErrorParameter */
static int hf_p7_MessageGroupErrorParameter_PDU;  /* MessageGroupErrorParameter */
static int hf_p7_MSExtensionErrorParameter_PDU;   /* MSExtensionErrorParameter */
static int hf_p7_PAR_register_ms_error_PDU;       /* PAR_register_ms_error */
static int hf_p7_ModifyErrorParameter_PDU;        /* ModifyErrorParameter */
static int hf_p7_EntryClassErrorParameter_PDU;    /* EntryClassErrorParameter */
static int hf_p7_ReportLocation_PDU;              /* ReportLocation */
static int hf_p7_PerRecipientReport_PDU;          /* PerRecipientReport */
static int hf_p7_ReportSummary_PDU;               /* ReportSummary */
static int hf_p7_DeferredDeliveryCancellationTime_PDU;  /* DeferredDeliveryCancellationTime */
static int hf_p7_DeletionTime_PDU;                /* DeletionTime */
static int hf_p7_SubmissionError_PDU;             /* SubmissionError */
static int hf_p7_SignatureVerificationStatus_PDU;  /* SignatureVerificationStatus */
static int hf_p7_StoragePeriod_PDU;               /* StoragePeriod */
static int hf_p7_StorageTime_PDU;                 /* StorageTime */
static int hf_p7_RTSE_apdus_PDU;                  /* RTSE_apdus */
static int hf_p7_attribute_type;                  /* AttributeType */
static int hf_p7_attribute_values;                /* AttributeValues */
static int hf_p7_attribute_values_item;           /* AttributeItem */
static int hf_p7_auto_action_type;                /* AutoActionType */
static int hf_p7_registration_identifier;         /* INTEGER_1_ub_per_auto_action */
static int hf_p7_registration_parameter;          /* T_registration_parameter */
static int hf_p7_error_code;                      /* T_error_code */
static int hf_p7_error_parameter;                 /* T_error_parameter */
static int hf_p7_MSExtensions_item;               /* MSExtensionItem */
static int hf_p7_MessageGroupName_item;           /* GroupNamePart */
static int hf_p7_initiator_name;                  /* T_initiator_name */
static int hf_p7_initiator_credentials;           /* InitiatorCredentials */
static int hf_p7_security_context;                /* SecurityContext */
static int hf_p7_fetch_restrictions;              /* Restrictions */
static int hf_p7_ms_configuration_request;        /* BOOLEAN */
static int hf_p7_ua_registration_identifier;      /* RegistrationIdentifier */
static int hf_p7_bind_extensions;                 /* MSExtensions */
static int hf_p7_allowed_content_types;           /* T_allowed_content_types */
static int hf_p7_allowed_content_types_item;      /* OBJECT_IDENTIFIER */
static int hf_p7_allowed_EITs;                    /* MS_EITs */
static int hf_p7_maximum_attribute_length;        /* INTEGER */
static int hf_p7_MS_EITs_item;                    /* MS_EIT */
static int hf_p7_responder_credentials;           /* ResponderCredentials */
static int hf_p7_available_auto_actions;          /* SET_SIZE_1_ub_auto_actions_OF_AutoActionType */
static int hf_p7_available_auto_actions_item;     /* AutoActionType */
static int hf_p7_available_attribute_types;       /* SET_SIZE_1_ub_attributes_supported_OF_AttributeType */
static int hf_p7_available_attribute_types_item;  /* AttributeType */
static int hf_p7_alert_indication;                /* BOOLEAN */
static int hf_p7_content_types_supported;         /* T_content_types_supported */
static int hf_p7_content_types_supported_item;    /* OBJECT_IDENTIFIER */
static int hf_p7_entry_classes_supported;         /* SET_SIZE_1_ub_entry_classes_OF_EntryClass */
static int hf_p7_entry_classes_supported_item;    /* EntryClass */
static int hf_p7_matching_rules_supported;        /* T_matching_rules_supported */
static int hf_p7_matching_rules_supported_item;   /* OBJECT_IDENTIFIER */
static int hf_p7_bind_result_extensions;          /* MSExtensions */
static int hf_p7_message_group_depth;             /* INTEGER_1_ub_group_depth */
static int hf_p7_auto_action_error_indication;    /* AutoActionErrorIndication */
static int hf_p7_unsupported_extensions;          /* T_unsupported_extensions */
static int hf_p7_unsupported_extensions_item;     /* OBJECT_IDENTIFIER */
static int hf_p7_ua_registration_id_unknown;      /* BOOLEAN */
static int hf_p7_service_information;             /* GeneralString_SIZE_1_ub_service_information_length */
static int hf_p7_ChangeCredentialsAlgorithms_item;  /* OBJECT_IDENTIFIER */
static int hf_p7_indication_only;                 /* NULL */
static int hf_p7_auto_action_log_entry;           /* SequenceNumber */
static int hf_p7_unqualified_error;               /* BindProblem */
static int hf_p7_qualified_error;                 /* T_qualified_error */
static int hf_p7_bind_problem;                    /* BindProblem */
static int hf_p7_supplementary_information;       /* GeneralString_SIZE_1_ub_supplementary_info_length */
static int hf_p7_bind_extension_errors;           /* T_bind_extension_errors */
static int hf_p7_bind_extension_errors_item;      /* OBJECT_IDENTIFIER */
static int hf_p7_sequence_number_range;           /* NumberRange */
static int hf_p7_creation_time_range;             /* TimeRange */
static int hf_p7_from_number;                     /* T_from_number */
static int hf_p7_to_number;                       /* T_to_number */
static int hf_p7_from_time;                       /* CreationTime */
static int hf_p7_to_time;                         /* CreationTime */
static int hf_p7_filter_item;                     /* FilterItem */
static int hf_p7_and;                             /* SET_OF_Filter */
static int hf_p7_and_item;                        /* Filter */
static int hf_p7_or;                              /* SET_OF_Filter */
static int hf_p7_or_item;                         /* Filter */
static int hf_p7_not;                             /* Filter */
static int hf_p7_equality;                        /* AttributeValueAssertion */
static int hf_p7_substrings;                      /* T_substrings */
static int hf_p7_type;                            /* AttributeType */
static int hf_p7_strings;                         /* T_strings */
static int hf_p7_strings_item;                    /* T_strings_item */
static int hf_p7_initial;                         /* T_initial */
static int hf_p7_any;                             /* T_any */
static int hf_p7_final;                           /* T_final */
static int hf_p7_greater_or_equal;                /* AttributeValueAssertion */
static int hf_p7_less_or_equal;                   /* AttributeValueAssertion */
static int hf_p7_present;                         /* AttributeType */
static int hf_p7_approximate_match;               /* AttributeValueAssertion */
static int hf_p7_other_match;                     /* MatchingRuleAssertion */
static int hf_p7_matching_rule;                   /* OBJECT_IDENTIFIER */
static int hf_p7_match_value;                     /* T_match_value */
static int hf_p7_attribute_value;                 /* T_attribute_value */
static int hf_p7_child_entries;                   /* BOOLEAN */
static int hf_p7_range;                           /* Range */
static int hf_p7_filter;                          /* Filter */
static int hf_p7_limit;                           /* INTEGER_1_ub_messages */
static int hf_p7_override;                        /* OverrideRestrictions */
static int hf_p7_EntryInformationSelection_item;  /* AttributeSelection */
static int hf_p7_from;                            /* INTEGER_1_ub_attribute_values */
static int hf_p7_selection_count;                 /* INTEGER_0_ub_attribute_values */
static int hf_p7_sequence_number;                 /* SequenceNumber */
static int hf_p7_attributes;                      /* SET_SIZE_1_ub_per_entry_OF_Attribute */
static int hf_p7_attributes_item;                 /* Attribute */
static int hf_p7_value_count_exceeded;            /* SET_SIZE_1_ub_per_entry_OF_AttributeValueCount */
static int hf_p7_value_count_exceeded_item;       /* AttributeValueCount */
static int hf_p7_total;                           /* INTEGER */
static int hf_p7_object_entry_class;              /* EntryClass */
static int hf_p7_disable_auto_modify;             /* BOOLEAN */
static int hf_p7_add_message_group_names;         /* SET_SIZE_1_ub_message_groups_OF_MessageGroupName */
static int hf_p7_add_message_group_names_item;    /* MessageGroupName */
static int hf_p7_ms_submission_extensions;        /* MSExtensions */
static int hf_p7_created_entry;                   /* SequenceNumber */
static int hf_p7_ms_submission_result_extensions;  /* MSExtensions */
static int hf_p7_entry_class;                     /* EntryClass */
static int hf_p7_selector;                        /* Selector */
static int hf_p7_summary_requests;                /* SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType */
static int hf_p7_summary_requests_item;           /* AttributeType */
static int hf_p7_summarize_extensions;            /* MSExtensions */
static int hf_p7_next;                            /* SequenceNumber */
static int hf_p7_count;                           /* T_count */
static int hf_p7_span;                            /* Span */
static int hf_p7_summaries;                       /* SEQUENCE_SIZE_1_ub_summaries_OF_Summary */
static int hf_p7_summaries_item;                  /* Summary */
static int hf_p7_summarize_result_extensions;     /* MSExtensions */
static int hf_p7_lowest;                          /* SequenceNumber */
static int hf_p7_highest;                         /* SequenceNumber */
static int hf_p7_absent;                          /* INTEGER_1_ub_messages */
static int hf_p7_summary_present;                 /* T_summary_present */
static int hf_p7_summary_present_item;            /* T_summary_present_item */
static int hf_p7_value;                           /* SummaryPresentItemValue */
static int hf_p7_summary_count;                   /* INTEGER_1_ub_messages */
static int hf_p7_requested_attributes;            /* EntryInformationSelection */
static int hf_p7_list_extensions;                 /* MSExtensions */
static int hf_p7_requested;                       /* SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation */
static int hf_p7_requested_item;                  /* EntryInformation */
static int hf_p7_list_result_extensions;          /* MSExtensions */
static int hf_p7_item;                            /* T_item */
static int hf_p7_search;                          /* Selector */
static int hf_p7_precise;                         /* SequenceNumber */
static int hf_p7_fetch_extensions;                /* MSExtensions */
static int hf_p7_entry_information;               /* EntryInformation */
static int hf_p7_list;                            /* SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber */
static int hf_p7_list_item;                       /* SequenceNumber */
static int hf_p7_fetch_result_extensions;         /* MSExtensions */
static int hf_p7_items;                           /* T_items */
static int hf_p7_sequence_numbers;                /* SET_SIZE_1_ub_messages_OF_SequenceNumber */
static int hf_p7_sequence_numbers_item;           /* SequenceNumber */
static int hf_p7_delete_extensions;               /* MSExtensions */
static int hf_p7_delete_result_88;                /* NULL */
static int hf_p7_delete_result_94;                /* T_delete_result_94 */
static int hf_p7_entries_deleted_94;              /* SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber */
static int hf_p7_entries_deleted_94_item;         /* SequenceNumber */
static int hf_p7_delete_result_extensions;        /* MSExtensions */
static int hf_p7_auto_action_registrations;       /* SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration */
static int hf_p7_auto_action_registrations_item;  /* AutoActionRegistration */
static int hf_p7_auto_action_deregistrations;     /* SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration */
static int hf_p7_auto_action_deregistrations_item;  /* AutoActionDeregistration */
static int hf_p7_list_attribute_defaults;         /* SET_SIZE_0_ub_default_registrations_OF_AttributeType */
static int hf_p7_list_attribute_defaults_item;    /* AttributeType */
static int hf_p7_fetch_attribute_defaults;        /* SET_SIZE_0_ub_default_registrations_OF_AttributeType */
static int hf_p7_fetch_attribute_defaults_item;   /* AttributeType */
static int hf_p7_change_credentials;              /* T_change_credentials */
static int hf_p7_register_old_credentials;        /* Credentials */
static int hf_p7_new_credentials;                 /* Credentials */
static int hf_p7_user_security_labels;            /* SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel */
static int hf_p7_user_security_labels_item;       /* SecurityLabel */
static int hf_p7_ua_registrations;                /* SET_SIZE_1_ub_ua_registrations_OF_UARegistration */
static int hf_p7_ua_registrations_item;           /* UARegistration */
static int hf_p7_submission_defaults;             /* MSSubmissionOptions */
static int hf_p7_message_group_registrations;     /* MessageGroupRegistrations */
static int hf_p7_registration_status_request;     /* RegistrationTypes */
static int hf_p7_register_ms_extensions;          /* MSExtensions */
static int hf_p7_ua_list_attribute_defaults;      /* SET_SIZE_0_ub_default_registrations_OF_AttributeType */
static int hf_p7_ua_list_attribute_defaults_item;  /* AttributeType */
static int hf_p7_ua_fetch_attribute_defaults;     /* SET_SIZE_0_ub_default_registrations_OF_AttributeType */
static int hf_p7_ua_fetch_attribute_defaults_item;  /* AttributeType */
static int hf_p7_ua_submission_defaults;          /* MSSubmissionOptions */
static int hf_p7_content_specific_defaults;       /* MSExtensions */
static int hf_p7_MessageGroupRegistrations_item;  /* MessageGroupRegistrations_item */
static int hf_p7_register_group;                  /* MessageGroupNameAndDescriptor */
static int hf_p7_deregister_group;                /* MessageGroupName */
static int hf_p7_change_descriptors;              /* MessageGroupNameAndDescriptor */
static int hf_p7_message_group_name;              /* MessageGroupName */
static int hf_p7_message_group_descriptor;        /* GeneralString_SIZE_1_ub_group_descriptor_length */
static int hf_p7_registrations;                   /* T_registrations */
static int hf_p7_extended_registrations;          /* T_extended_registrations */
static int hf_p7_extended_registrations_item;     /* T_extended_registrations_item */
static int hf_p7_restrict_message_groups;         /* MessageGroupsRestriction */
static int hf_p7_parent_group;                    /* MessageGroupName */
static int hf_p7_immediate_descendants_only;      /* BOOLEAN */
static int hf_p7_omit_descriptors;                /* BOOLEAN */
static int hf_p7_algorithm_identifier;            /* OBJECT_IDENTIFIER */
static int hf_p7_old_credentials;                 /* InitiatorCredentials */
static int hf_p7_password_delta;                  /* BIT_STRING */
static int hf_p7_no_status_information;           /* NULL */
static int hf_p7_registered_information;          /* T_registered_information */
static int hf_p7_registered_list_attribute_defaults;  /* SET_SIZE_1_ub_default_registrations_OF_AttributeType */
static int hf_p7_registered_list_attribute_defaults_item;  /* AttributeType */
static int hf_p7_registered_fetch_attribute_defaults;  /* SET_SIZE_1_ub_default_registrations_OF_AttributeType */
static int hf_p7_registered_fetch_attribute_defaults_item;  /* AttributeType */
static int hf_p7_registered_message_group_registrations;  /* SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor */
static int hf_p7_registered_message_group_registrations_item;  /* MessageGroupNameAndDescriptor */
static int hf_p7_register_ms_result_extensions;   /* MSExtensions */
static int hf_p7_alert_registration_identifier;   /* INTEGER_1_ub_auto_actions */
static int hf_p7_new_entry;                       /* EntryInformation */
static int hf_p7_entries;                         /* T_entries */
static int hf_p7_specific_entries;                /* SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber */
static int hf_p7_specific_entries_item;           /* SequenceNumber */
static int hf_p7_modifications;                   /* SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification */
static int hf_p7_modifications_item;              /* EntryModification */
static int hf_p7_modify_extensions;               /* MSExtensions */
static int hf_p7_strict;                          /* BOOLEAN */
static int hf_p7_modification;                    /* T_modification */
static int hf_p7_add_attribute;                   /* Attribute */
static int hf_p7_remove_attribute;                /* AttributeType */
static int hf_p7_add_values;                      /* OrderedAttribute */
static int hf_p7_remove_values;                   /* OrderedAttribute */
static int hf_p7_ordered_attribute_values;        /* OrderedAttributeValues */
static int hf_p7_ordered_attribute_values_item;   /* OrderedAttributeItem */
static int hf_p7_ordered_attribute_value;         /* OrderedAttributeValue */
static int hf_p7_ordered_position;                /* INTEGER_1_ub_attribute_values */
static int hf_p7_entries_modified;                /* SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber */
static int hf_p7_entries_modified_item;           /* SequenceNumber */
static int hf_p7_modify_result_extensions;        /* MSExtensions */
static int hf_p7_envelope;                        /* MessageSubmissionEnvelope */
static int hf_p7_content;                         /* Content */
static int hf_p7_submission_options;              /* MSSubmissionOptions */
static int hf_p7_mts_result;                      /* T_mts_result */
static int hf_p7_message_submission_identifier;   /* MessageSubmissionIdentifier */
static int hf_p7_message_submission_time;         /* MessageSubmissionTime */
static int hf_p7_content_identifier;              /* ContentIdentifier */
static int hf_p7_extensions;                      /* SET_OF_ExtensionField */
static int hf_p7_extensions_item;                 /* ExtensionField */
static int hf_p7_ms_message_result;               /* CommonSubmissionResults */
static int hf_p7_store_draft_result;              /* CommonSubmissionResults */
static int hf_p7_originator_name;                 /* OriginatorName */
static int hf_p7_original_encoded_information_types;  /* OriginalEncodedInformationTypes */
static int hf_p7_content_type;                    /* ContentType */
static int hf_p7_content_length;                  /* ContentLength */
static int hf_p7_per_message_indicators;          /* PerMessageIndicators */
static int hf_p7_per_recipient_fields;            /* SEQUENCE_OF_PerRecipientProbeSubmissionFields */
static int hf_p7_per_recipient_fields_item;       /* PerRecipientProbeSubmissionFields */
static int hf_p7_probe_submission_identifier;     /* ProbeSubmissionIdentifier */
static int hf_p7_probe_submission_time;           /* ProbeSubmissionTime */
static int hf_p7_ms_probe_result;                 /* CommonSubmissionResults */
static int hf_p7_attribute_problems;              /* AttributeProblems */
static int hf_p7_attribute_problem_item;          /* AttributeProblemItem */
static int hf_p7_attribute_problem;               /* AttributeProblem */
static int hf_p7_attr_value;                      /* T_attr_value */
static int hf_p7_auto_action_request_problems;    /* AutoActionRequestProblems */
static int hf_p7_auto_action_request_problem_item;  /* AutoActionRequestProblemItem */
static int hf_p7_auto_action_request_problem;     /* AutoActionRequestProblem */
static int hf_p7_delete_problems;                 /* DeleteProblems */
static int hf_p7_delete_problem_item;             /* DeleteProblemItem */
static int hf_p7_delete_problem;                  /* DeleteProblem */
static int hf_p7_entries_deleted;                 /* SET_SIZE_1_ub_messages_OF_SequenceNumber */
static int hf_p7_entries_deleted_item;            /* SequenceNumber */
static int hf_p7_fetch_restriction_problems;      /* FetchRestrictionProblems */
static int hf_p7_fetch_restriction_problem_item;  /* FetchRestrictionProblemItem */
static int hf_p7_fetch_restriction_problem;       /* FetchRestrictionProblem */
static int hf_p7_restriction;                     /* T_restriction */
static int hf_p7_extended_content_type;           /* OBJECT_IDENTIFIER */
static int hf_p7_eit;                             /* MS_EITs */
static int hf_p7_attribute_length;                /* INTEGER */
static int hf_p7_range_problem;                   /* RangeProblem */
static int hf_p7_sequence_number_problems;        /* SequenceNumberProblems */
static int hf_p7_sequence_number_problem_item;    /* SequenceNumberProblemItem */
static int hf_p7_sequence_number_problem;         /* SequenceNumberProblem */
static int hf_p7_service_problem;                 /* ServiceProblem */
static int hf_p7_message_group_problem;           /* MessageGroupProblem */
static int hf_p7_name;                            /* MessageGroupName */
static int hf_p7_ms_extension_problem;            /* MSExtensionItem */
static int hf_p7_unknown_ms_extension;            /* OBJECT_IDENTIFIER */
static int hf_p7_register_ms_problem;             /* RegistrationProblem */
static int hf_p7_registration_type;               /* RegistrationTypes */
static int hf_p7_failing_entry;                   /* SequenceNumber */
static int hf_p7_modification_number;             /* INTEGER */
static int hf_p7_modify_problem;                  /* ModifyProblem */
static int hf_p7_entry_class_problem;             /* T_entry_class_problem */
static int hf_p7_no_correlated_reports;           /* NULL */
static int hf_p7_location;                        /* SEQUENCE_OF_PerRecipientReport */
static int hf_p7_location_item;                   /* PerRecipientReport */
static int hf_p7_report_entry;                    /* SequenceNumber */
static int hf_p7_position;                        /* INTEGER_1_ub_recipients */
static int hf_p7_submission_control_violated;     /* NULL */
static int hf_p7_originator_invalid;              /* NULL */
static int hf_p7_recipient_improperly_specified;  /* ImproperlySpecifiedRecipients */
static int hf_p7_element_of_service_not_subscribed;  /* NULL */
static int hf_p7_inconsistent_request;            /* NULL */
static int hf_p7_security_error;                  /* SecurityProblem */
static int hf_p7_unsupported_critical_function;   /* NULL */
static int hf_p7_remote_bind_error;               /* NULL */
static int hf_p7_service_error;                   /* ServiceErrorParameter */
static int hf_p7_message_group_error;             /* MessageGroupErrorParameter */
static int hf_p7_ms_extension_error;              /* MSExtensionErrorParameter */
static int hf_p7_entry_class_error;               /* EntryClassErrorParameter */
static int hf_p7_content_integrity_check;         /* SignatureStatus */
static int hf_p7_message_origin_authentication_check;  /* SignatureStatus */
static int hf_p7_message_token;                   /* SignatureStatus */
static int hf_p7_report_origin_authentication_check;  /* SignatureStatus */
static int hf_p7_proof_of_delivery;               /* SignatureStatus */
static int hf_p7_proof_of_submission;             /* SignatureStatus */
static int hf_p7_rtorq_apdu;                      /* RTORQapdu */
static int hf_p7_rtoac_apdu;                      /* RTOACapdu */
static int hf_p7_rtorj_apdu;                      /* RTORJapdu */
static int hf_p7_rttp_apdu;                       /* RTTPapdu */
static int hf_p7_rttr_apdu;                       /* RTTRapdu */
static int hf_p7_rtab_apdu;                       /* RTABapdu */
static int hf_p7_abortReason;                     /* AbortReason */
static int hf_p7_reflectedParameter;              /* BIT_STRING */
static int hf_p7_userdataAB;                      /* T_userdataAB */
/* named bits */
static int hf_p7_OverrideRestrictions_override_content_types_restriction;
static int hf_p7_OverrideRestrictions_override_EITs_restriction;
static int hf_p7_OverrideRestrictions_override_attribute_length_restriction;
static int hf_p7_T_registrations_auto_action_registrations;
static int hf_p7_T_registrations_list_attribute_defaults;
static int hf_p7_T_registrations_fetch_attribute_defaults;
static int hf_p7_T_registrations_ua_registrations;
static int hf_p7_T_registrations_submission_defaults;
static int hf_p7_T_registrations_message_group_registrations;
static int hf_p7_T_entry_class_problem_unsupported_entry_class;
static int hf_p7_T_entry_class_problem_entry_class_not_subscribed;
static int hf_p7_T_entry_class_problem_inappropriate_entry_class;

/* Initialize the subtree pointers */
static int ett_p7;
static int ett_p7_Attribute;
static int ett_p7_AttributeValues;
static int ett_p7_AutoActionRegistration;
static int ett_p7_AutoActionError;
static int ett_p7_MSExtensions;
static int ett_p7_MessageGroupName;
static int ett_p7_MSBindArgument;
static int ett_p7_Restrictions;
static int ett_p7_T_allowed_content_types;
static int ett_p7_MS_EITs;
static int ett_p7_MSBindResult;
static int ett_p7_SET_SIZE_1_ub_auto_actions_OF_AutoActionType;
static int ett_p7_SET_SIZE_1_ub_attributes_supported_OF_AttributeType;
static int ett_p7_T_content_types_supported;
static int ett_p7_SET_SIZE_1_ub_entry_classes_OF_EntryClass;
static int ett_p7_T_matching_rules_supported;
static int ett_p7_T_unsupported_extensions;
static int ett_p7_ChangeCredentialsAlgorithms;
static int ett_p7_AutoActionErrorIndication;
static int ett_p7_PAR_ms_bind_error;
static int ett_p7_T_qualified_error;
static int ett_p7_T_bind_extension_errors;
static int ett_p7_Range;
static int ett_p7_NumberRange;
static int ett_p7_TimeRange;
static int ett_p7_Filter;
static int ett_p7_SET_OF_Filter;
static int ett_p7_FilterItem;
static int ett_p7_T_substrings;
static int ett_p7_T_strings;
static int ett_p7_T_strings_item;
static int ett_p7_MatchingRuleAssertion;
static int ett_p7_AttributeValueAssertion;
static int ett_p7_Selector;
static int ett_p7_OverrideRestrictions;
static int ett_p7_EntryInformationSelection;
static int ett_p7_AttributeSelection;
static int ett_p7_EntryInformation;
static int ett_p7_SET_SIZE_1_ub_per_entry_OF_Attribute;
static int ett_p7_SET_SIZE_1_ub_per_entry_OF_AttributeValueCount;
static int ett_p7_AttributeValueCount;
static int ett_p7_MSSubmissionOptions;
static int ett_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupName;
static int ett_p7_CommonSubmissionResults;
static int ett_p7_SummarizeArgument;
static int ett_p7_SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType;
static int ett_p7_SummarizeResult;
static int ett_p7_SEQUENCE_SIZE_1_ub_summaries_OF_Summary;
static int ett_p7_Span;
static int ett_p7_Summary;
static int ett_p7_T_summary_present;
static int ett_p7_T_summary_present_item;
static int ett_p7_ListArgument;
static int ett_p7_ListResult;
static int ett_p7_SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation;
static int ett_p7_FetchArgument;
static int ett_p7_T_item;
static int ett_p7_FetchResult;
static int ett_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber;
static int ett_p7_DeleteArgument;
static int ett_p7_T_items;
static int ett_p7_SET_SIZE_1_ub_messages_OF_SequenceNumber;
static int ett_p7_DeleteResult;
static int ett_p7_T_delete_result_94;
static int ett_p7_Register_MSArgument;
static int ett_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration;
static int ett_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration;
static int ett_p7_SET_SIZE_0_ub_default_registrations_OF_AttributeType;
static int ett_p7_T_change_credentials;
static int ett_p7_SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel;
static int ett_p7_SET_SIZE_1_ub_ua_registrations_OF_UARegistration;
static int ett_p7_AutoActionDeregistration;
static int ett_p7_UARegistration;
static int ett_p7_MessageGroupRegistrations;
static int ett_p7_MessageGroupRegistrations_item;
static int ett_p7_MessageGroupNameAndDescriptor;
static int ett_p7_RegistrationTypes;
static int ett_p7_T_registrations;
static int ett_p7_T_extended_registrations;
static int ett_p7_MessageGroupsRestriction;
static int ett_p7_ProtectedChangeCredentials;
static int ett_p7_Register_MSResult;
static int ett_p7_T_registered_information;
static int ett_p7_SET_SIZE_1_ub_default_registrations_OF_AttributeType;
static int ett_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor;
static int ett_p7_AlertArgument;
static int ett_p7_ModifyArgument;
static int ett_p7_T_entries;
static int ett_p7_SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification;
static int ett_p7_EntryModification;
static int ett_p7_T_modification;
static int ett_p7_OrderedAttribute;
static int ett_p7_OrderedAttributeValues;
static int ett_p7_OrderedAttributeItem;
static int ett_p7_ModifyResult;
static int ett_p7_MSMessageSubmissionArgument;
static int ett_p7_MSMessageSubmissionResult;
static int ett_p7_T_mts_result;
static int ett_p7_SET_OF_ExtensionField;
static int ett_p7_MSProbeSubmissionArgument;
static int ett_p7_SEQUENCE_OF_PerRecipientProbeSubmissionFields;
static int ett_p7_MSProbeSubmissionResult;
static int ett_p7_PAR_attribute_error;
static int ett_p7_AttributeProblems;
static int ett_p7_AttributeProblemItem;
static int ett_p7_PAR_auto_action_request_error;
static int ett_p7_AutoActionRequestProblems;
static int ett_p7_AutoActionRequestProblemItem;
static int ett_p7_PAR_delete_error;
static int ett_p7_DeleteProblems;
static int ett_p7_DeleteProblemItem;
static int ett_p7_PAR_fetch_restriction_error;
static int ett_p7_FetchRestrictionProblems;
static int ett_p7_FetchRestrictionProblemItem;
static int ett_p7_T_restriction;
static int ett_p7_PAR_range_error;
static int ett_p7_PAR_sequence_number_error;
static int ett_p7_SequenceNumberProblems;
static int ett_p7_SequenceNumberProblemItem;
static int ett_p7_ServiceErrorParameter;
static int ett_p7_MessageGroupErrorParameter;
static int ett_p7_MSExtensionErrorParameter;
static int ett_p7_PAR_register_ms_error;
static int ett_p7_ModifyErrorParameter;
static int ett_p7_EntryClassErrorParameter;
static int ett_p7_T_entry_class_problem;
static int ett_p7_ReportLocation;
static int ett_p7_SEQUENCE_OF_PerRecipientReport;
static int ett_p7_PerRecipientReport;
static int ett_p7_SubmissionError;
static int ett_p7_SignatureVerificationStatus;
static int ett_p7_RTSE_apdus;
static int ett_p7_RTABapdu;


/* P7 ABSTRACT-OPERATIONS */
static const value_string p7_opr_code_string_vals[] = {
	{ op_ros_bind, "ms_bind" },
	{ op_summarize, "summarize" },
	{ op_list, "list" },
	{ op_fetch, "fetch" },
	{ op_delete, "delete" },
	{ op_register_ms, "register_MS" },
	{ op_alert, "alert" },
	{ op_modify, "modify" },
	{ op_ms_message_submission, "ms_message_submission" },
	{ op_ms_probe_submission, "ms_probe_submission" },
	{ 0, NULL }
};


/* P7 ERRORS */
static const value_string p7_err_code_string_vals[] = {
	{ err_ros_bind, "ms_bind_error" },
	{ err_attribute_error, "attribute_error" },
	{ err_auto_action_request_error, "auto_action_request_error" },
	{ err_delete_error, "delete_error" },
	{ err_fetch_restriction_error, "fetch_restriction_error" },
	{ err_invalid_parameters_error, "invalid_parameters_error" },
	{ err_range_error, "range_error" },
	{ err_sequence_number_error, "sequence_number_error" },
	{ err_service_error, "service_error" },
	{ err_message_group_error, "message_group_error" },
	{ err_ms_extension_error, "ms_extension_error" },
	{ err_register_ms_error, "register_ms_error" },
	{ err_modify_error, "modify_error" },
	{ err_entry_class_error, "entry_class_error" },
	  { 0, NULL }
};


/*--- Cyclic dependencies ---*/

/* Filter -> Filter/and -> Filter */
/* Filter -> Filter */
static int dissect_p7_Filter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);




static int
dissect_p7_AttributeType(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_object_identifier_str(implicit_tag, actx, tree, tvb, offset, hf_index, &actx->external.direct_reference);

  return offset;
}



static int
dissect_p7_AttributeItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}


static const ber_sequence_t AttributeValues_sequence_of[1] = {
  { &hf_p7_attribute_values_item, BER_CLASS_ANY, 0, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeItem },
};

static int
dissect_p7_AttributeValues(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_attribute_values, AttributeValues_sequence_of, hf_index, ett_p7_AttributeValues);

  return offset;
}


static const ber_sequence_t Attribute_sequence[] = {
  { &hf_p7_attribute_type   , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
  { &hf_p7_attribute_values , BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeValues },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Attribute(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   Attribute_sequence, hf_index, ett_p7_Attribute);

  return offset;
}



static int
dissect_p7_AutoActionType(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_object_identifier_str(implicit_tag, actx, tree, tvb, offset, hf_index, &actx->external.direct_reference);

  return offset;
}



static int
dissect_p7_INTEGER_1_ub_per_auto_action(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            1U, ub_per_auto_action, hf_index, NULL);

  return offset;
}



static int
dissect_p7_T_registration_parameter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);



  return offset;
}


static const ber_sequence_t AutoActionRegistration_sequence[] = {
  { &hf_p7_auto_action_type , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AutoActionType },
  { &hf_p7_registration_identifier, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_1_ub_per_auto_action },
  { &hf_p7_registration_parameter, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_T_registration_parameter },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AutoActionRegistration(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   AutoActionRegistration_sequence, hf_index, ett_p7_AutoActionRegistration);

  return offset;
}



static int
dissect_p7_T_error_code(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	/* XXX: Is this really the best way to do this? */
	offset = dissect_ros_Code(implicit_tag, tvb, offset, actx, tree, hf_index);


  return offset;
}



static int
dissect_p7_T_error_parameter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}


static const ber_sequence_t AutoActionError_set[] = {
  { &hf_p7_error_code       , BER_CLASS_CON, 0, 0, dissect_p7_T_error_code },
  { &hf_p7_error_parameter  , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_T_error_parameter },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AutoActionError(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              AutoActionError_set, hf_index, ett_p7_AutoActionError);

  return offset;
}



static int
dissect_p7_MSExtensionItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_external_type(implicit_tag, tree, tvb, offset, actx, hf_index, NULL);

  return offset;
}


static const ber_sequence_t MSExtensions_sequence_of[1] = {
  { &hf_p7_MSExtensions_item, BER_CLASS_UNI, BER_UNI_TAG_EXTERNAL, BER_FLAGS_NOOWNTAG, dissect_p7_MSExtensionItem },
};

static int
dissect_p7_MSExtensions(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_extensions, MSExtensions_sequence_of, hf_index, ett_p7_MSExtensions);

  return offset;
}


static const value_string p7_EntryClass_vals[] = {
  {   0, "delivery" },
  {   1, "submission" },
  {   2, "draft" },
  {   3, "stored-message" },
  {   4, "delivery-log" },
  {   5, "submission-log" },
  {   6, "message-log" },
  {   7, "auto-action-log" },
  { 0, NULL }
};


static int
dissect_p7_EntryClass(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_entry_classes, hf_index, NULL);

  return offset;
}


static const value_string p7_EntryType_vals[] = {
  {   0, "delivered-message" },
  {   1, "delivered-report" },
  {   2, "returned-content" },
  {   3, "submitted-message" },
  {   4, "submitted-probe" },
  {   5, "draft-message" },
  {   6, "auto-action-event" },
  { 0, NULL }
};


static int
dissect_p7_EntryType(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                                NULL);

  return offset;
}



int
dissect_p7_SequenceNumber(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_messages, hf_index, &seqno);

  return offset;
}


static const value_string p7_RetrievalStatus_vals[] = {
  {   0, "new" },
  {   1, "listed" },
  {   2, "processed" },
  { 0, NULL }
};


static int
dissect_p7_RetrievalStatus(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                                NULL);

  return offset;
}



static int
dissect_p7_GroupNamePart(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_restricted_string(implicit_tag, BER_UNI_TAG_GeneralString,
                                                        actx, tree, tvb, offset,
                                                        1, ub_group_part_length, hf_index, NULL);

  return offset;
}


static const ber_sequence_t MessageGroupName_sequence_of[1] = {
  { &hf_p7_MessageGroupName_item, BER_CLASS_UNI, BER_UNI_TAG_GeneralString, BER_FLAGS_NOOWNTAG, dissect_p7_GroupNamePart },
};

static int
dissect_p7_MessageGroupName(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_group_depth, MessageGroupName_sequence_of, hf_index, ett_p7_MessageGroupName);

  return offset;
}



static int
dissect_p7_T_initiator_name(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	const char *ora = NULL;

	  offset = dissect_p1_ORAddressAndOrDirectoryName(implicit_tag, tvb, offset, actx, tree, hf_index);


	if ((ora = p1_get_last_oraddress(actx))) {
		col_append_fstr(actx->pinfo->cinfo, COL_INFO, " (initiator=%s)", ora);
	}


  return offset;
}



static int
dissect_p7_OBJECT_IDENTIFIER(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_object_identifier(implicit_tag, actx, tree, tvb, offset, hf_index, NULL);

  return offset;
}


static const ber_sequence_t T_allowed_content_types_set_of[1] = {
  { &hf_p7_allowed_content_types_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_OBJECT_IDENTIFIER },
};

static int
dissect_p7_T_allowed_content_types(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_content_types, T_allowed_content_types_set_of, hf_index, ett_p7_T_allowed_content_types);

  return offset;
}



static int
dissect_p7_MS_EIT(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_object_identifier(implicit_tag, actx, tree, tvb, offset, hf_index, NULL);

  return offset;
}


static const ber_sequence_t MS_EITs_set_of[1] = {
  { &hf_p7_MS_EITs_item     , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_MS_EIT },
};

static int
dissect_p7_MS_EITs(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_encoded_information_types, MS_EITs_set_of, hf_index, ett_p7_MS_EITs);

  return offset;
}



static int
dissect_p7_INTEGER(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                                NULL);

  return offset;
}


static const ber_sequence_t Restrictions_set[] = {
  { &hf_p7_allowed_content_types, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_T_allowed_content_types },
  { &hf_p7_allowed_EITs     , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_MS_EITs },
  { &hf_p7_maximum_attribute_length, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Restrictions(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              Restrictions_set, hf_index, ett_p7_Restrictions);

  return offset;
}



static int
dissect_p7_BOOLEAN(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_boolean(implicit_tag, actx, tree, tvb, offset, hf_index, NULL);

  return offset;
}



static int
dissect_p7_RegistrationIdentifier(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_restricted_string(implicit_tag, BER_UNI_TAG_PrintableString,
                                                        actx, tree, tvb, offset,
                                                        1, ub_ua_registration_identifier_length, hf_index, NULL);

  return offset;
}


static const ber_sequence_t MSBindArgument_set[] = {
  { &hf_p7_initiator_name   , BER_CLASS_APP, 0, BER_FLAGS_NOOWNTAG, dissect_p7_T_initiator_name },
  { &hf_p7_initiator_credentials, BER_CLASS_CON, 2, 0, dissect_p1_InitiatorCredentials },
  { &hf_p7_security_context , BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL|BER_FLAGS_IMPLTAG, dissect_p1_SecurityContext },
  { &hf_p7_fetch_restrictions, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_Restrictions },
  { &hf_p7_ms_configuration_request, BER_CLASS_CON, 5, BER_FLAGS_OPTIONAL, dissect_p7_BOOLEAN },
  { &hf_p7_ua_registration_identifier, BER_CLASS_CON, 6, BER_FLAGS_OPTIONAL, dissect_p7_RegistrationIdentifier },
  { &hf_p7_bind_extensions  , BER_CLASS_CON, 7, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MSBindArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              MSBindArgument_set, hf_index, ett_p7_MSBindArgument);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_auto_actions_OF_AutoActionType_set_of[1] = {
  { &hf_p7_available_auto_actions_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AutoActionType },
};

static int
dissect_p7_SET_SIZE_1_ub_auto_actions_OF_AutoActionType(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_auto_actions, SET_SIZE_1_ub_auto_actions_OF_AutoActionType_set_of, hf_index, ett_p7_SET_SIZE_1_ub_auto_actions_OF_AutoActionType);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_attributes_supported_OF_AttributeType_set_of[1] = {
  { &hf_p7_available_attribute_types_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
};

static int
dissect_p7_SET_SIZE_1_ub_attributes_supported_OF_AttributeType(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_attributes_supported, SET_SIZE_1_ub_attributes_supported_OF_AttributeType_set_of, hf_index, ett_p7_SET_SIZE_1_ub_attributes_supported_OF_AttributeType);

  return offset;
}


static const ber_sequence_t T_content_types_supported_set_of[1] = {
  { &hf_p7_content_types_supported_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_OBJECT_IDENTIFIER },
};

static int
dissect_p7_T_content_types_supported(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_content_types, T_content_types_supported_set_of, hf_index, ett_p7_T_content_types_supported);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_entry_classes_OF_EntryClass_set_of[1] = {
  { &hf_p7_entry_classes_supported_item, BER_CLASS_UNI, BER_UNI_TAG_INTEGER, BER_FLAGS_NOOWNTAG, dissect_p7_EntryClass },
};

static int
dissect_p7_SET_SIZE_1_ub_entry_classes_OF_EntryClass(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_entry_classes, SET_SIZE_1_ub_entry_classes_OF_EntryClass_set_of, hf_index, ett_p7_SET_SIZE_1_ub_entry_classes_OF_EntryClass);

  return offset;
}


static const ber_sequence_t T_matching_rules_supported_set_of[1] = {
  { &hf_p7_matching_rules_supported_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_OBJECT_IDENTIFIER },
};

static int
dissect_p7_T_matching_rules_supported(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_matching_rules, T_matching_rules_supported_set_of, hf_index, ett_p7_T_matching_rules_supported);

  return offset;
}



static int
dissect_p7_INTEGER_1_ub_group_depth(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            1U, ub_group_depth, hf_index, NULL);

  return offset;
}



static int
dissect_p7_NULL(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_null(implicit_tag, actx, tree, tvb, offset, hf_index);

  return offset;
}


static const value_string p7_AutoActionErrorIndication_vals[] = {
  {   0, "indication-only" },
  {   1, "auto-action-log-entry" },
  { 0, NULL }
};

static const ber_choice_t AutoActionErrorIndication_choice[] = {
  {   0, &hf_p7_indication_only  , BER_CLASS_CON, 0, 0, dissect_p7_NULL },
  {   1, &hf_p7_auto_action_log_entry, BER_CLASS_CON, 1, 0, dissect_p7_SequenceNumber },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AutoActionErrorIndication(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 AutoActionErrorIndication_choice, hf_index, ett_p7_AutoActionErrorIndication,
                                 NULL);

  return offset;
}


static const ber_sequence_t T_unsupported_extensions_set_of[1] = {
  { &hf_p7_unsupported_extensions_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_OBJECT_IDENTIFIER },
};

static int
dissect_p7_T_unsupported_extensions(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_extensions, T_unsupported_extensions_set_of, hf_index, ett_p7_T_unsupported_extensions);

  return offset;
}



static int
dissect_p7_GeneralString_SIZE_1_ub_service_information_length(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_restricted_string(implicit_tag, BER_UNI_TAG_GeneralString,
                                                        actx, tree, tvb, offset,
                                                        1, ub_service_information_length, hf_index, NULL);

  return offset;
}


static const ber_sequence_t MSBindResult_set[] = {
  { &hf_p7_responder_credentials, BER_CLASS_CON, 2, 0, dissect_p1_ResponderCredentials },
  { &hf_p7_available_auto_actions, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_auto_actions_OF_AutoActionType },
  { &hf_p7_available_attribute_types, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_attributes_supported_OF_AttributeType },
  { &hf_p7_alert_indication , BER_CLASS_CON, 5, BER_FLAGS_OPTIONAL, dissect_p7_BOOLEAN },
  { &hf_p7_content_types_supported, BER_CLASS_CON, 6, BER_FLAGS_OPTIONAL, dissect_p7_T_content_types_supported },
  { &hf_p7_entry_classes_supported, BER_CLASS_CON, 7, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_entry_classes_OF_EntryClass },
  { &hf_p7_matching_rules_supported, BER_CLASS_CON, 8, BER_FLAGS_OPTIONAL, dissect_p7_T_matching_rules_supported },
  { &hf_p7_bind_result_extensions, BER_CLASS_CON, 9, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { &hf_p7_message_group_depth, BER_CLASS_CON, 10, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_1_ub_group_depth },
  { &hf_p7_auto_action_error_indication, BER_CLASS_CON, 11, BER_FLAGS_OPTIONAL|BER_FLAGS_NOTCHKTAG, dissect_p7_AutoActionErrorIndication },
  { &hf_p7_unsupported_extensions, BER_CLASS_CON, 12, BER_FLAGS_OPTIONAL, dissect_p7_T_unsupported_extensions },
  { &hf_p7_ua_registration_id_unknown, BER_CLASS_CON, 13, BER_FLAGS_OPTIONAL, dissect_p7_BOOLEAN },
  { &hf_p7_service_information, BER_CLASS_CON, 14, BER_FLAGS_OPTIONAL, dissect_p7_GeneralString_SIZE_1_ub_service_information_length },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MSBindResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              MSBindResult_set, hf_index, ett_p7_MSBindResult);

  return offset;
}


static const ber_sequence_t ChangeCredentialsAlgorithms_set_of[1] = {
  { &hf_p7_ChangeCredentialsAlgorithms_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_OBJECT_IDENTIFIER },
};

static int
dissect_p7_ChangeCredentialsAlgorithms(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set_of(implicit_tag, actx, tree, tvb, offset,
                                 ChangeCredentialsAlgorithms_set_of, hf_index, ett_p7_ChangeCredentialsAlgorithms);

  return offset;
}


static const value_string p7_BindProblem_vals[] = {
  {   0, "authentication-error" },
  {   1, "unacceptable-security-context" },
  {   2, "unable-to-establish-association" },
  {   3, "bind-extension-problem" },
  {   4, "inadequate-association-confidentiality" },
  { 0, NULL }
};


static int
dissect_p7_BindProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                  NULL);

  return offset;
}



static int
dissect_p7_GeneralString_SIZE_1_ub_supplementary_info_length(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_restricted_string(implicit_tag, BER_UNI_TAG_GeneralString,
                                                        actx, tree, tvb, offset,
                                                        1, ub_supplementary_info_length, hf_index, NULL);

  return offset;
}


static const ber_sequence_t T_bind_extension_errors_set_of[1] = {
  { &hf_p7_bind_extension_errors_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_OBJECT_IDENTIFIER },
};

static int
dissect_p7_T_bind_extension_errors(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_extensions, T_bind_extension_errors_set_of, hf_index, ett_p7_T_bind_extension_errors);

  return offset;
}


static const ber_sequence_t T_qualified_error_set[] = {
  { &hf_p7_bind_problem     , BER_CLASS_CON, 0, 0, dissect_p7_BindProblem },
  { &hf_p7_supplementary_information, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_GeneralString_SIZE_1_ub_supplementary_info_length },
  { &hf_p7_bind_extension_errors, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_T_bind_extension_errors },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_qualified_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              T_qualified_error_set, hf_index, ett_p7_T_qualified_error);

  return offset;
}


static const value_string p7_PAR_ms_bind_error_vals[] = {
  {   0, "unqualified-error" },
  {   1, "qualified-error" },
  { 0, NULL }
};

static const ber_choice_t PAR_ms_bind_error_choice[] = {
  {   0, &hf_p7_unqualified_error, BER_CLASS_UNI, BER_UNI_TAG_ENUMERATED, BER_FLAGS_NOOWNTAG, dissect_p7_BindProblem },
  {   1, &hf_p7_qualified_error  , BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_T_qualified_error },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PAR_ms_bind_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 PAR_ms_bind_error_choice, hf_index, ett_p7_PAR_ms_bind_error,
                                 NULL);

  return offset;
}



static int
dissect_p7_T_from_number(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_p7_SequenceNumber(implicit_tag, tvb, offset, actx, tree, hf_index);

	col_append_fstr(actx->pinfo->cinfo, COL_INFO, " from %d", seqno);

  return offset;
}



static int
dissect_p7_T_to_number(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_p7_SequenceNumber(implicit_tag, tvb, offset, actx, tree, hf_index);

	col_append_fstr(actx->pinfo->cinfo, COL_INFO, " to %d", seqno);

  return offset;
}


static const ber_sequence_t NumberRange_sequence[] = {
  { &hf_p7_from_number      , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_T_from_number },
  { &hf_p7_to_number        , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_T_to_number },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_NumberRange(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	col_append_str(actx->pinfo->cinfo, COL_INFO, " (range=");
	  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   NumberRange_sequence, hf_index, ett_p7_NumberRange);

	col_append_str(actx->pinfo->cinfo, COL_INFO, ")");


  return offset;
}



static int
dissect_p7_CreationTime(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_UTCTime(implicit_tag, actx, tree, tvb, offset, hf_index, NULL, NULL);

  return offset;
}


static const ber_sequence_t TimeRange_sequence[] = {
  { &hf_p7_from_time        , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_CreationTime },
  { &hf_p7_to_time          , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_CreationTime },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_TimeRange(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   TimeRange_sequence, hf_index, ett_p7_TimeRange);

  return offset;
}


static const value_string p7_Range_vals[] = {
  {   0, "sequence-number-range" },
  {   1, "creation-time-range" },
  { 0, NULL }
};

static const ber_choice_t Range_choice[] = {
  {   0, &hf_p7_sequence_number_range, BER_CLASS_CON, 0, 0, dissect_p7_NumberRange },
  {   1, &hf_p7_creation_time_range, BER_CLASS_CON, 1, 0, dissect_p7_TimeRange },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Range(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 Range_choice, hf_index, ett_p7_Range,
                                 NULL);

  return offset;
}



static int
dissect_p7_T_attribute_value(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}


static const ber_sequence_t AttributeValueAssertion_sequence[] = {
  { &hf_p7_attribute_type   , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
  { &hf_p7_attribute_value  , BER_CLASS_ANY, 0, BER_FLAGS_NOOWNTAG, dissect_p7_T_attribute_value },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AttributeValueAssertion(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   AttributeValueAssertion_sequence, hf_index, ett_p7_AttributeValueAssertion);

  return offset;
}



static int
dissect_p7_T_initial(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}



static int
dissect_p7_T_any(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}



static int
dissect_p7_T_final(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}


static const value_string p7_T_strings_item_vals[] = {
  {   0, "initial" },
  {   1, "any" },
  {   2, "final" },
  { 0, NULL }
};

static const ber_choice_t T_strings_item_choice[] = {
  {   0, &hf_p7_initial          , BER_CLASS_CON, 0, 0, dissect_p7_T_initial },
  {   1, &hf_p7_any              , BER_CLASS_CON, 1, 0, dissect_p7_T_any },
  {   2, &hf_p7_final            , BER_CLASS_CON, 2, 0, dissect_p7_T_final },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_strings_item(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 T_strings_item_choice, hf_index, ett_p7_T_strings_item,
                                 NULL);

  return offset;
}


static const ber_sequence_t T_strings_sequence_of[1] = {
  { &hf_p7_strings_item     , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG|BER_FLAGS_NOTCHKTAG, dissect_p7_T_strings_item },
};

static int
dissect_p7_T_strings(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                      T_strings_sequence_of, hf_index, ett_p7_T_strings);

  return offset;
}


static const ber_sequence_t T_substrings_sequence[] = {
  { &hf_p7_type             , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
  { &hf_p7_strings          , BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_T_strings },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_substrings(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   T_substrings_sequence, hf_index, ett_p7_T_substrings);

  return offset;
}



static int
dissect_p7_T_match_value(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}


static const ber_sequence_t MatchingRuleAssertion_sequence[] = {
  { &hf_p7_matching_rule    , BER_CLASS_CON, 0, 0, dissect_p7_OBJECT_IDENTIFIER },
  { &hf_p7_attribute_type   , BER_CLASS_CON, 1, 0, dissect_p7_AttributeType },
  { &hf_p7_match_value      , BER_CLASS_CON, 2, 0, dissect_p7_T_match_value },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MatchingRuleAssertion(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   MatchingRuleAssertion_sequence, hf_index, ett_p7_MatchingRuleAssertion);

  return offset;
}


static const value_string p7_FilterItem_vals[] = {
  {   0, "equality" },
  {   1, "substrings" },
  {   2, "greater-or-equal" },
  {   3, "less-or-equal" },
  {   4, "present" },
  {   5, "approximate-match" },
  {   6, "other-match" },
  { 0, NULL }
};

static const ber_choice_t FilterItem_choice[] = {
  {   0, &hf_p7_equality         , BER_CLASS_CON, 0, 0, dissect_p7_AttributeValueAssertion },
  {   1, &hf_p7_substrings       , BER_CLASS_CON, 1, 0, dissect_p7_T_substrings },
  {   2, &hf_p7_greater_or_equal , BER_CLASS_CON, 2, 0, dissect_p7_AttributeValueAssertion },
  {   3, &hf_p7_less_or_equal    , BER_CLASS_CON, 3, 0, dissect_p7_AttributeValueAssertion },
  {   4, &hf_p7_present          , BER_CLASS_CON, 4, 0, dissect_p7_AttributeType },
  {   5, &hf_p7_approximate_match, BER_CLASS_CON, 5, 0, dissect_p7_AttributeValueAssertion },
  {   6, &hf_p7_other_match      , BER_CLASS_CON, 6, 0, dissect_p7_MatchingRuleAssertion },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_FilterItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 FilterItem_choice, hf_index, ett_p7_FilterItem,
                                 NULL);

  return offset;
}


static const ber_sequence_t SET_OF_Filter_set_of[1] = {
  { &hf_p7_and_item         , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG|BER_FLAGS_NOTCHKTAG, dissect_p7_Filter },
};

static int
dissect_p7_SET_OF_Filter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set_of(implicit_tag, actx, tree, tvb, offset,
                                 SET_OF_Filter_set_of, hf_index, ett_p7_SET_OF_Filter);

  return offset;
}


static const value_string p7_Filter_vals[] = {
  {   0, "item" },
  {   1, "and" },
  {   2, "or" },
  {   3, "not" },
  { 0, NULL }
};

static const ber_choice_t Filter_choice[] = {
  {   0, &hf_p7_filter_item      , BER_CLASS_CON, 0, 0, dissect_p7_FilterItem },
  {   1, &hf_p7_and              , BER_CLASS_CON, 1, 0, dissect_p7_SET_OF_Filter },
  {   2, &hf_p7_or               , BER_CLASS_CON, 2, 0, dissect_p7_SET_OF_Filter },
  {   3, &hf_p7_not              , BER_CLASS_CON, 3, 0, dissect_p7_Filter },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Filter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  // Filter -> Filter/and -> Filter
  actx->pinfo->dissection_depth += 2;
  increment_dissection_depth(actx->pinfo);
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 Filter_choice, hf_index, ett_p7_Filter,
                                 NULL);

  actx->pinfo->dissection_depth -= 2;
  decrement_dissection_depth(actx->pinfo);
  return offset;
}



static int
dissect_p7_INTEGER_1_ub_messages(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            1U, ub_messages, hf_index, NULL);

  return offset;
}


static int * const OverrideRestrictions_bits[] = {
  &hf_p7_OverrideRestrictions_override_content_types_restriction,
  &hf_p7_OverrideRestrictions_override_EITs_restriction,
  &hf_p7_OverrideRestrictions_override_attribute_length_restriction,
  NULL
};

static int
dissect_p7_OverrideRestrictions(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_bitstring(implicit_tag, actx, tree, tvb, offset,
                                                1, ub_ua_restrictions, OverrideRestrictions_bits, 3, hf_index, ett_p7_OverrideRestrictions,
                                                NULL);

  return offset;
}


static const ber_sequence_t Selector_set[] = {
  { &hf_p7_child_entries    , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_BOOLEAN },
  { &hf_p7_range            , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL|BER_FLAGS_NOTCHKTAG, dissect_p7_Range },
  { &hf_p7_filter           , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL|BER_FLAGS_NOTCHKTAG, dissect_p7_Filter },
  { &hf_p7_limit            , BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_1_ub_messages },
  { &hf_p7_override         , BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_OverrideRestrictions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Selector(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              Selector_set, hf_index, ett_p7_Selector);

  return offset;
}



static int
dissect_p7_INTEGER_1_ub_attribute_values(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            1U, ub_attribute_values, hf_index, NULL);

  return offset;
}



static int
dissect_p7_INTEGER_0_ub_attribute_values(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_attribute_values, hf_index, NULL);

  return offset;
}


static const ber_sequence_t AttributeSelection_set[] = {
  { &hf_p7_type             , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
  { &hf_p7_from             , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_1_ub_attribute_values },
  { &hf_p7_selection_count  , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_0_ub_attribute_values },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AttributeSelection(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              AttributeSelection_set, hf_index, ett_p7_AttributeSelection);

  return offset;
}


static const ber_sequence_t EntryInformationSelection_set_of[1] = {
  { &hf_p7_EntryInformationSelection_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeSelection },
};

static int
dissect_p7_EntryInformationSelection(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             0, ub_per_entry, EntryInformationSelection_set_of, hf_index, ett_p7_EntryInformationSelection);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_per_entry_OF_Attribute_set_of[1] = {
  { &hf_p7_attributes_item  , BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_Attribute },
};

static int
dissect_p7_SET_SIZE_1_ub_per_entry_OF_Attribute(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_per_entry, SET_SIZE_1_ub_per_entry_OF_Attribute_set_of, hf_index, ett_p7_SET_SIZE_1_ub_per_entry_OF_Attribute);

  return offset;
}


static const ber_sequence_t AttributeValueCount_sequence[] = {
  { &hf_p7_type             , BER_CLASS_CON, 0, 0, dissect_p7_AttributeType },
  { &hf_p7_total            , BER_CLASS_CON, 1, 0, dissect_p7_INTEGER },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AttributeValueCount(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   AttributeValueCount_sequence, hf_index, ett_p7_AttributeValueCount);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_per_entry_OF_AttributeValueCount_set_of[1] = {
  { &hf_p7_value_count_exceeded_item, BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeValueCount },
};

static int
dissect_p7_SET_SIZE_1_ub_per_entry_OF_AttributeValueCount(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_per_entry, SET_SIZE_1_ub_per_entry_OF_AttributeValueCount_set_of, hf_index, ett_p7_SET_SIZE_1_ub_per_entry_OF_AttributeValueCount);

  return offset;
}


static const ber_sequence_t EntryInformation_sequence[] = {
  { &hf_p7_sequence_number  , BER_CLASS_UNI, BER_UNI_TAG_INTEGER, BER_FLAGS_NOOWNTAG, dissect_p7_SequenceNumber },
  { &hf_p7_attributes       , BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_OPTIONAL|BER_FLAGS_NOOWNTAG, dissect_p7_SET_SIZE_1_ub_per_entry_OF_Attribute },
  { &hf_p7_value_count_exceeded, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_per_entry_OF_AttributeValueCount },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_EntryInformation(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	p1_initialize_content_globals (actx, NULL, false);
	  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   EntryInformation_sequence, hf_index, ett_p7_EntryInformation);

	p1_initialize_content_globals (actx, NULL, false);


  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_message_groups_OF_MessageGroupName_set_of[1] = {
  { &hf_p7_add_message_group_names_item, BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_MessageGroupName },
};

static int
dissect_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupName(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_message_groups, SET_SIZE_1_ub_message_groups_OF_MessageGroupName_set_of, hf_index, ett_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupName);

  return offset;
}


static const ber_sequence_t MSSubmissionOptions_set[] = {
  { &hf_p7_object_entry_class, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_EntryClass },
  { &hf_p7_disable_auto_modify, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_BOOLEAN },
  { &hf_p7_add_message_group_names, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupName },
  { &hf_p7_ms_submission_extensions, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MSSubmissionOptions(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              MSSubmissionOptions_set, hf_index, ett_p7_MSSubmissionOptions);

  return offset;
}



static int
dissect_p7_OriginatorToken(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_p1_MessageToken(implicit_tag, tvb, offset, actx, tree, hf_index);

  return offset;
}


static const ber_sequence_t CommonSubmissionResults_set[] = {
  { &hf_p7_created_entry    , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SequenceNumber },
  { &hf_p7_auto_action_error_indication, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL|BER_FLAGS_NOTCHKTAG, dissect_p7_AutoActionErrorIndication },
  { &hf_p7_ms_submission_result_extensions, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_CommonSubmissionResults(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              CommonSubmissionResults_set, hf_index, ett_p7_CommonSubmissionResults);

  return offset;
}


static const ber_sequence_t SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType_sequence_of[1] = {
  { &hf_p7_summary_requests_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
};

static int
dissect_p7_SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_summaries, SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType_sequence_of, hf_index, ett_p7_SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType);

  return offset;
}


static const ber_sequence_t SummarizeArgument_set[] = {
  { &hf_p7_entry_class      , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_EntryClass },
  { &hf_p7_selector         , BER_CLASS_CON, 1, 0, dissect_p7_Selector },
  { &hf_p7_summary_requests , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType },
  { &hf_p7_summarize_extensions, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_SummarizeArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              SummarizeArgument_set, hf_index, ett_p7_SummarizeArgument);

  return offset;
}



static int
dissect_p7_T_count(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	int count = 0;

	  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_messages, hf_index, &count);


	col_append_fstr(actx->pinfo->cinfo, COL_INFO, " (count=%d)", count);


  return offset;
}


static const ber_sequence_t Span_sequence[] = {
  { &hf_p7_lowest           , BER_CLASS_CON, 0, 0, dissect_p7_SequenceNumber },
  { &hf_p7_highest          , BER_CLASS_CON, 1, 0, dissect_p7_SequenceNumber },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Span(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   Span_sequence, hf_index, ett_p7_Span);

  return offset;
}



static int
dissect_p7_SummaryPresentItemValue(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}


static const ber_sequence_t T_summary_present_item_sequence[] = {
  { &hf_p7_type             , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
  { &hf_p7_value            , BER_CLASS_ANY, 0, BER_FLAGS_NOOWNTAG, dissect_p7_SummaryPresentItemValue },
  { &hf_p7_summary_count    , BER_CLASS_UNI, BER_UNI_TAG_INTEGER, BER_FLAGS_NOOWNTAG, dissect_p7_INTEGER_1_ub_messages },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_summary_present_item(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   T_summary_present_item_sequence, hf_index, ett_p7_T_summary_present_item);

  return offset;
}


static const ber_sequence_t T_summary_present_set_of[1] = {
  { &hf_p7_summary_present_item, BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_T_summary_present_item },
};

static int
dissect_p7_T_summary_present(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_attribute_values, T_summary_present_set_of, hf_index, ett_p7_T_summary_present);

  return offset;
}


static const ber_sequence_t Summary_set[] = {
  { &hf_p7_absent           , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_1_ub_messages },
  { &hf_p7_summary_present  , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_T_summary_present },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Summary(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              Summary_set, hf_index, ett_p7_Summary);

  return offset;
}


static const ber_sequence_t SEQUENCE_SIZE_1_ub_summaries_OF_Summary_sequence_of[1] = {
  { &hf_p7_summaries_item   , BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_Summary },
};

static int
dissect_p7_SEQUENCE_SIZE_1_ub_summaries_OF_Summary(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_summaries, SEQUENCE_SIZE_1_ub_summaries_OF_Summary_sequence_of, hf_index, ett_p7_SEQUENCE_SIZE_1_ub_summaries_OF_Summary);

  return offset;
}


static const ber_sequence_t SummarizeResult_set[] = {
  { &hf_p7_next             , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SequenceNumber },
  { &hf_p7_count            , BER_CLASS_CON, 1, 0, dissect_p7_T_count },
  { &hf_p7_span             , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_Span },
  { &hf_p7_summaries        , BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_SEQUENCE_SIZE_1_ub_summaries_OF_Summary },
  { &hf_p7_summarize_result_extensions, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_SummarizeResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              SummarizeResult_set, hf_index, ett_p7_SummarizeResult);

  return offset;
}


static const ber_sequence_t ListArgument_set[] = {
  { &hf_p7_entry_class      , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_EntryClass },
  { &hf_p7_selector         , BER_CLASS_CON, 1, 0, dissect_p7_Selector },
  { &hf_p7_requested_attributes, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_EntryInformationSelection },
  { &hf_p7_list_extensions  , BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_ListArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              ListArgument_set, hf_index, ett_p7_ListArgument);

  return offset;
}


static const ber_sequence_t SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation_sequence_of[1] = {
  { &hf_p7_requested_item   , BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_EntryInformation },
};

static int
dissect_p7_SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_messages, SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation_sequence_of, hf_index, ett_p7_SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation);

  return offset;
}


static const ber_sequence_t ListResult_set[] = {
  { &hf_p7_next             , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SequenceNumber },
  { &hf_p7_requested        , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation },
  { &hf_p7_list_result_extensions, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_ListResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              ListResult_set, hf_index, ett_p7_ListResult);

  return offset;
}


static const value_string p7_T_item_vals[] = {
  {   1, "search" },
  {   2, "precise" },
  { 0, NULL }
};

static const ber_choice_t T_item_choice[] = {
  {   1, &hf_p7_search           , BER_CLASS_CON, 1, 0, dissect_p7_Selector },
  {   2, &hf_p7_precise          , BER_CLASS_CON, 2, 0, dissect_p7_SequenceNumber },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_item(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 T_item_choice, hf_index, ett_p7_T_item,
                                 NULL);

  return offset;
}


static const ber_sequence_t FetchArgument_set[] = {
  { &hf_p7_entry_class      , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_EntryClass },
  { &hf_p7_item             , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG|BER_FLAGS_NOTCHKTAG, dissect_p7_T_item },
  { &hf_p7_requested_attributes, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_EntryInformationSelection },
  { &hf_p7_fetch_extensions , BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_FetchArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              FetchArgument_set, hf_index, ett_p7_FetchArgument);

  return offset;
}


static const ber_sequence_t SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber_sequence_of[1] = {
  { &hf_p7_list_item        , BER_CLASS_UNI, BER_UNI_TAG_INTEGER, BER_FLAGS_NOOWNTAG, dissect_p7_SequenceNumber },
};

static int
dissect_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_messages, SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber_sequence_of, hf_index, ett_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber);

  return offset;
}


static const ber_sequence_t FetchResult_set[] = {
  { &hf_p7_entry_information, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_EntryInformation },
  { &hf_p7_list             , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber },
  { &hf_p7_next             , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_SequenceNumber },
  { &hf_p7_fetch_result_extensions, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_FetchResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              FetchResult_set, hf_index, ett_p7_FetchResult);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_messages_OF_SequenceNumber_set_of[1] = {
  { &hf_p7_sequence_numbers_item, BER_CLASS_UNI, BER_UNI_TAG_INTEGER, BER_FLAGS_NOOWNTAG, dissect_p7_SequenceNumber },
};

static int
dissect_p7_SET_SIZE_1_ub_messages_OF_SequenceNumber(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_messages, SET_SIZE_1_ub_messages_OF_SequenceNumber_set_of, hf_index, ett_p7_SET_SIZE_1_ub_messages_OF_SequenceNumber);

  return offset;
}


static const value_string p7_T_items_vals[] = {
  {   1, "selector" },
  {   2, "sequence-numbers" },
  { 0, NULL }
};

static const ber_choice_t T_items_choice[] = {
  {   1, &hf_p7_selector         , BER_CLASS_CON, 1, 0, dissect_p7_Selector },
  {   2, &hf_p7_sequence_numbers , BER_CLASS_CON, 2, 0, dissect_p7_SET_SIZE_1_ub_messages_OF_SequenceNumber },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_items(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 T_items_choice, hf_index, ett_p7_T_items,
                                 NULL);

  return offset;
}


static const ber_sequence_t DeleteArgument_set[] = {
  { &hf_p7_entry_class      , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_EntryClass },
  { &hf_p7_items            , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG|BER_FLAGS_NOTCHKTAG, dissect_p7_T_items },
  { &hf_p7_delete_extensions, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_DeleteArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              DeleteArgument_set, hf_index, ett_p7_DeleteArgument);

  return offset;
}


static const ber_sequence_t T_delete_result_94_set[] = {
  { &hf_p7_entries_deleted_94, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber },
  { &hf_p7_delete_result_extensions, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_delete_result_94(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              T_delete_result_94_set, hf_index, ett_p7_T_delete_result_94);

  return offset;
}


static const value_string p7_DeleteResult_vals[] = {
  {   0, "delete-result-88" },
  {   1, "delete-result-94" },
  { 0, NULL }
};

static const ber_choice_t DeleteResult_choice[] = {
  {   0, &hf_p7_delete_result_88 , BER_CLASS_UNI, BER_UNI_TAG_NULL, BER_FLAGS_NOOWNTAG, dissect_p7_NULL },
  {   1, &hf_p7_delete_result_94 , BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_T_delete_result_94 },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_DeleteResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 DeleteResult_choice, hf_index, ett_p7_DeleteResult,
                                 NULL);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration_set_of[1] = {
  { &hf_p7_auto_action_registrations_item, BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_AutoActionRegistration },
};

static int
dissect_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_auto_registrations, SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration_set_of, hf_index, ett_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration);

  return offset;
}


static const ber_sequence_t AutoActionDeregistration_sequence[] = {
  { &hf_p7_auto_action_type , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AutoActionType },
  { &hf_p7_registration_identifier, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_1_ub_per_auto_action },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AutoActionDeregistration(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   AutoActionDeregistration_sequence, hf_index, ett_p7_AutoActionDeregistration);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration_set_of[1] = {
  { &hf_p7_auto_action_deregistrations_item, BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_AutoActionDeregistration },
};

static int
dissect_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_auto_registrations, SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration_set_of, hf_index, ett_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration);

  return offset;
}


static const ber_sequence_t SET_SIZE_0_ub_default_registrations_OF_AttributeType_set_of[1] = {
  { &hf_p7_list_attribute_defaults_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
};

static int
dissect_p7_SET_SIZE_0_ub_default_registrations_OF_AttributeType(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             0, ub_default_registrations, SET_SIZE_0_ub_default_registrations_OF_AttributeType_set_of, hf_index, ett_p7_SET_SIZE_0_ub_default_registrations_OF_AttributeType);

  return offset;
}


static const ber_sequence_t T_change_credentials_sequence[] = {
  { &hf_p7_register_old_credentials, BER_CLASS_CON, 0, 0, dissect_p1_Credentials },
  { &hf_p7_new_credentials  , BER_CLASS_CON, 1, 0, dissect_p1_Credentials },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_change_credentials(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   T_change_credentials_sequence, hf_index, ett_p7_T_change_credentials);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel_set_of[1] = {
  { &hf_p7_user_security_labels_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p1_SecurityLabel },
};

static int
dissect_p7_SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_labels_and_redirections, SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel_set_of, hf_index, ett_p7_SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel);

  return offset;
}


static const ber_sequence_t UARegistration_set[] = {
  { &hf_p7_ua_registration_identifier, BER_CLASS_CON, 0, 0, dissect_p7_RegistrationIdentifier },
  { &hf_p7_ua_list_attribute_defaults, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_0_ub_default_registrations_OF_AttributeType },
  { &hf_p7_ua_fetch_attribute_defaults, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_0_ub_default_registrations_OF_AttributeType },
  { &hf_p7_ua_submission_defaults, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_MSSubmissionOptions },
  { &hf_p7_content_specific_defaults, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_UARegistration(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              UARegistration_set, hf_index, ett_p7_UARegistration);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_ua_registrations_OF_UARegistration_set_of[1] = {
  { &hf_p7_ua_registrations_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_UARegistration },
};

static int
dissect_p7_SET_SIZE_1_ub_ua_registrations_OF_UARegistration(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_ua_registrations, SET_SIZE_1_ub_ua_registrations_OF_UARegistration_set_of, hf_index, ett_p7_SET_SIZE_1_ub_ua_registrations_OF_UARegistration);

  return offset;
}



static int
dissect_p7_GeneralString_SIZE_1_ub_group_descriptor_length(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_restricted_string(implicit_tag, BER_UNI_TAG_GeneralString,
                                                        actx, tree, tvb, offset,
                                                        1, ub_group_descriptor_length, hf_index, NULL);

  return offset;
}


static const ber_sequence_t MessageGroupNameAndDescriptor_set[] = {
  { &hf_p7_message_group_name, BER_CLASS_CON, 0, 0, dissect_p7_MessageGroupName },
  { &hf_p7_message_group_descriptor, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_GeneralString_SIZE_1_ub_group_descriptor_length },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MessageGroupNameAndDescriptor(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              MessageGroupNameAndDescriptor_set, hf_index, ett_p7_MessageGroupNameAndDescriptor);

  return offset;
}


static const value_string p7_MessageGroupRegistrations_item_vals[] = {
  {   0, "register-group" },
  {   1, "deregister-group" },
  {   2, "change-descriptors" },
  { 0, NULL }
};

static const ber_choice_t MessageGroupRegistrations_item_choice[] = {
  {   0, &hf_p7_register_group   , BER_CLASS_CON, 0, 0, dissect_p7_MessageGroupNameAndDescriptor },
  {   1, &hf_p7_deregister_group , BER_CLASS_CON, 1, 0, dissect_p7_MessageGroupName },
  {   2, &hf_p7_change_descriptors, BER_CLASS_CON, 2, 0, dissect_p7_MessageGroupNameAndDescriptor },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MessageGroupRegistrations_item(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 MessageGroupRegistrations_item_choice, hf_index, ett_p7_MessageGroupRegistrations_item,
                                 NULL);

  return offset;
}


static const ber_sequence_t MessageGroupRegistrations_sequence_of[1] = {
  { &hf_p7_MessageGroupRegistrations_item, BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG|BER_FLAGS_NOTCHKTAG, dissect_p7_MessageGroupRegistrations_item },
};

static int
dissect_p7_MessageGroupRegistrations(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_default_registrations, MessageGroupRegistrations_sequence_of, hf_index, ett_p7_MessageGroupRegistrations);

  return offset;
}


static int * const T_registrations_bits[] = {
  &hf_p7_T_registrations_auto_action_registrations,
  &hf_p7_T_registrations_list_attribute_defaults,
  &hf_p7_T_registrations_fetch_attribute_defaults,
  &hf_p7_T_registrations_ua_registrations,
  &hf_p7_T_registrations_submission_defaults,
  &hf_p7_T_registrations_message_group_registrations,
  NULL
};

static int
dissect_p7_T_registrations(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_bitstring(implicit_tag, actx, tree, tvb, offset,
                                    T_registrations_bits, 6, hf_index, ett_p7_T_registrations,
                                    NULL);

  return offset;
}



static int
dissect_p7_T_extended_registrations_item(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	/* XXX: Is this really the best way to do this? */
	offset = dissect_ber_object_identifier_str(implicit_tag, actx, tree, tvb, offset, hf_index, &actx->external.direct_reference);


  return offset;
}


static const ber_sequence_t T_extended_registrations_set_of[1] = {
  { &hf_p7_extended_registrations_item, BER_CLASS_ANY, 0, BER_FLAGS_NOOWNTAG, dissect_p7_T_extended_registrations_item },
};

static int
dissect_p7_T_extended_registrations(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set_of(implicit_tag, actx, tree, tvb, offset,
                                 T_extended_registrations_set_of, hf_index, ett_p7_T_extended_registrations);

  return offset;
}


static const ber_sequence_t MessageGroupsRestriction_set[] = {
  { &hf_p7_parent_group     , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_MessageGroupName },
  { &hf_p7_immediate_descendants_only, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_BOOLEAN },
  { &hf_p7_omit_descriptors , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_BOOLEAN },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MessageGroupsRestriction(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              MessageGroupsRestriction_set, hf_index, ett_p7_MessageGroupsRestriction);

  return offset;
}


static const ber_sequence_t RegistrationTypes_set[] = {
  { &hf_p7_registrations    , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_T_registrations },
  { &hf_p7_extended_registrations, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_T_extended_registrations },
  { &hf_p7_restrict_message_groups, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_MessageGroupsRestriction },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_RegistrationTypes(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              RegistrationTypes_set, hf_index, ett_p7_RegistrationTypes);

  return offset;
}


static const ber_sequence_t Register_MSArgument_set[] = {
  { &hf_p7_auto_action_registrations, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration },
  { &hf_p7_auto_action_deregistrations, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration },
  { &hf_p7_list_attribute_defaults, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_0_ub_default_registrations_OF_AttributeType },
  { &hf_p7_fetch_attribute_defaults, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_0_ub_default_registrations_OF_AttributeType },
  { &hf_p7_change_credentials, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_T_change_credentials },
  { &hf_p7_user_security_labels, BER_CLASS_CON, 5, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel },
  { &hf_p7_ua_registrations , BER_CLASS_CON, 6, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_ua_registrations_OF_UARegistration },
  { &hf_p7_submission_defaults, BER_CLASS_CON, 7, BER_FLAGS_OPTIONAL, dissect_p7_MSSubmissionOptions },
  { &hf_p7_message_group_registrations, BER_CLASS_CON, 8, BER_FLAGS_OPTIONAL, dissect_p7_MessageGroupRegistrations },
  { &hf_p7_registration_status_request, BER_CLASS_CON, 9, BER_FLAGS_OPTIONAL, dissect_p7_RegistrationTypes },
  { &hf_p7_register_ms_extensions, BER_CLASS_CON, 10, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Register_MSArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              Register_MSArgument_set, hf_index, ett_p7_Register_MSArgument);

  return offset;
}



static int
dissect_p7_BIT_STRING(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_bitstring(implicit_tag, actx, tree, tvb, offset,
                                    NULL, 0, hf_index, -1,
                                    NULL);

  return offset;
}


static const ber_sequence_t ProtectedChangeCredentials_sequence[] = {
  { &hf_p7_algorithm_identifier, BER_CLASS_CON, 0, BER_FLAGS_IMPLTAG, dissect_p7_OBJECT_IDENTIFIER },
  { &hf_p7_old_credentials  , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG, dissect_p1_InitiatorCredentials },
  { &hf_p7_password_delta   , BER_CLASS_CON, 2, BER_FLAGS_IMPLTAG, dissect_p7_BIT_STRING },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_ProtectedChangeCredentials(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   ProtectedChangeCredentials_sequence, hf_index, ett_p7_ProtectedChangeCredentials);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_default_registrations_OF_AttributeType_set_of[1] = {
  { &hf_p7_registered_list_attribute_defaults_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
};

static int
dissect_p7_SET_SIZE_1_ub_default_registrations_OF_AttributeType(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_default_registrations, SET_SIZE_1_ub_default_registrations_OF_AttributeType_set_of, hf_index, ett_p7_SET_SIZE_1_ub_default_registrations_OF_AttributeType);

  return offset;
}


static const ber_sequence_t SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor_set_of[1] = {
  { &hf_p7_registered_message_group_registrations_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_MessageGroupNameAndDescriptor },
};

static int
dissect_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_message_groups, SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor_set_of, hf_index, ett_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor);

  return offset;
}


static const ber_sequence_t T_registered_information_set[] = {
  { &hf_p7_auto_action_registrations, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration },
  { &hf_p7_registered_list_attribute_defaults, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_default_registrations_OF_AttributeType },
  { &hf_p7_registered_fetch_attribute_defaults, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_default_registrations_OF_AttributeType },
  { &hf_p7_ua_registrations , BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_ua_registrations_OF_UARegistration },
  { &hf_p7_submission_defaults, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_MSSubmissionOptions },
  { &hf_p7_registered_message_group_registrations, BER_CLASS_CON, 5, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor },
  { &hf_p7_register_ms_result_extensions, BER_CLASS_CON, 6, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_registered_information(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              T_registered_information_set, hf_index, ett_p7_T_registered_information);

  return offset;
}


static const value_string p7_Register_MSResult_vals[] = {
  {   0, "no-status-information" },
  {   1, "registered-information" },
  { 0, NULL }
};

static const ber_choice_t Register_MSResult_choice[] = {
  {   0, &hf_p7_no_status_information, BER_CLASS_UNI, BER_UNI_TAG_NULL, BER_FLAGS_NOOWNTAG, dissect_p7_NULL },
  {   1, &hf_p7_registered_information, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_T_registered_information },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_Register_MSResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 Register_MSResult_choice, hf_index, ett_p7_Register_MSResult,
                                 NULL);

  return offset;
}



static int
dissect_p7_INTEGER_1_ub_auto_actions(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            1U, ub_auto_actions, hf_index, NULL);

  return offset;
}


static const ber_sequence_t AlertArgument_set[] = {
  { &hf_p7_alert_registration_identifier, BER_CLASS_CON, 0, 0, dissect_p7_INTEGER_1_ub_auto_actions },
  { &hf_p7_new_entry        , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_EntryInformation },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AlertArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              AlertArgument_set, hf_index, ett_p7_AlertArgument);

  return offset;
}



static int
dissect_p7_AlertResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_null(implicit_tag, actx, tree, tvb, offset, hf_index);

  return offset;
}


static const value_string p7_T_entries_vals[] = {
  {   1, "selector" },
  {   2, "specific-entries" },
  { 0, NULL }
};

static const ber_choice_t T_entries_choice[] = {
  {   1, &hf_p7_selector         , BER_CLASS_CON, 1, 0, dissect_p7_Selector },
  {   2, &hf_p7_specific_entries , BER_CLASS_CON, 2, 0, dissect_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_entries(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 T_entries_choice, hf_index, ett_p7_T_entries,
                                 NULL);

  return offset;
}



static int
dissect_p7_OrderedAttributeValue(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);


  return offset;
}


static const ber_sequence_t OrderedAttributeItem_sequence[] = {
  { &hf_p7_ordered_attribute_value, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_OrderedAttributeValue },
  { &hf_p7_ordered_position , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_1_ub_attribute_values },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_OrderedAttributeItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   OrderedAttributeItem_sequence, hf_index, ett_p7_OrderedAttributeItem);

  return offset;
}


static const ber_sequence_t OrderedAttributeValues_sequence_of[1] = {
  { &hf_p7_ordered_attribute_values_item, BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_OrderedAttributeItem },
};

static int
dissect_p7_OrderedAttributeValues(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_attribute_values, OrderedAttributeValues_sequence_of, hf_index, ett_p7_OrderedAttributeValues);

  return offset;
}


static const ber_sequence_t OrderedAttribute_sequence[] = {
  { &hf_p7_attribute_type   , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeType },
  { &hf_p7_ordered_attribute_values, BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_OrderedAttributeValues },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_OrderedAttribute(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   OrderedAttribute_sequence, hf_index, ett_p7_OrderedAttribute);

  return offset;
}


static const value_string p7_T_modification_vals[] = {
  {   1, "add-attribute" },
  {   2, "remove-attribute" },
  {   3, "add-values" },
  {   4, "remove-values" },
  { 0, NULL }
};

static const ber_choice_t T_modification_choice[] = {
  {   1, &hf_p7_add_attribute    , BER_CLASS_CON, 1, 0, dissect_p7_Attribute },
  {   2, &hf_p7_remove_attribute , BER_CLASS_CON, 2, 0, dissect_p7_AttributeType },
  {   3, &hf_p7_add_values       , BER_CLASS_CON, 3, 0, dissect_p7_OrderedAttribute },
  {   4, &hf_p7_remove_values    , BER_CLASS_CON, 4, 0, dissect_p7_OrderedAttribute },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_modification(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 T_modification_choice, hf_index, ett_p7_T_modification,
                                 NULL);

  return offset;
}


static const ber_sequence_t EntryModification_set[] = {
  { &hf_p7_strict           , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_BOOLEAN },
  { &hf_p7_modification     , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG|BER_FLAGS_NOTCHKTAG, dissect_p7_T_modification },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_EntryModification(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	p1_initialize_content_globals (actx, NULL, false);
	  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              EntryModification_set, hf_index, ett_p7_EntryModification);

	p1_initialize_content_globals (actx, NULL, false);

  return offset;
}


static const ber_sequence_t SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification_sequence_of[1] = {
  { &hf_p7_modifications_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_EntryModification },
};

static int
dissect_p7_SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                                  1, ub_modifications, SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification_sequence_of, hf_index, ett_p7_SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification);

  return offset;
}


static const ber_sequence_t ModifyArgument_set[] = {
  { &hf_p7_entry_class      , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_EntryClass },
  { &hf_p7_entries          , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG|BER_FLAGS_NOTCHKTAG, dissect_p7_T_entries },
  { &hf_p7_modifications    , BER_CLASS_CON, 3, 0, dissect_p7_SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification },
  { &hf_p7_modify_extensions, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_ModifyArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              ModifyArgument_set, hf_index, ett_p7_ModifyArgument);

  return offset;
}


static const ber_sequence_t ModifyResult_set[] = {
  { &hf_p7_entries_modified , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber },
  { &hf_p7_modify_result_extensions, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_MSExtensions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_ModifyResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              ModifyResult_set, hf_index, ett_p7_ModifyResult);

  return offset;
}


static const ber_sequence_t MSMessageSubmissionArgument_sequence[] = {
  { &hf_p7_envelope         , BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p1_MessageSubmissionEnvelope },
  { &hf_p7_content          , BER_CLASS_UNI, BER_UNI_TAG_OCTETSTRING, BER_FLAGS_NOOWNTAG, dissect_p1_Content },
  { &hf_p7_submission_options, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_MSSubmissionOptions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MSMessageSubmissionArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	p1_initialize_content_globals (actx, tree, true);
	  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   MSMessageSubmissionArgument_sequence, hf_index, ett_p7_MSMessageSubmissionArgument);

	p1_initialize_content_globals (actx, NULL, false);


  return offset;
}


static const ber_sequence_t SET_OF_ExtensionField_set_of[1] = {
  { &hf_p7_extensions_item  , BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p1_ExtensionField },
};

static int
dissect_p7_SET_OF_ExtensionField(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set_of(implicit_tag, actx, tree, tvb, offset,
                                 SET_OF_ExtensionField_set_of, hf_index, ett_p7_SET_OF_ExtensionField);

  return offset;
}


static const ber_sequence_t T_mts_result_set[] = {
  { &hf_p7_message_submission_identifier, BER_CLASS_APP, 4, BER_FLAGS_NOOWNTAG, dissect_p1_MessageSubmissionIdentifier },
  { &hf_p7_message_submission_time, BER_CLASS_CON, 0, BER_FLAGS_IMPLTAG, dissect_p1_MessageSubmissionTime },
  { &hf_p7_content_identifier, BER_CLASS_APP, 10, BER_FLAGS_OPTIONAL|BER_FLAGS_NOOWNTAG, dissect_p1_ContentIdentifier },
  { &hf_p7_extensions       , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SET_OF_ExtensionField },
  { &hf_p7_ms_message_result, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_CommonSubmissionResults },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_mts_result(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              T_mts_result_set, hf_index, ett_p7_T_mts_result);

  return offset;
}


static const value_string p7_MSMessageSubmissionResult_vals[] = {
  {   0, "mts-result" },
  {   1, "store-draft-result" },
  { 0, NULL }
};

static const ber_choice_t MSMessageSubmissionResult_choice[] = {
  {   0, &hf_p7_mts_result       , BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_T_mts_result },
  {   1, &hf_p7_store_draft_result, BER_CLASS_CON, 4, 0, dissect_p7_CommonSubmissionResults },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MSMessageSubmissionResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 MSMessageSubmissionResult_choice, hf_index, ett_p7_MSMessageSubmissionResult,
                                 NULL);

  return offset;
}


static const ber_sequence_t SEQUENCE_OF_PerRecipientProbeSubmissionFields_sequence_of[1] = {
  { &hf_p7_per_recipient_fields_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p1_PerRecipientProbeSubmissionFields },
};

static int
dissect_p7_SEQUENCE_OF_PerRecipientProbeSubmissionFields(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                      SEQUENCE_OF_PerRecipientProbeSubmissionFields_sequence_of, hf_index, ett_p7_SEQUENCE_OF_PerRecipientProbeSubmissionFields);

  return offset;
}


static const ber_sequence_t MSProbeSubmissionArgument_set[] = {
  { &hf_p7_originator_name  , BER_CLASS_APP, 0, BER_FLAGS_NOOWNTAG, dissect_p1_OriginatorName },
  { &hf_p7_original_encoded_information_types, BER_CLASS_APP, 5, BER_FLAGS_OPTIONAL|BER_FLAGS_NOOWNTAG, dissect_p1_OriginalEncodedInformationTypes },
  { &hf_p7_content_type     , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG, dissect_p1_ContentType },
  { &hf_p7_content_identifier, BER_CLASS_APP, 10, BER_FLAGS_OPTIONAL|BER_FLAGS_NOOWNTAG, dissect_p1_ContentIdentifier },
  { &hf_p7_content_length   , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p1_ContentLength },
  { &hf_p7_per_message_indicators, BER_CLASS_APP, 8, BER_FLAGS_OPTIONAL|BER_FLAGS_NOOWNTAG, dissect_p1_PerMessageIndicators },
  { &hf_p7_extensions       , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_SET_OF_ExtensionField },
  { &hf_p7_per_recipient_fields, BER_CLASS_CON, 3, 0, dissect_p7_SEQUENCE_OF_PerRecipientProbeSubmissionFields },
  { &hf_p7_submission_options, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_MSSubmissionOptions },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MSProbeSubmissionArgument(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              MSProbeSubmissionArgument_set, hf_index, ett_p7_MSProbeSubmissionArgument);

  return offset;
}


static const ber_sequence_t MSProbeSubmissionResult_set[] = {
  { &hf_p7_probe_submission_identifier, BER_CLASS_APP, 4, BER_FLAGS_NOOWNTAG, dissect_p1_ProbeSubmissionIdentifier },
  { &hf_p7_probe_submission_time, BER_CLASS_CON, 0, 0, dissect_p1_ProbeSubmissionTime },
  { &hf_p7_content_identifier, BER_CLASS_APP, 10, BER_FLAGS_OPTIONAL|BER_FLAGS_NOOWNTAG, dissect_p1_ContentIdentifier },
  { &hf_p7_extensions       , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SET_OF_ExtensionField },
  { &hf_p7_ms_probe_result  , BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_CommonSubmissionResults },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MSProbeSubmissionResult(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              MSProbeSubmissionResult_set, hf_index, ett_p7_MSProbeSubmissionResult);

  return offset;
}


static const value_string p7_AttributeProblem_vals[] = {
  {   0, "invalid-attribute-value" },
  {   1, "unavailable-attribute-type" },
  {   2, "inappropriate-matching" },
  {   3, "attribute-type-not-subscribed" },
  {   4, "inappropriate-for-operation" },
  {   5, "inappropriate-modification" },
  {   6, "single-valued-attribute" },
  { 0, NULL }
};


static int
dissect_p7_AttributeProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}



static int
dissect_p7_T_attr_value(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	if(actx->external.direct_reference)
		call_ber_oid_callback(actx->external.direct_reference, tvb, offset, actx->pinfo, tree, NULL);



  return offset;
}


static const ber_sequence_t AttributeProblemItem_set[] = {
  { &hf_p7_attribute_problem, BER_CLASS_CON, 0, 0, dissect_p7_AttributeProblem },
  { &hf_p7_type             , BER_CLASS_CON, 1, 0, dissect_p7_AttributeType },
  { &hf_p7_attr_value       , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_T_attr_value },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AttributeProblemItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              AttributeProblemItem_set, hf_index, ett_p7_AttributeProblemItem);

  return offset;
}


static const ber_sequence_t AttributeProblems_set_of[1] = {
  { &hf_p7_attribute_problem_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_AttributeProblemItem },
};

static int
dissect_p7_AttributeProblems(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_per_entry, AttributeProblems_set_of, hf_index, ett_p7_AttributeProblems);

  return offset;
}


static const ber_sequence_t PAR_attribute_error_set[] = {
  { &hf_p7_attribute_problems, BER_CLASS_CON, 0, 0, dissect_p7_AttributeProblems },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PAR_attribute_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              PAR_attribute_error_set, hf_index, ett_p7_PAR_attribute_error);

  return offset;
}


static const value_string p7_AutoActionRequestProblem_vals[] = {
  {   0, "unavailable-auto-action-type" },
  {   1, "auto-action-type-not-subscribed" },
  {   2, "not-willing-to-perform" },
  { 0, NULL }
};


static int
dissect_p7_AutoActionRequestProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}


static const ber_sequence_t AutoActionRequestProblemItem_set[] = {
  { &hf_p7_auto_action_request_problem, BER_CLASS_CON, 0, 0, dissect_p7_AutoActionRequestProblem },
  { &hf_p7_auto_action_type , BER_CLASS_CON, 1, 0, dissect_p7_AutoActionType },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_AutoActionRequestProblemItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              AutoActionRequestProblemItem_set, hf_index, ett_p7_AutoActionRequestProblemItem);

  return offset;
}


static const ber_sequence_t AutoActionRequestProblems_set_of[1] = {
  { &hf_p7_auto_action_request_problem_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_AutoActionRequestProblemItem },
};

static int
dissect_p7_AutoActionRequestProblems(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_auto_registrations, AutoActionRequestProblems_set_of, hf_index, ett_p7_AutoActionRequestProblems);

  return offset;
}


static const ber_sequence_t PAR_auto_action_request_error_set[] = {
  { &hf_p7_auto_action_request_problems, BER_CLASS_CON, 0, 0, dissect_p7_AutoActionRequestProblems },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PAR_auto_action_request_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              PAR_auto_action_request_error_set, hf_index, ett_p7_PAR_auto_action_request_error);

  return offset;
}


static const value_string p7_DeleteProblem_vals[] = {
  {   0, "child-entry-specified" },
  {   1, "delete-restriction-problem" },
  {   2, "new-entry-specified" },
  {   3, "entry-class-restriction" },
  {   4, "stored-message-exists" },
  { 0, NULL }
};


static int
dissect_p7_DeleteProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}


static const ber_sequence_t DeleteProblemItem_set[] = {
  { &hf_p7_delete_problem   , BER_CLASS_CON, 0, 0, dissect_p7_DeleteProblem },
  { &hf_p7_sequence_number  , BER_CLASS_CON, 1, 0, dissect_p7_SequenceNumber },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_DeleteProblemItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              DeleteProblemItem_set, hf_index, ett_p7_DeleteProblemItem);

  return offset;
}


static const ber_sequence_t DeleteProblems_set_of[1] = {
  { &hf_p7_delete_problem_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_DeleteProblemItem },
};

static int
dissect_p7_DeleteProblems(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_messages, DeleteProblems_set_of, hf_index, ett_p7_DeleteProblems);

  return offset;
}


static const ber_sequence_t PAR_delete_error_set[] = {
  { &hf_p7_delete_problems  , BER_CLASS_CON, 0, 0, dissect_p7_DeleteProblems },
  { &hf_p7_entries_deleted  , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SET_SIZE_1_ub_messages_OF_SequenceNumber },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PAR_delete_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              PAR_delete_error_set, hf_index, ett_p7_PAR_delete_error);

  return offset;
}


static const value_string p7_FetchRestrictionProblem_vals[] = {
  {   1, "content-type-problem" },
  {   2, "eit-problem" },
  {   3, "maximum-length-problem" },
  { 0, NULL }
};


static int
dissect_p7_FetchRestrictionProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}


static const value_string p7_T_restriction_vals[] = {
  {   0, "content-type" },
  {   1, "eit" },
  {   2, "attribute-length" },
  { 0, NULL }
};

static const ber_choice_t T_restriction_choice[] = {
  {   0, &hf_p7_extended_content_type, BER_CLASS_CON, 0, 0, dissect_p7_OBJECT_IDENTIFIER },
  {   1, &hf_p7_eit              , BER_CLASS_CON, 1, 0, dissect_p7_MS_EITs },
  {   2, &hf_p7_attribute_length , BER_CLASS_CON, 2, 0, dissect_p7_INTEGER },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_T_restriction(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 T_restriction_choice, hf_index, ett_p7_T_restriction,
                                 NULL);

  return offset;
}


static const ber_sequence_t FetchRestrictionProblemItem_set[] = {
  { &hf_p7_fetch_restriction_problem, BER_CLASS_CON, 3, 0, dissect_p7_FetchRestrictionProblem },
  { &hf_p7_restriction      , BER_CLASS_ANY/*choice*/, -1/*choice*/, BER_FLAGS_NOOWNTAG|BER_FLAGS_NOTCHKTAG, dissect_p7_T_restriction },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_FetchRestrictionProblemItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              FetchRestrictionProblemItem_set, hf_index, ett_p7_FetchRestrictionProblemItem);

  return offset;
}


static const ber_sequence_t FetchRestrictionProblems_set_of[1] = {
  { &hf_p7_fetch_restriction_problem_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_FetchRestrictionProblemItem },
};

static int
dissect_p7_FetchRestrictionProblems(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_default_registrations, FetchRestrictionProblems_set_of, hf_index, ett_p7_FetchRestrictionProblems);

  return offset;
}


static const ber_sequence_t PAR_fetch_restriction_error_set[] = {
  { &hf_p7_fetch_restriction_problems, BER_CLASS_CON, 0, 0, dissect_p7_FetchRestrictionProblems },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PAR_fetch_restriction_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              PAR_fetch_restriction_error_set, hf_index, ett_p7_PAR_fetch_restriction_error);

  return offset;
}



static int
dissect_p7_PAR_invalid_parameters_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_null(implicit_tag, actx, tree, tvb, offset, hf_index);

  return offset;
}


static const value_string p7_RangeProblem_vals[] = {
  {   0, "reversed" },
  { 0, NULL }
};


static int
dissect_p7_RangeProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}


static const ber_sequence_t PAR_range_error_set[] = {
  { &hf_p7_range_problem    , BER_CLASS_CON, 0, 0, dissect_p7_RangeProblem },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PAR_range_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              PAR_range_error_set, hf_index, ett_p7_PAR_range_error);

  return offset;
}


static const value_string p7_SequenceNumberProblem_vals[] = {
  {   0, "no-such-entry" },
  { 0, NULL }
};


static int
dissect_p7_SequenceNumberProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}


static const ber_sequence_t SequenceNumberProblemItem_set[] = {
  { &hf_p7_sequence_number_problem, BER_CLASS_CON, 0, 0, dissect_p7_SequenceNumberProblem },
  { &hf_p7_sequence_number  , BER_CLASS_CON, 1, 0, dissect_p7_SequenceNumber },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_SequenceNumberProblemItem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              SequenceNumberProblemItem_set, hf_index, ett_p7_SequenceNumberProblemItem);

  return offset;
}


static const ber_sequence_t SequenceNumberProblems_set_of[1] = {
  { &hf_p7_sequence_number_problem_item, BER_CLASS_UNI, BER_UNI_TAG_SET, BER_FLAGS_NOOWNTAG, dissect_p7_SequenceNumberProblemItem },
};

static int
dissect_p7_SequenceNumberProblems(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_set_of(implicit_tag, actx, tree, tvb, offset,
                                             1, ub_messages, SequenceNumberProblems_set_of, hf_index, ett_p7_SequenceNumberProblems);

  return offset;
}


static const ber_sequence_t PAR_sequence_number_error_set[] = {
  { &hf_p7_sequence_number_problems, BER_CLASS_CON, 1, 0, dissect_p7_SequenceNumberProblems },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PAR_sequence_number_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              PAR_sequence_number_error_set, hf_index, ett_p7_PAR_sequence_number_error);

  return offset;
}


static const value_string p7_ServiceProblem_vals[] = {
  {   0, "busy" },
  {   1, "unavailable" },
  {   2, "unwilling-to-perform" },
  { 0, NULL }
};


static int
dissect_p7_ServiceProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}


static const ber_sequence_t ServiceErrorParameter_set[] = {
  { &hf_p7_service_problem  , BER_CLASS_CON, 0, 0, dissect_p7_ServiceProblem },
  { &hf_p7_supplementary_information, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_GeneralString_SIZE_1_ub_supplementary_info_length },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_ServiceErrorParameter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              ServiceErrorParameter_set, hf_index, ett_p7_ServiceErrorParameter);

  return offset;
}


static const value_string p7_MessageGroupProblem_vals[] = {
  {   0, "name-not-registered" },
  {   1, "name-already-registered" },
  {   2, "parent-not-registered" },
  {   3, "group-not-empty" },
  {   4, "name-in-use" },
  {   5, "child-group-registered" },
  {   6, "group-depth-exceeded" },
  { 0, NULL }
};


static int
dissect_p7_MessageGroupProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}


static const ber_sequence_t MessageGroupErrorParameter_set[] = {
  { &hf_p7_message_group_problem, BER_CLASS_CON, 0, 0, dissect_p7_MessageGroupProblem },
  { &hf_p7_name             , BER_CLASS_CON, 1, 0, dissect_p7_MessageGroupName },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MessageGroupErrorParameter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              MessageGroupErrorParameter_set, hf_index, ett_p7_MessageGroupErrorParameter);

  return offset;
}


static const value_string p7_MSExtensionErrorParameter_vals[] = {
  {   0, "ms-extension-problem" },
  {   1, "unknown-ms-extension" },
  { 0, NULL }
};

static const ber_choice_t MSExtensionErrorParameter_choice[] = {
  {   0, &hf_p7_ms_extension_problem, BER_CLASS_CON, 0, 0, dissect_p7_MSExtensionItem },
  {   1, &hf_p7_unknown_ms_extension, BER_CLASS_CON, 1, 0, dissect_p7_OBJECT_IDENTIFIER },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_MSExtensionErrorParameter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 MSExtensionErrorParameter_choice, hf_index, ett_p7_MSExtensionErrorParameter,
                                 NULL);

  return offset;
}


static const value_string p7_RegistrationProblem_vals[] = {
  {   0, "registration-not-supported" },
  {   1, "registration-improperly-specified" },
  {   2, "registration-limit-exceeded" },
  { 0, NULL }
};


static int
dissect_p7_RegistrationProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                  NULL);

  return offset;
}


static const ber_sequence_t PAR_register_ms_error_set[] = {
  { &hf_p7_register_ms_problem, BER_CLASS_CON, 0, 0, dissect_p7_RegistrationProblem },
  { &hf_p7_registration_type, BER_CLASS_CON, 1, 0, dissect_p7_RegistrationTypes },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PAR_register_ms_error(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              PAR_register_ms_error_set, hf_index, ett_p7_PAR_register_ms_error);

  return offset;
}


static const value_string p7_ModifyProblem_vals[] = {
  {   0, "attribute-not-present" },
  {   1, "value-not-present" },
  {   2, "attribute-or-value-already-exists" },
  {   3, "invalid-position" },
  {   4, "modify-restriction-problem" },
  { 0, NULL }
};


static int
dissect_p7_ModifyProblem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            0U, ub_error_reasons, hf_index, NULL);

  return offset;
}


static const ber_sequence_t ModifyErrorParameter_set[] = {
  { &hf_p7_entries_modified , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber },
  { &hf_p7_failing_entry    , BER_CLASS_CON, 1, 0, dissect_p7_SequenceNumber },
  { &hf_p7_modification_number, BER_CLASS_CON, 2, 0, dissect_p7_INTEGER },
  { &hf_p7_modify_problem   , BER_CLASS_CON, 3, 0, dissect_p7_ModifyProblem },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_ModifyErrorParameter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              ModifyErrorParameter_set, hf_index, ett_p7_ModifyErrorParameter);

  return offset;
}


static int * const T_entry_class_problem_bits[] = {
  &hf_p7_T_entry_class_problem_unsupported_entry_class,
  &hf_p7_T_entry_class_problem_entry_class_not_subscribed,
  &hf_p7_T_entry_class_problem_inappropriate_entry_class,
  NULL
};

static int
dissect_p7_T_entry_class_problem(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_bitstring(implicit_tag, actx, tree, tvb, offset,
                                    T_entry_class_problem_bits, 3, hf_index, ett_p7_T_entry_class_problem,
                                    NULL);

  return offset;
}


static const ber_sequence_t EntryClassErrorParameter_set[] = {
  { &hf_p7_entry_class      , BER_CLASS_CON, 0, 0, dissect_p7_EntryClass },
  { &hf_p7_entry_class_problem, BER_CLASS_CON, 1, 0, dissect_p7_T_entry_class_problem },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_EntryClassErrorParameter(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              EntryClassErrorParameter_set, hf_index, ett_p7_EntryClassErrorParameter);

  return offset;
}



static int
dissect_p7_INTEGER_1_ub_recipients(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_constrained_integer(implicit_tag, actx, tree, tvb, offset,
                                                            1U, ub_recipients, hf_index, NULL);

  return offset;
}


static const ber_sequence_t PerRecipientReport_sequence[] = {
  { &hf_p7_report_entry     , BER_CLASS_CON, 0, 0, dissect_p7_SequenceNumber },
  { &hf_p7_position         , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_INTEGER_1_ub_recipients },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_PerRecipientReport(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   PerRecipientReport_sequence, hf_index, ett_p7_PerRecipientReport);

  return offset;
}


static const ber_sequence_t SEQUENCE_OF_PerRecipientReport_sequence_of[1] = {
  { &hf_p7_location_item    , BER_CLASS_UNI, BER_UNI_TAG_SEQUENCE, BER_FLAGS_NOOWNTAG, dissect_p7_PerRecipientReport },
};

static int
dissect_p7_SEQUENCE_OF_PerRecipientReport(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                      SEQUENCE_OF_PerRecipientReport_sequence_of, hf_index, ett_p7_SEQUENCE_OF_PerRecipientReport);

  return offset;
}


static const value_string p7_ReportLocation_vals[] = {
  {   0, "no-correlated-reports" },
  {   1, "location" },
  { 0, NULL }
};

static const ber_choice_t ReportLocation_choice[] = {
  {   0, &hf_p7_no_correlated_reports, BER_CLASS_CON, 0, 0, dissect_p7_NULL },
  {   1, &hf_p7_location         , BER_CLASS_CON, 1, 0, dissect_p7_SEQUENCE_OF_PerRecipientReport },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_ReportLocation(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 ReportLocation_choice, hf_index, ett_p7_ReportLocation,
                                 NULL);

  return offset;
}


static const value_string p7_ReportSummary_vals[] = {
  {   0, "no-report-requested" },
  {   1, "no-report-received" },
  {   2, "report-outstanding" },
  {   3, "delivery-cancelled" },
  {   4, "delivery-report-from-another-recipient" },
  {   5, "non-delivery-report-from-another-recipient" },
  {   6, "delivery-report-from-intended-recipient" },
  {   7, "non-delivery-report-from-intended-recipient" },
  { 0, NULL }
};


static int
dissect_p7_ReportSummary(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                  NULL);

  return offset;
}



static int
dissect_p7_DeferredDeliveryCancellationTime(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_UTCTime(implicit_tag, actx, tree, tvb, offset, hf_index, NULL, NULL);

  return offset;
}



static int
dissect_p7_DeletionTime(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_UTCTime(implicit_tag, actx, tree, tvb, offset, hf_index, NULL, NULL);

  return offset;
}


static const value_string p7_SubmissionError_vals[] = {
  {   1, "submission-control-violated" },
  {   2, "originator-invalid" },
  {   3, "recipient-improperly-specified" },
  {   4, "element-of-service-not-subscribed" },
  {  11, "inconsistent-request" },
  {  12, "security-error" },
  {  13, "unsupported-critical-function" },
  {  15, "remote-bind-error" },
  {  27, "service-error" },
  {  30, "message-group-error" },
  {  31, "ms-extension-error" },
  {  34, "entry-class-error" },
  { 0, NULL }
};

static const ber_choice_t SubmissionError_choice[] = {
  {   1, &hf_p7_submission_control_violated, BER_CLASS_CON, 1, 0, dissect_p7_NULL },
  {   2, &hf_p7_originator_invalid, BER_CLASS_CON, 2, 0, dissect_p7_NULL },
  {   3, &hf_p7_recipient_improperly_specified, BER_CLASS_CON, 3, 0, dissect_p1_ImproperlySpecifiedRecipients },
  {   4, &hf_p7_element_of_service_not_subscribed, BER_CLASS_CON, 4, 0, dissect_p7_NULL },
  {  11, &hf_p7_inconsistent_request, BER_CLASS_CON, 11, 0, dissect_p7_NULL },
  {  12, &hf_p7_security_error   , BER_CLASS_CON, 12, 0, dissect_p1_SecurityProblem },
  {  13, &hf_p7_unsupported_critical_function, BER_CLASS_CON, 13, 0, dissect_p7_NULL },
  {  15, &hf_p7_remote_bind_error, BER_CLASS_CON, 15, 0, dissect_p7_NULL },
  {  27, &hf_p7_service_error    , BER_CLASS_CON, 27, 0, dissect_p7_ServiceErrorParameter },
  {  30, &hf_p7_message_group_error, BER_CLASS_CON, 30, 0, dissect_p7_MessageGroupErrorParameter },
  {  31, &hf_p7_ms_extension_error, BER_CLASS_CON, 31, 0, dissect_p7_MSExtensionErrorParameter },
  {  34, &hf_p7_entry_class_error, BER_CLASS_CON, 34, 0, dissect_p7_EntryClassErrorParameter },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_SubmissionError(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 SubmissionError_choice, hf_index, ett_p7_SubmissionError,
                                 NULL);

  return offset;
}


const value_string p7_SignatureStatus_vals[] = {
  {   0, "signature-absent" },
  {   1, "verification-in-progress" },
  {   2, "verification-succeeded" },
  {   3, "verification-not-possible" },
  {   4, "content-converted" },
  {   5, "signature-encrypted" },
  {   6, "algorithm-not-supported" },
  {   7, "certificate-not-obtainable" },
  {   8, "verification-failed" },
  { 0, NULL }
};


int
dissect_p7_SignatureStatus(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                                NULL);

  return offset;
}


static const ber_sequence_t SignatureVerificationStatus_set[] = {
  { &hf_p7_content_integrity_check, BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_p7_SignatureStatus },
  { &hf_p7_message_origin_authentication_check, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_p7_SignatureStatus },
  { &hf_p7_message_token    , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_SignatureStatus },
  { &hf_p7_report_origin_authentication_check, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_p7_SignatureStatus },
  { &hf_p7_proof_of_delivery, BER_CLASS_CON, 4, BER_FLAGS_OPTIONAL, dissect_p7_SignatureStatus },
  { &hf_p7_proof_of_submission, BER_CLASS_CON, 5, BER_FLAGS_OPTIONAL, dissect_p7_SignatureStatus },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_SignatureVerificationStatus(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              SignatureVerificationStatus_set, hf_index, ett_p7_SignatureVerificationStatus);

  return offset;
}



static int
dissect_p7_StoragePeriod(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                                NULL);

  return offset;
}



static int
dissect_p7_StorageTime(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_UTCTime(implicit_tag, actx, tree, tvb, offset, hf_index, NULL, NULL);

  return offset;
}



static int
dissect_p7_RTTPapdu(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                                NULL);

  return offset;
}



static int
dissect_p7_RTTRapdu(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_octet_string(implicit_tag, actx, tree, tvb, offset, hf_index,
                                       NULL);

  return offset;
}


static const value_string p7_AbortReason_vals[] = {
  {   0, "localSystemProblem" },
  {   1, "invalidParameter" },
  {   2, "unrecognizedActivity" },
  {   3, "temporaryProblem" },
  {   4, "protocolError" },
  {   5, "permanentProblem" },
  {   6, "userError" },
  {   7, "transferCompleted" },
  { 0, NULL }
};


static int
dissect_p7_AbortReason(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                                NULL);

  return offset;
}



static int
dissect_p7_T_userdataAB(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
	offset = dissect_unknown_ber(actx->pinfo, tvb, offset, tree);


  return offset;
}


static const ber_sequence_t RTABapdu_set[] = {
  { &hf_p7_abortReason      , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL|BER_FLAGS_IMPLTAG, dissect_p7_AbortReason },
  { &hf_p7_reflectedParameter, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL|BER_FLAGS_IMPLTAG, dissect_p7_BIT_STRING },
  { &hf_p7_userdataAB       , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_p7_T_userdataAB },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_RTABapdu(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_set(implicit_tag, actx, tree, tvb, offset,
                              RTABapdu_set, hf_index, ett_p7_RTABapdu);

  return offset;
}


static const value_string p7_RTSE_apdus_vals[] = {
  {   0, "rtorq-apdu" },
  {   1, "rtoac-apdu" },
  {   2, "rtorj-apdu" },
  {   3, "rttp-apdu" },
  {   4, "rttr-apdu" },
  {   5, "rtab-apdu" },
  { 0, NULL }
};

static const ber_choice_t RTSE_apdus_choice[] = {
  {   0, &hf_p7_rtorq_apdu       , BER_CLASS_CON, 16, BER_FLAGS_IMPLTAG, dissect_rtse_RTORQapdu },
  {   1, &hf_p7_rtoac_apdu       , BER_CLASS_CON, 17, BER_FLAGS_IMPLTAG, dissect_rtse_RTOACapdu },
  {   2, &hf_p7_rtorj_apdu       , BER_CLASS_CON, 18, BER_FLAGS_IMPLTAG, dissect_rtse_RTORJapdu },
  {   3, &hf_p7_rttp_apdu        , BER_CLASS_UNI, BER_UNI_TAG_INTEGER, BER_FLAGS_NOOWNTAG, dissect_p7_RTTPapdu },
  {   4, &hf_p7_rttr_apdu        , BER_CLASS_UNI, BER_UNI_TAG_OCTETSTRING, BER_FLAGS_NOOWNTAG, dissect_p7_RTTRapdu },
  {   5, &hf_p7_rtab_apdu        , BER_CLASS_CON, 22, BER_FLAGS_IMPLTAG, dissect_p7_RTABapdu },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_p7_RTSE_apdus(bool implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 RTSE_apdus_choice, hf_index, ett_p7_RTSE_apdus,
                                 NULL);

  return offset;
}

/*--- PDUs ---*/

static int dissect_AutoActionType_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_AutoActionType(false, tvb, offset, &asn1_ctx, tree, hf_p7_AutoActionType_PDU);
  return offset;
}
static int dissect_AutoActionError_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_AutoActionError(false, tvb, offset, &asn1_ctx, tree, hf_p7_AutoActionError_PDU);
  return offset;
}
static int dissect_EntryType_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_EntryType(false, tvb, offset, &asn1_ctx, tree, hf_p7_EntryType_PDU);
  return offset;
}
static int dissect_SequenceNumber_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_SequenceNumber(false, tvb, offset, &asn1_ctx, tree, hf_p7_SequenceNumber_PDU);
  return offset;
}
static int dissect_RetrievalStatus_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_RetrievalStatus(false, tvb, offset, &asn1_ctx, tree, hf_p7_RetrievalStatus_PDU);
  return offset;
}
static int dissect_MessageGroupName_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MessageGroupName(false, tvb, offset, &asn1_ctx, tree, hf_p7_MessageGroupName_PDU);
  return offset;
}
static int dissect_MSBindArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MSBindArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_MSBindArgument_PDU);
  return offset;
}
static int dissect_MSBindResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MSBindResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_MSBindResult_PDU);
  return offset;
}
static int dissect_MS_EIT_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MS_EIT(false, tvb, offset, &asn1_ctx, tree, hf_p7_MS_EIT_PDU);
  return offset;
}
static int dissect_ChangeCredentialsAlgorithms_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ChangeCredentialsAlgorithms(false, tvb, offset, &asn1_ctx, tree, hf_p7_ChangeCredentialsAlgorithms_PDU);
  return offset;
}
static int dissect_PAR_ms_bind_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_ms_bind_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_ms_bind_error_PDU);
  return offset;
}
static int dissect_CreationTime_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_CreationTime(false, tvb, offset, &asn1_ctx, tree, hf_p7_CreationTime_PDU);
  return offset;
}
static int dissect_OriginatorToken_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_OriginatorToken(false, tvb, offset, &asn1_ctx, tree, hf_p7_OriginatorToken_PDU);
  return offset;
}
static int dissect_SummarizeArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_SummarizeArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_SummarizeArgument_PDU);
  return offset;
}
static int dissect_SummarizeResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_SummarizeResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_SummarizeResult_PDU);
  return offset;
}
static int dissect_ListArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ListArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_ListArgument_PDU);
  return offset;
}
static int dissect_ListResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ListResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_ListResult_PDU);
  return offset;
}
static int dissect_FetchArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_FetchArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_FetchArgument_PDU);
  return offset;
}
static int dissect_FetchResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_FetchResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_FetchResult_PDU);
  return offset;
}
static int dissect_DeleteArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_DeleteArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_DeleteArgument_PDU);
  return offset;
}
static int dissect_DeleteResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_DeleteResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_DeleteResult_PDU);
  return offset;
}
static int dissect_Register_MSArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_Register_MSArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_Register_MSArgument_PDU);
  return offset;
}
static int dissect_Register_MSResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_Register_MSResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_Register_MSResult_PDU);
  return offset;
}
static int dissect_ProtectedChangeCredentials_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ProtectedChangeCredentials(false, tvb, offset, &asn1_ctx, tree, hf_p7_ProtectedChangeCredentials_PDU);
  return offset;
}
static int dissect_AlertArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_AlertArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_AlertArgument_PDU);
  return offset;
}
static int dissect_AlertResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_AlertResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_AlertResult_PDU);
  return offset;
}
static int dissect_ModifyArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ModifyArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_ModifyArgument_PDU);
  return offset;
}
static int dissect_ModifyResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ModifyResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_ModifyResult_PDU);
  return offset;
}
static int dissect_MSMessageSubmissionArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MSMessageSubmissionArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_MSMessageSubmissionArgument_PDU);
  return offset;
}
static int dissect_MSMessageSubmissionResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MSMessageSubmissionResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_MSMessageSubmissionResult_PDU);
  return offset;
}
static int dissect_MSProbeSubmissionArgument_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MSProbeSubmissionArgument(false, tvb, offset, &asn1_ctx, tree, hf_p7_MSProbeSubmissionArgument_PDU);
  return offset;
}
static int dissect_MSProbeSubmissionResult_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MSProbeSubmissionResult(false, tvb, offset, &asn1_ctx, tree, hf_p7_MSProbeSubmissionResult_PDU);
  return offset;
}
static int dissect_PAR_attribute_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_attribute_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_attribute_error_PDU);
  return offset;
}
static int dissect_PAR_auto_action_request_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_auto_action_request_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_auto_action_request_error_PDU);
  return offset;
}
static int dissect_PAR_delete_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_delete_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_delete_error_PDU);
  return offset;
}
static int dissect_PAR_fetch_restriction_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_fetch_restriction_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_fetch_restriction_error_PDU);
  return offset;
}
static int dissect_PAR_invalid_parameters_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_invalid_parameters_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_invalid_parameters_error_PDU);
  return offset;
}
static int dissect_PAR_range_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_range_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_range_error_PDU);
  return offset;
}
static int dissect_PAR_sequence_number_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_sequence_number_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_sequence_number_error_PDU);
  return offset;
}
static int dissect_ServiceErrorParameter_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ServiceErrorParameter(false, tvb, offset, &asn1_ctx, tree, hf_p7_ServiceErrorParameter_PDU);
  return offset;
}
static int dissect_MessageGroupErrorParameter_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MessageGroupErrorParameter(false, tvb, offset, &asn1_ctx, tree, hf_p7_MessageGroupErrorParameter_PDU);
  return offset;
}
static int dissect_MSExtensionErrorParameter_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_MSExtensionErrorParameter(false, tvb, offset, &asn1_ctx, tree, hf_p7_MSExtensionErrorParameter_PDU);
  return offset;
}
static int dissect_PAR_register_ms_error_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PAR_register_ms_error(false, tvb, offset, &asn1_ctx, tree, hf_p7_PAR_register_ms_error_PDU);
  return offset;
}
static int dissect_ModifyErrorParameter_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ModifyErrorParameter(false, tvb, offset, &asn1_ctx, tree, hf_p7_ModifyErrorParameter_PDU);
  return offset;
}
static int dissect_EntryClassErrorParameter_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_EntryClassErrorParameter(false, tvb, offset, &asn1_ctx, tree, hf_p7_EntryClassErrorParameter_PDU);
  return offset;
}
static int dissect_ReportLocation_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ReportLocation(false, tvb, offset, &asn1_ctx, tree, hf_p7_ReportLocation_PDU);
  return offset;
}
static int dissect_PerRecipientReport_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_PerRecipientReport(false, tvb, offset, &asn1_ctx, tree, hf_p7_PerRecipientReport_PDU);
  return offset;
}
static int dissect_ReportSummary_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_ReportSummary(false, tvb, offset, &asn1_ctx, tree, hf_p7_ReportSummary_PDU);
  return offset;
}
static int dissect_DeferredDeliveryCancellationTime_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_DeferredDeliveryCancellationTime(false, tvb, offset, &asn1_ctx, tree, hf_p7_DeferredDeliveryCancellationTime_PDU);
  return offset;
}
static int dissect_DeletionTime_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_DeletionTime(false, tvb, offset, &asn1_ctx, tree, hf_p7_DeletionTime_PDU);
  return offset;
}
static int dissect_SubmissionError_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_SubmissionError(false, tvb, offset, &asn1_ctx, tree, hf_p7_SubmissionError_PDU);
  return offset;
}
static int dissect_SignatureVerificationStatus_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_SignatureVerificationStatus(false, tvb, offset, &asn1_ctx, tree, hf_p7_SignatureVerificationStatus_PDU);
  return offset;
}
static int dissect_StoragePeriod_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_StoragePeriod(false, tvb, offset, &asn1_ctx, tree, hf_p7_StoragePeriod_PDU);
  return offset;
}
static int dissect_StorageTime_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_StorageTime(false, tvb, offset, &asn1_ctx, tree, hf_p7_StorageTime_PDU);
  return offset;
}
static int dissect_RTSE_apdus_PDU(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, void *data _U_) {
  int offset = 0;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);
  offset = dissect_p7_RTSE_apdus(false, tvb, offset, &asn1_ctx, tree, hf_p7_RTSE_apdus_PDU);
  return offset;
}



static const ros_opr_t p7_opr_tab[] = {
  /* ms-bind */
  { op_ros_bind              ,	dissect_MSBindArgument_PDU,	dissect_MSBindResult_PDU },
  /* summarize */
  { op_summarize             ,	dissect_SummarizeArgument_PDU,	dissect_SummarizeResult_PDU },
  /* list */
  { op_list                  ,	dissect_ListArgument_PDU,	dissect_ListResult_PDU },
  /* fetch */
  { op_fetch                 ,	dissect_FetchArgument_PDU,	dissect_FetchResult_PDU },
  /* delete */
  { op_delete                ,	dissect_DeleteArgument_PDU,	dissect_DeleteResult_PDU },
  /* register-MS */
  { op_register_ms           ,	dissect_Register_MSArgument_PDU,	dissect_Register_MSResult_PDU },
  /* alert */
  { op_alert                 ,	dissect_AlertArgument_PDU,	dissect_AlertResult_PDU },
  /* modify */
  { op_modify                ,	dissect_ModifyArgument_PDU,	dissect_ModifyResult_PDU },
  /* ms-message-submission */
  { op_ms_message_submission ,	dissect_MSMessageSubmissionArgument_PDU,	dissect_MSMessageSubmissionResult_PDU },
  /* ms-probe-submission */
  { op_ms_probe_submission   ,	dissect_MSProbeSubmissionArgument_PDU,	dissect_MSProbeSubmissionResult_PDU },
  { 0,				(dissector_t)(-1),	(dissector_t)(-1) },
};


static const ros_err_t p7_err_tab[] = {
  /* ms-bind-error*/
  { err_ros_bind,	dissect_PAR_ms_bind_error_PDU },
  /* attribute-error*/
  { err_attribute_error,	dissect_PAR_attribute_error_PDU },
  /* auto-action-request-error*/
  { err_auto_action_request_error,	dissect_PAR_auto_action_request_error_PDU },
  /* delete-error*/
  { err_delete_error,	dissect_PAR_delete_error_PDU },
  /* fetch-restriction-error*/
  { err_fetch_restriction_error,	dissect_PAR_fetch_restriction_error_PDU },
  /* invalid-parameters-error*/
  { err_invalid_parameters_error,	dissect_PAR_invalid_parameters_error_PDU },
  /* range-error*/
  { err_range_error,	dissect_PAR_range_error_PDU },
  /* sequence-number-error*/
  { err_sequence_number_error,	dissect_PAR_sequence_number_error_PDU },
  /* service-error*/
  { err_service_error,	dissect_ServiceErrorParameter_PDU },
  /* message-group-error*/
  { err_message_group_error,	dissect_MessageGroupErrorParameter_PDU },
  /* ms-extension-error*/
  { err_ms_extension_error,	dissect_MSExtensionErrorParameter_PDU },
  /* register-ms-error*/
  { err_register_ms_error,	dissect_PAR_register_ms_error_PDU },
  /* modify-error*/
  { err_modify_error,	dissect_ModifyErrorParameter_PDU },
  /* entry-class-error*/
  { err_entry_class_error,	dissect_EntryClassErrorParameter_PDU },
  { 0,	(dissector_t)(-1) },
};


static const ros_info_t p7_ros_info = {
  "P7",
  &proto_p7,
  &ett_p7,
  p7_opr_code_string_vals,
  p7_opr_tab,
  p7_err_code_string_vals,
  p7_err_tab
};


/*--- proto_register_p7 -------------------------------------------*/
void proto_register_p7(void) {

  /* List of fields */
  static hf_register_info hf[] =
  {
    { &hf_p7_AutoActionType_PDU,
      { "AutoActionType", "p7.AutoActionType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_AutoActionError_PDU,
      { "AutoActionError", "p7.AutoActionError_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_EntryType_PDU,
      { "EntryType", "p7.EntryType",
        FT_INT32, BASE_DEC, VALS(p7_EntryType_vals), 0,
        NULL, HFILL }},
    { &hf_p7_SequenceNumber_PDU,
      { "SequenceNumber", "p7.SequenceNumber",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_RetrievalStatus_PDU,
      { "RetrievalStatus", "p7.RetrievalStatus",
        FT_INT32, BASE_DEC, VALS(p7_RetrievalStatus_vals), 0,
        NULL, HFILL }},
    { &hf_p7_MessageGroupName_PDU,
      { "MessageGroupName", "p7.MessageGroupName",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MSBindArgument_PDU,
      { "MSBindArgument", "p7.MSBindArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MSBindResult_PDU,
      { "MSBindResult", "p7.MSBindResult_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MS_EIT_PDU,
      { "MS-EIT", "p7.MS_EIT",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ChangeCredentialsAlgorithms_PDU,
      { "ChangeCredentialsAlgorithms", "p7.ChangeCredentialsAlgorithms",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_PAR_ms_bind_error_PDU,
      { "PAR-ms-bind-error", "p7.PAR_ms_bind_error",
        FT_UINT32, BASE_DEC, VALS(p7_PAR_ms_bind_error_vals), 0,
        NULL, HFILL }},
    { &hf_p7_CreationTime_PDU,
      { "CreationTime", "p7.CreationTime",
        FT_STRING, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_OriginatorToken_PDU,
      { "OriginatorToken", "p7.OriginatorToken_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_SummarizeArgument_PDU,
      { "SummarizeArgument", "p7.SummarizeArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_SummarizeResult_PDU,
      { "SummarizeResult", "p7.SummarizeResult_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ListArgument_PDU,
      { "ListArgument", "p7.ListArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ListResult_PDU,
      { "ListResult", "p7.ListResult_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_FetchArgument_PDU,
      { "FetchArgument", "p7.FetchArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_FetchResult_PDU,
      { "FetchResult", "p7.FetchResult_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_DeleteArgument_PDU,
      { "DeleteArgument", "p7.DeleteArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_DeleteResult_PDU,
      { "DeleteResult", "p7.DeleteResult",
        FT_UINT32, BASE_DEC, VALS(p7_DeleteResult_vals), 0,
        NULL, HFILL }},
    { &hf_p7_Register_MSArgument_PDU,
      { "Register-MSArgument", "p7.Register_MSArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_Register_MSResult_PDU,
      { "Register-MSResult", "p7.Register_MSResult",
        FT_UINT32, BASE_DEC, VALS(p7_Register_MSResult_vals), 0,
        NULL, HFILL }},
    { &hf_p7_ProtectedChangeCredentials_PDU,
      { "ProtectedChangeCredentials", "p7.ProtectedChangeCredentials_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_AlertArgument_PDU,
      { "AlertArgument", "p7.AlertArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_AlertResult_PDU,
      { "AlertResult", "p7.AlertResult_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ModifyArgument_PDU,
      { "ModifyArgument", "p7.ModifyArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ModifyResult_PDU,
      { "ModifyResult", "p7.ModifyResult_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MSMessageSubmissionArgument_PDU,
      { "MSMessageSubmissionArgument", "p7.MSMessageSubmissionArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MSMessageSubmissionResult_PDU,
      { "MSMessageSubmissionResult", "p7.MSMessageSubmissionResult",
        FT_UINT32, BASE_DEC, VALS(p7_MSMessageSubmissionResult_vals), 0,
        NULL, HFILL }},
    { &hf_p7_MSProbeSubmissionArgument_PDU,
      { "MSProbeSubmissionArgument", "p7.MSProbeSubmissionArgument_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MSProbeSubmissionResult_PDU,
      { "MSProbeSubmissionResult", "p7.MSProbeSubmissionResult_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_PAR_attribute_error_PDU,
      { "PAR-attribute-error", "p7.PAR_attribute_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_PAR_auto_action_request_error_PDU,
      { "PAR-auto-action-request-error", "p7.PAR_auto_action_request_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_PAR_delete_error_PDU,
      { "PAR-delete-error", "p7.PAR_delete_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_PAR_fetch_restriction_error_PDU,
      { "PAR-fetch-restriction-error", "p7.PAR_fetch_restriction_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_PAR_invalid_parameters_error_PDU,
      { "PAR-invalid-parameters-error", "p7.PAR_invalid_parameters_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_PAR_range_error_PDU,
      { "PAR-range-error", "p7.PAR_range_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_PAR_sequence_number_error_PDU,
      { "PAR-sequence-number-error", "p7.PAR_sequence_number_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ServiceErrorParameter_PDU,
      { "ServiceErrorParameter", "p7.ServiceErrorParameter_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MessageGroupErrorParameter_PDU,
      { "MessageGroupErrorParameter", "p7.MessageGroupErrorParameter_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MSExtensionErrorParameter_PDU,
      { "MSExtensionErrorParameter", "p7.MSExtensionErrorParameter",
        FT_UINT32, BASE_DEC, VALS(p7_MSExtensionErrorParameter_vals), 0,
        NULL, HFILL }},
    { &hf_p7_PAR_register_ms_error_PDU,
      { "PAR-register-ms-error", "p7.PAR_register_ms_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ModifyErrorParameter_PDU,
      { "ModifyErrorParameter", "p7.ModifyErrorParameter_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_EntryClassErrorParameter_PDU,
      { "EntryClassErrorParameter", "p7.EntryClassErrorParameter_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ReportLocation_PDU,
      { "ReportLocation", "p7.ReportLocation",
        FT_UINT32, BASE_DEC, VALS(p7_ReportLocation_vals), 0,
        NULL, HFILL }},
    { &hf_p7_PerRecipientReport_PDU,
      { "PerRecipientReport", "p7.PerRecipientReport_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ReportSummary_PDU,
      { "ReportSummary", "p7.ReportSummary",
        FT_UINT32, BASE_DEC, VALS(p7_ReportSummary_vals), 0,
        NULL, HFILL }},
    { &hf_p7_DeferredDeliveryCancellationTime_PDU,
      { "DeferredDeliveryCancellationTime", "p7.DeferredDeliveryCancellationTime",
        FT_STRING, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_DeletionTime_PDU,
      { "DeletionTime", "p7.DeletionTime",
        FT_STRING, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_SubmissionError_PDU,
      { "SubmissionError", "p7.SubmissionError",
        FT_UINT32, BASE_DEC, VALS(p7_SubmissionError_vals), 0,
        NULL, HFILL }},
    { &hf_p7_SignatureVerificationStatus_PDU,
      { "SignatureVerificationStatus", "p7.SignatureVerificationStatus_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_StoragePeriod_PDU,
      { "StoragePeriod", "p7.StoragePeriod",
        FT_INT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_StorageTime_PDU,
      { "StorageTime", "p7.StorageTime",
        FT_STRING, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_RTSE_apdus_PDU,
      { "RTSE-apdus", "p7.RTSE_apdus",
        FT_UINT32, BASE_DEC, VALS(p7_RTSE_apdus_vals), 0,
        NULL, HFILL }},
    { &hf_p7_attribute_type,
      { "attribute-type", "p7.attribute_type",
        FT_OID, BASE_NONE, NULL, 0,
        "AttributeType", HFILL }},
    { &hf_p7_attribute_values,
      { "attribute-values", "p7.attribute_values",
        FT_UINT32, BASE_DEC, NULL, 0,
        "AttributeValues", HFILL }},
    { &hf_p7_attribute_values_item,
      { "attribute-values item", "p7.attribute_values_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "AttributeItem", HFILL }},
    { &hf_p7_auto_action_type,
      { "auto-action-type", "p7.auto_action_type",
        FT_OID, BASE_NONE, NULL, 0,
        "AutoActionType", HFILL }},
    { &hf_p7_registration_identifier,
      { "registration-identifier", "p7.registration_identifier",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_per_auto_action", HFILL }},
    { &hf_p7_registration_parameter,
      { "registration-parameter", "p7.registration_parameter_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_error_code,
      { "error-code", "p7.error_code_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_error_parameter,
      { "error-parameter", "p7.error_parameter_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MSExtensions_item,
      { "MSExtensionItem", "p7.MSExtensionItem_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_MessageGroupName_item,
      { "GroupNamePart", "p7.GroupNamePart",
        FT_STRING, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_initiator_name,
      { "initiator-name", "p7.initiator_name_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_initiator_credentials,
      { "initiator-credentials", "p7.initiator_credentials",
        FT_UINT32, BASE_DEC, VALS(p1_Credentials_vals), 0,
        "InitiatorCredentials", HFILL }},
    { &hf_p7_security_context,
      { "security-context", "p7.security_context",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SecurityContext", HFILL }},
    { &hf_p7_fetch_restrictions,
      { "fetch-restrictions", "p7.fetch_restrictions_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "Restrictions", HFILL }},
    { &hf_p7_ms_configuration_request,
      { "ms-configuration-request", "p7.ms_configuration_request",
        FT_BOOLEAN, BASE_NONE, NULL, 0,
        "BOOLEAN", HFILL }},
    { &hf_p7_ua_registration_identifier,
      { "ua-registration-identifier", "p7.ua_registration_identifier",
        FT_STRING, BASE_NONE, NULL, 0,
        "RegistrationIdentifier", HFILL }},
    { &hf_p7_bind_extensions,
      { "bind-extensions", "p7.bind_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_allowed_content_types,
      { "allowed-content-types", "p7.allowed_content_types",
        FT_UINT32, BASE_DEC, NULL, 0,
        "T_allowed_content_types", HFILL }},
    { &hf_p7_allowed_content_types_item,
      { "allowed-content-types item", "p7.allowed_content_types_item",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_allowed_EITs,
      { "allowed-EITs", "p7.allowed_EITs",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MS_EITs", HFILL }},
    { &hf_p7_maximum_attribute_length,
      { "maximum-attribute-length", "p7.maximum_attribute_length",
        FT_INT32, BASE_DEC, NULL, 0,
        "INTEGER", HFILL }},
    { &hf_p7_MS_EITs_item,
      { "MS-EIT", "p7.MS_EIT",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_responder_credentials,
      { "responder-credentials", "p7.responder_credentials",
        FT_UINT32, BASE_DEC, VALS(p1_Credentials_vals), 0,
        "ResponderCredentials", HFILL }},
    { &hf_p7_available_auto_actions,
      { "available-auto-actions", "p7.available_auto_actions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_auto_actions_OF_AutoActionType", HFILL }},
    { &hf_p7_available_auto_actions_item,
      { "AutoActionType", "p7.AutoActionType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_available_attribute_types,
      { "available-attribute-types", "p7.available_attribute_types",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_attributes_supported_OF_AttributeType", HFILL }},
    { &hf_p7_available_attribute_types_item,
      { "AttributeType", "p7.AttributeType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_alert_indication,
      { "alert-indication", "p7.alert_indication",
        FT_BOOLEAN, BASE_NONE, NULL, 0,
        "BOOLEAN", HFILL }},
    { &hf_p7_content_types_supported,
      { "content-types-supported", "p7.content_types_supported",
        FT_UINT32, BASE_DEC, NULL, 0,
        "T_content_types_supported", HFILL }},
    { &hf_p7_content_types_supported_item,
      { "content-types-supported item", "p7.content_types_supported_item",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_entry_classes_supported,
      { "entry-classes-supported", "p7.entry_classes_supported",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_entry_classes_OF_EntryClass", HFILL }},
    { &hf_p7_entry_classes_supported_item,
      { "EntryClass", "p7.EntryClass",
        FT_UINT32, BASE_DEC, VALS(p7_EntryClass_vals), 0,
        NULL, HFILL }},
    { &hf_p7_matching_rules_supported,
      { "matching-rules-supported", "p7.matching_rules_supported",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_matching_rules_supported_item,
      { "matching-rules-supported item", "p7.matching_rules_supported_item",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_bind_result_extensions,
      { "bind-result-extensions", "p7.bind_result_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_message_group_depth,
      { "message-group-depth", "p7.message_group_depth",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_group_depth", HFILL }},
    { &hf_p7_auto_action_error_indication,
      { "auto-action-error-indication", "p7.auto_action_error_indication",
        FT_UINT32, BASE_DEC, VALS(p7_AutoActionErrorIndication_vals), 0,
        "AutoActionErrorIndication", HFILL }},
    { &hf_p7_unsupported_extensions,
      { "unsupported-extensions", "p7.unsupported_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_unsupported_extensions_item,
      { "unsupported-extensions item", "p7.unsupported_extensions_item",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_ua_registration_id_unknown,
      { "ua-registration-id-unknown", "p7.ua_registration_id_unknown",
        FT_BOOLEAN, BASE_NONE, NULL, 0,
        "BOOLEAN", HFILL }},
    { &hf_p7_service_information,
      { "service-information", "p7.service_information",
        FT_STRING, BASE_NONE, NULL, 0,
        "GeneralString_SIZE_1_ub_service_information_length", HFILL }},
    { &hf_p7_ChangeCredentialsAlgorithms_item,
      { "ChangeCredentialsAlgorithms item", "p7.ChangeCredentialsAlgorithms_item",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_indication_only,
      { "indication-only", "p7.indication_only_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_auto_action_log_entry,
      { "auto-action-log-entry", "p7.auto_action_log_entry",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_unqualified_error,
      { "unqualified-error", "p7.unqualified_error",
        FT_UINT32, BASE_DEC, VALS(p7_BindProblem_vals), 0,
        "BindProblem", HFILL }},
    { &hf_p7_qualified_error,
      { "qualified-error", "p7.qualified_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_bind_problem,
      { "bind-problem", "p7.bind_problem",
        FT_UINT32, BASE_DEC, VALS(p7_BindProblem_vals), 0,
        "BindProblem", HFILL }},
    { &hf_p7_supplementary_information,
      { "supplementary-information", "p7.supplementary_information",
        FT_STRING, BASE_NONE, NULL, 0,
        "GeneralString_SIZE_1_ub_supplementary_info_length", HFILL }},
    { &hf_p7_bind_extension_errors,
      { "bind-extension-errors", "p7.bind_extension_errors",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_bind_extension_errors_item,
      { "bind-extension-errors item", "p7.bind_extension_errors_item",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_sequence_number_range,
      { "sequence-number-range", "p7.sequence_number_range_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "NumberRange", HFILL }},
    { &hf_p7_creation_time_range,
      { "creation-time-range", "p7.creation_time_range_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "TimeRange", HFILL }},
    { &hf_p7_from_number,
      { "from", "p7.numberRange.number",
        FT_UINT32, BASE_DEC, NULL, 0,
        "T_from_number", HFILL }},
    { &hf_p7_to_number,
      { "to", "p7.NumberRange.to",
        FT_UINT32, BASE_DEC, NULL, 0,
        "T_to_number", HFILL }},
    { &hf_p7_from_time,
      { "from", "p7.timeRange.time",
        FT_STRING, BASE_NONE, NULL, 0,
        "CreationTime", HFILL }},
    { &hf_p7_to_time,
      { "to", "p7.timeRange.to",
        FT_STRING, BASE_NONE, NULL, 0,
        "CreationTime", HFILL }},
    { &hf_p7_filter_item,
      { "item", "p7.filter_item",
        FT_UINT32, BASE_DEC, VALS(p7_FilterItem_vals), 0,
        "FilterItem", HFILL }},
    { &hf_p7_and,
      { "and", "p7.and",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_OF_Filter", HFILL }},
    { &hf_p7_and_item,
      { "Filter", "p7.Filter",
        FT_UINT32, BASE_DEC, VALS(p7_Filter_vals), 0,
        NULL, HFILL }},
    { &hf_p7_or,
      { "or", "p7.or",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_OF_Filter", HFILL }},
    { &hf_p7_or_item,
      { "Filter", "p7.Filter",
        FT_UINT32, BASE_DEC, VALS(p7_Filter_vals), 0,
        NULL, HFILL }},
    { &hf_p7_not,
      { "not", "p7.not",
        FT_UINT32, BASE_DEC, VALS(p7_Filter_vals), 0,
        "Filter", HFILL }},
    { &hf_p7_equality,
      { "equality", "p7.equality_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "AttributeValueAssertion", HFILL }},
    { &hf_p7_substrings,
      { "substrings", "p7.substrings_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_type,
      { "type", "p7.type",
        FT_OID, BASE_NONE, NULL, 0,
        "AttributeType", HFILL }},
    { &hf_p7_strings,
      { "strings", "p7.strings",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_strings_item,
      { "strings item", "p7.strings_item",
        FT_UINT32, BASE_DEC, VALS(p7_T_strings_item_vals), 0,
        NULL, HFILL }},
    { &hf_p7_initial,
      { "initial", "p7.initial_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_any,
      { "any", "p7.any_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_final,
      { "final", "p7.final_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_greater_or_equal,
      { "greater-or-equal", "p7.greater_or_equal_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "AttributeValueAssertion", HFILL }},
    { &hf_p7_less_or_equal,
      { "less-or-equal", "p7.less_or_equal_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "AttributeValueAssertion", HFILL }},
    { &hf_p7_present,
      { "present", "p7.present",
        FT_OID, BASE_NONE, NULL, 0,
        "AttributeType", HFILL }},
    { &hf_p7_approximate_match,
      { "approximate-match", "p7.approximate_match_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "AttributeValueAssertion", HFILL }},
    { &hf_p7_other_match,
      { "other-match", "p7.other_match_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MatchingRuleAssertion", HFILL }},
    { &hf_p7_matching_rule,
      { "matching-rule", "p7.matching_rule",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_match_value,
      { "match-value", "p7.match_value_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_attribute_value,
      { "attribute-value", "p7.attribute_value_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_child_entries,
      { "child-entries", "p7.child_entries",
        FT_BOOLEAN, BASE_NONE, NULL, 0,
        "BOOLEAN", HFILL }},
    { &hf_p7_range,
      { "range", "p7.range",
        FT_UINT32, BASE_DEC, VALS(p7_Range_vals), 0,
        NULL, HFILL }},
    { &hf_p7_filter,
      { "filter", "p7.filter",
        FT_UINT32, BASE_DEC, VALS(p7_Filter_vals), 0,
        NULL, HFILL }},
    { &hf_p7_limit,
      { "limit", "p7.limit",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_messages", HFILL }},
    { &hf_p7_override,
      { "override", "p7.override",
        FT_BYTES, BASE_NONE, NULL, 0,
        "OverrideRestrictions", HFILL }},
    { &hf_p7_EntryInformationSelection_item,
      { "AttributeSelection", "p7.AttributeSelection_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_from,
      { "from", "p7.from",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_attribute_values", HFILL }},
    { &hf_p7_selection_count,
      { "count", "p7.selection_count",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_0_ub_attribute_values", HFILL }},
    { &hf_p7_sequence_number,
      { "sequence-number", "p7.sequence_number",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_attributes,
      { "attributes", "p7.attributes",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_per_entry_OF_Attribute", HFILL }},
    { &hf_p7_attributes_item,
      { "Attribute", "p7.Attribute_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_value_count_exceeded,
      { "value-count-exceeded", "p7.value_count_exceeded",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_per_entry_OF_AttributeValueCount", HFILL }},
    { &hf_p7_value_count_exceeded_item,
      { "AttributeValueCount", "p7.AttributeValueCount_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_total,
      { "total", "p7.total",
        FT_INT32, BASE_DEC, NULL, 0,
        "INTEGER", HFILL }},
    { &hf_p7_object_entry_class,
      { "object-entry-class", "p7.object_entry_class",
        FT_UINT32, BASE_DEC, VALS(p7_EntryClass_vals), 0,
        "EntryClass", HFILL }},
    { &hf_p7_disable_auto_modify,
      { "disable-auto-modify", "p7.disable_auto_modify",
        FT_BOOLEAN, BASE_NONE, NULL, 0,
        "BOOLEAN", HFILL }},
    { &hf_p7_add_message_group_names,
      { "add-message-group-names", "p7.add_message_group_names",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_message_groups_OF_MessageGroupName", HFILL }},
    { &hf_p7_add_message_group_names_item,
      { "MessageGroupName", "p7.MessageGroupName",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ms_submission_extensions,
      { "ms-submission-extensions", "p7.ms_submission_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_created_entry,
      { "created-entry", "p7.created_entry",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_ms_submission_result_extensions,
      { "ms-submission-result-extensions", "p7.ms_submission_result_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_entry_class,
      { "entry-class", "p7.entry_class",
        FT_UINT32, BASE_DEC, VALS(p7_EntryClass_vals), 0,
        "EntryClass", HFILL }},
    { &hf_p7_selector,
      { "selector", "p7.selector_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_summary_requests,
      { "summary-requests", "p7.summary_requests",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType", HFILL }},
    { &hf_p7_summary_requests_item,
      { "AttributeType", "p7.AttributeType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_summarize_extensions,
      { "summarize-extensions", "p7.summarize_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_next,
      { "next", "p7.next",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_count,
      { "count", "p7.count",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_span,
      { "span", "p7.span_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_summaries,
      { "summaries", "p7.summaries",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_SIZE_1_ub_summaries_OF_Summary", HFILL }},
    { &hf_p7_summaries_item,
      { "Summary", "p7.Summary_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_summarize_result_extensions,
      { "summarize-result-extensions", "p7.summarize_result_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_lowest,
      { "lowest", "p7.lowest",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_highest,
      { "highest", "p7.highest",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_absent,
      { "absent", "p7.absent",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_messages", HFILL }},
    { &hf_p7_summary_present,
      { "present", "p7.summary.present",
        FT_UINT32, BASE_DEC, NULL, 0,
        "T_summary_present", HFILL }},
    { &hf_p7_summary_present_item,
      { "present item", "p7.summary_present_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "T_summary_present_item", HFILL }},
    { &hf_p7_value,
      { "value", "p7.value_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "SummaryPresentItemValue", HFILL }},
    { &hf_p7_summary_count,
      { "count", "p7.summary_count",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_messages", HFILL }},
    { &hf_p7_requested_attributes,
      { "requested-attributes", "p7.requested_attributes",
        FT_UINT32, BASE_DEC, NULL, 0,
        "EntryInformationSelection", HFILL }},
    { &hf_p7_list_extensions,
      { "list-extensions", "p7.list_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_requested,
      { "requested", "p7.requested",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation", HFILL }},
    { &hf_p7_requested_item,
      { "EntryInformation", "p7.EntryInformation_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_list_result_extensions,
      { "list-result-extensions", "p7.list_result_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_item,
      { "item", "p7.item",
        FT_UINT32, BASE_DEC, VALS(p7_T_item_vals), 0,
        NULL, HFILL }},
    { &hf_p7_search,
      { "search", "p7.search_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "Selector", HFILL }},
    { &hf_p7_precise,
      { "precise", "p7.precise",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_fetch_extensions,
      { "fetch-extensions", "p7.fetch_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_entry_information,
      { "entry-information", "p7.entry_information_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "EntryInformation", HFILL }},
    { &hf_p7_list,
      { "list", "p7.list",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber", HFILL }},
    { &hf_p7_list_item,
      { "SequenceNumber", "p7.SequenceNumber",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_fetch_result_extensions,
      { "fetch-result-extensions", "p7.fetch_result_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_items,
      { "items", "p7.items",
        FT_UINT32, BASE_DEC, VALS(p7_T_items_vals), 0,
        NULL, HFILL }},
    { &hf_p7_sequence_numbers,
      { "sequence-numbers", "p7.sequence_numbers",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_messages_OF_SequenceNumber", HFILL }},
    { &hf_p7_sequence_numbers_item,
      { "SequenceNumber", "p7.SequenceNumber",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_delete_extensions,
      { "delete-extensions", "p7.delete_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_delete_result_88,
      { "delete-result-88", "p7.delete_result_88_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_delete_result_94,
      { "delete-result-94", "p7.delete_result_94_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "T_delete_result_94", HFILL }},
    { &hf_p7_entries_deleted_94,
      { "entries-deleted", "p7.entries_deleted_94",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber", HFILL }},
    { &hf_p7_entries_deleted_94_item,
      { "SequenceNumber", "p7.SequenceNumber",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_delete_result_extensions,
      { "delete-result-extensions", "p7.delete_result_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_auto_action_registrations,
      { "auto-action-registrations", "p7.auto_action_registrations",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration", HFILL }},
    { &hf_p7_auto_action_registrations_item,
      { "AutoActionRegistration", "p7.AutoActionRegistration_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_auto_action_deregistrations,
      { "auto-action-deregistrations", "p7.auto_action_deregistrations",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration", HFILL }},
    { &hf_p7_auto_action_deregistrations_item,
      { "AutoActionDeregistration", "p7.AutoActionDeregistration_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_list_attribute_defaults,
      { "list-attribute-defaults", "p7.list_attribute_defaults",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_0_ub_default_registrations_OF_AttributeType", HFILL }},
    { &hf_p7_list_attribute_defaults_item,
      { "AttributeType", "p7.AttributeType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_fetch_attribute_defaults,
      { "fetch-attribute-defaults", "p7.fetch_attribute_defaults",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_0_ub_default_registrations_OF_AttributeType", HFILL }},
    { &hf_p7_fetch_attribute_defaults_item,
      { "AttributeType", "p7.AttributeType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_change_credentials,
      { "change-credentials", "p7.change_credentials_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_register_old_credentials,
      { "old-credentials", "p7.register_old_credentials",
        FT_UINT32, BASE_DEC, VALS(p1_Credentials_vals), 0,
        "Credentials", HFILL }},
    { &hf_p7_new_credentials,
      { "new-credentials", "p7.new_credentials",
        FT_UINT32, BASE_DEC, VALS(p1_Credentials_vals), 0,
        "Credentials", HFILL }},
    { &hf_p7_user_security_labels,
      { "user-security-labels", "p7.user_security_labels",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel", HFILL }},
    { &hf_p7_user_security_labels_item,
      { "SecurityLabel", "p7.SecurityLabel_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ua_registrations,
      { "ua-registrations", "p7.ua_registrations",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_ua_registrations_OF_UARegistration", HFILL }},
    { &hf_p7_ua_registrations_item,
      { "UARegistration", "p7.UARegistration_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_submission_defaults,
      { "submission-defaults", "p7.submission_defaults_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MSSubmissionOptions", HFILL }},
    { &hf_p7_message_group_registrations,
      { "message-group-registrations", "p7.message_group_registrations",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MessageGroupRegistrations", HFILL }},
    { &hf_p7_registration_status_request,
      { "registration-status-request", "p7.registration_status_request_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "RegistrationTypes", HFILL }},
    { &hf_p7_register_ms_extensions,
      { "register-ms-extensions", "p7.register_ms_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_ua_list_attribute_defaults,
      { "ua-list-attribute-defaults", "p7.ua_list_attribute_defaults",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_0_ub_default_registrations_OF_AttributeType", HFILL }},
    { &hf_p7_ua_list_attribute_defaults_item,
      { "AttributeType", "p7.AttributeType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ua_fetch_attribute_defaults,
      { "ua-fetch-attribute-defaults", "p7.ua_fetch_attribute_defaults",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_0_ub_default_registrations_OF_AttributeType", HFILL }},
    { &hf_p7_ua_fetch_attribute_defaults_item,
      { "AttributeType", "p7.AttributeType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ua_submission_defaults,
      { "ua-submission-defaults", "p7.ua_submission_defaults_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MSSubmissionOptions", HFILL }},
    { &hf_p7_content_specific_defaults,
      { "content-specific-defaults", "p7.content_specific_defaults",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_MessageGroupRegistrations_item,
      { "MessageGroupRegistrations item", "p7.MessageGroupRegistrations_item",
        FT_UINT32, BASE_DEC, VALS(p7_MessageGroupRegistrations_item_vals), 0,
        NULL, HFILL }},
    { &hf_p7_register_group,
      { "register-group", "p7.register_group_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MessageGroupNameAndDescriptor", HFILL }},
    { &hf_p7_deregister_group,
      { "deregister-group", "p7.deregister_group",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MessageGroupName", HFILL }},
    { &hf_p7_change_descriptors,
      { "change-descriptors", "p7.change_descriptors_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MessageGroupNameAndDescriptor", HFILL }},
    { &hf_p7_message_group_name,
      { "message-group-name", "p7.message_group_name",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MessageGroupName", HFILL }},
    { &hf_p7_message_group_descriptor,
      { "message-group-descriptor", "p7.message_group_descriptor",
        FT_STRING, BASE_NONE, NULL, 0,
        "GeneralString_SIZE_1_ub_group_descriptor_length", HFILL }},
    { &hf_p7_registrations,
      { "registrations", "p7.registrations",
        FT_BYTES, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_extended_registrations,
      { "extended-registrations", "p7.extended_registrations",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_extended_registrations_item,
      { "extended-registrations item", "p7.extended_registrations_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_restrict_message_groups,
      { "restrict-message-groups", "p7.restrict_message_groups_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MessageGroupsRestriction", HFILL }},
    { &hf_p7_parent_group,
      { "parent-group", "p7.parent_group",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MessageGroupName", HFILL }},
    { &hf_p7_immediate_descendants_only,
      { "immediate-descendants-only", "p7.immediate_descendants_only",
        FT_BOOLEAN, BASE_NONE, NULL, 0,
        "BOOLEAN", HFILL }},
    { &hf_p7_omit_descriptors,
      { "omit-descriptors", "p7.omit_descriptors",
        FT_BOOLEAN, BASE_NONE, NULL, 0,
        "BOOLEAN", HFILL }},
    { &hf_p7_algorithm_identifier,
      { "algorithm-identifier", "p7.algorithm_identifier",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_old_credentials,
      { "old-credentials", "p7.old_credentials",
        FT_UINT32, BASE_DEC, VALS(p1_Credentials_vals), 0,
        "InitiatorCredentials", HFILL }},
    { &hf_p7_password_delta,
      { "password-delta", "p7.password_delta",
        FT_BYTES, BASE_NONE, NULL, 0,
        "BIT_STRING", HFILL }},
    { &hf_p7_no_status_information,
      { "no-status-information", "p7.no_status_information_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_registered_information,
      { "registered-information", "p7.registered_information_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_registered_list_attribute_defaults,
      { "list-attribute-defaults", "p7.registered_list_attribute_defaults",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_default_registrations_OF_AttributeType", HFILL }},
    { &hf_p7_registered_list_attribute_defaults_item,
      { "AttributeType", "p7.AttributeType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_registered_fetch_attribute_defaults,
      { "fetch-attribute-defaults", "p7.registered_fetch_attribute_defaults",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_default_registrations_OF_AttributeType", HFILL }},
    { &hf_p7_registered_fetch_attribute_defaults_item,
      { "AttributeType", "p7.AttributeType",
        FT_OID, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_registered_message_group_registrations,
      { "message-group-registrations", "p7.registered_message_group_registrations",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor", HFILL }},
    { &hf_p7_registered_message_group_registrations_item,
      { "MessageGroupNameAndDescriptor", "p7.MessageGroupNameAndDescriptor_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_register_ms_result_extensions,
      { "register-ms-result-extensions", "p7.register_ms_result_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_alert_registration_identifier,
      { "alert-registration-identifier", "p7.alert_registration_identifier",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_auto_actions", HFILL }},
    { &hf_p7_new_entry,
      { "new-entry", "p7.new_entry_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "EntryInformation", HFILL }},
    { &hf_p7_entries,
      { "entries", "p7.entries",
        FT_UINT32, BASE_DEC, VALS(p7_T_entries_vals), 0,
        NULL, HFILL }},
    { &hf_p7_specific_entries,
      { "specific-entries", "p7.specific_entries",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber", HFILL }},
    { &hf_p7_specific_entries_item,
      { "SequenceNumber", "p7.SequenceNumber",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_modifications,
      { "modifications", "p7.modifications",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification", HFILL }},
    { &hf_p7_modifications_item,
      { "EntryModification", "p7.EntryModification_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_modify_extensions,
      { "modify-extensions", "p7.modify_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_strict,
      { "strict", "p7.strict",
        FT_BOOLEAN, BASE_NONE, NULL, 0,
        "BOOLEAN", HFILL }},
    { &hf_p7_modification,
      { "modification", "p7.modification",
        FT_UINT32, BASE_DEC, VALS(p7_T_modification_vals), 0,
        NULL, HFILL }},
    { &hf_p7_add_attribute,
      { "add-attribute", "p7.add_attribute_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "Attribute", HFILL }},
    { &hf_p7_remove_attribute,
      { "remove-attribute", "p7.remove_attribute",
        FT_OID, BASE_NONE, NULL, 0,
        "AttributeType", HFILL }},
    { &hf_p7_add_values,
      { "add-values", "p7.add_values_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "OrderedAttribute", HFILL }},
    { &hf_p7_remove_values,
      { "remove-values", "p7.remove_values_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "OrderedAttribute", HFILL }},
    { &hf_p7_ordered_attribute_values,
      { "attribute-values", "p7.ordered_attribute_values",
        FT_UINT32, BASE_DEC, NULL, 0,
        "OrderedAttributeValues", HFILL }},
    { &hf_p7_ordered_attribute_values_item,
      { "attribute-values item", "p7.ordered_attribute_values_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "OrderedAttributeItem", HFILL }},
    { &hf_p7_ordered_attribute_value,
      { "value", "p7.ordered_attribute_value_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "OrderedAttributeValue", HFILL }},
    { &hf_p7_ordered_position,
      { "position", "p7.ordered_position",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_attribute_values", HFILL }},
    { &hf_p7_entries_modified,
      { "entries-modified", "p7.entries_modified",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber", HFILL }},
    { &hf_p7_entries_modified_item,
      { "SequenceNumber", "p7.SequenceNumber",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_modify_result_extensions,
      { "modify-result-extensions", "p7.modify_result_extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MSExtensions", HFILL }},
    { &hf_p7_envelope,
      { "envelope", "p7.envelope_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MessageSubmissionEnvelope", HFILL }},
    { &hf_p7_content,
      { "content", "p7.content",
        FT_BYTES, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_submission_options,
      { "submission-options", "p7.submission_options_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MSSubmissionOptions", HFILL }},
    { &hf_p7_mts_result,
      { "mts-result", "p7.mts_result_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_message_submission_identifier,
      { "message-submission-identifier", "p7.message_submission_identifier_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MessageSubmissionIdentifier", HFILL }},
    { &hf_p7_message_submission_time,
      { "message-submission-time", "p7.message_submission_time",
        FT_STRING, BASE_NONE, NULL, 0,
        "MessageSubmissionTime", HFILL }},
    { &hf_p7_content_identifier,
      { "content-identifier", "p7.content_identifier",
        FT_STRING, BASE_NONE, NULL, 0,
        "ContentIdentifier", HFILL }},
    { &hf_p7_extensions,
      { "extensions", "p7.extensions",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_OF_ExtensionField", HFILL }},
    { &hf_p7_extensions_item,
      { "ExtensionField", "p7.ExtensionField_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_ms_message_result,
      { "ms-message-result", "p7.ms_message_result_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "CommonSubmissionResults", HFILL }},
    { &hf_p7_store_draft_result,
      { "store-draft-result", "p7.store_draft_result_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "CommonSubmissionResults", HFILL }},
    { &hf_p7_originator_name,
      { "originator-name", "p7.originator_name_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "OriginatorName", HFILL }},
    { &hf_p7_original_encoded_information_types,
      { "original-encoded-information-types", "p7.original_encoded_information_types_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "OriginalEncodedInformationTypes", HFILL }},
    { &hf_p7_content_type,
      { "content-type", "p7.content_type",
        FT_UINT32, BASE_DEC, VALS(p1_ContentType_vals), 0,
        "ContentType", HFILL }},
    { &hf_p7_content_length,
      { "content-length", "p7.content_length",
        FT_UINT32, BASE_DEC, NULL, 0,
        "ContentLength", HFILL }},
    { &hf_p7_per_message_indicators,
      { "per-message-indicators", "p7.per_message_indicators",
        FT_BYTES, BASE_NONE, NULL, 0,
        "PerMessageIndicators", HFILL }},
    { &hf_p7_per_recipient_fields,
      { "per-recipient-fields", "p7.per_recipient_fields",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_OF_PerRecipientProbeSubmissionFields", HFILL }},
    { &hf_p7_per_recipient_fields_item,
      { "PerRecipientProbeSubmissionFields", "p7.PerRecipientProbeSubmissionFields_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_probe_submission_identifier,
      { "probe-submission-identifier", "p7.probe_submission_identifier_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "ProbeSubmissionIdentifier", HFILL }},
    { &hf_p7_probe_submission_time,
      { "probe-submission-time", "p7.probe_submission_time",
        FT_STRING, BASE_NONE, NULL, 0,
        "ProbeSubmissionTime", HFILL }},
    { &hf_p7_ms_probe_result,
      { "ms-probe-result", "p7.ms_probe_result_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "CommonSubmissionResults", HFILL }},
    { &hf_p7_attribute_problems,
      { "problems", "p7.attribute_problems",
        FT_UINT32, BASE_DEC, NULL, 0,
        "AttributeProblems", HFILL }},
    { &hf_p7_attribute_problem_item,
      { "problems item", "p7.attribute_problems_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "AttributeProblemItem", HFILL }},
    { &hf_p7_attribute_problem,
      { "problem", "p7.attribute_problem",
        FT_UINT32, BASE_DEC, VALS(p7_AttributeProblem_vals), 0,
        "AttributeProblem", HFILL }},
    { &hf_p7_attr_value,
      { "value", "p7.attr_value_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "T_attr_value", HFILL }},
    { &hf_p7_auto_action_request_problems,
      { "problems", "p7.auto_action_request_problems",
        FT_UINT32, BASE_DEC, NULL, 0,
        "AutoActionRequestProblems", HFILL }},
    { &hf_p7_auto_action_request_problem_item,
      { "problems item", "p7.auto_action_request_problems_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "AutoActionRequestProblemItem", HFILL }},
    { &hf_p7_auto_action_request_problem,
      { "problem", "p7.auto_action_request_problem",
        FT_UINT32, BASE_DEC, VALS(p7_AutoActionRequestProblem_vals), 0,
        "AutoActionRequestProblem", HFILL }},
    { &hf_p7_delete_problems,
      { "problems", "p7.delete_problems",
        FT_UINT32, BASE_DEC, NULL, 0,
        "DeleteProblems", HFILL }},
    { &hf_p7_delete_problem_item,
      { "problems item", "p7.delete_problems_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "DeleteProblemItem", HFILL }},
    { &hf_p7_delete_problem,
      { "problem", "p7.delete_problem",
        FT_UINT32, BASE_DEC, VALS(p7_DeleteProblem_vals), 0,
        "DeleteProblem", HFILL }},
    { &hf_p7_entries_deleted,
      { "entries-deleted", "p7.entries_deleted",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SET_SIZE_1_ub_messages_OF_SequenceNumber", HFILL }},
    { &hf_p7_entries_deleted_item,
      { "SequenceNumber", "p7.SequenceNumber",
        FT_UINT32, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_fetch_restriction_problems,
      { "problems", "p7.fetch_restriction_problems",
        FT_UINT32, BASE_DEC, NULL, 0,
        "FetchRestrictionProblems", HFILL }},
    { &hf_p7_fetch_restriction_problem_item,
      { "problems item", "p7.fetch_restriction_problems_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "FetchRestrictionProblemItem", HFILL }},
    { &hf_p7_fetch_restriction_problem,
      { "problem", "p7.fetch-restriction-problem",
        FT_UINT32, BASE_DEC, VALS(p7_FetchRestrictionProblem_vals), 0,
        "FetchRestrictionProblem", HFILL }},
    { &hf_p7_restriction,
      { "restriction", "p7.restriction",
        FT_UINT32, BASE_DEC, VALS(p7_T_restriction_vals), 0,
        NULL, HFILL }},
    { &hf_p7_extended_content_type,
      { "content-type", "p7.extended-content-type",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_eit,
      { "eit", "p7.eit",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MS_EITs", HFILL }},
    { &hf_p7_attribute_length,
      { "attribute-length", "p7.attribute_length",
        FT_INT32, BASE_DEC, NULL, 0,
        "INTEGER", HFILL }},
    { &hf_p7_range_problem,
      { "problem", "p7.pAR-range-error.problem",
        FT_UINT32, BASE_DEC, VALS(p7_RangeProblem_vals), 0,
        "RangeProblem", HFILL }},
    { &hf_p7_sequence_number_problems,
      { "problems", "p7.sequence_number_problems",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumberProblems", HFILL }},
    { &hf_p7_sequence_number_problem_item,
      { "problems item", "p7.sequence_number_problems_item_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "SequenceNumberProblemItem", HFILL }},
    { &hf_p7_sequence_number_problem,
      { "problem", "p7.sequence_number_problem",
        FT_UINT32, BASE_DEC, VALS(p7_SequenceNumberProblem_vals), 0,
        "SequenceNumberProblem", HFILL }},
    { &hf_p7_service_problem,
      { "problem", "p7.serviceErrorParameter.problem",
        FT_UINT32, BASE_DEC, VALS(p7_ServiceProblem_vals), 0,
        "ServiceProblem", HFILL }},
    { &hf_p7_message_group_problem,
      { "problem", "p7.messageGroupErrorParameter.group-problem",
        FT_UINT32, BASE_DEC, VALS(p7_MessageGroupProblem_vals), 0,
        "MessageGroupProblem", HFILL }},
    { &hf_p7_name,
      { "name", "p7.name",
        FT_UINT32, BASE_DEC, NULL, 0,
        "MessageGroupName", HFILL }},
    { &hf_p7_ms_extension_problem,
      { "ms-extension-problem", "p7.ms_extension_problem_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MSExtensionItem", HFILL }},
    { &hf_p7_unknown_ms_extension,
      { "unknown-ms-extension", "p7.unknown_ms_extension",
        FT_OID, BASE_NONE, NULL, 0,
        "OBJECT_IDENTIFIER", HFILL }},
    { &hf_p7_register_ms_problem,
      { "problem", "p7.register_ms_problem",
        FT_UINT32, BASE_DEC, VALS(p7_RegistrationProblem_vals), 0,
        "RegistrationProblem", HFILL }},
    { &hf_p7_registration_type,
      { "registration-type", "p7.registration_type_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "RegistrationTypes", HFILL }},
    { &hf_p7_failing_entry,
      { "failing-entry", "p7.failing_entry",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_modification_number,
      { "modification-number", "p7.modification_number",
        FT_INT32, BASE_DEC, NULL, 0,
        "INTEGER", HFILL }},
    { &hf_p7_modify_problem,
      { "problem", "p7.modifyErrorParameter.problem",
        FT_UINT32, BASE_DEC, VALS(p7_ModifyProblem_vals), 0,
        "ModifyProblem", HFILL }},
    { &hf_p7_entry_class_problem,
      { "problem", "p7.entryClassErrorParameter.problem",
        FT_BYTES, BASE_NONE, NULL, 0,
        "T_entry_class_problem", HFILL }},
    { &hf_p7_no_correlated_reports,
      { "no-correlated-reports", "p7.no_correlated_reports_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_location,
      { "location", "p7.location",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SEQUENCE_OF_PerRecipientReport", HFILL }},
    { &hf_p7_location_item,
      { "PerRecipientReport", "p7.PerRecipientReport_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_report_entry,
      { "report-entry", "p7.report_entry",
        FT_UINT32, BASE_DEC, NULL, 0,
        "SequenceNumber", HFILL }},
    { &hf_p7_position,
      { "position", "p7.position",
        FT_UINT32, BASE_DEC, NULL, 0,
        "INTEGER_1_ub_recipients", HFILL }},
    { &hf_p7_submission_control_violated,
      { "submission-control-violated", "p7.submission_control_violated_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_originator_invalid,
      { "originator-invalid", "p7.originator_invalid_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_recipient_improperly_specified,
      { "recipient-improperly-specified", "p7.recipient_improperly_specified",
        FT_UINT32, BASE_DEC, NULL, 0,
        "ImproperlySpecifiedRecipients", HFILL }},
    { &hf_p7_element_of_service_not_subscribed,
      { "element-of-service-not-subscribed", "p7.element_of_service_not_subscribed_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_inconsistent_request,
      { "inconsistent-request", "p7.inconsistent_request_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_security_error,
      { "security-error", "p7.security_error",
        FT_UINT32, BASE_DEC, VALS(p1_SecurityProblem_vals), 0,
        "SecurityProblem", HFILL }},
    { &hf_p7_unsupported_critical_function,
      { "unsupported-critical-function", "p7.unsupported_critical_function_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_remote_bind_error,
      { "remote-bind-error", "p7.remote_bind_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_service_error,
      { "service-error", "p7.service_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "ServiceErrorParameter", HFILL }},
    { &hf_p7_message_group_error,
      { "message-group-error", "p7.message_group_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "MessageGroupErrorParameter", HFILL }},
    { &hf_p7_ms_extension_error,
      { "ms-extension-error", "p7.ms_extension_error",
        FT_UINT32, BASE_DEC, VALS(p7_MSExtensionErrorParameter_vals), 0,
        "MSExtensionErrorParameter", HFILL }},
    { &hf_p7_entry_class_error,
      { "entry-class-error", "p7.entry_class_error_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "EntryClassErrorParameter", HFILL }},
    { &hf_p7_content_integrity_check,
      { "content-integrity-check", "p7.content_integrity_check",
        FT_INT32, BASE_DEC, VALS(p7_SignatureStatus_vals), 0,
        "SignatureStatus", HFILL }},
    { &hf_p7_message_origin_authentication_check,
      { "message-origin-authentication-check", "p7.message_origin_authentication_check",
        FT_INT32, BASE_DEC, VALS(p7_SignatureStatus_vals), 0,
        "SignatureStatus", HFILL }},
    { &hf_p7_message_token,
      { "message-token", "p7.message_token",
        FT_INT32, BASE_DEC, VALS(p7_SignatureStatus_vals), 0,
        "SignatureStatus", HFILL }},
    { &hf_p7_report_origin_authentication_check,
      { "report-origin-authentication-check", "p7.report_origin_authentication_check",
        FT_INT32, BASE_DEC, VALS(p7_SignatureStatus_vals), 0,
        "SignatureStatus", HFILL }},
    { &hf_p7_proof_of_delivery,
      { "proof-of-delivery", "p7.proof_of_delivery",
        FT_INT32, BASE_DEC, VALS(p7_SignatureStatus_vals), 0,
        "SignatureStatus", HFILL }},
    { &hf_p7_proof_of_submission,
      { "proof-of-submission", "p7.proof_of_submission",
        FT_INT32, BASE_DEC, VALS(p7_SignatureStatus_vals), 0,
        "SignatureStatus", HFILL }},
    { &hf_p7_rtorq_apdu,
      { "rtorq-apdu", "p7.rtorq_apdu_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "RTORQapdu", HFILL }},
    { &hf_p7_rtoac_apdu,
      { "rtoac-apdu", "p7.rtoac_apdu_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "RTOACapdu", HFILL }},
    { &hf_p7_rtorj_apdu,
      { "rtorj-apdu", "p7.rtorj_apdu_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "RTORJapdu", HFILL }},
    { &hf_p7_rttp_apdu,
      { "rttp-apdu", "p7.rttp_apdu",
        FT_INT32, BASE_DEC, NULL, 0,
        "RTTPapdu", HFILL }},
    { &hf_p7_rttr_apdu,
      { "rttr-apdu", "p7.rttr_apdu",
        FT_BYTES, BASE_NONE, NULL, 0,
        "RTTRapdu", HFILL }},
    { &hf_p7_rtab_apdu,
      { "rtab-apdu", "p7.rtab_apdu_element",
        FT_NONE, BASE_NONE, NULL, 0,
        "RTABapdu", HFILL }},
    { &hf_p7_abortReason,
      { "abortReason", "p7.abortReason",
        FT_INT32, BASE_DEC, VALS(p7_AbortReason_vals), 0,
        NULL, HFILL }},
    { &hf_p7_reflectedParameter,
      { "reflectedParameter", "p7.reflectedParameter",
        FT_BYTES, BASE_NONE, NULL, 0,
        "BIT_STRING", HFILL }},
    { &hf_p7_userdataAB,
      { "userdataAB", "p7.userdataAB_element",
        FT_NONE, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_p7_OverrideRestrictions_override_content_types_restriction,
      { "override-content-types-restriction", "p7.OverrideRestrictions.override.content.types.restriction",
        FT_BOOLEAN, 8, NULL, 0x80,
        NULL, HFILL }},
    { &hf_p7_OverrideRestrictions_override_EITs_restriction,
      { "override-EITs-restriction", "p7.OverrideRestrictions.override.EITs.restriction",
        FT_BOOLEAN, 8, NULL, 0x40,
        NULL, HFILL }},
    { &hf_p7_OverrideRestrictions_override_attribute_length_restriction,
      { "override-attribute-length-restriction", "p7.OverrideRestrictions.override.attribute.length.restriction",
        FT_BOOLEAN, 8, NULL, 0x20,
        NULL, HFILL }},
    { &hf_p7_T_registrations_auto_action_registrations,
      { "auto-action-registrations", "p7.T.registrations.auto.action.registrations",
        FT_BOOLEAN, 8, NULL, 0x80,
        NULL, HFILL }},
    { &hf_p7_T_registrations_list_attribute_defaults,
      { "list-attribute-defaults", "p7.T.registrations.list.attribute.defaults",
        FT_BOOLEAN, 8, NULL, 0x40,
        NULL, HFILL }},
    { &hf_p7_T_registrations_fetch_attribute_defaults,
      { "fetch-attribute-defaults", "p7.T.registrations.fetch.attribute.defaults",
        FT_BOOLEAN, 8, NULL, 0x20,
        NULL, HFILL }},
    { &hf_p7_T_registrations_ua_registrations,
      { "ua-registrations", "p7.T.registrations.ua.registrations",
        FT_BOOLEAN, 8, NULL, 0x10,
        NULL, HFILL }},
    { &hf_p7_T_registrations_submission_defaults,
      { "submission-defaults", "p7.T.registrations.submission.defaults",
        FT_BOOLEAN, 8, NULL, 0x08,
        NULL, HFILL }},
    { &hf_p7_T_registrations_message_group_registrations,
      { "message-group-registrations", "p7.T.registrations.message.group.registrations",
        FT_BOOLEAN, 8, NULL, 0x04,
        NULL, HFILL }},
    { &hf_p7_T_entry_class_problem_unsupported_entry_class,
      { "unsupported-entry-class", "p7.T.entry.class.problem.unsupported.entry.class",
        FT_BOOLEAN, 8, NULL, 0x80,
        NULL, HFILL }},
    { &hf_p7_T_entry_class_problem_entry_class_not_subscribed,
      { "entry-class-not-subscribed", "p7.T.entry.class.problem.entry.class.not.subscribed",
        FT_BOOLEAN, 8, NULL, 0x40,
        NULL, HFILL }},
    { &hf_p7_T_entry_class_problem_inappropriate_entry_class,
      { "inappropriate-entry-class", "p7.T.entry.class.problem.inappropriate.entry.class",
        FT_BOOLEAN, 8, NULL, 0x20,
        NULL, HFILL }},
  };

  /* List of subtrees */
  static int *ett[] = {
    &ett_p7,
    &ett_p7_Attribute,
    &ett_p7_AttributeValues,
    &ett_p7_AutoActionRegistration,
    &ett_p7_AutoActionError,
    &ett_p7_MSExtensions,
    &ett_p7_MessageGroupName,
    &ett_p7_MSBindArgument,
    &ett_p7_Restrictions,
    &ett_p7_T_allowed_content_types,
    &ett_p7_MS_EITs,
    &ett_p7_MSBindResult,
    &ett_p7_SET_SIZE_1_ub_auto_actions_OF_AutoActionType,
    &ett_p7_SET_SIZE_1_ub_attributes_supported_OF_AttributeType,
    &ett_p7_T_content_types_supported,
    &ett_p7_SET_SIZE_1_ub_entry_classes_OF_EntryClass,
    &ett_p7_T_matching_rules_supported,
    &ett_p7_T_unsupported_extensions,
    &ett_p7_ChangeCredentialsAlgorithms,
    &ett_p7_AutoActionErrorIndication,
    &ett_p7_PAR_ms_bind_error,
    &ett_p7_T_qualified_error,
    &ett_p7_T_bind_extension_errors,
    &ett_p7_Range,
    &ett_p7_NumberRange,
    &ett_p7_TimeRange,
    &ett_p7_Filter,
    &ett_p7_SET_OF_Filter,
    &ett_p7_FilterItem,
    &ett_p7_T_substrings,
    &ett_p7_T_strings,
    &ett_p7_T_strings_item,
    &ett_p7_MatchingRuleAssertion,
    &ett_p7_AttributeValueAssertion,
    &ett_p7_Selector,
    &ett_p7_OverrideRestrictions,
    &ett_p7_EntryInformationSelection,
    &ett_p7_AttributeSelection,
    &ett_p7_EntryInformation,
    &ett_p7_SET_SIZE_1_ub_per_entry_OF_Attribute,
    &ett_p7_SET_SIZE_1_ub_per_entry_OF_AttributeValueCount,
    &ett_p7_AttributeValueCount,
    &ett_p7_MSSubmissionOptions,
    &ett_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupName,
    &ett_p7_CommonSubmissionResults,
    &ett_p7_SummarizeArgument,
    &ett_p7_SEQUENCE_SIZE_1_ub_summaries_OF_AttributeType,
    &ett_p7_SummarizeResult,
    &ett_p7_SEQUENCE_SIZE_1_ub_summaries_OF_Summary,
    &ett_p7_Span,
    &ett_p7_Summary,
    &ett_p7_T_summary_present,
    &ett_p7_T_summary_present_item,
    &ett_p7_ListArgument,
    &ett_p7_ListResult,
    &ett_p7_SEQUENCE_SIZE_1_ub_messages_OF_EntryInformation,
    &ett_p7_FetchArgument,
    &ett_p7_T_item,
    &ett_p7_FetchResult,
    &ett_p7_SEQUENCE_SIZE_1_ub_messages_OF_SequenceNumber,
    &ett_p7_DeleteArgument,
    &ett_p7_T_items,
    &ett_p7_SET_SIZE_1_ub_messages_OF_SequenceNumber,
    &ett_p7_DeleteResult,
    &ett_p7_T_delete_result_94,
    &ett_p7_Register_MSArgument,
    &ett_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionRegistration,
    &ett_p7_SET_SIZE_1_ub_auto_registrations_OF_AutoActionDeregistration,
    &ett_p7_SET_SIZE_0_ub_default_registrations_OF_AttributeType,
    &ett_p7_T_change_credentials,
    &ett_p7_SET_SIZE_1_ub_labels_and_redirections_OF_SecurityLabel,
    &ett_p7_SET_SIZE_1_ub_ua_registrations_OF_UARegistration,
    &ett_p7_AutoActionDeregistration,
    &ett_p7_UARegistration,
    &ett_p7_MessageGroupRegistrations,
    &ett_p7_MessageGroupRegistrations_item,
    &ett_p7_MessageGroupNameAndDescriptor,
    &ett_p7_RegistrationTypes,
    &ett_p7_T_registrations,
    &ett_p7_T_extended_registrations,
    &ett_p7_MessageGroupsRestriction,
    &ett_p7_ProtectedChangeCredentials,
    &ett_p7_Register_MSResult,
    &ett_p7_T_registered_information,
    &ett_p7_SET_SIZE_1_ub_default_registrations_OF_AttributeType,
    &ett_p7_SET_SIZE_1_ub_message_groups_OF_MessageGroupNameAndDescriptor,
    &ett_p7_AlertArgument,
    &ett_p7_ModifyArgument,
    &ett_p7_T_entries,
    &ett_p7_SEQUENCE_SIZE_1_ub_modifications_OF_EntryModification,
    &ett_p7_EntryModification,
    &ett_p7_T_modification,
    &ett_p7_OrderedAttribute,
    &ett_p7_OrderedAttributeValues,
    &ett_p7_OrderedAttributeItem,
    &ett_p7_ModifyResult,
    &ett_p7_MSMessageSubmissionArgument,
    &ett_p7_MSMessageSubmissionResult,
    &ett_p7_T_mts_result,
    &ett_p7_SET_OF_ExtensionField,
    &ett_p7_MSProbeSubmissionArgument,
    &ett_p7_SEQUENCE_OF_PerRecipientProbeSubmissionFields,
    &ett_p7_MSProbeSubmissionResult,
    &ett_p7_PAR_attribute_error,
    &ett_p7_AttributeProblems,
    &ett_p7_AttributeProblemItem,
    &ett_p7_PAR_auto_action_request_error,
    &ett_p7_AutoActionRequestProblems,
    &ett_p7_AutoActionRequestProblemItem,
    &ett_p7_PAR_delete_error,
    &ett_p7_DeleteProblems,
    &ett_p7_DeleteProblemItem,
    &ett_p7_PAR_fetch_restriction_error,
    &ett_p7_FetchRestrictionProblems,
    &ett_p7_FetchRestrictionProblemItem,
    &ett_p7_T_restriction,
    &ett_p7_PAR_range_error,
    &ett_p7_PAR_sequence_number_error,
    &ett_p7_SequenceNumberProblems,
    &ett_p7_SequenceNumberProblemItem,
    &ett_p7_ServiceErrorParameter,
    &ett_p7_MessageGroupErrorParameter,
    &ett_p7_MSExtensionErrorParameter,
    &ett_p7_PAR_register_ms_error,
    &ett_p7_ModifyErrorParameter,
    &ett_p7_EntryClassErrorParameter,
    &ett_p7_T_entry_class_problem,
    &ett_p7_ReportLocation,
    &ett_p7_SEQUENCE_OF_PerRecipientReport,
    &ett_p7_PerRecipientReport,
    &ett_p7_SubmissionError,
    &ett_p7_SignatureVerificationStatus,
    &ett_p7_RTSE_apdus,
    &ett_p7_RTABapdu,
  };
  module_t *p7_module;

  /* Register protocol */
  proto_p7 = proto_register_protocol(PNAME, PSNAME, PFNAME);

  /* Register fields and subtrees */
  proto_register_field_array(proto_p7, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));

  /* Register our configuration options for P7, particularly our port */

  p7_module = prefs_register_protocol_subtree("OSI/X.400", proto_p7, NULL);

  prefs_register_obsolete_preference(p7_module, "tcp.port");

  prefs_register_static_text_preference(p7_module, "tcp_port_info",
            "The TCP ports used by the P7 protocol should be added to the TPKT preference \"TPKT TCP ports\", or by selecting \"TPKT\" as the \"Transport\" protocol in the \"Decode As\" dialog.",
            "P7 TCP Port preference moved information");
}


/*--- proto_reg_handoff_p7 --- */
void proto_reg_handoff_p7(void) {

  register_ber_oid_dissector("2.6.4.3.42", dissect_ReportLocation_PDU, proto_p7, "id-att-ac-correlated-report-list");
  register_ber_oid_dissector("2.6.4.3.76", dissect_SequenceNumber_PDU, proto_p7, "id-att-ac-report-subject-entry");
  register_ber_oid_dissector("2.6.4.3.43", dissect_ReportSummary_PDU, proto_p7, "id-att-ac-report-summary");
  register_ber_oid_dissector("2.6.4.3.44", dissect_PerRecipientReport_PDU, proto_p7, "id-att-ac-uncorrelated-report-list");
  register_ber_oid_dissector("2.6.4.3.46", dissect_AutoActionError_PDU, proto_p7, "id-att-auto-action-error");
  register_ber_oid_dissector("2.6.4.3.48", dissect_SequenceNumber_PDU, proto_p7, "id-att-auto-action-subject-entry");
  register_ber_oid_dissector("2.6.4.3.49", dissect_AutoActionType_PDU, proto_p7, "id-att-auto-action-type");
  register_ber_oid_dissector("2.6.4.3.0", dissect_SequenceNumber_PDU, proto_p7, "id-att-child-sequence-numbers");
  register_ber_oid_dissector("2.6.4.3.10", dissect_MS_EIT_PDU, proto_p7, "id-att-converted-EITs");
  register_ber_oid_dissector("2.6.4.3.11", dissect_CreationTime_PDU, proto_p7, "id-att-creation-time");
  register_ber_oid_dissector("2.6.4.3.50", dissect_DeferredDeliveryCancellationTime_PDU, proto_p7, "id-att-deferred-delivery-cancellation-time");
  register_ber_oid_dissector("2.6.4.3.52", dissect_DeletionTime_PDU, proto_p7, "id-att-deletion-time");
  register_ber_oid_dissector("2.6.4.3.12", dissect_MS_EIT_PDU, proto_p7, "id-att-delivered-EITs");
  register_ber_oid_dissector("2.6.4.3.16", dissect_EntryType_PDU, proto_p7, "id-att-entry-type");
  register_ber_oid_dissector("2.6.4.3.57", dissect_MessageGroupName_PDU, proto_p7, "id-att-message-group-name");
  register_ber_oid_dissector("2.6.4.3.61", dissect_SubmissionError_PDU, proto_p7, "id-att-ms-submission-error");
  register_ber_oid_dissector("2.6.4.3.25", dissect_MS_EIT_PDU, proto_p7, "id-att-original-EITs");
  register_ber_oid_dissector("2.6.4.3.29", dissect_SequenceNumber_PDU, proto_p7, "id-att-parent-sequence-number");
  register_ber_oid_dissector("2.6.4.3.15", dissect_RetrievalStatus_PDU, proto_p7, "id-att-retrieval-status");
  register_ber_oid_dissector("2.6.4.3.39", dissect_SequenceNumber_PDU, proto_p7, "id-att-sequence-number");
  register_ber_oid_dissector("2.6.4.3.79", dissect_SignatureVerificationStatus_PDU, proto_p7, "id-att-signature-verification-status");
  register_ber_oid_dissector("2.6.4.3.73", dissect_StoragePeriod_PDU, proto_p7, "id-att-storage-period");
  register_ber_oid_dissector("2.6.4.3.74", dissect_StorageTime_PDU, proto_p7, "id-att-storage-time");
  register_ber_oid_dissector("2.6.4.9.5", dissect_ChangeCredentialsAlgorithms_PDU, proto_p7, "id-ext-protected-change-credentials-capability");
  register_ber_oid_dissector("2.6.4.9.3", dissect_OriginatorToken_PDU, proto_p7, "id-ext-originator-token");
  register_ber_oid_dissector("2.6.4.9.4", dissect_ProtectedChangeCredentials_PDU, proto_p7, "id-ext-protected-change-credentials");
  register_ber_oid_dissector("2.6.0.2.10", dissect_RTSE_apdus_PDU, proto_p7, "id-as-ms-rtse");


  /* APPLICATION CONTEXT */

  oid_add_from_string("id-ac-ms-access","2.6.0.1.11");
  oid_add_from_string("id-ac-ms-reliable-access","2.6.0.1.12");

  /* ABSTRACT SYNTAXES */

  /* Register P7 with ROS (with no use of RTSE) */
  register_ros_protocol_info("2.6.0.2.9", &p7_ros_info, 0, "id-as-ms", false);
  register_ros_protocol_info("2.6.0.2.5", &p7_ros_info, 0, "id-as-mrse", false);
  register_ros_protocol_info("2.6.0.2.1", &p7_ros_info, 0, "id-as-msse", false);
}
