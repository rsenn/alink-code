#include "alink.h"

struct lineref
{
    PCHAR filename;
    UINT num;
    PSEG seg;
    UINT ofs;
};

typedef struct lineref LINEREF,*PLINEREF;

static PLINEREF globalLines=NULL;
static UINT globalLineCount=0;

static void dumpSegment(FILE *f,PSEG s)
{
    PSEG p;
    UINT offset;
    UINT i;
    INT section;
    
    if(!s) return;
    p=s;
    offset=s->base;
    section=s->section;
    i=0;
    while(p->parent)
    {
	++i;
	p=p->parent;
	offset+=p->base;
	if(section<0)
	{
	    section=p->section;
	}
    }

    if(section>=0)
    {
	fprintf(f,"%04lX:",section);
    }
    else
    {
	fprintf(f,"    :");
    }
    
    
    fprintf(f,"%08lX %08lX ",offset,s->length);
    while(i)
    {
	fputc(' ',f);
	--i;
    }
    if(s->addressspace)
    {
	fprintf(f,"SPACE");
	fprintf(f," %-26s",s->name?s->name:"");
    }
    else if(s->group)
    {
	fprintf(f,"GROUP");
	fprintf(f," %-26s",s->name?s->name:"");
    }
    else
    {
	fprintf(f,"%-15s",s->name?s->name:"");
	fprintf(f,":%-15s",s->class?s->class:"");
    }
    if(s->use32) fprintf(f," U32");
    
    if(s->mod)
    {
	for(i=0;i<moduleCount;++i)
	{
	    if(modules[i]==s->mod) break;
	}
	if(i<moduleCount)
	    fprintf(f,"Module %li",i);
	
	if(s->mod->file) fprintf(f," :%s",s->mod->file);
    }

    fprintf(f,"\n");
    
    for(i=0;i<s->contentCount;++i)
    {
	if(s->contentList[i].flag==SEGMENT)
	{
	    /*	    if(s->group || s->addressspace)*/
		dumpSegment(f,s->contentList[i].seg);
	}
    }
}

void getLines(PSEG s)
{
    UINT i;
    
    if(s->mod && s->lineCount)
    {
	globalLines=checkRealloc(globalLines,(globalLineCount+s->lineCount)*sizeof(LINEREF));
	for(i=0;i<s->lineCount;++i)
	{
	    globalLines[globalLineCount].filename=s->mod->sourceFiles[s->lines[i].sourceFile-1];
	    globalLines[globalLineCount].num=s->lines[i].num;
	    globalLines[globalLineCount].seg=s;
	    globalLines[globalLineCount].ofs=s->lines[i].offset;
	    
	    globalLineCount++;
	}
    }
    
    for(i=0;i<s->contentCount;++i)
    {
	if(s->contentList[i].flag==SEGMENT)
	{
	    getLines(s->contentList[i].seg);
	}
    }
}

void dumpOldSegment(PFILE afile,PSEG seg)
{
    if(seg->internal)
    {
        return;
    }
    
    fprintf(afile,"SEGMENT %s ",
            (seg->name?seg->name:""));
    switch(seg->combine)
    {
    case SEGF_PRIVATE:
        fprintf(afile,"PRIVATE ");
        break;
    case SEGF_PUBLIC:
        fprintf(afile,"PUBLIC ");
        break;
    case SEGF_STACK:
        fprintf(afile,"STACK ");
        break;
    case SEGF_COMMON:
        fprintf(afile,"COMMON ");
        break;
    default:
        fprintf(afile,"unknown ");
        break;
    }
    if(seg->use32)
    {
        fprintf(afile,"USE32 ");
    }
    else
    {
        fprintf(afile,"USE16 ");
    }
    if(seg->absolute)
    {
        fprintf(afile,"AT 0%04lXh ",seg->section);
    }
    else
    {
        switch(seg->align)
        {
        case 1:
            fprintf(afile,"BYTE ");
            break;
        case 2:
            fprintf(afile,"WORD ");
            break;
        case 16:
            fprintf(afile,"PARA ");
            break;
        case 0x100:
            fprintf(afile,"PAGE ");
            break;
        case 4:
            fprintf(afile,"DWORD ");
            break;
        case 0x1000:
            fprintf(afile,"MEMPAGE ");
            break;
        default:
            fprintf(afile,"unknown ");
        }
    }
    
    if(seg->class)
        fprintf(afile,"'%s'\n",seg->class);
    else
        fprintf(afile,"\n");
    fprintf(afile,"  at %08lX, length %08lX\n",seg->base,seg->length);
}

void dumpOldGroup(PFILE afile,PSEG seg)
{
    UINT j;
    fprintf(afile,"\nGroup %s:\n",seg->name?seg->name:"");
    for(j=0;j<seg->contentCount;j++)
    {
        if(seg->contentList[j].flag==SEGMENT)
        {
            fprintf(afile,"    %s\n",seg->contentList[j].seg->name?seg->contentList[j].seg->name:"");
        }
    }
}

void writeOldImports(PFILE afile)
{
    UINT importCount;
    UINT i;

    for(i=0,importCount=0;i<globalSymbolCount;++i)
    {
	if(globalSymbols[i]->type!=PUB_IMPORT) continue; /* ignore non-imports */
	if(!globalSymbols[i]->refCount) continue; /* ignore unrefenced symbols */
        ++importCount;
    }

    if(!importCount) return;

    fprintf(afile,"\n %li imports:\n",importCount);
    for(i=0;i<globalSymbolCount;++i)
    {
	if(globalSymbols[i]->type!=PUB_IMPORT) continue; /* ignore non-imports */
	if(!globalSymbols[i]->refCount) continue; /* ignore unrefenced symbols */

        fprintf(afile,"%s=%s:%s(%i)\n",globalSymbols[i]->name,globalSymbols[i]->dllname,globalSymbols[i]->impname?globalSymbols[i]->impname:"",
                globalSymbols[i]->ordinal);
    }
    
}


