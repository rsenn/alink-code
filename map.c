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


void generateMap(PCHAR mapname)
{
    UINT i,j,section,ofs,addr;
    PSEG p;
    FILE *afile;

    afile=fopen(mapname,"wt");
    if(!afile)
    {
	printf("Error opening map file %s\n",mapname);
	exit(1);
    }
    printf("Generating map file %s\n",mapname);

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

    fprintf(afile,"\n%i public symbols:\n",globalSymbolCount);

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
	
	fprintf(afile,"%s: %8lu = %04X:%08lX = %08lX\n",globalLines[i].filename,globalLines[i].num,section,ofs,addr);
    }
    

    fclose(afile);
}
