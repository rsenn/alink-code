#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <limits.h>

#ifndef __GCC__
#define __inline__
#endif

#include "alink.h"

char case_sensitive=1;
char padsegments=0;
char mapfile=0;
PCHAR mapname=0;
unsigned short maxalloc=0xffff;
int output_type=OUTPUT_EXE;
PCHAR outname=0;

UINT max_segs=DEF_SEG_COUNT,
	max_names=DEF_NAME_COUNT,
	max_grps=DEF_GRP_COUNT,
	max_relocs=DEF_RELOC_COUNT,
	max_imports=DEF_IMP_COUNT,
	max_exports=DEF_EXP_COUNT,
	max_publics=DEF_PUB_COUNT,
	max_externs=DEF_EXT_COUNT;

FILE *afile=0;
UINT filepos=0;
long reclength=0;
unsigned char rectype=0;
char li_le=0;
UINT prevofs=0;
long prevseg=0;
long gotstart=0;
RELOC startaddr;

long errcount=0;

unsigned char buf[65536];
PDATABLOCK lidata;

PPCHAR namelist;
PPSEG seglist;
PPSEG outlist;
PPGRP grplist;
PPPUBLIC publics;
PPEXTREC externs;
PPRELOC relocs;
PPIMPREC impdefs;
PPEXPREC expdefs;
PPLIBFILE libfiles;
PCHAR modname[256];
PCHAR filename[256];
UINT namecount=0,namemin=0,
	segcount=0,segmin=0,outcount=0,
	grpcount=0,grpmin=0,
	pubcount=0,pubmin=0,
	extcount=0,extmin=0,
	fixcount=0,fixmin=0,
	impcount=0,impmin=0,impsreq=0,
	expcount=0,expmin=0,
	nummods=0,
	filecount=0,
	libcount=0;
	
__inline__ void ClearNbit(PUCHAR mask,long i)
{
	mask[i/8]&=!(1<<(i%8));
}

__inline__ void SetNbit(PUCHAR mask,long i)
{
	mask[i/8]|=(1<<(i%8));
}

