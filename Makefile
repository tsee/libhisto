.PHONY: all build test test-all test-doc-examples test-asan test-fuzz test-tsan test-msan test-valgrind memcheck clean format docs

BUILD_DIR ?= build

all: build

build:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

test: test-asan

test-all: test-asan test-doc-examples test-fuzz test-tsan memcheck docs
	@echo "======================================================================"
	@echo " ALL TEST SUITES, SANITIZERS (ASan, UBSan, TSan), DOC TESTS, MEMCHECK & DOCS PASSED"
	@echo "======================================================================"

test-doc-examples: build
	python3 tests/scripts/test_doc_examples.py --source-dir . --build-dir $(BUILD_DIR) --verbose


test-asan:
	cmake -B $(BUILD_DIR)-asan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_ASAN=ON -DLIBHISTO_ENABLE_FUZZING=ON
	cmake --build $(BUILD_DIR)-asan
	ctest --test-dir $(BUILD_DIR)-asan --output-on-failure

test-fuzz:
	cmake -B $(BUILD_DIR)-fuzz -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_ASAN=ON -DLIBHISTO_ENABLE_FUZZING=ON
	cmake --build $(BUILD_DIR)-fuzz
	ctest --test-dir $(BUILD_DIR)-fuzz -R "fuzz_" --output-on-failure

test-tsan:
	cmake -B $(BUILD_DIR)-tsan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_TSAN=ON
	cmake --build $(BUILD_DIR)-tsan
	ctest --test-dir $(BUILD_DIR)-tsan --output-on-failure

test-msan:
	cmake -B $(BUILD_DIR)-msan -S . -DCMAKE_BUILD_TYPE=Debug -DLIBHISTO_ENABLE_MSAN=ON
	cmake --build $(BUILD_DIR)-msan
	ctest --test-dir $(BUILD_DIR)-msan --output-on-failure

memcheck test-valgrind:
	cmake -B $(BUILD_DIR)-valgrind -S . -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)-valgrind
	cd $(BUILD_DIR)-valgrind && ctest -T memcheck --output-on-failure

docs:
	cmake -B $(BUILD_DIR) -S . -DLIBHISTO_BUILD_DOCS=ON
	cmake --build $(BUILD_DIR) --target docs

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR)-*
