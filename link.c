#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

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

typedef char *PCHAR;
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
} PUBLIC, *PPUBLIC;

typedef struct __extdef {
 PCHAR name;
 long typenum;
 long pubnum;
} EXTREC, *PEXTREC;

typedef struct __reloc {
 UINT ofs;
 long segnum;
 unsigned char ftype,ttype,rtype;
 long target;
 UINT disp;
 long frame;
} RELOC, *PRELOC;

typedef struct __grp {
 long nameindex;
 long numsegs;
 long segindex[256];
 long segnum;
} GRP, *PGRP;

FILE *afile;
UINT filepos;
long reclength;
unsigned char rectype;
char case_sensitive;
char padsegments;
unsigned short maxalloc;
unsigned char buf[65536];
PCHAR namelist[1024];
UINT namecount,namemin;
PSEG seglist[1024];
PPSEG outlist;
UINT segcount,segmin,outcount;
PGRP grplist[1024];
UINT grpcount,grpmin;
char li_le;
PDATABLOCK lidata;
UINT prevofs;
long prevseg;
PPUBLIC publics[1024];
UINT pubcount,pubmin;
PEXTREC externs[1024];
UINT extcount,extmin;
PRELOC relocs[65536];
UINT fixcount,fixmin;
UINT nummods;
PCHAR modname[256];
UINT filecount;
PCHAR filename[256];
PCHAR outname;
long invext;
long gotstart;
RELOC startaddr;
long errcount;

int output_type;

void __inline__ ClearNbit(PUCHAR mask,long i)
{
	mask[i/8]&=!(1<<(i%8));
}

void __inline__ SetNbit(PUCHAR mask,long i)
{
	mask[i/8]|=(1<<(i%8));
}

char __inline__ GetNbit(PUCHAR mask,long i)
{
	return (mask[i/8]>>(i%8))&1;
}

long GetIndex(PUCHAR buf,long *index)
{
	long i;
	if(buf[*index]&0x80)
	{
		i=((buf[*index]&0x7f)*256)+buf[(*index)+1];
		(*index)+=2;
		return i;
	}
	else
	{
		return buf[(*index)++];
	}
}

void ReportError(long errnum)
{
		printf("\nError in file at %08lX",filepos);
		switch(errnum)
		{
		case ERR_EXTRA_DATA:
				printf(" - extra data in record\n");
				break;
		case ERR_NO_HEADER:
				printf(" - no header record\n");
				break;
		case ERR_NO_RECDATA:
				printf(" - record data not present\n");
				break;
		case ERR_NO_MEM:
				printf(" - insufficient memory\n");
				break;
		case ERR_INV_DATA:
				printf(" - invalid data address\n");
				break;
		case ERR_INV_SEG:
				printf(" - invalid segment number\n");
				break;
		case ERR_BAD_FIXUP:
				printf(" - invalid fixup record\n");
				break;
		case ERR_BAD_SEGDEF:
				printf(" - invalid segment definition record\n");
				break;
		case ERR_ABS_SEG:
				printf(" - data emitted to absolute segment\n");
				break;
		case ERR_DUP_PUBLIC:
				printf(" - duplicate public definition - %s\n",publics[pubcount]->name);
				break;
		case ERR_NO_MODEND:
			 printf(" - unexpected end of file (no MODEND record)\n");
			 break;
		case ERR_EXTRA_HEADER:
			 printf(" - duplicate module header\n");
			 break;
		case ERR_UNKNOWN_RECTYPE:
			 printf(" - unknown object module record type %02X\n",rectype);
			 break;
		case ERR_SEG_TOO_LARGE:
			 printf(" - 4Gb Non-Absolute segments not supported.\n");
			 break;
		case ERR_MULTIPLE_STARTS:
			 printf(" - start address defined in more than one module.\n");
			 break;
		case ERR_BAD_GRPDEF:
			 printf(" - illegal group definition\n");
			 break;
		case ERR_OVERWRITE:
			printf(" - overlapping data regions\n");
			break;
		default:
				printf("\n");
		}
		exit(1);
}

void DestroyLIDATA(PDATABLOCK p)
{
	long i;
	if(p->blocks)
	{
		for(i=0;i<p->blocks;i++)
		{
			DestroyLIDATA(((PPDATABLOCK)(p->data))[i]);
		}
	}
	free(p->data);
	free(p);
}

PDATABLOCK BuildLiData(long *bufofs)
{
	PDATABLOCK p;
	long i,j;

	p=malloc(sizeof(DATABLOCK));
	if(!p)
	{
		ReportError(ERR_NO_MEM);
	}
	i=*bufofs;
	p->dataofs=i-lidata->dataofs;
	p->count=buf[i]+256*buf[i+1];
	i+=2;
	if(rectype==LIDATA32)
	{
		p->count+=(buf[i]+256*buf[i+1])<<16;
		i+=2;
	}
	p->blocks=buf[i]+256*buf[i+1];
	i+=2;
	if(p->blocks)
	{
		p->data=malloc(p->blocks*sizeof(PDATABLOCK));
		if(!p->data)
		{
			ReportError(ERR_NO_MEM);
		}
		for(j=0;j<p->blocks;j++)
		{
			((PPDATABLOCK)p->data)[j]=BuildLiData(&i);
		}
	}
	else
	{
		p->data=malloc(buf[i]+1);
		if(!p->data)
		{
			ReportError(ERR_NO_MEM);
		}
		((char*)p->data)[0]=buf[i];
		i++;
		for(j=0;j<((PUCHAR)p->data)[0];j++,i++)
		{
			((PUCHAR)p->data)[j+1]=buf[i];
		}
	}
	*bufofs=i;
	return p;
}

void EmitLiData(PDATABLOCK p,long segnum,long *ofs)
{
	long i,j;

	for(i=0;i<p->count;i++)
	{
		if(p->blocks)
		{
			for(j=0;j<p->blocks;j++)
			{
				EmitLiData(((PPDATABLOCK)p->data)[j],segnum,ofs);
			}
		}
		else
		{
			for(j=0;j<((PUCHAR)p->data)[0];j++,(*ofs)++)
			{
				if((*ofs)>=seglist[segnum]->length)
				{
					ReportError(ERR_INV_DATA);
				}
				if(GetNbit(seglist[segnum]->datmask,*ofs))
				{
					if(seglist[segnum]->data[*ofs]!=((PUCHAR)p->data)[j+1])
					{
						ReportError(ERR_OVERWRITE);
					}
				}
				seglist[segnum]->data[*ofs]=((PUCHAR)p->data)[j+1];
			}
		}
	}
}

void RelocLIDATA(PDATABLOCK p,long *ofs)
{
	long i,j;

	for(i=0;i<p->count;i++)
	{
		if(p->blocks)
		{
			for(j=0;j<p->blocks;j++)
			{
				RelocLIDATA(((PPDATABLOCK)p->data)[j],ofs);
			}
		}
		else
		{
			j=relocs[fixcount]->ofs-p->dataofs;
			if(j>=0)
			{
				if((j<5) || ((li_le==PREV_LI32) && (j<7)))
				{
					ReportError(ERR_BAD_FIXUP);
				}
				fixcount++;
				relocs[fixcount]=malloc(sizeof(RELOC));
				if(!relocs[fixcount])
				{
					ReportError(ERR_NO_MEM);
				}
				memcpy(relocs[fixcount],relocs[fixcount-1],sizeof(RELOC));
				relocs[fixcount-1]->ofs=*ofs+j;
				*ofs+=((PUCHAR)p->data)[0];
			}
		}
	}
}

