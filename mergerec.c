#include "mergerec.h"
#include "alink.h"

PMERGEREC createMergeRec(PCHAR sourceName,PCHAR targetName)
{
    PMERGEREC rec=(PMERGEREC)checkMalloc(sizeof(MERGEREC));

    rec->sourceName=checkStrdup(sourceName);
    rec->targetName=checkStrdup(targetName);
    
    return rec;
}

PCHAR lookupTargetName(PCHAR sourceName)
{
    unsigned i=0;
    for(i=0;i<mergeCount;++i)
    {
        if(!strcmp(mergeList[i]->sourceName,sourceName))
        {
            return mergeList[i]->targetName;
        }
    }

    return sourceName;
}

