#include "alink.h"

static void reorderGroups(void)
{
    UINT i,j;
    PSEG grp;
    UINT contentCount;
    PCONTENT contentList;
    
    for(i=0;i<globalSegCount;++i)
    {
	if(!globalSegs[i]) continue;
	if(!globalSegs[i]->group) continue;
	grp=globalSegs[i];
	contentCount=grp->contentCount;
	contentList=grp->contentList;
	grp->contentCount=0;
	grp->contentList=NULL;
	grp->length=0;

	/* first add all group headers */
	for(j=0;j<contentCount;++j)
	{
	    if(contentList[j].seg->group)
		addSeg(grp,contentList[j].seg);
	}
	/* next add all segments with content */
	for(j=0;j<contentCount;++j)
	{
	    if(contentList[j].seg->group) continue;
	    if(getInitLength(contentList[j].seg))
		addSeg(grp,contentList[j].seg);
	}
	/* finally add all segments without content */
	for(j=0;j<contentCount;++j)
	{
	    if(contentList[j].seg->group) continue;
	    if(!getInitLength(contentList[j].seg))
		addSeg(grp,contentList[j].seg);
	}
    }
}


static void combineGroups(void)
{
    UINT i,j,k,l;
    PSEG mgrp,s;

    diagnostic(DIAG_VERBOSE,"Combining Groups");

    for(i=0;i<globalSegCount;++i)
    {
	if(!globalSegs[i]) continue; /* skip if entry in global seg list is empty */
	if(!globalSegs[i]->group) continue; /* skip if not a group */

	mgrp=NULL; /* no master group yet */
	
	for(j=i+1;j<globalSegCount;++j)
	{
	    /* search for another group */
	    if(!globalSegs[j]) continue;
	    if(!globalSegs[j]->group) continue;
	    /* OK we've found another group. Check name */
	    if(strcmp(globalSegs[i]->name,globalSegs[j]->name)) continue; /* skip if no match */
	    /* same name. Create master group if not already one */
	    if(!mgrp)
	    {
		mgrp=createDuplicateSection(globalSegs[i]);
		mgrp->mod=NULL;
		mgrp->group=TRUE;
		mgrp->parent=globalSegs[i]->parent;
		mgrp->base=globalSegs[i]->base;
		addSeg(mgrp,globalSegs[i]); /* add original to master */
		globalSegs[i]=mgrp; /* replace original group with master in list */
	    }
	    addSeg(mgrp,globalSegs[j]); /* add second, third, etc. group to master */
	    globalSegs[j]=NULL; /* and remove from list */
	}

	if(mgrp)
	{
	    /* OK, so we got a master group. Now move segments out of sub-groups, and into master */
	    mgrp->length=0; /* no length for master yet */
	    l=mgrp->contentCount; /* get number of sub-groups */
	    	    
	    for(j=0;j<l;++j)
	    {
		s=mgrp->contentList[j].seg;
		s->base=0; /* all sub-groups go at beginning */
		s->length=0; /* and have no length */
		
		/* add all entries from subgroup to main group */
		for(k=0;k<s->contentCount;++k)
		{
		    addSeg(mgrp,s->contentList[k].seg);
		}
		/* remove subgroup list of entries */
		checkFree(s->contentList);
		s->contentList=NULL;
		s->contentCount=0;
	    }
	}
    }
    
}

