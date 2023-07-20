# Makefile to build a program with gcc and install config and binary files

# Compiler
CC := gcc

# Compiler flags
CFLAGS := -Wall -Wextra -std=c11

# Program source files
SRCS := backlight_manager.c

# Program executable name
TARGET := backlight_manager

# XDG_CONFIG_HOME directory
XDG_CONFIG_HOME := $(HOME)/.config

# Directory for the program config
CONFIG_DIR := $(XDG_CONFIG_HOME)/backlight_manager

# Installation directories
BIN_DIR := /usr/bin

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

install: all
	mkdir -p $(CONFIG_DIR)
	cp backlight_manager.conf $(CONFIG_DIR)/backlight_manager.conf
	cp $(TARGET) $(BIN_DIR)/$(TARGET)

uninstall:
	rm -f $(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(CONFIG_DIR)
	rm -f $(TARGET)

.PHONY: all install uninstall clean

