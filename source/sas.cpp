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
#include <unistd.h>

#include "sas.h"
#include "sas_eventq.h"
#include "sas_internal.h"

const char* SAS_PORT = "6761";

// MIN/MAX string lengths for init parameters.
const unsigned int MAX_SYSTEM_LEN = 64;
const unsigned int MAX_RESOURCE_ID_LEN = 255;

const uint8_t ASSOC_OP_ASSOCIATE = 0x01;
const uint8_t ASSOC_OP_NO_REACTIVATE = 0x02;

std::atomic<SAS::TrailId> SAS::_next_trail_id(1);
SAS::Connection* SAS::_connection = NULL;
SAS::sas_log_callback_t* SAS::_log_callback = NULL;
SAS::create_socket_callback_t* SAS::_socket_callback = NULL;

class SAS::Connection
{
public:
  Connection(const std::string& system_name,
             const std::string& system_type,
             const std::string& resource_identifier,
             const std::string& sas_address);
  ~Connection();

  void send_msg(std::string msg);

  static void* writer_thread(void* p);

private:
  bool connect_init();
  static int get_local_sock(const char* sas_address, const char* sas_port);
  static bool set_send_timeout(int sock, int timeout);
  void writer();

  std::string _system_name;
  std::string _system_type;
  std::string _resource_identifier;
  std::string _sas_address;

  SASeventq<std::string> _msg_q;
  bool _connected;

  pthread_t _writer;

  // Socket for the connection.
  int _sock;

  /// Send timeout for the socket in seconds.
  static const int SEND_TIMEOUT = 5;

  /// Maximum depth of SAS message queue.
  static const int MAX_MSG_QUEUE = 100000;
};

