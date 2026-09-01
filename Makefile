.PHONY: all build test test-cpp test-all test-doc-examples \
        bindings bindings-test bindings-dist \
        build-perl-alien build-perl-histo build-perl-pdl build-perl \
        test-perl-alien test-perl-histo test-perl-pdl test-perl \
        perl-alien-dist perl-histo-dist perl-pdl-dist perl-dist test-perl-dist \
        build-python test-python python-dist test-python-dist \
        build-node test-node \
        test-asan test-fuzz test-tsan test-msan test-valgrind memcheck clean format docs \
        test-musl test-big-endian test-32bit test-32bit-native test-arm64 test-armv7 test-riscv64 \
        test-matrix-local portability test-portability

BUILD_DIR ?= build
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# ======================================================================
# Default Core C Library & CLI Targets
# ======================================================================
all: build

build:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(JOBS)

test: test-asan

test-cpp:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR) -R "test_histo_cpp" -j$(JOBS) --output-on-failure

test-all: test-asan test-cpp test-doc-examples test-perl test-perl-dist test-python test-python-dist test-node test-fuzz test-tsan memcheck docs
	@echo "======================================================================"
	@echo " ALL TEST SUITES, SANITIZERS (ASan, UBSan, TSan), DOC TESTS, C++, PERL, PYTHON & NODE BINDINGS & DISTRIBUTIONS, MEMCHECK & DOCS PASSED"
	@echo "======================================================================"

test-doc-examples: build
	python3 tests/scripts/test_doc_examples.py --source-dir . --build-dir $(BUILD_DIR) -j $(JOBS) --verbose

# ======================================================================
# Language Bindings: Summary Targets
# ======================================================================
bindings: build-python build-perl build-node

bindings-test: test-python test-perl test-node

bindings-dist: python-dist perl-dist

# ======================================================================
# Perl Bindings (Alien-libhisto, Math-Histo, Math-Histo-PDL)
# ======================================================================
build-perl-alien:
	cd bindings/perl/Alien-libhisto && perl Makefile.PL && $(MAKE)

test-perl-alien: build-perl-alien
	cd bindings/perl/Alien-libhisto && $(MAKE) test

perl-alien-dist:
	cd bindings/perl/Alien-libhisto && perl Makefile.PL && perl -MExtUtils::Manifest=mkmanifest -e mkmanifest && $(MAKE) dist

build-perl-histo: build-perl-alien
	cd bindings/perl/Math-Histo && perl Makefile.PL && $(MAKE)

test-perl-histo: build-perl-histo
	cd bindings/perl/Math-Histo && $(MAKE) test

perl-histo-dist:
	cd bindings/perl/Math-Histo && perl Makefile.PL && perl -MExtUtils::Manifest=mkmanifest -e mkmanifest && $(MAKE) dist

build-perl-pdl: build-perl-histo
	cd bindings/perl/Math-Histo-PDL && perl Makefile.PL && $(MAKE)

test-perl-pdl: build-perl-pdl
	cd bindings/perl/Math-Histo-PDL && $(MAKE) test

perl-pdl-dist:
	cd bindings/perl/Math-Histo-PDL && perl Makefile.PL && perl -MExtUtils::Manifest=mkmanifest -e mkmanifest && $(MAKE) dist

build-perl: build-perl-alien build-perl-histo build-perl-pdl

test-perl: test-perl-alien test-perl-histo test-perl-pdl

test-perl-dist:
	perl tests/scripts/test_perl_dist.pl

perl-dist: perl-alien-dist perl-histo-dist perl-pdl-dist

# ======================================================================
# Python Bindings (histo C-extension, UHI, SciPy & Boost converters)
# ======================================================================
build-python:
	cd bindings/python && python3 setup.py build_ext --inplace

test-python: build-python
	cd bindings/python && PYTHONPATH=. python3 -m unittest discover -s tests -v

python-dist:
	cd bindings/python && python3 setup.py sdist

test-python-dist:
	python3 tests/scripts/test_python_dist.py

# ======================================================================
# Node.js / TypeScript Native Addon (N-API)
# ======================================================================
build-node:
	cd bindings/node && node-gyp rebuild

test-node: build-node
	cd bindings/node && npm test




asan test-asan:
	cmake -B $(BUILD_DIR)-asan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_ASAN=ON -DLIBHISTO_ENABLE_FUZZING=ON
	cmake --build $(BUILD_DIR)-asan --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR)-asan -j$(JOBS) --output-on-failure

fuzz test-fuzz:
	cmake -B $(BUILD_DIR)-fuzz -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_ASAN=ON -DLIBHISTO_ENABLE_FUZZING=ON
	cmake --build $(BUILD_DIR)-fuzz --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR)-fuzz -R "fuzz_" -j$(JOBS) --output-on-failure

tsan test-tsan:
	cmake -B $(BUILD_DIR)-tsan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_TSAN=ON
	cmake --build $(BUILD_DIR)-tsan --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR)-tsan -j$(JOBS) --output-on-failure

msan test-msan:
	CC=clang cmake -B $(BUILD_DIR)-msan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_MSAN=ON
	cmake --build $(BUILD_DIR)-msan --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR)-msan -j$(JOBS) --output-on-failure

valgrind memcheck test-valgrind:
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

# ======================================================================
# Multi-Architecture & Portability Test Targets
# ======================================================================
test-musl:
	python3 tools/scripts/test_container.py --target musl -j $(JOBS)

test-big-endian:
	python3 tools/scripts/test_container.py --target s390x -j $(JOBS)

test-32bit:
	python3 tools/scripts/test_container.py --target i386 -j $(JOBS)

test-32bit-native:
	python3 tools/scripts/test_container.py --target native-32bit -j $(JOBS)

test-arm64:
	python3 tools/scripts/test_container.py --target arm64 -j $(JOBS)

test-armv7:
	python3 tools/scripts/test_container.py --target armv7 -j $(JOBS)

test-riscv64:
	python3 tools/scripts/test_container.py --target riscv64 -j $(JOBS)

test-matrix-local:
	python3 tools/scripts/test_container.py --target all -j $(JOBS)

portability test-portability: test-all
	@echo "======================================================================"
	@echo " RUNNING FULL MULTI-ARCHITECTURE CONTAINER PORTABILITY MATRIX"
	@echo "======================================================================"
	python3 tools/scripts/test_container.py --target all --full -j $(JOBS)

clean:
	-rm -rf $(BUILD_DIR) $(BUILD_DIR)-* 2>/dev/null
	python3 tools/scripts/test_container.py --clean 2>/dev/null || true

