#ifndef NE_H
#define NE_H

#define NE_SIGNATURE      0x00
#define NE_LMAJOR         0x02
#define NE_LMINOR         0x03
#define NE_EXPORTOFFSET   0x04
#define NE_EXPORTLENGTH   0x06
#define NE_CHECKSUM       0x08
#define NE_FLAGS          0x0c
#define NE_DATASEGNUM     0x0e
#define NE_HEAPSIZE       0x10
#define NE_STACKSIZE      0x12
#define NE_ENTRYOFFSET    0x14
#define NE_ENTRYSEG       0x16
#define NE_STACKOFFSET    0x18
#define NE_STACKSEG       0x1a
#define NE_NUMSEGS        0x1c
#define NE_MODREFCOUNT    0x1e
#define NE_NONRESLENGTH   0x20
#define NE_SEGOFFSET      0x22
#define NE_RESOURCEOFFSET 0x24
#define NE_RESNAMEOFFSET  0x26
#define NE_MODREFOFFSET   0x28
#define NE_IMPNAMEOFFSET  0x2a
#define NE_NONRESOFFSET   0x2c
#define NE_NUMMOVABLE     0x30
#define NE_SHIFTCOUNT     0x32
#define NE_RESSEGCOUNT    0x34
#define NE_OS             0x36
#define NE_OSFLAGS        0x37
#define NE_FASTLOADOFFSET 0x38
#define NE_FASTLOADLENGTH 0x3a
#define NE_MINSWAPSIZE    0x3c
#define NE_WINVER         0x3e

#define NE_HEADERSIZE     0x40
#define NE_SEGENTRY_SIZE  0x08

#define NE_OS_UNKNOWN     0
#define NE_OS_OS2         1
#define NE_OS_WIN         2
#define NE_OS_MSDOS       3
#define NE_OS_WIN386      4
#define NE_OS_BORLAND     5

#endif
