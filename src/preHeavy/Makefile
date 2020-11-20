INCLUDE_DIR = ../../include/preHeavy
BIN_DIR = ../../bin

EXEC_NAME = preHeavy
CC = gcc
LINK_LIBS = -lpthread $(shell pkg-config --libs python3)
INCLUDES = $(INCLUDE_DIR) $(shell pkg-config --cflags python3)

all: directory preHeavy 

directory:
		@mkdir -p $(BIN_DIR)

preHeavy:
		$(CC) -I $(INCLUDES)  $(EXEC_NAME).c -o $(BIN_DIR)/$(EXEC_NAME) $(LINK_LIBS)