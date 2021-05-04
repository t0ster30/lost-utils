# IMHO auto{make,conf} is overkill. Simple, readable makefiles improve security

prefix=/usr/local
NAME = since
VERSION = 1.1

CFLAGS = -Wall -O2 -DVERSION=\"$(VERSION)\"
# disable/enable as desired 
CFLAGS += $(shell test -f /usr/include/sys/inotify.h && echo -DUSE_INOTIFY)
CFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
#CFLAGS += -DDEBUG

CC = gcc
RM = rm -f
INSTALL = install -D

$(NAME): $(NAME).c
	$(CC) $(CFLAGS) -o $@ $^

install: $(NAME)
	$(INSTALL) $(NAME) $(prefix)/bin/$(NAME)
	$(INSTALL) $(NAME).1 $(prefix)/share/man/man1/$(NAME).1

clean: 
	$(RM) $(NAME) core *.o
