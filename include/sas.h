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

// This is autogenerated by the sas-client configure script.
#include <config.h>

#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

#if HAVE_ATOMIC
  #include <atomic>
#elif HAVE_CSTDATOMIC
  #include <cstdatomic>
#else
  #error "Atomic types not supported"
#endif

// SAS Client library Version number
// format x.y.z
// The SAS Client library uses semantic versioning. This means:
// We use a three-part version number: <major version>.<minor version>.<patch>
// * The Patch number is incremented for bug fix releases.
// * The Minor version number is incremented for releases that add new API
//   features, but are fully backwards compatible.
// * The Major version number must be incremented for releases that break
//   backwards compatibility in any way.
#define SAS_CLIENT_VERSION = "1.0.0"

// Marker IDs
static const int MARKER_ID_PROTOCOL_ERROR       = 0x01000001;
static const int MARKER_ID_START                = 0x01000003;
static const int MARKER_ID_END                  = 0x01000004;
static const int MARKER_ID_DIALED_DIGITS        = 0x01000005;
static const int MARKER_ID_CALLING_DN           = 0x01000006;
static const int MARKER_ID_CALLED_DN            = 0x01000007;
static const int MARKER_ID_ICC_BRANCH_INDEX     = 0x01000010;

// Sometimes referred to as "subscriber number"
static const int MARKER_ID_PRIMARY_DEVICE       = 0x01000008;

static const int MARKER_ID_MVD_MOVABLE_BLOCK    = 0x01000015;
static const int MARKER_ID_GENERIC_CORRELATOR   = 0x01000016;
static const int MARKER_ID_FLUSH                = 0x01000017;

static const int MARKER_ID_SIP_REGISTRATION     = 0x010B0004;
static const int MARKER_ID_SIP_ALL_REGISTER     = 0x010B0005;
static const int MARKER_ID_SIP_SUBSCRIBE_NOTIFY = 0x010B0006;
static const int MARKER_ID_SIP_CALL_ID          = 0x010C0001;
static const int MARKER_ID_IMS_CHARGING_ID      = 0x010C0002;
static const int MARKER_ID_VIA_BRANCH_PARAM     = 0x010C0003;

static const int MARKER_ID_OUTBOUND_CALLING_URI = 0x05000003;
static const int MARKER_ID_INBOUND_CALLING_URI  = 0x05000004;
static const int MARKER_ID_OUTBOUND_CALLED_URI  = 0x05000005;
static const int MARKER_ID_INBOUND_CALLED_URI   = 0x05000006;

// SAS::init return codes
static const int SAS_INIT_RC_OK  = 0;
static const int SAS_INIT_RC_ERR = 1;

// Uniqueness scopes
enum struct UniquenessScopes
{
  DIAMETER_SID_RFC6733 = 1,
  UUID_RFC4122 = 2,
  ASYNC_CFG_SUB_DN = 3,
  DIGEST_OPAQUE = 4,
  STEERING_ID = 5
};

class SAS
{
public:
  typedef uint64_t TrailId;
  typedef uint64_t Timestamp;

  // Compression profile
  class Profile
  {
  public:
    enum Algorithm {
      ZLIB = 0,
      LZ4
    };

    Profile(std::string dictionary, Algorithm a = ZLIB):
      _dictionary(dictionary),_algorithm(a) {};
    Profile(Algorithm a):
      _dictionary(""),_algorithm(a) {};
    ~Profile() {};
    inline const std::string& get_dictionary() const {return _dictionary;}
    inline Algorithm get_algorithm() const {return _algorithm;}

  private:
    const std::string _dictionary;
    const Algorithm _algorithm;
  };

  class Compressor
  {
  public:
    virtual std::string compress(const std::string& s, const Profile* profile) = 0;
    static Compressor* get(Profile::Algorithm algorithm);

  protected:
    Compressor() {};
    virtual ~Compressor() {};
    static void destroy(void* compressor_ptr);
  };

  class Message
  {
  public:
    static const int MAX_NUM_STATIC_PARAMS = 20;
    static const int MAX_NUM_VAR_PARAMS = 20;

    inline Message(TrailId trail,
                   uint32_t id,
                   uint32_t instance=0u) :
      _trail(trail),
      _id(id),
      _instance(instance),
      _static_params(),
      _var_params()
    {
    }

    virtual ~Message()
    {
    }

