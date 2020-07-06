###############################################################################
# Author:	Luke McEvoy
# Date:		April 15, 2020
# Pledge:	I pledge my Honor I have abided by the Stevens Honor System.
# Description:	Replica of Linux shell in C.
###############################################################################

CC     = gcc
C_FILE = $(wildcard *.c)
TARGET = $(patsubst %.c,%,$(C_FILE))
CFLAGS = -g -Wall -Werror -pedantic-errors

all:
	$(CC) $(CFLAGS) $(C_FILE) -o $(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