__inline__ char GetNbit(PUCHAR mask,long i)
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
		case ERR_INVALID_COMENT:
			printf(" - COMENT record format invalid\n");
			break;
		case ERR_ILLEGAL_IMPORTS:
			printf(" - Imports required to link, and not supported by output file type\n");
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
			 impmin=impcount;
			 expmin=expcount;
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
				case COMENT_LIB_SPEC:
				case COMENT_DEFLIB:
					break;
				case COMENT_OMFEXT:
					if(reclength<4)
					{
						ReportError(ERR_INVALID_COMENT);
					}
					switch(buf[2])
					{
					case EXT_IMPDEF:
						j=4;
						if(reclength<(j+4))
						{
							ReportError(ERR_INVALID_COMENT);
						}
						impdefs[impcount]=malloc(sizeof(IMPREC));
						if(!impdefs[impcount])
						{
							ReportError(ERR_NO_MEM);
						}
						impdefs[impcount]->flags=buf[3];
						impdefs[impcount]->int_name=malloc(buf[j]+1);
						if(!impdefs[impcount]->int_name)
						{
							ReportError(ERR_NO_MEM);
						}
						for(i=0;i<buf[j];i++)
						{
							impdefs[impcount]->int_name[i]=buf[j+i+1];
						}
						j+=buf[j]+1;
						impdefs[impcount]->int_name[i]=0;
						if(!case_sensitive)
						{
							strupr(impdefs[impcount]->int_name);
						}
						impdefs[impcount]->mod_name=malloc(buf[j]+1);
						if(!impdefs[impcount]->mod_name)
						{
							ReportError(ERR_NO_MEM);
						}
						for(i=0;i<buf[j];i++)
						{
							impdefs[impcount]->mod_name[i]=buf[j+i+1];
						}
						j+=buf[j]+1;
						impdefs[impcount]->mod_name[i]=0;
						if(!case_sensitive)
						{
							strupr(impdefs[impcount]->mod_name);
						}
						if(impdefs[impcount]->flags)
						{
							impdefs[impcount]->ordinal=buf[j]+256*buf[j+1];
							j+=2;
						}
						else
						{
							if(buf[j])
							{
								impdefs[impcount]->imp_name=malloc(buf[j]+1);
								if(!impdefs[impcount]->imp_name)
								{
									ReportError(ERR_NO_MEM);
								}
								for(i=0;i<buf[j];i++)
								{
									impdefs[impcount]->imp_name[i]=buf[j+i+1];
								}
								j+=buf[j]+1;
								impdefs[impcount]->imp_name[i]=0;
								if(!case_sensitive)
								{
									strupr(impdefs[impcount]->imp_name);
								}
							}
							else
							{
								impdefs[impcount]->imp_name=malloc(strlen(impdefs[impcount]->int_name)+1);
								if(!impdefs[impcount]->imp_name)
								{
									ReportError(ERR_NO_MEM);
								}
								strcpy(impdefs[impcount]->imp_name,impdefs[impcount]->int_name); 
							}
						}
						impcount++;						   
						break;
					case EXT_EXPDEF:
						expdefs[expcount]=malloc(sizeof(EXPREC));
						if(!expdefs[expcount])
						{
							ReportError(ERR_NO_MEM);
						}
						j=4;
						expdefs[expcount]->flags=buf[3];
						expdefs[expcount]->exp_name=malloc(buf[j]+1);
						if(!expdefs[expcount]->exp_name)
						{
							ReportError(ERR_NO_MEM);
						}
						for(i=0;i<buf[j];i++)
						{
							expdefs[expcount]->exp_name[i]=buf[j+i+1];
						}
						expdefs[expcount]->exp_name[buf[j]]=0;
						if(!case_sensitive)
						{
							strupr(expdefs[expcount]->exp_name);
						}
						j+=buf[j]+1;
						if(buf[j])
						{
							expdefs[expcount]->int_name=malloc(buf[j]+1);
							if(!expdefs[expcount]->int_name)
							{
								ReportError(ERR_NO_MEM);
							}
							for(i=0;i<buf[j];i++)
							{
								expdefs[expcount]->int_name[i]=buf[j+i+1];
							}
							expdefs[expcount]->int_name[buf[j]]=0;
							if(!case_sensitive)
							{
								strupr(expdefs[expcount]->int_name);
							}
						}
						else
						{
							expdefs[expcount]->int_name=malloc(strlen(expdefs[expcount]->exp_name)+1);
							if(!expdefs[expcount]->int_name)
							{
								ReportError(ERR_NO_MEM);
							}
							strcpy(expdefs[expcount]->int_name,expdefs[expcount]->exp_name);
						}
						j+=buf[j]+1;
						if(expdefs[expcount]->flags&EXP_ORD)
						{
							expdefs[expcount]->ordinal=buf[j]+256*buf[j+1];
						}
						else
						{
							expdefs[expcount]->ordinal=0;
						}
						expcount++;
						break;
					default:
						ReportError(ERR_INVALID_COMENT);
					}
					break;
				case COMENT_DOSSEG:
					break;
				case COMENT_TRANSLATOR:
				case COMENT_INTEL_COPYRIGHT:
				case COMENT_MSDOS_VER:
				case COMENT_MEMMODEL:
				case COMENT_NEWOMF:
				case COMENT_LINKPASS:
				case COMENT_LIBMOD:
				case COMENT_EXESTR:
				case COMENT_INCERR:
				case COMENT_NOPAD:
				case COMENT_WKEXT:
				case COMENT_LZEXT:
				case COMENT_PHARLAP:
				case COMENT_IBM386:
				case COMENT_RECORDER:
				case COMENT_COMMENT:
				case COMENT_COMPILER:
				case COMENT_DATE:
				case COMENT_TIME:
				case COMENT_USER:
				case COMENT_DEPFILE:
				case COMENT_COMMANDLINE:
				case COMENT_PUBTYPE:
				case COMENT_COMPARAM:
				case COMENT_TYPDEF:
				case COMENT_STRUCTMEM:
				case COMENT_OPENSCOPE:
				case COMENT_LOCAL:
				case COMENT_ENDSCOPE:
				case COMENT_SOURCEFILE:
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
				externs[extcount]->pubnum=-1;
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

