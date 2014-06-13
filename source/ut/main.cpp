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

// Exception class used to signal a test failure.
class TestError {};

// The number of test failures we've hit.
static int failures = 0;

// Fail a test if a condition is not true.
#define ASSERT(COND)                                                           \
  if (!(COND))                                                                 \
  {                                                                            \
    std::cout << "ASSERT failed: '" << #COND << "' at "                        \
              << __FILE__ << ":" << __LINE__ << std::endl;                     \
    TestError err; throw err;                                                  \
  }

// Fail a test if a condition is not true, and dump out a string as hex.
// This is useful when testing messages build by the client library.
#define ASSERT_PRINT_BYTES(COND, STR) \
  if (!(COND))                                                                 \
  {                                                                            \
    std::cout << "ASSERT failed: '" << #COND << "' at "                        \
              << __FILE__ << ":" << __LINE__ << std::endl;                     \
    std::cout << str_dump_hex(STR) << std::endl;                               \
    TestError err; throw err;                                                  \
  }

// Run a test, incremening the failures count if it does not succeed.
#define RUN_TEST(NAME)                                                         \
{                                                                              \
  try                                                                          \
  {                                                                            \
    std::cout << "Running " #NAME << "..." << std::endl;                       \
    NAME();                                                                    \
    std::cout << #NAME " PASSED" << std::endl;                                 \
  }                                                                            \
  catch (TestError)                                                            \
  {                                                                            \
    std::cout << #NAME " FAILED" << std::endl;                                 \
    failures++;                                                                \
  }                                                                            \
}

// Utility method to format a string as a hex dump.
//
// Returns a string like this:
//
// Offset: |  0|  1|  2|  3|  4
//         --------------------
// Data:   | ff| ee| aa| 06| 54
std::string str_dump_hex(const std::string& s)
{
  std::ostringstream oss;
  oss << "Offset: ";
  for (size_t i = 0; i < s.length(); ++i)
  {
    oss << "|" << std::setw(3) << i;
  }
  oss << std::endl;

  std::string divider;
  divider.assign(s.length()*4, '-');
  oss << "        " << divider << std::endl;

  oss << "Data:   ";
  for (size_t i = 0; i < s.length(); ++i)
  {
    oss << "| " << std::hex << std::setw(2) << std::setfill('0')
        << (unsigned int)SasTest::to_byte(s[i]);
  }

  return oss.str();
}

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

} // namespace MarkerTest

int main(int argc, char *argv[])
{
  RUN_TEST(EventTest::test_empty);
  RUN_TEST(EventTest::test_one_static_param);
  RUN_TEST(EventTest::test_two_static_params);
  RUN_TEST(EventTest::test_one_var_param);
  RUN_TEST(EventTest::test_two_var_params);
  RUN_TEST(EventTest::test_var_then_static);
  RUN_TEST(EventTest::test_static_then_var);

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

