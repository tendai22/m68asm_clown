CFLAGS = -ggdb3 -Og -Wall -Wextra -pedantic -fwrapv

all: generators

generators: lexical.c lexical.h syntactic.c syntactic.h

lexical.c lexical.h: clownassembler-flex.l syntactic.h
	flex --outfile=lexical.c --header-file=lexical.h $<

syntactic.c syntactic.h: clownassembler-bison.y
	bison --output=syntactic.c --header=syntactic.h --debug $<

clownassembler: main.c lexical.c semantic.c syntactic.c symbols.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)
