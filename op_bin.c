#include "alink.h"

static UINT imageBase=0;

CSWITCHENTRY BINSwitches[]={
    {"base",1,"Set image Base"},
    {NULL,0,NULL}
};

BOOL COMInitialise(PSWITCHPARAM options)
{
    imageBase=0x100;
    
    return TRUE;
}

BOOL BINInitialise(PSWITCHPARAM sp)
{
    UINT i;
    PCHAR end;
    
    for(;sp && sp->name;++sp)
    {
	if(!strcmp(sp->name,"base"))
	{
	    errno=0;
	    i=strtoul(sp->params[0],&end,0);
	    if(errno || (*end))
	    {
		addError("Invalid number (%s) for base parameter",sp->params[0]);
		return FALSE;
	    }
	    imageBase=i;
	}
    }
    
    return TRUE;
}


BOOL BINFinalise(PCHAR name)
{
    PSEG a;
    UINT i;

    for(i=0;i<globalSymbolCount;++i)
    {
	if(globalSymbols[i]->type==PUB_IMPORT)
	{
	    addError("Imported symbols not allowed for binary output");
	    return FALSE;
	}
    }
    
    
    a=createSection("Global",NULL,NULL,NULL,0,1);
    a->internal=TRUE;
    a->addressspace=TRUE;
    a->base=imageBase;
    
    for(i=0;i<globalSegCount;++i)
    {
	if(!globalSegs[i]) continue;
	addSeg(a,globalSegs[i]);
	globalSegs[i]=NULL;
    }
    spaceCount=1;
    spaceList=checkMalloc(sizeof(PSEG));
    spaceList[0]=a;

    performFixups(a);

    return TRUE;
}

