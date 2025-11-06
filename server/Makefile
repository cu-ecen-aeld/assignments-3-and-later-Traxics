# Cross-compilation support

#CROSS_COMPILE ?= aarch64-none-linux-gnu-

ifdef CROSS_COMPILE
CC=$(CROSS_COMPILE)gcc
else
CC=gcc
endif

CFLAGS=-Wall -Wextra -std=c99
TARGET=aesdsocket
SOURCE=aesdsocket.c

all: $(TARGET)

default: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

.PHONY: all default clean

