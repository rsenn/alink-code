% :: RCS/%
	co $@

all: alink.exe

OBJS=alink.o args.o util.o objload.o segments.o combine.o symbols.o op_exe.o map.o relocs.o op_pe.o coff.o op_bin.o res.o omflib.o cofflib.o message.o

$(OBJS) : %.o : %.c alink.h
	gcc -Zrsx32 -c -g2 -Wall -o $@ $<

objload.o alink.o omflib.o: omf.h

op_pe.o: pe.h

coff.o: coff.h

alink.exe: $(OBJS)
	gcc -Zrsx32 -g2 -o $@ $^

