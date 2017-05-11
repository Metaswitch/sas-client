/**
 * @file main.cpp SAS client library test script.
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

#include "sas.h"
#include "sastestutil.h"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

//
// Event tests.
//
namespace EventTest
{

void test_empty()
{
  SAS::Event event(111, 222, 333);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.version == 3, bytes);
  ASSERT_PRINT_BYTES(expected.msg_type == 3, bytes); // 3 => Event
  ASSERT_PRINT_BYTES(expected.trail == 111, bytes);
  // The library sets the top bytes to 0x0F
  ASSERT_PRINT_BYTES(expected.event_id == (0x0F000000 + 222), bytes);
  ASSERT_PRINT_BYTES(expected.instance_id == 333, bytes);
  ASSERT_PRINT_BYTES(expected.static_params.empty(), bytes);
  ASSERT_PRINT_BYTES(expected.var_params.empty(), bytes);
}

void test_one_static_param()
{
  SAS::Event event(111, 222, 333);
  event.add_static_param(1000);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[0] == 1000, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 0, bytes);
}

void test_two_static_params()
{
  SAS::Event event(111, 222, 333);
  event.add_static_param(1000);
  event.add_static_param(2000);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 2, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[0] == 1000, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[1] == 2000, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 0, bytes);
}

void test_one_var_param()
{
  SAS::Event event(111, 222, 333);
  event.add_var_param("hello");
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 0, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "hello", bytes);
}

void test_two_var_params()
{
  SAS::Event event(111, 222, 333);
  event.add_var_param("hello");
  event.add_var_param("world");
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 0, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 2, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "hello", bytes);
  ASSERT_PRINT_BYTES(expected.var_params[1] == "world", bytes);
}

void test_static_then_var()
{
  SAS::Event event(111, 222, 333);
  event.add_static_param(1000);
  event.add_var_param("hello");
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[0] == 1000, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "hello", bytes);
}

void test_var_then_static()
{
  SAS::Event event(111, 222, 333);
  event.add_var_param("hello");
  event.add_static_param(1000);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[0] == 1000, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "hello", bytes);
}

void test_timestamps_default_to_current_time()
{
  SAS::Event event(111, 222, 333);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  // Check the timestamp is approximately equal to the current time. Allow 5s
  // either way in case we are running slowly (under Valgrind for example).
  SAS::Timestamp ts = (time(NULL) * 1000);
  ASSERT_PRINT_BYTES(expected.timestamp > (ts - 5000), bytes);
  ASSERT_PRINT_BYTES(expected.timestamp < (ts + 5000), bytes);
}

void test_timestamps_can_be_overriden()
{
  SAS::Event event(111, 222, 333);
  event.set_timestamp(444);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.timestamp == 444, bytes);
}
} // namespace EventTest

//
// Marker tests
//

namespace MarkerTest
{

void test_empty()
{
  SAS::Marker marker(111, 222, 333);
  std::string bytes = marker.to_string(SAS::Marker::None, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.version == 3, bytes);
  ASSERT_PRINT_BYTES(expected.msg_type == 4, bytes); // 4 => Marker
  ASSERT_PRINT_BYTES(expected.trail == 111, bytes);
  ASSERT_PRINT_BYTES(expected.marker_id == 222, bytes);
  ASSERT_PRINT_BYTES(expected.instance_id == 333, bytes);
  ASSERT_PRINT_BYTES(!expected.associate, bytes);
  ASSERT_PRINT_BYTES(!expected.no_reactivate, bytes);
  ASSERT_PRINT_BYTES(expected.scope == 0, bytes);
  ASSERT_PRINT_BYTES(expected.static_params.empty(), bytes);
  ASSERT_PRINT_BYTES(expected.var_params.empty(), bytes);
}

void test_branch_scope_correlator()
{
  SAS::Marker marker(111, 222, 333);
  std::string bytes = marker.to_string(SAS::Marker::Branch, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.associate, bytes);
  ASSERT_PRINT_BYTES(!expected.no_reactivate, bytes);
  ASSERT_PRINT_BYTES(expected.scope == 1, bytes);
}

void test_trace_scope_correlator()
{
  SAS::Marker marker(111, 222, 333);
  std::string bytes = marker.to_string(SAS::Marker::Trace, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.associate, bytes);
  ASSERT_PRINT_BYTES(!expected.no_reactivate, bytes);
  ASSERT_PRINT_BYTES(expected.scope == 2, bytes);
}

void test_one_static_param()
{
  SAS::Marker marker(111, 222, 333);
  marker.add_static_param(1000);
  std::string bytes = marker.to_string(SAS::Marker::None, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[0] == 1000, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 0, bytes);
}

void test_two_static_params()
{
  SAS::Marker marker(111, 222, 333);
  marker.add_static_param(1000);
  marker.add_static_param(2000);
  std::string bytes = marker.to_string(SAS::Marker::None, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 2, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[0] == 1000, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[1] == 2000, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 0, bytes);
}

void test_one_var_param()
{
  SAS::Marker marker(111, 222, 333);
  marker.add_var_param("hello");
  std::string bytes = marker.to_string(SAS::Marker::None, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 0, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "hello", bytes);
}

void test_two_var_params()
{
  SAS::Marker marker(111, 222, 333);
  marker.add_var_param("hello");
  marker.add_var_param("world");
  std::string bytes = marker.to_string(SAS::Marker::None, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 0, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 2, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "hello", bytes);
  ASSERT_PRINT_BYTES(expected.var_params[1] == "world", bytes);
}

void test_static_then_var()
{
  SAS::Marker marker(111, 222, 333);
  marker.add_static_param(1000);
  marker.add_var_param("hello");
  std::string bytes = marker.to_string(SAS::Marker::None, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[0] == 1000, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "hello", bytes);
}

void test_var_then_static()
{
  SAS::Marker marker(111, 222, 333);
  marker.add_var_param("hello");
  marker.add_static_param(1000);
  std::string bytes = marker.to_string(SAS::Marker::None, true);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.static_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.static_params[0] == 1000, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "hello", bytes);
}

void test_no_reactivate_flag()
{
  SAS::Marker marker(111, 222, 333);
  std::string bytes = marker.to_string(SAS::Marker::Trace, false);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(expected.associate, bytes);
  ASSERT_PRINT_BYTES(expected.no_reactivate, bytes);
}

void test_no_reactivate_not_set_for_non_correlating_marker()
{
  SAS::Marker marker(111, 222, 333);
  std::string bytes = marker.to_string(SAS::Marker::None, false);

  SasTest::Marker expected;
  expected.parse(bytes);

  ASSERT_PRINT_BYTES(!expected.associate, bytes);
  ASSERT_PRINT_BYTES(!expected.no_reactivate, bytes);
}

void test_timestamps_use_current_time()
{
  SAS::Marker marker(111, 222, 333);
  std::string bytes = marker.to_string(SAS::Marker::None, false);

  SasTest::Marker expected;
  expected.parse(bytes);

  // Check the timestamp is approximately equal to the current time. Allow 5s
  // either way in case we are running slowly (under Valgrind for example).
  SAS::Timestamp ts = (time(NULL) * 1000);
  ASSERT_PRINT_BYTES(expected.timestamp > (ts - 5000), bytes);
  ASSERT_PRINT_BYTES(expected.timestamp < (ts + 5000), bytes);
}
} // namespace MarkerTest

// Analytics event tests
namespace AnalyticsTest
{

void test_json_no_store()
{
  SAS::Analytics analytics(111,
                           SAS::Analytics::Format::JSON,
                           "Test source",
                           "Test Friendly ID",
                           222);
  analytics.add_var_param("{\"JSON formated data\"}");
  std::string bytes = analytics.to_string(false);

  SasTest::Analytics expected;
  expected.parse(bytes);
  ASSERT_PRINT_BYTES(expected.version == 3, bytes);
  ASSERT_PRINT_BYTES(expected.msg_type == 7, bytes); // 7 => Analytics
  ASSERT_PRINT_BYTES(expected.trail == 111, bytes);
  // The library sets the top bytes to 0x0F
  ASSERT_PRINT_BYTES(expected.event_id == (0x0F000000 + 222), bytes);
  ASSERT_PRINT_BYTES(expected.instance_id == 0, bytes); // defaults to 0
  ASSERT_PRINT_BYTES(expected.format_type == 1, bytes); // 1 => JSON format
  ASSERT_PRINT_BYTES(expected.store_msg == 0, bytes);
  ASSERT_PRINT_BYTES(expected.source_type == "Test source", bytes);
  ASSERT_PRINT_BYTES(expected.friendly_id == "Test Friendly ID", bytes);
  ASSERT_PRINT_BYTES(expected.static_params.size() == 0, bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "{\"JSON formated data\"}", bytes);
}

void test_xml_with_store()
{
  SAS::Analytics analytics(111,
                           SAS::Analytics::Format::XML,
                           "Test source",
                           "Test Friendly ID",
                           222,
                           333);
  analytics.add_var_param("<data>XML format</data>");
  std::string bytes = analytics.to_string(true);

  SasTest::Analytics expected;
  expected.parse(bytes);
  ASSERT_PRINT_BYTES(expected.version == 3, bytes);
  ASSERT_PRINT_BYTES(expected.msg_type == 7, bytes); // 7 => Analytics
  ASSERT_PRINT_BYTES(expected.trail == 111, bytes);
  // The library sets the top bytes to 0x0F
  ASSERT_PRINT_BYTES(expected.event_id == (0x0F000000 + 222), bytes);
  ASSERT_PRINT_BYTES(expected.instance_id == 333, bytes);
  ASSERT_PRINT_BYTES(expected.format_type == 2, bytes); // 2 => XML format
  ASSERT_PRINT_BYTES(expected.store_msg == 1, bytes);
  ASSERT_PRINT_BYTES(expected.source_type == "Test source", bytes);
  ASSERT_PRINT_BYTES(expected.friendly_id == "Test Friendly ID", bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == "<data>XML format</data>", bytes);
}
} // namespace AnalyticsTest


namespace InitTest {

void test_initialization()
{
  SAS::init("Sprout",
            "Sprout",
            "org.projectclearwater",
            "127.0.0.1|127.0.0.2",
            SAS::log_to_stdout);
  sleep(60);
  SAS::term();
}


}

int main(int argc, char *argv[])
{
  RUN_TEST(EventTest::test_empty);
  RUN_TEST(EventTest::test_one_static_param);
  RUN_TEST(EventTest::test_two_static_params);
  RUN_TEST(EventTest::test_one_var_param);
  RUN_TEST(EventTest::test_two_var_params);
  RUN_TEST(EventTest::test_var_then_static);
  RUN_TEST(EventTest::test_static_then_var);
  RUN_TEST(EventTest::test_timestamps_default_to_current_time);
  RUN_TEST(EventTest::test_timestamps_can_be_overriden);

  RUN_TEST(MarkerTest::test_empty);
  RUN_TEST(MarkerTest::test_branch_scope_correlator);
  RUN_TEST(MarkerTest::test_trace_scope_correlator);
  RUN_TEST(MarkerTest::test_one_static_param);
  RUN_TEST(MarkerTest::test_two_static_params);
  RUN_TEST(MarkerTest::test_one_var_param);
  RUN_TEST(MarkerTest::test_two_var_params);
  RUN_TEST(MarkerTest::test_var_then_static);
  RUN_TEST(MarkerTest::test_static_then_var);
  RUN_TEST(MarkerTest::test_no_reactivate_flag);
  RUN_TEST(MarkerTest::test_no_reactivate_not_set_for_non_correlating_marker);
  RUN_TEST(MarkerTest::test_timestamps_use_current_time);

  RUN_TEST(AnalyticsTest::test_json_no_store);
  RUN_TEST(AnalyticsTest::test_xml_with_store);

  RUN_TEST(InitTest::test_initialization);

  if (failures == 0)
  {
    std::cout << std::endl << "All tests passed" << std::endl;
  }
  else
  {
    std::cout << std::endl << failures << " tests failed" << std::endl;
  }

  return (failures == 0 ? 0 : 1);
}

