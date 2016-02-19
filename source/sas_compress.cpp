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
#include <unordered_map>

#include <lz4.h>
#include <zlib.h>

#include "sas.h"
#include "sas_internal.h"

class ZlibCompressor : public SAS::Compressor
{
public:
  ZlibCompressor();
  ~ZlibCompressor();
  std::string compress(const std::string& s, const SAS::Profile* profile);

  static SAS::Compressor* get();

private:
  static const int WINDOW_BITS = 15;
  static const int MEM_LEVEL = 9;

  static void init();
  // Variables with which to store a compressor on a per-thread basis.
  static pthread_once_t _once;
  static pthread_key_t _key;

  z_stream _stream;
  char _buffer[4096];
};

typedef std::pair<LZ4_stream_t*, struct preserved_hash_table_entry_t*> saved_lz4_stream ;

class LZ4Compressor : public SAS::Compressor
{
public:
  LZ4Compressor();
  ~LZ4Compressor();

  std::string compress(const std::string& s, const SAS::Profile* profile);

  static SAS::Compressor* get();

private:
  // The maximum size of input we can process in one go.
  static const int MAX_INPUT_SIZE = 4096;

  // The default acceleration (1) is sufficient for us and gives best
  // compression.
  static const int ACCELERATION = 1;

  static void init();
  // Variables with which to store a compressor on a per-thread basis.
  static pthread_once_t _once;
  static pthread_key_t _key;

  LZ4_stream_t* _stream;
  char _buffer[LZ4_COMPRESSBOUND(MAX_INPUT_SIZE)];

  std::unordered_map<const SAS::Profile*, saved_lz4_stream> _saved_streams;
};

pthread_once_t ZlibCompressor::_once = PTHREAD_ONCE_INIT;
pthread_key_t ZlibCompressor::_key = {0};
pthread_once_t LZ4Compressor::_once = PTHREAD_ONCE_INIT;
pthread_key_t LZ4Compressor::_key = {0};

/// Statically initialize the Compressor class by creating the thread-local key.
void ZlibCompressor::init()
{
  int rc = pthread_key_create(&_key, SAS::Compressor::destroy);
  if (rc != 0)
  {
    SAS_LOG_WARNING("Failed to create key for zlib SAS parameter compressor");
  }
}

/// Statically initialize the Compressor class by creating the thread-local key.
void LZ4Compressor::init()
{
  int rc = pthread_key_create(&_key, SAS::Compressor::destroy);
  if (rc != 0)
  {
    SAS_LOG_WARNING("Failed to create key for LZ4 SAS parameter compressor");
  }
}

SAS::Profile::Profile(std::string dictionary, Profile::Algorithm a) :
  _dictionary(dictionary),
  _algorithm(a)

{
}

SAS::Profile::Profile(Profile::Algorithm a) :
  _dictionary(""),
  _algorithm(a)

{
}


/// Get a thread-scope Compressor, or create one if it doesn't exist already.
SAS::Compressor* SAS::Compressor::get(SAS::Profile::Algorithm algorithm)
{
  if (algorithm == SAS::Profile::Algorithm::LZ4)
  {
    return LZ4Compressor::get();
  }
  else
  {
    return ZlibCompressor::get();
  }
}

/// Get a thread-scope Compressor, or create one if it doesn't exist already.
SAS::Compressor* ZlibCompressor::get()
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

/// Get a thread-scope Compressor, or create one if it doesn't exist already.
SAS::Compressor* LZ4Compressor::get()
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
void SAS::Compressor::destroy(void* compressor_ptr)
{
  Compressor* compressor = (Compressor*)compressor_ptr;
  delete compressor;
}

/// Compressor constructor.  Initializes the zlib compressor.
ZlibCompressor::ZlibCompressor()
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
    SAS_LOG_WARNING("Failed to initialize zlib SAS parameter compressor (rc=%d)", rc);
  }
}

/// Compressor destructor.  Terminates the zlib compressor.
ZlibCompressor::~ZlibCompressor()
{
  deflateEnd(&_stream);
}

/// Compresses the specified string using the dictionary from the profile (if non-empty).
std::string ZlibCompressor::compress(const std::string& s, const SAS::Profile* profile)
{
  if (profile)
  {
    std::string dictionary = profile->get_dictionary();
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
    SAS_LOG_WARNING("Failed to zlib-compress SAS parameter (rc=%d)", rc);
  }

  // Reset the compressor before we return.
  deflateReset(&_stream);

  return compressed;
}

/// Compressor constructor.  Initializes the LZ4 compressor.
LZ4Compressor::LZ4Compressor()
{
  _stream = LZ4_createStream();
  if (_stream == NULL)
  {
    SAS_LOG_WARNING("Failed to initialize LZ$ SAS parameter compressor");
  }
}

/// Compressor destructor.  Terminates the LZ4 compressor.
LZ4Compressor::~LZ4Compressor()
{
  // Free the stream.  Ignore the return code - the interface doesn't define
  // its meaning, and it's currently hard-coded to 0.
  (void)LZ4_freeStream(_stream); _stream = NULL;

  for (std::unordered_map<const SAS::Profile*, saved_lz4_stream>::iterator saved_stream_iterator = _saved_streams.begin();
       saved_stream_iterator != _saved_streams.end();
       saved_stream_iterator++)
  {
    LZ4_freeStream(saved_stream_iterator->second.first);
    free(saved_stream_iterator->second.second);
  }
}

/// Compresses the specified string using the dictionary from the profile (if non-empty).
std::string LZ4Compressor::compress(const std::string& s, const SAS::Profile* profile)
{
  if (profile)
  {
    std::unordered_map<const SAS::Profile*, saved_lz4_stream>::iterator saved_stream_iterator = _saved_streams.find(profile);

    if (saved_stream_iterator == _saved_streams.end())
    {
      // Set up and cache the stream
      LZ4_stream_t* base_stream = LZ4_createStream();
      struct preserved_hash_table_entry_t* stream_saved_buf;
      LZ4_loadDict(base_stream, profile->get_dictionary().c_str(), profile->get_dictionary().length());
      LZ4_stream_preserve(base_stream, &stream_saved_buf);
      _saved_streams[profile] = saved_lz4_stream(base_stream, stream_saved_buf);
      saved_stream_iterator = _saved_streams.find(profile);
    }
    
    LZ4_stream_restore_preserved(_stream,
                                 saved_stream_iterator->second.first,
                                 saved_stream_iterator->second.second);
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
