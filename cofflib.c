#include "alink.h"

static BOOL COFFLibModLoad(PFILE f,PMODULE libmod,BOOL isDjgpp);
static BOOL DJGPPLibModLoad(PFILE f,PMODULE libmod);
static BOOL MSCOFFLibModLoad(PFILE f,PMODULE libmod);
static BOOL COFFLibLoad(PFILE libfile,PMODULE mod,BOOL isDjgpp);

BOOL COFFLibDetect(PFILE libfile,PCHAR libname)
{
    UCHAR buf[60];
    UINT memberSize;
    PCHAR endptr;
    UINT i;

    if(fread(buf,1,8,libfile)!=8)
    {
	return FALSE;
    }
    buf[8]=0;
    /* complain if file header is wrong */
    if(strcmp(buf,"!<arch>\n"))
    {
	return FALSE;
    }
    /* read archive member header */
    if(fread(buf,1,60,libfile)!=60)
    {
	return FALSE;
    }
    if((buf[58]!=0x60) || (buf[59]!='\n'))
    {
	return FALSE;
    }
    buf[16]=0;
    /* check name of first linker member */
    if(strcmp(buf,"/               ")) /* 15 spaces */
    {
	return FALSE;
    }
    buf[58]=0;

    /* strip trailing spaces from size */
    i=57;
    while((i>48) && isspace(buf[i]))
    {
	buf[i]=0;
	i--;
    }
    /* get size */
    errno=0;
    memberSize=strtoul(buf+48,&endptr,10);
    if(errno || (*endptr))
    {
	return FALSE;
    }
    if((memberSize<4) && memberSize)
    {
	return FALSE;
    }
    return TRUE;
}

BOOL MSCOFFLibLoad(PFILE libfile,PMODULE mod)
{
    return COFFLibLoad(libfile,mod,FALSE);
}

BOOL DJGPPLibLoad(PFILE libfile,PMODULE mod)
{
    return COFFLibLoad(libfile,mod,TRUE);
}

static BOOL COFFLibLoad(PFILE libfile,PMODULE mod,BOOL isDjgpp)
{
    UINT i,j;
    UINT numsyms;
    UINT modpage;
    UINT memberSize;
    UINT startPoint;
    PUCHAR endptr;
    PCHAR name;
    PUCHAR modbuf;
    PUCHAR longnames;
    PPSYMBOL symlist;
    INT x;
    UCHAR buf[60];

    startPoint=ftell(libfile);
    
    if(fread(buf,1,8,libfile)!=8)
    {
	addError("Error reading from file %s",mod->file);
	return FALSE;
    }
    buf[8]=0;
    /* complain if file header is wrong */
    if(strcmp(buf,"!<arch>\n"))
    {
	addError("Invalid library file format - bad file header: \"%s\"",buf);
	
	return FALSE;
    }
    /* read archive member header */
    if(fread(buf,1,60,libfile)!=60)
    {
	addError("Error reading from file %s",mod->file);
	return FALSE;
    }
    if((buf[58]!=0x60) || (buf[59]!='\n'))
    {
	addError("Invalid library file format for %s - bad member signature",mod->file);
	return FALSE;
    }
    buf[16]=0;
    /* check name of first linker member */
    if(strcmp(buf,"/               ")) /* 15 spaces */
    {
	addError("Invalid library file format for %s - bad member name",mod->file);
	return FALSE;
    }
    buf[58]=0;

    /* strip trailing spaces from size */
    endptr=buf+57;
    while((endptr>(buf+48)) && isspace(*endptr))
    {
	*endptr=0;
	endptr--;
    }
    
    /* get size */
    errno=0;
    memberSize=strtoul(buf+48,(PPCHAR)&endptr,10);
    if(errno || (*endptr))
    {
	addError("Invalid library file format - bad member size\n");
	return FALSE;
    }
    if((memberSize<4) && memberSize)
    {
	addError("Invalid library file format - bad member size\n");
	return FALSE;
    }
    if(!memberSize)
    {
	numsyms=0;
    }
    else
    {
	if(fread(buf,1,4,libfile)!=4)
	{
	    addError("Error reading from file\n");
	    return FALSE;
	}
	numsyms=buf[3]+(buf[2]<<8)+(buf[1]<<16)+(buf[0]<<24);
    }
    modbuf=(PUCHAR)checkMalloc(numsyms*4);
   
    if(numsyms)
    {
	if(fread(modbuf,1,4*numsyms,libfile)!=4*numsyms)
	{
	    addError("Error reading from file\n");
	    return FALSE;
	}
	symlist=(PPSYMBOL)checkMalloc(sizeof(PSYMBOL)*numsyms);
    }
    
    for(i=0;i<numsyms;i++)
    {
	modpage=modbuf[3+i*4]+(modbuf[2+i*4]<<8)+(modbuf[1+i*4]<<16)+(modbuf[i*4]<<24);
	    
	name=NULL;
	for(j=0;TRUE;j++)
	{
	    if((x=getc(libfile))==EOF)
	    {
		addError("Error reading from file\n");
		return FALSE;
	    }
	    if(!x) break;
	    name=(char*)checkRealloc(name,j+2);
	    name[j]=x;
	    name[j+1]=0;
	}
	if(!name)
	{
	    addError("NULL name for symbol %li\n",i);
	    return FALSE;
	}
	
	symlist[i]=createSymbol(name,PUB_LIBSYM,mod,modpage,isDjgpp?DJGPPLibModLoad:MSCOFFLibModLoad);
    }

    checkFree(modbuf);
    
    if(ftell(libfile)!=(startPoint+68+memberSize))
    {
	addError("Invalid first linker member: Pos=%08lX, should be %08lX",ftell(libfile),startPoint+68+memberSize);
	
	return FALSE;
    }

    /* move to an even byte boundary in the file */
    if(ftell(libfile)&1)
    {
	fseek(libfile,1,SEEK_CUR);
    }

    startPoint=ftell(libfile);

    /* read archive member header */
    if(fread(buf,1,60,libfile)!=60)
    {
	addError("Error reading from file\n");
	return FALSE;
    }
    if((buf[58]!=0x60) || (buf[59]!='\n'))
    {
	addError("Invalid library file format - bad member signature\n");
	return FALSE;
    }
    buf[16]=0;
    /* check name of second linker member */
    if(!strcmp(buf,"/               ")) /* 15 spaces */
    {
	/* OK, so we've found it, now skip over */
	buf[58]=0;

	/* strip trailing spaces from size */
	endptr=buf+57;
	while((endptr>(buf+48)) && isspace(*endptr))
	{
	    *endptr=0;
	    endptr--;
	}
    
	/* get size */
	errno=0;
	memberSize=strtoul(buf+48,(PPCHAR)&endptr,10);
	if(errno || (*endptr))
	{
	    addError("Invalid library file format - bad member size\n");
	    return FALSE;
	}
	if((memberSize<8) && memberSize)
	{
	    addError("Invalid library file format - bad member size\n");
	    return FALSE;
	}

	/* move over second linker member */
	fseek(libfile,startPoint+60+memberSize,SEEK_SET);
    
	/* move to an even byte boundary in the file */
	if(ftell(libfile)&1)
	{
	    fseek(libfile,1,SEEK_CUR);
	}
    }
    else
    {
	fseek(libfile,startPoint,SEEK_SET);
    }
    
    
    startPoint=ftell(libfile);
    longnames=NULL;

    /* read archive member header */
    if(fread(buf,1,60,libfile)!=60)
    {
	addError("Error reading from file\n");
	return FALSE;
    }
    if((buf[58]!=0x60) || (buf[59]!='\n'))
    {
	addError("Invalid library file format - bad 3rd member signature\n");
	return FALSE;
    }
    buf[16]=0;
    /* check name of long names linker member */
    if(!strcmp(buf,"//              ")) /* 14 spaces */
    {
	buf[58]=0;

	/* strip trailing spaces from size */
	endptr=buf+57;
	while((endptr>(buf+48)) && isspace(*endptr))
	{
	    *endptr=0;
	    endptr--;
	}
    
	/* get size */
	errno=0;
	memberSize=strtoul(buf+48,(PPCHAR)&endptr,10);
	if(errno || (*endptr))
	{
	    addError("Invalid library file format - bad member size\n");
	    return FALSE;
	}
	if(memberSize)
	{
	    longnames=(PUCHAR)checkMalloc(memberSize);
	    if(fread(longnames,1,memberSize,libfile)!=memberSize)
	    {
		addError("Error reading from file\n");
		return FALSE;
	    }
	}
    }
    else
    {
	/* if no long names member, move back to member header */
	fseek(libfile,startPoint,SEEK_SET);
    }

    mod->formatSpecificData=longnames;
    
    for(i=0;i<numsyms;++i)
    {
	addGlobalSymbol(symlist[i]);
    }

    checkFree(symlist);
    return TRUE;
}

