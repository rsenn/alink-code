#include "alink.h"

static USHORT maxalloc=0xffff;
static USHORT minalloc=0;

CSWITCHENTRY EXESwitches[]={
    {"maxalloc",1,"Maximimum extra paragraphs to allocate"},
    {"minalloc",1,"Minimimum extra paragraphs to allocate"},
    {NULL,0,NULL}
};

BOOL EXEInitialise(PSWITCHPARAM sp)
{
    UINT i;
    PCHAR end;

    frameAlign=16; /* align to paragraphs when taking offsets relative to segments */
    
    if(!sp) return TRUE;
    for(;sp->name;++sp)
    {
	if(!strcmp(sp->name,"maxalloc"))
	{
	    errno=0;
	    i=strtoul(sp->params[0],&end,0);
	    if(errno || (*end))
	    {
		addError("Invalid number for maxalloc parameter");
		return FALSE;
	    }
	    if(i>USHRT_MAX)
	    {
		addError("Maxalloc value too large");
		return FALSE;
	    }
	    maxalloc=i;
	}
	else if(!strcmp(sp->name,"minalloc"))
	{
	    errno=0;
	    i=strtoul(sp->params[0],&end,0);
	    if(errno || (*end))
	    {
		addError("Invalid number for minalloc parameter");
		return FALSE;
	    }
	    if(i>USHRT_MAX)
	    {
		addError("Minalloc value too large");
		return FALSE;
	    }
	    minalloc=i;
	}
    }
    
    return TRUE;
}

static BOOL addEXERelocs(PSEG seg,UINT *relCount,UINT **relOfs,PSEG **relSeg)
{
    UINT i,ofs;
    PSEG f;
    PRELOC r;
    PSYMBOL pub;

    for(i=0;i<seg->contentCount;++i)
    {
	if(seg->contentList[i].flag!=SEGMENT) continue;
	if(!addEXERelocs(seg->contentList[i].seg,relCount,relOfs,relSeg))
	    return FALSE;
    }
    
    for(i=0;i<seg->relocCount;++i)
    {
	r=seg->relocs+i;
	
	/* get frame seg */
	if(r->fseg)
	{
	    f=r->fseg;
	}
	else if(r->fext)
	{
	    pub=r->fext->pubdef;
	    if(!pub)
	    {
		addError("Unresolved external %s",r->fext->name);
		return FALSE;
	    }
	    f=pub->seg;
	}
	else
	{
	    f=NULL;
	}
	if(f && f->absolute) continue; /* if absolute frame, skip */
	switch(r->base)
	{
	case REL_DEFAULT:
	    /* all default relocs are FRAMED in MSDOS EXE format */
	    r->base=REL_FRAME;
	    break;
	case REL_ABS:
	case REL_RVA:
	    f=NULL;
	    if(r->tseg)
	    {
		f=r->tseg;
	    }
	    else if(r->text)
	    {
		pub=r->text->pubdef;
		if(!pub)
		{
		    addError("Unresolved external %s",r->text->name);
		    return FALSE;
		}
		f=pub->seg;
	    }
	    break;
	default:
	    break;
	}
	
	if(!f || f->absolute) continue; /* continue if no frame, or it's absolute */
	/* check we've got a segment number relocation, otherwise skip */
	switch(r->rtype)
	{
	case REL_SEG:
	    ofs=r->ofs;
	    break;
	case REL_PTR16:
	    ofs=r->ofs+2;
	    break;
	case REL_PTR32:
	    ofs=r->ofs+4;
	    break;
	default:
	    continue;
	}

	(*relOfs)=checkRealloc((*relOfs),((*relCount)+1)*sizeof(UINT));
	(*relOfs)[(*relCount)]=ofs;
	(*relSeg)=checkRealloc((*relSeg),((*relCount)+1)*sizeof(UINT));
	(*relSeg)[(*relCount)]=seg;
	(*relCount)++;
    }
    return TRUE;
}

