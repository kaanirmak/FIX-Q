# ─────────────────────────────────────────────────────────────
# Finora Post-Quantum Cryptographic Framework Makefile
# Standards: NIST FIPS 203 (ML-KEM), NIST FIPS 204 (ML-DSA)
# ─────────────────────────────────────────────────────────────

CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./include
PRODFLAGS = -O3 -march=native -flto -DNDEBUG
LDFLAGS = -lcrypto -lssl

# OS Detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    SHLIB_EXT = dylib
    SHLIB_FLAGS = -dynamiclib -install_name @rpath/libfinora.$(SHLIB_EXT)
    CXXFLAGS += -I/opt/homebrew/opt/openssl@3/include
    LDFLAGS += -L/opt/homebrew/opt/openssl@3/lib
else
    SHLIB_EXT = so
    SHLIB_FLAGS = -shared -Wl,-soname,libfinora.$(SHLIB_EXT)
    CXXFLAGS += -I/usr/include/openssl -I/usr/local/include
    LDFLAGS += -L/usr/lib/x86_64-linux-gnu -L/usr/local/lib
endif

BIN_DIR = bin
LIB_DIR = lib

# Core Framework Targets
CORE_TARGETS = $(LIB_DIR)/libfinora.$(SHLIB_EXT) $(BIN_DIR)/finora_proxy $(BIN_DIR)/pqc_proxy
TOOL_TARGETS = $(BIN_DIR)/finora_pqc_tool $(BIN_DIR)/finora_tls_proxy $(BIN_DIR)/pqc_tool $(BIN_DIR)/tls_proxy
TEST_TARGETS = $(BIN_DIR)/test_nist_kat
EXAMPLE_TARGETS = $(BIN_DIR)/mock_exchange \
                  $(BIN_DIR)/mock_bist \
                  $(BIN_DIR)/benchmark \
                  $(BIN_DIR)/micro_bench \
                  $(BIN_DIR)/client_example_c \
                  $(BIN_DIR)/client_example_cpp

# Default target: build core library, proxy, and test suite
all: directories $(CORE_TARGETS) $(TEST_TARGETS)

# Production optimized build
prod: CXXFLAGS += $(PRODFLAGS)
prod: all

# Full build including tools and all examples
full: all tools examples

tools: directories $(TOOL_TARGETS)
examples: directories $(EXAMPLE_TARGETS)

directories:
	@mkdir -p $(BIN_DIR) $(LIB_DIR)

# ─────────────────────────────────────────────────────────────
# Core Library & Gateway
# ─────────────────────────────────────────────────────────────
$(LIB_DIR)/libfinora.$(SHLIB_EXT): src/finora_sdk.cpp
	$(CXX) $(CXXFLAGS) -fPIC $(SHLIB_FLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/finora_proxy: src/pqc_proxy.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# ─────────────────────────────────────────────────────────────
# Standalone CLI Tools
# ─────────────────────────────────────────────────────────────
$(BIN_DIR)/finora_pqc_tool: tools/pqc_tool.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/finora_tls_proxy: tools/tls_proxy.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# ─────────────────────────────────────────────────────────────
# Test Suite (NIST Known Answer Tests)
# ─────────────────────────────────────────────────────────────
$(BIN_DIR)/test_nist_kat: tests/test_nist_kat.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test: $(BIN_DIR)/test_nist_kat
	@echo "Running Finora NIST KAT Verification Test Suite..."
	@./$(BIN_DIR)/test_nist_kat

test_nist_kat: test

# ─────────────────────────────────────────────────────────────
# Examples & Benchmarks
# ─────────────────────────────────────────────────────────────
$(BIN_DIR)/mock_exchange: examples/mock_exchange/mock_bist.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/benchmark: examples/benchmarks/benchmark.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/micro_bench: examples/benchmarks/micro_bench.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/client_example_cpp: examples/minimal_client/client_example.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/client_example_c: examples/minimal_client/client_example.c $(LIB_DIR)/libfinora.$(SHLIB_EXT)
	$(CC) -Wall -Wextra -I./include $< -o $@ -L$(LIB_DIR) -lfinora -Wl,-rpath,@executable_path/../$(LIB_DIR)

# Compatibility Symlinks
$(BIN_DIR)/pqc_proxy: $(BIN_DIR)/finora_proxy
	@ln -sf finora_proxy $@

$(BIN_DIR)/mock_bist: $(BIN_DIR)/mock_exchange
	@ln -sf mock_exchange $@

$(BIN_DIR)/tls_proxy: $(BIN_DIR)/finora_tls_proxy
	@ln -sf finora_tls_proxy $@

$(BIN_DIR)/pqc_tool: $(BIN_DIR)/finora_pqc_tool
	@ln -sf finora_pqc_tool $@

# ─────────────────────────────────────────────────────────────
# Installation
# ─────────────────────────────────────────────────────────────
PREFIX ?= /usr/local

install: $(CORE_TARGETS)
	@mkdir -p $(DESTDIR)$(PREFIX)/include/finora
	@mkdir -p $(DESTDIR)$(PREFIX)/lib
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@mkdir -p $(DESTDIR)/etc/finora
	cp -r include/finora/* $(DESTDIR)$(PREFIX)/include/finora/
	cp $(LIB_DIR)/libfinora.$(SHLIB_EXT) $(DESTDIR)$(PREFIX)/lib/
	cp $(BIN_DIR)/finora_proxy $(DESTDIR)$(PREFIX)/bin/
	cp config/*.yaml $(DESTDIR)/etc/finora/
	@echo "Finora installed successfully to $(PREFIX)"

clean:
	rm -rf $(BIN_DIR) $(LIB_DIR)

.PHONY: all prod full tools examples directories test test_nist_kat install clean
