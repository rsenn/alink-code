#include "alink.h"

BOOL Res32Detect(PFILE f,PCHAR name)
{
    unsigned char buf[32];
    static unsigned char buf2[32]={0,0,0,0,0x20,0,0,0,0xff,0xff,0,0,0xff,0xff,0,0,
				   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    if(fread(buf,1,32,f)!=32)
    {
	return FALSE;
    }
    if(memcmp(buf,buf2,32))
    {
	return FALSE;
    }
    return TRUE;
}

BOOL Res32Load(PFILE f,PMODULE mod)
{
    unsigned char buf[32];
    static unsigned char buf2[32]={0,0,0,0,0x20,0,0,0,0xff,0xff,0,0,0xff,0xff,0,0,
				   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    UINT i,j;
    UINT hdrsize,datsize;
    PUCHAR data;
    PUCHAR hdr;
    PRESOURCE resource=NULL;
    UINT rescount=0;

    if(fread(buf,1,32,f)!=32)
    {
	addError("Invalid resource file");
	return FALSE;
    }
    if(memcmp(buf,buf2,32))
    {
	addError("Invalid resource file");
        return FALSE;
    }
    diagnostic(DIAG_BASIC,"Loading Win32 Resource File");
    while(!feof(f))
    {
	i=ftell(f);
	if(i&3)
	{
	    fseek(f,4-(i&3),SEEK_CUR);
	}
        i=fread(buf,1,8,f);
        if(i==0 && feof(f)) break;
        if(i!=8)
	{
	    addError("Invalid resource file, no header");
            return FALSE;
	}
	datsize=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
        hdrsize=buf[4]+(buf[5]<<8)+(buf[6]<<16)+(buf[7]<<24);
	if(hdrsize<16)
        {
	    addError("Invalid resource file, bad header");
            return FALSE;
        }
        hdr=(PUCHAR)checkMalloc(hdrsize);
	if(fread(hdr,1,hdrsize-8,f)!=(hdrsize-8))
	{
	    addError("Invalid resource file, missing header");
	    return FALSE;
	}
	/* if this is a NULL resource, then skip */
	if(!datsize && (hdrsize==32) && !memcmp(buf2+8,hdr,24))
	{
	    checkFree(hdr);
	    continue;
	}
	if(datsize)
	{
	    data=(PUCHAR)checkMalloc(datsize);
	    if(fread(data,1,datsize,f)!=datsize)
	    {
		addError("Invalid resource file, no data");
		return FALSE;
	    }
	}
	else data=NULL;
	resource=(PRESOURCE)checkRealloc(resource,(rescount+1)*sizeof(RESOURCE));
	resource[rescount].is32=TRUE;
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
		addError("Invalid resource file, bad type name");
		return FALSE;
	    }
	    resource[rescount].typename=(PUCHAR)checkMalloc(j-i+2);
	    memcpy(resource[rescount].typename,hdr+i,j-i+2);
	    i=j+2; /* no need for padding between type and name */
	}
	if(i>hdrsize)
	{
	    addError("Invalid resource file, missing resource name");
	    return FALSE;
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
		addError("Invalid resource file, bad name");
		return FALSE;
	    }
	    resource[rescount].name=(PUCHAR)checkMalloc(j-i+2);
	    memcpy(resource[rescount].name,hdr+i,j-i+2);
	    i=j+2;
	}
	/* align to DWORD boundary for version info */
	i+=3;
	i&=0xfffffffc;
	i+=6; /* point to Language ID */
	if(i>hdrsize)
	{
	    addError("Invalid resource file, missing language id");
	    return FALSE;
	}
	resource[rescount].languageid=hdr[i]+256*hdr[i+1];
	rescount++;
	checkFree(hdr);
    }

    if(rescount)
    {
	globalResources=checkRealloc(globalResources,(globalResourceCount+rescount)*sizeof(RESOURCE));
	memcpy(globalResources+globalResourceCount,resource,rescount*sizeof(RESOURCE));
	globalResourceCount+=rescount;
    }
    
    return TRUE;
}

