#define SWITCHCHAR '-'
#define PATH_CHAR '\\'

#define ERR_EXTRA_DATA 1
#define ERR_NO_HEADER 2
#define ERR_NO_RECDATA 3
#define ERR_NO_MEM 4
#define ERR_INV_DATA 5
#define ERR_INV_SEG 6
#define ERR_BAD_FIXUP 7
#define ERR_BAD_SEGDEF 8
#define ERR_ABS_SEG 9
#define ERR_DUP_PUBLIC 10
#define ERR_NO_MODEND 11
#define ERR_EXTRA_HEADER 12
#define ERR_UNKNOWN_RECTYPE 13
#define ERR_SEG_TOO_LARGE 14
#define ERR_MULTIPLE_STARTS 15
#define ERR_BAD_GRPDEF 16
#define ERR_OVERWRITE 17
#define ERR_INVALID_COMENT 18
#define ERR_ILLEGAL_IMPORTS 19

#define PREV_LE 1
#define PREV_LI 2
#define PREV_LI32 3

#define THEADR 0x80
#define LHEADR 0x82
#define COMENT 0x88
#define MODEND 0x8a
#define MODEND32 0x8b
#define EXTDEF 0x8c
#define TYPDEF 0x8e
#define PUBDEF 0x90
#define PUBDEF32 0x91
#define LINNUM 0x94
#define LINNUM32 0x95
#define LNAMES 0x96
#define SEGDEF 0x98
#define SEGDEF32 0x99
#define GRPDEF 0x9a
#define FIXUPP 0x9c
#define FIXUPP32 0x9d
#define LEDATA 0xa0
#define LEDATA32 0xa1
#define LIDATA 0xa2
#define LIDATA32 0xa3
#define COMDEF 0xb0
#define LIBHDR 0xf0
#define LIBEND 0xf1

#define COMENT_TRANSLATOR 0x00
#define COMENT_INTEL_COPYRIGHT 0x01
#define COMENT_LIB_SPEC 0x81
#define COMENT_MSDOS_VER 0x9c
#define COMENT_MEMMODEL 0x9d
#define COMENT_DOSSEG 0x9e
#define COMENT_DEFLIB 0x9f
#define COMENT_OMFEXT 0xa0
#define COMENT_NEWOMF 0xa1
#define COMENT_LINKPASS 0xa2
#define COMENT_LIBMOD 0xa3 
#define COMENT_EXESTR 0xa4
#define COMENT_INCERR 0xa6
#define COMENT_NOPAD 0xa7
#define COMENT_WKEXT 0xa8
#define COMENT_LZEXT 0xa9
#define COMENT_PHARLAP 0xaa
#define COMENT_IBM386 0xb0
#define COMENT_RECORDER 0xb1
#define COMENT_COMMENT 0xda
#define COMENT_COMPILER 0xdb
#define COMENT_DATE 0xdc
#define COMENT_TIME 0xdd
#define COMENT_USER 0xdf
#define COMENT_DEPFILE 0xe9
#define COMENT_COMMANDLINE 0xff
#define COMENT_PUBTYPE 0xe1
#define COMENT_COMPARAM 0xea
#define COMENT_TYPDEF 0xe3
#define COMENT_STRUCTMEM 0xe2
#define COMENT_OPENSCOPE 0xe5
#define COMENT_LOCAL 0xe6
#define COMENT_ENDSCOPE 0xe7
#define COMENT_SOURCEFILE 0xe8

#define EXT_IMPDEF 0x01
#define EXT_EXPDEF 0x02

#define SEG_ALIGN 0xe0
#define SEG_COMBINE 0x1c
#define SEG_BIG 0x02
#define SEG_USE32 0x01

#define SEG_ABS 0x00
#define SEG_BYTE 0x20
#define SEG_WORD 0x40
#define SEG_PARA 0x60
#define SEG_PAGE 0x80
#define SEG_DWORD 0xa0
#define SEG_MEMPAGE 0xc0
#define SEG_BADALIGN 0xe0

#define SEG_PRIVATE 0x00
#define SEG_PUBLIC 0x08
#define SEG_PUBLIC2 0x10
#define SEG_STACK 0x14
#define SEG_COMMON 0x18
#define SEG_PUBLIC3 0x1c