void loadlib(FILE *libfile,PCHAR libname)
{
	unsigned int i,j,k,n;
	PCHAR name;
	unsigned short modpage;
	PLIBFILE p;
	
	libfiles[libcount]=malloc(sizeof(LIBFILE));
	if(!libfiles[libcount])
	{
		ReportError(ERR_NO_MEM);
	}
	p=libfiles[libcount];
	p->filename=malloc(strlen(libname)+1);
	if(!p->filename)
	{
		ReportError(ERR_NO_MEM);
	}
	strcpy(p->filename,libname);
	
	if(fread(buf,1,3,libfile)!=3)
	{
		printf("Error reading from file\n");
		exit(1);
	}
	p->blocksize=buf[1]+256*buf[2];
	if(fread(buf,1,p->blocksize,libfile)!=p->blocksize)
	{
		printf("Error reading from file\n");
		exit(1);
	}
	p->blocksize+=3;
	p->dicstart=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
	p->numdicpages=buf[4]+256*buf[5];
	p->flags=buf[6];
	fseek(libfile,p->dicstart,SEEK_SET);
	
	p->syms=malloc(sizeof(PLIBENTRY)*37*p->numdicpages);
	if(!p->syms)
	{
		ReportError(ERR_NO_MEM);
	}
	
	p->numsyms=0;
	
	for(i=0;i<p->numdicpages;i++)
	{
		if(fread(buf,1,512,libfile)!=512)
		{
			printf("Error reading from file\n");
			exit(1);
		}
		for(j=0;j<37;j++)
		{
			k=buf[j]*2;
			if(k)
			{
				name=malloc(buf[k]+1);
				if(!name)
				{
					ReportError(ERR_NO_MEM);
				}
				for(n=0;n<buf[k];n++)
				{
					name[n]=buf[n+k+1];
				}
				name[buf[k]]=0;
				k+=buf[k]+1;
				modpage=buf[k]+256*buf[k+1];
				if(!p->flags)
				{
					strupr(name);
				}
				if(name[strlen(name)-1]=='!')
				{
					name[strlen(name)-1]=0;
					free(name);
				}
				else
				{
					p->syms[p->numsyms]=malloc(sizeof(LIBENTRY));
					if(!p->syms[p->numsyms])
					{
						ReportError(ERR_NO_MEM);
					}
					p->syms[p->numsyms]->name=name;
					p->syms[p->numsyms]->modpage=modpage;
					p->numsyms++;
				}
			}
		}
	}
	libcount++;
}

