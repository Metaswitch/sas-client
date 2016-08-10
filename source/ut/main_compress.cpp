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
#include "lz4.h"

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
  SAS::Profile profile(SAS::Profile::Algorithm::LZ4);
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
  SAS::Profile profile("Test string.", SAS::Profile::Algorithm::LZ4);
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

// Test that we can compress data larger than 4096 bytes (MAX_INPUT_SIZE).
void test_large_data()
{
  std::string lorem_ipsum = "Lorem ipsum dolor sit amet, soluta accusamus ei vix, quo an quas quidam scribentur, cu illud atqui mea. No mel adipisci repudiandae. Congue doctus cu vix, dolorum accusata id ius, ad dicat recusabo voluptatum his. Nam ludus choro ut, quo justo accommodare ea. Cu odio civibus dolores duo, et eos cibo reprimique, simul quaestio signiferumque ei pri.  Impedit consetetur te vel. Altera pericula ut his, ut cum offendit prodesset referrentur. Falli blandit te vel, sed te assum consectetuer vituperatoribus. Qui odio nobis labores ei. Eu iudico appareat eum, in sed causae definitionem, vocent tritani volutpat est eu.  Te vis atqui nominavi voluptatum. Quem alienum rationibus ad eum. Ignota laboramus honestatis ad quo. Vel eu virtute veritus, et brute euripidis ius. Tollit consequat scriptorem ad eam, in vix vituperata honestatis, no vitae recusabo sea.  Congue graeci laoreet et eam, quo ex modus suscipit contentiones, oratio accusamus pri et. Cu mea legendos molestiae prodesset. Ius ubique dolores conclusionemque at. An vel malis consequat. Mel an suas voluptua.  Nec te aliquam mandamus. Eum ut aliquam albucius, ad vis sonet intellegebat, no pro detracto fabellas. Vocent fuisset deleniti te quo, audiam contentiones mei ne. Ea tempor invenire quaerendum est, eu possim instructior eum. Ut aliquam evertitur tincidunt duo, impetus nostrud nam ei, eos affert oporteat persequeris in. Ne justo integre omittantur vel.  Per epicuri fastidii at, usu causae delicatissimi id. Ne ius consul repudiare. Dicat appetere ius eu, odio nobis oportere cu est. Ea natum numquam per, molestiae deseruisse vix ex, denique singulis has no. Ut novum adipiscing duo, nec cu ornatus appareat.  Ei placerat verterem tractatos eos. Cu doctus civibus has, eam volumus splendide at. Erant insolens invenire eu nec, augue scripta est ne. Wisi partem has in, civibus albucius pro ea, his stet postea no. Postea aliquip vituperata sed no, eos eu erat repudiare. Cu invenire salutatus deterruisset vis, sea ei nulla impetus, ea alii noster vivendo ius.  Et vel lorem graecis iudicabit. Ei mea utamur dolorem, tantas impedit vivendum in cum. Quis fuisset moderatius sit eu, vide meis assueverit sit te, quo errem impetus repudiandae te. Mel agam quas graecis ea.  Duo doming mollis latine et. Putant equidem inimicus mea et, te purto lorem vix, eum choro clita accusamus at. Eros alienum insolens ex eum, vitae equidem iudicabit te eam. Eam tale duis in. His no illud antiopam, erat sale te vix, agam impedit quo ei.  Ut mei albucius referrentur. Ea mel solum accusam, quodsi everti vulputate te mei. Tota putent nemore eam te, saepe fastidii ea cum. Legimus reprimique vel an, at qui detracto definiebas sadipscing, ut quod nostrud sadipscing nec. Vel id vidit fierent, ex sed tale platonem. Vel eu deleniti perfecto dissentiet, ut mei quodsi detracto, mea id novum suavitate. Ne viris postea dictas sed, erat iisque necessitatibus in has.  Mel ei iisque meliore, ne wisi tamquam est. At appareat scriptorem mea, inermis perpetua ne ius, id vitae eligendi ius. Soleat prodesset no vis, hinc munere appellantur eum te. Tale laoreet principes his an, instructior complectitur voluptatibus vel eu. Expetenda efficiendi per an. Essent expetendis quo ut, his sapientem ocurreret ei.  Per graece animal iudicabit et. Pri debet vituperata ne. Porro senserit sed ne, at has sint ocurreret. Vis populo essent expetenda an, graece aliquando ne usu, his ei noster omnium habemus.  Ex ius corrumpit expetendis. Ne his nullam evertitur eloquentiam. Ad vero eruditi cum, malis maiestatis et mei. Vim dicit deserunt definitionem ad, facete malorum molestiae in vim, cum te libris referrentur efficiantur.  No tation animal omnesque sea. Usu illud petentium in, blandit torquatos mnesarchum his at, eum an audire blandit. Mel illum ponderum prodesset an, qui ea summo postulant gubergren, est graeci ullamcorper te. Prima equidem deseruisse vim in, altera iuvaret nec no, ea putant nusquam corrumpit per. Liber scriptorem nam cu, mea quis corpora ea, sea vivendum indoctum forensibus te. Vix alia sapientem et, pri an esse mucius.  Movet vocent nostrum ex quo. His ne civibus ponderum quaestio, et habeo tation gubergren his. In modo everti qui. Probatus atomorum ne has, duo utinam mandamus voluptatibus eu, id option pertinax assueverit has.  Mei ad solet iudicabit. Mea ei minim harum iusto, eu nam affert dicunt philosophia. Homero cetero no quo, eum ad qualisque posidonium. Nec falli regione nonumes no, eu vim illud nominati. Facete nonumes eu mei, per ex labores expetenda. Eu harum epicuri atomorum sit, dicat latine inermis et mei.  Dolorem periculis efficiantur vim id. Ne omnis impetus ornatus nec. Nec in fuisset interesset, an eam tation legere. Ne mei dicit expetenda. Et mel affert prompta, zril altera bonorum vel an, ullum nonumy vituperata et eum.  At eum singulis accommodare, ius in sumo quot. Est ea harum patrioque comprehensam. Eros fierent qui an. Quo in mazim perpetua deterruisset, doming postulant constituto ex has, utinam consectetuer nam ex. Luptatum atomorum nec ex, lorem splendide repudiandae ei eos.  Fugit molestie eum an, odio eripuit mea id, sed ea choro nominati. Docendi evertitur eum id. Ei alii facete hendrerit mei, at quot graeco integre nam, in qui soluta contentiones. An eius suscipiantur per, nam in quaeque percipit, odio intellegam eum id. Zril accusam cu pro, cu mei sint nibh corpora.  Vis at ornatus nonumes sensibus, adipisci scribentur sit ut, quis molestie vim ei. Vel labores appetere repudiandae ea. Te discere omittam qui, regione omnesque sapientem his ne. Ut sea errem nostro labores.";

  SAS::Profile profile(SAS::Profile::Algorithm::LZ4);
  SAS::Event event(1, 2, 3);
  event.add_compressed_param(lorem_ipsum, &profile);
  std::string bytes = event.to_string();

  SasTest::Event expected;
  expected.parse(bytes);

  // Attempt to decompress the compressed var param.
  char decompressed_data[8192] = {0};
  int rc = LZ4_decompress_fast(expected.var_params[0].data(), decompressed_data, lorem_ipsum.size());

  // LZ4_decompress_fast returns the number of bytes if successful, or a negative number if unsuccessful.
  ASSERT(rc > 0);
  std::string decompressed_data_str = decompressed_data;

  // The decompressed data should be equal to the original data.
  ASSERT(lorem_ipsum == decompressed_data_str);
}


} // namespace CompressionTest

int main(int argc, char *argv[])
{
  RUN_TEST(CompressionTest::test_hello_world);
  RUN_TEST(CompressionTest::test_dictionary);
  RUN_TEST(CompressionTest::test_hello_world_lz4);
  RUN_TEST(CompressionTest::test_dictionary_lz4);
  RUN_TEST(CompressionTest::test_empty);
  RUN_TEST(CompressionTest::test_large_data);

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

