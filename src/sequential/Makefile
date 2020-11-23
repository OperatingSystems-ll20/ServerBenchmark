INCLUDE_DIR = ../../include/sequential -I../../include/common
BIN_DIR = ../../bin
COMMON_SRC = ../common/pathHelper.c ../common/sobelPython.c

EXEC_NAME = sequential
CC = gcc
LINK_LIBS = -lpthread $(shell pkg-config --libs python3)
INCLUDES = $(INCLUDE_DIR) $(shell pkg-config --cflags python3)

all: directory sequential 

directory:
		@mkdir -p $(BIN_DIR)

sequential:
		$(CC) -I $(INCLUDES)  $(EXEC_NAME).c socketFIFO.c $(COMMON_SRC) -o $(BIN_DIR)/$(EXEC_NAME) $(LINK_LIBS)