BOOL combineSegments(void)
{
    UINT i,j,k,l;
    PSEG sa,sb,mseg,ga,gb;

    /* remove segments marked for discard */
    for(i=0;i<globalSegCount;++i)
    {
	if(!globalSegs[i]) continue;
	if(globalSegs[i]->discard)
	{
	    globalSegs[i]=NULL;
	}
    }

    /* ensure groups are combined first, as it makes things easier */
    combineGroups();

    /* combine grouped segs with non-grouped segs */
    /* and warn if match other grouped segs */
    for(i=0;i<globalSegCount;++i)
    {
	if(!globalSegs[i]) continue; /* skip empty entries */
	if(!globalSegs[i]->group) continue; /* skip all but groups */
	ga=globalSegs[i];
	for(k=0;k<ga->contentCount;++k)
	{
	    sa=ga->contentList[k].seg; /* get included seg */
	    if(sa->group) continue; /* skip included groups */
	    mseg=NULL;

	    for(l=k+1;l<ga->contentCount;++l)
	    {
		sb=ga->contentList[l].seg;
		if(sb->group) continue; /* skip included groups */
		if(sb->combine != sa->combine) continue; /* skip if don't combine the same way */
		/* if "STACK" segments then combine, otherwise check names */
		if(sa->combine!=SEGF_STACK)
		{
		    if((sa->name && sb->name && strcmp(sa->name,sb->name)) 
		       || (sa->name && !sb->name) || (!sa->name && sb->name)
		       ) continue; /* skip if names don't match */
		    if((sa->class && sb->class && strcmp(sa->class,sb->class)) 
		       || (sa->class && !sb->class) || (!sa->class && sb->class)
		       ) continue; /* skip if classes don't match */
		    if((sa->combine==SEGF_PRIVATE) && ((sa->mod!=sb->mod) || !sa->mod))
			continue; /* skip if private segs from different modules, or global */
		}
		
		/* we now have a match within the same group */
		/* same name+class. Create master seg if not already one */
		if(!mseg)
		{
		    mseg=createDuplicateSection(sa);
		    mseg->mod=NULL;
		    mseg->parent=sa->parent;
		    mseg->base=sa->base;
		    ga->contentList[k].seg=mseg; /* replace original group with master in list */
		    if(sa->combine==SEGF_COMMON)
		    {
			addCommonSeg(mseg,sa);
		    }
		    else
		    {
			addSeg(mseg,sa); /* add original to master */
		    }
		}
		removeContent(ga,l); /* remove from list */
		if(sb->combine==SEGF_COMMON)
		{
		    addCommonSeg(mseg,sb);
		}
		else
		{
		    addSeg(mseg,sb); /* add second, third, etc. group to master */
		}
		--l; /* next check has same index */
	    }
	    
	    
	    for(j=0;j<globalSegCount;++j)
	    {
		if(j==i) continue; /* don't process the same entry against itself */
		if(!globalSegs[j]) continue;
		if(globalSegs[j]->group)
		{
		    if(j<i) continue; /* don't repeat a comparison already made */
		    
		    /* seg is a group, so check contents */
		    gb=globalSegs[j];
		    for(l=0;l<gb->contentCount;++l)
		    {
			sb=gb->contentList[l].seg; /* get included seg */
			if(sb->group) continue; /* skip included groups */
			if(sb->combine != sa->combine) continue; /* skip if don't combine the same way */
			if(sa->combine==SEGF_STACK)
			{
			    addError("Explicit stack part of group %s and %s\n",
				   ga->name,gb->name);
			    return FALSE;
			}
			
			if((sa->name && sb->name && strcmp(sa->name,sb->name)) 
			   || (sa->name && !sb->name) || (!sa->name && sb->name)
			   ) continue; /* skip if names don't match */
			if((sa->class && sb->class && strcmp(sa->class,sb->class)) 
			   || (sa->class && !sb->class) || (!sa->class && sb->class)
			   ) continue; /* skip if classes don't match */
			if((sa->combine==SEGF_PRIVATE) && ((sa->mod!=sb->mod) || !sa->mod))
			    continue; /* skip if private segs from different modules, or global */
			diagnostic(DIAG_BASIC,"Warning, segment %s is a member of groups %s and %s\n",
				   sa->name?sa->name:"",
				   ga->name?ga->name:"",
				   gb->name?gb->name:"");
		    }
		}
		else
		{
		    sb=globalSegs[j]; /* get next seg */
		    if(sb->combine != sa->combine) continue; /* skip if don't combine the same way */
		    /* if "STACK" segments then combine, otherwise check names */
		    if(sa->combine!=SEGF_STACK)
		    {
			if((sa->name && sb->name && strcmp(sa->name,sb->name)) 
			   || (sa->name && !sb->name) || (!sa->name && sb->name)
			   ) continue; /* skip if names don't match */
			if((sa->class && sb->class && strcmp(sa->class,sb->class)) 
			   || (sa->class && !sb->class) || (!sa->class && sb->class)
			   ) continue; /* skip if classes don't match */
			if((sa->combine==SEGF_PRIVATE) && ((sa->mod!=sb->mod) || !sa->mod))
			    continue; /* skip if private segs from different modules, or global */
		    }
		    
		    /* same name+class. Create master seg if not already one */
		    if(!mseg)
		    {
			mseg=createDuplicateSection(sa);
			mseg->mod=NULL;
			mseg->parent=sa->parent;
			mseg->base=sa->base;
			ga->contentList[k].seg=mseg; /* replace original group with master in list */
			if(sa->combine==SEGF_COMMON)
			{
			    addCommonSeg(mseg,sa);
			}
			else
			{
			    addSeg(mseg,sa); /* add original to master */
			}
		    }
		    if(sb->combine==SEGF_COMMON)
		    {
			addCommonSeg(mseg,sb);
		    }
		    else
		    {
			addSeg(mseg,sb); /* add second, third, etc. group to master */
		    }
		    globalSegs[j]=NULL; /* and remove from list */
		}
	    }
	    
	}
	
    }
    /* now combine non-grouped segments */
    for(i=0;i<globalSegCount;++i)
    {
	if(!globalSegs[i]) continue; /* skip empty entries */
	if(globalSegs[i]->group) continue; /* skip groups */
	sa=globalSegs[i];
	mseg=NULL;
	
	for(j=i+1;j<globalSegCount;++j)
	{
	    if(!globalSegs[j]) continue;
	    if(globalSegs[j]->group) continue;
	    sb=globalSegs[j]; /* get next seg */
	    if(sb->combine != sa->combine) continue; /* skip if don't combine the same way */
	    /* if "STACK" segments then combine, otherwise check names */
	    if(sa->combine!=SEGF_STACK)
	    {
		if((sa->name && sb->name && strcmp(sa->name,sb->name)) 
		   || (sa->name && !sb->name) || (!sa->name && sb->name)
		   ) continue; /* skip if names don't match */
		if((sa->class && sb->class && strcmp(sa->class,sb->class)) 
		   || (sa->class && !sb->class) || (!sa->class && sb->class)
		   ) continue; /* skip if classes don't match */
		if((sa->combine==SEGF_PRIVATE) && ((sa->mod!=sb->mod) || !sa->mod))
		    continue; /* skip if private segs from different modules, or global */
	    }
	    
	    /* same name+class. Create master seg if not already one */
	    if(!mseg)
	    {
		mseg=createDuplicateSection(sa);
		mseg->mod=NULL;
		mseg->parent=sa->parent;
		mseg->base=sa->base;
		globalSegs[i]=mseg; /* replace original group with master in list */
		if(sa->combine==SEGF_COMMON)
		{
		    addCommonSeg(mseg,sa);
		}
		else
		{
		    addSeg(mseg,sa); /* add original to master */
		}
	    }
	    if(sb->combine==SEGF_COMMON)
	    {
		addCommonSeg(mseg,sb);
	    }
	    else
	    {
		addSeg(mseg,sb); /* add second, third, etc. group to master */
	    }
	    globalSegs[j]=NULL; /* and remove from list */
	}
	
    }

    reorderGroups();
    return TRUE;
}
