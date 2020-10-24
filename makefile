CC = gcc
#TARGET_OPTIONS=-Zrsx32 #for EMX/RSX compiler
TARGET_OPTIONS = -w -O2
#EXEEXT = .exe
prefix ?= /usr/local

all: alink$(EXEEXT)

SRCS = $(wildcard *.c)
OBJS = alink.o args.o util.o objload.o segments.o combine.o symbols.o op_exe.o map.o relocs.o op_pe.o coff.o op_bin.o res.o omflib.o cofflib.o message.o mergerec.o
OBJS := $(SRCS:%.c=%.o)

$(OBJS) : %.o : %.c alink.h
	gcc $(TARGET_OPTIONS) -c -Wall -o $@ $<

objload.o alink.o omflib.o: omf.h
alink.o meregerec.o: mergerec.h
op_pe.o: pe.h
coff.o: coff.h

alink$(EXEEXT): $(OBJS)
	gcc $(TARGET_OPTIONS) -o $@ $^

%.obj: %.asm
	nasm -fwin32 $<

tdll.obj: tdll.asm
	nasm -fobj $<

t2.obj: t2.asm
	nasm -fobj $<

rtn.obj: rtn.asm
	nasm -fobj $<

check: alink$(EXEEXT) test1.obj test2.obj tdll.obj rtn.obj tcoff.obj t2.obj
	./alink -fpe -m+ -o t1$(EXEEXT) test1.obj
	./alink -fpe -m+ -o t2$(EXEEXT) test1.obj -mergesegs a b
	./alink -fpe -m+ -o t3$(EXEEXT) test1.obj -mergesegs a c
	./alink -fpe -m+ -o t4$(EXEEXT) test1.obj -mergesegs aa bb
	./alink -fpe -m+ -o t5$(EXEEXT) test2.obj
	./alink -fpe -m+ -o t6$(EXEEXT) test2.obj -mergesegs a b
	./alink -fpe -m+ -o t7$(EXEEXT) test2.obj -mergesegs a c
	./alink -fpe -m+ -o t8$(EXEEXT) test2.obj -mergesegs aa bb
	./alink -fpe -m+ -o t9$(EXEEXT) test1.obj test2.obj
	./alink -fpe -m+ -o t10$(EXEEXT) test1.obj test2.obj -mergesegs a b
	./alink -fpe -m+ -o t11$(EXEEXT) test1.obj test2.obj -mergesegs a c
	./alink -fpe -m+ -o t12$(EXEEXT) test1.obj test2.obj -mergesegs aa bb
	./alink -fpe -m+ -o t13$(EXEEXT) test1.obj test2.obj -mergesegs a z -mergesegs b z
	./alink -fpe -m+ -o t14$(EXEEXT) rtn.obj rtn.res win32.lib -subsys windows -reloc
	./alink -fpe -m+ -o t15$(EXEEXT) tcoff.obj win32.lib -entry main
	./alink -fpe -m+ -o t16.dll tdll.obj win32.lib -oldmap -m t16dll.map -dll
	./alink -fpe -m+ -o t16$(EXEEXT) t2.obj win32.lib -oldmap -m t16exe.map

install:
	install -m755 alink$(EXEEXT) $(prefix)/bin/
