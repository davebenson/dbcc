CC = cc
CFLAGS = -W -Wall -g -std=c11

all: tests/test-parser

libdbcc.a: dbcc-parser-p.o dbcc-parser.o dbcc-symbol.o \
        dbcc-code-position.o dbcc-type.o \
dsk/dsk-buffer.o dsk/dsk-common.o dsk/dsk-object.o dsk/dsk-error.o dsk/dsk-mem-pool.o dsk/dsk-dir.o dsk/dsk-file-util.o dsk/dsk-ascii.o dsk/dsk-rand.o dsk/dsk-rand-xorshift1024.o dsk/dsk-fd.o dsk/dsk-path.o
	ar cru $@ $^

lemon: lemon.c
	cc -o lemon lemon.c

dbcc-parser-p.c: lemon dbcc-parser-p.lemon
	@rm -f dbcc-parser-p.c dbcc-parser-p.out
	./lemon dbcc-parser-p.lemon || true
	chmod -w dbcc-parser-p.c

dbcc-parser-p.o: dbcc-parser-p.c dbcc.h
	cc $(CFLAGS) -c dbcc-parser-p.c 

tests/test-parser: tests/test-parser.c libdbcc.a
	cc $(CFLAGS) tests/test-parser.c libdbcc.a

clean:
	rm -f lemon *.o dbcc-parser-p.{c,out,h}


missing-refs:
	make 2>&1 | perl -ne 'print "$$1\n" if /^\s*"([^"]+)"/'
