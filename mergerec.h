#ifndef MERGEREC_H
#define MERGEREC_H

#include "basetypes.h"

struct mergerec
{
    PCHAR sourceName;
    PCHAR targetName;
};

typedef struct mergerec MERGEREC, *PMERGEREC,**PPMERGEREC;

extern PPMERGEREC mergeList;
extern UINT mergeCount;

PMERGEREC createMergeRec(PCHAR sourceName,PCHAR targetName);
PCHAR lookupTargetName(PCHAR sourceName);

#endif
