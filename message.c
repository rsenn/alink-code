#include "alink.h"

PPCHAR errorList=NULL;
UINT errorCount=0;
UINT diagnosticLevel=0;

#ifndef GOT_SNPRINTF
#ifdef GOT__SNPRINTF

/* define standard snprintf based on MSVC _vsnprintf */
int snprintf(char *buf,size_t count,const char *fmt,...)
{
	va_list ap;
	int i;

	if(count<=0) return 0;

	va_start(ap,fmt);
	i=_vsnprintf(buf,count-1,fmt,ap);
	va_end(ap);

	buf[count-1]=0;
	return (i>=0)?i:count;
}

/* define standard vsnprintf based on MSVC _vsnprintf */
int vsnprintf(char *buf,size_t count,const char*fmt,va_list ap)
{
	int i;

	if(count<=0) return 0;

	i=_vsnprintf(buf,count-1,fmt,ap);

	buf[count-1]=0;
	return (i>=0)?i:count;
}

#else
#error snprintf or _snprintf needed
#endif

#endif

void diagnostic(int level,char *msg,...)
{
    va_list ap;

    /* check we have enabled sufficiently detailed diagnostics */
    if(level>diagnosticLevel) return;
    
    /* if we have, then print the message */
    va_start(ap,msg);
    vprintf(msg,ap);
    va_end(ap);
}

void addError(char *msg,...)
{
    va_list ap;
    char buf[2048];
    
    /* format message */
    va_start(ap,msg);
    vsnprintf(buf,sizeof(buf),msg,ap);
    va_end(ap);

    /* add to error list */
    errorList=checkRealloc(errorList,(errorCount+1)*sizeof(PCHAR));
    errorList[errorCount]=checkStrdup(buf);
    ++errorCount;
}

void listErrors(void)
{
    int i;
    
    for(i=0;i<errorCount;++i)
    {
	fprintf(stderr,"Error: %s\n",errorList[i]);
    }
}
