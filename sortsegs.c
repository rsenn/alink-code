#include "alink.h"

static sortDOSSEG()
{
    long i,j,k;
    long dgroupNum=-1;
    
    /* rule 1: all segs of class code, not in any groups */

    for(i=0;i<segcount;i++)
    {
	if(!seglist[i]) continue;
	if(seglist[i]->absframe) continue; /* skip already output segments */
	if((seglist[i]->attr&SEG_ALIGN)==SEG_ABS) continue; /* skip absolute segments */
	if(seglist[i]->winFlags&WINF_REMOVE) continue; /* skip segments marked for removal */
	
	/* search groups */
	for(j=0;j<grpcount;j++)
	{
	    if(!grplist[j]) continue;
	    if(i==grplist[j]->segnum) break; /* skip if this is a group segment */
	    
	    for(k=0;k<grplist[j]->numsegs;k++)
	    {
		if(grplist[j]->segindex[k]==i)
		    break; /* skip if seg is in group */
	    }
	    if(k!=grplist[j]->numsegs) break; /* if we found seg, don't search any more groups */
	}
	if(j!=grpcount) continue; /* if we found a group with seg in, skip seg */
	if(seglist[j]->classindex<0) continue; /* skip if no class name */
	if(strcmp(namelist[seglist[j]->classindex],"CODE")) continue; /* skip if class name not CODE */
	seglist[i]->absframe=1;
	seglist[i]->absofs=0;
	outlist[outcount]=seglist[i];
	outcount++;
    }
    
    /* rule 2: all segs not in DGROUP, not of class code */
    /* 2a: GROUPed segments */
    for(i=0;i<grpcount;i++)
    {
	if(!grplist[i]) continue;
	if(grplist[i]->nameindex>=0)
	{
	    /* skip DGROUP */
	    if(!strcmp(namelist[grplist[i]->nameindex],"DGROUP"))
	    {
		dgroupNum=i;
		continue;
	    }
	}
	/* add group-base seg to out list */
	outlist[outcount]=seglist[grplist[i]->segnum];
	outcount++;
	seglist[grplist[i]->segnum]->absframe=1;
	for(j=0;j<grplist[i]->numsegs;j++)
	{
	    k=grplist[i]->segindex[j];
	    if(!seglist[k])
	    {
		printf("Error - group %s contains non-existent segment %i\n",namelist[grplist[i]->nameindex],k);
		exit(1);
	    }
	    if((seglist[k]->attr&SEG_ALIGN)==SEG_ABS) continue; /* skip absolute segments */
	    if(seglist[k]->winFlags&WINF_REMOVE) continue; /* skip segments marked for removal */
	    if(!seglist[k]->length) continue; /* skip zero-length segments */
	    if(seglist[k]->absframe)
	    {
		printf("Error - Segment %s part of more than one group\n",namelist[seglist[k]->nameindex]);
		exit(1);
	    }
	    seglist[k]->absframe=1;
	    seglist[k]->absofs=i+1;
	    outlist[outcount]=seglist[k];
	    outcount++;
	}
    }
    /* 2b: non-GROUPed segments */
    for(i=0;i<segcount;i++)
    {
	if(!seglist[i]) continue;
	if(seglist[i]->absframe) continue; /* skip already output segments */
	if((seglist[i]->attr&SEG_ALIGN)==SEG_ABS) continue; /* skip absolute segments */
	if(seglist[i]->winFlags&WINF_REMOVE) continue; /* skip segments marked for removal */

	/* if we found DGROUP, check if seg in it */
	if(dgroupNum>=0)
	{
	    if(i==grplist[dgroupNum]->segnum) continue;
	    for(j=0;j<grplist[dgroupNum]->numsegs;j++)
	    {
		/* skip if we find seg */
		if(grplist[dgroupNum]->segindex[j]==i) break;
	    }
	    /* skip seg if in DGROUP */
	    if(j!=grplist[dgroupNum]->numsegs) continue;
	}
	seglist[i]->absframe=1;
	seglist[i]->absofs=0;
	outlist[outcount]=seglist[i];
	outcount++;
    }
    
    /* rule 3: Segments in DGROUP */
    if(dgroupNum<0) return; /* all is done if DGROUP not found */

    i=grplist[dgroupNum]->segnum; /* get base seg for DGROUP */
    
    /* this is the next one to output */
    seglist[i]->absframe=1;
    seglist[i]->absofs=0;
    outlist[outcount]=seglist[i];
    outcount++;

    /* 3a: Segments of class BEGDATA */
    for(i=0;i<segcount;i++)
    {
	if(!seglist[i]) continue;
	if(seglist[i]->absframe) continue; /* skip already output segments */
	if((seglist[i]->attr&SEG_ALIGN)==SEG_ABS) continue; /* skip absolute segments */
	if(seglist[i]->winFlags&WINF_REMOVE) continue; /* skip segments marked for removal */

	/* Assume that any seg that passes these checks IS in DGROUP, otherwise it would */
	/* have been picked up above */
	
	if(seglist[i]->classindex<0) continue; /* skip if no class name */
	if(strcmp(namelist[seglist[i]->classindex],"BEGDATA")) continue; /* skip if class name not BEGDATA */
	seglist[i]->absframe=1;
	seglist[i]->absofs=0;
	outlist[outcount]=seglist[i];
	outcount++;
    }
    /* 3b: Segments not of class BEGDATA,BSS or STACK */
    for(i=0;i<segcount;i++)
    {
	if(!seglist[i]) continue;
	if(seglist[i]->absframe) continue; /* skip already output segments */
	if((seglist[i]->attr&SEG_ALIGN)==SEG_ABS) continue; /* skip absolute segments */
	if(seglist[i]->winFlags&WINF_REMOVE) continue; /* skip segments marked for removal */

	/* Assume that any seg that passes these checks IS in DGROUP, otherwise it would */
	/* have been picked up above */
	if(seglist[i]->classindex>=0)
	{
	    /* Assume that BEGDATA segments are caught by previous chunk */
	    if(!strcmp(namelist[seglist[i]->classindex],"BSS")) continue; /* skip if class name BSS */
	    if(!strcmp(namelist[seglist[i]->classindex],"STACK")) continue; /* skip if class name STACK */
	}
	seglist[i]->absframe=1;
	seglist[i]->absofs=0;
	outlist[outcount]=seglist[i];
	outcount++;
    }
    /* 3c: Segments of class BSS */
    for(i=0;i<segcount;i++)
    {
	if(!seglist[i]) continue;
	if(seglist[i]->absframe) continue; /* skip already output segments */
	if((seglist[i]->attr&SEG_ALIGN)==SEG_ABS) continue; /* skip absolute segments */
	if(seglist[i]->winFlags&WINF_REMOVE) continue; /* skip segments marked for removal */

	/* Assume that any seg that passes these checks IS in DGROUP, otherwise it would */
	/* have been picked up above */
	if(seglist[i]->classindex<0) continue; /* skip if no class name */
	if(strcmp(namelist[seglist[i]->classindex],"BSS")) continue; /* skip if class name not BSS */
	seglist[i]->absframe=1;
	seglist[i]->absofs=0;
	outlist[outcount]=seglist[i];
	outcount++;
    }
    /* 3d: Segments of class STACK */
    for(i=0;i<segcount;i++)
    {
	if(!seglist[i]) continue;
	if(seglist[i]->absframe) continue; /* skip already output segments */
	if((seglist[i]->attr&SEG_ALIGN)==SEG_ABS) continue; /* skip absolute segments */
	if(seglist[i]->winFlags&WINF_REMOVE) continue; /* skip segments marked for removal */

	/* Assume that any seg that passes these checks IS in DGROUP, otherwise it would */
	/* have been picked up above */
	/* since this is the final rule, assume that all remaining segs are of class STACK */
	
	seglist[i]->absframe=1;
	seglist[i]->absofs=0;
	outlist[outcount]=seglist[i];
	outcount++;
    }
}