void LoadFIXUP(PRELOC r,PUCHAR buf,long *p)
{
	long j;

	j=*p;

	r->ftype=buf[j]>>4;
	r->ttype=buf[j]&0xf;
	j++;
	if(r->ftype&FIX_THRED)
	{
		if((r->ftype&THRED_MASK)>3)
		{
			ReportError(ERR_BAD_FIXUP);
		}
		printf("(FIXUPP) FRAME Referenced to THRED %i\n",r->ftype & THRED_MASK);
	}
	else
	{
		switch(r->ftype)
		{
		case REL_SEGFRAME:
		case REL_GRPFRAME:
		case REL_EXTFRAME:
			 r->frame=GetIndex(buf,&j);
			 if(!r->frame)
			 {
				ReportError(ERR_BAD_FIXUP);
			 }
			 break;
		case REL_LILEFRAME:
		case REL_TARGETFRAME:
			 break;
		default:
				ReportError(ERR_BAD_FIXUP);
		}
		switch(r->ftype)
		{
		case REL_SEGFRAME:
			 r->frame+=segmin-1;
			 break;
		case REL_GRPFRAME:
			 r->frame+=grpmin-1;
			 break;
		case REL_EXTFRAME:
			 r->frame+=extmin-1;
			 break;
		case REL_LILEFRAME:
			 r->frame=prevseg;
			 break;
		default:
				break;
		}
	}
	if(r->ttype&FIX_THRED)
	{
		if((r->ttype&THRED_MASK)>3)
		{
			ReportError(ERR_BAD_FIXUP);
		}
		printf("(FIXUPP) TARGET Referenced to THRED %i\n",r->ftype & THRED_MASK);
	}
	else
	{
		r->target=GetIndex(buf,&j);
		switch(r->ttype)
		{
		case REL_SEGDISP:
		case REL_GRPDISP:
		case REL_EXTDISP:
		case REL_SEGONLY:
		case REL_GRPONLY:
		case REL_EXTONLY:
			 if(!r->target)
			 {
				ReportError(ERR_BAD_FIXUP);
			 }
			 break;
		case REL_EXPFRAME:
			 break;
		default:
			ReportError(ERR_BAD_FIXUP);
		}
		switch(r->ttype)
		{
		case REL_SEGDISP:
			 r->target+=segmin-1;
			 break;
		case REL_GRPDISP:
			 r->target+=grpmin-1;
			 break;
		case REL_EXTDISP:
			 r->target+=extmin-1;
			 break;
		case REL_EXPFRAME:
			 break;
		case REL_SEGONLY:
			 r->target+=segmin-1;
			 break;
		case REL_GRPONLY:
			 r->target+=grpmin-1;
			 break;
		case REL_EXTONLY:
			 r->target+=extmin-1;
			 break;
		}
	}
	switch(r->ttype)
	{
	case REL_SEGDISP:
	case REL_GRPDISP:
	case REL_EXTDISP:
	case REL_EXPFRAME:
		 r->disp=buf[j]+buf[j+1]*256;
		 j+=2;
		 if(rectype==FIXUPP32)
		 {
			 r->disp+=(buf[j]+buf[j+1]*256)<<16;
			 j+=2;
		 }
		 break;
	default:
			break;
	}
	if((r->ftype==REL_TARGETFRAME)&&((r->ttype&FIX_THRED)==0))
	{
		switch(r->ttype)
		{
		case REL_SEGDISP:
		case REL_GRPDISP:
		case REL_EXTDISP:
		case REL_EXPFRAME:
			 r->ftype=r->ttype;
			 r->frame=r->target;
			 break;
		case REL_SEGONLY:
		case REL_GRPONLY:
		case REL_EXTONLY:
			 r->ftype=r->ttype-4;
			 r->frame=r->target;
			 break;
		}
	}

	*p=j;
}

