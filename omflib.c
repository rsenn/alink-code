#include "alink.h"
#include "omf.h"

static UCHAR buf[65536];
static BOOL OMFLibModLoad(PFILE f,PMODULE libmod);

BOOL OMFLibDetect(PFILE f,PCHAR name)
{
    UINT blocksize,dicstart,numdicpages,flags;
    INT i;
    
    if(fread(buf,1,3,f)!=3)
    {
	return FALSE;
    }
    if(buf[0]!=LIBHDR)
    {
	return FALSE;
    }
    
    blocksize=buf[1]+256*buf[2];
    if(fread(buf,1,blocksize,f)!=blocksize)
    {
	return FALSE;
    }
    blocksize+=3;

    dicstart=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
    numdicpages=buf[4]+256*buf[5];
    flags=buf[6];

    /* seek to last byte of dictionary */
    fseek(f,dicstart+(numdicpages*512)-1,SEEK_SET);

    i=fgetc(f); /* read that last byte */

    /* if error, or end of file, then not valid */
    if(ferror(f) || feof(f))
    {
	return FALSE;
    }
    
    return TRUE;
}

BOOL OMFLibLoad(PFILE f,PMODULE mod)
{
    UINT blocksize,dicstart,numdicpages,flags;
    INT i,j,k,n;
    PPSYMBOL symlist;
    UINT numsyms;
    PCHAR name;
    UINT modpage;
    
    if(fread(buf,1,3,f)!=3)
    {
	return FALSE;
    }
    if(buf[0]!=LIBHDR)
    {
	return FALSE;
    }
    
    blocksize=buf[1]+256*buf[2];
    if(fread(buf,1,blocksize,f)!=blocksize)
    {
	return FALSE;
    }
    blocksize+=3;

    dicstart=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
    numdicpages=buf[4]+256*buf[5];
    flags=buf[6];

    if(!(flags&LIBF_CASESENSITIVE) && case_sensitive)
    {
	addError("Case-insensitive library cannot be used in case-sensitive link");
	return FALSE;
    }

    if(!numdicpages) return TRUE;
    
    /* seek to dictionary */
    fseek(f,dicstart,SEEK_SET);

    numsyms=0;
    symlist=checkMalloc(numdicpages*37*sizeof(PSYMBOL));

    for(i=0;i<numdicpages;i++)
    {
	if(fread(buf,1,512,f)!=512)
	{
	    addError("Error reading from file %s",mod->file);
	    return FALSE;
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
		if(name[strlen(name)-1]=='!')
		{
		    checkFree(name);
		}
		else
		{
		    symlist[numsyms]=createSymbol(name,PUB_LIBSYM,mod,modpage*blocksize,OMFLibModLoad);
		    ++numsyms;
		}
	    }
	}
    }

    for(i=0;i<numsyms;++i)
    {
	addGlobalSymbol(symlist[i]);
    }
    
    checkFree(symlist);

    return TRUE;
}

static BOOL OMFLibModLoad(PFILE f,PMODULE libmod)
{
    PMODULE mod;
    UINT i;
    
    mod=createModule(libmod->file);
    for(i=0;inputFormats[i].name;++i)
    {
	if(!strcmp(inputFormats[i].name,"omf"))
	{
	    mod->fmt=inputFormats+i;
	    break;
	}
    }
    if(!inputFormats[i].name)
    {
	addError("Unable to find format descriptor for 'omf'");
	return FALSE;
    }
    return mod->fmt->load(f,mod);
}
