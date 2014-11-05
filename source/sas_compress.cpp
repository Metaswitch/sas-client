/**
 * @file sas.cpp Implementation of SAS class used for reporting events
 * and markers to Service Assurance Server.
 *
 * Service Assurance Server client library
 * Copyright (C) 2014  Metaswitch Networks Ltd
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

#if HAVE_ZLIB_H
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/socket.h>

#include "sas.h"
#include "sas_compress.h"

// Compression-related function is only available for zlib is
pthread_once_t SAS::Compressor::_once = PTHREAD_ONCE_INIT;
pthread_key_t SAS::Compressor::_key = {0};

void SAS::Compressor::init()
{
  int rc = pthread_key_create(&_key, destroy);
  if (rc != 0)
  {
// TODO!
    SAS_LOG_WARNING("Failed to create Compressor key");
  }
}

SAS::Compressor* SAS::Compressor::get()
{
  (void)pthread_once(&_once, init);
  Compressor* compressor = (Compressor*)pthread_getspecific(_key);
  if (compressor == NULL)
  {
    compressor = new Compressor();
    pthread_setspecific(_key, compressor);
  }
  return compressor;
}

void SAS::Compressor::destroy(void* compressor_ptr)
{
  Compressor* compressor = (Compressor*)compressor_ptr;
  delete compressor;
}

SAS::Compressor::Compressor()
{
  int rc = deflateInit2(&_stream,
                        Z_DEFAULT_COMPRESSION,
                        Z_DEFLATED,
                        WINDOW_BITS,
                        MEM_LEVEL,
                        Z_DEFAULT_STRATEGY);
  if (rc != Z_OK)
  {
// TODO!
  }
}

SAS::Compressor::~Compressor()
{
  deflateEnd(&_stream);
}

std::string SAS::Compressor::compress(const std::string& s, const Profile* profile)
{
  if (profile != NULL)
  {
    std::string dictionary = profile->get_dictionary();
    deflateSetDictionary(&_stream, (const unsigned char*)dictionary.c_str(), dictionary.length());
  }
  _stream.next_in = (unsigned char*)s.c_str();
  _stream.avail_in = s.length();
  std::string compressed;
  int rc = Z_OK;
  do
  {
    _stream.next_out = (unsigned char*)_buffer;
    _stream.avail_out = sizeof(_buffer);
    deflate(&_stream, Z_FINISH);
    compressed += std::string(_buffer, sizeof(_buffer) - _stream.avail_out);
  }
  while (rc == Z_OK);
  if (rc != Z_STREAM_END)
  {
// TODO!
  }
  deflateReset(&_stream);
  return compressed;
}
#endif