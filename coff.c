#include "alink.h"
#include "coff.h"

static BOOL loadCoffImport(FILE *objfile);

static BOOL loadcoff(PFILE objfile,PMODULE mod,BOOL isDjgpp)
{
    PPEXTREF externs=NULL;
    UINT extcount=0;

    PSYMBOL pubdef;
    PPSYMBOL publics=NULL;
    UINT pubcount=0;
    PPSYMBOL locals=NULL;
    UINT localcount=0;

    UINT localExtRefCount=0,globalExtRefCount=0;

    UCHAR headbuf[COFF_BASE_HEADER_SIZE];
    UCHAR buf[100];
    PUCHAR bigbuf;
    PUCHAR stringList;
    PUCHAR symbolMem;
    PUCHAR symPtr;
    UINT thiscpu;
    UINT numSect;
    UINT headerSize;
    UINT symbolPtr;
    UINT numSymbols;
    UINT stringPtr;
    UINT stringSize;
    UINT stringOfs;
    UINT i,j,k;
    UINT fileStart;
    UINT numrel;
    UINT *relofs=NULL;
    UINT *relshift=NULL;
    UINT *numlines=NULL;
    UINT *lineofs=NULL;
    PUCHAR lineptr;
    PCHAR sectname;
    PCHAR sectorder;
    PCOFFSYM sym=NULL;
    UINT combineType,linkwith;
    PCHAR comdatsym;
    PCOMDATREC comdat;
    PPSYMBOL comdatList=NULL;
    UINT comdatCount=0;
    PPSEG seglist=NULL;
    PSEG thisSect;
    UINT winFlags;
    UINT align;
    UINT base;
    PDATABLOCK data;
    SEG comdatParent={"",NULL,NULL,NULL,0,0,1,0,FALSE,FALSE,FALSE,TRUE,FALSE,
		      FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,
		      FALSE,FALSE,FALSE,FALSE,FALSE,0,0,0,NULL,NULL,0,NULL,
		      0,NULL};
    INT currentSource=-1;
    UINT linenum;
    PSEG lineSect;
    INT lineSource;
    UINT baseOffset;
    UINT baseSym;
    UINT baseNum;
    UINT baseLines;

    fileStart=ftell(objfile);

    if(fread(headbuf,1,COFF_BASE_HEADER_SIZE,objfile)!=COFF_BASE_HEADER_SIZE)
    {
        addError("Unable to read from file %s",mod->file);
        return FALSE;
    }
    thiscpu=headbuf[COFF_MACHINEID]+256*headbuf[COFF_MACHINEID+1];
    if(!thiscpu)
    {
	/* if we've got an import module, start at the beginning */
	fseek(objfile,fileStart,SEEK_SET);
	/* and load it */
	return loadCoffImport(objfile);
    }
    
    if((thiscpu<0x14c) || (thiscpu>0x14e))
    {
        addError("Unsupported CPU type for module in %s",mod->file);
        return FALSE;
    }
    numSect=headbuf[COFF_NUMOBJECTS]+256*headbuf[COFF_NUMOBJECTS+1];

    symbolPtr=headbuf[COFF_SYMBOLPTR]+(headbuf[COFF_SYMBOLPTR+1]<<8)+
        (headbuf[COFF_SYMBOLPTR+2]<<16)+(headbuf[COFF_SYMBOLPTR+3]<<24);

    numSymbols=headbuf[COFF_NUMSYMBOLS]+(headbuf[COFF_NUMSYMBOLS+1]<<8)+
        (headbuf[COFF_NUMSYMBOLS+2]<<16)+(headbuf[COFF_NUMSYMBOLS+3]<<24);

    if(headbuf[COFF_HDRSIZE]|headbuf[COFF_HDRSIZE+1])
    {
        diagnostic(DIAG_VERBOSE,"warning, optional header discarded");
	headerSize=headbuf[COFF_HDRSIZE]+256*headbuf[COFF_HDRSIZE+1];
    }
    else
	headerSize=0;
    headerSize+=COFF_BASE_HEADER_SIZE;

    if(symbolPtr && numSymbols)
    {
	symbolMem=checkMalloc(numSymbols*COFF_SYMBOL_SIZE);
	fseek(objfile,fileStart+symbolPtr,SEEK_SET);
        if(fread(symbolMem,COFF_SYMBOL_SIZE,numSymbols,objfile)!=numSymbols)
        {
            addError("Unable to read COFF symbol table for %s",mod->file);
            return FALSE;
        }

	stringPtr=0;
	for(i=0,symPtr=symbolMem;i<numSymbols;++i,symPtr+=COFF_SYMBOL_SIZE)
	{
	    /* we only need a string table if there is a reference to it in the symbol table */
	    if(!symPtr[0] && !symPtr[1] && !symPtr[2] && !symPtr[3])
	    {
		stringPtr=symbolPtr+numSymbols*COFF_SYMBOL_SIZE;
		break;
	    }
	    i+=symPtr[COFF_SYMBOL_NUMAUX];
	    symPtr+=symPtr[COFF_SYMBOL_NUMAUX]*COFF_SYMBOL_SIZE;
	}

	/* if we need a string table, load it */
	if(stringPtr)
	{
	    fseek(objfile,fileStart+stringPtr,SEEK_SET);
	    if(fread(buf,1,4,objfile)!=4)
	    {
		addError("Unable to read COFF string table size for %s",mod->file);
		return FALSE;;
	    }
	    stringSize=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
	    if(!stringSize) stringSize=4;
	    if(stringSize<4)
	    {
		addError("Bad COFF string table size %li for %s",stringSize,mod->file);
		return FALSE;
	    }
	    stringPtr+=4;
	    stringSize-=4;
	}
	else
	{
	    stringSize=0;
	}
    }
    else
    {
	symbolMem=NULL;
	stringSize=0;
    }
    
    if(stringSize)
    {
        stringList=(PUCHAR)checkMalloc(stringSize);
        if(fread(stringList,1,stringSize,objfile)!=stringSize)
        {
            addError("Unable to read COFF string table for %s",mod->file);
            return FALSE;
        }
        if(stringList[stringSize-1])
        {
            addError("Invalid COFF string table in %s, last string unterminated",mod->file);
            return FALSE;
        }
    }
    else
    {
	stringList=NULL;
    }
    
    if(symbolPtr && numSymbols)
    {
        sym=(PCOFFSYM)checkMalloc(sizeof(COFFSYM)*numSymbols);
        for(i=0,symPtr=symbolMem;i<numSymbols;i++,symPtr+=COFF_SYMBOL_SIZE)
        {
            if(symPtr[COFF_SYMBOL_NAME]|symPtr[COFF_SYMBOL_NAME+1]|symPtr[COFF_SYMBOL_NAME+2]|symPtr[COFF_SYMBOL_NAME+3])
            {
                sym[i].name=(PUCHAR)checkMalloc(9);
                strncpy(sym[i].name,symPtr+COFF_SYMBOL_NAME,8);
                sym[i].name[8]=0;
            }
            else
            {
                stringOfs=symPtr[COFF_SYMBOL_NAME+4]+(symPtr[COFF_SYMBOL_NAME+5]<<8)+(symPtr[COFF_SYMBOL_NAME+6]<<16)+(symPtr[COFF_SYMBOL_NAME+7]<<24);
                if(stringOfs<4)
                {
                    addError("Bad COFF symbol location for %s",mod->file);
                    return FALSE;
                }
                stringOfs-=4;
                if(stringOfs>=stringSize)
                {
                    addError("Bad COFF symbol location for %s",mod->file);
                    return FALSE;
                }
                sym[i].name=checkStrdup(stringList+stringOfs);
            }
	    
            sym[i].value=symPtr[COFF_SYMBOL_VALUE]+(symPtr[COFF_SYMBOL_VALUE+1]<<8)+(symPtr[COFF_SYMBOL_VALUE+2]<<16)+(symPtr[COFF_SYMBOL_VALUE+3]<<24);
            sym[i].section=symPtr[COFF_SYMBOL_SECTION]+(symPtr[COFF_SYMBOL_SECTION+1]<<8);
            sym[i].type=symPtr[COFF_SYMBOL_TYPE]+(symPtr[COFF_SYMBOL_TYPE+1]<<8);
            sym[i].class=symPtr[COFF_SYMBOL_STORAGE];
            sym[i].extnum=-1;
	    sym[i].numAuxRecs=symPtr[COFF_SYMBOL_NUMAUX];
	    sym[i].isComDat=FALSE;

	    if(sym[i].numAuxRecs)
	    {
		sym[i].auxRecs=(PUCHAR)checkMalloc(sym[i].numAuxRecs*COFF_SYMBOL_SIZE);
	    }
	    else
	    {
		sym[i].auxRecs=NULL;
	    }
	    
	    /* read in the auxillary records for this symbol */
            for(j=0;j<sym[i].numAuxRecs;j++)
            {
		memcpy(sym[i].auxRecs+j*COFF_SYMBOL_SIZE,symPtr+(j+1)*COFF_SYMBOL_SIZE,COFF_SYMBOL_SIZE);
		sym[i+j+1].name=NULL;
		sym[i+j+1].numAuxRecs=0;
		sym[i+j+1].value=0;
		sym[i+j+1].section=-1;
		sym[i+j+1].type=0;
		sym[i+j+1].class=0;
		sym[i+j+1].extnum=-1;
		sym[i+j+1].sourceFile=-1;
            }
            switch(sym[i].class)
            {
            case COFF_SYM_FILE:
		/* filename is given in auxillary records */
		/* check if already given for this module */
		for(k=0;k<mod->sourceFileCount;++k)
		{
		    /* skip if so */
		    if(!strncmp(mod->sourceFiles[k],sym[i].auxRecs,sym[i].numAuxRecs*COFF_SYMBOL_SIZE)) break;
		}
		if(k==mod->sourceFileCount)
		{
		    /* add entry if not found */
		    mod->sourceFiles=checkRealloc(mod->sourceFiles,(mod->sourceFileCount+1)*sizeof(PCHAR));
		    /* allocate memory for entry */
		    mod->sourceFiles[k]=checkMalloc(sym[i].numAuxRecs*COFF_SYMBOL_SIZE+1);
		    /* copy in string */
		    memcpy(mod->sourceFiles[k],sym[i].auxRecs,sym[i].numAuxRecs*COFF_SYMBOL_SIZE);
		    /* ensure null-terminated */
		    mod->sourceFiles[k][sym[i].numAuxRecs*COFF_SYMBOL_SIZE]=0;
		    mod->sourceFileCount++;
		}
		currentSource=k;
		break;
	    case COFF_SYM_SECTION: /* section symbol */
	    case COFF_SYM_STATIC: /* allowed, but ignored for now as we only want to process if required */
	    case COFF_SYM_LABEL:
            case COFF_SYM_FUNCTION:
            case COFF_SYM_EXTERNAL:
                break;
            default:
                addError("Unsupported symbol class %02X for symbol %s in %s",sym[i].class,sym[i].name,mod->file);
                return FALSE;
            }
	    sym[i].sourceFile=currentSource;

	    i+=j;
	    symPtr+=j*COFF_SYMBOL_SIZE;
        }
    }

    checkFree(symbolMem);

    if(numSect)
    {
	seglist=checkMalloc(sizeof(PSEG)*numSect);
	relofs=checkMalloc(sizeof(UINT)*numSect);
	relshift=checkMalloc(sizeof(UINT)*numSect);
	numlines=checkMalloc(sizeof(UINT)*numSect);
	lineofs=checkMalloc(sizeof(UINT)*numSect);
    }
    
    for(i=0;i<numSect;i++)
    {
        fseek(objfile,fileStart+headerSize+i*COFF_OBJECTENTRY_SIZE,
	      SEEK_SET);
        if(fread(buf,1,COFF_OBJECTENTRY_SIZE,objfile)!=COFF_OBJECTENTRY_SIZE)
        {
	    addError("Unable to read COFF section header for %s",mod->file);
	    return FALSE;
        }
        /* virtual size is also the offset of the data into the segment */
	/*
	  if(buf[COFF_OBJECT_VIRTSIZE]|buf[COFF_OBJECT_VIRTSIZE+1]|buf[COFF_OBJECT_VIRTSIZE+2]
	  |buf[COFF_OBJECT_VIRTSIZE+3])
	  {
	  addError("Invalid COFF object file, section has non-zero virtual size\n");
	  exit(1);
	  }
	*/
        buf[8]=0; /* null terminate name */
        /* get shift value for relocs */
        relshift[i]=buf[COFF_OBJECT_VIRTADDR] +(buf[COFF_OBJECT_VIRTADDR+1]<<8)+
	    (buf[COFF_OBJECT_VIRTADDR+2]<<16)+(buf[COFF_OBJECT_VIRTADDR+3]<<24);

        if(buf[0]=='/')
        {
            j=strtoul(buf+1,(char**)&bigbuf,10);
            if(*bigbuf)
            {
                addError("Invalid COFF object file %s, invalid number %s",mod->file,buf+1);
                return FALSE;
            }
            if(j<4)
            {
                addError("Invalid COFF object file %s, bad section name offset",mod->file);
                return FALSE;
            }
            j-=4;
            if(j>=stringSize)
            {
                addError("Invalid COFF object file %s, bad section name offset",mod->file);
                return FALSE;
            }
	    sectname=checkStrdup(stringList+j);
        }
        else
        {
            sectname=checkStrdup(buf);
        }
        if((sectorder=strchr(sectname,'$')))
        {
            /* if we have a grouped segment, sort by tag */
            /* and get real name, without $ sort section */
            *(sectorder)=0;
	    sectorder++;
        }

        numrel=buf[COFF_OBJECT_NUMREL]+(buf[COFF_OBJECT_NUMREL+1]<<8);
        relofs[i]=buf[COFF_OBJECT_RELPTR]+(buf[COFF_OBJECT_RELPTR+1]<<8)+
            (buf[COFF_OBJECT_RELPTR+2]<<16) + (buf[COFF_OBJECT_RELPTR+3]<<24);

        numlines[i]=buf[COFF_OBJECT_NUMLINE]+(buf[COFF_OBJECT_NUMLINE+1]<<8);
        lineofs[i]=buf[COFF_OBJECT_LINEPTR]+(buf[COFF_OBJECT_LINEPTR+1]<<8)+
            (buf[COFF_OBJECT_LINEPTR+2]<<16) + (buf[COFF_OBJECT_LINEPTR+3]<<24);

        winFlags=buf[COFF_OBJECT_FLAGS]+(buf[COFF_OBJECT_FLAGS+1]<<8)+
	    (buf[COFF_OBJECT_FLAGS+2]<<16)+(buf[COFF_OBJECT_FLAGS+3]<<24);

        base=buf[COFF_OBJECT_RAWPTR]+(buf[COFF_OBJECT_RAWPTR+1]<<8)+
	    (buf[COFF_OBJECT_RAWPTR+2]<<16)+(buf[COFF_OBJECT_RAWPTR+3]<<24);

	/* nopad is equivalent to align to 1 byte */
	if(winFlags & WINF_ALIGN_NOPAD)
	{
	    winFlags &= (0xffffffff-WINF_ALIGN);
	    winFlags |= WINF_ALIGN_BYTE;
	}
	
        switch(winFlags & WINF_ALIGN)
        {
        case WINF_ALIGN_BYTE:
	    align=1;
	    break;
        case WINF_ALIGN_WORD:
	    align=2;
	    break;
        case WINF_ALIGN_DWORD:
	    align=4;
	    break;
        case WINF_ALIGN_8:
	    align=8;
	    break;
        case WINF_ALIGN_PARA:
	    align=16;
	    break;
        case WINF_ALIGN_32:
	    align=32;
	    break;
        case WINF_ALIGN_64:
	    align=64;
	    break;
        case WINF_ALIGN_128:
	    align=128;
	    break;
        case WINF_ALIGN_256:
	    align=256;
	    break;
        case WINF_ALIGN_512:
	    align=512;
	    break;
        case WINF_ALIGN_1024:
	    align=1024;
	    break;
        case WINF_ALIGN_2048:
	    align=2048;
	    break;
        case WINF_ALIGN_4096:
	    align=4096;
	    break;
        case WINF_ALIGN_8192:
	    align=8192;
	    break;
        case 0:
	    align=16; /* default */
	    break;
        default:
	    addError("Invalid COFF object file %s, bad section alignment %08lX",mod->file,winFlags);
	    return FALSE;
        }

	thisSect=createSection(sectname,NULL,sectorder,mod,
			       buf[COFF_OBJECT_RAWSIZE]+(buf[COFF_OBJECT_RAWSIZE+1]<<8)+
			       (buf[COFF_OBJECT_RAWSIZE+2]<<16)+(buf[COFF_OBJECT_RAWSIZE+3]<<24),
			       align);

	seglist[i]=thisSect;
	
        thisSect->combine=SEGF_PUBLIC;
	thisSect->use32=TRUE;
	thisSect->code=(winFlags&WINF_CODE)!=0;
	thisSect->initdata=(winFlags&WINF_INITDATA)!=0;
	thisSect->uninitdata=(winFlags&WINF_UNINITDATA)!=0;
	thisSect->read=(winFlags&WINF_READABLE)!=0;
	thisSect->write=(winFlags&WINF_WRITEABLE)!=0;
	thisSect->execute=(winFlags&WINF_EXECUTE)!=0;
	thisSect->discardable=(winFlags&WINF_DISCARDABLE)!=0;
	thisSect->shared=(winFlags&WINF_SHARED)!=0;
	thisSect->discard=(winFlags&(WINF_REMOVE | WINF_COMMENT))!=0;

	if(isDjgpp)
	{
	    thisSect->read=thisSect->code||thisSect->initdata||thisSect->uninitdata;
	    thisSect->write=thisSect->initdata||thisSect->uninitdata;
	    thisSect->execute=thisSect->code;
	}
	
	/* remove .debug sections */
	if(!stricmp(sectname,".debug"))
	{
	    thisSect->discard=TRUE;
	    thisSect->length=0;
	    numrel=0;
	    numlines[i]=0;
	}

	if(winFlags & WINF_COMDAT)
	{
	    combineType=0;
	    linkwith=0;
	    for(j=0;j<numSymbols;j++)
	    {
		if(!sym[j].name) continue;
		if(sym[j].section==(i+1))
		{
		    if(sym[j].numAuxRecs!=1)
		    {
			addError("Invalid COFF COMDAT section reference %s in %s",sym[j].name,mod->file);
			return FALSE;
		    }
		    combineType=sym[j].auxRecs[14];
		    linkwith=sym[j].auxRecs[12]+(sym[j].auxRecs[13]<<8)-1;
		    break;
		}
	    }
	    if(j==numSymbols)
	    {
		addError("Invalid COFF COMDAT section %s - no symbol, in %s",sectname,mod->file);
		return FALSE;
	    }
	    for(j++;j<numSymbols;j++)
	    {
		if(!sym[j].name) continue;
		if(sym[j].section==(i+1))
		{
		    comdatsym=sym[j].name;
		    sym[j].isComDat=TRUE;
		    break;
		}
	    }
	    /* associative sections don't have a name */
	    if(j==numSymbols)
	    {
		if(combineType!=5)
		{
		    addError("Invalid COFF COMDAT section %s - no name symbol, in %s",sectname,mod->file);
		    return FALSE;
		}
	    }
	    if(combineType==5)
	    {
		if(linkwith>i)
		{
		    addError("COFF COMDAT %s set to link with unloaded section %li, in %s",comdatsym?comdatsym:sectname,linkwith,mod->file);
		    return FALSE;
		}
		for(j=0;j<comdatCount;++j)
		{
		    comdat=comdatList[j]->comdatList[0];
		    for(k=0;k<comdat->segCount;++k)
		    {
			if(comdat->segList[k]==seglist[linkwith])
			{
			    comdat->segList=checkRealloc(comdat->segList,
							 (comdat->segCount+1)*sizeof(PSEG));
			    comdat->segList[comdat->segCount]=thisSect;
			    comdat->segCount++;
			    break;
			}
		    }
		    if(k!=comdat->segCount) break;
		}
		if(j==comdatCount)
		{
		    /* section set to link with non-COMDAT section, convert to normal section? */
		    addError("COFF COMDAT %s set to link with non-COMDAT section %li, in %s",comdatsym?comdatsym:sectname,linkwith,mod->file);
		    return FALSE;
		}
		thisSect->parent=&comdatParent;
	    }
	    else
	    {
		comdat=(PCOMDATREC)checkMalloc(sizeof(COMDATREC));
		switch(combineType)
		{
		case 1:
		    comdat->combine=COMDAT_UNIQUE;
		    break;
		case 2:
		    comdat->combine=COMDAT_ANY;
		    break;
		case 3:
		    comdat->combine=COMDAT_SAMESIZE;
		    break;
		case 4:
		    comdat->combine=COMDAT_EXACT;
		    break;
		case 6:
		    comdat->combine=COMDAT_LARGEST;
		    break;
		default:
		    addError("Unsupported COFF COMDAT combine type %li for %s, in %s",combineType,comdatsym,mod->file);
		    return FALSE;
		}
		comdat->segCount=1;
		comdat->segList=checkMalloc(sizeof(PSEG));
		comdat->segList[0]=thisSect;
		comdatList=checkRealloc(comdatList,(comdatCount+1)*sizeof(PSYMBOL));
		comdatList[comdatCount]=createSymbol(comdatsym,PUB_COMDAT,mod,comdat);
		comdatCount++;
		thisSect->parent=&comdatParent;
	    }
	}

        if(thisSect->length)
        {
            if(base)
            {
		data=createDataBlock(NULL,0,thisSect->length,1);
	    
                fseek(objfile,fileStart+base,SEEK_SET);
                if(fread(data->data,1,thisSect->length,objfile)
		   !=thisSect->length)
                {
		    addError("Invalid COFF object file %s, unable to read section data for %s",mod->file,sectname);
		    return FALSE;
                }
		addFixedData(thisSect,data);
            }
        }

	if(numrel)
	{
	    thisSect->relocCount=numrel;
	    thisSect->relocs=checkMalloc(numrel*sizeof(RELOC));
	}
    }
    
    for(i=0;i<numSect;++i)
    {
	if(!numlines[i]) continue;

	fseek(objfile,fileStart+lineofs[i],SEEK_SET);
	lineptr=checkMalloc(numlines[i]*6);
	if(fread(lineptr,1,numlines[i]*6,objfile)!=(numlines[i]*6))
	{
	    addError("Error reading from COFF object file %s",mod->file);
	    return FALSE;
	}

	lineSource=-1;
	baseSym=UINT_MAX;
	baseNum=0;
	baseOffset=0;
	baseLines=0;
	
	for(j=0;j<numlines[i];++j)
	{
	    linenum=lineptr[j*6+4]+(lineptr[j*6+5]<<8);
	    k=lineptr[j*6]+(lineptr[j*6+1]<<8)+(lineptr[j*6+2]<<16)+(lineptr[j*6+3]<<24);
	    if(!linenum)
	    {
		if((k>numSymbols) || 
		   ((sym[k].class!=COFF_SYM_EXTERNAL) 
		   && (sym[k].class!=COFF_SYM_STATIC))
		   || (sym[k].numAuxRecs<1))
		{
		    addError("Invalid line number base %li, in COFF object file %s",k,mod->file);
		    return FALSE;
		}
		
		baseSym=sym[k].auxRecs[0]+(sym[k].auxRecs[1]<<8)+(sym[k].auxRecs[2]<<16)+(sym[k].auxRecs[3]<<24);
		baseOffset=sym[k].value;
		
		if(baseSym>numSymbols)
		{
		    addError("Missing line number base %li, in COFF file %s",baseSym,mod->file);
		    return FALSE;
		}
		k=sym[baseSym].section;
		if((k==0)|| (k>numSect))
		{
		    addError("Bad base section %li for COFF line number, in %s",k,mod->file);
		    return FALSE;
		}
		else
		{
		    lineSect=seglist[k-1];
		}
		lineSource=sym[baseSym].sourceFile+1;
		if(sym[baseSym].numAuxRecs<1)
		{
		    addError("Bad base symbol %li, for COFF line number in %s",baseSym,mod->file);
		    return FALSE;
		}
		
		baseNum=sym[baseSym].auxRecs[4]+(sym[baseSym].auxRecs[5]<<8);

		lineSect->lines=checkRealloc(lineSect->lines,(lineSect->lineCount+1)*sizeof(LINENUM));
		lineSect->lines[lineSect->lineCount].sourceFile=lineSource;
		lineSect->lines[lineSect->lineCount].num=baseNum;
		lineSect->lines[lineSect->lineCount].offset=baseOffset;
		lineSect->lineCount++;
		baseLines=0;
		/* next symbol */
		baseSym+=sym[baseSym].numAuxRecs+1;
		if(sym[baseSym].class!=COFF_SYM_FUNCTION)
		{
		    addError("Invalid COFF line num symbol - entry %li not function, in %s",baseSym,mod->file);
		    return FALSE;
		}
		if(!strcmp(sym[baseSym].name,".lf"))
		{
		    if((lineSource!=(sym[baseSym].sourceFile+1))
		       ||(lineSect!=seglist[sym[baseSym].section-1]))
		    {
			addError("Invalid COFF line number data - lf record gives wrong source file/section, in %s",mod->file);
			return FALSE;
		    }
		    baseLines=sym[baseSym].value;
		}
		else
		{
		    addError("Invalid COFF line num symbols - entry %li not .lf (%s), in %s",baseSym,sym[baseSym].name,mod->file);
		    return FALSE;
		}
		baseLines--;
	    }
	    else
	    {
		if(baseSym!=UINT_MAX)
		{
		    /* if no lines left in current block, read next block */
		    if(!baseLines)
		    {
			/* next symbol */
			baseSym+=sym[baseSym].numAuxRecs+1;
			while(sym[baseSym].class==COFF_SYM_FILE)
			{
			    baseSym+=sym[baseSym].numAuxRecs+1;
			}
			
			if(sym[baseSym].class!=COFF_SYM_FUNCTION)
			{
			    addError("Invalid COFF line num symbols - entry %li not function, in %s",baseSym,mod->file);
			    return FALSE;
			}
			/* end of function?*/
			if(!strcmp(sym[baseSym].name,".ef"))
			{
			    baseSym=UINT_MAX;
			    lineSource=-1;
			    baseNum=0;
			    baseOffset=0;
			}
			else if(!strcmp(sym[baseSym].name,".lf"))
			{
			    lineSource=sym[baseSym].sourceFile+1;
			    lineSect=seglist[sym[baseSym].section-1];
			    baseLines=sym[baseSym].value;
			}
			else
			{
			    addError("Invalid COFF line num symbols - entry %li not .lf (%s), in %s",baseSym,sym[baseSym].name,mod->file);
			    return FALSE;
			}
		    }
		}
		if(baseSym!=UINT_MAX)
		{
		    /* get 16-bit line number */
		    linenum+=baseNum;
		    linenum&=0xffff;
		    baseLines--;
		    lineSect->lines=checkRealloc(lineSect->lines,(lineSect->lineCount+1)*sizeof(LINENUM));
		    lineSect->lines[lineSect->lineCount].sourceFile=lineSource;
		    lineSect->lines[lineSect->lineCount].num=linenum;
		    lineSect->lines[lineSect->lineCount].offset=k;
		    lineSect->lineCount++;
		}
	    }
	}
	checkFree(lineptr);
    }
    
    for(i=0;i<numSect;++i)
    {
	if(!seglist[i]->relocCount) continue; /* skip seg if no relocs */
	thisSect=seglist[i];
	
        fseek(objfile,fileStart+relofs[i],SEEK_SET);
        for(j=0;j<thisSect->relocCount;++j)
        {
	    if(fread(buf,1,COFF_RELOC_SIZE,objfile)!=COFF_RELOC_SIZE)
	    {
		addError("Invalid COFF object file %s, unable to read reloc table",mod->file);
		return FALSE;
	    }
	    /* get address to relocate */
	    thisSect->relocs[j].ofs=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
	    thisSect->relocs[j].ofs-=relshift[i];
	    /* get segment */
	    thisSect->relocs[j].disp=0;
	    thisSect->relocs[j].tseg=NULL;
	    /* frame is current segment (only relevant for non-FLAT output) */
	    thisSect->relocs[j].fseg=thisSect;
	    thisSect->relocs[j].fext=NULL;
	    thisSect->relocs[j].text=NULL;
	    thisSect->relocs[j].base=REL_ABS;
	    /* get relocation target external index */
	    k=buf[4]+(buf[5]<<8)+(buf[6]<<16)+(buf[7]<<24);
	    if(k>=numSymbols)
	    {
		addError("Invalid COFF object file %s, undefined symbol %li",mod->file,k);
		return FALSE;
	    }
	    /* assume external reloc */
	    switch(sym[k].class)
	    {
	    case COFF_SYM_SECTION:
		if(sym[k].section<-1)
		{
		    addError("Cannot create a fixup against a debug information, in COFF object file %s",mod->file);
		    return FALSE;
		}
		/* external section? */
		if(sym[k].section==0)
		{
		    /* if so, define an extern */
		    if(sym[k].extnum<0)
		    {
			externs=(PPEXTREF)checkRealloc(externs,(extcount+1)*sizeof(PEXTREF));
			externs[extcount]->typenum=-1;
			externs[extcount]->local=FALSE;
			externs[extcount]->name=sym[k].name;
			externs[extcount]->pubdef=NULL;
			externs[extcount]->mod=mod;
			sym[k].extnum=extcount;
			extcount++;
		    }
		    thisSect->relocs[j].text=externs[sym[k].extnum];
		}
		else
		{
		    /* section declared in this module, so reference section directly */
		    thisSect->relocs[j].tseg=seglist[sym[k].section-1];
		}
		break;
	    case COFF_SYM_EXTERNAL:
		/* global symbols require an extern when used in relocs */
		/* cannot reference symbol directly, as may be overridden */
		if(sym[k].extnum<0)
		{
		    externs=(PPEXTREF)checkRealloc(externs,(extcount+1)*sizeof(PEXTREF));
		    externs[extcount]=(PEXTREF)checkMalloc(sizeof(EXTREF));
		    externs[extcount]->typenum=-1;
		    externs[extcount]->local=FALSE;
		    externs[extcount]->name=sym[k].name;
		    externs[extcount]->pubdef=NULL;
		    externs[extcount]->mod=mod;
		    sym[k].extnum=extcount;
		    extcount++;
		}
		thisSect->relocs[j].text=externs[sym[k].extnum];
		/* they may also include a COMDEF or a PUBDEF */
		/* this is dealt with after all sections loaded, to cater for COMDAT symbols */
		break;
	    case COFF_SYM_STATIC: /* static symbol */
	    case COFF_SYM_LABEL: /* code label symbol */
		if(sym[k].section<-1)
		{
		    addError("cannot relocate against a debug info symbol, in COFF object file %s",mod->file);
		    return FALSE;
		    break;
		}
		if(sym[k].section==0)
		{
		    /* static reference to a COMDEF? */
		    if(sym[k].value)
		    {
			/* yes, so build an extern */
			if(sym[k].extnum<0)
			{
			    /* build symbol too, since local */
			    externs=(PPEXTREF)checkRealloc(externs,(extcount+1)*sizeof(PEXTREF));
			    externs[extcount]=(PEXTREF)checkMalloc(sizeof(EXTREF));
			    externs[extcount]->typenum=-1;
			    externs[extcount]->local=TRUE;
			    externs[extcount]->name=sym[k].name;
			    pubdef=createSymbol(checkStrdup(sym[k].name),PUB_COMDEF,mod,sym[k].value,FALSE);
			    locals=checkRealloc(locals,sizeof(PSYMBOL)*(localcount+1));
			    locals[localcount]=pubdef;
			    localcount++;
			    externs[extcount]->pubdef=pubdef;
			    pubdef->refCount++;
			    externs[extcount]->mod=mod;
			    sym[k].extnum=extcount;
			    extcount++;
			}
			thisSect->relocs[j].text=externs[sym[k].extnum];
		    }
		    else
		    {
			/* no, undefined symbol then */
			addError("Undefined symbol %s, in COFF object file %s",sym[k].name,mod->file);
			return FALSE;
		    }
		}
		else
		{
		    /* update relocation information to reflect symbol */
		    thisSect->relocs[j].disp=sym[k].value;
		    if(sym[k].section==-1)
		    {
			/* absolute symbols have their own section */
			thisSect->relocs[j].tseg=absoluteSegment;
		    }
		    else
		    {
			/* else get real number of section */
			thisSect->relocs[j].tseg=seglist[sym[k].section-1];
		    }
		}
		break;
	    default:
		addError("undefined symbol class 0x%02X for symbol %s, in COFF object file %s",sym[k].class,sym[k].name,mod->file);
		return FALSE;
	    }
	    
	    /* set relocation type */
	    switch(buf[8]+(buf[9]<<8))
	    {
	    case COFF_FIX_DIR32:
		thisSect->relocs[j].rtype=REL_OFS32;
		thisSect->relocs[j].base=REL_DEFAULT;
		break;
	    case COFF_FIX_RVA32:
		thisSect->relocs[j].rtype=REL_OFS32;
		thisSect->relocs[j].base=REL_RVA;
		break;
	    case COFF_FIX_SECTION:
		thisSect->relocs[j].rtype=REL_SEG;
		thisSect->relocs[j].base=REL_DEFAULT;
		break;
	    case COFF_FIX_SECREL:
		/* get offset of target within its own section */
		thisSect->relocs[j].rtype=REL_OFS32;
		thisSect->relocs[j].base=REL_FRAME;
		thisSect->relocs[j].fext=thisSect->relocs[j].text;
		thisSect->relocs[j].fseg=thisSect->relocs[j].tseg;
		break;
	    case COFF_FIX_REL32:
		thisSect->relocs[j].rtype=REL_OFS32;
		thisSect->relocs[j].base=REL_SELF;
		if(isDjgpp)
		{
		    /* DJGPP offsets are already shifted, so shift displacement to compensate */
		    thisSect->relocs[j].disp+=thisSect->relocs[j].ofs+relshift[i]; 
		}
		else
		{
		    thisSect->relocs[j].disp-=4; /* for MS-COFF, shift by 4 to get correct address */
		}
		break;
	    default:
		addError("unsupported COFF relocation type %04X in %s",buf[8]+(buf[9]<<8),mod->file);
		return FALSE;
	    }
        }
    }
    /* build PUBDEFs or COMDEFs for external symbols defined here that aren't COMDAT symbols. */
    for(i=0;i<numSymbols;i++)
    {
	switch(sym[i].class)
	{
	case COFF_SYM_SECTION:
	    if(sym[i].section<-1)
	    {
		break;
	    }
	    if(sym[i].section!=0) /* if the section is defined here, make public */
	    {
		pubdef=createSymbol(checkStrdup(sym[i].name),PUB_PUBLIC,mod,
				    seglist[sym[i].section-1],sym[i].value,
				    -1,-1);
		publics=checkRealloc(publics,sizeof(PSYMBOL)*(pubcount+1));
		publics[pubcount]=pubdef;
		pubcount++;
	    }
	    break;
	case COFF_SYM_EXTERNAL:
	    if(sym[i].isComDat) continue;
	    if(sym[i].section<-1)
	    {
		break;
	    }
	    if(sym[i].section==-1) /* absolute address */
	    {
		pubdef=createSymbol(checkStrdup(sym[i].name),PUB_PUBLIC,mod,
				    absoluteSegment,
				    sym[i].value,
				    -1,-1);
		publics=checkRealloc(publics,sizeof(PSYMBOL)*(pubcount+1));
		publics[pubcount]=pubdef;
		pubcount++;
	    }
	    else if(sym[i].section==0)
	    {
		if(sym[i].value)
		{
		    pubdef=createSymbol(checkStrdup(sym[i].name),PUB_COMDEF,mod,sym[i].value,FALSE);
		    publics=checkRealloc(publics,sizeof(PSYMBOL)*(pubcount+1));
		    publics[pubcount]=pubdef;
		    pubcount++;
		}
	    }
	    else
	    {
		pubdef=createSymbol(checkStrdup(sym[i].name),PUB_PUBLIC,mod,
				    seglist[sym[i].section-1],
				    sym[i].value,
				    -1,-1);
		publics=checkRealloc(publics,sizeof(PSYMBOL)*(pubcount+1));
		publics[pubcount]=pubdef;
		pubcount++;
	    }
	    break;
	case COFF_SYM_STATIC: /* static symbol */
	case COFF_SYM_LABEL: /* code label symbol */
	    if(sym[i].section<-1)
	    {
		break;
	    }
	    if(sym[i].section==0)
	    {
		if(sym[i].value)
		{
		    /* local COMDEF already created */
		    break;
		}
		else
		{
		    /* undefined symbol */
		    addError("Undefined symbol %s in COFF object file %s",sym[i].name,mod->file);
		    return FALSE;
		}
	    }
	    else
	    {
		/* create a LOCAL PUBLIC */
		pubdef=createSymbol(checkStrdup(sym[i].name),PUB_PUBLIC,mod,
				    seglist[sym[i].section-1],
				    sym[i].value,
				    -1,-1);
		locals=checkRealloc(locals,sizeof(PSYMBOL)*(localcount+1));
		locals[localcount]=pubdef;
		localcount++;
	    }
	    break;
	default:
	    break;
	}
    }
    
    /* add segments to master list */
    for(i=0;i<numSect;i++)
    {
	if(!seglist[i]) continue; /* don't add segments that don't exist */
	if(seglist[i]->parent) continue; /* don't add segments subsumed within a group */
	globalSegs=checkRealloc(globalSegs,(globalSegCount+1)*sizeof(PSEG));
	globalSegs[globalSegCount]=seglist[i];
	globalSegCount++;
    }
    /* add comdats to symbol list */
    for(i=0;i<comdatCount;++i)
    {
	comdat=comdatList[i]->comdatList[0];
	for(j=0;j<comdat->segCount;++j)
	{
	    comdat->segList[j]->parent=NULL;
	}
	addGlobalSymbol(comdatList[i]);
    }
    

    /* add external references */
    for(i=0;i<extcount;++i)
    {
	if(externs[i]->local)
	{
	    if(!externs[i]->pubdef) /* if no match found, then error */
	    {
		addError("Unmatched Local Symbol Reference %s, in COFF object file %s",externs[i]->name,mod->file);
		return FALSE;
	    }
	    localExtRefCount++;
	}
	else
	{
	    globalExtRefCount++;
	}
    }
    if(globalExtRefCount)
    {
	globalExterns=checkRealloc(globalExterns,(globalExternCount+globalExtRefCount)*sizeof(PEXTREF));
	for(i=0;i<extcount;++i)
	{
	    if(externs[i]->local) continue; /* skip for locals */
	    globalExterns[globalExternCount]=externs[i];
	    globalExternCount++;
	}
    }
    if(localExtRefCount)
    {
	localExterns=checkRealloc(localExterns,(localExternCount+localExtRefCount)*sizeof(PEXTREF));
	for(i=0;i<extcount;++i)
	{
	    if(!externs[i]->local) continue; /* skip for globals */
	    localExterns[localExternCount]=externs[i];
	    localExternCount++;
	}
    }

    if(localcount)
    {
	localSymbols=checkRealloc(localSymbols,(localSymbolCount+localcount)*sizeof(PSYMBOL));
	for(i=0;i<localcount;++i)
	{
	    localSymbols[localSymbolCount]=locals[i];
	    localSymbolCount++;
	}
    }

    for(i=0;i<pubcount;++i)
    {
	addGlobalSymbol(publics[i]);
    }


    checkFree(comdatList);
    checkFree(seglist);
    checkFree(relofs);
    checkFree(relshift);
    checkFree(sym);
    checkFree(stringList);
    checkFree(externs);
    checkFree(publics);
    checkFree(locals);
    return TRUE;
}

