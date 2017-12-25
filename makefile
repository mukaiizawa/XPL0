# makefile
# Usage: make [os={linux|windows}] [cc={clang|gcc}] [debug={on|off}]

supportos=linux windows
os?=linux
cc?=clang

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

.SUFFIXES: .c .o
.c.o:
	$(cc) $(cflags) $<

all: xpl0

xpl0: main.o
	$(link)

clean:
	rm -f *.o *.exe xpl0
