BUILD_DIR := build

.PHONY: all clean

all:
	@echo "Batos OS build system"

clean:
	rm -rf $(BUILD_DIR)