long loadmod(FILE *objfile)
{
	long modpos;
	long done;
	long i,j,k;

	modpos=0;
	done=0;
	li_le=0;
	lidata=0;

	while(!done)
	{
		if(fread(buf,1,3,objfile)!=3)
		{
			ReportError(ERR_NO_MODEND);
		}
		rectype=buf[0];
		reclength=buf[1]+256*buf[2];
		if(fread(buf,1,reclength,afile)!=reclength)
		{
			ReportError(ERR_NO_RECDATA);
		}
		reclength--;
		if((!modpos)&&(rectype!=THEADR)&&(rectype!=LHEADR))
		{
			ReportError(ERR_NO_HEADER);
		}
		switch(rectype)
		{
		case THEADR:
		case LHEADR:
			 if(modpos)
			 {
				ReportError(ERR_EXTRA_HEADER);
			 }
			 modname[nummods]=malloc(buf[0]+1);
			 if(!modname[nummods])
			 {
				ReportError(ERR_NO_MEM);
			 }
			 for(i=0;i<buf[0];i++)
			 {
				modname[nummods][i]=buf[i+1];
			 }
			 modname[nummods][i]=0;
			 strupr(modname[nummods]);
			 printf("Loading module %s\n",modname[nummods]);
			 if((buf[0]+1)!=reclength)
			 {
				ReportError(ERR_EXTRA_DATA);
			 }
			 namemin=namecount;
			 segmin=segcount;
			 pubmin=pubcount;
			 extmin=extcount;
			 fixmin=fixcount;
			 grpmin=grpcount;
			 nummods++;
			 break;
		case COMENT:
			 li_le=0;
			 if(lidata)
			 {
				DestroyLIDATA(lidata);
				lidata=0;
			 }
			 if(reclength>=3)
			 {
				switch(buf[1])
				{
				case COMENT_TRANSLATOR:
				case COMENT_INTEL_COPYRIGHT:
					break;
				default:
					printf("COMENT Record (unknown type %02X)\n",buf[1]);
					break;
				}
			 }
			 break;
		case LNAMES:
			 j=0;
			 while(j<reclength)
			 {
				namelist[namecount]=malloc(buf[j]+1);
				if(!namelist[namecount])
				{
					ReportError(ERR_NO_MEM);
				}
				for(i=0;i<buf[j];i++)
				{
					namelist[namecount][i]=buf[j+i+1];
				}
				namelist[namecount][buf[j]]=0;
				if(!case_sensitive)
				{
					strupr(namelist[namecount]);
				}
				j+=buf[j]+1;
				namecount++;
			 }
			 break;
		case SEGDEF:
		case SEGDEF32:
			 seglist[segcount]=malloc(sizeof(SEG));
			 if(!seglist[segcount])
			 {
				ReportError(ERR_NO_MEM);
			 }
			 seglist[segcount]->attr=buf[0];
			 j=1;
			 if((seglist[segcount]->attr & SEG_ALIGN)==SEG_ABS)
			 {
				seglist[segcount]->absframe=buf[j]+256*buf[j+1];
				seglist[segcount]->absofs=buf[j+2];
				j+=3;
			 }
			 seglist[segcount]->length=buf[j]+256*buf[j+1];
			 j+=2;
			 if(rectype==SEGDEF32)
			 {
				seglist[segcount]->length+=(buf[j]+256*buf[j+1])<<16;
				j+=2;
			 }
			 if(seglist[segcount]->attr&SEG_BIG)
			 {
				if(rectype==SEGDEF)
				{
					seglist[segcount]->length+=65536;
				}
				else
				{
					if((seglist[segcount]->attr&SEG_ALIGN)!=SEG_ABS)
					{
						ReportError(ERR_SEG_TOO_LARGE);
					}
				}
			 }
			 seglist[segcount]->nameindex=GetIndex(buf,&j)-1;
			 seglist[segcount]->classindex=GetIndex(buf,&j)-1;
			 seglist[segcount]->overlayindex=GetIndex(buf,&j)-1;
			 if(seglist[segcount]->nameindex>=0)
			 {
				seglist[segcount]->nameindex+=namemin;
			 }
			 if(seglist[segcount]->classindex>=0)
			 {
				seglist[segcount]->classindex+=namemin;
			 }
			 if(seglist[segcount]->overlayindex>=0)
			 {
				seglist[segcount]->overlayindex+=namemin;
			 }
			 if((seglist[segcount]->attr&SEG_ALIGN)!=SEG_ABS)
			 {
				seglist[segcount]->data=malloc(seglist[segcount]->length);
				if(!seglist[segcount]->data)
				{
					ReportError(ERR_NO_MEM);
				}
				seglist[segcount]->datmask=malloc((seglist[segcount]->length+7)/8);
				if(!seglist[segcount]->datmask)
				{
					ReportError(ERR_NO_MEM);
				}
				for(i=0;i<(seglist[segcount]->length+7)/8;i++)
				{
					seglist[segcount]->datmask[i]=0;
				}
			 }
			 else
			 {
				seglist[segcount]->data=0;
				seglist[segcount]->datmask=0;
				seglist[segcount]->attr&=(0xffff-SEG_COMBINE);
				seglist[segcount]->attr|=SEG_PRIVATE;
			 }
			 switch(seglist[segcount]->attr&SEG_COMBINE)
			 {
			 case SEG_PRIVATE:
			 case SEG_PUBLIC:
			 case SEG_PUBLIC2:
			 case SEG_COMMON:
			 case SEG_PUBLIC3:
				  break;
			 case SEG_STACK:
				  /* stack segs are always byte aligned */
				  seglist[segcount]->attr&=(0xffff-SEG_ALIGN);
				  seglist[segcount]->attr|=SEG_BYTE;
				  break;
			 default:
					 ReportError(ERR_BAD_SEGDEF);
					 break;
			 }
			 if((seglist[segcount]->attr&SEG_ALIGN)==SEG_BADALIGN)
			 {
				ReportError(ERR_BAD_SEGDEF);
			 }
			 segcount++;
			 break;
		case LEDATA:
		case LEDATA32:
			 j=0;
			 prevseg=GetIndex(buf,&j)-1;
			 if(prevseg<0)
			 {
				ReportError(ERR_INV_SEG);
			 }
			 prevseg+=segmin;
			 if((seglist[prevseg]->attr&SEG_ALIGN)==SEG_ABS)
			 {
				ReportError(ERR_ABS_SEG);
			 }
			 prevofs=buf[j]+(buf[j+1]<<8);
			 j+=2;
			 if(rectype==LEDATA32)
			 {
				prevofs+=(buf[j]+(buf[j+1]<<8))<<16;
				j+=2;
			 }
			 for(k=0;j<reclength;j++,k++)
			 {
				if((prevofs+k)>=seglist[prevseg]->length)
				{
					ReportError(ERR_INV_DATA);
				}
				if(GetNbit(seglist[prevseg]->datmask,prevofs+k))
				{
					if(seglist[prevseg]->data[prevofs+k]!=buf[j])
					{
						printf("%08lX: %08lX: %i, %li,%li,%li\n",prevofs+k,j,GetNbit(seglist[prevseg]->datmask,prevofs+k),segcount,segmin,prevseg);
						ReportError(ERR_OVERWRITE);
					}
				}
				seglist[prevseg]->data[prevofs+k]=buf[j];
				SetNbit(seglist[prevseg]->datmask,prevofs+k);
			 }
			 li_le=PREV_LE;
			 break;
		case LIDATA:
		case LIDATA32:
			 if(lidata)
			 {
				DestroyLIDATA(lidata);
			 }
			 j=0;
			 prevseg=GetIndex(buf,&j)-1;
			 if(prevseg<0)
			 {
				ReportError(ERR_INV_SEG);
			 }
			 prevseg+=segmin;
			 if((seglist[prevseg]->attr&SEG_ALIGN)==SEG_ABS)
			 {
				ReportError(ERR_ABS_SEG);
			 }
			 prevofs=buf[j]+(buf[j+1]<<8);
			 j+=2;
			 if(rectype==LIDATA32)
			 {
				prevofs+=(buf[j]+(buf[j+1]<<8))<<16;
				j+=2;
			 }
			 lidata=malloc(sizeof(DATABLOCK));
			 if(!lidata)
			 {
				ReportError(ERR_NO_MEM);
			 }
			 lidata->data=malloc(sizeof(PDATABLOCK)*(1024/sizeof(DATABLOCK)+1));
			 if(!lidata->data)
			 {
				ReportError(ERR_NO_MEM);
			 }
			 lidata->blocks=0;
			 lidata->dataofs=j;
			 for(i=0;j<reclength;i++)
			 {
				((PPDATABLOCK)lidata->data)[i]=BuildLiData(&j);
			 }
			 lidata->blocks=i;
			 lidata->count=1;

			 k=prevofs;
			 EmitLiData(lidata,prevseg,&k);
			 li_le=(rectype==LIDATA)?PREV_LI:PREV_LI32;
			 break;
		case PUBDEF:
		case PUBDEF32:
			 publics[pubcount]=malloc(sizeof(PUBLIC));
			 if(!publics[pubcount])
			 {
				ReportError(ERR_NO_MEM);
			 }
			 j=0;
			 for(;j<reclength;j++)
			 {
				publics[pubcount]->grpnum=GetIndex(buf,&j)-1;
				if(publics[pubcount]->grpnum>=0)
				{

				}
				publics[pubcount]->segnum=GetIndex(buf,&j)-1;
				if(publics[pubcount]->segnum<0)
				{
					j+=2;
				}
				else
				{
					publics[pubcount]->segnum+=segmin;
				}
				publics[pubcount]->name=malloc(buf[j]+1);
				if(!publics[pubcount]->name)
				{
					ReportError(ERR_NO_MEM);
				}
				k=buf[j];
				j++;
				for(i=0;i<k;i++)
				{
					publics[pubcount]->name[i]=buf[j];
					j++;
				}
				publics[pubcount]->name[i]=0;
				if(!case_sensitive)
				{
					strupr(publics[pubcount]->name);
				}
				for(i=0;i<pubcount;i++)
				{
					if(!strcmp(publics[i]->name,publics[pubcount]->name))
					{
						ReportError(ERR_DUP_PUBLIC);
					}
				}
				publics[pubcount]->ofs=buf[j]+256*buf[j+1];
				j+=2;
				if(reclength==PUBDEF32)
				{
					publics[pubcount]->ofs+=(buf[j]+256*buf[j+1])<<16;
					j+=2;
				}
				publics[pubcount]->typenum=GetIndex(buf,&j);
				pubcount++;
			 }
			 break;
		case EXTDEF:
			 for(j=0;j<reclength;)
			 {
				externs[extcount]=malloc(sizeof(EXTREC));
				if(!externs[extcount])
				{
					ReportError(ERR_NO_MEM);
				}
				externs[extcount]->name=malloc(buf[j]+1);
				k=buf[j];
				j++;
				if(!externs[extcount]->name)
				{
					ReportError(ERR_NO_MEM);
				}
				for(i=0;i<k;i++,j++)
				{
					externs[extcount]->name[i]=buf[j];
				}
				externs[extcount]->name[i]=0;
				if(!case_sensitive)
				{
					strupr(externs[extcount]->name);
				}
				externs[extcount]->typenum=GetIndex(buf,&j);
				extcount++;
			 }
			 break;
		case GRPDEF:
			 printf("GRPDEF Record\n");
			 grplist[grpcount]=malloc(sizeof(GRP));
			 if(!grplist[grpcount])
			 {
				ReportError(ERR_NO_MEM);
			 }
			 j=0;
			 grplist[grpcount]->nameindex=GetIndex(buf,&j)-1;
			 if(grplist[grpcount]->nameindex<0)
			 {
				ReportError(ERR_BAD_GRPDEF);
			 }
			 printf("  Name: %s\n",namelist[grplist[grpcount]->nameindex]);
			 grplist[grpcount]->numsegs=0;
			 while(j<reclength)
			 {
				if(buf[j]==0xff)
				{
					j++;
					i=GetIndex(buf,&j)-1+segmin;
					if(i<segmin)
					{
						ReportError(ERR_BAD_GRPDEF);
					}
					printf("    Segment: %li:%s\n",i-segmin,namelist[seglist[i]->nameindex]);
					grplist[grpcount]->segindex[grplist[grpcount]->numsegs]=i;
					grplist[grpcount]->numsegs++;
				}
				else
				{
					ReportError(ERR_BAD_GRPDEF);
				}
			 }
			 grpcount++;
			 break;
		case FIXUPP:
		case FIXUPP32:
			 j=0;
			 if(!li_le)
			 {
				ReportError(ERR_BAD_FIXUP);
			 }
			 while(j<reclength)
			 {
				if(buf[j]&0x80)
				{
					relocs[fixcount]=malloc(sizeof(RELOC));
					if(!relocs[fixcount])
					{
						ReportError(ERR_NO_MEM);
					}
					relocs[fixcount]->rtype=(buf[j]>>2);
					relocs[fixcount]->ofs=buf[j]*256+buf[j+1];
					j+=2;
					relocs[fixcount]->ofs&=0x3ff;
					relocs[fixcount]->rtype^=FIX_SELFREL;
					relocs[fixcount]->rtype&=FIX_MASK;
					switch(relocs[fixcount]->rtype)
					{
					case FIX_LBYTE:
					case FIX_OFS16:
					case FIX_BASE:
					case FIX_PTR1616:
					case FIX_HBYTE:
					case FIX_OFS16_2:
					case FIX_OFS32:
					case FIX_PTR1632:
					case FIX_OFS32_2:
					case FIX_SELF_LBYTE:
					case FIX_SELF_OFS16:
					case FIX_SELF_OFS16_2:
					case FIX_SELF_OFS32:
					case FIX_SELF_OFS32_2:
						 break;
					default:
							ReportError(ERR_BAD_FIXUP);
					}

					LoadFIXUP(relocs[fixcount],buf,&j);

					if(li_le==PREV_LE)
					{
						relocs[fixcount]->ofs+=prevofs;
						relocs[fixcount]->segnum=prevseg;
						fixcount++;
					}
					else
					{
						relocs[fixcount]->segnum=prevseg;
						i=prevofs;
						RelocLIDATA(lidata,&i);
						free(relocs[fixcount]);
					}
				}
				else
				{
					printf("(FIXUPP) THRED subrecord\n");
					j++;
				}
			 }
			 break;
		case LINNUM:
			 printf("LINNUM record\n");
			 break;
		case MODEND:
		case MODEND32:
			 done=1;
			 if(buf[0]&0x40)
			 {
				if(gotstart)
				{
					ReportError(ERR_MULTIPLE_STARTS);
				}
				gotstart=1;
				j=1;
				LoadFIXUP(&startaddr,buf,&j);
				if(startaddr.ftype==REL_LILEFRAME)
				{
					ReportError(ERR_BAD_FIXUP);
				}
			 }
			 break;
		default:
				ReportError(ERR_UNKNOWN_RECTYPE);
		}
		filepos+=4+reclength;
		modpos+=4+reclength;
	}
	if(lidata)
	{
		DestroyLIDATA(lidata);
	}
	return 0;
}

