# CMakeLists.txt
#
# Wireshark - Network traffic analyzer
# By Gerald Combs <gerald@wireshark.org>
# Copyright 1998 Gerald Combs
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

set( PROTOCOL_NAME cosem )

set( PROTO_OPT )

set( EXPORT_FILES
    ${PROTOCOL_NAME}-exp.cnf
)

set( EXT_ASN_FILE_LIST
)

set( ASN_FILE_LIST
    cosem.asn
)

set( EXTRA_DIST
    ${ASN_FILE_LIST}
    packet-${PROTOCOL_NAME}-template.c
    ${PROTOCOL_NAME}.cnf
)

set( EXTRA_CNF
    "${CMAKE_CURRENT_BINARY_DIR}/../x509if/x509if-exp.cnf"
)

set( SRC_FILES
    ${EXTRA_DIST}
    ${EXT_ASN_FILE_LIST}
)

set( A2W_FLAGS -b -C )

ASN2WRS()