#define REL_SEGDISP 0
#define REL_EXTDISP 2
#define REL_GRPDISP 1
#define REL_EXPFRAME 3
#define REL_SEGONLY 4
#define REL_EXTONLY 6
#define REL_GRPONLY 5

#define REL_SEGFRAME 0
#define REL_GRPFRAME 1
#define REL_EXTFRAME 2
#define REL_LILEFRAME 4
#define REL_TARGETFRAME 5

#define FIX_SELFREL 0x10
#define FIX_MASK (0x0f+FIX_SELFREL)

#define FIX_THRED 0x08
#define THRED_MASK 0x07

#define FIX_LBYTE 0
#define FIX_OFS16 1
#define FIX_BASE 2
#define FIX_PTR1616 3
#define FIX_HBYTE 4
#define FIX_OFS16_2 5
#define FIX_OFS32 9
#define FIX_PTR1632 11
#define FIX_OFS32_2 13

#define FIX_SELF_LBYTE (FIX_LBYTE+FIX_SELFREL)
#define FIX_SELF_OFS16 (FIX_OFS16+FIX_SELFREL)
#define FIX_SELF_OFS16_2 (FIX_OFS16_2+FIX_SELFREL)
#define FIX_SELF_OFS32 (FIX_OFS32+FIX_SELFREL)
#define FIX_SELF_OFS32_2 (FIX_OFS32_2+FIX_SELFREL)

#define OUTPUT_COM 1
#define OUTPUT_EXE 2

#define EXP_ORD 0x80

#define DEF_SEG_COUNT 1024
#define DEF_NAME_COUNT 1024
#define DEF_GRP_COUNT 1024
#define DEF_PUB_COUNT 1024
#define DEF_EXT_COUNT 1024
#define DEF_RELOC_COUNT 65536
#define DEF_IMP_COUNT 1024
#define DEF_EXP_COUNT 1024
#define DEF_LIBFILE_COUNT 64

typedef char *PCHAR,**PPCHAR;
typedef unsigned char *PUCHAR;
typedef unsigned long UINT;

typedef struct __seg {
 long nameindex;
 long classindex;
 long overlayindex;
 UINT length;
 UINT absframe;
 UINT absofs;
 UINT base;
 unsigned char attr;
 PUCHAR data;
 PUCHAR datmask;
} SEG, *PSEG, **PPSEG;

typedef struct __imprec {
 PCHAR int_name;
 PCHAR mod_name;
 PCHAR imp_name;
 unsigned short ordinal;
 char flags;
} IMPREC, *PIMPREC, **PPIMPREC;

typedef struct __exprec {
 PCHAR int_name;
 PCHAR exp_name;
 unsigned short ordinal;
 char flags;
 long pubnum;
} EXPREC, *PEXPREC, **PPEXPREC;

typedef struct __datablock {
 long count;
 long blocks;
 long dataofs;
 void *data;
} DATABLOCK, *PDATABLOCK, **PPDATABLOCK;

typedef struct __pubdef {
 PCHAR name;
 long segnum;
 long grpnum;
 long typenum;
 UINT ofs;
} PUBLIC, *PPUBLIC,**PPPUBLIC;

typedef struct __extdef {
 PCHAR name;
 long typenum;
 long pubnum;
 long impnum;
} EXTREC, *PEXTREC,**PPEXTREC;

typedef struct __reloc {
 UINT ofs;
 long segnum;
 unsigned char ftype,ttype,rtype;
 long target;
 UINT disp;
 long frame;
} RELOC, *PRELOC,**PPRELOC;

typedef struct __grp {
 long nameindex;
 long numsegs;
 long segindex[256];
 long segnum;
} GRP, *PGRP, **PPGRP;

typedef struct __libentry {
 PCHAR name;
 unsigned short modpage; 
} LIBENTRY, *PLIBENTRY, **PPLIBENTRY;

typedef struct __libfile {
 PCHAR filename;
 unsigned short blocksize;
 unsigned short numdicpages;
 UINT dicstart;
 char flags;
 long numsyms;
 PPLIBENTRY syms;
} LIBFILE, *PLIBFILE, **PPLIBFILE;