void GetFixupTarget(PRELOC r,long *tseg,UINT *tofs)
{
	long baseseg;
	long targseg;
	UINT targofs;

	switch(r->ftype)
	{
	case REL_SEGFRAME:
	case REL_LILEFRAME:
		 baseseg=r->frame;
		 break;
	case REL_GRPFRAME:
		 baseseg=grplist[r->frame]->segnum;
		 break;
	case REL_EXTFRAME:
		 baseseg=publics[externs[r->frame]->pubnum]->segnum;
		 break;
	default:
			printf("Reloc:Unsupported FRAME type %i\n",r->ftype);
			errcount++;
	}
	switch(r->ttype)
	{
	case REL_EXTDISP:
		 targseg=publics[externs[r->target]->pubnum]->segnum;
		 targofs=publics[externs[r->target]->pubnum]->ofs;
		 targofs+=r->disp;
		 break;
	case REL_EXTONLY:
		 targseg=publics[externs[r->target]->pubnum]->segnum;
		 targofs=publics[externs[r->target]->pubnum]->ofs;
		 break;
	case REL_SEGONLY:
		 targseg=r->target;
		 targofs=0;
		 break;
	case REL_SEGDISP:
		 targseg=r->target;
		 targofs=r->disp;
		 break;
	case REL_GRPONLY:
		 targseg=grplist[r->target]->segnum;
		 targofs=0;
		 break;
	case REL_GRPDISP:
		 targseg=grplist[r->target]->segnum;
		 targofs=r->disp;
		 break;
	default:
			printf("Reloc:Unsupported TARGET type %i\n",r->ttype);
			errcount++;
	}
	if(!seglist[targseg])
	{
		errcount++;
	}
	if(!seglist[baseseg])
	{
		errcount++;
	}
	if(!errcount)
	{
/*
		if(((seglist[targseg]->attr&SEG_ALIGN)!=SEG_ABS) &&
		   ((seglist[baseseg]->attr&SEG_ALIGN)!=SEG_ABS))
		{
			if(seglist[baseseg]->base>seglist[targseg]->base)
			{
				printf("Reloc:Negative base address\n");
				errcount++;
			}
			targofs+=seglist[targseg]->base-seglist[baseseg]->base;
		}
*/		  
		if((seglist[baseseg]->attr&SEG_ALIGN)==SEG_ABS)
		{
			printf("Warning: Reloc frame is absolute segment\n");
			targseg=baseseg;
		}
		else if((seglist[targseg]->attr&SEG_ALIGN)==SEG_ABS)
		{
			printf("Warning: Reloc target is in absolute segment\n");
			targseg=baseseg;
		}
		if(seglist[baseseg]->base>(seglist[targseg]->base+targofs))
		{
			printf("Error target address out of frame\n");
			errcount++;
		}
		targofs+=seglist[targseg]->base-seglist[baseseg]->base;
		*tseg=baseseg;
		*tofs=targofs;
	}
	else
	{
		*tseg=0;
		*tofs=0;
	}
}


void OutputCOMfile(PCHAR outname)
{
	long i,j;
	UINT started;
	UINT lastout;
	long targseg;
	UINT targofs;
	FILE*outfile;

	errcount=0;

	if(gotstart)
	{
		printf("Start Address Present.\n");
		GetFixupTarget(&startaddr,&startaddr.segnum,&startaddr.ofs);
		if(errcount)
		{
			printf("Invalid start address record\n");
			exit(1);
		}

		printf("Start address = %s:%08lX = %08lX\n",namelist[seglist[startaddr.segnum]->nameindex],startaddr.ofs,startaddr.ofs+seglist[startaddr.segnum]->base);

		if((startaddr.ofs+seglist[startaddr.segnum]->base)!=0x100)
		{
			printf("Warning, start address not 0100h as required for COM file\n");
		}
	}
	else
	{
		printf("Warning, no entry point specified\n");
	}

	for(i=0;i<fixcount;i++)
	{
		GetFixupTarget(relocs[i],&targseg,&targofs);
		switch(relocs[i]->rtype)
		{
		case FIX_BASE:
		case FIX_PTR1616:
		case FIX_PTR1632:
			 if((seglist[targseg]->attr&SEG_ALIGN)!=SEG_ABS)
			 {
				printf("Reloc %li:Segment selector relocations are not supported in COM files\n",i);
				errcount++;
			 }
			 else
			 {
				j=relocs[i]->ofs;
				if(relocs[i]->rtype==FIX_PTR1616)
				{
					if(targofs>0xffff)
					{
						printf("Relocs %li:Offset out of range\n",i);
						errcount++;
					}
					*((unsigned short *)(&seglist[relocs[i]->segnum]->data[j]))+=targofs;
					j+=2;
				}
				else if(relocs[i]->rtype==FIX_PTR1632)
				{
					*((unsigned long *)(&seglist[relocs[i]->segnum]->data[j]))+=targofs;
					j+=4;
				}
				*((unsigned short *)(&seglist[relocs[i]->segnum]->data[j]))+=seglist[targseg]->absframe;
			 }			   
			 break;
		case FIX_OFS32:
		case FIX_OFS32_2:
			 *((unsigned long *)(&seglist[relocs[i]->segnum]->data[relocs[i]->ofs]))+=targofs;
			 break;
		case FIX_OFS16:
		case FIX_OFS16_2:
			 if(targofs>0xffff)
			 {
				 printf("Relocs %li:Offset out of range\n",i);
				 errcount++;
			 }
			 *((unsigned short *)(&seglist[relocs[i]->segnum]->data[relocs[i]->ofs]))+=targofs;
			 break;
		case FIX_LBYTE:
			 seglist[relocs[i]->segnum]->data[relocs[i]->ofs]+=targofs&0xff;
			 break;
		case FIX_HBYTE:
			 seglist[relocs[i]->segnum]->data[relocs[i]->ofs]+=(targofs>>8)&0xff;
			 break;
		case FIX_SELF_LBYTE:
			if((seglist[targseg]->attr&SEG_ALIGN)==SEG_ABS)
			{
				printf("Error: Absolute Reloc target not supported for self-relative fixups\n");
				errcount++;
			}
			else
			{
				j=seglist[targseg]->base+targofs;
				j-=seglist[relocs[i]->segnum]->base+relocs[i]->ofs;
				if((j<-128)||(j>127))
				{
					printf("Error: Reloc %li out of range\n",i);
				}
				else
				{
					seglist[relocs[i]->segnum]->data[relocs[i]->ofs]+=j;
				}
			}
			break;
		case FIX_SELF_OFS16:
		case FIX_SELF_OFS16_2:
			if((seglist[targseg]->attr&SEG_ALIGN)==SEG_ABS)
			{
				printf("Error: Absolute Reloc target not supported for self-relative fixups\n");
				errcount++;
			}
			else
			{
				j=seglist[targseg]->base+targofs;
				j-=seglist[relocs[i]->segnum]->base+relocs[i]->ofs;
				if((j<-32768)||(j>32767))
				{
					printf("Error: Reloc %li out of range\n",i);
				}
				else
				{
					*((unsigned short *)(&seglist[relocs[i]->segnum]->data[relocs[i]->ofs]))+=j;
				}
			}
			break;
		case FIX_SELF_OFS32:
		case FIX_SELF_OFS32_2:
			if((seglist[targseg]->attr&SEG_ALIGN)==SEG_ABS)
			{
				printf("Error: Absolute Reloc target not supported for self-relative fixups\n");
				errcount++;
			}
			else
			{
				j=seglist[targseg]->base+targofs;
				j-=seglist[relocs[i]->segnum]->base+relocs[i]->ofs;
				*((unsigned long *)(&seglist[relocs[i]->segnum]->data[relocs[i]->ofs]))+=j;
			}
			break;
		default:
				printf("Reloc %li:Relocation type %i not supported\n",i,relocs[i]->rtype);
				errcount++;
		}
	}

	if(errcount!=0)
	{
		exit(1);
	}
	outfile=fopen(outname,"wb");
	if(!outfile)
	{
		printf("Error writing to file %s\n",outname);
		exit(1);
	}

	started=lastout=0;

	for(i=0;i<outcount;i++)
	{
		printf("Emitting segment %s\n",namelist[outlist[i]->nameindex]);
		if(outlist[i] && ((outlist[i]->attr&SEG_ALIGN) !=SEG_ABS))
		{
			if(started>outlist[i]->base)
			{	
				printf("Segment overlap\n");
				fclose(outfile);
				exit(1);
			}
			if(padsegments)
			{
				while(started<outlist[i]->base)
				{
					fputc(0,outfile);
					started++;
				}
			}
			else
			{
				started=outlist[i]->base;
			}
			printf("Starting at %08lX\n",started);
			for(j=0;j<outlist[i]->length;j++)
			{
				if(started>=0x100)
				{
					if(GetNbit(outlist[i]->datmask,j))
					{
						for(;lastout<(started-1);lastout++)
						{
							fputc(0,outfile);
						}
						fputc(outlist[i]->data[j],outfile);
						lastout=started;
					}
					else if(padsegments)
					{
						fputc(0,outfile);
						lastout=started;
					}
				}
				else
				{
					lastout=started;
					if(GetNbit(outlist[i]->datmask,j))
					{
						printf("Warning - data at offset %08lX (%s:%08lX) discarded\n",started,namelist[outlist[i]->nameindex],j);
					}
				}
				started++;
			}
		}
	}

	fclose(outfile);
}

