CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I/opt/homebrew/opt/openssl@3/include -I./include
PRODFLAGS = -O3 -march=native -flto -DNDEBUG
LDFLAGS = -L/opt/homebrew/opt/openssl@3/lib -lcrypto -lssl

BIN_DIR = bin
SRC_DIR = src

TARGETS = $(BIN_DIR)/benchmark $(BIN_DIR)/web_server $(BIN_DIR)/mock_bist $(BIN_DIR)/pqc_proxy $(BIN_DIR)/tls_proxy $(BIN_DIR)/micro_bench

all: $(TARGETS)

prod: CXXFLAGS += $(PRODFLAGS)
prod: all

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/benchmark: $(SRC_DIR)/benchmark.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/web_server: $(SRC_DIR)/web_server.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/mock_bist: $(SRC_DIR)/mock_bist.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/pqc_proxy: $(SRC_DIR)/pqc_proxy.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/tls_proxy: $(SRC_DIR)/tls_proxy.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/micro_bench: $(SRC_DIR)/micro_bench.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all prod clean
