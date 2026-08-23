CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -Isrc
LDLIBS  = -lws2_32
BUILD   = build
TARGET  = $(BUILD)/http-server.exe

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir $(BUILD)

test: all
	powershell -NoProfile -ExecutionPolicy Bypass -File tests\run-tests.ps1

clean:
	if exist $(BUILD) rmdir /s /q $(BUILD)

.PHONY: all test clean
