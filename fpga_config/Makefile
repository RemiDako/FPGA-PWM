CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic

TARGET := gowin_sspi
SOURCE := gowin_sspi.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -std=c11 -o $@ $<

clean:
	rm -f $(TARGET)
