// Unit tests for binary wire serialization and deserialization in C++.

#include <vector>
#include "histo/histogram.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(SerializationTest, UniformRoundtrip) {
  Histogram h(20, -50.0, 50.0, HISTO_FLAG_EXACT_MOMENTS | HISTO_FLAG_TRACK_SUMW2);
  h.Fill(-12.5);
  h.Fill(33.3, 2.0);
  h.Fill(0.0);

  // Serialize to byte vector
  auto bytes_res = h.Serialize();
  EXPECT_TRUE(bytes_res.ok());
  EXPECT_TRUE(bytes_res->size() > 0);

  // Deserialize
  auto h_deser_res = Histogram::Deserialize(*bytes_res);
  EXPECT_TRUE(h_deser_res.ok());

  const Histogram& h2 = *h_deser_res;
  EXPECT_EQ(h2.nbins(), 20u);
  EXPECT_NEAR(h2.min(), -50.0, 1e-9);
  EXPECT_NEAR(h2.max(), 50.0, 1e-9);
  EXPECT_EQ(h2.n_entries(), 3u);
  EXPECT_NEAR(h2.total_weight(), 4.0, 1e-9);
  EXPECT_NEAR(h2.mean(), h.mean(), 1e-9);
}

HISTO_TEST(SerializationTest, CorruptedBufferSafe) {
  std::vector<uint8_t> bad_bytes = {0x00, 0x01, 0x02, 0x03};
  auto h_res = Histogram::Deserialize(bad_bytes);
  EXPECT_FALSE(h_res.ok());
}
