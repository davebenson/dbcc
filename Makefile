CC = cc
CFLAGS = -W -Wall -g -std=c11

all: generated tests/test-parser

libdbcc.a: dbcc-parser-p.o dbcc-parser.o dbcc-symbol.o \
        dbcc-code-position.o dbcc-type.o dbcc-statement.o \
        dbcc-expr.o dbcc-error.o dbcc-namespace.o dbcc.o \
        dbcc-common.o dbcc-constant.o cpp-expr-evaluate-p.o \
        dbcc-ptr-table.o \
dsk/dsk-buffer.o dsk/dsk-common.o dsk/dsk-object.o dsk/dsk-error.o dsk/dsk-mem-pool.o dsk/dsk-dir.o dsk/dsk-file-util.o dsk/dsk-ascii.o dsk/dsk-rand.o dsk/dsk-rand-xorshift1024.o dsk/dsk-fd.o dsk/dsk-path.o
	ar cru $@ $^

lemon: lemon.c
	cc -o lemon lemon.c

generated/mk-prime-table: mk-prime-table.c
	cc -o generated/mk-prime-table mk-prime-table.c -lm
generated/prime-table.inc: generated/mk-prime-table
	generated/mk-prime-table > generated/prime-table.inc

dbcc-parser-p.c dbcc-parser-p.h: lemon dbcc-parser-p.lemon
	@rm -f dbcc-parser-p.c dbcc-parser-p.out dbcc-parser-p.h
	./lemon dbcc-parser-p.lemon || true
	touch dbcc-parser-p.c dbcc-parser-p.out dbcc-parser-p.h
	chmod -w dbcc-parser-p.c

cpp-expr-evaluate-p.c cpp-expr-evaluate-p.h: lemon dbcc-parser-p.lemon
	@rm -f cpp-expr-evaluate-p.c cpp-expr-evaluate-p.out cpp-expr-evaluate-p.h
	./lemon cpp-expr-evaluate-p.lemon || true
	touch cpp-expr-evaluate-p.c cpp-expr-evaluate-p.out cpp-expr-evaluate-p.h
	chmod -w cpp-expr-evaluate-p.c

dbcc-parser-p.o: dbcc-parser-p.c dbcc.h
	cc $(CFLAGS) -c dbcc-parser-p.c 

dbcc-parser.o: cpp-expr-evaluate-p.h

tests/test-parser: tests/test-parser.c libdbcc.a
	cc $(CFLAGS) tests/test-parser.c libdbcc.a

clean:
	rm -f lemon *.o dbcc-parser-p.{c,out,h} cpp-expr-evaluate-p.{c,out,h}


missing-refs:
	@make 2>&1 | perl -ne 'print "$$1\n" if /^\s*"([^"]+)"/' | sort -u

dbcc-error.o: generated/dbcc-error-codes.inc
dbcc-ptr-table.o: generated/prime-table.inc

generated/dbcc-error-codes.inc: generated scripts/mk-error-codes-inc dbcc-error.h
	scripts/mk-error-codes-inc < dbcc-error.h > generated/dbcc-error-codes.inc

generated:
	mkdir -p generated
