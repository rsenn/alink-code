#include "alink.h"

PDATABLOCK createDataBlock(PUCHAR p,UINT offset,UINT length,UINT align)
{
    PDATABLOCK d;

    d=(PDATABLOCK)checkMalloc(sizeof(DATABLOCK));
    d->data=checkMalloc(length);
    if(p)
    {
	memcpy(d->data,p,length);
    }
    else
    {
	memset(d->data,0,length);
    }
    d->offset=offset;
    d->length=length;
    d->align=align;

    return d;
}

void freeDataBlock(PDATABLOCK d)
{
    if(!d) return;
    checkFree(d->data);
    checkFree(d);
}

PSEG createSection(PCHAR name,PCHAR class,PCHAR sortKey,PMODULE mod,UINT length,
		  UINT align)
{
    PSEG s;

    s=(PSEG)checkMalloc(sizeof(SEG));
    s->name=checkStrdup(name);
    s->class=checkStrdup(class);
    s->sortKey=checkStrdup(sortKey);
    s->mod=mod;
    s->base=0;
    s->section=-1;
    s->filepos=0;
    s->fpset=FALSE;
    s->length=length;
    s->align=align;
    if(getBitCount(align)!=1)
    {
	addError("Invalid alignment %08lX for segment '%s'\n",align,name?name:"");
	s->align=1;
    }

    s->use32=defaultUse32;

    s->absolute=FALSE;
    s->combine=SEGF_PRIVATE;
    s->code=s->initdata=s->uninitdata=FALSE;
    s->read=s->write=FALSE;
    s->execute=FALSE;
    s->moveable=FALSE;
    s->discardable=FALSE;
    s->shared=FALSE;
    s->discard=FALSE;
    s->nocache=FALSE;
    s->nopage=FALSE;
    s->addressspace=s->group=FALSE;
    s->internal=FALSE;

    s->contentCount=0;
    s->contentList=NULL;

    s->parent=NULL;

    s->lines=NULL;
    s->lineCount=0;

    s->relocs=NULL;
    s->relocCount=0;

    return s;
}

PSEG createDuplicateSection(PSEG old)
{
    PSEG s;

    if(!old) return NULL;

    s=(PSEG)checkMalloc(sizeof(SEG));
    s->name=checkStrdup(old->name);
    s->class=checkStrdup(old->class);
    s->sortKey=checkStrdup(old->sortKey);
    s->mod=old->mod;
    s->base=0;
    s->section=-1;
    s->filepos=0;
    s->fpset=FALSE;
    s->length=0;
    s->align=old->align;

    s->combine=old->combine;
    s->addressspace=old->addressspace;
    s->group=old->group;
    s->read=old->read;
    s->write=old->write;
    s->absolute=old->absolute;
    s->use32=old->use32;
    s->moveable=old->moveable;
    s->discardable=old->discardable;
    s->shared=old->shared;
    s->code=old->code;
    s->initdata=old->initdata;
    s->uninitdata=old->uninitdata;
    s->execute=old->execute;
    s->discard=old->discard;
    s->nocache=old->nocache;
    s->nopage=old->nopage;
    s->internal=old->internal;

    s->parent=NULL;

    s->contentCount=0;
    s->contentList=NULL;

    s->lines=NULL;
    s->lineCount=0;

    s->relocs=NULL;
    s->relocCount=0;

    return s;
}


void freeSection(PSEG s)
{
    UINT i;

    if(!s) return;

    for(i=0;i<s->contentCount;i++)
    {
	switch(s->contentList[i].flag)
	{
	case DATA:
	    freeDataBlock(s->contentList[i].data);
	    break;
	case SEGMENT:
	    freeSection(s->contentList[i].seg);
	    break;
	}
    }
    checkFree(s->contentList);
    checkFree(s->name);
    checkFree(s->class);
    checkFree(s->sortKey);
    checkFree(s->lines);
    checkFree(s);
}

static void realignSeg(PSEG s)
{
    UINT x,i,oldlength;
    PCONTENT c;

    while(s)
    {
	oldlength=s->length;

	if(!s->contentCount)
	{
	    s->length=0;
	}
	else
	{
	    x=0;
	    for(i=0;i<s->contentCount;++i)
	    {
		if(s->contentList[i].flag==DATA)
		{
		    if(x>s->contentList[i].data->offset)
		    {
			addError("data overlap in segment %s\n",s->name);
			return;
		    }

		    x=s->contentList[i].data->offset;
		    x+=s->contentList[i].data->length;
		    continue;
		}

		if(s->contentList[i].seg->absolute) continue;
		x+=s->contentList[i].seg->align-1;
		x&=UINT_MAX-(s->contentList[i].seg->align-1);
		s->contentList[i].seg->base=x;
		x+=s->contentList[i].seg->length;
	    }
	    c=s->contentList+s->contentCount-1;
	    if(c->flag==SEGMENT)
	    {
		if(!c->seg->absolute)
		{
		    s->length=c->seg->base+c->seg->length;
		}
	    }
	    else
	    {
		s->length=c->data->offset+c->data->length;
	    }

	}

	if(oldlength==s->length) break; /* we're done if length hasn't changed */
	s=s->parent; /* otherwise go up a level */
    }
}


