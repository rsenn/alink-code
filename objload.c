#include "alink.h"

char t_thred[4];
char f_thred[4];
int t_thredindex[4];
int f_thredindex[4];

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
	int thrednum;

	j=*p;

	r->ftype=buf[j]>>4;
	r->ttype=buf[j]&0xf;
	j++;
	if(r->ftype&FIX_THRED)
	{
		thrednum=r->ftype&THRED_MASK;
		if(thrednum>3)
		{
			ReportError(ERR_BAD_FIXUP);
		}
		r->ftype=(f_thred[thrednum]>>2)&7;
		switch(r->ftype)
		{
		case REL_SEGFRAME:
		case REL_GRPFRAME:
		case REL_EXTFRAME:
			 r->frame=f_thredindex[thrednum];
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
		thrednum=r->ttype&3;
		if(r->ttype&4) /* P bit set? */
		{
			r->ttype=(t_thred[thrednum]>>2)&3; /* DISP present */
		}
		else
		{
			r->ttype=(t_thred[thrednum]>>2)&7 | 4; /* no disp */
		}
		r->target=t_thredindex[thrednum];
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
			seglist[segcount]->winFlags=WINF_UNDEFINED; /* no windows segment flags */
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
				externs[extcount]->flags=EXT_NOMATCH;
				extcount++;
			 }
			 break;
		case GRPDEF:
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
					i=buf[j]; /* get thred number */
					j++;
					if(i&0x40) /* Frame? */
					{
						f_thred[i&3]=i;
						/* get index if required */
						if((i&0x1c)<0xc)
						{
							f_thredindex[i&3]=GetIndex(buf,&j);
						}
						i&=3;
					}
					else
					{
						t_thred[i&3]=i;
						/* target always has index */
						t_thredindex[i&3]=GetIndex(buf,&j);
					}
				}
			 }
			 break;
		case LINNUM:
		case LINNUM32:
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
	p->modsloaded=0;
	p->modlist=malloc(sizeof(unsigned short)*p->numsyms);
	if(!p->modlist)
		ReportError(ERR_NO_MEM);
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
	p->modlist[p->modsloaded]=modpage;
	p->modsloaded++;
	fclose(libfile);
}

