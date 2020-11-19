INCLUDE_DIR = ../../include/preHeavy
BIN_DIR = ../../bin

EXEC_NAME = preHeavy
CC = gcc
LINK_LIBS = -lpthread

all: directory preHeavy 

directory:
		@mkdir -p $(BIN_DIR)

preHeavy:
		$(CC) -I $(INCLUDE_DIR) $(EXEC_NAME).c -o $(BIN_DIR)/$(EXEC_NAME) $(LINK_LIBS)