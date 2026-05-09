CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I.
LDFLAGS  =

# Source files
MAIN_SRC      = main.cpp
RUNNER_SRC    = test_runner.cpp

# Output binaries
MAIN_BIN      = nanodb
RUNNER_BIN    = test_runner

# Header-only project — no separate .cpp compilation needed
all: $(MAIN_BIN) $(RUNNER_BIN)
	@echo ""
	@echo "✓ Build complete."
	@echo "  Run: ./nanodb            — initialise database"
	@echo "  Run: ./test_runner       — run all 7 demo test cases"
	@echo ""

$(MAIN_BIN): $(MAIN_SRC) include/*.h
	$(CXX) $(CXXFLAGS) -o $@ $(MAIN_SRC) $(LDFLAGS)

$(RUNNER_BIN): $(RUNNER_SRC) include/*.h
	$(CXX) $(CXXFLAGS) -o $@ $(RUNNER_SRC) $(LDFLAGS)

# Run data generation then tests
run: all
	./nanodb --generate
	./test_runner

# Run test_runner only (assumes data exists)
test: $(RUNNER_BIN)
	./test_runner

# Check for memory leaks with Valgrind
valgrind: $(RUNNER_BIN)
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes --verbose \
	         ./test_runner 2>&1 | tee logs/valgrind.log

# Clean build artifacts (NOT data files)
clean:
	rm -f $(MAIN_BIN) $(RUNNER_BIN)
	@echo "Cleaned binaries."

# Clean everything including data
distclean: clean
	rm -f data/*.db logs/*.log
	@echo "Cleaned all generated files."

.PHONY: all run test valgrind clean distclean
