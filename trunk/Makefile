CC = gcc

LIBPURPLE_CFLAGS = -I/usr/include/libpurple -I/usr/local/include/libpurple
GLIB_CFLAGS = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include -I/usr/local/include 


all:   google-contact.so install

install: google-contact.so
	cp google-contact.so ~/.purple/plugins/

clean:
	rm -f google-contact.so google-contact.dll

google-contact.so: google-contact.c
	${CC} ${LIBPURPLE_CFLAGS} -Wall ${GLIB_CFLAGS} -I. -g -O2 google-contact.c -o google-contact.so -shared -fPIC -DPIC -ggdb

dll:
	make -f Makefile.mingw