static BOOL loadCoffImport(FILE *objfile)
{
    UCHAR buf[100];
    UINT fileStart;
    UINT thiscpu;
    
    fileStart=ftell(objfile);

    if(fread(buf,1,20,objfile)!=20)
    {
        addError("Unable to read from object file");
        return FALSE;
    }

    if(buf[0] || buf[1] || (buf[2]!=0xff) || (buf[3]|=0xff))
    {
	addError("Invalid Import entry");
	return FALSE;
    }
    /* get CPU type */
    thiscpu=buf[6]+256*buf[7];
    diagnostic(DIAG_VERBOSE,"Import CPU=%04lX",thiscpu);
    
    if((thiscpu<0x14c) || (thiscpu>0x14e))
    {
        addError("Unsupported CPU type %li for module",thiscpu);
        return FALSE;
    }

    /* add segments, externs and symbols to global list */
    return TRUE;
}

BOOL MSCOFFLoad(PFILE objfile,PMODULE mod)
{
    return loadcoff(objfile,mod,FALSE);
}

BOOL DJGPPLoad(PFILE objfile,PMODULE mod)
{
    return loadcoff(objfile,mod,TRUE);
}

BOOL COFFDetect(PFILE objfile,PCHAR name)
{
    UCHAR headbuf[COFF_BASE_HEADER_SIZE];
    UINT thiscpu;

    if(fread(headbuf,1,COFF_BASE_HEADER_SIZE,objfile)!=COFF_BASE_HEADER_SIZE)
	return FALSE;
    thiscpu=headbuf[COFF_MACHINEID]+256*headbuf[COFF_MACHINEID+1];

    if(thiscpu && ((thiscpu<0x14c) || (thiscpu>0x14e)))
	return FALSE;

    return TRUE;
}
