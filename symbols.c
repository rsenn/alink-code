#include "alink.h"

static UINT checksumSection(PSEG s)
{
    UINT i=0,j,k;

    for(j=0;j<s->contentCount;++j)
    {
	if(s->contentList[j].flag!=DATA)
	{
	    addError("Checksum of non-data segment %s",s->name);
	    return 0;
	}
	i+=s->contentList[j].data->offset;
	for(k=0;k<s->contentList[j].data->length;++k)
	{
	    i+=s->contentList[j].data->data[k];
	}
    }

    return i;
}

static BOOL loadLibraryModules(void)
{
    UINT i,j;
    PFILE f;

    static UINT loadedLibCount=0;
    static PLIBMOD loadedLib=NULL;

    for(i=0;i<globalSymbolCount;++i)
    {
	/* only process referenced library symbols */
	if(!globalSymbols[i]->refCount) continue;
	if(globalSymbols[i]->type!=PUB_LIBSYM) continue;
	for(j=0;j<loadedLibCount;++j)
	{
	    if((loadedLib[j].mod==globalSymbols[i]->mod)
	       && (loadedLib[j].filepos==globalSymbols[i]->filepos))
	    {
		addError("Bad library %s: Symbol %s in dictionary, but not in specified module\n",
		       globalSymbols[i]->mod->file,globalSymbols[i]->name);
		exit(1);
	    }
	}
	loadedLib=checkRealloc(loadedLib,sizeof(LIBMOD)*(loadedLibCount+1));
	loadedLib[loadedLibCount].mod=globalSymbols[i]->mod;
	loadedLib[loadedLibCount].filepos=globalSymbols[i]->filepos;
	loadedLibCount++;

	f=fopen(globalSymbols[i]->mod->file,"rb");

	/* now load the module, as specified */
	fseek(f,globalSymbols[i]->filepos,SEEK_SET);
	if(!globalSymbols[i]->modload(f,globalSymbols[i]->mod))
	{
	    addError("Error loading library module from file %s",globalSymbols[i]->mod->file);
	    return FALSE;
	}

	fclose(f);
    }
    return TRUE;
}

void resolveExterns(void)
{
    UINT i;
    UINT oldmodCount=0;

    while(oldmodCount!=moduleCount)
    {
	oldmodCount=moduleCount;
	for(i=0;i<globalExternCount;++i)
	{
	    if(globalExterns[i]->pubdef) continue;
	    if((globalExterns[i]->pubdef=findSymbol(globalExterns[i]->name)))
		globalExterns[i]->pubdef->refCount++;
	}
	loadLibraryModules();
    }
    for(i=0;i<globalExternCount;++i)
    {
        if(globalExterns[i]->pubdef) continue;
        addError("Unresolved extern %s",globalExterns[i]->name);
    }
}