PSEG removeContent(PSEG s,UINT i)
{
    PSEG r;

    if(!s) return NULL; /* quit if no seg */
    if(i>=s->contentCount) return NULL; /* or bad content index */
    if(s->contentList[i].flag==DATA) return NULL; /* or specified content is data */

    /* get seg pointer */
    r=s->contentList[i].seg;

    /* move down data */
    s->contentCount--;
    memmove(s->contentList+i,s->contentList+i+1,(s->contentCount-i)*sizeof(CONTENT));

    /* re align seg content, and parent segs, now we've removed this entry */
    realignSeg(s);

    /* return the removed seg */
    return r;
}


PSEG addSeg(PSEG s,PSEG c)
{
    UINT i;
    PSEG p;

    if(!s)
    {
	addError("Attempt to add to NULL segment");
	return NULL;
    }
    if(!c)
    {
	addError("Attempt to add a NULL entry to segment %s",s->name);
	return NULL;
    }
    if(c->addressspace)
    {
	addError("Attempt to add an address space to another segment");
	return NULL;
    }

    /* add child segment to content list for parent */
    s->contentList=(PCONTENT)checkRealloc(s->contentList,
					  (s->contentCount+1)*sizeof(CONTENT));
    s->contentList[s->contentCount].flag=SEGMENT;
    s->contentList[s->contentCount].seg=c;
    s->contentCount++;

    /* flag parent-child relationship */
    c->parent=s;

    if(c->absolute) return s; /* don't adjust lengths for absolute segs */

    if((s->use32 != c->use32) && (s->contentCount-1))
    {
	diagnostic(DIAG_BASIC,"Warning: combining USE32 %s %s with USE16 %s %s\n",
	       (s->use32?s->group:c->group)?"group":"segment",
	       s->use32?s->name:c->name,
	       (s->use32?c->group:s->group)?"group":"segment",
	       s->use32?c->name:s->name);
    }

    p=c;
    while(p->parent)
    {
	/* positive flags => set if any combined seg has them */
	p->parent->code|=p->code;
	p->parent->initdata|=p->initdata;
	p->parent->uninitdata|=p->uninitdata;
	p->parent->execute|=p->execute;
	p->parent->read|=p->read;
	p->parent->write|=p->write;
	p->parent->use32|=p->use32;
	p->parent->nopage|=p->nopage;
	p->parent->nocache|=p->nocache;

	/* mutual agreement flags => lost if any combined seg doesn't have them */
	p->parent->moveable&=p->moveable;
	p->parent->discardable&=p->discardable;
	p->parent->shared&=p->shared;
	p=p->parent;
    }

    /* set base of child, and adjust length of parent */
    i=s->length;
    i+=c->align-1;
    i&=UINT_MAX-(c->align-1);
    c->base=i;

    i-=s->length; /* get extra length */
    s->length+=i+c->length;

    if(c->align>s->align) s->align=c->align; /* update alignment */

    /* propogate length increase up parent tree */
    if(s->parent)
    {
	realignSeg(s->parent);
    }

    return s;
}

PSEG addCommonSeg(PSEG s,PSEG c)
{
    UINT j;

    /* add contents of child to parent at correct positions */
    for(j=0;j<c->contentCount;++j)
    {
	if(c->contentList[j].flag==DATA)
	{
	    c->contentList[j].data->offset+=c->base; /* get offset within parent seg */
	    addFixedData(s,c->contentList[j].data);
	}
	else
	{
	    /* sub-segs of a common seg are treated as common too */
	    c->contentList[j].seg->base+=c->base; /* at correct offset */
	    addCommonSeg(s,c->contentList[j].seg);
	}
    }
    checkFree(c->contentList);
    c->contentCount=0;
    c->length=0;
    c->base=0;
    /* add child seg at start of parent */
    s->contentList=checkRealloc(s->contentList,(s->contentCount+1)*sizeof(CONTENT));
    if(s->contentCount)
	memmove(s->contentList+1,s->contentList,s->contentCount*sizeof(CONTENT));
    s->contentList[0].flag=SEGMENT;
    s->contentList[0].seg=c;
    s->contentCount++;
    /* update alignment */
    if(c->align>s->align) s->align=c->align;
    /* realign parent */
    if(s->parent)
    {
	realignSeg(s->parent);
    }
    return s;
}


