void loadlib(FILE *libfile,PCHAR libname)
{
    unsigned int i,j,k,n;
    PCHAR name;
    unsigned short modpage;
    PLIBFILE p;
    UINT numsyms;
    PSORTENTRY symlist;

    libfiles=checkRealloc(libfiles,(libcount+1)*sizeof(LIBFILE));
    p=&libfiles[libcount];
    
    p->filename=checkMalloc(strlen(libname)+1);
    strcpy(p->filename,libname);

    if(fread(buf,1,3,libfile)!=3)
    {
	printf("Error reading from file\n");
	exit(1);
    }
    p->blocksize=buf[1]+256*buf[2];
    if(fread(buf,1,p->blocksize,libfile)!=p->blocksize)
    {
	printf("Error reading from file\n");
	exit(1);
    }
    p->blocksize+=3;
    p->dicstart=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
    p->numdicpages=buf[4]+256*buf[5];
    p->flags=buf[6];
    p->libtype='O';
	
    fseek(libfile,p->dicstart,SEEK_SET);

    symlist=(PSORTENTRY)checkMalloc(p->numdicpages*37*sizeof(SORTENTRY));
    
    numsyms=0;
    for(i=0;i<p->numdicpages;i++)
    {
	if(fread(buf,1,512,libfile)!=512)
	{
	    printf("Error reading from file\n");
	    exit(1);
	}
	for(j=0;j<37;j++)
	{
	    k=buf[j]*2;
	    if(k)
	    {
		name=checkMalloc(buf[k]+1);
		for(n=0;n<buf[k];n++)
		{
		    name[n]=buf[n+k+1];
		}
		name[buf[k]]=0;
		k+=buf[k]+1;
		modpage=buf[k]+256*buf[k+1];
		if(!(p->flags&LIBF_CASESENSITIVE) || !case_sensitive)
		{
		    strupr(name);
		}
		if(name[strlen(name)-1]=='!')
		{
		    free(name);
		}
		else
		{
		    symlist[numsyms].id=name;
		    symlist[numsyms].count=modpage;
		    ++numsyms;
		}
	    }
	}
    }

    qsort(symlist,numsyms,sizeof(SORTENTRY),sortCompare);
    p->symbols=symlist;
    p->numsyms=numsyms;
    p->modsloaded=0;
    p->modlist=checkMalloc(sizeof(unsigned short)*numsyms);
    libcount++;
}

void loadlibmod(UINT libnum,UINT modpage)
{
    PLIBFILE p;
    FILE *libfile;
    UINT i;

    p=&libfiles[libnum];

    /* don't open a module we've loaded already */
    for(i=0;i<p->modsloaded;i++)
    {
	if(p->modlist[i]==modpage) return;
    }

    libfile=fopen(p->filename,"rb");
    if(!libfile)
    {
	printf("Error opening file %s\n",p->filename);
	exit(1);
    }
    fseek(libfile,modpage*p->blocksize,SEEK_SET);
    switch(p->libtype)
    {
    case 'O':
	loadmod(libfile);
	break;
    case 'C':
	loadcofflibmod(p,libfile);
	break;
    default:
	printf("Unknown library file format\n");
	exit(1);
    }
	
    p->modlist[p->modsloaded]=modpage;
    p->modsloaded++;
    fclose(libfile);
}

