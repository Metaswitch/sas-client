/**
 * @file sas_compress_lz4.cpp Implementation of LZ4 SAS parameter compression.
 *
 * Service Assurance Server client library
 * Copyright (C) 2015 Metaswitch Networks Ltd
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

pthread_once_t SAS::LZ4Compressor::_once = PTHREAD_ONCE_INIT;
pthread_key_t SAS::LZ4Compressor::_key = {0};

/// Statically initialize the Compressor class by creating the thread-local key.
void SAS::LZ4Compressor::init()
{
  int rc = pthread_key_create(&_key, destroy);
  if (rc != 0)
  {
    SAS_LOG_WARNING("Failed to create key for SAS parameter compressor");
  }
}

/// Get a thread-scope Compressor, or create one if it doesn't exist already.
SAS::Compressor* SAS::LZ4Compressor::get()
{
  (void)pthread_once(&_once, init);
  Compressor* compressor = (Compressor*)pthread_getspecific(_key);
  if (compressor == NULL)
  {
    compressor = new LZ4Compressor();
    pthread_setspecific(_key, compressor);
  }
  return compressor;
}

/// Destroy a Compressor.  (Called by pthread when a thread terminates.)
void SAS::LZ4Compressor::destroy(void* compressor_ptr)
{
  LZ4Compressor* compressor = (LZ4Compressor*)compressor_ptr;
  delete compressor;
}

/// Compressor constructor.  Initializes the LZ4 compressor.
SAS::LZ4Compressor::LZ4Compressor()
{
  _stream = LZ4_createStream();
  if (_stream == NULL)
  {
    SAS_LOG_WARNING("Failed to initialize SAS parameter compressor");
  }
}

/// Compressor destructor.  Terminates the LZ4 compressor.
SAS::LZ4Compressor::~LZ4Compressor()
{
  // Free the stream.  Ignore the return code - the interface doesn't define
  // its meaning, and it's currently hard-coded to 0.
  (void)LZ4_freeStream(_stream); _stream = NULL;
}

/// Compresses the specified string using the optional profile.
std::string SAS::LZ4Compressor::compress(const std::string& s, std::string dictionary)
{
  if (!dictionary.empty())
  {
    LZ4_loadDict(_stream, dictionary.c_str(), dictionary.length());
  }

  // Spin round, compressing up to a buffer's worth of input and appending it to the string.
  std::string compressed;
  for (unsigned int s_pos = 0; s_pos < s.length(); s_pos += MAX_INPUT_SIZE)
  {
    // Because we've allocated a big enough buffer, it's not possible for this call to fail.
    int buffer_len = LZ4_compress_fast_continue(_stream,
                                                &s.c_str()[s_pos],
                                                _buffer,
                                                (s.length() - s_pos > MAX_INPUT_SIZE) ? MAX_INPUT_SIZE : s.length() - s_pos,
                                                sizeof(_buffer),
                                                ACCELERATION);
    compressed += std::string(_buffer, buffer_len);
  }

  // Reset the compressor before we return.
  LZ4_resetStream(_stream);

  return compressed;
}
