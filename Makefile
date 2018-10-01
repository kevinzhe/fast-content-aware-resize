CC = gcc

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INC_DIR = include

TARGET = car

SOURCES = $(shell find $(SRC_DIR) -name '*.c')
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

CFLAGS = -O2 -std=c11 -march=native $(WFLAGS) $(DFLAGS)
WFLAGS = -Wall -Wextra -pedantic -Wfloat-equal -Wundef -Wshadow \
	-Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 \
	-Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default \
	-Wswitch-enum -Wconversion -Wunreachable-code
DFLAGS = -DLOG_USE_COLOR

LIB = \
	$(shell pkg-config --libs 'MagickWand < 7')

INC = \
	-I$(INC_DIR) \
	$(shell pkg-config --cflags 'MagickWand < 7' | sed s/-I/-isystem/)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $(BIN_DIR)/$@)
	@echo 'LD' $(BIN_DIR)/$@
	@$(CC) $(LIB) $(OBJECTS) -o $(BIN_DIR)/$@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo 'CC' $@
	@$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

.PHONY: clean
