BUILD_DIR = build
SRC_DIR = src
INCLUDE_DIR = include
MONITORING_DIR = monitoring
MONITORING_BIN = $(MONITORING_DIR)/monitor_sender
MONITORING_SCRIPT = $(MONITORING_DIR)/monitoring.py

EXECUTABLE = $(BUILD_DIR)/gen_app

SRC_FILES = $(shell find $(SRC_DIR) -name '*.cpp')
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))

CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -I$(INCLUDE_DIR) `pkg-config --cflags gtkmm-3.0`
LDFLAGS = `pkg-config --libs gtkmm-3.0` -lstdc++fs -lssh

all: clean build-monitoring appbuild copy-monitoring run

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

appbuild: $(OBJ_FILES)
	@echo "Linking $(EXECUTABLE)..."
	@$(CXX) $(OBJ_FILES) -o $(EXECUTABLE) $(LDFLAGS)
	@echo "The app has been generated in $(BUILD_DIR)."

build-monitoring:
	@echo "Building monitor_sender..."
	@$(MAKE) -C $(MONITORING_DIR)

copy-monitoring: build-monitoring
	@echo "Copying monitoring scripts..."
	@mkdir -p $(BUILD_DIR)/monitoring
	@cp $(MONITORING_BIN) $(BUILD_DIR)/monitoring/
	@cp $(MONITORING_SCRIPT) $(BUILD_DIR)/monitoring/

run:
	@echo "Running application..."
	@./$(EXECUTABLE)

clean:
	@echo "Cleaning up generated files..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(MONITORING_DIR) clean || true
	@echo "Cleanup finished."

.PHONY: all clean run appbuild copy-monitoring build-monitoring
