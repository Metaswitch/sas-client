/**
 * @file sas.h Definition of SAS class used for reporting events and markers
 * to Service Assurance Server
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

#ifndef SAS_H__
#define SAS_H__

#include <stdint.h>
#include <string.h>
#include <string>
#include <atomic>

#include "eventq.h"


class SAS
{
public:
  typedef uint64_t TrailId;

  class Message
  {
  public:
    static const int MAX_NUM_STATIC_PARAMS = 20;
    static const int MAX_NUM_VAR_PARAMS = 20;

    inline Message(TrailId trail, uint32_t id, uint32_t instance)
    {
      _trail = trail;
      _msg.hdr.id = id;
      _msg.hdr.instance = instance;
      _msg.hdr.static_data_len = 0;
      _msg.hdr.num_var_data = 0;
      _msg.hdr.var_data_array = _msg.var_data;
    }

    inline Message& add_static_param(uint32_t param)
    {
      _msg.static_data[_msg.hdr.static_data_len / sizeof(uint32_t)] = param;
      _msg.hdr.static_data_len += sizeof(uint32_t);
      return *this;
    }

    inline Message& add_var_param(size_t len, uint8_t* data)
    {
      _msg.var_data[_msg.hdr.num_var_data].len = (uint32_t)len;
      _msg.var_data[_msg.hdr.num_var_data].ptr = data;
      ++_msg.hdr.num_var_data;
      return *this;
    }

    inline Message& add_var_param(size_t len, char* s)
    {
      return add_var_param(len, (uint8_t*)s);
    }

    inline Message& add_var_param(char* s)
    {
      return add_var_param(strlen(s), (uint8_t*)s);
    }

    inline Message& add_var_param(const std::string& s)
    {
      return add_var_param(s.length(), (uint8_t*)s.data());
    }

    friend class SAS;

  private:
    TrailId _trail;
    struct
    {
      struct
      {
        uint32_t id;
        uint32_t instance;
        uint32_t static_data_len;
        uint32_t num_var_data;
        void* var_data_array;
      } hdr;
      uint32_t static_data[MAX_NUM_STATIC_PARAMS];
      struct {
        uint32_t len;
        uint8_t* ptr;
      } var_data[MAX_NUM_VAR_PARAMS];
    } _msg;
  };

  class Event : public Message
  {
  public:
    // Event IDs as defined by the application are restricted to 24 bits.
    // This is because the top byte of the event ID is reserved and set to 0x0F.
    // It is comprised of:
    //   - The top nibble, which is reserved for future use and must be set to
    //     0x0.
    //   - the bottom nibble, which SAS requires be set to the value 0xF.
    inline Event(TrailId trail, uint32_t event, uint32_t instance) :
      Message(trail,
              ((event & 0x00FFFFFF) | 0x0F000000),
              instance)
    {
    }

    std::string to_string() const;
  };

  class Marker : public Message
  {
  public:
    inline Marker(TrailId trail, uint32_t marker, uint32_t instance) :
      Message(trail, marker, instance)
    {
    }

    enum Scope
    {
      None = 0,
      Branch = 1,
      TrailGroup = 2
    };

    std::string to_string(Scope scope) const;
  };

  enum log_level_t {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_STATUS = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_VERBOSE = 4,
    LOG_LEVEL_DEBUG = 5,
  };

  typedef void (sas_log_callback_t)(log_level_t level,
                                    const char *module,
                                    int line_number,
                                    const char *fmt,
                                    ...);

  // A simple implementation of sas_log_callback_t that logs messages to stdout.
  static void log_to_stdout(log_level_t level,
                            const char *module,
                            int line_number,
                            const char *fmt,
                            ...);

  // A simple implementation of sas_log_callback_t that discards all logs.
  static void discard_logs(log_level_t level,
                           const char *module,
                           int line_number,
                           const char *fmt,
                           ...);

  static void init(const std::string& system_name,
                   const std::string& system_type,
                   const std::string& resource_identifier,
                   const std::string& sas_address,
                   sas_log_callback_t* log_callback);
  static void term();
  static TrailId new_trail(uint32_t instance);
  static void report_event(const Event& event);
  static void report_marker(const Marker& marker, Marker::Scope scope=Marker::Scope::None);

private:
  class Connection
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
    void writer();

    std::string _system_name;
    std::string _system_type;
    std::string _resource_identifier;
    std::string _sas_address;

    eventq<std::string> _msg_q;

    pthread_t _writer;

    // Socket for the connection.
    int _sock;

    /// Send timeout for the socket in seconds.
    static const int SEND_TIMEOUT = 30;

    /// Maximum depth of SAS message queue.
    static const int MAX_MSG_QUEUE = 1000;
  };

  static void write_hdr(std::string& s, uint16_t msg_length, uint8_t msg_type);
  static void write_int8(std::string& s, uint8_t c);
  static void write_int16(std::string& s, uint16_t v);
  static void write_int32(std::string& s, uint32_t v);
  static void write_int64(std::string& s, uint64_t v);
  static void write_data(std::string& s, size_t length, const char* data);
  static void write_timestamp(std::string& s);
  static void write_trail(std::string& s, TrailId trail);

  static std::atomic<TrailId> _next_trail_id;
  static Connection* _connection;
  static sas_log_callback_t* _log_callback;
};

#endif
