#include "alink.h"
#include "omf.h"
#include "mergerec.h"

BOOL case_sensitive=TRUE;
BOOL padsegments=FALSE;
static BOOL mapfile=FALSE;
static PCHAR mapname=NULL;
PCHAR outname=0;

BOOL defaultUse32=FALSE;

BOOL gotstart=FALSE;
RELOC startaddr;
UINT frameAlign=1;
BOOL dosSegOrdering=FALSE;
BOOL noDefaultLibs=FALSE;

UINT errcount=0;

PDATABLOCK lidata;

PPMODULE fileNames=NULL;
UINT fileCount=0;

PPSEG spaceList=NULL;
UINT spaceCount=0;
PPSEG globalSegs=NULL;
UINT globalSegCount=0;
PPEXTREF globalExterns=NULL;
UINT globalExternCount=0;
PPSYMBOL globalSymbols=NULL;
UINT globalSymbolCount=0;
PPEXTREF localExterns=NULL;
UINT localExternCount=0;
PPSYMBOL localSymbols=NULL;
UINT localSymbolCount=0;

PPEXPORTREC globalExports=NULL;
UINT globalExportCount=0;

PRESOURCE globalResources=NULL;
UINT globalResourceCount=0;

UINT expcount=0,expmin=0;

UINT libPathCount=0;
PCHAR *libPath=NULL;
char *entryPoint=NULL;

UINT moduleCount=0;
PPMODULE modules=NULL;

PSEG absoluteSegment=NULL;

PPMERGEREC mergeList=NULL;
UINT mergeCount=0;

BOOL useOldMap=FALSE;

static BOOL NULLDetect(PFILE f,PCHAR name)
{
    return FALSE;
}

static BOOL NULLLoad(PFILE f,PMODULE mod)
{
    return FALSE;
}

CINPUTFMT inputFormats[]={
    {"omf",".obj",OMFDetect,loadOMFModule,"Microsoft/Intel Object Module Format"},
    {"mscoff",".obj",COFFDetect,MSCOFFLoad,"Microsoft Win32 COFF"},
    {"djgpp",".o",COFFDetect,DJGPPLoad,"DJGPP COFF"},
    {"omflib",".lib",OMFLibDetect,OMFLibLoad,"MS/Intel OMF Library"},
    {"mslib",".lib",COFFLibDetect,MSCOFFLibLoad,"Microsoft Win32 COFF Library"},
    {"djgpplib",".a",COFFLibDetect,DJGPPLibLoad,"DJGPP COFF Library"},
    {"win32res",".res",Res32Detect,Res32Load,"Windows 32-bit resource file"},
    {NULL,NULL,NULL,NULL,NULL}
};

COUTPUTFMT outputFormats[]={
    {"pe",PEExtension,PEInitialise,PEFinalise,PESwitches,"MS Portable Executable format"},
    {"exe",".exe",EXEInitialise,EXEFinalise,EXESwitches,"MSDOS EXE format"},
    {"com",".com",COMInitialise,BINFinalise,NULL,"MSDOS COM format"},
    {"bin",".bin",BINInitialise,BINFinalise,BINSwitches,"Binary format"},
    {NULL,NULL,NULL,NULL,NULL,NULL}
};