void sortSegments()
{
    long i,j,k;

    for(i=0;i<segcount;i++)
    {
	if(seglist[i])
	{
	    if((seglist[i]->attr&SEG_ALIGN)!=SEG_ABS)
	    {
		seglist[i]->absframe=0;
	    }
	    else
	    {
		seglist[i]->base=(seglist[i]->absframe<<4)+seglist[i]->absofs;
		seglist[i]->section=seglist[i]->absframe;
	    }
	}
    }

    outcount=0;
    outlist=checkMalloc(sizeof(PSEG)*segcount);

    if(dosSegOrdering)
    {
	sortDOSSEG();
	return;
    }
    

    for(i=0;i<grpcount;i++)
    {
	if(!grplist[i]) continue;
	outlist[outcount]=seglist[grplist[i]->segnum];
	outcount++;
	seglist[grplist[i]->segnum]->absframe=1;
	for(j=0;j<grplist[i]->numsegs;j++)
	{
	    k=grplist[i]->segindex[j];
	    if(!seglist[k])
	    {
		printf("Error - group %s contains non-existent segment %i\n",namelist[grplist[i]->nameindex],k);
		exit(1);
	    }
	    /* don't add removed sections */
	    if(seglist[k]->winFlags & WINF_REMOVE)
	    {
		continue;
	    }
	    /* add non-absolute segment */
	    if((seglist[k]->attr&SEG_ALIGN)!=SEG_ABS)
	    {
		if(seglist[k]->absframe!=0)
		{
		    printf("Error - Segment %s part of more than one group\n",namelist[seglist[k]->nameindex]);
		    exit(1);
		}
		seglist[k]->absframe=1;
		seglist[k]->absofs=i+1;
		outlist[outcount]=seglist[k];
		outcount++;
	    }
	}
    }
    for(i=0;i<segcount;i++)
    {
	if(seglist[i])
	{
	    /* don't add removed sections */
	    if(seglist[i]->winFlags & WINF_REMOVE)
	    {
		continue;
	    }
	    /* add non-absolute segment, not already dealt with */
	    if(((seglist[i]->attr&SEG_ALIGN)!=SEG_ABS) &&
	       !seglist[i]->absframe)
	    {
		seglist[i]->absframe=1;
		seglist[i]->absofs=0;
		outlist[outcount]=seglist[i];
		outcount++;
	    }
	}
    }
}