void loadres(FILE *f)
{
    unsigned char buf[32];
    static unsigned char buf2[32]={0,0,0,0,0x20,0,0,0,0xff,0xff,0,0,0xff,0xff,0,0,
				   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    UINT i,j;
    UINT hdrsize,datsize;
    PUCHAR data;
    PUCHAR hdr;

    if(fread(buf,1,32,f)!=32)
    {
	printf("Invalid resource file\n");
	exit(1);
    }
    if(memcmp(buf,buf2,32))
    {
	printf("Invalid resource file\n");
        exit(1);
    }
    printf("Loading Win32 Resource File\n");
    while(!feof(f))
    {
	i=ftell(f);
	if(i&3)
	{
	    fseek(f,4-(i&3),SEEK_CUR);
	}
        i=fread(buf,1,8,f);
        if(i==0 && feof(f)) return;
        if(i!=8)
	{
	    printf("Invalid resource file, no header\n");
            exit(1);
	}
	datsize=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
        hdrsize=buf[4]+(buf[5]<<8)+(buf[6]<<16)+(buf[7]<<24);
	if(hdrsize<16)
        {
	    printf("Invalid resource file, bad header\n");
            exit(1);
        }
        hdr=(PUCHAR)checkMalloc(hdrsize);
	if(fread(hdr,1,hdrsize-8,f)!=(hdrsize-8))
	{
	    printf("Invalid resource file, missing header\n");
	    exit(1);
	}
	/* if this is a NULL resource, then skip */
	if(!datsize && (hdrsize==32) && !memcmp(buf2+8,hdr,24))
	{
	    free(hdr);
	    continue;
	}
	if(datsize)
	{
	    data=(PUCHAR)checkMalloc(datsize);
	    if(fread(data,1,datsize,f)!=datsize)
	    {
		printf("Invalid resource file, no data\n");
		exit(1);
	    }
	}
	else data=NULL;
	resource=(PRESOURCE)checkRealloc(resource,(rescount+1)*sizeof(RESOURCE));
	resource[rescount].data=data;
	resource[rescount].length=datsize;
	i=0;
	hdrsize-=8;
	if((hdr[i]==0xff) && (hdr[i+1]==0xff))
	{
	    resource[rescount].typename=NULL;
	    resource[rescount].typeid=hdr[i+2]+256*hdr[i+3];
	    i+=4;
	}
	else
	{
	    for(j=i;(j<(hdrsize-1))&&(hdr[j]|hdr[j+1]);j+=2);
	    if(hdr[j]|hdr[j+1])
	    {
		printf("Invalid resource file, bad name\n");
		exit(1);
	    }
	    resource[rescount].typename=(PUCHAR)checkMalloc(j-i+2);
	    memcpy(resource[rescount].typename,hdr+i,j-i+2);
	    i=j+5;
	    i&=0xfffffffc;
	}
	if(i>hdrsize)
	{
	    printf("Invalid resource file, overflow\n");
	    exit(1);
	}
	if((hdr[i]==0xff) && (hdr[i+1]==0xff))
	{
	    resource[rescount].name=NULL;
	    resource[rescount].id=hdr[i+2]+256*hdr[i+3];
	    i+=4;
	}
	else
	{
	    for(j=i;(j<(hdrsize-1))&&(hdr[j]|hdr[j+1]);j+=2);
	    if(hdr[j]|hdr[j+1])
	    {
		printf("Invalid resource file,bad name (2)\n");
		exit(1);
	    }
	    resource[rescount].name=(PUCHAR)checkMalloc(j-i+2);
	    memcpy(resource[rescount].name,hdr+i,j-i+2);
	    i=j+5;
	    i&=0xfffffffc;
	}
	i+=6; /* point to Language ID */
	if(i>hdrsize)
	{
	    printf("Invalid resource file, overflow(2)\n");
	    exit(1);
	}
	resource[rescount].languageid=hdr[i]+256*hdr[i+1];
	rescount++;
	free(hdr);
    }
}
void writeOutputFile(PCHAR outname)
{
    FILE *outfile;
    UINT lastout;
    UINT i,j,k;
    
    outfile=fopen(outname,"wb");
    if(!outfile)
    {
        printf("Error writing to file %s\n",outname);
        exit(1);
    }

    /* loop through sections on output list */
    for(i=0;i<outcount;i++)
    {
        if(outlist[i])
        {
	    if(outlist[i]->filepos < ftell(outfile))
	    {
		printf("Error - Section overlap in output file\n");
		exit(1);
	    }
	    lastout=0;
            for(j=0;j<outlist[i]->length;j++)
            {
		/* check if we need to output a byte */
                if(GetNbit(outlist[i]->datmask,j))
                {
		    /* get number of pad bytes to output */
		    if(outlist[i]->filepos > ftell(outfile))
		    {
			k=outlist[i]->filepos-ftell(outfile);
			for(;k;k--)
			{
			    fputc(0,outfile);
			}
		    }
    
		    /* if so, then make sure we've output Zeroes to pad */
		    /* if necessary */
                    for(;lastout<j;lastout++)
                    {
                        fputc(0,outfile);
                    }
		    /* now we can output this byte */
                    fputc(outlist[i]->data[j],outfile);
                    lastout++;
                }
                else if(padsegments)
                {
		    /* get number of pad bytes to output */
		    if(outlist[i]->filepos > ftell(outfile))
		    {
			k=outlist[i]->filepos-ftell(outfile);
			for(;k;k--)
			{
			    fputc(0,outfile);
			}
		    }
    
		    /* if no data, output zero if "Pad segments" is set */
                    fputc(0,outfile);
                    lastout++;
                }
            }
        }
    }

    fclose(outfile);
}

void alignSegments(void)
{
    UINT i;
    UINT base,align;
    
    base=imageBase;

    /* loop through output sections */
    for(i=0;i<outcount;i++)
    {
	/* get section alignment */
	switch(outlist[i]->attr&SEG_ALIGN)
	{
	case SEG_WORD:
	    align=2;
	    break;
	case SEG_DWORD:
	    align=4;
	    break;
	case SEG_8BYTE:
	    align=0x8;
	    break;
	case SEG_PARA:
	    align=0x10;
	    break;
	case SEG_32BYTE:
	    align=0x20;
	    break;
	case SEG_64BYTE:
	    align=0x40;
	    break;
	case SEG_PAGE:
	    align=0x100;
	    break;
	case SEG_MEMPAGE:
	    align=0x1000;
	    break;
	case SEG_BYTE:
	default:
	    align=1;
	    break;
	}
	/* if object alignment is more stringent, use that */
	if(align<objectAlign)
	{
	    align=objectAlign;
	}
	if((outlist[i]->attr&SEG_ALIGN)!=SEG_ABS)
	{
	    /* align base if not specified */
	    base=(base+align-1)&(0xffffffff-(align-1));
	    /* set segment base */
	    outlist[i]->base=base;
	}
	else
	{
	    /* if section has specified base, use that and proceed from here */
	    base=outlist[i]->base;
	}

	/* move past section in memory */
	base+=outlist[i]->length;
    }
}

void matchExterns()
{
    long i,j,k,old_nummods;
    int n;
    PSORTENTRY listnode;
    PCHAR name,lname;
    PPUBLIC pubdef;

    do
    {
	old_nummods=nummods; /* save count of loaded modules */

	/* loop through symbols until all done, or we load a library */
	for(i=0;(i<extcount)&&(nummods==old_nummods);i++)
	{
	    /* skip if we've already matched a public symbol */
	    /* as they override all others */
	    if(externs[i].flags==EXT_MATCHEDPUBLIC) continue;
	    externs[i].flags=EXT_NOMATCH;
	    name=externs[i].name;
	    /* loop as long as we follow chain of aliases */
	    while(externs[i].flags != EXT_MATCHEDPUBLIC)
	    {
		if(listnode=binarySearch(publics,pubcount,name))
		{
		    for(k=0;k<listnode->count;k++)
		    {
			pubdef=(PPUBLIC)listnode->object[k];
			/* local syms can only match externs in same scope */
			/* and global syms can only match global externs */
			if(pubdef->modnum==externs[i].modnum)
			{
			    /* if we didn't find an alias, this is a real */
			    /* symbol, so get it */
			    if(pubdef->type!=PUB_ALIAS)
			    {
				externs[i].pubdef=pubdef;
				externs[i].flags=EXT_MATCHEDPUBLIC;
				pubdef->useCount++;
			    }
			    else
			    {
				/* else search for alias symbol */
				name=pubdef->aliasName;
			    }
			    break;
			}
		    }
		    /* loop if we found something */
		    if(k!=listnode->count) continue;
		}
		/* check libraries if no match */
		for(k=0;k<libcount;++k)
		{
		    lname=checkStrdup(name);
		    if(!(libfiles[k].flags&LIBF_CASESENSITIVE))
		    {
			strupr(lname);
		    }
		
		    if(listnode=binarySearch(libfiles[k].symbols,libfiles[k].numsyms,lname))
		    {
			loadlibmod(k,listnode->count);
			break;
		    }
		    free(lname);
		}
		/* stop searching for symbol now we've checked library */
		break;
	    }
	}
    } while (old_nummods!=nummods);
}

void buildComDefSections(void)
{
    long i,j,k;
    long comseg=-1,comfarseg=-1;
    PPUBLIC pubdef;
    
    for(i=0;i<pubcount;i++)
    {
	for(j=0;j<publics[i].count;j++)
	{
	    pubdef=(PPUBLIC)publics[i].object[j];
	    if(pubdef->type!=PUB_COMDEF) continue;
	    if(!pubdef->isfar)
	    {
		if(comseg<0)
		{
		    comseg=createSection("COMDEFS",-1);
		    seglist[comseg]->attr=SEG_PUBLIC | SEG_PARA;
		    seglist[comseg]->winFlags=WINF_READABLE | WINF_WRITEABLE | WINF_NEG_FLAGS;
		    for(k=0;k<grpcount;k++)
		    {
			if(!grplist[k]) continue;
			if(grplist[k]->nameindex<0) continue;
			if(!strcmp("DGROUP",namelist[grplist[k]->nameindex]))
			{
			    if(grplist[k]->numsegs==0) continue; /* don't add to an emtpy group */
			    /* because empty groups are special */
			    /* else add to group */
			    grplist[k]->segindex[grplist[k]->numsegs]=comseg;
			    grplist[k]->numsegs++;
			    break;
			}
		    }
		}
		pubdef->segnum=comseg;
		pubdef->ofs=seglist[comseg]->length;
		seglist[comseg]->length+=pubdef->length;
	    }
	    else
	    {
		if(pubdef->length>65536)
		{
		    k=createSection("FARCOMDEFS",-1);
		    seglist[k]->attr=SEG_PUBLIC | SEG_PARA;
		    seglist[k]->winFlags=WINF_READABLE | WINF_WRITEABLE | WINF_NEG_FLAGS;
		    seglist[k]->length=pubdef->length;
		    seglist[k]->datmask=
			(PUCHAR)checkMalloc((pubdef->length+7)/8);
		    memset(seglist[k]->datmask,0,(pubdef->length+7)/8);
		    pubdef->segnum=k;
		    pubdef->ofs=0;
		    continue;
		}
		if((comfarseg>=0) && ((pubdef->length+seglist[comfarseg]->length)>65536))
		{
		    seglist[comfarseg]->datmask=
			(PUCHAR)checkMalloc((seglist[comfarseg]->length+7)/8);
		    memset(seglist[comfarseg]->datmask,0,(seglist[comfarseg]->length+7)/8);
		    comfarseg=-1;
		}
		if(comfarseg<0)
		{
		    k=createSection("FARCOMDEFS",-1);
		    seglist[k]->attr=SEG_PUBLIC | SEG_PARA;
		    seglist[k]->winFlags=WINF_READABLE | WINF_WRITEABLE | WINF_NEG_FLAGS;
		}
	
		pubdef->segnum=comfarseg;
		pubdef->ofs=seglist[comfarseg]->length;
		seglist[comfarseg]->length+=pubdef->length;
	    }
	}
    }
    if(comseg>=0)
    {
	seglist[comseg]->datmask=
	    (PUCHAR)checkMalloc((seglist[comseg]->length+7)/8);
	memset(seglist[comseg]->datmask,0,(seglist[comseg]->length+7)/8);
    }
    if(comfarseg>=0)
    {
	seglist[comfarseg]->datmask=
	    (PUCHAR)checkMalloc((seglist[comfarseg]->length+7)/8);
	memset(seglist[comfarseg]->datmask,0,(seglist[comfarseg]->length+7)/8);
    }
}

main
{
    if(rescount && (output_type!=OUTPUT_PE))
    {
	printf("Cannot link resources into a non-PE application\n");
	exit(1);
    }

    if(entryPoint)
    {
	if(!case_sensitive)
	{
	    strupr(entryPoint);
	}
	
	if(gotstart)
	{
	    printf("Warning, overriding entry point from Command Line\n");
	}
	/* define an external reference for entry point */
	externs=checkRealloc(externs,(extcount+1)*sizeof(EXTREC));
	externs[extcount].name=entryPoint;
	externs[extcount].typenum=-1;
	externs[extcount].pubdef=NULL;
	externs[extcount].flags=EXT_NOMATCH;
	externs[extcount].modnum=0;

	/* point start address to this external */
	startaddr.ftype=REL_EXTDISP;
	startaddr.frame=extcount;
	startaddr.ttype=REL_EXTONLY;
	startaddr.target=extcount;

	extcount++;
	gotstart=TRUE;
    }

    matchExterns();
    printf("matched Externs\n");
    
    for(i=0;i<expcount;i++)
    {
	if(externs[expdefs[i].extindex].flags==EXT_NOMATCH)
	{
	    printf("Unresolved export %s=%s\n",expdefs[i].exp_name,expdefs[i].int_name);
	    errcount++;
	}
	else if(externs[expdefs[i].extindex].pubdef->type ==PUB_ALIAS)
	{
	    printf("Unresolved export %s=%s, with alias %s\n",expdefs[i].exp_name,expdefs[i].int_name,externs[expdefs[i].extindex].pubdef->aliasName);
	    errcount++;
	}
	
    }

    for(i=0;i<extcount;i++)
    {
	if(externs[i].flags==EXT_NOMATCH)
	{
	    printf("Unresolved external %s\n",externs[i].name);
	    errcount++;
	}
	else if(externs[i].flags==EXT_MATCHEDPUBLIC)
	{
	    if(externs[i].pubdef->aliasName)
	    {
		printf("Unresolved external %s with alias %s\n",externs[i].name,externs[i].pubdef->aliasName);
		errcount++;
	    }
	}
    }

    if(errcount!=0)
    {
	exit(1);
    }
    
    buildComDefSections();

    combineBlocks();

    switch(output_type)
    {
    case OUTPUT_BIN:
    case OUTPUT_COM:
	/* set up final relocations */
	doFlatRelocs();
	/* merge and rearrange segments */
	sortSegments();
	alignSegments();
	/* set section number = -1 (unknown) */
	for(i=0;i<segcount;i++)
	{
	    /* skip if no segment, or absolute */
	    if(!seglist[i]) continue;
	    if((seglist[i]->attr&SEG_ALIGN)==SEG_ABS)
	    {
		seglist[i]->section=seglist[i]->absframe;
	    }
	    else
	    {
		seglist[i]->section=-1;
	    }
	}
	/* set file positions =offset from image base */
	for(i=0;i<outcount;i++)
	{
	    if(outlist[i]->base < imageBase)
	    {
		printf("base address of section below image base\n");
		exit(1);
	    }
	    
	    outlist[i]->filepos=outlist[i]->base-imageBase;
	}
	
	break;
    case OUTPUT_EXE:
	buildEXEFile();
	break;
    case OUTPUT_PE:
	buildPEFile();
	break;
    case OUTPUT_NE:
	buildNEFile();
	break;
    default:
	printf("Invalid output type\n");
	exit(1);
	break;
    }

    if(mapfile) generateMap();

    for(i=0;i<simpleRelocCount;++i)
    {
	applyFixup(simpleRelocs+i);
    }

    writeOutputFile(outname);

}

loadfiles
{
	switch(j)
	{
	case LIBHDR:
	    loadlib(afile,filename[i]);
	    break;
	case THEADR:
	case LHEADR:
	    loadmod(afile);
	    break;
	case 0:
	    loadres(afile);
	    break;
	case 0x4c:
	case 0x4d:
	case 0x4e:
	    loadcoff(afile);
	    break;
	case 0x21:
	    loadCoffLib(afile,filename[i]);
	    break;
	default:
	    printf("Unknown file type\n");
	    fclose(afile);
	    exit(1);
	}
}

void fixrelocs(int src,int dest,UINT shift)
{
    UINT k;
    
    for(k=0;k<fixcount;k++)
    {
	if(relocs[k]->segnum==src)
	{
	    relocs[k]->segnum=dest;
	    relocs[k]->ofs+=shift;
	}
	if(relocs[k]->ttype==REL_SEGDISP)
	{
	    if(relocs[k]->target==src)
	    {
		relocs[k]->target=dest;
		relocs[k]->disp+=shift;
	    }
	}
	else if(relocs[k]->ttype==REL_SEGONLY)
	{
	    if(relocs[k]->target==src)
	    {
		relocs[k]->target=dest;
		relocs[k]->ttype=REL_SEGDISP;
		relocs[k]->disp=shift;
	    }
	}
	if((relocs[k]->ftype==REL_SEGFRAME) ||
	   (relocs[k]->ftype==REL_LILEFRAME))
	{
	    if(relocs[k]->frame==src)
	    {
		relocs[k]->frame=dest;
	    }
	}
    }
}


void fixpubsegs(int src,int dest,UINT shift)
{
    UINT i,j;
    PPUBLIC q;
    
    for(i=0;i<pubcount;++i)
    {
	for(j=0;j<publics[i].count;++j)
	{
	    q=(PPUBLIC)publics[i].object[j];
	    if(q->segnum==src)
	    {
		q->segnum=dest;
		q->ofs+=shift;
	    }
	}
    }
}

void fixpubgrps(int src,int dest)
{
    UINT i,j;
    PPUBLIC q;
    
    for(i=0;i<pubcount;++i)
    {
	for(j=0;j<publics[i].count;++j)
	{
	    q=(PPUBLIC)publics[i].object[j];
	    if(q->grpnum==src)
	    {
		q->grpnum=dest;
	    }
	}
    }
}

void combine_segments(long dest,long src)
{
    UINT k,n;
    PUCHAR p,q;
    UINT a1;

    /* get position of second segment */
    k=seglist[dest]->length;

    /* aligned to its own alignment */
    a1=seglist[src]->align;
    k=(k+a1-1)&(0xffffffff-(a1-1));

    /* store as base of seg */
    seglist[src]->base=k;

    /* increase space allocated for data, and data mask, appropriately */
    p=checkRealloc(seglist[dest]->data,k+seglist[src]->length);
    q=checkRealloc(seglist[dest]->datmask,(k+seglist[src]->length+7)/8);

    /* get length of old segment */
    k=seglist[dest]->length;
    
    /* mark alignment space as unused */
    for(;k<seglist[src]->base;k++)
    {
	ClearNbit(q,k);
    }
    /* copy data/mask bits for second segment */
    for(;k<(seglist[src]->base+seglist[src]->length);k++)
    {
	if(GetNbit(seglist[src]->datmask,k-seglist[src]->base))
	{
	    p[k]=seglist[src]->data[k-seglist[src]->base];
	    SetNbit(q,k);
	}
	else
	{
	    ClearNbit(q,k);
	}
    }
    /* set total length */
    seglist[dest]->length=k;
    /* make alignment of combined seg equal to the more stringent of the alignments */
    if(seglist[src]->align>seglist[dest]->align)
	seglist[dest]->align=seglist[src]->align;
    /* combine flags */
    seglist[dest]->flags |= seglist[src]->flags;
    /* clear data/mask for second seg */
    free(seglist[src]->data);
    free(seglist[src]->datmask);
    seglist[src]->data=NULL;
    seglist[src]->datmask=NULL;

    /* mark second seg as part of combined seg */
    seglist[dest]->entryList=(PSEG)checkRealloc(seglist[dest]->entryList,(seglist[dest]->entryCount+1)*sizeof(SEG));
    seglist[dest]->entryList[seglist[dest]->entryCount]=(*seglist[src]);
    seglist[dest]->entryCount++;

    /* update references to second seg, to cover the whole entity */
    fixpubsegs(src,dest,seglist[src]->base);
    
    for(k=0;k<fixcount;k++)
    {
	if(relocs[k]->segnum==src)
	{
	    relocs[k]->segnum=dest;
	    relocs[k]->ofs+=seglist[src]->base;
	}
	if(relocs[k]->ttype==REL_SEGDISP)
	{
	    if(relocs[k]->target==src)
	    {
		relocs[k]->target=dest;
		relocs[k]->disp+=seglist[src]->base;
	    }
	}
	else if(relocs[k]->ttype==REL_SEGONLY)
	{
	    if(relocs[k]->target==src)
	    {
		relocs[k]->target=dest;
		relocs[k]->ttype=REL_SEGDISP;
		relocs[k]->disp=seglist[src]->base;
	    }
	}
	if((relocs[k]->ftype==REL_SEGFRAME) ||
	   (relocs[k]->ftype==REL_LILEFRAME))
	{
	    if(relocs[k]->frame==src)
	    {
		relocs[k]->frame=dest;
	    }
	}
    }

    if(gotstart)
    {
	if(startaddr.ttype==REL_SEGDISP)
	{
	    if(startaddr.target==src)
	    {
		startaddr.target=dest;
		startaddr.disp+=seglist[src]->base;
	    }
	}
	else if(startaddr.ttype==REL_SEGONLY)
	{
	    if(startaddr.target==src)
	    {
		startaddr.target=dest;
		startaddr.disp=seglist[src]->base;
		startaddr.ttype=REL_SEGDISP;
	    }
	}
	if((startaddr.ftype==REL_SEGFRAME) ||
	   (startaddr.ftype==REL_LILEFRAME))
	{
	    if(startaddr.frame==src)
	    {
		startaddr.frame=dest;
	    }
	}
    }

    for(k=0;k<grpcount;k++)
    {
	if(!grplist[k]) continue;
	if(grplist[k]->segnum==src)
	{
	    grplist[k]->segnum=dest;
	}
	for(n=0;n<grplist[k]->numsegs;n++)
	{
	    if(grplist[k]->segindex[n]==src)
	    {
		grplist[k]->segindex[n]=dest;
	    }
	}
    }

    /* remove the second one from the list */
    free(seglist[src]);
    seglist[src]=0;
}

void combine_common(long i,long j)
{
    UINT k,n;
    PUCHAR p,q;

    if(seglist[j]->length>seglist[i]->length)
    {
	k=seglist[i]->length;
	seglist[i]->length=seglist[j]->length;
	seglist[j]->length=k;
	p=seglist[i]->data;
	q=seglist[i]->datmask;
	seglist[i]->data=seglist[j]->data;
	seglist[i]->datmask=seglist[j]->datmask;
    }
    else
    {
	p=seglist[j]->data;
	q=seglist[j]->datmask;
    }
    for(k=0;k<seglist[j]->length;k++)
    {
	if(GetNbit(q,k))
	{
	    if(GetNbit(seglist[i]->datmask,k))
	    {
		if(seglist[i]->data[k]!=p[k])
		{
		    ReportError(ERR_OVERWRITE);
		}
	    }
	    else
	    {
		SetNbit(seglist[i]->datmask,k);
		seglist[i]->data[k]=p[k];
	    }
	}
    }
    free(p);
    free(q);

    fixpubsegs(j,i,0);
    
    for(k=0;k<fixcount;k++)
    {
	if(relocs[k]->segnum==j)
	{
	    relocs[k]->segnum=i;
	}
	if(relocs[k]->ttype==REL_SEGDISP)
	{
	    if(relocs[k]->target==j)
	    {
		relocs[k]->target=i;
	    }
	}
	else if(relocs[k]->ttype==REL_SEGONLY)
	{
	    if(relocs[k]->target==j)
	    {
		relocs[k]->target=i;
	    }
	}
	if((relocs[k]->ftype==REL_SEGFRAME) ||
	   (relocs[k]->ftype==REL_LILEFRAME))
	{
	    if(relocs[k]->frame==j)
	    {
		relocs[k]->frame=i;
	    }
	}
    }

    if(gotstart)
    {
	if(startaddr.ttype==REL_SEGDISP)
	{
	    if(startaddr.target==j)
	    {
		startaddr.target=i;
	    }
	}
	else if(startaddr.ttype==REL_SEGONLY)
	{
	    if(startaddr.target==j)
	    {
		startaddr.target=i;
	    }
	}
	if((startaddr.ftype==REL_SEGFRAME) ||
	   (startaddr.ftype==REL_LILEFRAME))
	{
	    if(startaddr.frame==j)
	    {
		startaddr.frame=i;
	    }
	}
    }

    for(k=0;k<grpcount;k++)
    {
	if(!grplist[k]) continue;
	if(grplist[k]->segnum==j)
	{
	    grplist[k]->segnum=i;
	}
	for(n=0;n<grplist[k]->numsegs;n++)
	{
	    if(grplist[k]->segindex[n]==j)
	    {
		grplist[k]->segindex[n]=i;
	    }
	}
    }

    free(seglist[j]);
    seglist[j]=0;
}

void combine_groups(long i,long j)
{
    long n,m;

    for(n=0;n<grplist[j]->numsegs;n++)
    {
	for(m=0;m<grplist[i]->numsegs;m++)
	{
	    if(grplist[j]->segindex[n]==grplist[i]->segindex[m])
	    {
		break;
	    }
	}
	if(m==grplist[i]->numsegs)
	{
	    grplist[i]->segindex[m]=grplist[j]->segindex[n];
	    grplist[i]->numsegs++;
	}
    }
    n=grplist[j]->segnum;
    free(seglist[n]);
    seglist[n]=0;
    free(grplist[j]);
    grplist[j]=0;

    fixpubgrps(j,i);

    for(n=0;n<fixcount;n++)
    {
	if(relocs[n]->ftype==REL_GRPFRAME)
	{
	    if(relocs[n]->frame==j)
	    {
		relocs[n]->frame=i;
	    }
	}
	if((relocs[n]->ttype==REL_GRPONLY) || (relocs[n]->ttype==REL_GRPDISP))
	{
	    if(relocs[n]->target==j)
	    {
		relocs[n]->target=i;
	    }
	}
    }

    if(gotstart)
    {
	if((startaddr.ttype==REL_GRPDISP) || (startaddr.ttype==REL_GRPONLY))
	{
	    if(startaddr.target==j)
	    {
		startaddr.target=i;
	    }
	}
	if(startaddr.ftype==REL_GRPFRAME)
	{
	    if(startaddr.frame==j)
	    {
		startaddr.frame=i;
	    }
	}
    }
}

void combineBlocks()
{
    long i,j,k;
    char *name;
    long attr;
    UINT count;
    UINT *slist;
    UINT curseg;

    for(i=0;i<segcount;i++)
    {
	if(seglist[i]&&((seglist[i]->attr&SEG_ALIGN)!=SEG_ABS))
	{
	    if(seglist[i]->winFlags & WINF_COMDAT) continue; /* don't combine COMDAT segments */
	    name=namelist[seglist[i]->nameindex];
	    attr=seglist[i]->attr&(SEG_COMBINE|SEG_USE32);
	    switch(attr&SEG_COMBINE)
	    {
	    case SEG_STACK:
		for(j=i+1;j<segcount;j++)
		{
		    if(!seglist[j]) continue;
		    if(seglist[j]->winFlags & WINF_COMDAT) continue;
		    if((seglist[j]->attr&SEG_ALIGN)==SEG_ABS) continue;
		    if((seglist[j]->attr&SEG_COMBINE)!=SEG_STACK) continue;
		    combine_segments(i,j);
		}
		break;
	    case SEG_PUBLIC:
	    case SEG_PUBLIC2:
	    case SEG_PUBLIC3:
		slist=(UINT*)checkMalloc(sizeof(UINT));
		slist[0]=i;
		/* get list of segments to combine */
		for(j=i+1,count=1;j<segcount;j++)
		{
		    if(!seglist[j]) continue;
		    if(seglist[j]->winFlags & WINF_COMDAT) continue;
		    if((seglist[j]->attr&SEG_ALIGN)==SEG_ABS) continue;
		    if(attr!=(seglist[j]->attr&(SEG_COMBINE|SEG_USE32))) continue;
		    if(strcmp(name,namelist[seglist[j]->nameindex])!=0) continue;
		    slist=(UINT*)checkRealloc(slist,(count+1)*sizeof(UINT));
		    slist[count]=j;
		    count++;
		}
		/* sort them by sortorder */
		for(j=1;j<count;j++)
		{
		    curseg=slist[j];
		    for(k=j-1;k>=0;k--)
		    {
			if(seglist[slist[k]]->orderindex<0) break;
			if(seglist[curseg]->orderindex>=0)
			{
			    if(strcmp(namelist[seglist[curseg]->orderindex],
				      namelist[seglist[slist[k]]->orderindex])>=0) break;
			}
			slist[k+1]=slist[k];
		    }
		    k++;
		    slist[k]=curseg;
		}
		/* then combine in that order */
		for(j=1;j<count;j++)
		{
		    combine_segments(i,slist[j]);
		}
		free(slist);
		break;
	    case SEG_COMMON:
		for(j=i+1;j<segcount;j++)
		{
		    if((seglist[j]&&((seglist[j]->attr&SEG_ALIGN)!=SEG_ABS)) &&
		       ((seglist[i]->attr&(SEG_ALIGN|SEG_COMBINE|SEG_USE32))==(seglist[j]->attr&(SEG_ALIGN|SEG_COMBINE|SEG_USE32)))
		       &&
		       (strcmp(name,namelist[seglist[j]->nameindex])==0)
		       && !(seglist[j]->winFlags & WINF_COMDAT)
		       )
		    {
			combine_common(i,j);
		    }
		}
		break;
	    default:
		break;
	    }
	}
    }
    for(i=0;i<grpcount;i++)
    {
	if(grplist[i])
	{
	    for(j=i+1;j<grpcount;j++)
	    {
		if(!grplist[j]) continue;
		if(strcmp(namelist[grplist[i]->nameindex],namelist[grplist[j]->nameindex])==0)
		{
		    combine_groups(i,j);
		}
	    }
	}
    }

}
static int relocCompare(const void *x1,const void *x2)
{
    if(!x1) return 1; /* move NULLs to end of list */
    if(!x2) return -1;
    /* do actual comparison */
    if((*(PPRELOC)x1)->segnum==(*(PPRELOC)x2)->segnum)
    {
	if((*(PPRELOC)x1)->ofs==(*(PPRELOC)x2)->ofs) return 0;
	if((*(PPRELOC)x1)->ofs<(*(PPRELOC)x2)->ofs) return -1;
	return 1;
    }
    
    if((*(PPRELOC)x1)->segnum>(*(PPRELOC)x2)->segnum) return 1;
    return -1;
}

static void buildPERelocs(void)
{
    int i,j;
    PRELOC r;
    UINT k;
    PSEG relocSect;
    UINT curStartPos;
    UINT curBlockPos;
    UINT curRelocSeg;
    PUCHAR relocData=NULL;
    UINT relocDataSize=0;

    if(!relocsRequired) return;
    
    /* create a section for the relocations */
    relocSectNum=createSection("relocs",-1);
    seglist[relocSectNum]->attr=SEG_PUBLIC | SEG_BYTE;
    seglist[relocSectNum]->winFlags=
        (WINF_INITDATA | WINF_SHARED | WINF_DISCARDABLE | WINF_READABLE) ^ WINF_NEG_FLAGS;

    /* group relocations by source section number */
    qsort(relocs,fixcount,sizeof(PRELOC),relocCompare);
    
    for(i=0,curStartPos=0,curBlockPos=0,curRelocSeg=0xffffffff;i<fixcount;i++)
    {
	r=relocs[i];
        switch(r->rtype)
        {
        case FIX_SELF_OFS32:
        case FIX_SELF_OFS32_2:
        case FIX_SELF_OFS16:
        case FIX_SELF_OFS16_2:
	case FIX_SELF_LBYTE:
	case FIX_RVA32:
	    continue; /* self-relative fixups and RVA fixups don't relocate */
        default:
	    break;
        }
        if((r->ofs>=(curStartPos+0x1000)) /* more than 4K past block start? */
	   || !relocDataSize || (r->segnum!=curRelocSeg)) /* or first reloc */
        {
            j=relocDataSize&3;
            if(j) /* unaligned block position */
            {
                relocDataSize+=4-j; /* update length to align block */
                /* and block memory */
                relocData=(PUCHAR)checkRealloc(relocData,relocDataSize);
                /* update size of current reloc block */
                k=relocData[curBlockPos+4];
                k+=relocData[curBlockPos+5]<<8;
                k+=relocData[curBlockPos+6]<<16;
                k+=relocData[curBlockPos+7]<<24;
                k+=4-j;
                relocData[curBlockPos+4]=k&0xff;
                relocData[curBlockPos+5]=(k>>8)&0xff;
                relocData[curBlockPos+6]=(k>>16)&0xff;
                relocData[curBlockPos+7]=(k>>24)&0xff;
                for(j=4-j;j>0;j--)
                {
                    relocData[relocDataSize-j]=0;
                }
            }
            curBlockPos=relocDataSize; /* get address in section of current block */
            relocDataSize+=8; /* 8 bytes block header */
            /* increase size of block */
            relocData=(PUCHAR)checkRealloc(relocData,relocDataSize);
            /* store reloc base address, and block size */
            curStartPos=r->ofs; /* start of page */

            /* start pos is relative to image base */
            relocData[curBlockPos]=curStartPos&0xff;
            relocData[curBlockPos+1]=(curStartPos>>8)&0xff;
            relocData[curBlockPos+2]=(curStartPos>>16)&0xff;
            relocData[curBlockPos+3]=(curStartPos>>24)&0xff;

            relocData[curBlockPos+4]=8; /* start size is 8 bytes */
            relocData[curBlockPos+5]=0;
            relocData[curBlockPos+6]=0;
            relocData[curBlockPos+7]=0;

	    /* add a final reloc to shift this reloc within segment */
	    simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	    /* use all 4 bytes for a 32-bit offset */
	    /* flag RVA */
	    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	    simpleRelocs[simpleRelocCount].targetseg=r->segnum;
	    simpleRelocs[simpleRelocCount].segnum=relocSectNum;
	    simpleRelocs[simpleRelocCount].ofs=curBlockPos;
	    simpleRelocCount++;
        }
	curRelocSeg=r->segnum;

        relocData=(PUCHAR)checkRealloc(relocData,relocDataSize+2);

        j=r->ofs-curStartPos; /* low 12 bits of address */

        switch(r->rtype)
        {
        case FIX_PTR1616:
        case FIX_OFS16:
        case FIX_OFS16_2:
            j|= PE_REL_LOW16;
            break;
        case FIX_PTR1632:
        case FIX_OFS32:
	case FIX_OFS32_2:
            j|= PE_REL_OFS32;
        }
        /* store relocation */
        relocData[relocDataSize]=j&0xff;
        relocData[relocDataSize+1]=(j>>8)&0xff;
        /* update block length */
        relocDataSize+=2;
        /* update size of current reloc block */
        k=relocData[curBlockPos+4];
        k+=relocData[curBlockPos+5]<<8;
        k+=relocData[curBlockPos+6]<<16;
        k+=relocData[curBlockPos+7]<<24;
        k+=2;
        relocData[curBlockPos+4]=k&0xff;
        relocData[curBlockPos+5]=(k>>8)&0xff;
        relocData[curBlockPos+6]=(k>>16)&0xff;
        relocData[curBlockPos+7]=(k>>24)&0xff;
    }
    /* if no fixups, then build NOP fixups, to make Windows NT happy */
    /* when it trys to relocate image */
    if(relocDataSize==0)
    {
	/* 12 bytes for dummy section */
        relocDataSize=12;
	relocData=(PUCHAR)checkMalloc(12);
	/* zero it out for now */
        for(i=0;i<12;i++) relocData[i]=0;
	relocData[4]=12; /* size of block */
    }

    seglist[relocSectNum]->data=relocData;
    seglist[relocSectNum]->length=relocDataSize;

    seglist[relocSectNum]->datmask=(PUCHAR)checkMalloc((relocDataSize+7)/8);
    memset(seglist[relocSectNum]->datmask,0xff,(relocDataSize+7)/8);
}

static void buildPEImports(void)
{
    UINT i,j,k,l;
    PPUBLIC pubdef;
    PIMPDLL dllList=NULL;
    UINT dllCount=0;
    SEG *impsect;
    PUCHAR data=NULL;
    UINT dllNameSize=0;
    UINT dllDataSize=0;
    UINT dllHintSize=0;
    UINT dllEntry,lookupEntry,boundEntry,hintEntry,nameEntry;

    /* build list of imported symbols */
    for(i=0;i<pubcount;i++)
    {
	for(j=0;j<publics[i].count;j++)
	{
	    /* get entry to use */
	    pubdef=(PPUBLIC)publics[i].object[j];
	    /* skip if not an import */
	    if(pubdef->type!=PUB_IMPORT) continue;
	    /* skip if not needed for any references */
	    if(!pubdef->useCount) continue;
	    for(k=0;k<dllCount;k++)
	    {
		if(!strcmp(dllList[k].name,pubdef->dllname)) break;
	    }
	    /* if dll not in list, add to it */
	    if(k==dllCount)
	    {
		dllCount++;
		dllList=(PIMPDLL)checkRealloc(dllList,dllCount*sizeof(IMPDLL));
		dllList[k].name=pubdef->dllname;
		dllList[k].entry=NULL;
		dllList[k].entryCount=0;
		dllNameSize+=strlen(pubdef->dllname)+1;
		dllDataSize+=8; /* space for NULLs at end of entry list */
	    }
	    for(l=0;l<dllList[k].entryCount;l++)
	    {
		/* skip if imported name vs no name */
		if(pubdef->impname && !dllList[k].entry[l].name) continue;
		if(!pubdef->impname && dllList[k].entry[l].name) continue;
		/* exit if names match */
		if(pubdef->impname)
		{
		    if(!strcmp(pubdef->impname,dllList[k].entry[l].name))
			break;
		}
		/* or if ordinals match */
		else
		{
		    if(pubdef->ordinal==dllList[k].entry[l].ordinal) break;
		}
	    }
	    if(l==dllList[k].entryCount)
	    {
		dllList[k].entryCount++;
		dllList[k].entry=(PIMPENTRY)checkRealloc(dllList[k].entry,dllList[k].entryCount*sizeof(IMPENTRY));
		dllList[k].entry[l].name=pubdef->impname;
		dllList[k].entry[l].ordinal=pubdef->ordinal;
		dllList[k].entry[l].publist=NULL;
		dllList[k].entry[l].pubcount=0;
		dllDataSize+=8; /* eight bytes per entry (Lookup + Bound) */
		if(pubdef->impname)
		{
		    /* add space for name if there is one */
		    /* including hint + terminating null */
		    /* with a pad byte if necessary for even length */
		    dllHintSize+=3+strlen(pubdef->impname)+1;
		    dllHintSize&=0xfffffffe;
		}
	    }
	    dllList[k].entry[l].publist=
		(PPPUBLIC)checkRealloc(dllList[k].entry[l].publist,(dllList[k].entry[l].pubcount+1)*sizeof(PPUBLIC));
	    dllList[k].entry[l].publist[dllList[k].entry[l].pubcount]=pubdef;
	    dllList[k].entry[l].pubcount++;
	}
    }

    /* if no imports needed, that's it */
    if(!dllCount) return;

    /* create the import section */
    importSectNum=createSection("imports",-1);
    seglist[importSectNum]->attr=SEG_PUBLIC | SEG_BYTE;
    seglist[importSectNum]->winFlags=
	(WINF_INITDATA | WINF_SHARED | WINF_READABLE ) ^ WINF_NEG_FLAGS;

    /* build section data */
    data=(PUCHAR)checkMalloc((dllCount+1)*PE_IMPORTDIRENTRY_SIZE+dllNameSize+dllDataSize+dllHintSize);

    /* get start addresses for tables */
    dllEntry=0;
    /* lookup tables follow dll directory */
    lookupEntry=(dllCount+1)*PE_IMPORTDIRENTRY_SIZE;
    /* hint-name table follows lookup tables */
    hintEntry=lookupEntry+dllDataSize;
    /* DLL names go at end of section */
    nameEntry=hintEntry+dllHintSize;

    for(i=0;i<dllCount;i++)
    {
	/* get table addresses */
	boundEntry=lookupEntry+4*(dllList[i].entryCount+1);
	
	/* set up directory entry */
	/* lookup table RVA */
	data[dllEntry+0]=lookupEntry&0xff;
	data[dllEntry+1]=(lookupEntry>>8)&0xff;
	data[dllEntry+2]=(lookupEntry>>16)&0xff;
	data[dllEntry+3]=(lookupEntry>>24)&0xff;

	data[dllEntry+4]=0; /* no timestamp, as unbound */
	data[dllEntry+5]=0;
	data[dllEntry+6]=0;
	data[dllEntry+7]=0;

	data[dllEntry+8]=0; /* no forwarder chain */
	data[dllEntry+9]=0;
	data[dllEntry+10]=0;
	data[dllEntry+11]=0;

	/* dll name RVA */
	data[dllEntry+12]=nameEntry&0xff;
	data[dllEntry+13]=(nameEntry>>8)&0xff;
	data[dllEntry+14]=(nameEntry>>16)&0xff;
	data[dllEntry+15]=(nameEntry>>24)&0xff;

	/* bound lookup table RVA */
	data[dllEntry+16]=boundEntry&0xff;
	data[dllEntry+17]=(boundEntry>>8)&0xff;
	data[dllEntry+18]=(boundEntry>>16)&0xff;
	data[dllEntry+19]=(boundEntry>>24)&0xff;
	
	/* create some final relocs for the RVA addresses in the table */

	/* lookup table */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+3)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=importSectNum;
	simpleRelocs[simpleRelocCount].segnum=importSectNum;
	simpleRelocs[simpleRelocCount].ofs=dllEntry;
	simpleRelocCount++;

	/* dll name */
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=importSectNum;
	simpleRelocs[simpleRelocCount].segnum=importSectNum;
	simpleRelocs[simpleRelocCount].ofs=dllEntry+12;
	simpleRelocCount++;

	/* bound lookup table */
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=importSectNum;
	simpleRelocs[simpleRelocCount].segnum=importSectNum;
	simpleRelocs[simpleRelocCount].ofs=dllEntry+16;
	simpleRelocCount++;

	/* copy name to name table */
	strcpy(data+nameEntry,dllList[i].name);
	
	dllEntry+=PE_IMPORTDIRENTRY_SIZE;
	nameEntry+=strlen(dllList[i].name)+1;

	/* now build import tables for this DLL */
	for(j=0;j<dllList[i].entryCount;j++)
	{
	    if(dllList[i].entry[j].name)
	    {
		/* name - get address of hint/name entry in table */
		k=hintEntry;
		data[lookupEntry]=data[boundEntry]=k&0xff;
		data[lookupEntry+1]=data[boundEntry+1]=(k>>8)&0xff;
		data[lookupEntry+2]=data[boundEntry+2]=(k>>16)&0xff;
		data[lookupEntry+3]=data[boundEntry+3]=(k>>24)&0xff;
		/* and set up hint/name entry */
		data[hintEntry]=dllList[i].entry[j].ordinal&0xff;
		data[hintEntry+1]=(dllList[i].entry[j].ordinal>>8)&0xff;
		strcpy(data+hintEntry+2,dllList[i].entry[j].name);
		hintEntry+=3+strlen(dllList[i].entry[j].name);
		if(hintEntry&1)
		{
		    data[hintEntry]=0;
		    hintEntry++;
		}
		/* set up relocs for RVAs of hint name */
		simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+2)*sizeof(FIXUPREC));
		/* use all 4 bytes for a 32-bit offset */
		/* flag RVA */
		simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
		simpleRelocs[simpleRelocCount].targetseg=importSectNum;
		simpleRelocs[simpleRelocCount].segnum=importSectNum;
		simpleRelocs[simpleRelocCount].ofs=lookupEntry;
		simpleRelocCount++;

		/* second is for bound table */
		/* use all 4 bytes for a 32-bit offset */
		/* flag RVA */
		simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
		simpleRelocs[simpleRelocCount].targetseg=importSectNum;
		simpleRelocs[simpleRelocCount].segnum=importSectNum;
		simpleRelocs[simpleRelocCount].ofs=boundEntry;
		simpleRelocCount++;
	    }
	    else
	    {
		/* ordinal */
		k=dllList[i].entry[j].ordinal | PE_ORDINAL_FLAG;
		data[lookupEntry]=data[boundEntry]=k&0xff;
		data[lookupEntry+1]=data[boundEntry+1]=(k>>8)&0xff;
		data[lookupEntry+2]=data[boundEntry+2]=(k>>16)&0xff;
		data[lookupEntry+3]=data[boundEntry+3]=(k>>24)&0xff;
	    }

	    /* go through symbols related to this import, and set up their addresses */
	    for(k=0;k<dllList[i].entry[j].pubcount;k++)
	    {
		pubdef=dllList[i].entry[j].publist[k];
		pubdef->ofs=boundEntry;
		pubdef->segnum=importSectNum;
	    }

	    lookupEntry+=4;
	    boundEntry+=4;
	}
	/* NULL terminate lists */
	memset(data+lookupEntry,0,4);
	memset(data+boundEntry,0,4);
	/* move past these tables for next DLL */
	lookupEntry=boundEntry+4;
    }

    /* blank out final entry */
    memset(data+dllEntry,0,PE_IMPORTDIRENTRY_SIZE);

    seglist[importSectNum]->data=data;
    seglist[importSectNum]->length=nameEntry;

    seglist[importSectNum]->datmask=(PUCHAR)checkMalloc((nameEntry+7)/8);
    memset(seglist[importSectNum]->datmask,0xff,(nameEntry+7)/8);
}