PSYMBOL createSymbol(PCHAR name,long type,PMODULE mod,...)
{
    PSYMBOL pubdef;
    va_list ap;

    if(!name) return NULL;

    /* names are uppercase if no case sensitivity */
    if(!case_sensitive) strupr(name);

    pubdef=(PSYMBOL)checkMalloc(sizeof(SYMBOL));
    va_start(ap,mod);
    /* modnum and symbol type are compulsory */
    pubdef->name=name;
    pubdef->mod=mod;
    pubdef->type=type;
    pubdef->refCount=0;

    /* clear out other stuff */
    pubdef->aliasName=NULL;
    pubdef->dllname=NULL;
    pubdef->impname=NULL;
    pubdef->ordinal=0;
    pubdef->grpnum=-1;
    pubdef->seg=NULL;
    pubdef->ofs=0;
    pubdef->typenum=-1;
    pubdef->comdatCount=0;
    pubdef->comdatList=NULL;

    switch(type)
    {
    case PUB_PUBLIC:
	/* extra params are segnum, offset, group, symbol type */
	pubdef->seg=va_arg(ap,PSEG);
	pubdef->ofs=va_arg(ap,UINT);
	pubdef->grpnum=va_arg(ap,INT);
	pubdef->typenum=va_arg(ap,INT);
	break;
    case PUB_ALIAS:
	/* extra param is alias name */
	pubdef->aliasName=va_arg(ap,PCHAR);
	if(!pubdef->aliasName)
	{
	    addError("Cannot define an alias with no name\n");
	    return NULL;
	}
	/* names are uppercase if not case sensitive */
	if(!case_sensitive) strupr(pubdef->aliasName);
	break;
    case PUB_IMPORT:
	/* extra params are DLL name, Import name and Import Ordinal */
	pubdef->dllname=va_arg(ap,PCHAR);
	pubdef->impname=va_arg(ap,PCHAR);
	pubdef->ordinal=va_arg(ap,int);
	/* ordinal is used as hint for named imports */
	if(!pubdef->dllname || !pubdef->dllname[0])
	{
	    addError("Cannot import a routine without a DLL name\n");
	    return NULL;
	}
	strupr(pubdef->dllname); /* uppercase DLL names */
	break;
    case PUB_COMDEF:
	/* extra params are length, isfar */
	pubdef->length=va_arg(ap,UINT);
	pubdef->isfar=va_arg(ap,BOOL);
	break;
    case PUB_COMDAT:
	pubdef->comdatCount=1;
	pubdef->comdatList=checkMalloc(sizeof(PCOMDATREC));
	pubdef->comdatList[0]=va_arg(ap,PCOMDATREC);
	break;
    case PUB_EXPORT:
	pubdef->export=va_arg(ap,PEXPORTREC);
	break;
    case PUB_LIBSYM:
	pubdef->filepos=va_arg(ap,UINT);
	pubdef->modload=va_arg(ap,PLOADFUNC);
	break;
    }
    va_end(ap);
    return pubdef;
}

PSYMBOL findSymbol(char *key)
{
    UINT i,count;
    INT j;
    PPSYMBOL list;

    if(!globalSymbolCount) return NULL;
    if(!key) return NULL;

    count=globalSymbolCount;
    list=globalSymbols;

    while(count)
    {
	i=count/2; /* get mid point */
	j=strcmp(key,list[i]->name); /* compare */
	if(!j) return list[i]; /* return match if found */
	if(j<0) /* if key is less than current id */
	{
	    count=i; /* only search those before current node */
	}
	else /* key is greater than current id */
	{
	    list+=i+1; /* start search at next node */
	    count-=(i+1); /* count of those remaining after current node */
	}
    }

    return NULL; /* return NULL if no match (count=0) */
}

static BOOL insertSymbol(PSYMBOL sym)
{
    UINT count,index,i;
    int j;

    if(!sym) return TRUE;

    count=globalSymbolCount;

    /* new node, insert into list */
    /* find index to insert at */
    index=0;
    while(count)
    {
	i=count/2;
	j=strcmp(sym->name,globalSymbols[index+i]->name); /* compare */
	if(!j) /* match !! */
	{
	    addError("Attempt to add a duplicate symbol %s",sym->name);
	    return FALSE;
	}

	if(j<0) /* if key is less than current id */
	{
	    count=i; /* only search those before current node */
	}
	else /* key is greater than current id */
	{
	    index+=i+1; /* start search at next node */
	    count-=(i+1); /* count of those remaining after current node */
	}

    }
    /* grow list */
    globalSymbolCount+=1;

    globalSymbols=(PPSYMBOL)checkRealloc(globalSymbols,sizeof(PSYMBOL)*globalSymbolCount);

    /* get number of entries after insertion index */
    j=globalSymbolCount-index-1;
    if(j) /* move them up 1 entry if some */
    {
	memmove(globalSymbols+index+1,globalSymbols+index,j*sizeof(PSYMBOL));
    }

    /* put new node in position */
    globalSymbols[index]=sym;
    return TRUE;
}

