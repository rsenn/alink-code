%.o: %.c alink.h
        gcc -Zrsx32 -c -o $@ $<

alink.exe: alink.o combine.o util.o output.o objload.o
        gcc -Zrsx32 -o $@ $^