static void buildPEExports(void)
{
    long i,j;
    UINT k;
    PSEG expSect;
    UINT namelen;
    UINT numNames=0;
    UINT RVAStart;
    UINT nameRVAStart;
    UINT ordinalStart;
    UINT nameSpaceStart;
    UINT minOrd;
    UINT maxOrd;
    UINT numOrds;
    PPEXPREC nameList;
    PEXPREC curName;

    if(!expcount) return; /* return if no exports */

    /* create the export section */
    exportSectNum=createSection("exports",-1);
    seglist[exportSectNum]->attr=SEG_PUBLIC | SEG_BYTE;
    seglist[exportSectNum]->winFlags=
	(WINF_INITDATA | WINF_SHARED | WINF_READABLE ) ^ WINF_NEG_FLAGS;
    expSect=seglist[exportSectNum];

    if(outname)
    {
        namelen=strlen(outname);
        /* search backwards for path separator */
        for(i=namelen-1;(i>=0) && (outname[i]!=PATH_CHAR);i--);
        if(i>=0) /* if found path separator */
        {
            outname+=(i+1); /* update name pointer past path */
            namelen -= (i+1); /* and reduce length */
        }
    }
    else namelen=0;

    expSect->length=PE_EXPORTHEADER_SIZE+4*expcount+namelen+1;
    /* min section size= header size + num exports * pointer size */
    /* plus space for null-terminated name */

    minOrd=0xffffffff; /* max ordinal num */
    maxOrd=0;

    for(i=0;i<expcount;i++)
    {
        /* check we've got an exported name */
        if(expdefs[i].exp_name && strlen(expdefs[i].exp_name))
        {
            /* four bytes for name pointer */
            /* two bytes for ordinal, 1 for null terminator */
            expSect->length+=strlen(expdefs[i].exp_name)+7;
            numNames++;
        }

        if(expdefs[i].flags&EXP_ORD) /* ordinal? */
        {
            if(expdefs[i].ordinal<minOrd) minOrd=expdefs[i].ordinal;
            if(expdefs[i].ordinal>maxOrd) maxOrd=expdefs[i].ordinal;
        }
    }

    numOrds=expcount; /* by default, number of RVAs=number of exports */
    if(maxOrd>=minOrd) /* actually got some ordinal references? */
    {
        i=maxOrd-minOrd+1; /* get number of ordinals */
        if(i>expcount) /* if bigger range than number of exports */
        {
            expSect->length+=4*(i-expcount); /* up length */
            numOrds=i; /* get new num RVAs */
        }
    }
    else
    {
        minOrd=1; /* if none defined, min is set to 1 */
    }

    expSect->data=(PUCHAR)checkMalloc(expSect->length);

    /* start with buf=all zero */
    memset(expSect->data,0,expSect->length);

    /* store creation time of export data */
    k=(UINT)time(NULL);
    expSect->data[4]=k&0xff;
    expSect->data[5]=(k>>8)&0xff;
    expSect->data[6]=(k>>16)&0xff;
    expSect->data[7]=(k>>24)&0xff;

    expSect->data[16]=(minOrd)&0xff; /* store ordinal base */
    expSect->data[17]=(minOrd>>8)&0xff;
    expSect->data[18]=(minOrd>>16)&0xff;
    expSect->data[19]=(minOrd>>24)&0xff;

    /* store number of RVAs */
    expSect->data[20]=numOrds&0xff;
    expSect->data[21]=(numOrds>>8)&0xff;
    expSect->data[22]=(numOrds>>16)&0xff;
    expSect->data[23]=(numOrds>>24)&0xff;

    RVAStart=PE_EXPORTHEADER_SIZE; /* start address of RVA table */
    nameRVAStart=RVAStart+numOrds*4; /* start of name table entries */
    ordinalStart=nameRVAStart+numNames*4; /* start of associated ordinal entries */
    nameSpaceStart=ordinalStart+numNames*2; /* start of actual names */

    /* store number of named exports */
    expSect->data[24]=numNames&0xff;
    expSect->data[25]=(numNames>>8)&0xff;
    expSect->data[26]=(numNames>>16)&0xff;
    expSect->data[27]=(numNames>>24)&0xff;

    /* store address of address table */
    expSect->data[28]=(RVAStart)&0xff;
    expSect->data[29]=((RVAStart)>>8)&0xff;
    expSect->data[30]=((RVAStart)>>16)&0xff;
    expSect->data[31]=((RVAStart)>>24)&0xff;

    /* store address of name table */
    expSect->data[32]=(nameRVAStart)&0xff;
    expSect->data[33]=((nameRVAStart)>>8)&0xff;
    expSect->data[34]=((nameRVAStart)>>16)&0xff;
    expSect->data[35]=((nameRVAStart)>>24)&0xff;

    /* store address of ordinal table */
    expSect->data[36]=(ordinalStart)&0xff;
    expSect->data[37]=((ordinalStart)>>8)&0xff;
    expSect->data[38]=((ordinalStart)>>16)&0xff;
    expSect->data[39]=((ordinalStart)>>24)&0xff;

    /* build RVA relocs */
    simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+3)*sizeof(FIXUPREC));
    /* use all 4 bytes for a 32-bit offset */
    /* flag RVA */
    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
    simpleRelocs[simpleRelocCount].targetseg=exportSectNum;
    simpleRelocs[simpleRelocCount].segnum=exportSectNum;
    simpleRelocs[simpleRelocCount].ofs=28;
    simpleRelocCount++;
    /* use all 4 bytes for a 32-bit offset */
    /* flag RVA */
    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
    simpleRelocs[simpleRelocCount].targetseg=exportSectNum;
    simpleRelocs[simpleRelocCount].segnum=exportSectNum;
    simpleRelocs[simpleRelocCount].ofs=32;
    simpleRelocCount++;
    /* use all 4 bytes for a 32-bit offset */
    /* flag RVA */
    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
    simpleRelocs[simpleRelocCount].targetseg=exportSectNum;
    simpleRelocs[simpleRelocCount].segnum=exportSectNum;
    simpleRelocs[simpleRelocCount].ofs=36;
    simpleRelocCount++;

    /* process numbered exports */
    for(i=0;i<expcount;i++)
    {
        if(expdefs[i].flags&EXP_ORD)
        {
            /* get current RVA */
            k=expSect->data[RVAStart+4*(expdefs[i].ordinal-minOrd)];
            k+=expSect->data[RVAStart+4*(expdefs[i].ordinal-minOrd)+1]<<8;
            k+=expSect->data[RVAStart+4*(expdefs[i].ordinal-minOrd)+2]<<16;
            k+=expSect->data[RVAStart+4*(expdefs[i].ordinal-minOrd)+3]<<24;
            if(k) /* error if already used */
            {
                printf("Duplicate export ordinal %i\n",expdefs[i].ordinal);
                exit(1);
            }
            /* get RVA of export entry */
            k=externs[expdefs[i].extindex].pubdef->ofs;
            /* store it */
            expSect->data[RVAStart+4*(expdefs[i].ordinal-minOrd)]=k&0xff;
            expSect->data[RVAStart+4*(expdefs[i].ordinal-minOrd)+1]=(k>>8)&0xff;
            expSect->data[RVAStart+4*(expdefs[i].ordinal-minOrd)+2]=(k>>16)&0xff;
            expSect->data[RVAStart+4*(expdefs[i].ordinal-minOrd)+3]=(k>>24)&0xff;
	    /* set up RVA reloc */
	    simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	    /* use all 4 bytes for a 32-bit offset */
	    /* flag RVA */
	    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	    simpleRelocs[simpleRelocCount].targetseg=externs[expdefs[i].extindex].pubdef->segnum;
	    simpleRelocs[simpleRelocCount].segnum=exportSectNum;
	    simpleRelocs[simpleRelocCount].ofs=RVAStart+4*(expdefs[i].ordinal-minOrd);
	    simpleRelocCount++;
        }
    }

    /* process non-numbered exports */
    for(i=0,j=RVAStart;i<expcount;i++)
    {
        if(!(expdefs[i].flags&EXP_ORD))
        {
            do
            {
                k=expSect->data[j];
                k+=expSect->data[j+1]<<8;
                k+=expSect->data[j+2]<<16;
                k+=expSect->data[j+3]<<24;
                if(k) j+=4;
            }
            while(k); /* move through table until we find a free spot */
            /* get RVA of export entry */
            k=externs[expdefs[i].extindex].pubdef->ofs;
            /* store RVA */
            expSect->data[j]=k&0xff;
            expSect->data[j+1]=(k>>8)&0xff;
            expSect->data[j+2]=(k>>16)&0xff;
            expSect->data[j+3]=(k>>24)&0xff;
            expdefs[i].ordinal=(j-RVAStart)/4+minOrd; /* store ordinal */
	    /* set up RVA reloc */
	    simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	    /* use all 4 bytes for a 32-bit offset */
	    /* flag RVA */
	    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	    simpleRelocs[simpleRelocCount].targetseg=externs[expdefs[i].extindex].pubdef->segnum;
	    simpleRelocs[simpleRelocCount].segnum=exportSectNum;
	    simpleRelocs[simpleRelocCount].ofs=j;
	    simpleRelocCount++;
            j+=4;
        }
    }

    if(numNames) /* sort name table if present */
    {
        nameList=(PPEXPREC)checkMalloc(numNames*sizeof(PEXPREC));
        j=0; /* no entries yet */
        for(i=0;i<expcount;i++)
        {
            if(expdefs[i].exp_name && expdefs[i].exp_name[0])
            {
                /* make entry in name list */
                nameList[j]=&expdefs[i];
                j++;
            }
        }
        /* sort them into order */
        for(i=1;i<numNames;i++)
        {
            curName=nameList[i];
            for(j=i-1;j>=0;j--)
            {
                /* break out if we're above previous entry */
                if(strcmp(curName->exp_name,nameList[j]->exp_name)>=0)
                {
                    break;
                }
                /* else move entry up */
                nameList[j+1]=nameList[j];
            }
            j++; /* move to one after better entry */
            nameList[j]=curName; /* insert current entry into position */
        }
        /* and store */
        for(i=0;i<numNames;i++)
        {
            /* store ordinal */
            expSect->data[ordinalStart]=(nameList[i]->ordinal-minOrd)&0xff;
            expSect->data[ordinalStart+1]=((nameList[i]->ordinal-minOrd)>>8)&0xff;
            ordinalStart+=2;
            /* store name RVA */
            expSect->data[nameRVAStart]=(nameSpaceStart)&0xff;
            expSect->data[nameRVAStart+1]=((nameSpaceStart)>>8)&0xff;
            expSect->data[nameRVAStart+2]=((nameSpaceStart)>>16)&0xff;
            expSect->data[nameRVAStart+3]=((nameSpaceStart)>>24)&0xff;
	    /* set up RVA reloc */
	    simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	    /* use all 4 bytes for a 32-bit offset */
	    /* flag RVA */
	    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	    simpleRelocs[simpleRelocCount].targetseg=exportSectNum;
	    simpleRelocs[simpleRelocCount].segnum=exportSectNum;
	    simpleRelocs[simpleRelocCount].ofs=nameRVAStart;
	    simpleRelocCount++;
            nameRVAStart+=4;
            /* store name */
	    if(nameList[i]->exp_name)
	    {
		strcpy(expSect->data+nameSpaceStart,nameList[i]->exp_name);
		nameSpaceStart+=strlen(nameList[i]->exp_name);
	    }
	    
            /* store NULL */
            expSect->data[nameSpaceStart]=0;
            nameSpaceStart++;
        }
    }

    /* store library name */
    for(j=0;j<namelen;j++)
    {
        expSect->data[nameSpaceStart+j]=outname[j];
    }
    if(namelen)
    {
        expSect->data[nameSpaceStart+j]=0;
        /* store name RVA */
        expSect->data[12]=(nameSpaceStart)&0xff;
        expSect->data[13]=((nameSpaceStart)>>8)&0xff;
        expSect->data[14]=((nameSpaceStart)>>16)&0xff;
        expSect->data[15]=((nameSpaceStart)>>24)&0xff;
	/* set up RVA reloc */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=exportSectNum;
	simpleRelocs[simpleRelocCount].segnum=exportSectNum;
	simpleRelocs[simpleRelocCount].ofs=12;
	simpleRelocCount++;
    }

    expSect->datmask=(PUCHAR)checkMalloc((expSect->length+7)/8);
    memset(expSect->datmask,0xff,(expSect->length+7)/8);
}