static BOOL getStack(PSEG src,PPSEG pseg,UINT *pofs)
{
    UINT i;
    UINT gotstack=FALSE;
    PSEG seg;
    UINT ofs;

    if(src->combine==SEGF_STACK)
    {
	(*pseg)=src;
	(*pofs)=src->length;
	
	return TRUE;
    }
    
    for(i=0;i<src->contentCount;++i)
    {
	if(src->contentList[i].flag!=SEGMENT) continue;
	if(getStack(src->contentList[i].seg,&seg,&ofs))
	{
	    if(gotstack)
	    {
		addError("Uncombined explicit stack segments");
		return FALSE;
	    }
	    
	    (*pseg)=seg;
	    (*pofs)=ofs;
	    gotstack=TRUE;
	}
    }
	
    return gotstack;
}



static PSEG buildEXEHeader(void)
{
    UINT relCount=0;
    UINT *relOfs=NULL;
    PSEG *relSeg=NULL;

    PUCHAR headbuf=NULL;
    UINT bufSize=0;
    PSEG header;
    UINT i,k,sofs;
    PSEG sseg,f;
    PDATABLOCK data;
    BOOL gotstack;
    
    /* build header */
    header=createSection("header",NULL,NULL,NULL,0,1);
    header->internal=TRUE;

    if(!addEXERelocs(spaceList[1],&relCount,&relOfs,&relSeg))
	return FALSE;

    if(relCount>65535)
    {
        addError("Too many relocations");
        return FALSE;
    }

    bufSize=EXE_HEADERSIZE+4*relCount;

    headbuf=checkMalloc(bufSize);

    memset(headbuf,0,bufSize);

    memset(headbuf,0,EXE_HEADERSIZE);
    
    headbuf[EXE_SIGNATURE]='M'; /* sig */
    headbuf[EXE_SIGNATURE+1]='Z';
    headbuf[EXE_RELOCPOS]=EXE_HEADERSIZE;

    headbuf[EXE_RELCOUNT]=relCount&0xff;
    headbuf[EXE_RELCOUNT+1]=relCount>>8;

    i=bufSize+0xf;
    i>>=4;
    headbuf[EXE_HEADSIZE]=i&0xff;
    headbuf[EXE_HEADSIZE+1]=i>>8;

    gotstack=getStack(spaceList[1],&sseg,&sofs);

    if(padsegments)
    {
	k=spaceList[1]->length;
    }
    else
    {
	k=getInitLength(spaceList[1]);
    }

    k+=(bufSize+0xf)&0xfffffff0;
    
    /* store file size */
    i=k%512;
    headbuf[EXE_NUMBYTES]=i&0xff;
    headbuf[EXE_NUMBYTES+1]=i>>8;

    i=(k+0x1ff)>>9;
    if(i>65535)
    {
	addError("File too large");
	return FALSE;
    }
    headbuf[EXE_NUMPAGES]=i&0xff;
    headbuf[EXE_NUMPAGES+1]=i>>8;

    /* get total amount of memory required */
    i=spaceList[1]->length+((bufSize+0xf)&0xfffffff0);
    /* minus amount in file=extra required */
    i-=k;
    /* in paragraphs */
    i+=0xf;
    i>>=4;
    i+=minalloc; /* add extra paragraphs requested */
    if(i>USHRT_MAX)
    {
	addError("Warning, minimum memory allocation overflow");
	i=USHRT_MAX;
    }

    if(i>maxalloc)
    {
	addError("Warning, minimum memory requirement exceeds maximum requested.");
	maxalloc=i;
    }

    headbuf[EXE_MINALLOC]=i&0xff;
    headbuf[EXE_MINALLOC+1]=(i>>8)&0xff;
    headbuf[EXE_MAXALLOC]=maxalloc&0xff;
    headbuf[EXE_MAXALLOC+1]=maxalloc>>8;

    /* build entries for relocation table */
    header->relocCount=relCount+(gotstart?1:0)+(gotstack?2:0);
    header->relocs=checkMalloc(sizeof(RELOC)*header->relocCount);
    for(i=0;i<relCount;i++)
    {
	header->relocs[i].rtype=REL_PTR16;
	header->relocs[i].base=REL_FRAME;
	header->relocs[i].tseg=header->relocs[i].fseg=relSeg[i];
	header->relocs[i].fext=header->relocs[i].text=NULL;
	header->relocs[i].disp=relOfs[i];
	header->relocs[i].ofs=EXE_HEADERSIZE+i*4;
    }
    if(gotstart)
    {
	header->relocs[relCount]=startaddr;
	header->relocs[relCount].rtype=REL_PTR16;
	header->relocs[relCount].base=REL_FRAME;
	header->relocs[relCount].ofs=EXE_STARTOFS;
	relCount++;
    }
    else
    {
        addError("Warning, no entry point specified");
    }

    if(gotstack)
    {
	f=sseg;
	/* go back up chain looking for enclosing group */
	while(f->parent)
	{
	    f=f->parent;
	    /* if we find one, use that as segment, updating offset */
	    if(f->group)
	    {
		break;
	    }
	}
	if(!f->group) f=sseg;
	
	header->relocs[relCount].rtype=REL_OFS16;
	header->relocs[relCount].base=REL_FRAME;
	header->relocs[relCount].ofs=EXE_STACKOFS;
	header->relocs[relCount].fseg=f;
	header->relocs[relCount].tseg=sseg;
	header->relocs[relCount].disp=sofs;
	relCount++;
	header->relocs[relCount].rtype=REL_SEG;
	header->relocs[relCount].base=REL_FRAME;
	header->relocs[relCount].ofs=EXE_STACKSEG;
	header->relocs[relCount].fseg=f;
	header->relocs[relCount].tseg=sseg;
	header->relocs[relCount].disp=sofs;
	relCount++;
    }
    else
    {
        diagnostic(DIAG_BASIC,"Warning - no stack");
    }
	

    data=createDataBlock(headbuf,0,bufSize,1);
    addData(header,data);
    return header;
}


