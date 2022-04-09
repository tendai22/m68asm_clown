LEX = flex
YACC = bison

CFLAGS = -ggdb3 -Og -Wall -Wextra -pedantic -fwrapv -ansi -Wno-long-long

all: generators

generators: lexical.c lexical.h syntactic.c syntactic.h

lexical.c lexical.h: lexical.l syntactic.h
	$(LEX) --outfile=lexical.c --header-file=lexical.h $<

syntactic.c syntactic.h: syntactic.y
	$(YACC) --output=syntactic.c --header=syntactic.h --debug $<

clownassembler_asm68k: frontend_asm68k.c dictionary.c lexical.c semantic.c strcmpci.c syntactic.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

clownassembler: frontend_custom.c dictionary.c lexical.c semantic.c strcmpci.c syntactic.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)