void loadlibmod(PLIBFILE p,unsigned short modpage)
{
	FILE *libfile;
	
	libfile=fopen(p->filename,"rb");
	if(!libfile)
	{
		printf("Error opening file %s\n",p->filename);
		exit(1);
	}
	fseek(libfile,modpage*p->blocksize,SEEK_SET);
	loadmod(libfile);
	fclose(libfile);
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
	if((!errcount) && (!seglist[targseg]))
	{
		errcount++;
	}
	if((!errcount) && (!seglist[baseseg]))
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
	
	if(impsreq)
	{
		ReportError(ERR_ILLEGAL_IMPORTS);
	}

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
		long old_nummods;

                printf("ALINK v0.02 (C) Copyright 1998 Anthony A.J. Williams.\n");
		printf("All Rights Reserved\n\n");

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
				case 'm':
					switch(strlen(argv[i]))
					{
					case 2:
						mapfile=1;
						break;
					case 3:
						if(argv[i][2]=='+')
						{
							mapfile=1;
							break;
						}
						else if(argv[i][2]=='-')
						{
							mapfile=0;
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

		seglist=malloc(max_segs*sizeof(PSEG));
		if(!seglist)
		{
			ReportError(ERR_NO_MEM);
		}
		namelist=malloc(max_names*sizeof(PCHAR));
		if(!namelist)
		{
			ReportError(ERR_NO_MEM);
		}
		grplist=malloc(max_grps*sizeof(PGRP));
		if(!grplist)
		{
			ReportError(ERR_NO_MEM);
		}
		relocs=malloc(max_relocs*sizeof(PRELOC));
		if(!relocs)
		{
			ReportError(ERR_NO_MEM);
		}
		impdefs=malloc(max_imports*sizeof(PIMPREC));
		if(!impdefs)
		{
			ReportError(ERR_NO_MEM);
		}
		expdefs=malloc(max_exports*sizeof(PEXPREC));
		if(!expdefs)
		{
			ReportError(ERR_NO_MEM);
		}
		publics=malloc(max_publics*sizeof(PPUBLIC));
		if(!publics)
		{
			ReportError(ERR_NO_MEM);
		}
		externs=malloc(max_externs*sizeof(PEXTREC));
		if(!externs)
		{
			ReportError(ERR_NO_MEM);
		}
		libfiles=malloc(DEF_LIBFILE_COUNT*sizeof(PLIBFILE));
		if(!libfiles)
		{
			ReportError(ERR_NO_MEM);
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
		
		if(mapfile)
		{
			if(!mapname)
			{
				mapname=malloc(strlen(outname)+1+4);
				if(!mapname)
				{
					ReportError(ERR_NO_MEM);
				}
				strcpy(mapname,outname);
				i=strlen(mapname);
				while((i>=0)&&(mapname[i]!='.')&&(mapname[i]!=PATH_CHAR)&&(mapname[i]!=':'))
				{
					i--;
				}
				if(mapname[i]!='.')
				{
					i=strlen(mapname);
				}
				strcpy(mapname+i,".MAP");
			}			 
		}
		else
		{
			if(mapname)
			{
				free(mapname);
				mapname=0;
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
			j=fgetc(afile);
			fseek(afile,0,SEEK_SET);
			switch(j)
			{
			case LIBHDR:
				loadlib(afile,filename[i]);
				break;
			case THEADR:
			case LHEADR:
				loadmod(afile);
				break;
			default:
				printf("Unknown file type\n");
				fclose(afile);
				exit(1);
			}
			fclose(afile);
		}
		
		if(!nummods)
		{
			printf("No required modules specified\n");
			exit(1);
		}
		
		for(i=0;i<expcount;i++)
		{
			expdefs[i]->pubnum=-1;
			for(j=0;j<pubcount;j++)
			{
				if(!strcmp(expdefs[i]->int_name,publics[j]->name))
				{
					expdefs[i]->pubnum=j;
				}
			}
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
				for(j=0;j<impcount;j++)
				{
					if(!strcmp(externs[i]->name,impdefs[j]->int_name))
					{
						externs[i]->pubnum=LONG_MAX;
						externs[i]->impnum=j;
						impsreq++;
					}
				}
			}
			if(externs[i]->pubnum<0)
			{
				for(j=0;j<expcount;j++)
				{
					if(!strcmp(externs[i]->name,expdefs[j]->exp_name))
					{
						externs[i]->pubnum=expdefs[j]->pubnum;
					}
				}
			}
		}
		
		do
		{
			old_nummods=nummods;
			for(i=0;(i<expcount)&&(nummods==old_nummods);i++)
			{
				if(expdefs[i]->pubnum<0)
				{
					for(j=0;(j<libcount)&&(nummods==old_nummods);j++)
					{
						for(k=0;(k<libfiles[j]->numsyms)&&(nummods==old_nummods);k++)
						{
							if((libfiles[j]->flags==0) || (case_sensitive==0))
							{
								if(stricmp(libfiles[j]->syms[k]->name,expdefs[i]->int_name)==0)
								{
									loadlibmod(libfiles[j],libfiles[j]->syms[k]->modpage);
								}
							} 
							else
							{
								if(strcmp(libfiles[j]->syms[k]->name,expdefs[i]->int_name)==0)
								{
									loadlibmod(libfiles[j],libfiles[j]->syms[k]->modpage);
								}
							}
						}
					}
				}
			}
			for(i=0;(i<extcount)&&(nummods==old_nummods);i++)
			{
				if(externs[i]->pubnum<0)
				{
					for(j=0;(j<libcount)&&(nummods==old_nummods);j++)
					{
						for(k=0;(k<libfiles[j]->numsyms)&&(nummods==old_nummods);k++)
						{
							if((libfiles[j]->flags==0) || (case_sensitive==0))
							{
								if(stricmp(libfiles[j]->syms[k]->name,externs[i]->name)==0)
								{
									loadlibmod(libfiles[j],libfiles[j]->syms[k]->modpage);
								}
							} 
							else
							{
								if(strcmp(libfiles[j]->syms[k]->name,externs[i]->name)==0)
								{
									loadlibmod(libfiles[j],libfiles[j]->syms[k]->modpage);
								}
							}
						}
					}
				}
			}
			if(nummods!=old_nummods)
			{
				for(i=0;i<expcount;i++)
				{
					if(expdefs[i]->pubnum<0)
					{
						for(j=0;j<pubcount;j++)
						{
							if(!strcmp(expdefs[i]->int_name,publics[j]->name))
							{
								expdefs[i]->pubnum=j;
							}
						}
					}
				}

				for(i=0;i<extcount;i++)
				{
					if(externs[i]->pubnum<0)
					{
						for(j=0;j<pubcount;j++)
						{
							if(!strcmp(externs[i]->name,publics[j]->name))
							{
								externs[i]->pubnum=j;
							}
						}
						if(externs[i]->pubnum<0)
						{
							for(j=0;j<impcount;j++)
							{
								if(!strcmp(externs[i]->name,impdefs[j]->int_name))
								{
									externs[i]->pubnum=LONG_MAX;
									externs[i]->impnum=j;
									impsreq++;
								}
							}
						}
						if(externs[i]->pubnum<0)
						{
							for(j=0;j<expcount;j++)
							{
								if(!strcmp(externs[i]->name,expdefs[j]->exp_name))
								{
									externs[i]->pubnum=expdefs[j]->pubnum;
								}
							}
						}
					}
				}
			}
		} while (old_nummods!=nummods);

		for(i=0;i<expcount;i++)
		{
			if(expdefs[i]->pubnum<0)
			{
				printf("Unresolved export %s=%s\n",expdefs[i]->exp_name,expdefs[i]->int_name);
				errcount++;
			}
		}

		for(i=0;i<extcount;i++)
		{
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
		if(mapfile)
		{
			afile=fopen(mapname,"wt");
			if(!afile)
			{
				printf("Error opening map file %s\n",mapname);
				exit(1);
			}
			
			for(i=0;i<segcount;i++)
			{
				if(seglist[i])
				{
					fprintf(afile,"SEGMENT %s ",namelist[seglist[i]->nameindex]);
					switch(seglist[i]->attr&SEG_COMBINE)
					{
					case SEG_PRIVATE:
						 fprintf(afile,"PRIVATE ");
						 break;
					case SEG_PUBLIC:
						 fprintf(afile,"PUBLIC ");
						 break;
					case SEG_PUBLIC2:
						 fprintf(afile,"PUBLIC(2) ");
						 break;
					case SEG_STACK:
						 fprintf(afile,"STACK ");
						 break;
					case SEG_COMMON:
						 fprintf(afile,"COMMON ");
						 break;
					case SEG_PUBLIC3:
						 fprintf(afile,"PUBLIC(3) ");
						 break;
					default:
						 fprintf(afile,"unknown ");
						 break;
					}
					if(seglist[i]->attr&SEG_USE32)
					{
						fprintf(afile,"USE32 ");
					}		
					else
					{
						fprintf(afile,"USE16 ");
					}
					switch(seglist[i]->attr&SEG_ALIGN)
					{
					case SEG_ABS:
						 fprintf(afile,"AT 0%04lXh ",seglist[i]->absframe);
						 break;
					case SEG_BYTE:
						 fprintf(afile,"BYTE ");
						 break;
					case SEG_WORD:
						 fprintf(afile,"WORD ");
						 break;
					case SEG_PARA:
						 fprintf(afile,"PARA ");
						 break;
					case SEG_PAGE:
						 fprintf(afile,"PAGE ");
						 break;
					case SEG_DWORD:
						 fprintf(afile,"DWORD ");
						 break;
					case SEG_MEMPAGE:
						 fprintf(afile,"MEMPAGE ");
						 break;
					default:
						 fprintf(afile,"unknown ");
					}
					fprintf(afile,"'%s'\n",namelist[seglist[i]->classindex]);
					fprintf(afile,"  at %08lX, length %08lX\n",seglist[i]->base,seglist[i]->length);
				}
			}

			if(pubcount)
			{
				fprintf(afile,"\n %li publics:\n",pubcount);
				for(i=0;i<pubcount;i++)
				{
					fprintf(afile,"%s at %s:%08lX\n",
						publics[i]->name,
						(publics[i]->segnum>=0) ? namelist[seglist[publics[i]->segnum]->nameindex] : "Absolute",
						publics[i]->ofs);
				}
			}	 
			
			if(expcount)
			{
				fprintf(afile,"\n %li exports:\n",expcount);
				for(i=0;i<expcount;i++)
				{
					fprintf(afile,"%s(%i)=%s\n",expdefs[i]->exp_name,expdefs[i]->ordinal,expdefs[i]->int_name);
				}
			}

			if(impcount)
			{
				fprintf(afile,"\n %li imports:\n",impcount);
				for(i=0;i<impcount;i++)
				{
					fprintf(afile,"%s=%s:%s(%i)\n",impdefs[i]->int_name,impdefs[i]->mod_name,impdefs[i]->flags==0?impdefs[i]->imp_name:"",	  
						impdefs[i]->flags==0?0:impdefs[i]->ordinal);
				}
			}
			fclose(afile);
		}
		return 0;
}
