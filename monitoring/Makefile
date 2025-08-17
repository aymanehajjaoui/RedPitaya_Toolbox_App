MODEL ?= Z10

TARGET := monitor_sender
SRC := monitor_sender.c
OUT_DIR := .

CC := arm-linux-gnueabihf-gcc
SYSROOT := $(HOME)/redpitaya-sysroot

COMMON_FLAGS  = -Wall -Wextra -O3 -pedantic -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mtune=cortex-a9 -D$(MODEL)
COMMON_FLAGS += --sysroot=$(SYSROOT)
COMMON_FLAGS += -I$(SYSROOT)/usr/include
COMMON_FLAGS += -I$(SYSROOT)/opt/redpitaya/include

CFLAGS = -std=gnu11 $(COMMON_FLAGS)
LDFLAGS = --sysroot=$(SYSROOT) -L$(SYSROOT)/usr/lib -L$(SYSROOT)/opt/redpitaya/lib -lpthread

BIN := $(OUT_DIR)/$(TARGET)

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	$(RM) -f $(TARGET)