static CSWITCHENTRY systemSwitches[]={
    {"c",0,"Enable case sensitivity"}, /* case sensitivity */
    {"c+",0,"Enable case sensitivity"}, 
    {"c-",0,"Disable case sensitivity"},
    {"p",0,"Enable segment padding"}, /* segment padding */
    {"p+",0,"Enable segment padding"},
    {"p-",0,"Disable segment padding"},
    {"m",1,"Set map file name"}, /* map file */
    {"m+",0,"Enable map file"},
    {"m-",0,"Disable map file"},
    {"h",0,"Display help screen"}, /* help */
    {"H",0,"Display help screen"},
    {"?",0,"Display help screen"},
    {"-help",0,"Display help screen"},
    {"L",1,"Add path to library search list"}, /* library paths */
    {"o",1,"Set output file name"}, /* output file */
    {"oPE",0,"Select PE output format (compatibility option)"}, /* output formats */
    {"oEXE",0,"Select EXE output format (compatibility option)"},
    {"oCOM",0,"Select COM output format (compatibility option)"},
    {"entry",1,"Set entry point"}, /* entry point */
    {"f",1,"Select specified output format"}, /* output format */
    {"nodeflib",0,"Don't use default libraries from object files"}, /* no default libraries */
    {"iformat",2,"Select specified input format for specified file"},
    {"mergesegs",2,"Merge two segments together"},
    {"v",0,"Verbose diagnostics"},
    {"oldmap",0,"Use ALINK v1.6 compatible map files"},
#if 0
    {"nocase",0,"Disable case sensitivity"}, /* disable case sensitivity */
    {"nosearch",1,"Don't search specified library"}, /* disable single library */
#endif
    {NULL,0}
};

static PSWITCHENTRY switchList=NULL;
static UINT switchCount;
static PCOUTPUTFMT chosenFormat=NULL;

PMODULE createModule(PCHAR filename)
{
    PMODULE m;

    modules=checkRealloc(modules,(moduleCount+1)*sizeof(PMODULE));

    modules[moduleCount]=m=checkMalloc(sizeof(MODULE));
    moduleCount++;
    m->name=NULL;
    m->file=filename;
    m->compiler=NULL;
    m->comments=NULL;
    m->dependencies=NULL;
    m->sourceFiles=NULL;
    m->commentCount=0;
    m->depCount=0;
    m->sourceFileCount=0;
    m->formatSpecificData=NULL;
    m->fmt=NULL;

    return m;
}

void loadFiles()
{
    UINT i,j;
    INT k;
    PCHAR name;
    PCHAR ext;
    FILE *afile;
    PMODULE m;
    PCINPUTFMT fmt;

    for(i=0;i<fileCount;i++)
    {
	m=fileNames[i];
	afile=fopen(m->file,"rb");
	if(!strpbrk(m->file,PATHCHARS))
	{
	    /* if no path specified, search library path list */
	    for(j=0;!afile && j<libPathCount;j++)
	    {
		name=(char*)checkMalloc(strlen(libPath[j])+strlen(m->file)+1);
		strcpy(name,libPath[j]);
		strcat(name,m->file);
		afile=fopen(name,"rb");
		if(afile)
		{
		    free(m->file);
		    m->file=name;
		    name=NULL;
		}
		else
		{
		    free(name);
		    name=NULL;
		}
	    }
	}
	if(!afile)
	{
	    addError("Unable to open file %s",m->file);
	    continue;
	}
	for(j=0;j<i;++j)
	{
#ifdef GOT_CASE_SENSITIVE_FILENAMES	    
	    if(!strcmp(m->file,fileNames[j]->file)) break;
#else
	    if(!stricmp(m->file,fileNames[j]->file)) break;
#endif
	}
	if(j!=i)
	{
	    fclose(afile);
	    continue;
	}
	
	diagnostic(DIAG_VERBOSE,"Loading file %s\n",m->file);

	if(!m->fmt)
	{
	    fmt=NULL;
	    for(j=0;inputFormats[j].name;++j)
	    {
		fseek(afile,0,SEEK_SET);
		name=m->file;
		if(!inputFormats[j].detect)
		{
		    addError("Missing detect routine for %s",inputFormats[i].name);
		    continue;
		}
		
		if(!inputFormats[j].extension)
		    ext="";
		else
		    ext=inputFormats[j].extension;
		
		k=strlen(name)-strlen(ext);
		if(k>=0)
		{
		    name+=k;
		}
		
		if(!strcmp(name,ext) &&
		   inputFormats[j].detect(afile,m->file))
		{
		    if(fmt)
		    {
			addError("%s satisfies detection criteria for %s and %s",m->file,fmt->name,inputFormats[j].name);
			continue;
		    }
		    fmt=inputFormats+j;
		}
	    }
	    if(!fmt)
	    {
		addError("Unable to detect format of %s",m->file);
		fclose(afile);
		continue;
	    }
	    m->fmt=fmt;
	}
	else
	name=NULL;

	if(!m->fmt) continue; /* if format not found, do next file */

	diagnostic(DIAG_VERBOSE,"Format %s\n",m->fmt->name);

	fseek(afile,0,SEEK_SET);
	if(!m->fmt->load(afile,m))
	{
	    addError("Error loading file %s",m->file);
	}
	
	fclose(afile);
    }
}

