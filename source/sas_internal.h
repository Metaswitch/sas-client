/**
 * @file sas_internal.h Internal contstants and definitionse.
 *
 * Service Assurance Server client library
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#ifndef SAS_INTERNAL__
#define SAS_INTERNAL__

#define SAS_LOG_ERROR(...) SAS_LOG(SAS::LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define SAS_LOG_WARNING(...) SAS_LOG(SAS::LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define SAS_LOG_STATUS(...) SAS_LOG(SAS::LOG_LEVEL_STATUS, __FILE__, __LINE__, __VA_ARGS__)
#define SAS_LOG_INFO(...) SAS_LOG(SAS::LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define SAS_LOG_VERBOSE(...) SAS_LOG(SAS::LOG_LEVEL_VERBOSE, __FILE__, __LINE__, __VA_ARGS__)
#define SAS_LOG_DEBUG(...) SAS_LOG(SAS::LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#define SAS_LOG(...) SAS::_log_callback(__VA_ARGS__)


// SAS message types.
const int SAS_MSG_INIT   = 1;
const int SAS_MSG_TRAIL_ASSOC   = 2;
const int SAS_MSG_EVENT  = 3;
const int SAS_MSG_MARKER = 4;
const int SAS_MSG_ANALYTICS = 7;

// SAS message header sizes

// SAS message header consists of 12 bytes in total:
// - [ 2 bytes ] Message length.
// - [ 1 bytes ] Interface version.
// - [ 1 bytes ] Message type.
// - [ 8 bytes ] Timestamp.
const int COMMON_HDR_SIZE = sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint64_t);

// Init headers are just the base header.
const int INIT_HDR_SIZE   = COMMON_HDR_SIZE;

// Event headers consist of the standard SAS header, plus 16 bytes:
// - [ 8 bytes ] Trail ID.
// - [ 4 bytes ] Event ID.
// - [ 4 bytes ] Instance ID.
const int EVENT_HDR_SIZE  = COMMON_HDR_SIZE + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t);

// Marker headers consist of the standard SAS header, plus 16 bytes:
// - [ 8 bytes ] Trail ID.
// - [ 4 bytes ] Marker ID.
// - [ 4 bytes ] Instance ID.
// - [ 1 byte  ] Is correlating?
// - [ 1 bytes ] Correlation scope.
const int MARKER_HDR_SIZE = COMMON_HDR_SIZE + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t);

// Analytics headers consist of the same fields as an Event plus several
// additional fields.
// This contant only defines the size of the static length header fields, the
// length of the variable length source_type and friendly_id fields can be
// calculated by calling Analytics::variable_header_buf_len().
//
// - [ 8 bytes ] Trail ID.
// - [ 4 bytes ] Event ID.
// - [ 4 bytes ] Instance ID.
// - [ 1 byte  ] Format type
// - [ 1 byte  ] Store event in SAS?
const int ANALYTICS_STATIC_HDR_SIZE = COMMON_HDR_SIZE + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t);

#endif