PSEG addData(PSEG s,PDATABLOCK c)
{
    UINT i;

    if(!s)
    {
	addError("Attempt to add to NULL segment");
	return NULL;
    }
    if(s->absolute)
    {
	addError("Attempt to add data to an absolute segment");
	return NULL;
    }
    if(!c)
    {
	addError("Attempt to add a NULL entry to segment %s",s->name);
	return NULL;
    }

    /* add datablock to content list for parent */
    s->contentList=(PCONTENT)checkRealloc(s->contentList,
					  (s->contentCount+1)*sizeof(CONTENT));
    s->contentList[s->contentCount].flag=DATA;
    s->contentList[s->contentCount].data=c;
    s->contentCount++;

    /* set base of child, and adjust length of parent */
    i=s->length;
    i+=c->align-1;
    i&=(0xffffffff-(c->align-1));
    c->offset=i;
    i-=s->length;
    s->length+=i+c->length;

    if(c->align>s->align) s->align=c->align; /* update alignment */

    /* propogate length increase up parent tree */
    if(s->parent)
    {
	realignSeg(s->parent);
    }

    return s;
}

PSEG addFixedData(PSEG s,PDATABLOCK c)
{
    UINT i,b,l;
    PCONTENT ct;

    if(!s)
    {
	addError("Attempt to add to NULL segment");
	return NULL;
    }
    if(s->absolute)
    {
	addError("Attempt to add data to an absolute segment");
	return NULL;
    }
    if(!c)
    {
	addError("Attempt to add a NULL entry to segment %s",s->name);
	return NULL;
    }

    if((c->offset+c->length)>s->length)
    {
	s->length=c->offset+c->length;
    }


    for(i=0;i<s->contentCount;i++)
    {
	ct=s->contentList+i;

	switch(ct->flag)
	{
	case SEGMENT:
	    b=ct->seg->base;
	    l=ct->seg->length;
	    break;
	case DATA:
	default:
	    b=ct->data->offset;
	    l=ct->data->length;
	    break;
	}
	if((b+l) <= c->offset) continue;
	if(b < (c->offset+c->length))
	{
	    addError("Attempt to add data block that overlaps with existing data in segment %s",s->name);
	    return NULL;
	}
	break;
    }

    /* add datablock to content list for parent */
    s->contentList=(PCONTENT)checkRealloc(s->contentList,
					  (++s->contentCount)*sizeof(CONTENT));
    /* if we're adding in the middle of the array, make space */
    if(i!=(s->contentCount-1))
    {
	memmove(s->contentList+i+1,s->contentList+i,(s->contentCount-i-1)*sizeof(CONTENT));
    }

    s->contentList[i].flag=DATA;
    s->contentList[i].data=c;

    if(c->align>s->align) s->align=c->align; /* update alignment */

    /* propogate length increase and/or alignment change up parent tree */
    if(s->parent)
    {
	realignSeg(s->parent);
    }

    return s;
}

UINT getInitLength(PSEG s)
{
    UINT i;
    UINT ofs=0,ofs2;

    for(i=0;i<s->contentCount;++i)
    {
	if(s->contentList[i].flag==SEGMENT)
	{
	    if(s->contentList[i].seg->absolute) continue;
	    ofs2=getInitLength(s->contentList[i].seg);
	    if(ofs2)
		ofs2+=s->contentList[i].seg->base;
	}
	else
	{
	    ofs2=s->contentList[i].data->length;
	    if(ofs2)
		ofs2+=s->contentList[i].data->offset;
	}
	if(ofs2>ofs) ofs=ofs2;
    }
    return ofs;
}

BOOL writeSeg(FILE *f,PSEG s)
{
    UINT i,j;
    PDATABLOCK d;

    /* success if no segment, failure if no file */
    if(!s) return TRUE;
    if(!f) return FALSE;

    i=ftell(f);
    if(i>s->filepos)
    {
	addError("Segment overlap in file");
	return FALSE;
    }

    for(i=0;i<s->contentCount;i++)
    {
	switch(s->contentList[i].flag)
	{
	case DATA:
	    d=s->contentList[i].data;
	    /* no output for zero-length data blocks */
	    if(!d->length) break;
	    j=s->filepos+d->offset-ftell(f); /* get number of zeroes to pad with */
	    while(j)
	    {
		if(fputc(0,f)==EOF)
		{
		    addError("Error writing to file");
		    return FALSE;
		}
		j--;
	    }
	    if(fwrite(d->data,1,d->length,f)!=d->length)
	    {
		addError("Error writing to file");
		return FALSE;
	    }

	    break;
	case SEGMENT:
	    if(!s->contentList[i].seg->absolute)
	    {
		if(!s->contentList[i].seg->fpset)
		    s->contentList[i].seg->filepos=s->filepos+s->contentList[i].seg->base;
		if(!writeSeg(f,s->contentList[i].seg))
                    return FALSE;
	    }

	    break;
	}
    }
    return TRUE;
}
