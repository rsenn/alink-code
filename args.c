#include "alink.h"

#define MAX_RESPONSE_DEPTH 32

PSWITCHPARAM processArgs(UINT argc,PCHAR *argv,UINT depth,PSWITCHENTRY switchList,UINT switchCount)
{
    UINT i,j;
    INT c;
    PCHAR p;
    PCHAR *newargs;
    UINT newargc;
    FILE *argFile;
    PSWITCHPARAM sp=NULL,sp2;
    UINT spcount=0;
    INT partmatch;

    for(i=0;i<argc;++i)
    {
	partmatch=-1; /* no partial match yet */
	
	/* cater for response files */
	if(argv[i][0]=='@')
	{
	    if(depth>=MAX_RESPONSE_DEPTH)
	    {
		addError("Too many nested response files to read %s",argv[i]+1);
		continue;
	    }
	    
	    argFile=fopen(argv[i]+1,"rt");
	    if(!argFile)
	    {
		addError("Unable to open response file \"%s\"",argv[i]+1);
		continue;
	    }
	    newargs=NULL;
	    newargc=0;
	    p=NULL;
	    j=0;
	    while((c=fgetc(argFile))!=EOF)
	    {
		if(c==';') /* allow comments, starting with ; */
		{
		    while(((c=fgetc(argFile))!=EOF) && (c!='\n')); /* loop until end of line */
		    /* continue main loop */
		    continue;
		}
		if(isspace(c))
		{
		    if(p) /* if we've got an argument, add to list */
		    {
			newargs=(char**)checkRealloc(newargs,(newargc+1)*sizeof(char*));
			newargs[newargc]=p;
			++newargc;
			/* clear pointer and length indicator */
			p=NULL;
			j=0;
		    }
		    /* and continue */
		    continue;
		}
		if(c=='"')
		{
		    /* quoted strings */
		    while(((c=fgetc(argFile))!=EOF) && (c!='"')) /* loop until end of string */
		    {
			if(c=='\\')
			{
			    c=fgetc(argFile);
			    if(c==EOF)
			    {
				addError("Missing character to escape in quoted string");
				break;
			    }
			}
			    
			p=(char*)checkRealloc(p,j+2);
			p[j]=c;
			j++;
			p[j]=0;
		    }
		    if(c==EOF)
		    {
			addError("Unexpected end of file encountered in quoted string");
		    }
		    
		    /* continue main loop */
		    continue;
		}
		/* if no special case, then add to string */
		p=(char*)checkRealloc(p,j+2);
		p[j]=c;
		j++;
		p[j]=0;
	    }
	    if(p)
	    {
		newargs=(char**)checkRealloc(newargs,(newargc+1)*sizeof(char*));
		newargs[newargc]=p;
		++newargc;
	    }
	    fclose(argFile);
	    
	    sp2=processArgs(newargc,newargs,depth+1,switchList,switchCount);
	    if(sp2)
	    {
		for(j=0;sp2[j].name;++j);
		if(j)
		{
		    sp=checkRealloc(sp,sizeof(SWITCHPARAM)*(spcount+j));
		    memcpy(sp+spcount,sp2,j*sizeof(SWITCHPARAM));
		    checkFree(sp2);
		    spcount+=j;
		}
	    }
	    for(j=0;j<newargc;++j)
	    {
		checkFree(newargs[j]);
	    }
	    checkFree(newargs);
	    newargs=NULL;
	    newargc=0;
	}
	else if(argv[i][0]==SWITCHCHAR)
	{
	    if(strlen(argv[i])<2)
	    {
		addError("Invalid argument \"%s\"\n",argv[i]);
		continue;
	    }
	    for(j=0;j<switchCount;++j)
	    {
		if(!strcmp(argv[i]+1,switchList[j].name))
		{
		    partmatch=-1;
		    if((argc-i)<=switchList[j].count)
		    {
			addError("Insufficient parameters for %s, %li expected\n",argv[i],switchList[j].count);
			i=argc;
			break;
		    }
		    sp=checkRealloc(sp,sizeof(SWITCHPARAM)*(spcount+1));
		    sp[spcount].name=switchList[j].name;
		    sp[spcount].count=0;
		    if(switchList[j].count)
		    {
			sp[spcount].params=checkMalloc(switchList[j].count*sizeof(PCHAR));
			for(;sp[spcount].count<switchList[j].count;++(sp[spcount].count))
			{
			    ++i;
			    sp[spcount].params[sp[spcount].count]=checkStrdup(argv[i]);
			}
		    }
		    else
		    {
			sp[spcount].params=NULL;
		    }
		    ++spcount;
		    break;
		}
		if((switchList[j].count==1) && !strncmp(argv[i]+1,switchList[j].name,strlen(switchList[j].name)))
		{
		    /* if we've already got a partial match, and this is another, then flag error */
		    if(partmatch>=0)
		    {
			partmatch=-2; /* two possible partial matches */
		    }
		    else if(partmatch==-1)
		    {
			partmatch=j; /* if no partial matches yet, then this is it */
		    }
		}
	    }
	    if(partmatch>=0)
	    {
		/* if we found a single partial match, then that is the option, and its */
		/* single parameter is the rest of the string */
		j=partmatch;
		sp=checkRealloc(sp,sizeof(SWITCHPARAM)*(spcount+1));
		sp[spcount].name=switchList[j].name;
		sp[spcount].count=0;
		sp[spcount].params=checkMalloc(sizeof(PCHAR));
		sp[spcount].params[0]=checkStrdup(argv[i]+1+strlen(switchList[j].name));
		sp[spcount].count=1;
		++spcount;
	    }
	    
	    if(j==switchCount)
	    {
		addError("Invalid option %s\n",argv[i]);
	    }
	    
	}
	else
	{
	    fileNames=checkRealloc(fileNames,(fileCount+1)*sizeof(PMODULE));
	    fileNames[fileCount]=createModule(checkStrdup(argv[i]));
	    fileCount++;
	}
    }
    if(spcount)
    {
	sp=checkRealloc(sp,(spcount+1)*sizeof(SWITCHPARAM));
	sp[spcount].name=NULL;
	sp[spcount].count=0;
	sp[spcount].params=0;
	return sp;
    }
    else return NULL;
}
