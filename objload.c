#include "alink.h"
#include "omf.h"

static char t_thred[4];
static char f_thred[4];
static int t_thredindex[4];
static int f_thredindex[4];

static PLIBLOCK lidata=NULL;
static UCHAR buf[65536];
static INT rectype;
static INT li_le=0;
static UINT reclength;
static PSEG prevseg=NULL;
static UINT prevofs=0;
static PPSEG seglist=NULL;
static PPSEG grplist=NULL;
static UINT grpcount=0;
static UINT segcount=0;

static PPEXPORTREC expdefs=NULL;
static UINT expcount=0;

static PPEXTREF externs=NULL;
static UINT extcount=0;

static PCOMDATENTRY comdatList=NULL;
static UINT comdatCount=0;

static enum {DBG_UNKNOWN, DBG_BORLAND, DBG_GCC, DBG_CODEVIEW} debugType;

static INT GetIndex(PUCHAR buf,UINT *index)
{
    UINT i;
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

static void DestroyLIDATA(PLIBLOCK p)
{
    UINT i;
    if(p->blocks)
    {
	for(i=0;i<p->blocks;i++)
	{
	    DestroyLIDATA(((PPLIBLOCK)(p->data))[i]);
	}
    }
    free(p->data);
    free(p);
}

static PLIBLOCK BuildLiData(UINT *bufofs)
{
    PLIBLOCK p;
    UINT i,j;

    p=checkMalloc(sizeof(DATABLOCK));
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
	p->data=checkMalloc(p->blocks*sizeof(PLIBLOCK));
	for(j=0;j<p->blocks;j++)
	{
	    ((PPLIBLOCK)p->data)[j]=BuildLiData(&i);
	}
    }
    else
    {
	p->data=checkMalloc(buf[i]+1);
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

static PDATABLOCK EmitLiData(PLIBLOCK p)
{
    UINT i,j;
    PDATABLOCK d,d2;

    if(!p->count) return NULL;

    if(p->blocks)
    {
	d=NULL;
	for(j=0;j<p->blocks;j++)
	{
	    d2=EmitLiData(((PPLIBLOCK)p->data)[j]);
	    if(!d)
	    {
		d=d2;
	    }
	    else if(d2)
	    {
		d->data=checkRealloc(d->data,d->length+d2->length);
		memcpy(d->data+d->length,d2->data,d2->length);
		d->length+=d2->length;
		freeDataBlock(d2);
	    }
	}
    }
    else
    {
	d=createDataBlock(((PUCHAR)p->data)+1,0,((PUCHAR)p->data)[0],1);
    }
    if(p->count>1)
    {
	d->data=checkRealloc(d->data,d->length*p->count);
	for(i=1;i<p->count;i++)
	{
	    memcpy(d->data+i*d->length,d->data,d->length);
	}
	d->length*=p->count;
    }
    return d;
}

static BOOL RelocLIDATA(PLIBLOCK p,PSEG s,UINT *ofs,PRELOC r)
{
    UINT i,j;

    for(i=0;i<p->count;i++)
    {
	if(p->blocks)
	{
	    for(j=0;j<p->blocks;j++)
	    {
		if(!RelocLIDATA(((PPLIBLOCK)p->data)[j],s,ofs,r))
		    return FALSE;
	    }
	}
	else
	{
	    j=r->ofs-p->dataofs;
	    if(j>=0)
	    {
		if((j<5) || ((li_le==PREV_LI32) && (j<7)))
		{
		    addError("Bad LIDATA offset");
		    return FALSE;
		}
		s->relocs=(PRELOC)checkRealloc(s->relocs,(s->relocCount+1)*sizeof(RELOC));
		memcpy(s->relocs+s->relocCount,r,sizeof(RELOC));
		s->relocs[s->relocCount].ofs=*ofs+j;
		s->relocCount++;
		*ofs+=((PUCHAR)p->data)[0];
	    }
	}
    }
    return TRUE;
}

static BOOL getFixupSeg(PRELOC r,UINT ftype,INT frame)
{
    if(frame<0)
    {
	addError("Invalid frame in fixup");
	return FALSE;
    }
    switch(ftype)
    {
    case REL_SEGFRAME:
	if(frame>=segcount)
	{
	    addError("Invalid frame segment");
	    return FALSE;
	}
	r->fseg=seglist[frame];
	break;
    case REL_GRPFRAME:
	if(frame>=grpcount)
	{
	    addError("Invalid frame group");
	    return FALSE;
	}
	r->fseg=grplist[frame];
	break;
    case REL_EXTFRAME:
	if(frame>=extcount)
	{
	    addError("Invalid frame external");
	    return FALSE;
	}
	r->fseg=NULL;
	r->fext=externs[frame];
	break;
    case REL_LILEFRAME:
	r->fseg=prevseg;
	break;
    case REL_TARGETFRAME:
	/* no seg yet */
	break;
    default:
	addError("Invalid Frame Type %lX",ftype);
	return FALSE;
    }
    return TRUE;
}


static BOOL LoadFIXUP(PRELOC r,PUCHAR buf,UINT *p)
{
    UINT j;
    UINT thrednum;
    UINT ftype,ttype;
    INT target;
    
    j=*p;

    ftype=buf[j]>>4;
    ttype=buf[j]&0xf;
    j++;
    r->fseg=r->tseg=NULL;
    r->text=r->fext=NULL;
    if(ftype&FIX_THRED)
    {
	thrednum=ftype&THRED_MASK;
	if(thrednum>3)
	{
	    addError("Invalid THRED number %i in fixup record",thrednum);
	    return FALSE;
	}
	ftype=f_thred[thrednum];
	if(!getFixupSeg(r,ftype,f_thredindex[thrednum]))
	    return FALSE;
    }
    else
    {
	switch(ftype)
	{
	case REL_SEGFRAME:
	case REL_GRPFRAME:
	case REL_EXTFRAME:
	    if(!getFixupSeg(r,ftype,GetIndex(buf,&j)-1))
		return FALSE;
	    break;
	case REL_LILEFRAME:
	case REL_TARGETFRAME:
	    if(!getFixupSeg(r,ftype,0))
		return FALSE;
	    break;
	default:
	    addError("Invalid FRAME type %02X in fixup record",ftype);
	    return FALSE;
	}
    }
    if(ttype&FIX_THRED)
    {
	thrednum=ttype&3;
	if((ttype&4)==0) /* P bit not set? */
	{
	    ttype=t_thred[thrednum]; /* DISP present */
	}
	else
	{
	    ttype=t_thred[thrednum] | 4; /* no disp */
	}
	target=t_thredindex[thrednum];
    }
    else
    {
	target=GetIndex(buf,&j)-1;
    }

    if(target<0)
    {
	addError("Invalid FIXUP");
	return FALSE;
    }

    switch(ttype)
    {
    case REL_SEGDISP:
    case REL_GRPDISP:
    case REL_EXTDISP:
	r->disp=buf[j]+buf[j+1]*256;
	j+=2;
	if(rectype==FIXUPP32)
	{
	    r->disp+=(buf[j]+buf[j+1]*256)<<16;
	    j+=2;
	}
	break;
    case REL_SEGONLY:
    case REL_GRPONLY:
    case REL_EXTONLY:
	r->disp=0;
	break;
    default:
	addError("Invalid Target Type for fixup %lX",ttype);
	return FALSE;
	break;
    }

    switch(ttype)
    {
    case REL_SEGONLY:
    case REL_SEGDISP:
	if(target>=segcount)
	{
	    addError("Invalid target segment %li",target);
	    return FALSE;
	}
	r->tseg=seglist[target];
	break;
    case REL_GRPONLY:
    case REL_GRPDISP:
	if(target>=grpcount)
	{
	    addError("Invalid target group");
	    return FALSE;
	}
	r->tseg=grplist[target];
	break;
    case REL_EXTDISP:
    case REL_EXTONLY:
	if(target>=extcount)
	{
	    addError("Invalid target external");
	    return FALSE;
	}
	r->tseg=NULL;
	r->text=externs[target];
	break;
    default:
	addError("Invalid Target Type %lX",ttype);
	return FALSE;
    }

    if(ftype==REL_TARGETFRAME)
    {
	switch(ttype)
	{
	case REL_SEGDISP:
	case REL_GRPDISP:
	case REL_EXTDISP:
	    ftype=ttype;
	    break;
	case REL_SEGONLY:
	case REL_GRPONLY:
	case REL_EXTONLY:
	    ftype=ttype-4;
	    break;
	}
	r->fseg=r->tseg;
	r->fext=r->text;
    }
    *p=j;
    return TRUE;
}

BOOL OMFDetect(PFILE objfile,PCHAR name)
{
    UINT rectype,reclength;

    if(fread(buf,1,3,objfile)!=3)
    {
	return FALSE;
    }
    rectype=buf[0];
    reclength=buf[1]+256*buf[2];
    if(fread(buf,1,reclength,objfile)!=reclength)
    {
	return FALSE;
    }
    return (rectype==THEADR) || (rectype==LHEADR);
}

BOOL loadOMFModule(PFILE objfile,PMODULE mod)
{
    UINT modpos=0;
    BOOL done=FALSE;
    UINT i,j,k;
    INT segnum,grpnum;
    INT typenum;
    UINT ofs;
    UINT length;
    PRELOC r;
    PSYMBOL pubdef;
    PCHAR name,class,aliasName,mod_name,imp_name;
    PCHAR moduleName=NULL;
    INT flag;
    USHORT ordinal;
    PPCHAR namelist=NULL;
    INT namecount=0;
    UINT absframe;
    UINT absofs;
    INT nameindex,classindex,overlayindex;
    PDATABLOCK d;
    PSEG seg;
    BOOL isPharlap=FALSE;
    UINT attr;
    UINT align;
    PCOMDATREC comdat;
    PPSYMBOL locSyms=NULL;
    UINT locSymCount=0;
    PPSYMBOL globSyms=NULL;
    UINT globSymCount=0;
    UINT localExtRefCount=0,globalExtRefCount=0;
    UINT currentSource=0;

    li_le=0;
    lidata=0;
    debugType=DBG_UNKNOWN;

    while(!done)
    {
	if(fread(buf,1,3,objfile)!=3)
	{
	    addError("Missing MODEND record");
	    return FALSE;
	}
	rectype=buf[0];
	reclength=buf[1]+256*buf[2];
	if(fread(buf,1,reclength,objfile)!=reclength)
	{
	    addError("Unable to read record data");
	    return FALSE;
	}
	reclength--; /* remove checksum */
	if((!moduleName)&&(rectype!=THEADR)&&(rectype!=LHEADR))
	{
	    addError("No LHEADR or THEADR record");
	    return FALSE;
	}
	switch(rectype)
	{
	case THEADR:
	case LHEADR:
	    if(moduleName)
	    {
		addError("Multiple LHEADR or THEADR records in same module");
		return FALSE;
	    }
	    moduleName=checkMalloc(buf[0]+1);
	    memcpy(moduleName,buf+1,buf[0]);
	    moduleName[buf[0]]=0;
	    strupr(moduleName);
	    if((buf[0]+1)!=reclength)
	    {
		addError("Additional data in THEADR/LHEADR record");
		return FALSE;
	    }

	    mod->name=moduleName;
	    break;
	case COMENT:
	    li_le=0;
	    if(lidata)
	    {
		DestroyLIDATA(lidata);
		lidata=0;
	    }
	    if(reclength>=2)
	    {
		switch(buf[1])
		{
		case COMENT_LIB_SPEC:
		case COMENT_DEFLIB:
		    if(noDefaultLibs) break; /* don't add name if "no default libs" set */
		    name=(PCHAR)checkMalloc(reclength-1+4);
		    /* get filename */
		    memcpy(name,buf+2,reclength-2);
		    name[reclength-2]=0;
		    for(i=strlen(name)-1;
			(i>=0) && !strchr(PATHCHARS,name[i]);
			i--)
		    {
			if(name[i]=='.') break;
		    }
		    if(((i>=0) &&  (name[i]!='.'))||(i<0))
		    {
			strcat(name,".lib");
		    }
		    /* add default library to file list */
		    fileNames=checkRealloc(fileNames,(fileCount+1)*sizeof(PMODULE));
		    fileNames[fileCount]=createModule(name);
		    fileCount++;
		    break;
		case COMENT_OMFEXT:
		    if(reclength<4)
		    {
			addError("Invalid COMENT record length");
			return FALSE;
		    }
		    switch(buf[2])
		    {
		    case EXT_IMPDEF:
			j=4;
			if(reclength<(j+4))
			{
			    addError("Invalid IMPDEF COMENT");
			    return FALSE;
			}
			name=checkMalloc(buf[j]+1);
			memcpy(name,buf+j+1,buf[j]);
			name[buf[j]]=0;
			j+=buf[j]+1;
			mod_name=checkMalloc(buf[j]+1);
			memcpy(mod_name,buf+j+1,buf[j]);
			mod_name[buf[j]]=0;
			j+=buf[j]+1;
			if(buf[3])
			{
			    ordinal=buf[j]+256*buf[j+1];
			    imp_name=NULL;
			    j+=2;
			}
			else
			{
			    if(buf[j])
			    {
				imp_name=checkMalloc(buf[j]+1);
				memcpy(imp_name,buf+j+1,buf[j]);
				imp_name[buf[j]]=0;
				j+=buf[j]+1;
			    }
			    else
			    {
				imp_name=checkStrdup(name);
			    }
			    ordinal=0;
			}
			pubdef=createSymbol(name,PUB_IMPORT,mod,mod_name,imp_name,ordinal);
			globSyms=checkRealloc(globSyms,(globSymCount+1)*sizeof(PSYMBOL));
			globSyms[globSymCount]=pubdef;
			globSymCount++;
			break;
		    case EXT_EXPDEF:
			
			expdefs=checkRealloc(expdefs,(expcount+1)*sizeof(PEXPORTREC));
			expdefs[expcount]=checkMalloc(sizeof(EXPORTREC));
			j=4;
			flag=buf[3];
			expdefs[expcount]->exp_name=checkMalloc(buf[j]+1);
			memcpy(expdefs[expcount]->exp_name,buf+j+1,buf[j]);
			expdefs[expcount]->exp_name[buf[j]]=0;
			if(!case_sensitive)
			{
			    strupr(expdefs[expcount]->exp_name);
			}
			j+=buf[j]+1;
			if(buf[j])
			{
			    expdefs[expcount]->int_name=checkMalloc(buf[j]+1);
			    memcpy(expdefs[expcount]->int_name,buf+j+1,buf[j]);
			    expdefs[expcount]->int_name[buf[j]]=0;
			    if(!case_sensitive)
			    {
				strupr(expdefs[expcount]->int_name);
			    }
			}
			else
			{
			    expdefs[expcount]->int_name=checkStrdup(expdefs[expcount]->exp_name);
			}
			j+=buf[j]+1;
			if(flag&EXP_ORD)
			{
			    expdefs[expcount]->ordinal=buf[j]+256*buf[j+1];
			}
			else
			{
			    expdefs[expcount]->ordinal=0;
			}
			expdefs[expcount]->isResident=(flag&EXP_RESIDENT)!=0;
			expdefs[expcount]->noData=(flag&EXP_NODATA)!=0;
			expdefs[expcount]->numParams=flag&EXP_NUMPARAMS;
			expcount++;
			break;
		    default:
			addError("Invalid COMENT record");
			return FALSE;
		    }
		    break;
		case COMENT_DOSSEG:
		    dosSegOrdering=TRUE;
		    break;
		case COMENT_PHARLAP:
		    isPharlap=TRUE;
		    break;
		case COMENT_TRANSLATOR:
		case COMENT_COMPILER:
		    name=checkMalloc(buf[2]);
		    memcpy(name,buf+3,buf[2]);
		    name[buf[2]]=0;
		    if(!mod->compiler)
		    {
			mod->compiler=name;
		    }
		    else
		    {
			mod->comments=checkRealloc(mod->comments,(mod->commentCount+1)*sizeof(PCHAR));
			mod->comments[mod->commentCount]=name;
			mod->commentCount++;
		    }
		    break;
		case COMENT_NEWOMF:
		    if(reclength==2)
		    {
			debugType=DBG_BORLAND;
		    }
		    else
		    {
			if(!memcmp(buf+2,"\03HL",3))
			{
			    debugType=DBG_GCC;
			    addError("GCC debug info");
			}
			else if(!memcmp(buf+2,"\01CV",3))
			{
			    debugType=DBG_CODEVIEW;
			    addError("Codeview debug info");
			}
		    }
		    break;
		case COMENT_SOURCEFILE:
		    j=2;
		    i=GetIndex(buf,&j);
		    if(i && (j!=reclength))
		    {
			addError("Invalid SOURCEFILE COMENT record");
			break;
		    }
		    if(i)
		    {
			if(i>mod->sourceFileCount)
			{
			    addError("Selection of non-existent source file");
			    break;
			}
			currentSource=i;
			break;
		    }
		    ++(mod->sourceFileCount);
		    currentSource=mod->sourceFileCount;
		    mod->sourceFiles=checkRealloc(mod->sourceFiles,currentSource*sizeof(PCHAR));
		    mod->sourceFiles[currentSource-1]=checkMalloc(buf[j]+1);
		    memcpy(mod->sourceFiles[currentSource-1],buf+j+1,buf[j]);
		    mod->sourceFiles[currentSource-1][buf[j]]=0;
		    break;
		case COMENT_DEPFILE:
		    if(reclength<6)
		    {
			break;
		    }
		    
		    mod->dependencies=checkRealloc(mod->dependencies,(mod->depCount+1)*sizeof(PCHAR));
		    mod->dependencies[mod->depCount]=checkMalloc(buf[6]+1);
		    memcpy(mod->dependencies[mod->depCount],buf+7,buf[6]);
		    mod->dependencies[mod->depCount][buf[6]]=0;
		    ++(mod->depCount);
		    break;
		case COMENT_INTEL_COPYRIGHT:
		case COMENT_MSDOS_VER:
		case COMENT_MEMMODEL:
		case COMENT_LINKPASS:
		case COMENT_LIBMOD:
		case COMENT_EXESTR:
		case COMENT_INCERR:
		case COMENT_NOPAD:
		case COMENT_WKEXT:
		case COMENT_LZEXT:
		case COMENT_IBM386:
		case COMENT_RECORDER:
		case COMENT_COMMENT:
		case COMENT_DATE:
		case COMENT_TIME:
		case COMENT_USER:
		case COMENT_COMMANDLINE:
		case COMENT_PUBTYPE:
		case COMENT_COMPARAM:
		case COMENT_TYPDEF:
		case COMENT_STRUCTMEM:
		case COMENT_OPENSCOPE:
		case COMENT_LOCAL:
		case COMENT_ENDSCOPE:
		    break;
		default:
		    addError("COMENT Record (unknown type %02X)",buf[1]);
		    break;
		}
	    }
	    break;
	case LLNAMES:
	case LNAMES:
	    j=0;
	    while(j<reclength)
	    {
		namelist=(PPCHAR)checkRealloc(namelist,(namecount+1)*sizeof(PCHAR));
		namelist[namecount]=checkMalloc(buf[j]+1);
		memcpy(namelist[namecount],buf+j+1,buf[j]);
		namelist[namecount][buf[j]]=0;
		j+=buf[j]+1;
		if(!case_sensitive)
		{
		    strupr(namelist[namecount]);
		}
		namecount++;
	    }
	    break;
	case SEGDEF:
	case SEGDEF32:
	    /* load section definition data */
	    attr=buf[0];
	    j=1;
	    if((attr & SEG_ALIGN)==SEG_ABS)
	    {
		absframe=buf[j]+256*buf[j+1];
		absofs=buf[j+2];
		j+=3;
	    }
	    length=buf[j]+256*buf[j+1];
	    j+=2;
	    if(rectype==SEGDEF32)
	    {
		length+=(buf[j]+256*buf[j+1])<<16;
		j+=2;
	    }
	    if(attr&SEG_BIG)
	    {
		if(rectype==SEGDEF)
		{
		    length+=65536;
		}
		else
		{
		    if((attr&SEG_ALIGN)!=SEG_ABS)
		    {
			addError("Segment too large (non-absolute seg 4Gb or larger)");
			return FALSE;
		    }
		}
	    }
	    nameindex=GetIndex(buf,&j)-1;
	    classindex=GetIndex(buf,&j)-1;
	    overlayindex=GetIndex(buf,&j)-1;
	    if((nameindex>namecount) || (classindex > namecount) || (overlayindex > namecount))
	    {
		addError("Reference to undefined LNAMES entry");
		addError("namecount=%li",namecount);
		addError("nameindex=%li",nameindex);
		addError("overlayindex=%li",overlayindex);
		addError("classindex=%li",classindex);
		
		return FALSE;
	    }
	    
	    /* don't combine absolute segments */
	    if((attr&SEG_ALIGN)==SEG_ABS)
	    {
		attr&=(0xffffffff-SEG_COMBINE);
		attr|=SEG_PRIVATE;
	    }
	    
	    switch(attr&SEG_COMBINE)
	    {
	    case SEG_PRIVATE:
	    case SEG_PUBLIC:
	    case SEG_PUBLIC2:
	    case SEG_COMMON:
	    case SEG_PUBLIC3:
		break;
	    case SEG_STACK:
		/* stack segs are always byte aligned */
		attr&=(0xffff-SEG_ALIGN);
		attr|=SEG_BYTE;
		break;
	    default:
		addError("Bad SEGDEF combine type");
		return FALSE;
		break;
	    }
	    switch(attr&SEG_ALIGN)
	    {
	    case SEG_ABS:
	    case SEG_BYTE:
		k=1;
		break;
	    case SEG_WORD:
		k=2;
		break;
	    case SEG_DWORD:
		k=4;
		break;
	    case SEG_PARA:
		k=16;
		break;
	    case SEG_PAGE:
		k=0x100;
		break;
	    case SEG_MEMPAGE:
		k=0x1000;
		break;
	    default:
		addError("Bad SEGDEF alignment");
		return FALSE;
		k=0;
	    }

	    seglist=checkRealloc(seglist,(segcount+1)*sizeof(PSEG));
	    seglist[segcount]=createSection(nameindex>=0?namelist[nameindex]:NULL,
					    classindex>=0?namelist[classindex]:NULL,
					    NULL,mod,length,k);

	    seglist[segcount]->use32=attr&SEG_USE32;

	    if((attr&SEG_ALIGN)==SEG_ABS)
	    {
		seglist[segcount]->absolute=TRUE;
		seglist[segcount]->section=absframe;
		seglist[segcount]->base=absofs;
	    }

	    switch(attr&SEG_COMBINE)
	    {
	    case SEG_PRIVATE:
		seglist[segcount]->combine=SEGF_PRIVATE;
		break;
	    case SEG_PUBLIC:
	    case SEG_PUBLIC2:
	    case SEG_PUBLIC3:
		seglist[segcount]->combine=SEGF_PUBLIC;
		break;
	    case SEG_COMMON:
		seglist[segcount]->combine=SEGF_COMMON;
		break;
	    case SEG_STACK:
		seglist[segcount]->combine=SEGF_STACK;
		break;
	    default:
		addError("Bad SEGDEF combine type");
		return FALSE;
		break;
	    }
	    

	    if((classindex>=0) &&
	       (!stricmp(namelist[classindex],"CODE") ||
		!stricmp(namelist[classindex],"TEXT")))
            {
                /* code segment */
		seglist[segcount]->code=TRUE;
		seglist[segcount]->initdata=TRUE;
		seglist[segcount]->execute=TRUE;
		seglist[segcount]->read=TRUE;
            }
            else    /* data segment */
	    {
		seglist[segcount]->initdata=TRUE;
		seglist[segcount]->write=TRUE;
		seglist[segcount]->read=TRUE;
	    }

	    if(!stricmp(namelist[nameindex],"$$SYMBOLS") ||
	       !stricmp(namelist[nameindex],"$$TYPES"))
	    {
		seglist[segcount]->discard=TRUE;
	    }
	    
	    segcount++;
	    break;
	case LEDATA:
	case LEDATA32:
	    j=0;
	    i=GetIndex(buf,&j)-1;
	    if(i<0)
	    {
		addError("Invalid segment number for LEDATA record");
		return FALSE;
	    }
	    if(seglist[i]->absolute)
	    {
		addError("LEDATA for absolute segment");
		return FALSE;
	    }
	    prevseg=seglist[i];
	    prevofs=buf[j]+(buf[j+1]<<8);
	    j+=2;
	    if(rectype==LEDATA32)
	    {
		prevofs+=(buf[j]+(buf[j+1]<<8))<<16;
		j+=2;
	    }
	    d=createDataBlock(buf+j,prevofs,reclength-j,1);
	    addFixedData(seglist[i],d);
	    li_le=PREV_LE;
	    break;
	case LIDATA:
	case LIDATA32:
	    if(lidata)
	    {
		DestroyLIDATA(lidata);
	    }
	    j=0;
	    i=GetIndex(buf,&j)-1;
	    if(i<0)
	    {
		addError("Invalid segment number for LIDATA record");
		return FALSE;
	    }
	    if(seglist[i]->absolute)
	    {
		addError("LIDATA for absolute segment");
		return FALSE;
	    }
	    prevofs=buf[j]+(buf[j+1]<<8);
	    j+=2;
	    if(rectype==LIDATA32)
	    {
		prevofs+=(buf[j]+(buf[j+1]<<8))<<16;
		j+=2;
	    }
	    lidata=checkMalloc(sizeof(LIBLOCK));
	    lidata->data=checkMalloc(sizeof(PLIBLOCK)*(1024/sizeof(LIBLOCK)+1));
	    lidata->blocks=0;
	    lidata->dataofs=j;
	    for(k=0;j<reclength;k++)
	    {
		((PPLIBLOCK)lidata->data)[k]=BuildLiData(&j);
	    }
	    lidata->blocks=k;
	    lidata->count=1;

	    if(!(d=EmitLiData(lidata)))
	    {
		addError("NULL LIDATA");
		return FALSE;
	    }
	       
	    d->offset=prevofs;
	    addFixedData(seglist[i],d);
	    li_le=(rectype==LIDATA)?PREV_LI:PREV_LI32;
	    break;
	case LPUBDEF:
	case LPUBDEF32:
	case PUBDEF:
	case PUBDEF32:
	    j=0;
	    grpnum=GetIndex(buf,&j)-1;
	    segnum=GetIndex(buf,&j)-1;
	    if(segnum<0)
	    {
		j+=2;
	    }
	    for(;j<reclength;)
	    {
		name=checkMalloc(buf[j]+1);
		memcpy(name,buf+j+1,buf[j]);
		name[buf[j]]=0;
		j+=buf[j]+1;
		ofs=buf[j]+256*buf[j+1];
		j+=2;
		if((rectype==PUBDEF32) || (rectype==LPUBDEF32))
		{
		    ofs+=(buf[j]+256*buf[j+1])<<16;
		    j+=2;
		}
		typenum=GetIndex(buf,&j);
		pubdef=createSymbol(name,PUB_PUBLIC,mod,seglist[segnum],ofs,grpnum,typenum);
		if(rectype==LPUBDEF || rectype==LPUBDEF32)
		{
		    locSyms=checkRealloc(locSyms,(locSymCount+1)*sizeof(PSYMBOL));
		    locSyms[locSymCount]=pubdef;
		    locSymCount++;
		}
		else
		{
		    globSyms=checkRealloc(globSyms,(globSymCount+1)*sizeof(PSYMBOL));
		    globSyms[globSymCount]=pubdef;
		    globSymCount++;
		}
	    }
	    break;
	case LEXTDEF:
	case LEXTDEF32:
	case EXTDEF:
	    for(j=0;j<reclength;)
	    {
		externs=(PPEXTREF)checkRealloc(externs,(extcount+1)*sizeof(PEXTREF));
		externs[extcount]=checkMalloc(sizeof(EXTREF));
		externs[extcount]->name=checkMalloc(buf[j]+1);
		k=buf[j];
		j++;
		memcpy(externs[extcount]->name,buf+j,k);
		externs[extcount]->name[k]=0;
		j+=k;
		if(!case_sensitive)
		{
		    strupr(externs[extcount]->name);
		}
		externs[extcount]->typenum=GetIndex(buf,&j);
		externs[extcount]->pubdef=NULL;
		externs[extcount]->mod=mod;
		externs[extcount]->local=((rectype==LEXTDEF) || (rectype==LEXTDEF32));
		extcount++;
	    }
	    break;
	case CEXTDEF:
	    for(j=0;j<reclength;)
	    {
		externs=(PPEXTREF)checkRealloc(externs,(extcount+1)*sizeof(PEXTREF));
		externs[extcount]=checkMalloc(sizeof(EXTREF));
		nameindex=GetIndex(buf,&j)-1;
		if(nameindex<0)
		{
		    addError("Error, reference to undefined name");
		    return FALSE;
		}
		
		externs[extcount]->name=checkStrdup(namelist[nameindex]);
		if(!case_sensitive)
		{
		    strupr(externs[extcount]->name);
		}
		externs[extcount]->typenum=GetIndex(buf,&j);
		externs[extcount]->pubdef=NULL;
		externs[extcount]->mod=mod;
		externs[extcount]->local=FALSE;
		extcount++;
	    }
	    break;
	case GRPDEF:
	    grplist=checkRealloc(grplist,(grpcount+1)*sizeof(PSEG));
	    j=0;
	    nameindex=GetIndex(buf,&j)-1;
	    if(nameindex<0)
	    {
		addError("Invalid name index for GRPDEF record");
		return FALSE;
	    }
	    /* create an empty, private section for group */
	    grplist[grpcount]=seg=createSection(namelist[nameindex],
						NULL,NULL,mod,0,1);
	    grplist[grpcount]->group=TRUE;
	    while(j<reclength)
	    {
		if(buf[j]==0xff)
		{
		    j++;
		    i=GetIndex(buf,&j)-1;
		    if(i<0)
		    {
			addError("Invalid segment index for GRPDEF record");
			return FALSE;
		    }
		    addSeg(seg,seglist[i]);
		}
		else
		{
		    addError("Invalid GRPDEF record");
		    return FALSE;
		}
	    }
	    grpcount++;
	    break;
	case FIXUPP:
	case FIXUPP32:
	    j=0;
	    while(j<reclength)
	    {
		if(buf[j]&0x80)
		{
		    /* FIXUP subrecord */
		    if(!li_le)
		    {
			addError("FIXUP without prior LIDATA or LEDATA record");
			return FALSE;
		    }
		    r=checkMalloc(sizeof(RELOC));
		    flag=(buf[j]>>2);
		    r->ofs=buf[j]*256+buf[j+1];
		    j+=2;
		    r->ofs&=0x3ff;
		    flag^=FIX_SELFREL;
		    flag&=FIX_MASK;

		    if(!LoadFIXUP(r,buf,&j))
			return FALSE;

		    r->base=REL_DEFAULT;
		    switch(flag)
		    {
		    case FIX_LBYTE:
			r->rtype=REL_OFS8;
			break;
		    case FIX_OFS16_2:
		    case FIX_OFS16:
			r->rtype=REL_OFS16;
			break;
		    case FIX_OFS32:
		    case FIX_OFS32_2:
			r->rtype=REL_OFS32;
			break;
		    case FIX_BASE:
			r->rtype=REL_SEG;
			break;
		    case FIX_PTR1616:
			r->rtype=REL_PTR16;
			break;
		    case FIX_PTR1632:
			r->rtype=REL_PTR32;
			break;
		    case FIX_HBYTE:
			r->rtype=REL_BYTE2;
			break;
		    case FIX_SELF_LBYTE:
			r->rtype=REL_OFS8;
			r->base=REL_SELF;
			r->disp--;
			break;
		    case FIX_SELF_OFS16:
		    case FIX_SELF_OFS16_2:
			r->rtype=REL_OFS16;
			r->base=REL_SELF;
			r->disp-=2;
			break;
		    case FIX_SELF_OFS32:
		    case FIX_SELF_OFS32_2:
			r->rtype=REL_OFS32;
			r->base=REL_SELF;
			r->disp-=4;
			break;
		    default:
			addError("Invalid FIXUP type");
			return FALSE;
		    }

		    seg=prevseg;
		    if(li_le==PREV_LE)
		    {
			r->ofs+=prevofs;
			seg->relocs=(PRELOC)checkRealloc(seg->relocs,(seg->relocCount+1)*sizeof(RELOC));
			seg->relocs[seg->relocCount]=*r;
			seg->relocCount++;
		    }
		    else
		    {
			i=prevofs;
			if(!RelocLIDATA(lidata,seg,&i,r))
			    return FALSE;
			free(r);
		    }
		}
		else
		{
		    /* THRED subrecord */
		    i=buf[j]; /* get thred number */
		    j++;
		    if(i&0x40) /* Frame? */
		    {
			/* get frame thread type */
			f_thred[i&3]=(i>>2)&7;
			/* get index if required */
			if((i&0x1c)<0xc)
			{
			    f_thredindex[i&3]=GetIndex(buf,&j)-1;
			}
		    }
		    else
		    {
			t_thred[i&3]=(i>>2)&3;
			/* target always has index */
			t_thredindex[i&3]=GetIndex(buf,&j)-1;
		    }
		}
	    }
	    break;
	case BAKPAT:
	case BAKPAT32:
	case NBKPAT:
	case NBKPAT32:
	    j=0;
	    if((rectype==BAKPAT) || (rectype==BAKPAT32))
	    {
		segnum=GetIndex(buf,&j)-1;
		seg=seglist[segnum];
	    }
	    
	    k=buf[j];
	    j++;

	    if((rectype==NBKPAT) || (rectype==NBKPAT32))
	    {
		segnum=GetIndex(buf,&j)-1;

		name=namelist[segnum];
		for(i=comdatCount-1;i>=0;--i)
		{
		    if(!strcmp(name,comdatList[i].name)) break;
		}
		if(i<0)
		{
		    addError("Reloc in unknown COMDAT %s, name index %li",name,segnum);
		    return FALSE;
		}
		seg=comdatList[i].comdat->segList[0];
	    }

	    while(j<reclength)
	    {
		seg->relocs=(PRELOC)checkRealloc(seg->relocs,(seg->relocCount+1)*sizeof(RELOC));
		r=seg->relocs+seg->relocCount;
		
		switch(k)
		{
		case 0: r->rtype=FIX_SELF_LBYTE; break;
		case 1: r->rtype=FIX_SELF_OFS16; break;
		case 2: r->rtype=FIX_SELF_OFS32; break;
		default:
		    addError("Bad BAKPAT record");
		    return FALSE;
		}
		r->ofs=buf[j]+256*buf[j+1];
		j+=2;
		if(rectype==BAKPAT32)
		{
		    r->ofs+=(buf[j]+256*buf[j+1])<<16;
		    j+=2;
		}
		r->tseg=r->fseg=seg;
		r->disp=buf[j]+256*buf[j+1];
		j+=2;
		if(rectype==BAKPAT32)
		{
		    r->disp+=(buf[j]+256*buf[j+1])<<16;
		    j+=2;
		}
		r->disp+=r->ofs;
		switch(k)
		{
		case 0: r->disp++; break;
		case 1: r->disp+=2; break;
		case 2: r->disp+=4; break;
		default:
		    addError("Bad BAKPAT record");
		    return FALSE;
		}
		seg->relocCount++;
	    }
	    break;
	case LINNUM:
	case LINNUM32:
	    j=0;
	    grpnum=GetIndex(buf,&j)-1;
	    segnum=GetIndex(buf,&j)-1;
	    if(segnum<0)
	    {
		addError("LINNUM record outside segment");
		return FALSE;
	    }
	    seg=seglist[segnum];

	    /* remainder of LINNUM data is compiler-dependent */
		
	    if(debugType==DBG_BORLAND)
	    {
		if(!currentSource)
		{
		    addError("Line numbers given for non-existent source file");
		    break;
		}
		/* count number of LINNUM entries here */
		k=reclength-j;
		k/=((rectype==LINNUM)?4:6);
		
		if(!k) break; /* skip if none */
		
		/* increase buffer size to cater for the extra records */
		i=seg->lineCount;
		seg->lineCount+=k;
		seg->lines=checkRealloc(seg->lines,seg->lineCount*sizeof(LINENUM));
		
		/* get size of each entry */
		k=((rectype==LINNUM)?4:6);
		
		/* load them in */
		for(;i<seg->lineCount;j+=k,i++)
		{
		    seg->lines[i].sourceFile=currentSource;
		    seg->lines[i].num=buf[j]+256*buf[j+1];
		    seg->lines[i].offset=buf[j+2]+256*buf[j+3];
		    if(rectype==LINNUM32)
		    {
			seg->lines[i].offset+=(buf[j+4]<<16)+(buf[j+5]<<24);
		    }
		}
	    }
	    break;
	case MODEND:
	case MODEND32:
	    done=TRUE;
	    if(buf[0]&0x40)
	    {
		if(gotstart)
		{
		    addError("Multiple entry points specified");
		    return FALSE;
		}
		gotstart=1;
		j=1;
		prevseg=NULL; /* no previous segment */
		
		if(!LoadFIXUP(&startaddr,buf,&j))
		    return FALSE;
	    }
	    break;
	case COMDEF:
	    for(j=0;j<reclength;)
	    {
		externs=(PPEXTREF)checkRealloc(externs,(extcount+1)*sizeof(PEXTREF));
		externs[extcount]=checkMalloc(sizeof(EXTREF));
		externs[extcount]->name=checkMalloc(buf[j]+1);
		k=buf[j];
		j++;
		memcpy(externs[extcount]->name,buf+j,k);
		externs[extcount]->name[k]=0;
		j+=k;
		
		if(!case_sensitive)
		{
		    strupr(externs[extcount]->name);
		}
		externs[extcount]->typenum=GetIndex(buf,&j);
		externs[extcount]->pubdef=NULL;
		externs[extcount]->mod=mod;
		externs[extcount]->local=FALSE;
		if(buf[j]==0x61)
		{
		    j++;
		    i=buf[j];
		    j++;
		    if(i==0x81)
		    {
			i=buf[j]+256*buf[j+1];
			j+=2;
		    }
		    else if(i==0x84)
		    {
			i=buf[j]+256*buf[j+1]+65536*buf[j+2];
			j+=3;
		    }
		    else if(i==0x88)
		    {
			i=buf[j]+256*buf[j+1]+65536*buf[j+2]+(buf[j+3]<<24);
			j+=4;
		    }
		    k=i;
		    i=buf[j];
		    j++;
		    if(i==0x81)
		    {
			i=buf[j]+256*buf[j+1];
			j+=2;
		    }
		    else if(i==0x84)
		    {
			i=buf[j]+256*buf[j+1]+65536*buf[j+2];
			j+=3;
		    }
		    else if(i==0x88)
		    {
			i=buf[j]+256*buf[j+1]+65536*buf[j+2]+(buf[j+3]<<24);
			j+=4;
		    }
		    i*=k;
		    k=1;
		}
		else if(buf[j]==0x62)
		{
		    j++;
		    i=buf[j];
		    j++;
		    if(i==0x81)
		    {
			i=buf[j]+256*buf[j+1];
			j+=2;
		    }
		    else if(i==0x84)
		    {
			i=buf[j]+256*buf[j+1]+65536*buf[j+2];
			j+=3;
		    }
		    else if(i==0x88)
		    {
			i=buf[j]+256*buf[j+1]+65536*buf[j+2]+(buf[j+3]<<24);
			j+=4;
		    }
		    k=0;
		}
		else
		{
		    addError("Unknown COMDEF data type %02X",buf[j]);
		    return FALSE;
		}
		flag=k;
		length=i;
		pubdef=createSymbol(checkStrdup(externs[extcount]->name),PUB_COMDEF,mod,length,flag);
		globSyms=checkRealloc(globSyms,(globSymCount+1)*sizeof(PSYMBOL));
		globSyms[globSymCount]=pubdef;
		globSymCount++;
		extcount++;
	    }

	    break;
	case COMDAT:
	case COMDAT32:
	    j=0;
	    flag=buf[j++];
	    attr=buf[j++];
	    
	    align=buf[j++];
	    
	    prevofs=buf[j]+256*buf[j+1];
	    j+=2;
	    if(rectype==COMDAT32)
	    {
		ofs+=(buf[j]<<16)+(buf[j+1]<<24);
		j+=2;
	    }
	    typenum=GetIndex(buf,&j);
	    if(!(attr&0xf)) /* explicit allocation in a given segment */
	    {
		/* obtain base group and segment */
		grpnum=GetIndex(buf,&j)-1;
		segnum=GetIndex(buf,&j)-1;
		if(segnum<0) /* ignore base frame if present */
		{
		    j+=2;
		    addError("Cannot create COMDAT %s in absolute segment",name);
		    return FALSE;
		}
	    }
	    nameindex=GetIndex(buf,&j)-1;
	    if(nameindex<0)
	    {
		addError("Un-named COMDAT not permitted");
		return FALSE;
	    }
	    name=namelist[nameindex];
	    /* make sure we have allocated a segment HERE */
	    if(flag &COMDAT_CONT)
	    {
		/* code for continuing a previous COMDAT */
		for(i=comdatCount-1;i>=0;--i)
		{
		    if(!strcmp(comdatList[i].name,name)) break;
		}
		if(i<0)
		{
		    addError("Attempt to continue non-existent COMDAT %s",name);
		    return FALSE;
		}
		prevseg=comdatList[i].comdat->segList[0];
	    }
	    else
	    {
		/* code for a new COMDAT */
		comdatList=checkRealloc(comdatList,(comdatCount+1)*sizeof(COMDATENTRY));
		comdatList[comdatCount].comdat=comdat=checkMalloc(sizeof(COMDATREC));
		comdatList[comdatCount].name=name;
		comdatCount++;

		comdat->segList=checkMalloc(sizeof(PSEG));
		comdat->segCount=1;
		switch(attr&0xf)
		{
		case 0:
		    break;
		case 1:
		    name="CODE16";
		    class="CODE";
		    break;
		case 2:
		    name="DATA16";
		    class="DATA";
		    break;
		case 3:
		    name="CODE32";
		    class="CODE";
		    break;
		case 4:
		    name="DATA32";
		    class="DATA";
		    break;
		default:
		    addError("Invalid COMDAT allocation type for %s",name);
		    return FALSE;
		}
		switch(attr>>4)
		{
		case 0:
		    comdat->combine=COMDAT_UNIQUE;
		    break;
		case 1:
		    comdat->combine=COMDAT_ANY;
		    break;
		case 2:
		    comdat->combine=COMDAT_SAMESIZE;
		    break;
		case 3:
		    comdat->combine=COMDAT_EXACT;
		    break;
		default:
		    addError("Unknown COMDAT combine type %02lX for %s",attr>>4,name);
		    return FALSE;
		}
		
		/* create section */
		if(!(attr&0xf))
		{
		    comdat->segList[0]=prevseg=createDuplicateSection(seglist[segnum]);
		}
		else
		{
		    comdat->segList[0]=prevseg=createSection(name,class,NULL,mod,0,16);
		}
		
	    }
	    
	    if(flag &COMDAT_LI)
	    {
		/* iterated data, like LIDATA */
		lidata=checkMalloc(sizeof(LIBLOCK));
		lidata->data=checkMalloc(sizeof(PLIBLOCK)*(1024/sizeof(LIBLOCK)+1));
		lidata->blocks=0;
		lidata->dataofs=j;
		for(i=0;j<reclength;i++)
		{
		    ((PPLIBLOCK)lidata->data)[i]=BuildLiData(&j);
		}
		lidata->blocks=i;
		lidata->count=1;
		
		d=EmitLiData(lidata);
		d->offset=prevofs;
		li_le=(rectype==COMDAT)?PREV_LI:PREV_LI32;
	    }
	    else
	    {
		/* enumerated data, like LEDATA */
		d=createDataBlock(buf+j,prevofs,reclength-j,1);
		li_le=PREV_LE;
	    }
	    if(prevseg->length < (d->offset+d->length))
	    {
		prevseg->length=d->offset+d->length;
	    }

	    addFixedData(prevseg,d);
	    break;
	case ALIAS:
	    j=0;
	    name=checkMalloc(buf[j]+1);
	    memcpy(name,buf+j+1,buf[j]);
	    name[buf[j]]=0;
	    j+=buf[j]+1;
	    aliasName=checkMalloc(buf[j]+1);
	    memcpy(aliasName,buf+j+1,buf[j]);
	    aliasName[buf[j]]=0;
	    if(!strlen(name))
	    {
		addError("Cannot alias a blank name");
		return FALSE;
	    }
	    if(!strlen(aliasName))
	    {
		addError("No Alias name specified for %s",name);
		return FALSE;
	    }
	    pubdef=createSymbol(name,PUB_ALIAS,mod,aliasName);
	    globSyms=checkRealloc(globSyms,(globSymCount+1)*sizeof(PSYMBOL));
	    globSyms[globSymCount]=pubdef;
	    globSymCount++;
	    break;
	default:
	    addError("Unknown record type %02X",rectype);
	    return FALSE;
	}
	modpos+=4+reclength;
    }
    if(lidata)
    {
	DestroyLIDATA(lidata);
    }

    if(expcount)
    {
	externs=(PPEXTREF)checkRealloc(externs,(extcount+expcount)*sizeof(PEXTREF));
	globalExports=checkRealloc(globalExports,(globalExportCount+expcount)*sizeof(PEXPORTREC));

	/* create externs, global symbols and global export entries for exports */
	for(i=0;i<expcount;i++)
	{
	    externs[extcount]=checkMalloc(sizeof(EXTREF));
	    externs[extcount]->name=expdefs[i]->int_name;
	    externs[extcount]->typenum=-1;
	    externs[extcount]->pubdef=NULL;
	    externs[extcount]->mod=mod;
	    externs[extcount]->local=FALSE;
	    expdefs[i]->intsym=externs[extcount];
	    extcount++;
	    /* add a global symbol for the export */
	    addGlobalSymbol(createSymbol(expdefs[i]->exp_name,PUB_EXPORT,mod,expdefs[i]));
	    globalExports[globalExportCount]=expdefs[i];
	    globalExportCount++;
	}
    }
    

    /* add comdat's to master list */
    for(i=0;i<comdatCount;++i)
    {
	seg=comdatList[i].comdat->segList[0];
	
	pubdef=createSymbol(comdatList[i].name,PUB_COMDAT,mod,comdatList[i].comdat);
	globSyms=checkRealloc(globSyms,(globSymCount+1)*sizeof(PSYMBOL));
	globSyms[globSymCount]=pubdef;
	globSymCount++;
    }

    /* add groups to master list */
    for(i=0;i<grpcount;i++)
    {
	globalSegs=checkRealloc(globalSegs,(globalSegCount+1)*sizeof(PSEG));
	globalSegs[globalSegCount]=grplist[i];
	globalSegCount++;
    }
    /* add segments to master list */
    for(i=0;i<segcount;i++)
    {
	if(!seglist[i]) continue; /* don't add segments that don't exist */
	if(seglist[i]->parent) continue; /* don't add segments subsumed within a group */
	globalSegs=checkRealloc(globalSegs,(globalSegCount+1)*sizeof(PSEG));
	globalSegs[globalSegCount]=seglist[i];
	globalSegCount++;
    }

    for(i=0;i<extcount;++i)
    {
	if(externs[i]->local)
	{
	    /* local symbol */
	    externs[i]->pubdef=NULL; /* no match yet */
	    for(j=0;j<locSymCount;++j) /* search local tables */
	    {
		if(!strcmp(locSyms[j]->name,externs[i]->name))
		{
		    externs[i]->pubdef=locSyms[j]; /* we found a match, so mark it, and stop searching */
		    locSyms[j]->refCount++;
		    break;
		}
	    }
	    if(!externs[i]->pubdef) /* if no match found, then error */
	    {
		addError("Unmatched Local Symbol Reference %s",externs[i]->name);
		return FALSE;
	    }
	    localExtRefCount++;
	}
	else
	{
	    globalExtRefCount++;
	}
    }

    if(globalExtRefCount)
    {
	globalExterns=checkRealloc(globalExterns,(globalExternCount+globalExtRefCount)*sizeof(PEXTREF));
	for(i=0;i<extcount;++i)
	{
	    if(externs[i]->local) continue; /* skip for locals */
	    globalExterns[globalExternCount]=externs[i];
	    globalExternCount++;
	}
    }
    if(localExtRefCount)
    {
	localExterns=checkRealloc(localExterns,(localExternCount+localExtRefCount)*sizeof(PEXTREF));
	for(i=0;i<extcount;++i)
	{
	    if(!externs[i]->local) continue; /* skip for globals */
	    localExterns[localExternCount]=externs[i];
	    localExternCount++;
	}
    }
    
    if(locSymCount)
    {
	localSymbols=checkRealloc(localSymbols,(localSymbolCount+locSymCount)*sizeof(PSYMBOL));
	for(i=0;i<locSymCount;++i)
	{
	    localSymbols[localSymbolCount]=locSyms[i];
	    localSymbolCount++;
	}
    }

    if(globSymCount)
    {
	for(i=0;i<globSymCount;++i)
	{
	    addGlobalSymbol(globSyms[i]);
	}
    }

    checkFree(comdatList);
    checkFree(externs);
    checkFree(seglist);
    checkFree(grplist);
    checkFree(locSyms);
    checkFree(globSyms);
    checkFree(expdefs);
    
    expdefs=NULL;
    locSyms=NULL;
    globSyms=NULL;
    externs=NULL;
    grplist=NULL;
    seglist=NULL;
    comdatList=NULL;
    locSymCount=globSymCount=comdatCount=segcount=grpcount=extcount=expcount=0;
    
    return TRUE;
}

