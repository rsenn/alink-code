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
	    printf("Fixup location %08lX is outside any data in segment %s\n",r->ofs,s->name);
	    exit(1);
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
		printf("Unresolved external %s\n",r->text->name);
		exit(1);
	    }
	    
	    t=pub->seg;
	    if(t)
		disp=t->base+pub->ofs;
	}
	if(!t)
	{
	    printf("Fixup target unresolved\n");
	    exit(1);
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
		printf("Unresolved external %s\n",r->fext->name);
		exit(1);
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
		printf("Attempt to get file position of absolute segment\n");
		exit(1);
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
		    printf("Attempt to get file position of absolute segment\n");
		    exit(1);
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
		printf("Unspecified frame\n");
		exit(1);
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
	    printf("Unspecified reloc base\n");
	    exit(1);
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
		printf("16-bit offset limit exceeded\n");
		exit(1);
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
		printf("Segmented reloc not permitted for specified frame\n");
		exit(1);
	    }
	    if(section>=0x10000)
	    {
		printf("Section number too large %08lX\n",section);
		exit(1);
	    }
	    
	    delta=d->data[offset]+(d->data[offset+1]<<8);
	    delta+=section;
	    d->data[offset]=delta&0xff;
	    d->data[offset+1]=(delta>>8)&0xff;
	    break;
	case REL_PTR16:
	    if(section<0)
	    {
		printf("Segmented reloc not permitted for specified frame\n");
		exit(1);
	    }
	    if(section>=0x10000)
	    {
		printf("Section number too large %08lX\n",section);
		exit(1);
	    }
	    if(disp>=0x10000)
	    {
		printf("16-bit offset limit exceeded\n");
		exit(1);
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
		printf("Segmented reloc not permitted for specified frame\n");
		exit(1);
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
	    printf("Invalid relocation type\n");
	    exit(1);
	    break;
	}
    }
}