void OutputEXEfile(PCHAR outname)
{
	long i,j;
	UINT started,lastout;
	long targseg;
	UINT targofs;
	FILE*outfile;
	PUCHAR headbuf;
	long relcount;
	int gotstack;
	UINT totlength;

	errcount=0;
	gotstack=0;
	headbuf=malloc(0x40+4*fixcount);
	if(!headbuf)
	{
		ReportError(ERR_NO_MEM);
	}
	relcount=0;
	
	for(i=0;i<0x40;i++)
	{
		headbuf[i]=0;
	}
	
	headbuf[0x00]='M'; /* sig */
	headbuf[0x01]='Z';
	headbuf[0x0c]=maxalloc&0xff;
	headbuf[0x0d]=maxalloc>>8;
	headbuf[0x18]=0x40;

	if(gotstart)
	{
		printf("Start Address Present.\n");
		GetFixupTarget(&startaddr,&startaddr.segnum,&startaddr.ofs);
		if(errcount)
		{
			printf("Invalid start address record\n");
			exit(1);
		}

		printf("Start address = %s:%08lX = %08lX\n",namelist[seglist[startaddr.segnum]->nameindex],startaddr.ofs,startaddr.ofs+seglist[startaddr.segnum]->base);
		i=seglist[startaddr.segnum]->base;
		j=i&0xf;
		i>>=4;
		if((startaddr.ofs>65535)||(i>65535)||(j!=0)||((seglist[startaddr.segnum]->attr&SEG_ALIGN)==SEG_ABS))
		{
			printf("Invalid start address\n");
			errcount++;
		}
		else
		{
			headbuf[0x14]=startaddr.ofs&0xff;
			headbuf[0x15]=startaddr.ofs>>8;
			
			headbuf[0x16]=i&0xff;
			headbuf[0x17]=i>>8;
		}
	}
	else
	{
		printf("Warning, no entry point specified\n");
	}
	
	totlength=0;
	
	for(i=0;i<outcount;i++)
	{
		if((outlist[i]->attr&SEG_ALIGN)!=SEG_ABS)
		{
			totlength=outlist[i]->base+outlist[i]->length;	  
			if((outlist[i]->attr&SEG_COMBINE)==SEG_STACK)
			{
				if(gotstack)
				{
					printf("Internal error - stack segments not combined\n");
					exit(1);
				}
				gotstack=1;
				if((outlist[i]->length>65536)||(outlist[i]->length==0))
				{
					printf("SP value out of range\n");
					errcount++;
				}
				if((outlist[i]->base>0xfffff) || ((outlist[i]->base&0xf)!=0))
				{
					printf("SS value out of range\n");
					errcount++;
				}
				if(!errcount)
				{
					headbuf[0x0e]=(outlist[i]->base>>4)&0xff;
					headbuf[0x0f]=outlist[i]->base>>12;
					headbuf[0x10]=outlist[i]->length&0xff;
					headbuf[0x11]=(outlist[i]->length>>8)&0xff;
				}
			}
		}
	}
	
	if(!gotstack)
	{
		printf("Warning - no stack\n");
	}
	
	for(i=0;i<fixcount;i++)
	{
		GetFixupTarget(relocs[i],&targseg,&targofs);
		switch(relocs[i]->rtype)
		{
		case FIX_BASE:
		case FIX_PTR1616:
		case FIX_PTR1632:
			j=relocs[i]->ofs;
			if(relocs[i]->rtype==FIX_PTR1616)
			{
				if(targofs>0xffff)
				{
					printf("Relocs %li:Offset out of range\n",i);
					errcount++;
				}
				*((unsigned short *)(&seglist[relocs[i]->segnum]->data[j]))+=targofs;
				j+=2;
			}
			else if(relocs[i]->rtype==FIX_PTR1632)
			{
				*((unsigned long *)(&seglist[relocs[i]->segnum]->data[j]))+=targofs;
				j+=4;
			}
			if((seglist[targseg]->attr&SEG_ALIGN)!=SEG_ABS)
			{
				if(seglist[targseg]->base>0xfffff)
				{
					printf("Relocs %li:Segment base out of range\n",i);
					errcount++;
				}
				*((unsigned short *)(&seglist[relocs[i]->segnum]->data[j]))+=(seglist[targseg]->base>>4);
				*((unsigned long *)(&headbuf[0x40+relcount*4]))=seglist[relocs[i]->segnum]->base+j;
				relcount++;
			}
			else
			{
				*((unsigned short *)(&seglist[relocs[i]->segnum]->data[j]))+=seglist[targseg]->absframe;
			}			  
			break;
		case FIX_OFS32:
		case FIX_OFS32_2:
			 *((unsigned long *)(&seglist[relocs[i]->segnum]->data[relocs[i]->ofs]))+=targofs;
			 break;
		case FIX_OFS16:
		case FIX_OFS16_2:
			 if(targofs>0xffff)
			 {
				 printf("Relocs %li:Offset out of range\n",i);
				 errcount++;
			 }
			 *((unsigned short *)(&seglist[relocs[i]->segnum]->data[relocs[i]->ofs]))+=targofs;
			 break;
		case FIX_LBYTE:
			 seglist[relocs[i]->segnum]->data[relocs[i]->ofs]+=targofs&0xff;
			 break;
		case FIX_HBYTE:
			 seglist[relocs[i]->segnum]->data[relocs[i]->ofs]+=(targofs>>8)&0xff;
			 break;
		case FIX_SELF_LBYTE:
			if((seglist[targseg]->attr&SEG_ALIGN)==SEG_ABS)
			{
				printf("Error: Absolute Reloc target not supported for self-relative fixups\n");
				errcount++;
			}
			else
			{
				j=seglist[targseg]->base+targofs;
				j-=seglist[relocs[i]->segnum]->base+relocs[i]->ofs;
				if((j<-128)||(j>127))
				{
					printf("Error: Reloc %li out of range\n",i);
				}
				else
				{
					seglist[relocs[i]->segnum]->data[relocs[i]->ofs]+=j;
				}
			}
			break;
		case FIX_SELF_OFS16:
		case FIX_SELF_OFS16_2:
			if((seglist[targseg]->attr&SEG_ALIGN)==SEG_ABS)
			{
				printf("Error: Absolute Reloc target not supported for self-relative fixups\n");
				errcount++;
			}
			else
			{
				j=seglist[targseg]->base+targofs;
				j-=seglist[relocs[i]->segnum]->base+relocs[i]->ofs;
				if((j<-32768)||(j>32767))
				{
					printf("Error: Reloc %li out of range\n",i);
				}
				else
				{
					*((unsigned short *)(&seglist[relocs[i]->segnum]->data[relocs[i]->ofs]))+=j;
				}
			}
			break;
		case FIX_SELF_OFS32:
		case FIX_SELF_OFS32_2:
			if((seglist[targseg]->attr&SEG_ALIGN)==SEG_ABS)
			{
				printf("Error: Absolute Reloc target not supported for self-relative fixups\n");
				errcount++;
			}
			else
			{
				j=seglist[targseg]->base+targofs;
				j-=seglist[relocs[i]->segnum]->base+relocs[i]->ofs;
				*((unsigned long *)(&seglist[relocs[i]->segnum]->data[relocs[i]->ofs]))+=j;
			}
			break;
		default:
				printf("Reloc %li:Relocation type %i not supported\n",i,relocs[i]->rtype);
				errcount++;
		}
	}
	
	if(relcount>65535)
	{
		printf("Too many relocations\n");
		exit(1);
	}	 
	
	headbuf[0x06]=relcount&0xff;
	headbuf[0x07]=relcount>>8;
	i=relcount*4+0x4f;
	i>>=4;
	totlength+=i<<4;
	headbuf[0x08]=i&0xff;
	headbuf[0x09]=i>>8;
	i=totlength%512;
	headbuf[0x02]=i&0xff;
	headbuf[0x03]=i>>8;
	i=(totlength+0x1ff)>>9;
	if(i>65535)
	{
		printf("File too large\n");
		exit(1);
	}
	headbuf[0x04]=i&0xff;
	headbuf[0x05]=i>>8;
	

	if(errcount!=0)
	{
		exit(1);
	}
	outfile=fopen(outname,"wb");
	if(!outfile)
	{
		printf("Error writing to file %s\n",outname);
		exit(1);
	}
	
	i=(headbuf[0x08]+256*headbuf[0x09])*16;
	if(fwrite(headbuf,1,i,outfile)!=i)
	{
		printf("Error writing to file %s\n",outname);
		exit(1);
	}

	started=0;
	lastout=0;

	for(i=0;i<outcount;i++)
	{
		printf("Emitting segment %s\n",namelist[outlist[i]->nameindex]);
		if(outlist[i] && ((outlist[i]->attr&SEG_ALIGN) !=SEG_ABS))
		{
			if(started>outlist[i]->base)
			{	
				printf("Segment overlap\n");
				fclose(outfile);
				exit(1);
			}
			if(padsegments)
			{
				while(started<outlist[i]->base)
				{
					fputc(0,outfile);
					started++;
				}
			}
			else
			{
				started=outlist[i]->base;
			}
			printf("Starting at %08lX\n",started);
			for(j=0;j<outlist[i]->length;j++)
			{
				if(GetNbit(outlist[i]->datmask,j))
				{
					for(;lastout<started;lastout++)
					{
						fputc(0,outfile);
					}
					fputc(outlist[i]->data[j],outfile);
					lastout=started+1;
				}
				else if(padsegments)
				{
					fputc(0,outfile);
					lastout=started+1;
				}
				started++;
			}
		}
	}
	
	printf("Totlength: %08lX Started: %08lX Lastout: %08lX\n",totlength,started,lastout);

	if(lastout!=started)
	{
		fseek(outfile,0,SEEK_SET);
		lastout+=(headbuf[8]+256*headbuf[9])<<4;
		i=lastout%512;
		headbuf[0x02]=i&0xff;
		headbuf[0x03]=i>>8;
		i=(lastout+0x1ff)>>9;
		headbuf[0x04]=i&0xff;
		headbuf[0x05]=i>>8;	 
		i=((totlength-lastout)+0xf)>>4;
		if(i>65535)
		{
			printf("Memory requirements too high\n");
		}
		headbuf[0x0a]=i&0xff;
		headbuf[0x0b]=i>>8;
		if(fwrite(headbuf,1,12,outfile)!=12)
		{
			printf("Error writing to file\n");
			exit(1);
		}
	}
	fclose(outfile);
}

