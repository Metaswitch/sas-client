/**
 * @file main_compress.cpp SAS client library compression test script.
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

#include "sas.h"
#include "sastestutil.h"

//
// Compression tests.
//
namespace CompressionTest
{

void test_hello_world()
{
  SAS::Event event(1, 2, 3);
  event.add_compressed_param("hello world\n");
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  unsigned char hello_world_compressed_bytes[] =
  {
    0x78, 0x9c, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x57,
    0x28, 0xcf, 0x2f, 0xca, 0x49, 0xe1, 0x02, 0x00,
    0x1e, 0x72, 0x04, 0x67
  };
  std::string hello_world_compressed((char*)hello_world_compressed_bytes,
                                     sizeof(hello_world_compressed_bytes));

  ASSERT_PRINT_BYTES(expected.static_params.empty(), bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == hello_world_compressed, expected.var_params[0]);
}

void test_dictionary()
{
  SAS::Profile profile("hello world");
  SAS::Event event(1, 2, 3);
  event.add_compressed_param("hello world\n", &profile);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  unsigned char dictionary_compressed_bytes[] =
  {
    0x78, 0xbb, 0x1a, 0x0b, 0x04, 0x5d, 0xcb, 0x40,
    0x30, 0xb9, 0x00, 0x1e, 0x72, 0x04, 0x67
  };
  std::string dictionary_compressed((char*)dictionary_compressed_bytes,
                                    sizeof(dictionary_compressed_bytes));

  ASSERT_PRINT_BYTES(expected.static_params.empty(), bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == dictionary_compressed, expected.var_params[0]);
}

void test_hello_world_lz4()
{
  SAS::Profile profile(SAS::Profile::CompressionType::LZ4);
  SAS::Event event(1, 2, 3);
  event.add_compressed_param("Test string.  Test string.\n", &profile);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  unsigned char hello_world_compressed_bytes[] =
  {
    0xe4, 0x54, 0x65, 0x73, 0x74, 0x20, 0x73, 0x74,
    0x72, 0x69, 0x6e, 0x67, 0x2e, 0x20, 0x20, 0x0e,
    0x00, 0x50, 0x69, 0x6e, 0x67, 0x2e, 0x0a
  };
  std::string hello_world_compressed((char*)hello_world_compressed_bytes,
                                     sizeof(hello_world_compressed_bytes));

  ASSERT_PRINT_BYTES(expected.static_params.empty(), bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == hello_world_compressed, expected.var_params[0]);
}

void test_dictionary_lz4()
{
  SAS::Profile profile("Test string.", SAS::Profile::CompressionType::LZ4);
  SAS::Event event(1, 2, 3);
  event.add_compressed_param("Test string.  Test string.\n", &profile);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  unsigned char dictionary_compressed_bytes[] =
  {
    0x08, 0x0c, 0x00, 0x24, 0x20, 0x20, 0x0e, 0x00,
    0x50, 0x69, 0x6e, 0x67, 0x2e, 0x0a
  };
  std::string dictionary_compressed((char*)dictionary_compressed_bytes,
                                    sizeof(dictionary_compressed_bytes));

  ASSERT_PRINT_BYTES(expected.static_params.empty(), bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == dictionary_compressed, expected.var_params[0]);
}

void test_empty()
{
  SAS::Event event(1, 2, 3);
  event.add_compressed_param("");
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  unsigned char empty_compressed_bytes[] =
  {
    0x78, 0x9c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01
  };
  std::string empty_compressed((char*)empty_compressed_bytes,
                               sizeof(empty_compressed_bytes));

  ASSERT_PRINT_BYTES(expected.static_params.empty(), bytes);
  ASSERT_PRINT_BYTES(expected.var_params.size() == 1, bytes);
  ASSERT_PRINT_BYTES(expected.var_params[0] == empty_compressed, expected.var_params[0]);
}
} // namespace CompressionTest

int main(int argc, char *argv[])
{
  RUN_TEST(CompressionTest::test_hello_world);
  RUN_TEST(CompressionTest::test_dictionary);
  RUN_TEST(CompressionTest::test_hello_world_lz4);
  RUN_TEST(CompressionTest::test_dictionary_lz4);
  RUN_TEST(CompressionTest::test_empty);

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

