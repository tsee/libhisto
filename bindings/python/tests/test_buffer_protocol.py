import unittest
import array
import struct
import histo


class TestBufferProtocol(unittest.TestCase):
    def test_1d_buffer_ingestion(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0))

        # 1. array.array('d')
        arr = array.array('d', [10.0, 20.0, 30.0, 40.0, 50.0])
        self.assertTrue(h.fill_buffer(arr))
        self.assertEqual(h.num_entries, 5)

        # 2. memoryview
        mv = memoryview(arr)
        self.assertTrue(h.fill_buffer(mv))
        self.assertEqual(h.num_entries, 10)

        # 3. raw packed bytes
        raw_bytes = struct.pack("4d", 15.0, 25.0, 35.0, 45.0)
        self.assertTrue(h.fill_buffer(raw_bytes))
        self.assertEqual(h.num_entries, 14)

    def test_2d_buffer_ingestion(self):
        h2 = histo.Histogram2D(xbins=10, xrange=(0.0, 10.0), ybins=10, yrange=(0.0, 10.0))
        x_arr = array.array('d', [1.0, 2.0, 3.0])
        y_arr = array.array('d', [4.0, 5.0, 6.0])

        self.assertTrue(h2.fill_buffer(x_arr, y_arr))
        self.assertEqual(h2.num_entries, 3)

    def test_sketch_buffer_ingestion(self):
        s = histo.Sketch(alpha=0.01)
        arr = array.array('d', [float(x) for x in range(1, 501)])
        self.assertTrue(s.insert_buffer(arr))
        self.assertEqual(s.num_entries, 500)


if __name__ == "__main__":
    unittest.main()
