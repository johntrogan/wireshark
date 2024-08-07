/** @file
 * Declaration of CRC-16 tvbuff routines
 *
 * 2004 Richard van der Hoff <richardv@mxtelecom.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __CRC16_TVB_H__
#define __CRC16_TVB_H__

#include "ws_symbol_export.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Compute CRC16 CCITT checksum of a tv buffer.
 @param tvb The tv buffer containing the data.
 @param len The number of bytes to include in the computation.
 @return The CRC16 CCITT checksum. */
WS_DLL_PUBLIC uint16_t crc16_ccitt_tvb(tvbuff_t *tvb, unsigned len);

/** Compute CRC16 X.25 CCITT checksum of a tv buffer.
 @param tvb The tv buffer containing the data.
 @param len The number of bytes to include in the computation.
 @return The CRC16 X.25 CCITT checksum. */
WS_DLL_PUBLIC uint16_t crc16_x25_ccitt_tvb(tvbuff_t *tvb, unsigned len);

/** Compute CRC16 ASSA R3 CCITT checksum of a tv buffer.
 @param tvb The tv buffer containing the data.
 @param offset The offset into tv buffer containing the data.
 @param len The number of bytes to include in the computation.
 @return The CRC16 X.25 CCITT checksum. */
WS_DLL_PUBLIC uint16_t crc16_r3_ccitt_tvb(tvbuff_t *tvb, int offset, unsigned len);

/** Compute CRC16 CCITT checksum of a tv buffer.
 @param tvb The tv buffer containing the data.
 @param offset The offset into the tv buffer.
 @param len The number of bytes to include in the computation.
 @return The CRC16 CCITT checksum. */
WS_DLL_PUBLIC uint16_t crc16_ccitt_tvb_offset(tvbuff_t *tvb, unsigned offset, unsigned len);

/** Compute CRC16 CCITT checksum of a tv buffer.  If computing the
 *  checksum over multiple tv buffers and you want to feed the partial CRC16
 *  back in, remember to take the 1's complement of the partial CRC16 first.
 @param tvb The tv buffer containing the data.
 @param len The number of bytes to include in the computation.
 @param seed The seed to use.
 @return The CRC16 CCITT checksum (using the given seed). */
WS_DLL_PUBLIC uint16_t crc16_ccitt_tvb_seed(tvbuff_t *tvb, unsigned len, uint16_t seed);

/** Compute CRC16 CCITT checksum of a tv buffer.  If computing the
 *  checksum over multiple tv buffers and you want to feed the partial CRC16
 *  back in, remember to take the 1's complement of the partial CRC16 first.
 @param tvb The tv buffer containing the data.
 @param offset The offset into the tv buffer.
 @param len The number of bytes to include in the computation.
 @param seed The seed to use.
 @return The CRC16 CCITT checksum (using the given seed). */
WS_DLL_PUBLIC uint16_t crc16_ccitt_tvb_offset_seed(tvbuff_t *tvb, unsigned offset,
                                           unsigned len, uint16_t seed);

/** Compute the 16bit CRC_A value of a tv buffer as defined in ISO14443-3.
 @param tvb The tv buffer containing the data.
 @param offset The offset into the tv buffer.
 @param len The number of bytes to include in the computation.
 @return The calculated CRC_A. */
WS_DLL_PUBLIC uint16_t crc16_iso14443a_tvb_offset(tvbuff_t *tvb, unsigned offset, unsigned len);

/** Compute the 16bit CRC value of a tv buffer as defined in USB Standard.
 @param tvb The tv buffer containing the data.
 @param offset The offset into the tv buffer.
 @param len The number of bytes to include in the computation.
 @return The calculated CRC. */
WS_DLL_PUBLIC uint16_t crc16_usb_tvb_offset(tvbuff_t *tvb, unsigned offset, unsigned len);

/** Compute the "plain" CRC16 checksum of a tv buffer using the following
 * parameters:
 *    Width        = 16
 *    Poly         = 0x8005
 *    XorIn        = 0x0000
 *    ReflectIn    = True
 *    XorOut       = 0x0000
 *    ReflectOut   = True
 *    Algorithm    = table-driven
 *    Direct       = True
 @param tvb The tv buffer containing the data.
 @param offset The offset into the tv buffer.
 @param len The number of bytes to include in the computation.
 @return The CRC16 checksum. */
WS_DLL_PUBLIC uint16_t crc16_plain_tvb_offset(tvbuff_t *tvb, unsigned offset, unsigned len);

/** Compute the "plain" CRC16 checksum of a tv buffer using the following
 * parameters:
 *    Width        = 16
 *    Poly         = 0x8005
 *    XorIn        = 0x0000
 *    ReflectIn    = True
 *    XorOut       = 0x0000
 *    ReflectOut   = True
 *    Algorithm    = table-driven
 *    Direct       = True
 @param tvb The tv buffer containing the data.
 @param offset The offset into the tv buffer.
 @param len The number of bytes to include in the computation.
 @param crc Starting CRC value
 @return The CRC16 checksum. */
WS_DLL_PUBLIC uint16_t crc16_plain_tvb_offset_seed(tvbuff_t *tvb, unsigned offset, unsigned len, uint16_t crc);

/** Compute CRC16 checksum of a tv buffer using the parameters
 *    Width        = 16 bits
 *    Poly         = 0x9949
 *    Reflection   = true
 *    Algorithm    = table-driven
 @param tvb The tv buffer containing the data.
 @param offset The offset into the tv buffer.
 @param len The number of bytes to include in the computation.
 @param seed The seed to use.
 @return The CRC16 checksum. */
WS_DLL_PUBLIC uint16_t crc16_0x9949_tvb_offset_seed(tvbuff_t *tvb, unsigned offset, unsigned len, uint16_t seed);

/** Compute CRC16 checksum of a tv buffer using the parameters
 *    Width        = 16 bits
 *    Poly         = 0x3D65
 *    Reflection   = true
 *    Algorithm    = table-driven
 @param tvb The tv buffer containing the data.
 @param offset The offset into the tv buffer.
 @param len The number of bytes to include in the computation.
 @param seed The seed to use.
 @return The CRC16 checksum. */
WS_DLL_PUBLIC uint16_t crc16_0x3D65_tvb_offset_seed(tvbuff_t *tvb, unsigned offset, unsigned len, uint16_t seed);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* crc16-tvb.h */
