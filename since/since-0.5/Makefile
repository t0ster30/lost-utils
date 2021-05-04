# Files
SRC       = since.c
OBJ       = since.o
TARGET    = since
MAN       = since.1
BINDIR    = /usr/local/bin
MANDIR    = /usr/local/man/man1

# Compiler and Flags
CC        = gcc
CFLAGS    = -Wall -O2 
RM        = rm -f
INSTALL   = install -c
LINK      = ln -s
TOUCH     = touch
TAR       = tar

# Generic build all
all : $(TARGET)

# Install
install : $(TARGET)
	$(INSTALL) -m 0755 $(TARGET) $(BINDIR)
	$(INSTALL) -m 0644 $(MAN) $(MANDIR)

# Build since executable 
$(TARGET) : $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

# Build object files from source
.c.o :
	$(CC) $(CFLAGS) -c $*.c

# Remove junk
clean : 
	$(RM) *.o core *~

reallyclean : clean
	$(RM) $(TARGET)
