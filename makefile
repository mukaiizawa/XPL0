# makefile
# Usage: make [os=windows] [cc=gcc] [debug=off]

supportos=linux windows
os?=linux
cc?=clang
debug?=on

ifeq ($(filter $(supportos),$(os)),)
$(error Unsupport os.)
endif
ifeq ($(os), windows)
exe=.exe
endif

uflags=-Wall -Werror
cflags=$(uflags) -c
lflags=$(uflags)
sflags=-s
link=$(cc) $(lflags) -o $@$(exe) $+

ifeq ($(debug),on)
uflags+=-g
else
cflags+=-O3 -DNDEBUG
endif

.SUFFIXES: .c .o
.c.o:
	$(cc) $(cflags) $<

xpl0=xpl0$(exe)
all: xpl0

xpl0: xpl0.o
	$(link)

test: $(xpl0)
	./$(xpl0) < test.xpl0

clean:
	rm -f *.o *.exe xpl0