    inline Message& add_static_param(uint32_t param)
    {
      _static_params.push_back(param);
      return *this;
    }

    inline Message& add_var_param(const std::string& s)
    {
      _var_params.push_back(s);
      return *this;
    }

    inline Message& add_var_param(size_t len, char* s)
    {
      std::string local_str(s, len);
      return add_var_param(local_str);
    }

    inline Message& add_var_param(size_t len, uint8_t* s)
    {
      std::string local_str((char *)s, len);
      return add_var_param(local_str);
    }

    inline Message& add_var_param(const char* s)
    {
      std::string local_str(s);
      return add_var_param(local_str);
    }

    inline Message& add_compressed_param(const std::string& s, const Profile* profile = NULL)
    {
      // Default compression is zlib with no dictionary
      Profile::Algorithm algorithm = Profile::Algorithm::ZLIB;

      // If a profile is provided, override those defaults
      if (profile != NULL)
      {
        algorithm = profile->get_algorithm();
      }

      Compressor* compressor = SAS::Compressor::get(algorithm);
      return add_var_param(compressor->compress(s, profile));
    }

    inline Message& add_compressed_param(size_t len, char* s, const Profile* profile = NULL)
    {
      std::string local_str(s, len);
      return add_compressed_param(local_str, profile);
    }

    inline Message& add_compressed_param(size_t len, uint8_t* s, const Profile* profile = NULL)
    {
      std::string local_str((char *)s, len);
      return add_compressed_param(local_str, profile);
    }

    inline Message& add_compressed_param(const char* s, const Profile* profile = NULL)
    {
      std::string local_str(s);
      return add_compressed_param(local_str, profile);
    }

    friend class SAS;

  protected:
    size_t params_buf_len() const;
    void write_params(std::string& s) const;

  private:
    TrailId _trail;
    uint32_t _id;
    uint32_t _instance;
    std::vector<uint32_t> _static_params;
    std::vector<std::string> _var_params;
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
    inline Event(TrailId trail, uint32_t event, uint32_t instance=0u) :
      Message(trail,
              ((event & 0x00FFFFFF) | 0x0F000000),
              instance),
      _timestamp(0),
      _timestamp_set(false)
    {
    }

    inline Event& set_timestamp(Timestamp timestamp)
    {
      _timestamp = timestamp;
      _timestamp_set = true;
      return *this;
    }

    Timestamp get_timestamp() const;

    std::string to_string() const;

  protected:
    Timestamp _timestamp;
    bool _timestamp_set;
  };

  class Analytics : public Message
  {
  public:

    enum Format
    {
      JSON = 1,
      XML = 2
    };

    inline Analytics(TrailId trail,
                     Format format,
                     const std::string& source_type,
                     const std::string& friendly_id,
                     uint32_t event_id,
                     uint32_t instance=0u) :
      Message(trail,
              ((event_id & 0x00FFFFFF) | 0x0F000000),
              instance),
      _format(format),
      _source_type(source_type),
      _friendly_id(friendly_id)
    {
    }

    Timestamp get_timestamp() const;
    std::string to_string(bool sas_store) const;

  private:
    size_t variable_header_buf_len() const;

    Format _format;
    std::string _source_type;
    std::string _friendly_id;
  };

  class Marker : public Message
  {
  public:
    inline Marker(TrailId trail, uint32_t marker, uint32_t instance=0u) :
      Message(trail, marker, instance)
    {
    }

    enum Scope
    {
      None = 0,
      Branch = 1,
      Trace = 2
    };

    Timestamp get_timestamp() const;

    std::string to_string(Scope scope, bool reactivate) const;
  };

  enum sas_log_level_t {
    SASCLIENT_LOG_CRITICAL=1,
    SASCLIENT_LOG_ERROR,
    SASCLIENT_LOG_WARNING,
    SASCLIENT_LOG_INFO,
    SASCLIENT_LOG_DEBUG,
    SASCLIENT_LOG_TRACE,
    SASCLIENT_LOG_STATS=12
  };


  typedef void (sas_log_callback_t)(sas_log_level_t level,
                                    int32_t log_id_len,
                                    unsigned char* log_id,
                                    int32_t sas_ip_len,
                                    unsigned char* sas_ip,
                                    int32_t msg_len,
                                    unsigned char* msg);