void combine_segments(long i,long j)
{
	UINT k,n;
	PUCHAR p,q;

	k=seglist[i]->length;
	switch(seglist[i]->attr&SEG_ALIGN)
	{
	case SEG_WORD:
		 k=(k+1)&0xfffffffe;
		 break;
	case SEG_PARA:
		 k=(k+0xf)&0xfffffff0;
		 break;
	case SEG_PAGE:
		 k=(k+0xff)&0xffffff00;
		 break;
	case SEG_DWORD:
		 k=(k+3)&0xfffffffc;
		 break;
	case SEG_MEMPAGE:
		 k=(k+0xfff)&0xfffff000;
		 break;
	default:
			break;
	}
	seglist[j]->base=k;
	p=malloc(k+seglist[j]->length);
	if(!p)
	{
		ReportError(ERR_NO_MEM);
	}
	q=malloc((k+seglist[j]->length+7)/8);
	if(!q)
	{
		ReportError(ERR_NO_MEM);
	}
	for(k=0;k<seglist[i]->length;k++)
	{
		if(GetNbit(seglist[i]->datmask,k))
		{
			SetNbit(q,k);
			p[k]=seglist[i]->data[k];
		}
		else
		{
			ClearNbit(q,k);
		}
	}
	for(;k<seglist[j]->base;k++)
	{
		ClearNbit(q,k);
	}
	for(;k<(seglist[j]->base+seglist[j]->length);k++)
	{
		if(GetNbit(seglist[j]->datmask,k-seglist[j]->base))
		{
			p[k]=seglist[j]->data[k-seglist[j]->base];
			SetNbit(q,k);
		}
		else
		{
			ClearNbit(q,k);
		}
	}
	seglist[i]->length=k;
	free(seglist[i]->data);
	free(seglist[j]->data);
	free(seglist[i]->datmask);
	free(seglist[j]->datmask);
	seglist[i]->data=p;
	seglist[i]->datmask=q;	 

	for(k=0;k<pubcount;k++)
	{
		if(publics[k]->segnum==j)
		{
			publics[k]->segnum=i;
			publics[k]->ofs+=seglist[j]->base;
		}
	}
	for(k=0;k<fixcount;k++)
	{
		if(relocs[k]->segnum==j)
		{
			relocs[k]->segnum=i;
			relocs[k]->ofs+=seglist[j]->base;
		}
		if(relocs[k]->ttype==REL_SEGDISP)
		{
			if(relocs[k]->target==j)
			{
				relocs[k]->target=i;
				relocs[k]->disp+=seglist[j]->base;
			}
		}
		else if(relocs[k]->ttype==REL_SEGONLY)
		{
			if(relocs[k]->target==j)
			{
				relocs[k]->target=i;
			}
		}
		if((relocs[k]->ftype==REL_SEGFRAME) ||
		  (relocs[k]->ftype==REL_LILEFRAME))
		{
			if(relocs[k]->frame==j)
			{
				relocs[k]->frame=i;
			}
		}
	}

	if(gotstart)
	{
		if(startaddr.ttype==REL_SEGDISP)
		{
			if(startaddr.target==j)
			{
				startaddr.target=i;
				startaddr.disp+=seglist[j]->base;
			}
		}
		else if(startaddr.ttype==REL_SEGONLY)
		{
			if(startaddr.target==j)
			{
				startaddr.target=i;
			}
		}
		if((startaddr.ftype==REL_SEGFRAME) ||
		  (startaddr.ftype==REL_LILEFRAME))
		{
			if(startaddr.frame==j)
			{
				startaddr.frame=i;
			}
		}
	}

	for(k=0;k<grpcount;k++)
	{
		if(grplist[k])
		{
			for(n=0;n<grplist[k]->numsegs;n++)
			{
				if(grplist[k]->segindex[n]==j)
				{
					grplist[k]->segindex[n]=i;
				}
			}
		}
	}

	free(seglist[j]);
	seglist[j]=0;
}

