#include "alink.h"

static unsigned char defaultStub[]={
    0x4D,0x5A,0x6E,0x00,0x01,0x00,0x00,0x00,
    0x04,0x00,0x11,0x00,0xFF,0xFF,0x03,0x00,
    0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0E,0x1F,0xBA,0x0E,0x00,0xB4,0x09,0xCD,
    0x21,0xB8,0x00,0x4C,0xCD,0x21,0x54,0x68,
    0x69,0x73,0x20,0x70,0x72,0x6F,0x67,0x72,
    0x61,0x6D,0x20,0x72,0x65,0x71,0x75,0x69,
    0x72,0x65,0x73,0x20,0x57,0x69,0x6E,0x64,
    0x6f,0x77,0x73,0x0D,0x0A,0x24
};

static UINT defaultStubSize=sizeof(defaultStub);
static unsigned char moduleDesc[256]="ALINK";
static unsigned char modName[256]="KNILA";

static void NECombineSegments(void)
{
    UINT i,j,k;

    for(i=0;i<grpcount;i++)
    {
	if(!grplist[i]) continue;
	seglist[grplist[i]->segnum]->attr=SEG_PUBLIC | SEG_BYTE;
	for(j=0;j<grplist[i]->numsegs;j++)
	{
	    k=grplist[i]->segindex[j];
	    if(!seglist[k]) continue;
	    /* skip absolute segments */
	    if((seglist[k]->attr&SEG_ALIGN)==SEG_ABS) continue;
	    combine_segments(grplist[i]->segnum,k);
	}
	if(seglist[grplist[i]->segnum]->winFlags&WINF_REMOVE)
            diagnostic(DIAG_BASIC,"Removed!");
	if((seglist[grplist[i]->segnum]->attr&SEG_COMBINE)==SEG_PRIVATE)
            diagnostic(DIAG_BASIC,"Private");


	free(grplist[i]);
	grplist[i]=NULL;
    }
}


static BOOL buildNEHeader(void)
{
    long headerSectNum;
    PUCHAR headbuf;
    PUCHAR stubData;
    UINT stubSize;
    UINT headerSize;
    UINT sectionStart;
    UINT headerStart;
    UINT tableStart;
    UINT i,j,k;

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

    headerSize=NE_HEADERSIZE+stubSize+outcount*(NE_SEGENTRY_SIZE)+1024;

    headbuf=checkMalloc(headerSize);
    memset(headbuf,0,headerSize);

    memcpy(headbuf,stubData,stubSize);

    headbuf[0x3c]=headerStart&0xff;         /* store pointer to PE header */
    headbuf[0x3d]=(headerStart>>8)&0xff;
    headbuf[0x3e]=(headerStart>>16)&0xff;
    headbuf[0x3f]=(headerStart>>24)&0xff;

    headbuf[headerStart+NE_SIGNATURE]='N';
    headbuf[headerStart+NE_SIGNATURE+1]='E';

    /* get shift count for file alignment */
    for(i=0,j=fileAlign;!(j&1);i++)
    {
	j>>=1;
    }

    headbuf[headerStart+NE_SHIFTCOUNT]=i&0xff;
    headbuf[headerStart+NE_SHIFTCOUNT+1]=(i>>8)&0xff;

    headbuf[headerStart+NE_SEGOFFSET]=NE_HEADERSIZE&0xff;
    headbuf[headerStart+NE_SEGOFFSET+1]=(NE_HEADERSIZE>>8)&0xff;

    tableStart=headerStart+NE_HEADERSIZE;

    for(i=0,j=1;i<outcount;i++)
    {
	/* check we want this segment - it has length */
	if(!outlist[i]->length) continue;
	outlist[i]->section=j;

	tableStart+=NE_SEGENTRY_SIZE;
	j++;
    }

    j--;

    headbuf[headerStart+NE_NUMSEGS]=j&0xff;
    headbuf[headerStart+NE_NUMSEGS+1]=(j>>8)&0xff;

    /* next is the resident name table */
    i=tableStart-headerStart;
    if(i>0xffff)
    {
	addError("Offset to resident-names table out of range\n");
	return FALSE;
    }

    headbuf[headerStart+NE_RESNAMEOFFSET]=i&0xff;
    headbuf[headerStart+NE_RESNAMEOFFSET+1]=(i>>8)&0xff;

    /* store the module name in the resident name table */
    headbuf[tableStart]=strlen(modName);
    memcpy(headbuf+tableStart+1,modName,headbuf[tableStart]);
    tableStart+=headbuf[tableStart]+1;
    headbuf[tableStart]=0;
    headbuf[tableStart+1]=0;
    tableStart+=2;

    /* leave a blank entry */
    tableStart+=3;

    /* entry table is next */
    j=tableStart; /* get start */
    i=j-headerStart;
    headbuf[headerStart+NE_EXPORTOFFSET]=i&0xff;
    headbuf[headerStart+NE_EXPORTOFFSET+1]=(i>>8)&0xff;

    /* put exports here */
    tableStart++; /* finish with a NULL */
    j=tableStart-j; /* get length of entry table */
    headbuf[headerStart+NE_EXPORTLENGTH]=j&0xff;
    headbuf[headerStart+NE_EXPORTLENGTH+1]=(j>>8)&0xff;

    /* next is the import table */

    /* last is the non-resident name table */
    headbuf[headerStart+NE_NONRESOFFSET]=tableStart&0xff;
    headbuf[headerStart+NE_NONRESOFFSET+1]=(tableStart>>8)&0xff;
    headbuf[headerStart+NE_NONRESOFFSET+2]=(tableStart>>16)&0xff;
    headbuf[headerStart+NE_NONRESOFFSET+3]=(tableStart>>24)&0xff;

    /* store module description in non-resident name table */
    j=tableStart;
    headbuf[tableStart]=strlen(moduleDesc);
    memcpy(headbuf+tableStart+1,moduleDesc,headbuf[tableStart]);
    tableStart+=headbuf[tableStart]+1;
    headbuf[tableStart]=0;
    headbuf[tableStart+1]=0;
    tableStart+=2;
    j=tableStart-j;
    headbuf[headerStart+NE_NONRESLENGTH]=j&0xff;
    headbuf[headerStart+NE_NONRESLENGTH+1]=(j>>8)&0xff;


    headerSize=tableStart;

    sectionStart=headerSize;

    for(i=0;i<outcount;i++)
    {
	sectionStart+=fileAlign-1;
	sectionStart&=0xffffffff-(fileAlign-1);
	outlist[i]->filepos=sectionStart;
	sectionStart+=outlist[i]->length;
    }

    /* make header the first section in the output list */
    seglist[headerSectNum]->data=headbuf;
    seglist[headerSectNum]->length=headerSize;
    seglist[headerSectNum]->datmask=checkMalloc((headerSize+7)/8);

    memset(seglist[headerSectNum]->datmask,0xff,(headerSize+7)/8);

    outcount++;
    outlist=(PPSEG)checkRealloc(outlist,outcount*sizeof(PSEG));
    memmove(outlist+1,outlist,(outcount-1)*sizeof(PSEG));
    outlist[0]=seglist[headerSectNum];
    outlist[0]->filepos=0;
}


BOOL buildNEFile(void)
{
    UINT i;

    doSegmentedRelocs();
    NECombineSegments();
    sortSegments();

    for(i=0;i<outcount;i++)
    {
	if(!outlist[i]) continue;
	if(outlist[i]->length > 0x10000)
	{
	    addError("Segment %s too long\n",namelist[outlist[i]->nameindex]);
	    return FALSE;
	}

    }


    buildNEHeader();

    alignSegments();
}



