#include "alink.h"

int getBitCount(UINT a)
{
    int count=0;

    while(a)
    {
	if(a&1) count++;
	a>>=1;
    }
    return count;
}

void ClearNbit(PUCHAR mask,long i)
{
    mask[i/8]&=0xff-(1<<(i%8));
}

void SetNbit(PUCHAR mask,long i)
{
    mask[i/8]|=(1<<(i%8));
}

char GetNbit(PUCHAR mask,long i)
{
    return (mask[i/8]>>(i%8))&1;
}

unsigned short wtoupper(unsigned short a)
{
    if(a>=256) return a;
    return toupper(a);
}

int wstricmp(const char *s1,const char *s2)
{
    int i=0;
    unsigned short a,b;

    while(TRUE)
    {
	a=s1[i]+(s1[i+1]<<8);
	b=s2[i]+(s2[i+1]<<8);
	if(wtoupper(a)<wtoupper(b)) return -1;
	if(wtoupper(a)>wtoupper(b)) return +1;
	if(!a) return 0;
	i+=2;
    }
}

int wstrlen(const char *s)
{
    int i;
    for(i=0;s[i]||s[i+1];i+=2);
    return i/2;
}

int sortCompare(const void *x1,const void *x2)
{
    return strcmp(((PSYMBOL) x1)->name,((PSYMBOL)x2)->name);
}

void *checkMalloc(size_t x)
{
    void *p;

    p=malloc(x);
    if(!p)
    {
	fprintf(stderr,"Error, Insufficient memory in call to malloc\n");
	exit(1);
    }
    return p;
}

void *checkRealloc(void *p,size_t x)
{
    p=realloc(p,x);
    if(!p)
    {
	fprintf(stderr,"Error, Insufficient memory in call to realloc\n");
	exit(1);
    }
    return p;
}

char *checkStrdup(const char *s)
{
    char *p;

    if(!s) return NULL;

    p=strdup(s);
    if(!p)
    {
	fprintf(stderr,"Error, Insufficient memory in call to strdup\n");
	exit(1);
    }
    return p;
}

void checkFree(void *p)
{
    if(!p) return;
    free(p);
}




BOOL getStub(PCHAR stubName,PUCHAR *pstubData,UINT *pstubSize)
{
    FILE *f;
    unsigned char headbuf[EXE_HEADERSIZE];
    PUCHAR buf;
    UINT imageSize;
    UINT headerSize;
    UINT relocSize;
    UINT relocStart;
    int i;

    if(stubName)
    {
        f=fopen(stubName,"rb");
        if(!f)
        {
            addError("Unable to open stub file %s",stubName);
            return FALSE;
        }
	/* try and read EXE header */
        if(fread(headbuf,1,EXE_HEADERSIZE,f)!=EXE_HEADERSIZE)
        {
            addError("Error reading from file %s",stubName);
            return FALSE;
        }
        if((headbuf[EXE_SIGNATURE]!=0x4d) || (headbuf[EXE_SIGNATURE+1]!=0x5a))
        {
            addError("Stub not valid EXE file");
            return FALSE;
        }
        /* get size of image */
        imageSize=headbuf[EXE_NUMBYTES]+(headbuf[EXE_NUMBYTES+1]<<8)+
	    ((headbuf[EXE_NUMPAGES]+(headbuf[EXE_NUMPAGES+1]<<8))<<9);
        if(imageSize%512) imageSize-=512;
        headerSize=(headbuf[EXE_HEADSIZE]+(headbuf[EXE_HEADSIZE+1]<<8))<<4;
        relocSize=(headbuf[EXE_RELCOUNT]+(headbuf[EXE_RELCOUNT+1]<<8))<<2;
        imageSize-=headerSize;

        /* allocate buffer for load image */
        buf=(PUCHAR)checkMalloc(imageSize+0x40+((relocSize+0xf)&0xfffffff0));
        /* copy header */
        for(i=0;i<EXE_HEADERSIZE;i++) buf[i]=headbuf[i];

        relocStart=headbuf[EXE_RELOCPOS]+(headbuf[EXE_RELOCPOS+1]<<8);
        /* load relocs */
        fseek(f,relocStart,SEEK_SET);

	if(relocSize)
	{
	    if(fread(buf+0x40,1,relocSize,f)!=relocSize)
	    {
		addError("Error reading from file %s",stubName);
		return FALSE;
	    }
	    /* paragraph align reloc size */
	    relocSize+=0xf;
	    relocSize&=0xfffffff0;

	}

	if(relocSize || (imageSize>0x1c))
	{
	    /* new header is 4 paragraphs long + relocSize*/
	    relocSize>>=4;
	    relocSize+=4;
	}
	else
	{
	    /* no relocs, and short data */
	    relocSize=EXE_HEADERSIZE>>4; /* min header size */
	}

        buf[EXE_HEADSIZE]=relocSize&0xff;
        buf[EXE_HEADSIZE+1]=(relocSize>>8)&0xff;
        /* move to start of data */
        fseek(f,headerSize,SEEK_SET);
        headerSize=relocSize<<4;
        /* load data into correct position */
        if(fread(buf+headerSize,1,imageSize,f)!=imageSize)
        {
            addError("Error reading from file %s",stubName);
            return FALSE;
        }
        /* relocations start at 0x40 */
        buf[EXE_RELOCPOS]=0x40;
        buf[EXE_RELOCPOS+1]=0;
        imageSize+=headerSize; /* total file size */
        /* store pointer and size */
        (*pstubData)=buf;
	if(imageSize>0x40)
	{
	    (*pstubSize)=imageSize;
	}
	else
	{
	    (*pstubSize)=0x40;
	}

        i=imageSize%512; /* size mod 512 */
        imageSize=(imageSize+511)>>9; /* number of 512-byte pages */
        buf[EXE_NUMBYTES]=i&0xff;
        buf[EXE_NUMBYTES+1]=(i>>8)&0xff;
        buf[EXE_NUMPAGES]=imageSize&0xff;
        buf[EXE_NUMPAGES+1]=(imageSize>>8)&0xff;
    }
    else
    {
        (*pstubData)=NULL;
        (*pstubSize)=0;
    }
    return TRUE;
}

/* define strupr if not otherwise defined */
#ifndef GOT_STRUPR

char *strupr(char *s)
{
    char *s1=s;
    if(!s) return NULL;
    for(;*s=toupper(*s);++s);
    return s1;
}

#endif