void generateOldMap(PCHAR mapname)
{
    long i,j;
    PFILE afile;

    afile=fopen(mapname,"wt");
    if(!afile)
    {
	addError("Error opening map file %s",mapname);
	return;
    }
    diagnostic(DIAG_VERBOSE,"Generating map file %s\n",mapname);

    for(i=0;i<spaceCount;++i)
    {
        if(!spaceList[i]) continue;
        
        for(j=0;j<spaceList[i]->contentCount;++j)
        {
            if(spaceList[i]->contentList[j].flag==SEGMENT)
            {
		dumpOldSegment(afile,spaceList[i]->contentList[j].seg);
            }
        }
    }
    
    for(i=0;i<spaceCount;++i)
    {
        if(!spaceList[i]) continue;
        
        for(j=0;j<spaceList[i]->contentCount;++j)
        {
            if(spaceList[i]->contentList[j].flag==SEGMENT)
            {
                if(spaceList[i]->contentList[j].seg->group)
                {
                    dumpOldGroup(afile,spaceList[i]->contentList[j].seg);
                }
            }
        }
    }
    

    if(globalSymbolCount)
    {
	fprintf(afile,"\npublics:\n");
    }
    for(i=0;i<globalSymbolCount;++i)
    {
        PSEG p,q;
	if(globalSymbols[i]->type==PUB_LIBSYM) continue;
	if(globalSymbols[i]->type==PUB_IMPORT) continue; 
	j=globalSymbols[i]->ofs;
	p=globalSymbols[i]->seg;
        q=globalSymbols[i]->seg;
	while(p && p->parent)
	{
	    j+=p->base;
            q=p;
	    p=p->parent;
	}
        if(q)
        {
            j-=q->base;
        }
        
	
	fprintf(afile,"%s at %s:%08lX\n",globalSymbols[i]->name?globalSymbols[i]->name:"",q?(q->name?q->name:""):"Absolute",j);
    }
    
    if(globalExportCount)
    {
	fprintf(afile,"\n %li exports:\n",globalExportCount);
	for(i=0;i<globalExportCount;i++)
	{
	    fprintf(afile,"%s(%lu=%s)\n",globalExports[i]->exp_name,globalExports[i]->ordinal,globalExports[i]->int_name);
	}
    }

    writeOldImports(afile);
    fclose(afile);
}



void generateMap(PCHAR mapname)
{
    UINT i,j,section,ofs,addr;
    PSEG p;
    FILE *afile;

    afile=fopen(mapname,"wt");
    if(!afile)
    {
	addError("Error opening map file %s",mapname);
	return;
    }
    diagnostic(DIAG_VERBOSE,"Generating map file %s\n",mapname);

    for(i=0;i<moduleCount;++i)
    {
	fprintf(afile,"Module   : %li\n",i);
	if(modules[i]->name)
	    fprintf(afile," Name    : %s\n",modules[i]->name);
	if(modules[i]->file)
	    fprintf(afile," File    : %s\n",modules[i]->file);
	if(modules[i]->fmt)
	    fprintf(afile," Format  : %s\n",modules[i]->fmt->description);
	if(modules[i]->compiler)
	    fprintf(afile," Compiler: %s\n",modules[i]->compiler);
	for(j=0;j<modules[i]->depCount;++j)
	{
	    fprintf(afile," Depends on: %s\n",modules[i]->dependencies[j]);
	}
	
    }

    for(i=0;i<spaceCount;++i)
    {
	dumpSegment(afile,spaceList[i]);
    }
    
    {
        UINT pubSymCount=0;
        
        for(i=0;i<globalSymbolCount;++i)
        {
            if(globalSymbols[i]->type==PUB_LIBSYM) continue;
            ++pubSymCount;
        }
        
        fprintf(afile,"\n%lu public symbols:\n",pubSymCount);
    }

    for(i=0;i<globalSymbolCount;++i)
    {
	if(globalSymbols[i]->type==PUB_LIBSYM) continue;
	j=globalSymbols[i]->ofs;
	p=globalSymbols[i]->seg;
	while(p)
	{
	    j+=p->base;
	    p=p->parent;
	}
	
	fprintf(afile,"Symbol %s at %08lX%s\n",globalSymbols[i]->name,j,globalSymbols[i]->refCount?"":" (Idle)");
    }

    for(i=0;i<spaceCount;++i)
    {
	getLines(spaceList[i]);
    }

    fprintf(afile,"\n%li Line numbers:\n",globalLineCount);
    for(i=0;i<globalLineCount;++i)
    {
	p=globalLines[i].seg;
	ofs=globalLines[i].ofs;
	addr=globalLines[i].ofs;
	section=p->section;
	while(p)
	{
	    addr+=p->base;
	    if(section<0)
	    {
		section=p->section;
	    }
	    if(section<0)
	    {
		ofs+=p->base;
	    }
	    p=p->parent;
	}
	
	fprintf(afile,"%s: %8lu = %04lX:%08lX = %08lX\n",globalLines[i].filename,globalLines[i].num,section,ofs,addr);
    }
    

    fclose(afile);
}
