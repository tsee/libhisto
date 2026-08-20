import unittest
import os
import tempfile
import histo
import histo.cli


class TestCLI(unittest.TestCase):
    def test_version_subcommand(self):
        code, out, err = histo.cli.run("version")
        self.assertEqual(code, 0)
        self.assertIn("libhisto", out)


    def test_stats_and_plot_subcommand(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            h_path = os.path.join(tmpdir, "test.histo")
            h = histo.Histogram(bins=20, range=(0.0, 100.0))
            for i in range(10):
                h.fill(float(i * 10))
            h.to_file(h_path, format="binary")

            # histo stats
            code, out, err = histo.cli.run("stats", h_path)
            self.assertEqual(code, 0)
            self.assertIn("Entries", out)

            # histo plot --sparkline
            code, out, err = histo.cli.run("plot", "-S", h_path)
            self.assertEqual(code, 0)
            self.assertGreater(len(out), 0)


if __name__ == "__main__":
    unittest.main()
