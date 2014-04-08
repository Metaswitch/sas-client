/**
 * @file sas.cpp Implementation of SAS class used for reporting events
 * and markers to Service Assurance Server.
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

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/socket.h>

#include "sas.h"


// SAS message types.
const int SAS_MSG_INIT   = 1;
const int SAS_MSG_EVENT  = 3;
const int SAS_MSG_MARKER = 4;

// SAS message header sizes
const int COMMON_HDR_SIZE = sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint64_t);
const int INIT_HDR_SIZE   = COMMON_HDR_SIZE;
const int EVENT_HDR_SIZE  = COMMON_HDR_SIZE + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t);
const int MARKER_HDR_SIZE = COMMON_HDR_SIZE + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t);

const char* SAS_PORT = "6761";

// MIN/MAX string lengths for init parameters.
const int MAX_SYSTEM_LEN = 64;
const int MAX_RESOURCE_ID_LEN = 255;


std::atomic<SAS::TrailId> SAS::_next_trail_id(1);
SAS::Connection* SAS::_connection = NULL;
SAS::sas_log_callback_t* SAS::_log_callback = NULL;


int SAS::init(const std::string& system_name,
               const std::string& system_type,
               const std::string& resource_identifier,
               const std::string& sas_address,
               sas_log_callback_t* log_callback)
{
  _log_callback = log_callback;

  // Check the system and resource parameters are present and have the correct
  // length.
  if (system_name.length() <= 0)
  {
    SAS_LOG_ERROR("Error connecting to SAS - System name is blank.");
    return SAS_INIT_RC_ERR;
  }

  if (system_name.length() > MAX_SYSTEM_LEN)
  {
    SAS_LOG_ERROR("Error connecting to SAS - System name is longer than %d characters.",
                  MAX_SYSTEM_LEN);
    return SAS_INIT_RC_ERR;
  }

  if (system_type.length() <= 0)
  {
    SAS_LOG_ERROR("Error connecting to SAS - System type is blank.");
    return SAS_INIT_RC_ERR;
  }

  if (system_type.length() > MAX_SYSTEM_LEN)
  {
    SAS_LOG_ERROR("Error connecting to SAS - System type is longer than %d characters.",
                  MAX_SYSTEM_LEN);
    return SAS_INIT_RC_ERR;
  }

  if (resource_identifier.length() <= 0)
  {
    SAS_LOG_ERROR("Error connecting to SAS - Resource Identifier is blank.");
    return SAS_INIT_RC_ERR;
  }

  if (resource_identifier.length() > MAX_RESOURCE_ID_LEN)
  {
    SAS_LOG_ERROR("Error connecting to SAS - Resource Identifier is longer than %d characters.",
                  MAX_RESOURCE_ID_LEN);
    return SAS_INIT_RC_ERR;
  }

  if (sas_address != "0.0.0.0")
  {
    _connection = new Connection(system_name,
                                 system_type,
                                 resource_identifier,
                                 sas_address);
  }

  return SAS_INIT_RC_OK;
}


void SAS::term()
{
  delete _connection;
  _connection = NULL;
}


SAS::Connection::Connection(const std::string& system_name,
                            const std::string& system_type,
                            const std::string& resource_identifier,
                            const std::string& sas_address) :
  _system_name(system_name),
  _system_type(system_type),
  _resource_identifier(resource_identifier),
  _sas_address(sas_address),
  _msg_q(MAX_MSG_QUEUE, false),
  _writer(0),
  _sock(-1)
{
  // Open the queue for input
  _msg_q.open();

  // Spawn a thread to open and write to the SAS connection.
  int rc = pthread_create(&_writer, NULL, &writer_thread, this);

  if (rc < 0)
  {
    // LCOV_EXCL_START
    SAS_LOG_ERROR("Error creating SAS thread");
    // LCOV_EXCL_STOP
  }
}


SAS::Connection::~Connection()
{
  // Close off the queue.
  _msg_q.close();

  if (_writer != 0)
  {
    // Signal the writer thread to disconnect the socket and end.
    _msg_q.terminate();

    // Wait for the writer thread to exit.
    pthread_join(_writer, NULL);

    _writer = 0;
  }
}


void* SAS::Connection::writer_thread(void* p)
{
  ((SAS::Connection*)p)->writer();
  return NULL;
}


void SAS::Connection::writer()
{
  while (true)
  {
    int reconnect_timeout = 10000;  // If connect fails, retry every 10 seconds.

    if (connect_init())
    {
      // Now can start dequeuing and sending data.
      std::string msg;
      while ((_sock != -1) && (_msg_q.pop(msg)))
      {
        int len = msg.length();
        char* buf = (char*)msg.data();
        while (len > 0)
        {
          int flags = 0;
#ifdef MSG_NOSIGNAL
          flags |= MSG_NOSIGNAL;
#endif
          int nsent = ::send(_sock, buf, len, flags);
          if (nsent > 0)
          {
            len -= nsent;
            buf += nsent;
          }
          else if ((nsent < 0) && (errno != EINTR))
          {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
            {
              // The send timeout has expired, so close the socket so we
              // try to connect again (and avoid buffering data while waiting
              // for long TCP timeouts).
              SAS_LOG_ERROR("SAS connection to %s:%s locked up: %d %s", _sas_address.c_str(), SAS_PORT, errno, ::strerror(errno));
            }
            else
            {
              // The socket has failed.
              SAS_LOG_ERROR("SAS connection to %s:%s failed: %d %s", _sas_address.c_str(), SAS_PORT, errno, ::strerror(errno));
            }
            ::close(_sock);
            _sock = -1;
            break;
          }
        }
      }

      // Terminate the socket.
      ::close(_sock);

      if (_msg_q.is_terminated())
      {
        // Received a termination signal on the queue, so exit.
        break;
      }

      // Try reconnecting after 1 second after a failure.
      reconnect_timeout = 1000;
    }

    // Wait on the input queue for the specified timeout before trying to
    // reconnect.  We wait on the queue so we get a kick if the term function
    // is called.
    std::string msg;
    SAS_LOG_DEBUG("Waiting to reconnect to SAS - timeout = %d", reconnect_timeout);
    if (!_msg_q.pop(msg, reconnect_timeout))
    {
      // Received a termination signal on the queue, so exit.
      break;
    }
  }
}


bool SAS::Connection::connect_init()
{
  int rc;
  struct addrinfo hints, *addrs;

  SAS_LOG_STATUS("Attempting to connect to SAS %s", _sas_address.c_str());

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  rc = getaddrinfo(_sas_address.c_str(), SAS_PORT, &hints, &addrs);

  if (rc != 0)
  {
    SAS_LOG_ERROR("Failed to get addresses for SAS %s:%s : %d %s",
                     _sas_address.c_str(), SAS_PORT, errno, ::strerror(errno));
    return false;
  }

  // Set a maximum send timeout on the socket so we don't wait forever if the
  // connection fails.
  struct timeval timeout;
  timeout.tv_sec = SEND_TIMEOUT;
  timeout.tv_usec = 0;

  struct addrinfo *p;

  // Reset the return code to error
  rc = 1;

  for (p = addrs; p != NULL; p = p->ai_next)
  {
    if ((_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      // There was an error opening the socket - try the next address
      SAS_LOG_DEBUG("Failed to open socket");
      continue;
    }

    rc = ::setsockopt(_sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                                                              sizeof(timeout));

    if (rc < 0)
    {
      SAS_LOG_ERROR("Failed to set send timeout on SAS connection : %d %d %s",
                                                 rc, errno, ::strerror(errno));
      ::close(_sock);
      _sock = -1;
      continue;
    }

    rc = ::connect(_sock, p->ai_addr, p->ai_addrlen);

    if (rc < 0)
    {
      // There was an error connecting - try the next address
      SAS_LOG_DEBUG("Failed to connect to address: %s", p->ai_addr);
      ::close(_sock);
      _sock = -1;
      continue;
    }

    // Connection successful at this point
    break;
  }

  if (rc != 0)
  {
    SAS_LOG_ERROR("Failed to connect to SAS %s:%s : %d %s", _sas_address.c_str(), SAS_PORT, errno, ::strerror(errno));
    return false;
  }

  freeaddrinfo(addrs);

  SAS_LOG_DEBUG("Connected SAS socket to %s:%s", _sas_address.c_str(), SAS_PORT);

  // Send an init message to SAS.
  uint8_t init_msg_buf[MAX_MSG_SIZE];
  uint8_t* write_ptr = init_msg_buf;

  std::string version("v0.1");

  // The resource version is part of the binary protocol but is not currently
  // exposed over the C++ API.
  std::string resource_version("");

  int init_len = INIT_HDR_SIZE +
                 sizeof(uint8_t) + _system_name.length() +
                 sizeof(uint32_t) +
                 sizeof(uint8_t) + version.length() +
                 sizeof(uint8_t) + _system_type.length() +
                 sizeof(uint8_t) + _resource_identifier.length() +
                 sizeof(uint8_t) + resource_version.length();

  write_hdr(write_ptr, init_len, SAS_MSG_INIT);
  write_int8(write_ptr, (uint8_t)_system_name.length());
  write_data(write_ptr, _system_name.length(), _system_name.data());
  int endianness = 1;
  write_data(write_ptr, sizeof(int), (char*)&endianness);     // Endianness must be written in machine order.
  write_int8(write_ptr, version.length());
  write_data(write_ptr, version.length(), version.data());
  write_int8(write_ptr, (uint8_t)_system_type.length());
  write_data(write_ptr, _system_type.length(), _system_type.data());
  write_int8(write_ptr, (uint8_t)_resource_identifier.length());
  write_data(write_ptr, _resource_identifier.length(), _resource_identifier.data());
  write_int8(write_ptr, (uint8_t)resource_version.length());
  write_data(write_ptr, resource_version.length(), resource_version.data());

  SAS_LOG_DEBUG("Sending SAS INIT message");

  rc = ::send(_sock, init_msg_buf, (write_ptr - init_msg_buf), 0);
  if (rc < 0)
  {
    SAS_LOG_ERROR("SAS connection to %s:%s failed: %d %s", _sas_address.c_str(), SAS_PORT, errno, ::strerror(errno));
    ::close(_sock);
    _sock = -1;
    return false;
  }

  SAS_LOG_STATUS("Connected to SAS %s:%s", _sas_address.c_str(), SAS_PORT);

  return true;
}


void SAS::Connection::send_msg(std::string msg)
{
  _msg_q.push_noblock(msg);
}


SAS::TrailId SAS::new_trail(uint32_t instance)
{
  TrailId trail = _next_trail_id++;
  return trail;
}


void SAS::report_event(Event& event)
{
  if (_connection)
  {
    _connection->send_msg(event.to_string());
  }
}


void SAS::report_marker(Marker& marker, Marker::Scope scope)
{
  if (_connection)
  {
    _connection->send_msg(marker.to_string(scope));
  }
}


void SAS::write_hdr(uint8_t*& write_ptr, uint16_t msg_length, uint8_t msg_type)
{
  SAS::write_int16(write_ptr, msg_length);
  SAS::write_int8(write_ptr, 3);             // Version = 3
  SAS::write_int8(write_ptr, msg_type);
  SAS::write_timestamp(write_ptr);
}


void SAS::write_int8(uint8_t*& write_ptr, uint8_t c)
{
  write_data(write_ptr, sizeof(uint8_t), (char*)&c);
}


void SAS::write_int16(uint8_t*& write_ptr, uint16_t v)
{
  uint16_t v_nw = htons(v);
  write_data(write_ptr, sizeof(uint16_t), (char*)&v_nw);
}


void SAS::write_int32(uint8_t*& write_ptr, uint32_t v)
{
  uint32_t v_nw = htonl(v);
  write_data(write_ptr, sizeof(uint32_t), (char*)&v_nw);
}


void SAS::write_int64(uint8_t*& write_ptr, uint64_t v)
{
  uint32_t vh_nw = htonl(v >> 32);
  uint32_t vl_nw = htonl(v & 0xffffffff);
  write_data(write_ptr, sizeof(uint32_t), (char*)&vh_nw);
  write_data(write_ptr, sizeof(uint32_t), (char*)&vl_nw);
}


void SAS::write_data(uint8_t*& write_ptr, size_t len, const char* data)
{
  memcpy(write_ptr, data, len);
  write_ptr += len;
}


void SAS::write_timestamp(uint8_t*& write_ptr)
{
  unsigned long long timestamp;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  timestamp = ts.tv_sec;
  timestamp = timestamp * 1000 + (ts.tv_nsec / 1000000);
  write_int64(write_ptr, timestamp);
}


void SAS::write_trail(uint8_t*& write_ptr, TrailId trail)
{
  write_int64(write_ptr, trail);
}


SAS::Message& SAS::Message::add_static_param(uint32_t param)
{
  if ((_buffer_len + 4) <= _buffer_size)
  {
    // There is space for the parameter - work out where it goes.
    uint8_t* write_ptr = _buffer + _params_offset + (_num_static_data * 4);

    // If we already have variable length parameters, shuffle them along in
    // memory.
    if (write_ptr < (_buffer + _buffer_len))
    {
      memmove(write_ptr, write_ptr + 4, (_buffer_len - (write_ptr - _buffer)));
    }

    // Static parameters are written in native byte order, not network order.
    write_data(write_ptr, sizeof(uint32_t), (char*)&param);
    _num_static_data++;
    _buffer_len += 4;

    // Write the new length of the static data.
    write_ptr = _buffer + _params_offset;
    write_int16(write_ptr, (_num_static_data * 4));
  }
  else
  {
    SAS_LOG_WARNING("Insufficient space for static param"
                    " Required: 4 bytes,"
                    " Available: %lu bytes",
                    (_buffer_size - _buffer_len));
  }

  return *this;
}


SAS::Message& SAS::Message::add_var_param(size_t len, uint8_t* data)
{
  if ((_buffer_len + len + 2) <= _buffer_size)
  {
    // There is space for the parameter, so write it. Note that variable
    // params go after static params, so this param should go at the end.
    uint8_t *write_ptr = _buffer + _buffer_len;
    write_int16(write_ptr, len);
    write_data(write_ptr, len, (char*)data);

    _var_data_lengths[_num_var_data] = len;
    _num_var_data++;

    _buffer_len = (write_ptr - _buffer);
  }
  else
  {
    SAS_LOG_WARNING("Insufficient space for var param"
                    " Required: %lu + 2 bytes,"
                    " Available: %lu bytes",
                    len, (_buffer_size - _buffer_len));
  }

  return *this;
}


std::string SAS::Event::to_string()
{
  std::string s;
  uint8_t* write_ptr = _buffer;

  write_hdr(write_ptr, _buffer_len, SAS_MSG_EVENT);
  write_trail(write_ptr, _trail);
  write_int32(write_ptr, _id);
  write_int32(write_ptr, _instance);

  s.assign((char*)_buffer, _buffer_len);

  return s;
}


std::string SAS::Marker::to_string(Marker::Scope scope)
{
  std::string s;
  uint8_t* write_ptr = _buffer;

  write_hdr(write_ptr, _buffer_len, SAS_MSG_MARKER);
  write_trail(write_ptr, _trail);
  write_int32(write_ptr, _id);
  write_int32(write_ptr, _instance);
  write_int8(write_ptr, (uint8_t)(scope != Scope::None));
  write_int8(write_ptr, (uint8_t)scope);

  s.assign((char*)_buffer, _buffer_len);

  return s;
}


void SAS::log_to_stdout(log_level_t level,
                        const char *module,
                        int line_number,
                        const char *fmt,
                        ...)
{
  va_list args;
  const char* level_str;

  va_start(args, fmt);

  switch (level) {
    case LOG_LEVEL_ERROR:   level_str = "ERROR"; break;
    case LOG_LEVEL_WARNING: level_str = "WARNING"; break;
    case LOG_LEVEL_STATUS:  level_str = "STATUS"; break;
    case LOG_LEVEL_INFO:    level_str = "INFO"; break;
    case LOG_LEVEL_VERBOSE: level_str = "VERBOSE"; break;
    case LOG_LEVEL_DEBUG:   level_str = "DEBUG"; break;
    default:                level_str = "UNKNOWN"; break;
  }

  printf("%s %s:%d: ", level_str, module, line_number);
  vprintf(fmt, args);
  printf("\n");
  fflush(stdout);

  va_end(args);
}


void SAS::discard_logs(log_level_t level,
                       const char *module,
                       int line_number,
                       const char *fmt,
                       ...)
{
}