BOOL addGlobalSymbol(PSYMBOL p)
{
    PSYMBOL oldpub;
    UINT i,j;

    if(!p) return TRUE;

    if((oldpub=findSymbol(p->name)))
    {
	/* Symbol found with same scope */
	/* verify we can overwrite it */

	if((oldpub->type==PUB_PUBLIC) && (p->type==PUB_PUBLIC))
	{
	    /* if both public symbols, then ERROR */
	    addError("Duplicate public symbol %s",p->name);
	    return FALSE;
	}

	if((oldpub->type==PUB_IMPORT) && (p->type==PUB_IMPORT)
	   && (strcmp(oldpub->dllname,p->dllname)
	       || strcmp(oldpub->impname,p->impname)))
	{
	    /* if both import symbols, then ERROR if different */
	    addError("Duplicate internal name %s for distinct imports",p->name);
	    return FALSE;
	}

	if((oldpub->type==PUB_ALIAS) && (p->type==PUB_ALIAS)
	   && !strcmp(oldpub->aliasName,p->aliasName))
	{
	    /* if both alias symbols, then warning */
	    diagnostic(DIAG_BASIC,"Warning: two substitute symbols (%s and %s) for %s, using %s\n",oldpub->aliasName,p->aliasName,p->name,p->aliasName);
	}

	/* check which of the two wins the clash */
	if((p->type==PUB_PUBLIC)
	   /* if new symbol is PUBLIC, then it wins */
	   || (oldpub->type==PUB_EXPORT)
	   /* exports always lose */
	   || ((oldpub->type==PUB_LIBSYM) &&(p->type!=PUB_LIBSYM))
	   /* as do library symbols except to new library symbols */
	   || ((oldpub->type==PUB_ALIAS) && (p->type!=PUB_EXPORT))
	   /* aliases lose to all but exports */
	   || ((p->type==PUB_IMPORT) && (oldpub->type=PUB_COMDEF))
	   /* imports beat common uninitialised data */
	   || ((p->type==PUB_COMDAT) && (oldpub->type=PUB_IMPORT))
	   /* but initialised common data beats an import */
	   || ((p->type==PUB_COMDAT) && (oldpub->type=PUB_COMDEF))
	   /* and unitialised common data */
	   )
	{
	    /* new symbol overpowers old one */
	    switch(oldpub->type)
	    {
	    case PUB_ALIAS:
		checkFree(oldpub->aliasName);
		break;
	    case PUB_IMPORT:
		checkFree(oldpub->dllname);
		checkFree(oldpub->impname);
		break;
	    case PUB_COMDEF:
		/* nothing to free for COMDEFs */
		break;
	    case PUB_COMDAT:
		/* free comdats, and associated segments */
		for(i=0;i<oldpub->comdatCount;++i)
		{
		    for(j=0;j<oldpub->comdatList[i]->segCount;++j)
		    {
			freeSection(oldpub->comdatList[i]->segList[j]);
		    }
		    checkFree(oldpub->comdatList[i]);
		}
		checkFree(oldpub->comdatList);
		break;
	    case PUB_EXPORT:
		break;
	    case PUB_LIBSYM:
		/* nothing to clean up */
		break;
	    }

	    /* preserve reference counts */
	    p->refCount=oldpub->refCount;

	    /* we can replace current one, so do so */
	    /* copying over it maintains pointers */
	    (*oldpub)=(*p);
	    /* free memory used for new template, now data copied */
	    checkFree(p);
	}
	else
	{
	    /* new symbol doesn't win automatically */
	    /* maybe merge new with old somehow, and */
	    /* free new symbol record */
	    switch(p->type)
	    {
	    case PUB_ALIAS:
		checkFree(p->aliasName);
		break;
	    case PUB_IMPORT:
		checkFree(p->dllname);
		checkFree(p->impname);
		break;
	    case PUB_COMDEF:
		if(oldpub->type==PUB_COMDEF)
		{
		    if(oldpub->isfar != p->isfar)
		    {
			addError("Mismatched near/far type for COMDEF %s",p->name);
			return FALSE;
		    }
		    if(p->length>oldpub->length)
		    {
			oldpub->length=p->length;
		    }
		}
		break;
	    case PUB_COMDAT:
		if(oldpub->type==PUB_COMDAT)
		{
		    /* merge comdat records together */
		    oldpub->comdatList=checkRealloc(oldpub->comdatList,(oldpub->comdatCount+p->comdatCount)*sizeof(PCOMDATREC));
		    memcpy(oldpub->comdatList+oldpub->comdatCount,p->comdatList,p->comdatCount*sizeof(PCOMDATREC));
		    checkFree(p->comdatList);
		    oldpub->comdatCount+=p->comdatCount;
		}
		else
		{
		    /* just free this one */
		    for(i=0;i<p->comdatCount;++i)
		    {
			for(j=0;j<p->comdatList[i]->segCount;++j)
			{
			    freeSection(p->comdatList[i]->segList[j]);
			}
			checkFree(p->comdatList[i]);
		    }
		    checkFree(p->comdatList);
		}
		break;
	    case PUB_EXPORT:
		break;
	    case PUB_LIBSYM:
		/* nothing to clean up */
		break;
	    }

	    checkFree(p);
	}
	return TRUE;
    }

    /* symbol not in list, so add to it */
    return insertSymbol(p);
}