void checkSegment(PSEG s)
{
    UINT i;
    
    if(!s->use32 && (s->length>=0x10000))
    {
	addError("16 bit %s %s exceeds 64K",s->group?"group":"segment",s->name);
	return;
    }
    for(i=0;i<s->contentCount;++i)
    {
	if(s->contentList[i].flag==SEGMENT)
	{
	    checkSegment(s->contentList[i].seg);
	}
    }
}

static void initialise(void)
{
    UINT i,j;
    PCSWITCHENTRY se;
    PCHAR libList;
    BOOL isend;
    PCOUTPUTFMT of;

    atexit(listErrors);
    
    for(i=0;systemSwitches[i].name;++i);

    switchCount=i;
    if(switchCount)
    {
	switchList=checkMalloc(sizeof(SWITCHPARAM)*switchCount);
	memcpy(switchList,systemSwitches,switchCount*sizeof(SWITCHPARAM));
    }
    else
    {
	switchList=NULL;
    }
    

    /* loop through output formats, getting valid switches */
    of=outputFormats;
    for(;of->name;++of)
    {
	se=of->switches;
	if(!se)
	{
	    continue;
	}
	
	/* loop through switch list for each in turn */
	for(;se->name;++se)
	{
	    /* check against system switches */
	    for(i=0;systemSwitches[i].name;++i)
	    {
		if(!strcmp(systemSwitches[i].name,se->name))
		{
		    addError("Format %s defines invalid switch %s",of->name,se->name);
		    break;
		}
	    }
	    /* check against list already built */
	    for(i=0;i<switchCount;++i)
	    {
		if(!strcmp(switchList[i].name,se->name))
		{
		    if(switchList[i].count==se->count) break;
		    addError("Format %s defines invalid switch %s",of->name,se->name);
		    break;
		}
	    }
	    /* not found, so add to list */
	    if(i==switchCount)
	    {
		switchList=checkRealloc(switchList,sizeof(SWITCHENTRY)*(switchCount+1));
		switchList[switchCount]=(*se);
		++switchCount;
	    }
	}
    }

    /* now get library search path */
    libList=getenv("LIB");
    if(libList)
    {
	for(i=0,j=0;;i++)
	{
	    isend=(!libList[i]);
	    if(libList[i]==';' || !libList[i])
	    {
		if(i-j)
		{
		    libPath=(PCHAR*)checkRealloc(libPath,(libPathCount+1)*sizeof(PCHAR));
		    libList[i]=0;
		    if(strchr(PATHCHARS,libList[i-1]))
		    {
			libPath[libPathCount]=checkStrdup(libList+j);
		    }
		    else
		    {
			libPath[libPathCount]=(PCHAR)checkMalloc(i-j+2);
			strcpy(libPath[libPathCount],libList+j);
			/* add default path separator=first in list */
			libPath[libPathCount][i-j]=PATHCHARS[0];
			libPath[libPathCount][i-j+1]=0;
		    }
		    libPathCount++;
		}
		j=i+1;
	    }
	    if(isend) break;
	}
    }

    /* now create the "absolute" segment */
    absoluteSegment=createSection("ABSOLUTE",NULL,NULL,NULL,0,1);
}