BOOL EXEFinalise(PCHAR name)
{
    UINT i,j;
    PSEG a,s;

    for(i=0;i<globalSymbolCount;++i)
    {
	if(globalSymbols[i]->type==PUB_IMPORT)
	{
	    addError("Imported symbols not allowed in MSDOS executable output");
	    return FALSE;
	}
    }
    spaceCount=2;
    spaceList=checkMalloc(sizeof(PSEG)*2);
    
    a=createSection("Global",NULL,NULL,NULL,0,1);
    a->internal=TRUE;
    a->addressspace=TRUE;
    a->base=0;
    spaceList[1]=a;

    for(i=0;i<globalSegCount;++i)
    {
	if(!globalSegs[i]) continue;
	addSeg(a,globalSegs[i]);
	globalSegs[i]=NULL;
    }

    for(i=0;i<a->contentCount;++i)
    {
	s=a->contentList[i].seg;
	if(!s->absolute)
	{
	    s->section=(s->base+a->base) >> 4;
	}
	if(s->group)
	{
	    for(j=0;j<s->contentCount;++j)
	    {
		if(s->contentList[j].flag!=SEGMENT) continue;
		if(s->contentList[j].seg->group
		   || s->contentList[j].seg->absolute) continue;
		s->contentList[j].seg->section=
		    (s->contentList[j].seg->base+s->base+a->base)>>4;
	    }
	}
    }
	
    spaceList[0]=buildEXEHeader();
    spaceList[1]->filepos=(spaceList[0]->length+0xf)&0xfffffff0;

    performFixups(spaceList[0]);
    performFixups(spaceList[1]);
    return TRUE;
}