static void buildPEResources(void)
{
    long i,j;
    UINT k;
    PUCHAR data;
    RESOURCE curres;
    int numtypes,numnamedtypes;
    int numPairs,numnames,numids;
    UINT nameSize,dataSize;
    UINT tableSize,dataListSize;
    UINT namePos,dataPos,tablePos,dataListPos;
    UINT curTypePos,curNamePos,curLangPos;
    char *curTypeName,*curName;
    int curTypeId,curId;

    if(!rescount) return;

    resourceSectNum=createSection("resource",-1);
    seglist[resourceSectNum]->attr=SEG_PUBLIC | SEG_BYTE;
    seglist[resourceSectNum]->winFlags=
        (WINF_INITDATA | WINF_SHARED | WINF_READABLE) ^ WINF_NEG_FLAGS;

    /* sort into type-id order */
    for(i=1;i<rescount;i++)
    {
        curres=resource[i];
        for(j=i-1;j>=0;j--)
        {
            if(resource[j].typename)
            {
                if(!curres.typename) break;
                if(wstricmp(curres.typename,resource[j].typename)>0) break;
                if(wstricmp(curres.typename,resource[j].typename)==0)
                {
                    if(resource[j].name)
                    {
                        if(!curres.name) break;
                        if(wstricmp(curres.name,resource[j].name)>0) break;
                        if(wstricmp(curres.name,resource[j].name)==0)
                        {
                            if(resource[j].languageid>curres.languageid)
                                break;
                            if(resource[j].languageid==curres.languageid)
                            {
                                printf("Error duplicate resource ID\n");
                                exit(1);
                            }
                        }
                    }
                    else
                    {
                        if(!curres.name)
                        {
                            if(curres.id>resource[j].id) break;
                            if(curres.id==resource[j].id)
                            {
                                if(resource[j].languageid>curres.languageid)
                                    break;
                                if(resource[j].languageid==curres.languageid)
                                {
                                    printf("Error duplicate resource ID\n");
                                    exit(1);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                if(!curres.typename)
                {
                    if(curres.typeid>resource[j].typeid) break;
                    if(curres.typeid==resource[j].typeid)
                    {
                        if(resource[j].name)
                        {
                            if(!curres.name) break;
                            if(wstricmp(curres.name,resource[j].name)>0) break;
                            if(wstricmp(curres.name,resource[j].name)==0)
                            {
                                if(resource[j].languageid>curres.languageid)
                                    break;
                                if(resource[j].languageid==curres.languageid)
                                {
                                    printf("Error duplicate resource ID\n");
                                    exit(1);
                                }
                            }
                        }
                        else
                        {
                            if(!curres.name)
                            {
                                if(curres.id>resource[j].id) break;
                                if(curres.id==resource[j].id)
                                {
                                    if(resource[j].languageid>curres.languageid)
                                        break;
                                    if(resource[j].languageid==curres.languageid)
                                    {
                                        printf("Error duplicate resource ID\n");
                                        exit(1);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            resource[j+1]=resource[j];
        }
        j++;
        resource[j]=curres;
    }

    nameSize=0;
    dataSize=0;
    for(i=0;i<rescount;i++)
    {
        if(resource[i].typename)
        {
            nameSize+=2+2*wstrlen(resource[i].typename);
	}
        if(resource[i].name)
        {
            nameSize+=2+2*wstrlen(resource[i].name);
        }
        dataSize+=resource[i].length+3;
	dataSize&=0xfffffffc;
    }

    /* count named types */
    numnamedtypes=0;
    numPairs=rescount;
    for(i=0;i<rescount;i++)
    {
        if(!resource[i].typename) break;
        if((i>0) && !wstricmp(resource[i].typename,resource[i-1].typename))
        {
            if(resource[i].name)
            {
                if(wstricmp(resource[i].name,resource[i-1].name)==0)
                    numPairs--;
            }
            else
            {
                if(!resource[i-1].name && (resource[i].id ==resource[i-1].id))
                    numPairs--;
            }
            continue;
        }
        numnamedtypes++;
    }
    numtypes=numnamedtypes;
    for(;i<rescount;i++)
    {
        if((i>0) && !resource[i-1].typename && (resource[i].typeid==resource[i-1].typeid))
        {
            if(resource[i].name)
            {
                if(wstricmp(resource[i].name,resource[i-1].name)==0)
                    numPairs--;
            }
            else
            {
                if(!resource[i-1].name && (resource[i].id ==resource[i-1].id))
                    numPairs--;
            }
            continue;
        }
        numtypes++;
    }

    tableSize=(rescount+numtypes+numPairs)*PE_RESENTRY_SIZE+
        (numtypes+numPairs+1)*PE_RESDIR_SIZE;
    dataListSize=rescount*PE_RESDATAENTRY_SIZE;

    tablePos=0;
    dataListPos=tableSize;
    namePos=tableSize+dataListSize;
    dataPos=tableSize+nameSize+dataListSize+3;
    dataPos&=0xfffffffc;

    data=(PUCHAR)checkMalloc(dataPos+dataSize);

    /* empty section to start with */
    memset(data,0,dataPos+dataSize);

    /* build master directory */
    /* store time/date of creation */
    k=(UINT)time(NULL);
    data[4]=k&0xff;
    data[5]=(k>>8)&0xff;
    data[6]=(k>>16)&0xff;
    data[7]=(k>>24)&0xff;

    data[12]=numnamedtypes&0xff;
    data[13]=(numnamedtypes>>8)&0xff;
    data[14]=(numtypes-numnamedtypes)&0xff;
    data[15]=((numtypes-numnamedtypes)>>8)&0xff;

    tablePos=16+numtypes*PE_RESENTRY_SIZE;
    curTypePos=16;
    curTypeName=NULL;
    curTypeId=-1;
    for(i=0;i<rescount;i++)
    {
        if(!(resource[i].typename && curTypeName &&
	     !wstricmp(resource[i].typename,curTypeName))
	   && !(!resource[i].typename && !curTypeName && curTypeId==resource[i].typeid))
        {
            if(resource[i].typename)
            {
                data[curTypePos]=(namePos)&0xff;
                data[curTypePos+1]=((namePos)>>8)&0xff;
                data[curTypePos+2]=((namePos)>>16)&0xff;
                data[curTypePos+3]=(((namePos)>>24)&0xff)|0x80;
                curTypeName=resource[i].typename;
                k=wstrlen(curTypeName);
                data[namePos]=k&0xff;
                data[namePos+1]=(k>>8)&0xff;
                namePos+=2;
                memcpy(data+namePos,curTypeName,k*2);
                namePos+=k*2;
                curTypeId=-1;
            }
            else
            {
                curTypeName=NULL;
                curTypeId=resource[i].typeid;
                data[curTypePos]=curTypeId&0xff;
                data[curTypePos+1]=(curTypeId>>8)&0xff;
                data[curTypePos+2]=(curTypeId>>16)&0xff;
                data[curTypePos+3]=(curTypeId>>24)&0xff;
            }
            data[curTypePos+4]=(tablePos)&0xff;
            data[curTypePos+5]=((tablePos)>>8)&0xff;
            data[curTypePos+6]=((tablePos)>>16)&0xff;
            data[curTypePos+7]=(((tablePos)>>24)&0x7f) | 0x80;

            numnames=numids=0;
            for(j=i;j<rescount;j++)
            {
                if(resource[i].typename)
                {
                    if(!resource[j].typename) break;
                    if(wstricmp(resource[i].typename,resource[j].typename)!=0) break;
                }
                else
                {
                    if(resource[j].typeid!=resource[i].typeid) break;
                }
                if(resource[j].name)
                {
                    if(((j>i) && (wstricmp(resource[j].name,resource[j-1].name)!=0))
		       || (j==i))
                        numnames++;
                }
                else
                {
                    if(((j>i) && (resource[j-1].name || (resource[j].id!=resource[j-1].id)))
		       || (j==i))
                        numids++;
                }
            }
	    /* store time/date of creation */
	    k=(UINT)time(NULL);
	    data[tablePos+4]=k&0xff;
	    data[tablePos+5]=(k>>8)&0xff;
	    data[tablePos+6]=(k>>16)&0xff;
	    data[tablePos+7]=(k>>24)&0xff;

            data[tablePos+12]=numnames&0xff;
            data[tablePos+13]=(numnames>>8)&0xff;
            data[tablePos+14]=numids&0xff;
            data[tablePos+15]=(numids>>8)&0xff;

            curNamePos=tablePos+PE_RESDIR_SIZE;
            curName=NULL;
            curId=-1;
            tablePos+=PE_RESDIR_SIZE+(numids+numnames)*PE_RESENTRY_SIZE;
            curTypePos+=PE_RESENTRY_SIZE;
        }
        if(!(resource[i].name && curName &&
	     !wstricmp(resource[i].name,curName))
	   && !(!resource[i].name && !curName && curId==resource[i].id))
        {
            if(resource[i].name)
            {
                data[curNamePos]=(namePos)&0xff;
                data[curNamePos+1]=((namePos)>>8)&0xff;
                data[curNamePos+2]=((namePos)>>16)&0xff;
                data[curNamePos+3]=(((namePos)>>24)&0xff)|0x80;
                curName=resource[i].name;
                k=wstrlen(curName);
                data[namePos]=k&0xff;
                data[namePos+1]=(k>>8)&0xff;
                namePos+=2;
                memcpy(data+namePos,curName,k*2);
                namePos+=k*2;
                curId=-1;
            }
            else
            {
                curName=NULL;
                curId=resource[i].id;
                data[curNamePos]=curId&0xff;
                data[curNamePos+1]=(curId>>8)&0xff;
                data[curNamePos+2]=(curId>>16)&0xff;
                data[curNamePos+3]=(curId>>24)&0xff;
            }
            data[curNamePos+4]=(tablePos)&0xff;
            data[curNamePos+5]=((tablePos)>>8)&0xff;
            data[curNamePos+6]=((tablePos)>>16)&0xff;
            data[curNamePos+7]=(((tablePos)>>24)&0x7f) | 0x80;

            numids=0;
            for(j=i;j<rescount;j++)
            {
                if(resource[i].typename)
                {
                    if(!resource[j].typename) break;
                    if(wstricmp(resource[i].typename,resource[j].typename)!=0) break;
                }
                else
                {
                    if(resource[j].typeid!=resource[i].typeid) break;
                }
                if(resource[i].name)
                {
                    if(!resource[j].name) break;
                    if(wstricmp(resource[j].name,resource[i].name)!=0) break;
                }
                else
                {
                    if(resource[j].id!=resource[i].id) break;
                }
                numids++;
            }
            numnames=0; /* no names for languages */
	    /* store time/date of creation */
	    k=(UINT)time(NULL);
	    data[tablePos+4]=k&0xff;
	    data[tablePos+5]=(k>>8)&0xff;
	    data[tablePos+6]=(k>>16)&0xff;
	    data[tablePos+7]=(k>>24)&0xff;

            data[tablePos+12]=numnames&0xff;
            data[tablePos+13]=(numnames>>8)&0xff;
            data[tablePos+14]=numids&0xff;
            data[tablePos+15]=(numids>>8)&0xff;

            curLangPos=tablePos+PE_RESDIR_SIZE;
            tablePos+=PE_RESDIR_SIZE+numids*PE_RESENTRY_SIZE;
            curNamePos+=PE_RESENTRY_SIZE;
        }
        data[curLangPos]=resource[i].languageid&0xff;
        data[curLangPos+1]=(resource[i].languageid>>8)&0xff;
        data[curLangPos+2]=(resource[i].languageid>>16)&0xff;
        data[curLangPos+3]=(resource[i].languageid>>24)&0xff;

        data[curLangPos+4]=(dataListPos)&0xff;
        data[curLangPos+5]=((dataListPos)>>8)&0xff;
        data[curLangPos+6]=((dataListPos)>>16)&0xff;
        data[curLangPos+7]=(((dataListPos)>>24)&0x7f);
        curLangPos+=PE_RESENTRY_SIZE;


        data[dataListPos]=(dataPos)&0xff;
        data[dataListPos+1]=((dataPos)>>8)&0xff;
        data[dataListPos+2]=((dataPos)>>16)&0xff;
        data[dataListPos+3]=((dataPos)>>24)&0xff;

	/* add reloc for real RVA */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=resourceSectNum;
	simpleRelocs[simpleRelocCount].segnum=resourceSectNum;
	simpleRelocs[simpleRelocCount].ofs=dataListPos;
	simpleRelocCount++;

        data[dataListPos+4]=resource[i].length&0xff;
        data[dataListPos+5]=(resource[i].length>>8)&0xff;
        data[dataListPos+6]=(resource[i].length>>16)&0xff;
        data[dataListPos+7]=(resource[i].length>>24)&0xff;
        memcpy(data+dataPos,resource[i].data,resource[i].length);
        dataPos+=resource[i].length+3;
	dataPos&=0xfffffffc;
        dataListPos+=PE_RESDATAENTRY_SIZE;
    }

    seglist[resourceSectNum]->data=data;
    seglist[resourceSectNum]->length=dataPos+dataSize;
    seglist[resourceSectNum]->datmask=(PUCHAR)checkMalloc((dataPos+dataSize+7)/8);
    memset(seglist[resourceSectNum]->datmask,0xff,(dataPos+dataSize+7)/8);
}

static void buildPEHeader(void)
{
    long headerSectNum;
    long i,j,k;
    long lastSection;
    PUCHAR headbuf;
    PUCHAR stubData;
    FILE*outfile;
    UINT headerSize;
    UINT headerVirtSize;
    UINT stubSize;
    long nameIndex;
    UINT sectionStart;
    UINT headerStart;
    UINT codeBase=0;
    UINT dataBase=0;
    UINT codeSize=0;
    UINT dataSize=0;

    /* build header */
    headerSectNum=createSection("header",-1);

    getStub(&stubData,&stubSize);

    if(!stubData)
    {
	stubData=defaultStub;
	stubSize=defaultStubSize;
    }

    headerStart=stubSize; /* get start of PE header */
    headerStart+=7;
    headerStart&=0xfffffff8; /* align PE header to 8 byte boundary */

    headerSize=PE_HEADBUF_SIZE+outcount*PE_OBJECTENTRY_SIZE+stubSize;
    headerVirtSize=headerSize+(objectAlign-1);
    headerVirtSize&=(0xffffffff-(objectAlign-1));
    headerSize+=(fileAlign-1);
    headerSize&=(0xffffffff-(fileAlign-1));

    seglist[headerSectNum]->length=headerSize;

    headbuf=checkMalloc(headerSize);

    seglist[headerSectNum]->data=headbuf;
    seglist[headerSectNum]->datmask=checkMalloc((headerSize+7)/8);

    memset(headbuf,0,headerSize);
    memset(seglist[headerSectNum]->datmask,0xff,(headerSize+7)/8);
    
    for(i=0;i<stubSize;i++) /* copy stub file */
        headbuf[i]=stubData[i];

    headbuf[0x3c]=headerStart&0xff;         /* store pointer to PE header */
    headbuf[0x3d]=(headerStart>>8)&0xff;
    headbuf[0x3e]=(headerStart>>16)&0xff;
    headbuf[0x3f]=(headerStart>>24)&0xff;

    headbuf[headerStart+PE_SIGNATURE]=0x50;   /* P */
    headbuf[headerStart+PE_SIGNATURE+1]=0x45; /* E */
    headbuf[headerStart+PE_SIGNATURE+2]=0x00; /* 0 */
    headbuf[headerStart+PE_SIGNATURE+3]=0x00; /* 0 */
    headbuf[headerStart+PE_MACHINEID]=PE_INTEL386&0xff;
    headbuf[headerStart+PE_MACHINEID+1]=(PE_INTEL386>>8)&0xff;
    /* store time/date of creation */
    k=(UINT)time(NULL);
    headbuf[headerStart+PE_DATESTAMP]=k&0xff;
    headbuf[headerStart+PE_DATESTAMP+1]=(k>>8)&0xff;
    headbuf[headerStart+PE_DATESTAMP+2]=(k>>16)&0xff;
    headbuf[headerStart+PE_DATESTAMP+3]=(k>>24)&0xff;

    headbuf[headerStart+PE_HDRSIZE]=PE_OPTIONAL_HEADER_SIZE&0xff;
    headbuf[headerStart+PE_HDRSIZE+1]=(PE_OPTIONAL_HEADER_SIZE>>8)&0xff;

    i=PE_FILE_EXECUTABLE | PE_FILE_32BIT;                   /* get flags */
    if(buildDll)
    {
        i|= PE_FILE_LIBRARY;                /* if DLL, flag it */
    }
    headbuf[headerStart+PE_FLAGS]=i&0xff;                   /* store them */
    headbuf[headerStart+PE_FLAGS+1]=(i>>8)&0xff;

    headbuf[headerStart+PE_MAGIC]=PE_MAGICNUM&0xff; /* store magic number */
    headbuf[headerStart+PE_MAGIC+1]=(PE_MAGICNUM>>8)&0xff;

    headbuf[headerStart+PE_IMAGEBASE]=imageBase&0xff; /* store image base */
    headbuf[headerStart+PE_IMAGEBASE+1]=(imageBase>>8)&0xff;
    headbuf[headerStart+PE_IMAGEBASE+2]=(imageBase>>16)&0xff;
    headbuf[headerStart+PE_IMAGEBASE+3]=(imageBase>>24)&0xff;

    headbuf[headerStart+PE_FILEALIGN]=fileAlign&0xff; /* store image base */
    headbuf[headerStart+PE_FILEALIGN+1]=(fileAlign>>8)&0xff;
    headbuf[headerStart+PE_FILEALIGN+2]=(fileAlign>>16)&0xff;
    headbuf[headerStart+PE_FILEALIGN+3]=(fileAlign>>24)&0xff;

    headbuf[headerStart+PE_OBJECTALIGN]=objectAlign&0xff; /* store image base */
    headbuf[headerStart+PE_OBJECTALIGN+1]=(objectAlign>>8)&0xff;
    headbuf[headerStart+PE_OBJECTALIGN+2]=(objectAlign>>16)&0xff;
    headbuf[headerStart+PE_OBJECTALIGN+3]=(objectAlign>>24)&0xff;

    headbuf[headerStart+PE_OSMAJOR]=osMajor;
    headbuf[headerStart+PE_OSMINOR]=osMinor;

    headbuf[headerStart+PE_SUBSYSMAJOR]=subsysMajor;
    headbuf[headerStart+PE_SUBSYSMINOR]=subsysMinor;

    headbuf[headerStart+PE_SUBSYSTEM]=subSystem&0xff;
    headbuf[headerStart+PE_SUBSYSTEM+1]=(subSystem>>8)&0xff;

    headbuf[headerStart+PE_NUMRVAS]=PE_NUM_VAS&0xff;
    headbuf[headerStart+PE_NUMRVAS+1]=(PE_NUM_VAS>>8)&0xff;

    headbuf[headerStart+PE_HEAPSIZE]=heapSize&0xff;
    headbuf[headerStart+PE_HEAPSIZE+1]=(heapSize>>8)&0xff;
    headbuf[headerStart+PE_HEAPSIZE+2]=(heapSize>>16)&0xff;
    headbuf[headerStart+PE_HEAPSIZE+3]=(heapSize>>24)&0xff;

    headbuf[headerStart+PE_HEAPCOMMSIZE]=heapCommitSize&0xff;
    headbuf[headerStart+PE_HEAPCOMMSIZE+1]=(heapCommitSize>>8)&0xff;
    headbuf[headerStart+PE_HEAPCOMMSIZE+2]=(heapCommitSize>>16)&0xff;
    headbuf[headerStart+PE_HEAPCOMMSIZE+3]=(heapCommitSize>>24)&0xff;

    headbuf[headerStart+PE_STACKSIZE]=stackSize&0xff;
    headbuf[headerStart+PE_STACKSIZE+1]=(stackSize>>8)&0xff;
    headbuf[headerStart+PE_STACKSIZE+2]=(stackSize>>16)&0xff;
    headbuf[headerStart+PE_STACKSIZE+3]=(stackSize>>24)&0xff;

    headbuf[headerStart+PE_STACKCOMMSIZE]=stackCommitSize&0xff;
    headbuf[headerStart+PE_STACKCOMMSIZE+1]=(stackCommitSize>>8)&0xff;
    headbuf[headerStart+PE_STACKCOMMSIZE+2]=(stackCommitSize>>16)&0xff;
    headbuf[headerStart+PE_STACKCOMMSIZE+3]=(stackCommitSize>>24)&0xff;


    /* shift segment start addresses up into place and build section headers */
    sectionStart=headerSize;
    j=headerStart+PE_HEADBUF_SIZE;

    for(i=0,lastSection=-1;i<outcount;i++)
    {
	outlist[i]->filepos=sectionStart;
	outlist[i]->section=-1;
	if(!outlist[i]->length) continue; /* don't output empty sections */
	/* store output section number */
	outlist[i]->section=(j-(headerStart+PE_HEADBUF_SIZE))/PE_OBJECTENTRY_SIZE+1;

        nameIndex=outlist[i]->nameindex;
        if(nameIndex>=0)
        {
            for(k=0;(k<strlen(namelist[nameIndex])) && (k<8);k++)
            {
                headbuf[j+k]=namelist[nameIndex][k];
            }
        }
	/* set virtual size of previous section */
	if(lastSection>=0)
	{
	    /* virtual size is base of this - base of previous */
	    /* set up a final reloc pair for RVAs of sections */
	    simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+2)*sizeof(FIXUPREC));
	    /* use all 4 bytes for a 32-bit offset */
	    /* flag RVA, and subtract for previous seg */
	    simpleRelocs[simpleRelocCount].mask=FIXUP_SUB | FIXUP_RVA | 0xf;
	    simpleRelocs[simpleRelocCount].targetseg=outlist[lastSection]->segnum;
	    simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	    simpleRelocs[simpleRelocCount].ofs=j+8-PE_OBJECTENTRY_SIZE;
	    simpleRelocCount++;

	    /* use all 4 bytes for a 32-bit offset */
	    /* flag RVA for this seg */
	    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	    simpleRelocs[simpleRelocCount].targetseg=outlist[i]->segnum;
	    simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	    simpleRelocs[simpleRelocCount].ofs=j+8-PE_OBJECTENTRY_SIZE;
	    simpleRelocCount++;
	}
	lastSection=i;

        if(!padsegments) /* if not padding segments, reduce space consumption */
        {
            for(k=outlist[i]->length-1;(k>=0)&&!GetNbit(outlist[i]->datmask,k);k--);
            k++; /* k=initialised length */
        }
        headbuf[j+16]=(k)&0xff; /* store initialised data size */
        headbuf[j+17]=(k>>8)&0xff;
        headbuf[j+18]=(k>>16)&0xff;
        headbuf[j+19]=(k>>24)&0xff;

        if(!k)
        {
            /* if no initialised data, zero section start */
            headbuf[j+20]=headbuf[j+21]=headbuf[j+22]=headbuf[j+23]=0;
        }
	else
	{
	    headbuf[j+20]=(sectionStart)&0xff; /* store physical file offset */
	    headbuf[j+21]=(sectionStart>>8)&0xff;
	    headbuf[j+22]=(sectionStart>>16)&0xff;
	    headbuf[j+23]=(sectionStart>>24)&0xff;
	}

        k+=fileAlign-1;
        k&=(0xffffffff-(fileAlign-1)); /* aligned initialised length */

        sectionStart+=k; /* update section start address for next section */

	/* set up reloc for RVA of section */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=outlist[i]->segnum;
	simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	simpleRelocs[simpleRelocCount].ofs=j+12;
	simpleRelocCount++;

        k=(outlist[i]->winFlags ^ WINF_NEG_FLAGS) & WINF_IMAGE_FLAGS; /* get characteristice for section */
        headbuf[j+36]=(k)&0xff; /* store characteristics */
        headbuf[j+37]=(k>>8)&0xff;
        headbuf[j+38]=(k>>16)&0xff;
        headbuf[j+39]=(k>>24)&0xff;
	j+=PE_OBJECTENTRY_SIZE;
    }

    headerSize=j;

    headbuf[headerStart+PE_HEADERSIZE]=headerSize&0xff;
    headbuf[headerStart+PE_HEADERSIZE+1]=(headerSize>>8)&0xff;
    headbuf[headerStart+PE_HEADERSIZE+2]=(headerSize>>16)&0xff;
    headbuf[headerStart+PE_HEADERSIZE+3]=(headerSize>>24)&0xff;

    if(lastSection>=0)
    {
	j-=PE_OBJECTENTRY_SIZE;
	k=outlist[lastSection]->length;
        headbuf[j+8]=(k)&0xff; /* store length as virtual size of last section */
        headbuf[j+9]=(k>>8)&0xff;
        headbuf[j+10]=(k>>16)&0xff;
        headbuf[j+11]=(k>>24)&0xff;
	k=outlist[lastSection]->section;
	headbuf[headerStart+PE_NUMOBJECTS]=k&0xff;       /* store number of sections */
	headbuf[headerStart+PE_NUMOBJECTS+1]=(k>>8)&0xff;
	/* get length of last section */
	i=outlist[lastSection]->length;

	/* this is the image size, when we add the base of that section */
	headbuf[headerStart+PE_IMAGESIZE]=i&0xff;
	headbuf[headerStart+PE_IMAGESIZE+1]=(i>>8)&0xff;
	headbuf[headerStart+PE_IMAGESIZE+2]=(i>>16)&0xff;
	headbuf[headerStart+PE_IMAGESIZE+3]=(i>>24)&0xff;

	/* RVA reloc for image size */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=outlist[lastSection]->segnum;
	simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	simpleRelocs[simpleRelocCount].ofs=headerStart+PE_IMAGESIZE;
	simpleRelocCount++;
    }
    else
    {
	/* no sections, so say so */
	headbuf[headerStart+PE_NUMOBJECTS]=0;
	headbuf[headerStart+PE_NUMOBJECTS+1]=0;

	/* The image size is the header size if no sections */
	headbuf[headerStart+PE_IMAGESIZE]=headerSize&0xff;
	headbuf[headerStart+PE_IMAGESIZE+1]=(headerSize>>8)&0xff;
	headbuf[headerStart+PE_IMAGESIZE+2]=(headerSize>>16)&0xff;
	headbuf[headerStart+PE_IMAGESIZE+3]=(headerSize>>24)&0xff;
    }

    if(errcount)
    {
        exit(1);
    }

    /* get start address */
    if(gotstart)
    {
        GetFixupTarget(&startaddr);
        if(errcount)
        {
            printf("Invalid start address record\n");
            exit(1);
        }
        i=startaddr.disp;
        headbuf[headerStart+PE_ENTRYPOINT]=i&0xff;
        headbuf[headerStart+PE_ENTRYPOINT+1]=(i>>8)&0xff;
        headbuf[headerStart+PE_ENTRYPOINT+2]=(i>>16)&0xff;
        headbuf[headerStart+PE_ENTRYPOINT+3]=(i>>24)&0xff;
        if(startaddr.target>=0)
        {
	    /* RVA reloc for start address */
	    simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	    /* use all 4 bytes for a 32-bit offset */
	    /* flag RVA */
	    simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	    simpleRelocs[simpleRelocCount].targetseg=startaddr.target;
	    simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	    simpleRelocs[simpleRelocCount].ofs=headerStart+PE_ENTRYPOINT;
	    simpleRelocCount++;
        }
        i-=imageBase; /* RVA */
        if(buildDll) /* if library */
        {
            /* flag that entry point should always be called */
            headbuf[headerStart+PE_DLLFLAGS]=0xf;
            headbuf[headerStart+PE_DLLFLAGS+1]=0;
        }
    }
    else
    {
        printf("Warning, no entry point specified\n");
    }

    printf("Got entry point\n");

    if(importSectNum>=0) /* if imports, add section entry */
    {
	/* RVA reloc for import list */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=importSectNum;
	simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	simpleRelocs[simpleRelocCount].ofs=headerStart+PE_IMPORTRVA;
	simpleRelocCount++;

        headbuf[headerStart+PE_IMPORTSIZE]=(seglist[importSectNum]->length)&0xff;
        headbuf[headerStart+PE_IMPORTSIZE+1]=(seglist[importSectNum]->length>>8)&0xff;
        headbuf[headerStart+PE_IMPORTSIZE+2]=(seglist[importSectNum]->length>>16)&0xff;
        headbuf[headerStart+PE_IMPORTSIZE+3]=(seglist[importSectNum]->length>>24)&0xff;
    }
    printf("done imports\n");
    
    if(relocSectNum>=0) /* if relocs, add section entry */
    {
	/* RVA reloc for reloc list */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=relocSectNum;
	simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	simpleRelocs[simpleRelocCount].ofs=headerStart+PE_FIXUPRVA;
	simpleRelocCount++;

        headbuf[headerStart+PE_FIXUPSIZE]=(seglist[relocSectNum]->length)&0xff;
        headbuf[headerStart+PE_FIXUPSIZE+1]=(seglist[relocSectNum]->length>>8)&0xff;
        headbuf[headerStart+PE_FIXUPSIZE+2]=(seglist[relocSectNum]->length>>16)&0xff;
        headbuf[headerStart+PE_FIXUPSIZE+3]=(seglist[relocSectNum]->length>>24)&0xff;
    }

    printf("done relocs\n");

    if(exportSectNum>=0) /* if relocs, add section entry */
    {
	/* RVA reloc for export list */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=exportSectNum;
	simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	simpleRelocs[simpleRelocCount].ofs=headerStart+PE_EXPORTRVA;
	simpleRelocCount++;

        headbuf[headerStart+PE_EXPORTSIZE]=(seglist[exportSectNum]->length)&0xff;
        headbuf[headerStart+PE_EXPORTSIZE+1]=(seglist[exportSectNum]->length>>8)&0xff;
        headbuf[headerStart+PE_EXPORTSIZE+2]=(seglist[exportSectNum]->length>>16)&0xff;
        headbuf[headerStart+PE_EXPORTSIZE+3]=(seglist[exportSectNum]->length>>24)&0xff;
    }

    if(resourceSectNum>=0) /* if relocs, add section entry */
    {
	/* RVA reloc for resource list */
	simpleRelocs = (PFIXUPREC) checkRealloc(simpleRelocs,(simpleRelocCount+1)*sizeof(FIXUPREC));
	/* use all 4 bytes for a 32-bit offset */
	/* flag RVA */
	simpleRelocs[simpleRelocCount].mask=FIXUP_RVA | 0xf;
	simpleRelocs[simpleRelocCount].targetseg=resourceSectNum;
	simpleRelocs[simpleRelocCount].segnum=headerSectNum;
	simpleRelocs[simpleRelocCount].ofs=headerStart+PE_RESOURCERVA;
	simpleRelocCount++;

        headbuf[headerStart+PE_RESOURCESIZE]=(seglist[resourceSectNum]->length)&0xff;
        headbuf[headerStart+PE_RESOURCESIZE+1]=(seglist[resourceSectNum]->length>>8)&0xff;
        headbuf[headerStart+PE_RESOURCESIZE+2]=(seglist[resourceSectNum]->length>>16)&0xff;
        headbuf[headerStart+PE_RESOURCESIZE+3]=(seglist[resourceSectNum]->length>>24)&0xff;
    }

    printf("Finishing off header\n");

    headbuf[headerStart+PE_CODEBASE]=codeBase&0xff;
    headbuf[headerStart+PE_CODEBASE+1]=(codeBase>>8)&0xff;
    headbuf[headerStart+PE_CODEBASE+2]=(codeBase>>16)&0xff;
    headbuf[headerStart+PE_CODEBASE+3]=(codeBase>>24)&0xff;

    headbuf[headerStart+PE_DATABASE]=dataBase&0xff;
    headbuf[headerStart+PE_DATABASE+1]=(dataBase>>8)&0xff;
    headbuf[headerStart+PE_DATABASE+2]=(dataBase>>16)&0xff;
    headbuf[headerStart+PE_DATABASE+3]=(dataBase>>24)&0xff;

    headbuf[headerStart+PE_CODESIZE]=codeSize&0xff;
    headbuf[headerStart+PE_CODESIZE+1]=(codeSize>>8)&0xff;
    headbuf[headerStart+PE_CODESIZE+2]=(codeSize>>16)&0xff;
    headbuf[headerStart+PE_CODESIZE+3]=(codeSize>>24)&0xff;

    headbuf[headerStart+PE_INITDATASIZE]=dataSize&0xff;
    headbuf[headerStart+PE_INITDATASIZE+1]=(dataSize>>8)&0xff;
    headbuf[headerStart+PE_INITDATASIZE+2]=(dataSize>>16)&0xff;
    headbuf[headerStart+PE_INITDATASIZE+3]=(dataSize>>24)&0xff;

    /* zero out section start for all zero-length segments */
    j=headerStart+PE_HEADBUF_SIZE;
    for(i=0;i<outcount;i++,j+=PE_OBJECTENTRY_SIZE)
    {
        /* check if size in file is zero */
        k=headbuf[j+16]|headbuf[j+17]|headbuf[j+18]|headbuf[j+19];
        if(!k)
        {
            /* if so, zero section start */
            headbuf[j+20]=headbuf[j+21]=headbuf[j+22]=headbuf[j+23]=0;
        }
    }

    if(errcount!=0)
    {
        exit(1);
    }

    /* make header the first section in the output list */
    outcount++;
    outlist=(PPSEG)checkRealloc(outlist,outcount*sizeof(PSEG));
    memmove(outlist+1,outlist,(outcount-1)*sizeof(PSEG));
    outlist[0]=seglist[headerSectNum];
    outlist[0]->filepos=0;
}

void buildPEFile(void)
{
    printf("Generating PE file\n",outname);

    relocSectNum=-1;
    importSectNum=-1;
    exportSectNum=-1;
    resourceSectNum=-1;

    buildPEImports();
    buildPEExports();
    buildPEResources();

    doFlatRelocs();

    buildPERelocs();

    sortSegments();

    buildPEHeader();

    alignSegments();
}
