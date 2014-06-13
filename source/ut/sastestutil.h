/**
 * @file sastestutil.h Test utilities for the SAS client library.
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

#ifndef SASTESTUTIL_H__
#define SASTESTUTIL_H__

#include <iostream>
#include <iomanip>
#include <vector>
#include <utility>
#include <sstream>

namespace SasTest
{
  // Utility function to turn a character into a byte.
  //
  // This is useful when working with STL strings which represent a sequence of
  // bytes.
  inline uint8_t to_byte(char c)
  {
    if (c >= 0)
    {
      return c;
    }
    else
    {
      return 256 + c;
    }
  }

  class Message
  {
  public:
    Message() : _offset(0) {}
    ~Message() {}

    // Parsed out static and variable parameters.
    std::vector<uint32_t> static_params;
    std::vector<std::string> var_params;

  protected:
    // Parsing offset.
    size_t _offset;

    // Buffer containing the message to parse.
    std::string _buffer;

    // Exception class used to signal a parsing error.
    class ParseError {};

    // Check there are at least n bytes left in the buffer.
    void check_remaining_bytes(int n)
    {
      if ((_offset + n) > _buffer.length())
      {
        ParseError ex; throw ex;
      }
    }

    //
    // Parse various data types out of the buffer.
    //

    void parse_int8(uint8_t& value)
    {
      check_remaining_bytes(1);

      value = 0;
      value |= (to_byte(_buffer[_offset++]) << 0);
    }

    void parse_network_int16(uint16_t& value)
    {
      check_remaining_bytes(2);

      value = 0;
      value |= ((uint16_t)to_byte(_buffer[_offset++]) << 8);
      value |= ((uint16_t)to_byte(_buffer[_offset++]) << 0);
    }

    void parse_network_int32(uint32_t& value)
    {
      check_remaining_bytes(4);

      value = 0;
      value |= ((uint32_t)to_byte(_buffer[_offset++]) << 24);
      value |= ((uint32_t)to_byte(_buffer[_offset++]) << 16);
      value |= ((uint32_t)to_byte(_buffer[_offset++]) << 8);
      value |= ((uint32_t)to_byte(_buffer[_offset++]) << 0);
    }

    void parse_native_int32(uint32_t& value)
    {
      check_remaining_bytes(4);

      value = 0;
      value |= ((uint32_t)to_byte(_buffer[_offset++]) << 0);
      value |= ((uint32_t)to_byte(_buffer[_offset++]) << 8);
      value |= ((uint32_t)to_byte(_buffer[_offset++]) << 16);
      value |= ((uint32_t)to_byte(_buffer[_offset++]) << 24);
    }

    void parse_network_int64(uint64_t& value)
    {
      check_remaining_bytes(8);

      value = 0;
      value |= ((uint64_t)to_byte(_buffer[_offset++]) << 56);
      value |= ((uint64_t)to_byte(_buffer[_offset++]) << 48);
      value |= ((uint64_t)to_byte(_buffer[_offset++]) << 40);
      value |= ((uint64_t)to_byte(_buffer[_offset++]) << 32);
      value |= ((uint64_t)to_byte(_buffer[_offset++]) << 24);
      value |= ((uint64_t)to_byte(_buffer[_offset++]) << 16);
      value |= ((uint64_t)to_byte(_buffer[_offset++]) << 8);
      value |= ((uint64_t)to_byte(_buffer[_offset++]) << 0);
    }

    void parse_string(std::string& s, int len)
    {
      check_remaining_bytes(len);

      s = _buffer.substr(_offset, len);
      _offset += len;
    }

    // Parse the static and variable parameters out of the buffer.
    void parse_params()
    {
      uint16_t static_len;
      parse_network_int16(static_len);

      for (int i = 0; i < (static_len / sizeof(uint32_t)); ++i)
      {
        uint32_t val;
        parse_native_int32(val);
        static_params.push_back(val);
      }

      while (_offset < _buffer.length())
      {
        uint16_t len;
        std::string val;

        parse_network_int16(len);
        parse_string(val, len);

        var_params.push_back(val);
      }
    }

    // Called when the prase is complete. Checks that;
    // -  All butes have been consumed.
    // -  The buffer has the supplied length (which will usually have been
    //    parsed out of the first two bytes of the buffer).
    void parse_complete(size_t length)
    {
      if (_offset != _buffer.length())
      {
        ParseError ex; throw ex;
      }

      if (length != _buffer.length())
      {
        ParseError ex; throw ex;
      }
    }

    // Utility method to print the static and variable parameters to a string.
    std::string params_to_string()
    {
      std::ostringstream oss;

      oss << "Static Params:" << std::endl;
      for (size_t i = 0; i < static_params.size(); ++i)
      {
        oss << "  " << i << ":  " << static_params[i] << std::endl;
      }

      oss << "Variable Params:" << std::endl;
      for (size_t i = 0; i < var_params.size(); ++i)
      {
        oss << "  " << i << ":  " << var_params[i] << std::endl;
      }

      return oss.str();
    }
  };

  class Event : public Message
  {
  public:
    Event() :
      Message(),
      length(0), version(0), msg_type(0), trail(0), event_id(0), instance_id(0)
    {}

    ~Event() {};

    // Parse a supplied buffer as an Event.
    //
    // @param buf the buffer to parse.
    // @return whether the message parsed successfully.
    bool parse(std::string buf)
    {
      _buffer = buf;
      _offset = 0;
      bool success = true;

      try
      {
        parse_network_int16(length);
        parse_int8(version);
        parse_int8(msg_type);
        parse_network_int64(timestamp);
        parse_network_int64(trail);
        parse_network_int32(event_id);
        parse_network_int32(instance_id);
        parse_params();
        parse_complete(length);
      }
      catch (ParseError)
      {
        success = false;
      }

      return success;
    }

    // Return a string representation of the Event.
    std::string to_string()
    {
      std::ostringstream oss;
      oss << "Length:            " << length << std::endl;
      oss << "Version:           " << (int)version << std::endl;
      oss << "Type:              " << (int)msg_type << std::endl;
      oss << "Trail ID:          " << trail << std::endl;
      oss << "Event ID:          " << event_id << std::endl;
      oss << "Instance ID:       " << instance_id << std::endl;
      oss << params_to_string();

      return oss.str();
    }

    uint16_t length;
    uint8_t version;
    uint8_t msg_type;
    uint64_t timestamp;
    SAS::TrailId trail;
    uint32_t event_id;
    uint32_t instance_id;
  };

  class Marker : public Message
  {
  public:
    static const uint8_t ASSOC_FLAG_ASSOCIATE = 0x01;
    static const uint8_t ASSOC_FLAG_NO_REACTIVATE = 0x02;

    Marker() :
      length(0), version(0), msg_type(0), trail(0), marker_id(0), instance_id(0),
      association_flags(0), scope(0)
    {}

    ~Marker() {};

    // Parse a supplied buffer as a Marker.
    //
    // @param buf the buffer to parse.
    // @return whether the message parsed successfully.
    bool parse(std::string buf)
    {
      _buffer = buf;
      _offset = 0;
      bool success = true;

      try
      {
        parse_network_int16(length);
        parse_int8(version);
        parse_int8(msg_type);
        parse_network_int64(timestamp);
        parse_network_int64(trail);
        parse_network_int32(marker_id);
        parse_network_int32(instance_id);
        parse_int8(association_flags);
        parse_int8(scope);
        parse_params();
        parse_complete(length);

        // Parse the association flags, checking no unexpected flags are set. 
        associate = ((association_flags & ASSOC_FLAG_ASSOCIATE) != 0);
        no_reactivate = ((association_flags & ASSOC_FLAG_NO_REACTIVATE) != 0);
        
        if ((association_flags & (ASSOC_FLAG_ASSOCIATE | ASSOC_FLAG_NO_REACTIVATE)) != 0)
        {
          throw ParseError();
        }
      }
      catch (ParseError)
      {
        success = false;
      }

      return success;
    }

    // Return a string representation of the Marker.
    std::string to_string()
    {
      std::ostringstream oss;
      oss << "Length:            " << length << std::endl;
      oss << "Version:           " << (int)version << std::endl;
      oss << "Type:              " << (int)msg_type << std::endl;
      oss << "Trail ID:          " << trail << std::endl;
      oss << "Marker ID:         " << marker_id << std::endl;
      oss << "Instance ID:       " << instance_id << std::endl;
      oss << "Assoc flags:       " << (int)association_flags << std::endl;
      oss << "Scope:             " << (int)scope << std::endl;
      oss << params_to_string();

      return oss.str();
    }

    uint16_t length;
    uint8_t version;
    uint8_t msg_type;
    SAS::TrailId trail;
    uint64_t timestamp;
    uint32_t marker_id;
    uint32_t instance_id;
    uint8_t association_flags;
    uint8_t scope;

    // Flags derived from the association_flags field. 
    bool associate;
    bool no_reactivate;
  };
}

#endif

