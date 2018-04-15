CC = cc
CFLAGS = -W -Wall -g -std=c11

libdbcc.a: dbcc-parser-p.o p-parser.o
	ar cru $@ $^

lemon: lemon.c
	cc -o lemon lemon.c

dbcc-parser-p.c: lemon dbcc-parser-p.lemon
	@rm -f dbcc-parser-p.c dbcc-parser-p.out
	./lemon dbcc-parser-p.lemon || true
	chmod -w dbcc-parser-p.c

dbcc-parser-p.o: dbcc-parser-p.c dbcc.h
	cc $(CFLAGS) -c dbcc-parser-p.c 


clean:
	rm -f lemon *.o dbcc-parser-p.{c,out,h}
