%.o: %.c
        gcc -Zrsx32 -c -g2 -o $@ $<

all: alink.exe

OBJS=alink.o args.o util.o objload.o segments.o combine.o

$(OBJS): alink.h

alink.exe: $(OBJS)
        gcc -Zrsx32 -g2 -o $@ $^

