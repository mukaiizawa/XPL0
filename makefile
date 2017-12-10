# makefile

cc=clang
uflags=-Wall -Werror
cflags=$(uflags) -c
lflags=$(uflags)
sflags=-s
link=$(cc) $(lflags) -o $@ $+

.SUFFIXES: .c .o
.c.o:
	$(cc) $(cflags) $<

all: xpl0

xpl0: main.o
	$(link)

clean:
	rm -f *.o *.exe