void combine_common(long i,long j)
{
	UINT k,n;
	PUCHAR p,q;

	if(seglist[j]->length>seglist[i]->length)
	{
		k=seglist[i]->length;
		seglist[i]->length=seglist[j]->length;
		seglist[j]->length=k;
		p=seglist[i]->data;
		q=seglist[i]->datmask;
		seglist[i]->data=seglist[j]->data;
		seglist[i]->datmask=seglist[j]->datmask;
	}
	else
	{
		p=seglist[j]->data;
		q=seglist[j]->datmask;
	}
	for(k=0;k<seglist[j]->length;k++)
	{
		if(GetNbit(q,k))
		{
			if(GetNbit(seglist[i]->datmask,k))
			{
				if(seglist[i]->data[k]!=p[k])
				{
					ReportError(ERR_OVERWRITE);
				}
			}
			else
			{
				SetNbit(seglist[i]->datmask,k);
				seglist[i]->data[k]=p[k];
			}
		}
	}
	free(p);
	free(q);

	for(k=0;k<pubcount;k++)
	{
		if(publics[k]->segnum==j)
		{
			publics[k]->segnum=i;
		}
	}
	for(k=0;k<fixcount;k++)
	{
		if(relocs[k]->segnum==j)
		{
			relocs[k]->segnum=i;
		}
		if(relocs[k]->ttype==REL_SEGDISP)
		{
			if(relocs[k]->target==j)
			{
				relocs[k]->target=i;
			}
		}
		else if(relocs[k]->ttype==REL_SEGONLY)
		{
			if(relocs[k]->target==j)
			{
				relocs[k]->target=i;
			}
		}
		if((relocs[k]->ftype==REL_SEGFRAME) ||
		  (relocs[k]->ftype==REL_LILEFRAME))
		{
			if(relocs[k]->frame==j)
			{
				relocs[k]->frame=i;
			}
		}
	}

	if(gotstart)
	{
		if(startaddr.ttype==REL_SEGDISP)
		{
			if(startaddr.target==j)
			{
				startaddr.target=i;
			}
		}
		else if(startaddr.ttype==REL_SEGONLY)
		{
			if(startaddr.target==j)
			{
				startaddr.target=i;
			}
		}
		if((startaddr.ftype==REL_SEGFRAME) ||
		  (startaddr.ftype==REL_LILEFRAME))
		{
			if(startaddr.frame==j)
			{
				startaddr.frame=i;
			}
		}
	}

	for(k=0;k<grpcount;k++)
	{
		if(grplist[k])
		{
			for(n=0;n<grplist[k]->numsegs;n++)
			{
				if(grplist[k]->segindex[n]==j)
				{
					grplist[k]->segindex[n]=i;
				}
			}
		}
	}

	free(seglist[j]);
	seglist[j]=0;
}

void combine_groups(long i,long j)
{
	long n,m;
	char match;
	
	for(n=0;n<grplist[j]->numsegs;n++)
	{
		match=0;
		for(m=0;m<grplist[i]->numsegs;m++)
		{
			if(grplist[j]->segindex[n]==grplist[i]->segindex[m])
			{
				match=1;
			}
		}
		if(!match)
		{
			grplist[i]->numsegs++;
			grplist[i]->segindex[grplist[i]->numsegs]=grplist[j]->segindex[n];
		}
	}
	free(grplist[j]);
	grplist[j]=0;
	
	for(n=0;n<pubcount;n++)
	{
		if(publics[n]->grpnum==j)
		{
			publics[n]->grpnum=i;
		}
	}
	
	for(n=0;n<fixcount;n++)
	{
		if(relocs[n]->ftype==REL_GRPFRAME)
		{
			if(relocs[n]->frame==j)
			{
				relocs[n]->frame=i;
			}
		}
		if((relocs[n]->ttype==REL_GRPONLY) || (relocs[n]->ttype==REL_GRPDISP))
		{
			if(relocs[n]->target==j)
			{
				relocs[n]->target=i;
			}
		}
	}
	
	if(gotstart)
	{
		if((startaddr.ttype==REL_GRPDISP) || (startaddr.ttype==REL_GRPONLY))
		{
			if(startaddr.target==j)
			{
				startaddr.target=i;
			}
		}
		if(startaddr.ftype==REL_GRPFRAME)
		{
			if(startaddr.frame==j)
			{
				startaddr.frame=i;
			}
		}
	}
}

