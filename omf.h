#ifndef OMF_H
#define OMF_H

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
#define BAKPAT 0xb2
#define BAKPAT32 0xb3
#define LEXTDEF 0xb4
#define LEXTDEF32 0xb5
#define LPUBDEF 0xb6
#define LPUBDEF32 0xb7
#define LCOMDEF 0xb8
#define CEXTDEF 0xbc
#define COMDAT 0xc2
#define COMDAT32 0xc3
#define LINSYM 0xc4
#define LINSYM32 0xc5
#define ALIAS 0xc6
#define NBKPAT 0xc8
#define NBKPAT32 0xc9
#define LLNAMES 0xca
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

#define SEG_ALIGN 0x3e0
#define SEG_COMBINE 0x1c
#define SEG_BIG 0x02
#define SEG_USE32 0x01

#define SEG_BYTE 0x20
#define SEG_WORD 0x40
#define SEG_PARA 0x60
#define SEG_PAGE 0x80
#define SEG_DWORD 0xa0
#define SEG_MEMPAGE 0xc0
#define SEG_BADALIGN 0xe0
#define SEG_8BYTE 0x100
#define SEG_32BYTE 0x200
#define SEG_64BYTE 0x300

#define SEG_ABS 0x00
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
#define FIX_PHAR_OFS32 5
#define FIX_PHAR_PTR1632 6
/* RVA32 fixups are not supported by OMF, so has an out-of-range number */
#define FIX_RVA32 256

#define FIX_SELF_LBYTE (FIX_LBYTE+FIX_SELFREL)
#define FIX_SELF_OFS16 (FIX_OFS16+FIX_SELFREL)
#define FIX_SELF_OFS16_2 (FIX_OFS16_2+FIX_SELFREL)
#define FIX_SELF_OFS32 (FIX_OFS32+FIX_SELFREL)
#define FIX_SELF_OFS32_2 (FIX_OFS32_2+FIX_SELFREL)

#define EXP_ORD       0x80
#define EXP_RESIDENT  0x40
#define EXP_NODATA    0x20
#define EXP_NUMPARAMS 0x1F

#define COMDAT_LI   0x02
#define COMDAT_CONT 0x01

typedef struct liblock LIBLOCK,*PLIBLOCK, **PPLIBLOCK;

struct liblock
{
    UINT count;
    UINT blocks;
    UINT dataofs;
    void *data;
};

typedef struct comdatentry COMDATENTRY,*PCOMDATENTRY;

struct comdatentry
{
    PCHAR name;
    PCOMDATREC comdat;
};


#endif
