# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -g -Wall -pedantic
LDFLAGS := -lglfw -lvulkan -ldl
SHADER_COMPILER := glslc

# Directories
SRC_DIR := src/cpp
BUILD_DIR := build
SHADER_DIR := src/shaders
SUBDIR_SHADER_FILES := $(shell find $(SHADER_DIR) -mindepth 2 -name "*.comp")
SHADER_BUILD_DIR := build/shaders

# Source files
SRC_FILES := $(shell find $(SRC_DIR) -name "*.cpp")
OBJ_FILES := $(SRC_FILES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Shader files
COMPUTE_SHADERS := $(wildcard $(SHADER_DIR)/*.comp)
COMPILED_COMPUTE_SHADERS := $(COMPUTE_SHADERS:$(SHADER_DIR)/%.comp=$(SHADER_BUILD_DIR)/%.comp.spv)

# Executable
TARGET := ray_tracer

# Rules
all: $(TARGET)
compile: $(TARGET)

$(TARGET): $(OBJ_FILES) $(COMPILED_COMPUTE_SHADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_FILES) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SRC_DIR)/%.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(BUILD_DIR) -c $< -o $@

$(SHADER_BUILD_DIR)/%.comp.spv: $(SHADER_DIR)/%.comp $(SUBDIR_SHADER_FILES)
	@mkdir -p $(SHADER_BUILD_DIR)
	$(SHADER_COMPILER) $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(SHADER_BUILD_DIR) $(TARGET)

.PHONY: all clean