void initFormat(PSWITCHPARAM sp)
{
    UINT i,j,k,l,line;
    int c;
    
    if(!sp)
    {
	if(!fileCount)
	{
	    /* if no files or switches, then display input+output formats */
	    diagnostic(DIAG_VITAL,"Usage:   ALINK [parameter] [parameter] ...\n\n");
	    diagnostic(DIAG_VITAL,"Where each parameter can either be a filename, or an allowed option\n\n");
	    diagnostic(DIAG_VITAL,"For detailed help specify -h, -H, -? or --help on the command line\n\n");
	    diagnostic(DIAG_VITAL,"Available input formats are: ");
	    for(i=0;inputFormats[i].name;++i)
	    {
		diagnostic(DIAG_VITAL,"%s ",inputFormats[i].name);
	    }
	    diagnostic(DIAG_VITAL,"\n");
	    diagnostic(DIAG_VITAL,"Available output formats are: ");
	    for(i=0;outputFormats[i].name;++i)
	    {
		diagnostic(DIAG_VITAL,"%s ",outputFormats[i].name);
	    }
	    diagnostic(DIAG_VITAL,"\n");
	    exit(0);
	}
	chosenFormat=outputFormats;
    }
    else
    {
	for(i=0;sp[i].name;++i)
	{
	    if(!strcmp(sp[i].name,"h")
	       || !strcmp(sp[i].name,"H")
	       || !strcmp(sp[i].name,"?")
	       || !strcmp(sp[i].name,"-help"))
	    {
		diagnostic(DIAG_VITAL,"Usage:   ALINK [parameter] [parameter] ...\n\n");
		diagnostic(DIAG_VITAL,"Where each parameter can either be a filename, or an allowed option\n\n");
		diagnostic(DIAG_VITAL,"For detailed help specify -h, -H, -? or --help on the command line\n\n");
		diagnostic(DIAG_VITAL,"Available input formats are: ");
		for(i=0;inputFormats[i].name;++i)
		{
		    diagnostic(DIAG_VITAL,"%s ",inputFormats[i].name);
		}
		diagnostic(DIAG_VITAL,"\n");
		diagnostic(DIAG_VITAL,"Available output formats are: ");
		for(i=0;outputFormats[i].name;++i)
		{
		    diagnostic(DIAG_VITAL,"%s ",outputFormats[i].name);
		}
		diagnostic(DIAG_VITAL,"\n----Press Enter for allowed options----");
		while(((c=getchar())!='\n') && (c!=EOF));
		diagnostic(DIAG_VITAL,"General Options:\n");
		line=1;
		for(i=0;systemSwitches[i].name;++i)
		{
		    if(line>=24)
		    {
			diagnostic(DIAG_VITAL,"----Press Enter to continue----");
			while(((c=getchar())!='\n') && (c!=EOF));
			line=0;
		    }
		    line++;
		    diagnostic(DIAG_VITAL,"  -%s%ln",systemSwitches[i].name,&k);
		    for(j=0;j<systemSwitches[i].count;++j)
		    {
			diagnostic(DIAG_VITAL," xx");
			k+=3;
		    }
		    for(;k<20;++k) putchar(' ');
		    diagnostic(DIAG_VITAL,"%.*s\n",(int)(80-k),systemSwitches[i].description?systemSwitches[i].description:"");
		}
		for(i=0;outputFormats[i].name;++i)
		{
		    if(line>=23)
		    {
			diagnostic(DIAG_VITAL,"----Press Enter to continue----");
			while(((c=getchar())!='\n') && (c!=EOF));
			line=0;
		    }
		    line+=2;
		    diagnostic(DIAG_VITAL,"\nSwitches for output format %s, %s:\n",outputFormats[i].name,
			   outputFormats[i].description?outputFormats[i].description:"");
		    if(!outputFormats[i].switches) continue;
		    for(j=0;outputFormats[i].switches[j].name;++j)
		    {
			if(line>=24)
			{
			    diagnostic(DIAG_VITAL,"----Press Enter to continue----");
			    while(((c=getchar())!='\n') && (c!=EOF));
			    line=0;
			}
			line++;
			diagnostic(DIAG_VITAL,"  -%s%ln",outputFormats[i].switches[j].name,&k);
			for(l=0;l<outputFormats[i].switches[j].count;++l)
			{
			    diagnostic(DIAG_VITAL," xx");
			    k+=3;
			}
			for(;k<20;++k) putchar(' ');
			diagnostic(DIAG_VITAL,"%.*s\n",(int)(80-k),outputFormats[i].switches[j].description?
				   outputFormats[i].switches[j].description:"");
		    }
		}
		exit(0);
	    }
	    else if(!strcmp(sp[i].name,"c")
		    || !strcmp(sp[i].name,"c+"))
	    {
		case_sensitive=TRUE;
	    }
	    else if(!strcmp(sp[i].name,"c-"))
	    {
		case_sensitive=FALSE;
	    }
	    else if(!strcmp(sp[i].name,"m+"))
	    {
		mapfile=TRUE;
	    }
	    else if(!strcmp(sp[i].name,"m-"))
	    {
		mapfile=FALSE;
	    }
	    else if(!strcmp(sp[i].name,"m"))
	    {
		mapfile=TRUE;
		mapname=sp[i].params[0];
	    }
	    else if(!strcmp(sp[i].name,"L"))
	    {
		libPath=(PCHAR*)checkRealloc(libPath,(libPathCount+1)*sizeof(PCHAR));
		j=strlen(sp[i].params[0]);
		if(strchr(PATHCHARS,sp[i].params[0][j-1]))
		{
		    libPath[libPathCount]=checkStrdup(sp[i].params[0]);
		}
		else
		{
		    libPath[libPathCount]=(PCHAR)checkMalloc(j+2);
		    strcpy(libPath[libPathCount],sp[i].params[0]);
		    /* add default path separator=first in list */
		    libPath[libPathCount][j]=PATHCHARS[0];
		    libPath[libPathCount][j+1]=0;
		}
		libPathCount++;
	    }
	    else if(!strcmp(sp[i].name,"f"))
	    {
		if(chosenFormat)
		{
		    addError("Two output formats specified");
		    continue;
		}
			
		for(j=0;outputFormats[j].name;++j)
		{
		    if(!strcmp(outputFormats[j].name,sp[i].params[0]))
		    {
			chosenFormat=outputFormats+j;
			break;
		    }
		}
		if(!chosenFormat)
		{
		    addError("Unknown output format %s",sp[i].params[0]);
		    continue;
		}
	    }
	    else if(!strcmp(sp[i].name,"o"))
	    {
		if(outname)
		{
		    addError("Two output names specified, \"%s\" and \"%s\"",
			     outname,sp[i].params[0]);
		    continue;
		}
		
		outname=sp[i].params[0];
	    }
	    else if(!strcmp(sp[i].name,"entry"))
	    {
		if(gotstart)
		{
		    addError("Second entry point specified");
		    continue;
		}
		
		globalExterns=checkRealloc(globalExterns,(globalExternCount+1)*sizeof(PEXTREF));
		globalExterns[globalExternCount]=checkMalloc(sizeof(EXTREF));
		globalExterns[globalExternCount]->name=sp[i].params[0];
		globalExterns[globalExternCount]->typenum=-1;
		globalExterns[globalExternCount]->pubdef=NULL;
		globalExterns[globalExternCount]->mod=NULL;
		globalExterns[globalExternCount]->local=FALSE;
		
		/* point start address to this external */
		startaddr.fseg=startaddr.tseg=NULL;
		startaddr.text=startaddr.fext=globalExterns[globalExternCount];
		startaddr.disp=0;

		globalExternCount++;
		gotstart=TRUE;
	    }
	    else if(!strcmp(sp[i].name,"nodeflib"))
	    {
		noDefaultLibs=TRUE;
	    }
	    else if(!strcmp(sp[i].name,"oEXE"))
	    {
		if(chosenFormat)
		{
		    addError("Two output formats specified");
		    continue;
		}
			
		for(j=0;outputFormats[j].name;++j)
		{
		    if(!strcmp(outputFormats[j].name,"exe"))
		    {
			chosenFormat=outputFormats+j;
			break;
		    }
		}
		if(!chosenFormat)
		{
		    addError("Unknown output format exe");
		    continue;
		}
	    }
	    else if(!strcmp(sp[i].name,"oCOM"))
	    {
		if(chosenFormat)
		{
		    addError("Two output formats specified");
		    continue;
		}
			
		for(j=0;outputFormats[j].name;++j)
		{
		    if(!strcmp(outputFormats[j].name,"com"))
		    {
			chosenFormat=outputFormats+j;
			break;
		    }
		}
		if(!chosenFormat)
		{
		    addError("Unknown output format com");
		    continue;
		}
	    }
	    else if(!strcmp(sp[i].name,"oPE"))
	    {
		if(chosenFormat)
		{
		    addError("Two output formats specified");
		    continue;
		}
			
		for(j=0;outputFormats[j].name;++j)
		{
		    if(!strcmp(outputFormats[j].name,"pe"))
		    {
			chosenFormat=outputFormats+j;
			break;
		    }
		}
		if(!chosenFormat)
		{
		    addError("Unknown output format pe");
		    continue;
		}
	    }
	    else if(!strcmp(sp[i].name,"iformat"))
	    {
		/* find specified input format */
		for(j=0;inputFormats[j].name;++j)
		{
		    if(!strcmp(inputFormats[j].name,sp[i].params[0]))
			break;
		}
		/* now add file to list, with specified format */
		fileNames=checkRealloc(fileNames,(fileCount+1)*sizeof(PMODULE));
		fileNames[fileCount]=createModule(sp[i].params[1]);
		if(!inputFormats[j].name)
		{
		    addError("Unknown input format %s",sp[i].params[0]);
		}
		else
		    fileNames[fileCount]->fmt=inputFormats+j;
		++fileCount;
	    }
	    else if(!strcmp(sp[i].name,"mergesegs"))
	    {
		/* now add file to list, with specified format */
                PMERGEREC newMergeRec=createMergeRec(sp[i].params[0],sp[i].params[1]);
                
		mergeList=checkRealloc(mergeList,(mergeCount+1)*sizeof(PMERGEREC));
		mergeList[mergeCount]=newMergeRec;
		++mergeCount;
	    }
	    else if(!strcmp(sp[i].name,"v"))
	    {
                diagnosticLevel=DIAG_VERBOSE;
	    }
	    else if(!strcmp(sp[i].name,"oldmap"))
	    {
                useOldMap=TRUE;
	    }
	}
	if(!chosenFormat)
	{
	    chosenFormat=outputFormats;
	    diagnostic(DIAG_VERBOSE,"No output format specified, defaulting to %s\n",chosenFormat->name);
	}
	/* OK now we've processed system switches, and chosen output format */
	/* check format-specific options */
	for(i=0;sp[i].name;++i)
	{
	    /* skip system switches */
	    for(j=0;systemSwitches[j].name;++j)
	    {
		if(!strcmp(sp[i].name,systemSwitches[j].name)) break;
	    }
	    if(systemSwitches[j].name) continue;
	    if(!chosenFormat->switches)
	    {
		addError("Switch \"%s\" not valid for format %s",sp[i].name,chosenFormat->name);
		continue;
	    }
	    
	    for(j=0;chosenFormat->switches[j].name;++j)
	    {
		if(!strcmp(chosenFormat->switches[j].name,sp[i].name)) break;
	    }
	    if(!chosenFormat->switches[j].name)
	    {
		addError("Switch \"%s\" not valid for format %s",sp[i].name,chosenFormat->name);
		continue;
	    }
	}
    }
    if(chosenFormat->initialise)
    {
	(*chosenFormat->initialise)(sp);
    }
}


