#include "alink.h"

void performFixups(PSEG s)
{
    UINT i,j;
    PSEG f,t;
    PRELOC r;
    UINT offset;
    INT section;
    UINT disp;
    PSYMBOL pub;
    UINT delta;
    PDATABLOCK d;

    if(!s) return;

    /* recurse down tree */
    for(i=0;i<s->contentCount;++i)
    {
	if(s->contentList[i].flag==SEGMENT)
	{
	    performFixups(s->contentList[i].seg);
	}
    }

    for(i=0;i<s->relocCount;++i)
    {
	r=s->relocs+i;
	for(j=0;j<s->contentCount;++j)
	{
	    if(s->contentList[j].flag!=DATA) continue;
	    if((s->contentList[j].data->offset <= r->ofs)
	       && ((s->contentList[j].data->offset + s->contentList[j].data->length) > r->ofs)) break;
	}
	if(j==s->contentCount)
	{
	    addError("Fixup location %08lX is outside any data in segment %s",r->ofs,s->name);
	    continue;
	}
	d=s->contentList[j].data;
	offset=r->ofs-d->offset;

	t=NULL;
	disp=0;
	if(r->tseg)
	{
	    t=r->tseg;
	    disp=t->base;
	}
	else if(r->text)
	{
	    pub=r->text->pubdef;
	    if(!pub)
	    {
		addError("Unresolved external %s",r->text->name);
		continue;
	    }
	    
	    t=pub->seg;
	    if(t)
		disp=t->base+pub->ofs;
	}
	if(!t)
	{
	    addError("Fixup target unresolved");
	    continue;
	}
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
		continue;
	    }
	    f=pub->seg;
	}
	else
	{
	    f=NULL;
	}
	if(f && f->absolute && (r->base==REL_DEFAULT))
	{
	    r->base=REL_FRAME;
	}
	
	switch(r->base)
	{
	case REL_ABS:
	case REL_DEFAULT:
	case REL_RVA:
	    section=t->section;
	    while(t->parent)
	    {
		t=t->parent;
		if(section<0) section=t->section;
		disp+=t->base;
	    }
	    if(r->base==REL_RVA)
	    {
		disp-=t->base;
	    }
	    break;
	case REL_FILEPOS:
	    if(t->absolute)
	    {
		addError("Attempt to get file position of absolute segment %s",t->name);
		continue;
	    }
	    section=t->section;
	    while(t->parent && !t->fpset)
	    {
		t=t->parent;
		if(section<0) section=t->section;
		disp+=t->base;
	    }
	    disp-=t->base;
	    if(t->fpset)
		disp+=t->filepos;
	    if(f)
	    {
		if(f->absolute)
		{
		    addError("Attempt to get file position of absolute segment %s",f->name);
		    continue;
		}
		while(f->parent && !f->fpset)
		{
		    f=f->parent;
		    if(section<0) section=f->section;
		    disp-=f->base;
		}
		disp+=f->base;
		if(f->fpset)
		    disp-=f->filepos;
	    }
	    break;
	case REL_SELF:
	    section=t->section;
	    while(t->parent)
	    {
		t=t->parent;
		if(section<0) section=t->section;
		disp+=t->base;
	    }
	    f=s;
	    disp-=r->ofs;
	    while(f)
	    {
		disp-=f->base;
		f=f->parent;
	    }
	    break;
	case REL_FRAME:
	    if(!f)
	    {
		addError("Unspecified frame");
		continue;
	    }
	    while(t->parent)
	    {
		t=t->parent;
		disp+=t->base;
	    }
	    
	    section=f->section;
	    disp-=f->base;
	    if(section>=0)
		disp+=f->base&(frameAlign-1);
	    while(f->parent && !f->absolute)
	    {
		f=f->parent;
		if(section<0)
		{
		    section=f->section;
		    if(section>=0)
			disp+=f->base&(frameAlign-1);
		}
		
		if(section>=0)
		{
		    disp-=f->base;
		}
	    }
	    break;
	default:
	    addError("Unspecified reloc base");
	    continue;
	}
	disp+=r->disp;
	switch(r->rtype)
	{
	case REL_OFS8:
	    delta=d->data[offset];
	    delta+=disp;
	    d->data[offset]=delta&0xff;
	    break;
	case REL_OFS16:
	    if(disp>=0x10000)
	    {
		addError("16-bit offset limit exceeded");
		continue;
	    }
	    
	    delta=d->data[offset]+(d->data[offset+1]<<8);
	    delta+=disp;
	    d->data[offset]=delta&0xff;
	    d->data[offset+1]=(delta>>8)&0xff;
	    break;
	case REL_OFS32:
	    delta=d->data[offset]+(d->data[offset+1]<<8)+(d->data[offset+2]<<16)+(d->data[offset+3]<<24);
	    delta+=disp;
	    d->data[offset]=delta&0xff;
	    d->data[offset+1]=(delta>>8)&0xff;
	    d->data[offset+2]=(delta>>16)&0xff;
	    d->data[offset+3]=(delta>>24)&0xff;
	    break;
	case REL_BYTE2:
	    delta=d->data[offset];
	    delta+=disp>>8;
	    d->data[offset]=delta&0xff;
	    break;
	case REL_SEG:
	    if(section<0)
	    {
		addError("Segmented reloc not permitted for specified frame");
		continue;
	    }
	    if(section>=0x10000)
	    {
		addError("Section number too large %08lX",section);
		continue;
	    }
	    
	    delta=d->data[offset]+(d->data[offset+1]<<8);
	    delta+=section;
	    d->data[offset]=delta&0xff;
	    d->data[offset+1]=(delta>>8)&0xff;
	    break;
	case REL_PTR16:
	    if(section<0)
	    {
		addError("Segmented reloc not permitted for specified frame");
		continue;
	    }
	    if(section>=0x10000)
	    {
		addError("Section number too large %08lX",section);
		continue;
	    }
	    if(disp>=0x10000)
	    {
		addError("16-bit offset limit exceeded");
		continue;
	    }
	    
	    delta=d->data[offset]+(d->data[offset+1]<<8);
	    delta+=disp;
	    d->data[offset]=delta&0xff;
	    d->data[offset+1]=(delta>>8)&0xff;

	    delta=d->data[offset+2]+(d->data[offset+3]<<8);
	    delta+=section;
	    d->data[offset+2]=delta&0xff;
	    d->data[offset+3]=(delta>>8)&0xff;
	    break;
	case REL_PTR32:
	    if(section<0)
	    {
		addError("Segmented reloc not permitted for specified frame");
		continue;
	    }
	    delta=d->data[offset]+(d->data[offset+1]<<8)+(d->data[offset+2]<<16)+(d->data[offset+3]<<24);
	    delta+=disp;
	    d->data[offset]=delta&0xff;
	    d->data[offset+1]=(delta>>8)&0xff;
	    d->data[offset+2]=(delta>>16)&0xff;
	    d->data[offset+3]=(delta>>24)&0xff;

	    delta=d->data[offset+4]+(d->data[offset+5]<<8);
	    delta+=section;
	    d->data[offset+4]=delta&0xff;
	    d->data[offset+5]=(delta>>8)&0xff;
	    break;
	default:
	    addError("Invalid relocation type");
	    continue;
	    break;
	}
    }
}
