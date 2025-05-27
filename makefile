LIBSRC := $(filter-out ./src/main.c, $(wildcard ./src/*.c))
LIBOBJ = $(LIBSRC:.c=.o)

CFLAGS = -g -Wall -std=c99 -pedantic -D_POSIX_C_SOURCE=200809L
LIBRARYOUTNAME = libpandaforever.a
TOOLOUTNAME = pandaforever
MANNAME = pandaforever.1
TOOLOBJ = ./src/main.o

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

all: $(LIBRARYOUTNAME) $(TOOLOUTNAME)

$(LIBRARYOUTNAME): $(LIBOBJ)
	ar -rcs $@ $^

$(TOOLOUTNAME): $(LIBRARYOUTNAME) $(TOOLOBJ)
	$(CC) $(CFLAGS) -o $(TOOLOUTNAME) $(TOOLOBJ) -L. -lpandaforever -lcurl

.PHONY: clean uninstall
clean:
	rm -f $(LIBRARYOUTNAME) $(TOOLOUTNAME) $(LIBOBJ) $(TOOLOBJ)

install: $(OUTNAME)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(TOOLOUTNAME) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/lib/
	install -m 644 $(LIBRARYOUTNAME) $(DESTDIR)$(PREFIX)/lib/
	install -d $(DESTDIR)$(PREFIX)/include/pandaforever/
	install -m 644 $(wildcard ./src/*.h) $(DESTDIR)$(PREFIX)/include/pandaforever/
	install -d $(DESTDIR)$(PREFIX)/share/man/man1/
	install -m 644 $(MANNAME) $(DESTDIR)$(PREFIX)/share/man/man1/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TOOLOUTNAME)
	rm -f $(DESTDIR)$(PREFIX)/lib/$(LIBRARYOUTNAME)
	rm -rf $(DESTDIR)$(PREFIX)/include/pandaforever/
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/$(MANNAME)

# DO NOT DELETE

./src/bonusmodes.o: ./src/panda.h ./src/mischelp.h ./src/metadatawrite.h
./src/bonusmodes.o: ./src/curlhelper.h ./src/configreader.h ./src/main.h
./src/bonusmodes.o: ./src/lose.h
./src/comments.o: ./src/mischelp.h ./src/panda.h ./src/fixhtmlgarbage.h
./src/comments.o: ./src/lose.h ./src/metadatawrite.h
./src/configreader.o: ./src/mischelp.h ./src/configreader.h ./src/lose.h
./src/curlhelper.o: ./src/lose.h
./src/hooks.o: ./src/lose.h ./src/mischelp.h ./src/panda.h
./src/lose.o: ./src/lose.h
./src/main.o: ./src/curlhelper.h ./src/panda.h ./src/mischelp.h
./src/main.o: ./src/configreader.h ./src/lose.h ./src/metadatawrite.h
./src/main.o: ./src/main.h ./src/bonusmodes.h ./src/hooks.h
./src/metadatawrite.o: ./src/mischelp.h ./src/fixhtmlgarbage.h ./src/panda.h
./src/metadatawrite.o: ./src/lose.h ./src/metadatawrite.h
./src/mischelp.o: ./src/fixhtmlgarbage.h ./src/lose.h ./src/stolenfunctions.h
./src/panda.o: ./src/curlhelper.h ./src/mischelp.h ./src/fixhtmlgarbage.h
./src/panda.o: ./src/metadatawrite.h ./src/panda.h ./src/lose.h ./src/hooks.h
./src/stolenfunctions.o: ./src/lose.h