int SAS::init(std::string system_name,
              const std::string& system_type,
              const std::string& resource_identifier,
              const std::string& sas_address,
              sas_log_callback_t* log_callback,
              create_socket_callback_t* socket_callback)
{
  _log_callback = log_callback;
  _socket_callback = socket_callback;

  if (sas_address != "0.0.0.0")
  {
    // Check the system and resource parameters are present and have the correct
    // length.
    if (system_name.length() <= 0)
    {
      SAS_LOG_ERROR("Error connecting to SAS - System name is blank.");
      return SAS_INIT_RC_ERR;
    }

    if (system_name.length() > MAX_SYSTEM_LEN)
    {
      SAS_LOG_WARNING("System name is longer than %d characters, truncating.",
                      MAX_SYSTEM_LEN);
      system_name.resize(MAX_SYSTEM_LEN);
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

    // If we haven't yet connected, we can cancel the thread - there's
    // no risk of truncating data mid-write. This prevents termination
    // from taking an unnecessarily long time when SAS is unreachable
    // (which would happen if we didn't cancel the thread and instead
    // waited for connect() to time out).
    if (!_connected)
    {
      pthread_cancel(_writer);
    }

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
    _connected = false;
    if (connect_init())
    {
      _connected = true;
      // Now can start dequeuing and sending data.
      std::string msg;
      while ((_sock > 0) && _msg_q.pop(msg, 1000))
      {
        if (msg.empty())
        {
          // No real messages for a second, so send a heartbeat message
          msg = SAS::heartbeat_msg();
        }

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
        msg.clear();
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

    // Wait for the specified timeout before trying to
    // reconnect.
    SAS_LOG_DEBUG("Waiting to reconnect to SAS - timeout = %d", reconnect_timeout);
    while (reconnect_timeout > 0 && !_msg_q.is_terminated())
    {
      usleep(1000 * 1000);
      reconnect_timeout -= 1000;
    }
    if (_msg_q.is_terminated())
    {
      // Received a termination signal on the queue, so exit.
      break;
    }
  }
}

bool SAS::Connection::set_send_timeout(int sock, int timeout_secs)
{
  // Set a maximum send timeout on the socket so we don't wait forever if the
  // connection fails.
  struct timeval timeout;
  timeout.tv_sec = timeout_secs;
  timeout.tv_usec = 0;

  int rc = ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                    sizeof(timeout));

  return (rc == 0);
}

int SAS::Connection::get_local_sock(const char* sas_address, const char* sas_port)
{
  int rc;
  struct addrinfo hints, *addrs;

  SAS_LOG_STATUS("Attempting to connect to SAS %s", sas_address);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  rc = getaddrinfo(sas_address, sas_port, &hints, &addrs);

  if (rc != 0)
  {
    SAS_LOG_ERROR("Failed to get addresses for SAS %s:%s : %d %s",
                  sas_address, sas_port, errno, ::strerror(errno));
    return -1;
  }

  struct addrinfo *p;

  int sock;

  // Reset the return code to error
  rc = 1;

  for (p = addrs; p != NULL; p = p->ai_next)
  {
    if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      // There was an error opening the socket - try the next address
      SAS_LOG_DEBUG("Failed to open socket");
      continue;
    }

    if (!set_send_timeout(sock, SEND_TIMEOUT))
    {
      SAS_LOG_ERROR("Failed to set send timeout on SAS connection : %d %d %s",
                                                 rc, errno, ::strerror(errno));
      ::close(sock);
      sock = -1;
      continue;
    }

    rc = ::connect(sock, p->ai_addr, p->ai_addrlen);

    if (rc < 0)
    {
      // There was an error connecting - try the next address
      SAS_LOG_DEBUG("Failed to connect to address: %s", p->ai_addr);
      ::close(sock);
      sock = -1;
      continue;
    }

    // Connection successful at this point
    break;
  }

  if (rc != 0)
  {
    SAS_LOG_ERROR("Failed to connect to SAS %s:%s : %d %s", sas_address, sas_port, errno, ::strerror(errno));
    return -1;
  }

  freeaddrinfo(addrs);

  return sock;
}


bool SAS::Connection::connect_init()
{
  if (_socket_callback)
  {
    _sock = _socket_callback(_sas_address.c_str(), SAS_PORT);
  }
  else
  {
    _sock = get_local_sock(_sas_address.c_str(), SAS_PORT);
  }

  if (_sock < 0)
  {
    return false;
  }

  SAS_LOG_DEBUG("Connected SAS socket to %s:%s", _sas_address.c_str(), SAS_PORT);
  set_send_timeout(_sock, SEND_TIMEOUT);

  // Send an init message to SAS.
  std::string init;
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
  init.reserve(init_len);
  write_hdr(init, init_len, SAS_MSG_INIT, get_current_timestamp());
  write_int8(init, (uint8_t)_system_name.length());
  write_data(init, _system_name.length(), _system_name.data());
  int endianness = 1;
  init.append((char*)&endianness, sizeof(int)); // Endianness must be written in machine order.
  write_int8(init, version.length());
  write_data(init, version.length(), version.data());
  write_int8(init, (uint8_t)_system_type.length());
  write_data(init, _system_type.length(), _system_type.data());
  write_int8(init, (uint8_t)_resource_identifier.length());
  write_data(init, _resource_identifier.length(), _resource_identifier.data());
  write_int8(init, (uint8_t)resource_version.length());
  write_data(init, resource_version.length(), resource_version.data());

  SAS_LOG_DEBUG("Sending SAS INIT message");

  int rc = ::send(_sock, init.data(), init.length(), 0);
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


void SAS::report_event(const Event& event)
{
  if (_connection)
  {
    _connection->send_msg(event.to_string());
  }
}

void SAS::report_analytics(const Analytics& analytics, bool sas_store)
{
  if (_connection)
  {
    _connection->send_msg(analytics.to_string(sas_store));
  }
}


void SAS::report_marker(const Marker& marker, Marker::Scope scope, bool reactivate)
{
  if (_connection)
  {
    _connection->send_msg(marker.to_string(scope, reactivate));
  }
}

void SAS::associate_trails(TrailId trail_a,
                           TrailId trail_b,
                           Marker::Scope scope)
{
  std::string trail_assoc_msg;
  write_hdr(trail_assoc_msg, 29, SAS_MSG_TRAIL_ASSOC, get_current_timestamp());
  write_trail(trail_assoc_msg, trail_a);
  write_trail(trail_assoc_msg, trail_b);
  write_int8(trail_assoc_msg, (uint8_t)scope);
  if (_connection)
  {
    _connection->send_msg(trail_assoc_msg);
  }
}

std::string SAS::heartbeat_msg()
{
  std::string s;
  SAS::write_int16(s, 4);
  SAS::write_int8(s, 3);             // Version = 3
  SAS::write_int8(s, 5);             // Type = Heartbeat
  return s;
}



SAS::Timestamp SAS::get_current_timestamp()
{
  Timestamp timestamp;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  timestamp = ts.tv_sec;
  timestamp = timestamp * 1000 + (ts.tv_nsec / 1000000);
  return timestamp;
}


void SAS::write_hdr(std::string& s,
                    uint16_t msg_length,
                    uint8_t msg_type,
                    Timestamp timestamp)
{
  SAS::write_int16(s, msg_length);
  SAS::write_int8(s, 3);             // Version = 3
  SAS::write_int8(s, msg_type);
  SAS::write_int64(s, timestamp);
}


void SAS::write_int8(std::string& s, uint8_t c)
{
  write_data(s, sizeof(uint8_t), (char*)&c);
}


void SAS::write_int16(std::string& s, uint16_t v)
{
  uint16_t v_nw = htons(v);
  write_data(s, sizeof(uint16_t), (char*)&v_nw);
}


void SAS::write_int32(std::string& s, uint32_t v)
{
  uint32_t v_nw = htonl(v);
  write_data(s, sizeof(uint32_t), (char*)&v_nw);
}


void SAS::write_int64(std::string& s, uint64_t v)
{
  uint32_t vh_nw = htonl(v >> 32);
  uint32_t vl_nw = htonl(v & 0xffffffff);
  write_data(s, sizeof(uint32_t), (char*)&vh_nw);
  write_data(s, sizeof(uint32_t), (char*)&vl_nw);
}


void SAS::write_data(std::string& s, size_t len, const char* data)
{
  s.append(data, len);
}


void SAS::write_trail(std::string& s, TrailId trail)
{
  write_int64(s, trail);
}


// Return the serialized length of the static and variable parameters (including
// length fields).
size_t SAS::Message::params_buf_len() const
{
  size_t len;
  len = 2 + (_static_params.size() * sizeof(uint32_t));

  for(std::vector<std::string>::const_iterator it = _var_params.begin();
      it != _var_params.end();
      ++it)
  {
    len += 2 + it->size();
  }

  return len;
}


// Write the static and variable parameters (including length fields) to the
// supplied string.
void SAS::Message::write_params(std::string& s) const
{
  write_int16(s, (_static_params.size() * 4));
  for(std::vector<uint32_t>::const_iterator sp = _static_params.begin();
      sp != _static_params.end();
      ++sp)
  {
    // Static parameters are written in native byte order.
    write_data(s, sizeof(*sp), (char*)&(*sp));
  }

  for(std::vector<std::string>::const_iterator vp = _var_params.begin();
      vp != _var_params.end();
      ++vp)
  {
    write_int16(s, vp->length());
    write_data(s, vp->length(), vp->data());
  }
}


std::string SAS::Event::to_string() const
{
  size_t len = EVENT_HDR_SIZE + params_buf_len();

  std::string s;
  s.reserve(len);

  write_hdr(s, len, SAS_MSG_EVENT, get_timestamp());
  write_trail(s, _trail);
  write_int32(s, _id);
  write_int32(s, _instance);
  write_params(s);

  return std::move(s);
}


std::string SAS::Analytics::to_string(bool sas_store) const
{
  size_t len = ANALYTICS_STATIC_HDR_SIZE + variable_header_buf_len() \
                  + params_buf_len();
  std::string s;
  s.reserve(len);

  write_hdr(s, len, SAS_MSG_ANALYTICS, get_timestamp());
  write_trail(s, _trail);
  write_int32(s, _id);
  write_int32(s, _instance);
  write_int8(s, (uint8_t)_format);

  // Set the 'store message' bit if the message should be stored by SAS as well
  // as forwarded to the Analytics server.
  write_int8(s, (uint8_t)sas_store);

  write_int16(s, (uint16_t)_source_type.length());
  write_data(s, _source_type.length(), _source_type.data());
  write_int16(s, (uint16_t)_friendly_id.length());
  write_data(s, _friendly_id.length(), _friendly_id.data());
  write_params(s);

  return std::move(s);
}


// Get the timestamp to be used on the message.
SAS::Timestamp SAS::Analytics::get_timestamp() const
{
  return SAS::get_current_timestamp();
}


// Return the length of the source_type and friendly_id fields (including
// length fields).
// These consist of:
//   [ 2 bytes ] Source type length
//   [ n bytes ] Source type
//   [ 2 bytes ] Friendly ID length
//   [ n bytes ] Friendly ID
size_t SAS::Analytics::variable_header_buf_len() const
{
  return 2 + _source_type.length() + 2 + _friendly_id.length();
}


// Get the timestamp to be used on the message.
SAS::Timestamp SAS::Event::get_timestamp() const
{
  if (!_timestamp_set)
  {
    // Timestamp not already specified - use the current time.
    return SAS::get_current_timestamp();
  }
  else
  {
    // Timestamp has already been specified - use that.
    return _timestamp;
  }
}


std::string SAS::Marker::to_string(Marker::Scope scope, bool reactivate) const
{
  size_t len = MARKER_HDR_SIZE + params_buf_len();

  std::string s;
  s.reserve(len);

  write_hdr(s, len, SAS_MSG_MARKER, get_timestamp());
  write_trail(s, _trail);
  write_int32(s, _id);
  write_int32(s, _instance);

  // Work out how to fill in the association flags byte.
  uint8_t assoc_flags = 0;
  if (scope != Scope::None)
  {
    assoc_flags = (assoc_flags | ASSOC_OP_ASSOCIATE);

    if (!reactivate)
    {
      assoc_flags = (assoc_flags | ASSOC_OP_NO_REACTIVATE);
    }
  }

  write_int8(s, assoc_flags);
  write_int8(s, (uint8_t)scope);
  write_params(s);

  return std::move(s);
}


// Get the timestamp to be used on the message.
SAS::Timestamp SAS::Marker::get_timestamp() const
{
  return SAS::get_current_timestamp();
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
