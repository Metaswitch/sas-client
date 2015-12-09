/**
 * @file sas_compress.cpp Implementation of SAS parameter compression.
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

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/socket.h>

#include "sas.h"
#include "sas_internal.h"

pthread_once_t SAS::ZlibCompressor::_once = PTHREAD_ONCE_INIT;
pthread_key_t SAS::ZlibCompressor::_key = {0};

/// Statically initialize the Compressor class by creating the thread-local key.
void SAS::ZlibCompressor::init()
{
  int rc = pthread_key_create(&_key, destroy);
  if (rc != 0)
  {
    SAS_LOG_WARNING("Failed to create key for SAS parameter compressor");
  }
}

/// Get a thread-scope Compressor, or create one if it doesn't exist already.
SAS::Compressor* SAS::ZlibCompressor::get()
{
  (void)pthread_once(&_once, init);
  Compressor* compressor = (Compressor*)pthread_getspecific(_key);
  if (compressor == NULL)
  {
    compressor = new ZlibCompressor();
    pthread_setspecific(_key, compressor);
  }
  return compressor;
}

/// Destroy a Compressor.  (Called by pthread when a thread terminates.)
void SAS::ZlibCompressor::destroy(void* compressor_ptr)
{
  ZlibCompressor* compressor = (ZlibCompressor*)compressor_ptr;
  delete compressor;
}

/// Compressor constructor.  Initializes the zlib compressor.
SAS::ZlibCompressor::ZlibCompressor()
{
  _stream.next_in = Z_NULL;
  _stream.avail_in = 0;
  _stream.zalloc = Z_NULL;
  _stream.zfree = Z_NULL;
  _stream.opaque = Z_NULL;
  int rc = deflateInit2(&_stream,
                        Z_DEFAULT_COMPRESSION,
                        Z_DEFLATED,
                        WINDOW_BITS,
                        MEM_LEVEL,
                        Z_DEFAULT_STRATEGY);
  if (rc != Z_OK)
  {
    SAS_LOG_WARNING("Failed to initialize SAS parameter compressor (rc=%d)", rc);
  }
}

/// Compressor destructor.  Terminates the zlib compressor.
SAS::ZlibCompressor::~ZlibCompressor()
{
  deflateEnd(&_stream);
}

/// Compresses the specified string using the optional profile.
std::string SAS::ZlibCompressor::compress(const std::string& s, std::string dictionary)
{
  if (!dictionary.empty())
  {
    deflateSetDictionary(&_stream, (const unsigned char*)dictionary.c_str(), dictionary.length());
  }

  // Initialize the zlib compressor with the input.
  _stream.next_in = (unsigned char*)s.c_str();
  _stream.avail_in = s.length();

  // Spin round, compressing up to a buffer's worth of input and appending it to the string.  Z_OK
  // indicates that we compressed data but still have work to do.  Z_STREAM_END means we've
  // finished.
  std::string compressed;
  int rc = Z_OK;
  do
  {
    _stream.next_out = (unsigned char*)_buffer;
    _stream.avail_out = sizeof(_buffer);
    rc = deflate(&_stream, Z_FINISH);
    compressed += std::string(_buffer, sizeof(_buffer) - _stream.avail_out);
  }
  while (rc == Z_OK);

  // Check we succeeded.
  if (rc != Z_STREAM_END)
  {
    SAS_LOG_WARNING("Failed to compress SAS parameter (rc=%d)", rc);
  }

  // Reset the compressor before we return.
  deflateReset(&_stream);

  return compressed;
}