static BOOL emitCommonSymbol(PSYMBOL sym)
{
    static PSEG comdefSeg=NULL,farcomdefSeg=NULL;
    UINT j,k,n,m;
    PCOMDATREC c,c2,c3;
    UINT linkType;
    UINT maxLength;
    PSEG seg;

    if(!sym) return TRUE;

    if(sym->type==PUB_COMDEF)
    {
	if(sym->isfar)
	{
	    if(!farcomdefSeg || ((farcomdefSeg->length+sym->length)>0x10000))
	    {
		farcomdefSeg=createSection("FARCOMDEFS","BSS",NULL,NULL,0,1);
		farcomdefSeg->combine=SEGF_PRIVATE;
		farcomdefSeg->align=16;
		farcomdefSeg->read=TRUE;
		farcomdefSeg->write=TRUE;
	    }

	    seg=farcomdefSeg;
	}
	else
	{
	    if(!comdefSeg)
	    {
		comdefSeg=createSection("COMDEFS","BSS",NULL,NULL,0,1);
		comdefSeg->combine=SEGF_PUBLIC;
		comdefSeg->align=16;
		comdefSeg->read=TRUE;
		comdefSeg->write=TRUE;

		/* add to DGROUP, if possible */
		for(j=0;j<globalSegCount;++j)
		{
		    if(!globalSegs[j]) continue;
		    if(!globalSegs[j]->group) continue;
		    /* OK we've found a group */
		    if(strcmp(globalSegs[j]->name,"DGROUP")) continue; /* skip if not DGROUP */
		    addSeg(globalSegs[j],comdefSeg);
		    break;
		}
	    }
	    seg=comdefSeg;
	}
	sym->seg=seg;
	sym->ofs=seg->length;
	seg->length+=sym->length;
	if(!seg->parent)
	{
	    /* add seg to global list, if not already part of a group */
	    globalSegs=checkRealloc(globalSegs,(globalSegCount+1)*sizeof(PSEG));
	    globalSegs[globalSegCount]=seg;
	    globalSegCount++;
	}
    }
    else if(sym->type==PUB_COMDAT)
    {
	linkType=COMDAT_ANY;
	c=NULL;

	for(j=0;j<sym->comdatCount;++j)
	{
	    switch(sym->comdatList[j]->combine)
	    {
	    case COMDAT_ANY:
		break;
	    case COMDAT_UNIQUE:
		c=sym->comdatList[j];
		if(sym->comdatCount>1)
		{
		    addError("Attempt to link multiple copies of unique COMDAT %s\n",sym->name);
		    return FALSE;
		}
		break;
	    case COMDAT_LARGEST:
		c2=NULL;
		maxLength=0;
		for(k=0;k<sym->comdatCount;++k)
		{
		    c3=sym->comdatList[k];
		    n=0;
		    for(m=0;m<c3->segCount;++m)
		    {
			n+=c3->segList[m]->length;
		    }
		    if((n>maxLength) || !c2)
		    {
			c2=c3;
			maxLength=n;
		    }
		}
		if(!c)
		{
		    c=c2; /* if no previous best choice, make one */
		    linkType=COMDAT_LARGEST;
		}
		else
		{
		    if(c!=c2)
		    {
			addError("Different link types cause conflicting choice for instance of COMDAT %s\n",
			       sym->name);
			return FALSE;
		    }
		}
		break;
	    case COMDAT_SAMESIZE:
		c2=NULL;
		maxLength=0;
		for(k=0;k<sym->comdatCount;++k)
		{
		    c3=sym->comdatList[k];
		    n=0;
		    for(m=0;m<c3->segCount;++m)
		    {
			n+=c3->segList[m]->length;
		    }
		    if((n!=maxLength) && c2)
		    {
			addError("Different sized instances for same-size linking of COMDAT %s\n",sym->name);
			return FALSE;
		    }
		    else if(!c2)
		    {
			c2=c3;
			maxLength=n;
		    }
		}
		if(!c)
		{
		    c=c2; /* if no previous best choice, make one */
		    linkType=COMDAT_SAMESIZE;
		}
		else
		{
		    if(c!=c2)
		    {
			addError("Different link types cause conflicting choice for instance of COMDAT %s\n",
			       sym->name);
			return FALSE;
		    }
		}
		break;
	    case COMDAT_EXACT:
		c2=NULL;
		maxLength=0;
		for(k=0;k<sym->comdatCount;++k)
		{
		    c3=sym->comdatList[k];
		    n=0;
		    for(m=0;m<c3->segCount;++m)
		    {
			n+=checksumSection(c3->segList[m]);
		    }
		    if((n!=maxLength) && c2)
		    {
			addError("Different instances for exact-match linking of COMDAT %s",sym->name);
			return FALSE;
		    }
		    else if(!c2)
		    {
			c2=c3;
			maxLength=n;
		    }
		}
		if(!c)
		{
		    c=c2; /* if no previous best choice, make one */
		    linkType=COMDAT_SAMESIZE;
		}
		else
		{
		    if(c!=c2)
		    {
			addError("Different link types cause conflicting choice for instance of COMDAT %s",
			       sym->name);
			return FALSE;
		    }
		}
		break;
	    default:
		addError("Unknown COMDAT combine type for %s",sym->name);
		return FALSE;
	    }
	}
	if(linkType==COMDAT_ANY && sym->comdatCount)
	{
	    c=sym->comdatList[0];
	}

	if(!c)
	{
	    addError("No possible choices for COMDAT %s",sym->name);
	    return FALSE;
	}

	globalSegs=checkRealloc(globalSegs,(globalSegCount+c->segCount)*sizeof(PSEG));
	for(k=0;k<c->segCount;++k)
	{
	    globalSegs[globalSegCount]=c->segList[k];
	    globalSegCount++;
	}

	/* define location of symbols */
	sym->seg=c->segList[0];
	sym->ofs=0;
    }
    return TRUE;
}


void emitCommonSymbols(void)
{
    UINT i;

    for(i=0;i<globalSymbolCount;++i)
    {
	emitCommonSymbol(globalSymbols[i]);
    }
    for(i=0;i<localSymbolCount;++i)
    {
	emitCommonSymbol(localSymbols[i]);
    }
}