static BOOL DJGPPLibModLoad(PFILE libfile,PMODULE libmod)
{
    return COFFLibModLoad(libfile,libmod,TRUE);
}

static BOOL MSCOFFLibModLoad(PFILE libfile,PMODULE libmod)
{
    return COFFLibModLoad(libfile,libmod,FALSE);
}

static BOOL COFFLibModLoad(PFILE libfile,PMODULE libmod,BOOL isDjgpp)
{
    PCHAR name;
    UINT ofs;
    UCHAR buf[60];
    PMODULE mod;
    UINT i;
    
    if(fread(buf,1,60,libfile)!=60)
    {
	addError("Error reading from file\n");
	return FALSE;
    }
    if((buf[58]!=0x60) || (buf[59]!='\n'))
    {
	addError("Invalid library member header\n");
	return FALSE;
    }
    buf[16]=0;
    if(buf[0]=='/')
    {
	ofs=15;
	while(isspace(buf[ofs]))
	{
	    buf[ofs]=0;
	    ofs--;
	}
	
	ofs=strtoul(buf+1,&name,10);
	if(!buf[1] || *name)
	{
	    addError("Invalid string number \n");
	    return FALSE;
	}
	name=libmod->formatSpecificData;
	if(!name)
	{
	    addError("Missing long name library member\n");
	    return FALSE;
	}
	name+=ofs;
    }
    else
    {
	name=buf;
    }
    
    mod=createModule(libmod->file);
    mod->name=checkStrdup(name);
    for(i=0;inputFormats[i].name;++i)
    {
	if(isDjgpp)
	{
	    if(!strcmp(inputFormats[i].name,"djgpp"))
	    {
		mod->fmt=inputFormats+i;
		break;
	    }
	}
	else
	{
	    if(!strcmp(inputFormats[i].name,"mscoff"))
	    {
		mod->fmt=inputFormats+i;
		break;
	    }
	}
    }
    if(!inputFormats[i].name)
    {
	addError("Unable to find format descriptor\n");
	return FALSE;
    }
    return mod->fmt->load(libfile,mod);    
}