  // Optional callback, to create the SAS connection socket in some other way than the 'socket' call.
  //
  // For example, this allows callers to use socket control messages
  // (http://man7.org/linux/man-pages/man3/cmsg.3.html) to get a network socket with enhanced
  // privileges.
  //
  // If this callback isn't provided to SAS::init, we'll use SAS::Connection::get_local_sock by
  // default.
  typedef int (create_socket_callback_t)(const char* hostname,
                                         const char* port);

  /// Initialises the SAS client library.  This call must
  /// complete before any other functions on the API can be called.
  ///
  /// @param  system_name
  ///     The unique name for the system, eg: hostname
  /// @param  system_type
  ///     The type of this system
  ///
  /// @param  resource_identifier
  ///     The version of the resource bundle
  /// @param  sas_address
  ///     Takes a single ipv4 address or domain name.
  /// @param  log_callback
  ///     Optional Logging callback
  /// @param  socket_callback
  ///     Optional socket callback
  ///
  /// @returns
  ///     SAS_INIT_RC_OK    on success
  ///     SAS_INIT_RC_ERR   on failure
  ///
  static int init(std::string system_name,
                  const std::string& system_type,
                  const std::string& resource_identifier,
                  const std::string& sas_address,
                  sas_log_callback_t* log_callback,
                  create_socket_callback_t* socket_callback = NULL);

  /// Terminates the connection to the SAS Client Library.
  ///
  static void term();

  /// Request a new trail ID.
  ///
  /// @param instance
  ///    Can be used to identify a code location where a particular trail was created.
  ///
  static TrailId new_trail(uint32_t instance=0u);

  /// Send a SAS event.
  /// The contents of the supplied Event are unchanged, and the ownership
  /// remains with the calling code
  ///
  /// @param event
  ///    The pre-constructed Event to send
  ///
  static void report_event(const Event& event);

  /// Send a SAS analytics message.
  /// The contents of the supplied analytics message are unchanged, and the ownership
  /// remains with the calling code
  ///
  /// @param analytics
  ///    The pre-constructed analytics message to send
  /// @param sas_store
  ///    Specifies whether the message should be stored in the SAS database (as
  ///    an event) in addition to being forwarded to the analytics server.
  ///
  static void report_analytics(const Analytics& analytics,
                               bool sas_store = false);

  /// Send a SAS marker.
  /// The contents of the supplied marker are unchanged, and the ownership
  /// remains with the calling code
  ///
  /// @param marker
  ///    The pre-constructed Marker to send
  /// @param scope
  ///    The association scope.  One of: NONE, BRANCH, TRACE
  /// @param reactivate
  ///    Sets the association flag if true.
  ///    If two markers are reported with this association flag set, with the same
  ///    marker-specific data, on different trails, within 60s of one another, then this
  ///    will cause the two trails to become associated.
  ///
  static void report_marker(const Marker& marker,
                            Marker::Scope scope = Marker::Scope::None,
                            bool reactivate = true);

  /// Associate the two trails with the given IDs.
  ///
  /// @param trail_a
  ///      The first trail
  /// @param trail_b
  ///     The second trail
  /// @param scope
  ///     The association scope.  One of: NONE, BRANCH, TRACE
  ///
  static void associate_trails(TrailId trail_a,
                               TrailId trail_b,
                               Marker::Scope scope = Marker::Scope::Branch);

  static Timestamp get_current_timestamp();

  static sas_log_callback_t* _log_callback;



  /// Converts the format of a log raised within the SAS-Client to that expected by
  /// the common log callback, and calls the common log callback with the converted
  /// arguments.
  static void sasclient_log_callback(sas_log_level_t level,
                                     const char *module,
                                     int line_number,
                                     const char *fmt,
                                     ...);

  private:
  static void write_hdr(std::string& s,
                        uint16_t msg_length,
                        uint8_t msg_type,
                        Timestamp timestamp);
  static void write_int8(std::string& s, uint8_t c);
  static void write_int16(std::string& s, uint16_t v);
  static void write_int32(std::string& s, uint32_t v);
  static void write_int64(std::string& s, uint64_t v);
  static void write_data(std::string& s, size_t length, const char* data);
  static void write_timestamp(std::string& s);
  static void write_trail(std::string& s, TrailId trail);

  static std::string heartbeat_msg();

  static std::atomic<TrailId> _next_trail_id;
  class Connection;
  static Connection* _connection;
  static create_socket_callback_t* _socket_callback;
};

#endif
