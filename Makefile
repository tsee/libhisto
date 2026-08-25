.PHONY: all build test test-all test-doc-examples test-perl-alien test-perl-histo test-perl-pdl test-perl test-perl-dist perl-alien-dist perl-histo-dist perl-pdl-dist perl-dist test-python python-dist test-python-dist test-node build-node test-asan test-fuzz test-tsan test-msan test-valgrind memcheck clean format docs

BUILD_DIR ?= build
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

all: build

build:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(JOBS)

test: test-asan

test-all: test-asan test-doc-examples test-perl-alien test-perl-histo test-perl-pdl test-perl-dist test-python test-python-dist test-node test-fuzz test-tsan memcheck docs
	@echo "======================================================================"
	@echo " ALL TEST SUITES, SANITIZERS (ASan, UBSan, TSan), DOC TESTS, PERL, PYTHON & NODE BINDINGS & DISTRIBUTIONS, MEMCHECK & DOCS PASSED"
	@echo "======================================================================"

test-doc-examples: build
	python3 tests/scripts/test_doc_examples.py --source-dir . --build-dir $(BUILD_DIR) -j $(JOBS) --verbose

test-perl-alien:
	cd bindings/perl/Alien-libhisto && perl Makefile.PL && $(MAKE) test

perl-alien-dist:
	cd bindings/perl/Alien-libhisto && perl Makefile.PL && perl -MExtUtils::Manifest=mkmanifest -e mkmanifest && $(MAKE) dist

test-perl-histo: test-perl-alien
	cd bindings/perl/Math-Histo && perl Makefile.PL && $(MAKE) test

perl-histo-dist:
	cd bindings/perl/Math-Histo && perl Makefile.PL && perl -MExtUtils::Manifest=mkmanifest -e mkmanifest && $(MAKE) dist

test-perl-pdl: test-perl-histo
	cd bindings/perl/Math-Histo-PDL && perl Makefile.PL && $(MAKE) test

perl-pdl-dist:
	cd bindings/perl/Math-Histo-PDL && perl Makefile.PL && perl -MExtUtils::Manifest=mkmanifest -e mkmanifest && $(MAKE) dist

test-perl: test-perl-alien test-perl-histo test-perl-pdl

test-perl-dist:
	perl tests/scripts/test_perl_dist.pl

perl-dist: perl-alien-dist perl-histo-dist perl-pdl-dist

test-python:
	cd bindings/python && python3 setup.py build_ext --inplace && PYTHONPATH=. python3 -m unittest discover -s tests -v

python-dist:
	cd bindings/python && python3 setup.py sdist

test-python-dist:
	python3 tests/scripts/test_python_dist.py

build-node:
	cd bindings/node && node-gyp rebuild

test-node: build-node
	cd bindings/node && npm test




test-asan:
	cmake -B $(BUILD_DIR)-asan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_ASAN=ON -DLIBHISTO_ENABLE_FUZZING=ON
	cmake --build $(BUILD_DIR)-asan --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR)-asan -j$(JOBS) --output-on-failure

test-fuzz:
	cmake -B $(BUILD_DIR)-fuzz -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_ASAN=ON -DLIBHISTO_ENABLE_FUZZING=ON
	cmake --build $(BUILD_DIR)-fuzz --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR)-fuzz -R "fuzz_" -j$(JOBS) --output-on-failure

test-tsan:
	cmake -B $(BUILD_DIR)-tsan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_TSAN=ON
	cmake --build $(BUILD_DIR)-tsan --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR)-tsan -j$(JOBS) --output-on-failure

test-msan:
	cmake -B $(BUILD_DIR)-msan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_MSAN=ON
	cmake --build $(BUILD_DIR)-msan --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR)-msan -j$(JOBS) --output-on-failure

memcheck test-valgrind:
	cmake -B $(BUILD_DIR)-valgrind -S . -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)-valgrind --parallel $(JOBS)
	cd $(BUILD_DIR)-valgrind && ctest -T memcheck -E "test_doc_examples|test_perl_dist|test_python|test_python_dist" --output-on-failure



docs:
	cmake -B $(BUILD_DIR) -S . -DLIBHISTO_BUILD_DOCS=ON
	cmake --build $(BUILD_DIR) --target docs

check-versions:
	python3 tools/scripts/bump_version.py --check

bump-version:
	@if [ -z "$(VERSION)" ]; then echo "Usage: make bump-version VERSION=X.Y.Z"; exit 1; fi
	python3 tools/scripts/bump_version.py --set $(VERSION)

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR)-*