long main(long argc,char *argv[])
{
		long i,j,k;

		printf("ALINK v0.01 (C) Copyright 1998 Anthony A.J. Williams.\n");
		printf("All Rights Reserved\n\n");

		namemin=namecount=0;
		segmin=segcount=0;
		grpmin=grpcount=0;
		pubmin=pubcount=0;
		extmin=extcount=0;
		fixmin=fixcount=0;
		nummods=0;
		filecount=0;

		gotstart=0;

		case_sensitive=1;
		maxalloc=0xffff;
		padsegments=0;
		outname=0;
		
		output_type=OUTPUT_EXE;

		for(i=1;i<argc;i++)
		{
			if(argv[i][0]==SWITCHCHAR)
			{
				if(strlen(argv[i])<2)
				{
					printf("Invalid argument \"%s\"\n",argv[i]);
					exit(1);
				}
				switch(argv[i][1])
				{
				case 'c':
					if(strlen(argv[i])==2)
					{
						case_sensitive=1;
						break;
					}
					else if(strlen(argv[i])==3)
					{
						if(argv[i][2]=='+')
						{
							case_sensitive=1;
							break;
						}
						else if(argv[i][2]=='-')
						{
							case_sensitive=0;
							break;
						}
					}
					printf("Invalid switch %s\n",argv[i]);
					exit(1);
					break;
				case 'p':
					switch(strlen(argv[i]))
					{
					case 2:
						padsegments=1;
						break;
					case 3:
						if(argv[i][2]=='+')
						{
							padsegments=1;
							break;
						}
						else if(argv[i][2]=='-')
						{
							padsegments=0;
							break;
						}
					default:
						printf("Invalid switch %s\n",argv[i]);
						exit(1);
					}
					break;
				case 'o':
					switch(strlen(argv[i]))
					{
					case 2:
						if(i<(argc-1))
						{
							i++;
							if(!outname)
							{
								outname=malloc(strlen(argv[i])+1);
								if(!outname)
								{
									ReportError(ERR_NO_MEM);
								}
								strcpy(outname,argv[i]);
							}
							else
							{
								printf("Can't specify two output names\n");
								exit(1);
							}
						}
						else
						{
							printf("Invalid switch %s\n",argv[i]);
							exit(1);
						}
						break;
					case 5:
						if(!strcmp(&(argv[i][2]),"EXE"))
						{
							output_type=OUTPUT_EXE;
						}
						else if(!strcmp(&(argv[i][2]),"COM"))
						{
							output_type=OUTPUT_COM;
						}
						else
						{
							printf("Invalid switch %s\n",argv[i]);
							exit(1);
						}
						break;
					default:
						printf("Invalid switch %s\n",argv[i]);
						exit(1);
					}
					break;
				default:
					printf("Invalid switch %s\n",argv[i]);
					exit(1);
				}
			}
			else
			{
				filename[filecount]=malloc(strlen(argv[i])+1);
				if(!filename[filecount])
				{
					printf("Insufficient memory\n");
					exit(1);
				}
				memcpy(filename[filecount],argv[i],strlen(argv[i])+1);
				filecount++;
			}
		}

		if(!filecount)
		{
			printf("No files specified\n");
			exit(1);
		}

		if(!outname)
		{
			outname=malloc(strlen(filename[0])+1+4);
			if(!outname)
			{
				ReportError(ERR_NO_MEM);
			}
			strcpy(outname,filename[0]);
			i=strlen(outname);
			while((i>=0)&&(outname[i]!='.')&&(outname[i]!=PATH_CHAR)&&(outname[i]!=':'))
			{
				i--;
			}
			if(outname[i]!='.')
			{
				i=strlen(outname);
			}
			switch(output_type)
			{
			case OUTPUT_EXE:
				strcpy(&outname[i],".EXE");
				break;
			case OUTPUT_COM:
				strcpy(&outname[i],".COM");
				break;
			default:
				break;
			}
		}

		for(i=0;i<filecount;i++)
		{
			afile=fopen(filename[i],"rb");
			if(!afile)
			{
				printf("Error opening file %s\n",filename[i]);
				exit(1);
			}
			filepos=0;
			printf("Loading file %s\n",filename[i]);
			loadmod(afile);
			fclose(afile);
		}

		for(i=0;i<extcount;i++)
		{
			externs[i]->pubnum=-1;
			for(j=0;j<pubcount;j++)
			{
				if(!strcmp(externs[i]->name,publics[j]->name))
				{
					externs[i]->pubnum=j;
				}
			}
			if(externs[i]->pubnum<0)
			{
				printf("Unresolved external %s\n",externs[i]->name);
				errcount++;
			}
		}

		if(errcount!=0)
		{
			exit(1);
		}
		
		for(i=0;i<segcount;i++)
		{
			if(seglist[i]&&((seglist[i]->attr&SEG_ALIGN)!=SEG_ABS))
			{
				switch(seglist[i]->attr&SEG_COMBINE)
				{
				case SEG_PUBLIC:
				case SEG_PUBLIC2:
				case SEG_STACK:
				case SEG_PUBLIC3:
					 for(j=i+1;j<segcount;j++)
					 {
						if((seglist[j]&&((seglist[j]->attr&SEG_ALIGN)!=SEG_ABS)) &&
						  (
							((seglist[i]->attr&SEG_COMBINE)==SEG_STACK) ||
							((seglist[i]->attr&(SEG_ALIGN|SEG_COMBINE|SEG_USE32))==(seglist[j]->attr&(SEG_ALIGN|SEG_COMBINE|SEG_USE32)))
						  ) &&
						  (strcmp(namelist[seglist[i]->nameindex],namelist[seglist[j]->nameindex])==0)
						  )
						{
							combine_segments(i,j);
						}
					 }
					 break;
				case SEG_COMMON:
					 for(j=i+1;j<segcount;j++)
					 {
						if((seglist[j]&&((seglist[j]->attr&SEG_ALIGN)!=SEG_ABS)) &&
						  ((seglist[i]->attr&(SEG_ALIGN|SEG_COMBINE|SEG_USE32))==(seglist[j]->attr&(SEG_ALIGN|SEG_COMBINE|SEG_USE32)))
						  &&
						  (strcmp(namelist[seglist[i]->nameindex],namelist[seglist[j]->nameindex])==0)
						  )
						{
							combine_common(i,j);
						}
					 }
					 break;
				default:
						break;
				}
			}
		}
		
		for(i=0;i<grpcount;i++)
		{
			if(grplist[i])
			{
				for(j=i+1;j<grpcount;j++)
				{
					if(strcmp(namelist[grplist[i]->nameindex],namelist[grplist[j]->nameindex])==0)
					{
						combine_groups(i,j);
					}
				}
			}
		}
		
		for(i=0;i<segcount;i++)
		{
			if(seglist[i])
			{
				if((seglist[i]->attr&SEG_ALIGN)!=SEG_ABS)
				{
					seglist[i]->absframe=0;
				}
			}
		}

		outcount=0;
		outlist=malloc(sizeof(PSEG)*segcount);
		if(!outlist)
		{
			ReportError(ERR_NO_MEM);
		}
		for(i=0;i<grpcount;i++)
		{
			if(grplist[i])
			{
				grplist[i]->segnum=-1;
				for(j=0;j<grplist[i]->numsegs;j++)
				{
					k=grplist[i]->segindex[j];
					if(!seglist[k])
					{
						printf("Error - group %s contains non-existent segment\n",namelist[grplist[i]->nameindex]);
						exit(1);
					}
					if((seglist[k]->attr&SEG_ALIGN)!=SEG_ABS)
					{
						if(seglist[k]->absframe!=0)
						{
							printf("Error - Segment %s part of more than one group\n",namelist[seglist[k]->nameindex]);
							exit(1);
						}
						seglist[k]->absframe=1;
						seglist[k]->absofs=i+1;
						if(grplist[i]->segnum<0)
						{
							grplist[i]->segnum=k;
						}
						outlist[outcount]=seglist[k];
						outcount++;
					}
				}
			}
		}

		for(i=0;i<segcount;i++)
		{
			if(seglist[i])
			{
				if((seglist[i]->attr&SEG_ALIGN)!=SEG_ABS)
				{
					if(!seglist[i]->absframe)
					{
						seglist[i]->absframe=1;
						seglist[i]->absofs=0;
						outlist[outcount]=seglist[i];
						outcount++;
					}
				}
				else
				{
					seglist[i]->base=(seglist[i]->absframe<<4)+seglist[i]->absofs;
				}
			}
		}
		
		j=0;
		k=0;
		for(i=0;i<outcount;i++)
		{
			if(outlist[i])
			{
				switch(outlist[i]->attr&SEG_ALIGN)
				{
				case SEG_WORD:
				case SEG_BYTE:
				case SEG_DWORD:
				case SEG_PARA:
					 j=(j+0xf)&0xfffffff0;
					 break;
				case SEG_PAGE:
					 j=(j+0xff)&0xffffff00;
					 break;
				case SEG_MEMPAGE:
					 j=(j+0xfff)&0xfffff000;
					 break;
				default:
						break;
				}
				outlist[i]->base=j;
				j+=outlist[i]->length;
			}
		}

		printf("\n %li segments:\n",segcount);
		for(i=0;i<segcount;i++)
		{
		if(seglist[i])
		{
			printf("SEGMENT %s ",namelist[seglist[i]->nameindex]);
			switch(seglist[i]->attr&SEG_COMBINE)
			{
			case SEG_PRIVATE:
				 printf("PRIVATE ");
				 break;
			case SEG_PUBLIC:
				 printf("PUBLIC ");
				 break;
			case SEG_PUBLIC2:
				 printf("PUBLIC(2) ");
				 break;
			case SEG_STACK:
				 printf("STACK ");
				 break;
			case SEG_COMMON:
				 printf("COMMON ");
				 break;
			case SEG_PUBLIC3:
				 printf("PUBLIC(3) ");
				 break;
			default:
				 printf("unknown ");
				 break;
			}
			if(seglist[i]->attr&SEG_USE32)
			{
				printf("USE32 ");
			}
			else
			{
				printf("USE16 ");
			}
			switch(seglist[i]->attr&SEG_ALIGN)
			{
			case SEG_ABS:
				 printf("AT 0%04lXh ",seglist[i]->absframe);
				 break;
			case SEG_BYTE:
				 printf("BYTE ");
				 break;
			case SEG_WORD:
				 printf("WORD ");
				 break;
			case SEG_PARA:
				 printf("PARA ");
				 break;
			case SEG_PAGE:
				 printf("PAGE ");
				 break;
			case SEG_DWORD:
				 printf("DWORD ");
				 break;
			case SEG_MEMPAGE:
				 printf("MEMPAGE ");
				 break;
			default:
				 printf("unknown ");
			}
			printf("'%s'\n",namelist[seglist[i]->classindex]);
			printf("  at %08lX, length %08lX\n",seglist[i]->base,seglist[i]->length);
		}
		}

		printf("\n %li publics:\n",pubcount);
		for(i=0;i<pubcount;i++)
		{
			printf("%s at %s:%08lX\n",
				publics[i]->name,
				(publics[i]->segnum>=0) ? namelist[seglist[publics[i]->segnum]->nameindex] : "Absolute",
				publics[i]->ofs);
		}

		printf("\n %li externals:\n",extcount);
		for(i=0;i<extcount;i++)
		{
			printf("%s\n",externs[i]->name);
		}

/*
		printf("\n %i relocations\n",fixcount);
		for(i=0;i<fixcount;i++)
		{
			switch(relocs[i]->rtype&0xf)
			{
			case 0:
				 printf("Low order offset byte ");
				 break;
			case 1:
				 printf("16-bit offset ");
				 break;
			case 2:
				 printf("16-bit segment number ");
				 break;
			case 3:
				 printf("16:16 pointer ");
				 break;
			case 4:
				 printf("High order byte of 16bit offset ");
				 break;
			case 5:
				 printf("16-bit offset(2) ");
				 break;
			case 9:
				 printf("32-bit offset ");
				 break;
			case 11:
				 printf("16:32 pointer ");
				 break;
			case 13:
				 printf("32-bit offset(2) ");
				 break;
			default:
			}
			printf("at %s:%08X\n",namelist[seglist[relocs[i]->segnum]->nameindex],relocs[i]->ofs);
		}
*/
		switch(output_type)
		{
		case OUTPUT_COM:
			OutputCOMfile(outname);
			break;
		case OUTPUT_EXE:
			OutputEXEfile(outname);
			break;
		default:
			printf("Invalid output type\n");
			exit(1);
			break;
		}
		return 0;
}