int main(int argc,char *argv[])
{
    UINT i;
    PSEG a;
    PFILE ofile;
    PSWITCHPARAM sp;
    PCHAR str;

    diagnosticLevel=DIAG_BASIC;

    diagnostic(DIAG_VITAL,"ALINK v%i.%i (C) Copyright 1998-2004 Anthony A.J. Williams.\n",ALINK_MAJOR,ALINK_MINOR);
    diagnostic(DIAG_VITAL,"All Rights Reserved\n\n");

    initialise();

    if(argc>=1)
    {
	sp=processArgs(argc-1,argv+1,0,switchList,switchCount);
    }
    else
    {
	sp=NULL;
    }
    initFormat(sp);

    if(!fileCount)
    {
	addError("No files specified");
	goto prog_end;
    }

    if(!outname)
    {
	outname=checkStrdup(fileNames[0]->file);
	i=strlen(outname);
	while((i>0)&&(outname[i]!='.')&&(!outname[i] || !strchr(PATHCHARS,outname[i])))
	{
	    i--;
	}
	if(outname[i]=='.')
	{
	    outname[i]=0;
	}
    }
    i=strlen(outname);
    while((i>0)&&(outname[i]!='.')&&(!outname[i] ||!strchr(PATHCHARS,outname[i])))
    {
	i--;
    }
    if(outname[i]!='.')
    {
	if(chosenFormat->extension)
	{
	    str=checkMalloc(strlen(outname)+1+strlen(chosenFormat->extension));
	    strcpy(str,outname);
	    outname=str;
	    strcat(outname,chosenFormat->extension);
	}
    }
    else
    {
	/* strip trailing dots */
	if(i==(strlen(outname)-1))
	{
	    outname[i]=0;
	}
    }
    if(!strlen(outname))
    {
	addError("Empty output filename");
	goto prog_end;
    }

    if(mapfile)
    {
	if(!mapname)
	{
	    mapname=checkMalloc(strlen(outname)+1+4);
	    strcpy(mapname,outname);
	    i=strlen(mapname);
	    while((i>=0)&&(mapname[i]!='.')&&(!mapname[i] || !strchr(PATHCHARS,mapname[i])))
	    {
		i--;
	    }
	    if(mapname[i]!='.')
	    {
		i=strlen(mapname);
	    }
	    strcpy(mapname+i,".map");
	}
    }
    else
    {
	if(mapname)
	{
	    free(mapname);
	    mapname=0;
	}
    }

    diagnostic(DIAG_VERBOSE,"Loading files\n");

    loadFiles();

    /* resolve externs before checking for required modules, in case entry point forces a load */

    resolveExterns();
    
    if(!moduleCount)
    {
	addError("No required modules specified");
	goto prog_end;
    }

    emitCommonSymbols();

    combineSegments();

    diagnostic(DIAG_VERBOSE,"Output format %s\n",chosenFormat->name);

    if(chosenFormat->finalise)
    {
	diagnostic(DIAG_VERBOSE,"Finalising\n");
	chosenFormat->finalise(outname);
    }
    else
    {
	a=createSection("Global",NULL,NULL,NULL,0,1);
        a->internal=TRUE;
	a->addressspace=TRUE;
	a->base=0;

	for(i=0;i<globalSegCount;++i)
	{
	    if(!globalSegs[i]) continue;
	    addSeg(a,globalSegs[i]);
	    globalSegs[i]=NULL;
	}
	spaceCount=1;
	spaceList=checkMalloc(sizeof(PSEG));
	spaceList[0]=a;
	for(i=0;i<spaceCount;++i)
	{	
	    performFixups(spaceList[i]);
	}
    }

    if(mapfile)
    {
        if(useOldMap)
        {
            generateOldMap(mapname);
        }
        else
        {
            generateMap(mapname);
        }
    }
    
    for(i=0;i<spaceCount;++i)
    {
	checkSegment(spaceList[i]);
    }

    if(!errorCount)
    {
	diagnostic(DIAG_VERBOSE,"Writing %s\n",outname);

	ofile=fopen(outname,"w+b");

	if(!ofile)
	{
	    addError("Unable to open output file %s",outname);
	    goto prog_end;
	}
    
	for(i=0;i<spaceCount;++i)
	{
	    writeSeg(ofile,spaceList[i]);
	}

	fclose(ofile);
    }
 prog_end:

    return errorCount?1:0;
}
