#include "alink.h"

void loadcoff(FILE *objfile)
{
    unsigned char headbuf[20];
    unsigned char buf[100];
    PUCHAR bigbuf;
    UINT thiscpu;
    UINT numSect;
    UINT symbolPtr;
    UINT numSymbols;
    UINT stringPtr;
    UINT stringSize;
    UINT stringBase=namecount;
    UINT stringCount=0;
    UINT *stringStart=NULL;
    UINT stringOfs;
    UINT i,j;
    UINT fileStart;
    UINT minseg;
    UINT numrel;
    UINT relofs;
    UINT relshift;
    UINT sectname;
    long sectorder;
    PCOFFSYM sym;

    nummods++;
    minseg=segcount;
    fileStart=ftell(objfile);

    if(fread(headbuf,1,20,objfile)!=20)
    {
        printf("Invalid COFF object file\n");
        exit(1);
    }
    thiscpu=headbuf[0]+256*headbuf[1];
    if((thiscpu<0x14c) || (thiscpu>0x14e))
    {
        printf("Unsupported CPU type for module\n");
        exit(1);
    }
    numSect=headbuf[PE_NUMOBJECTS-PE_MACHINEID]+256*headbuf[PE_NUMOBJECTS-PE_MACHINEID+1];

    symbolPtr=headbuf[PE_SYMBOLPTR-PE_MACHINEID]+(headbuf[PE_SYMBOLPTR-PE_MACHINEID+1]<<8)+
        (headbuf[PE_SYMBOLPTR-PE_MACHINEID+2]<<16)+(headbuf[PE_SYMBOLPTR-PE_MACHINEID+3]<<24);

    numSymbols=headbuf[PE_NUMSYMBOLS-PE_MACHINEID]+(headbuf[PE_NUMSYMBOLS-PE_MACHINEID+1]<<8)+
        (headbuf[PE_NUMSYMBOLS-PE_MACHINEID+2]<<16)+(headbuf[PE_NUMSYMBOLS-PE_MACHINEID+3]<<24);

    if(headbuf[PE_HDRSIZE-PE_MACHINEID]|headbuf[PE_HDRSIZE-PE_MACHINEID+1])
    {
        printf("Invalid COFF object file, optional header present\n");
        exit(1);
    }
    stringPtr=symbolPtr+numSymbols*PE_SYMBOL_SIZE;
    if(stringPtr)
    {
        fseek(objfile,fileStart+stringPtr,SEEK_SET);
        if(fread(buf,1,4,objfile)!=4)
        {
            printf("Invalid COFF object file, unable to read string table size\n");
            exit(1);
        }
        stringSize=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
        if(!stringSize) stringSize=4;
        if(stringSize<4)
        {
            printf("Invalid COFF object file, bad string table size %i\n",stringSize);
            exit(1);
        }
        stringPtr+=4;
        stringSize-=4;
    }
    else
    {
        stringSize=0;
    }
    if(stringSize)
    {
        bigbuf=(PUCHAR)malloc(stringSize);
        if(!bigbuf) ReportError(ERR_NO_MEM);
        if(fread(bigbuf,1,stringSize,objfile)!=stringSize)
        {
            printf("Invalid COFF object file, unable to read string table\n");
            exit(1);
        }
        if(bigbuf[stringSize-1])
        {
            printf("Invalid COFF object file, last string unterminated\n");
            exit(1);
        }
        for(i=0;i<stringSize;i++)
        {
            namelist[namecount]=strdup(bigbuf+i);
            if(!namelist[namecount]) ReportError(ERR_NO_MEM);
            stringStart=(UINT *)realloc(stringStart,(stringCount+1)*sizeof(UINT));
            if(!stringStart) ReportError(ERR_NO_MEM);
            stringStart[stringCount]=i;
            i+=strlen(namelist[namecount]);
            namecount++;
            stringCount++;
        }
        free(bigbuf);
    }
    if(symbolPtr && numSymbols)
    {
        fseek(objfile,fileStart+symbolPtr,SEEK_SET);
        sym=(PCOFFSYM)malloc(sizeof(COFFSYM)*numSymbols);
        if(!sym) ReportError(ERR_NO_MEM);
        for(i=0;i<numSymbols;i++)
        {
            if(fread(buf,1,PE_SYMBOL_SIZE,objfile)!=PE_SYMBOL_SIZE)
            {
                printf("Invalid COFF object file, unable to read symbols\n");
                exit(1);
            }
            if(buf[0]|buf[1]|buf[2]|buf[3])
            {
                namelist[namecount]=(PUCHAR)malloc(9);
                strncpy(namelist[namecount],buf,8);
                namelist[namecount][8]=0;
                sym[i].name=namecount;
                namecount++;
            }
            else
            {
                stringOfs=buf[4]+(buf[5]<<8)+(buf[6]<<16)+(buf[7]<<24);
                if(stringOfs<4)
                {
                    printf("Invalid COFF object file\n");
                    exit(1);
                }
                stringOfs-=4;
                if(stringOfs>=stringSize)
                {
                    printf("Invalid COFF object file\n");
                    exit(1);
                }
                for(j=0;j<stringCount;j++)
                {
                    if(stringStart[j]==stringOfs) break;
                }
                if(j==stringCount)
                {
                    printf("Invalid COFF object file\n");
                    exit(1);
                }
                sym[i].name=stringBase+j;
            }
            sym[i].value=buf[8]+(buf[9]<<8)+(buf[10]<<16)+(buf[11]<<24);
            sym[i].section=buf[12]+(buf[13]<<8);
            sym[i].type=buf[14]+(buf[15]<<8);
            sym[i].class=buf[16];
            sym[i].extnum=-1;

            switch(sym[i].class)
            {
            case 2:
                if(sym[i].section<-1)
                {
                    break;
                }
                /* global symbols declare an extern always, so can use in relocs */
                /* they may also include a COMDEF or a PUBDEF */
                externs[extcount]=(PEXTREC)malloc(sizeof(EXTREC));
                if(!externs[extcount]) ReportError(ERR_NO_MEM);
                externs[extcount]->name=strdup(namelist[sym[i].name]);
                if(!externs[extcount]->name) ReportError(ERR_NO_MEM);
                externs[extcount]->pubnum=-1;
                externs[extcount]->modnum=0;
                externs[extcount]->flags=EXT_NOMATCH;
                sym[i].extnum=extcount;
                extcount++;

                if(sym[i].section==0)
                {
                        if(sym[i].value)
                        {
                                comdefs=(PPCOMREC)realloc(comdefs,(comcount+1)*sizeof(PCOMREC));
                                if(!comdefs) ReportError(ERR_NO_MEM);
                                comdefs[comcount]=(PCOMREC)malloc(sizeof(COMREC));
                                if(!comdefs[comcount]) ReportError(ERR_NO_MEM);
                                comdefs[comcount]->length=sym[i].value;
                                comdefs[comcount]->isFar=FALSE;
                                comdefs[comcount]->name=strdup(namelist[sym[i].name]);
                                if(!comdefs[comcount]->name) ReportError(ERR_NO_MEM);
                                comdefs[comcount]->modnum=0;
                                comcount++;                             
                        }
                }
                else
                {
                        publics[pubcount]=(PPUBLIC)malloc(sizeof(PUBLIC));
                        if(!publics[pubcount]) ReportError(ERR_NO_MEM);
                        publics[pubcount]->name=strdup(namelist[sym[i].name]);
                        if(!publics[pubcount]->name) ReportError(ERR_NO_MEM);
                        publics[pubcount]->grpnum=-1;
                        publics[pubcount]->typenum=0;
                        publics[pubcount]->modnum=0;
                        publics[pubcount]->ofs=sym[i].value;

                        if(sym[i].section==-1)
                        {
                                publics[pubcount]->segnum=-1;
                        }
                        else
                        {
                                publics[pubcount]->segnum=minseg+sym[i].section-1;
                        }
                        pubcount++;
                }
                break;
            case 3:
                if(sym[i].section<-1)
                {
                    break;
                }
                /* global symbols declare an extern always, so can use in relocs */
                /* they may also include a COMDEF or a PUBDEF */
                externs[extcount]=(PEXTREC)malloc(sizeof(EXTREC));
                if(!externs[extcount]) ReportError(ERR_NO_MEM);
                externs[extcount]->name=strdup(namelist[sym[i].name]);
                if(!externs[extcount]->name) ReportError(ERR_NO_MEM);
                externs[extcount]->pubnum=-1;
                externs[extcount]->modnum=nummods;
                externs[extcount]->flags=EXT_NOMATCH;
                sym[i].extnum=extcount;
                extcount++;

                if(sym[i].section==0)
                {
                        if(sym[i].value)
                        {
                                comdefs=(PPCOMREC)realloc(comdefs,(comcount+1)*sizeof(PCOMREC));
                                if(!comdefs) ReportError(ERR_NO_MEM);
                                comdefs[comcount]=(PCOMREC)malloc(sizeof(COMREC));
                                if(!comdefs[comcount]) ReportError(ERR_NO_MEM);
                                comdefs[comcount]->length=sym[i].value;
                                comdefs[comcount]->isFar=FALSE;
                                comdefs[comcount]->name=strdup(namelist[sym[i].name]);
                                if(!comdefs[comcount]->name) ReportError(ERR_NO_MEM);
                                comdefs[comcount]->modnum=nummods;
                                comcount++;                             
                        }
                }
                else
                {
                        publics[pubcount]=(PPUBLIC)malloc(sizeof(PUBLIC));
                        if(!publics[pubcount]) ReportError(ERR_NO_MEM);
                        publics[pubcount]->name=strdup(namelist[sym[i].name]);
                        if(!publics[pubcount]->name) ReportError(ERR_NO_MEM);
                        publics[pubcount]->grpnum=-1;
                        publics[pubcount]->typenum=0;
                        publics[pubcount]->modnum=nummods;
                        publics[pubcount]->ofs=sym[i].value;

                        if(sym[i].section==-1)
                        {
                                publics[pubcount]->segnum=-1;
                        }
                        else
                        {
                                publics[pubcount]->segnum=minseg+sym[i].section-1;
                        }
                        pubcount++;
                }
                break;
            case 101:
            case 103:
                break;
            default:
                printf("unsupported symbol class %02X for symbol %s\n",sym[i].class,namelist[sym[i].name]);
                exit(1);
            }
            j=buf[17];
            while(j)
            {
                if(fread(buf,1,PE_SYMBOL_SIZE,objfile)!=PE_SYMBOL_SIZE)
                {
                    printf("Invalid COFF object file\n");
                    exit(1);
                }
                j--;
                i++;
            }
        }
    }
    for(i=0;i<numSect;i++)
    {
        fseek(objfile,fileStart+PE_BASE_HEADER_SIZE-PE_MACHINEID+i*PE_OBJECTENTRY_SIZE,
            SEEK_SET);
        if(fread(buf,1,PE_OBJECTENTRY_SIZE,objfile)!=PE_OBJECTENTRY_SIZE)
        {
                printf("Invalid COFF object file, unable to read section header\n");
                exit(1);
        }
        /* virtual size is also the offset of the data into the segment */
/*
        if(buf[PE_OBJECT_VIRTSIZE]|buf[PE_OBJECT_VIRTSIZE+1]|buf[PE_OBJECT_VIRTSIZE+2]
            |buf[PE_OBJECT_VIRTSIZE+3])
        {
            printf("Invalid COFF object file, section has non-zero virtual size\n");
            exit(1);
        }
*/
        buf[8]=0; /* null terminate name */
        /* get shift value for relocs */
        relshift=buf[PE_OBJECT_VIRTADDR] +(buf[PE_OBJECT_VIRTADDR+1]<<8)+
                (buf[PE_OBJECT_VIRTADDR+2]<<16)+(buf[PE_OBJECT_VIRTADDR+3]<<24);

        if(buf[0]=='/')
        {
            sectname=strtoul(buf+1,(char**)&bigbuf,10);
            if(*bigbuf)
            {
                printf("Invalid COFF object file, invalid number %s\n",buf+1);
                exit(1);
            }
            if(sectname<4)
            {
                printf("Invalid COFF object file\n");
                exit(1);
            }
            sectname-=4;
            if(sectname>=stringSize)
            {
                printf("Invalid COFF object file\n");
                exit(1);
            }
            for(j=0;j<stringCount;j++)
            {
                if(stringStart[j]==sectname) break;
            }
            if(j==stringCount)
            {
                printf("Invalid COFF object file\n");
                exit(1);
            }
            sectname=stringBase+j;
        }
        else
        {
            namelist[namecount]=strdup(buf);
            if(!namelist[namecount]) ReportError(ERR_NO_MEM);
            sectname=namecount;
            namecount++;
        }
        if(strchr(namelist[sectname],'$'))
        {
            /* if we have a grouped segment, sort by original name */
            sectorder=sectname;
            /* and get real name, without $ sort section */
            namelist[namecount]=strdup(namelist[sectname]);
            if(!namelist[namecount]) ReportError(ERR_NO_MEM);
            *(strchr(namelist[namecount],'$'))=0;
            sectname=namecount;
            namecount++;
        }
        else
        {
            sectorder=-1;
        }

        numrel=buf[PE_OBJECT_NUMREL]+(buf[PE_OBJECT_NUMREL+1]<<8);
        relofs=buf[PE_OBJECT_RELPTR]+(buf[PE_OBJECT_RELPTR+1]<<8)+
            (buf[PE_OBJECT_RELPTR+2]<<16) + (buf[PE_OBJECT_RELPTR+3]<<24);

        seglist[segcount]=(PSEG)malloc(sizeof(SEG));
        if(!seglist[segcount]) ReportError(ERR_NO_MEM);

        seglist[segcount]->nameindex=sectname;
        seglist[segcount]->orderindex=sectorder;
        seglist[segcount]->classindex=-1;
        seglist[segcount]->overlayindex=-1;
        seglist[segcount]->length=buf[PE_OBJECT_RAWSIZE]+(buf[PE_OBJECT_RAWSIZE+1]<<8)+
            (buf[PE_OBJECT_RAWSIZE+2]<<16)+(buf[PE_OBJECT_RAWSIZE+3]<<24);

        seglist[segcount]->attr=SEG_PUBLIC|SEG_USE32;
        seglist[segcount]->winFlags=buf[PE_OBJECT_FLAGS]+(buf[PE_OBJECT_FLAGS+1]<<8)+
                (buf[PE_OBJECT_FLAGS+2]<<16)+(buf[PE_OBJECT_FLAGS+3]<<24);
        seglist[segcount]->base=buf[PE_OBJECT_RAWPTR]+(buf[PE_OBJECT_RAWPTR+1]<<8)+
                (buf[PE_OBJECT_RAWPTR+2]<<16)+(buf[PE_OBJECT_RAWPTR+3]<<24);

        switch(seglist[segcount]->winFlags & WINF_ALIGN)
        {
        case WINF_ALIGN_NOPAD:
        case WINF_ALIGN_BYTE:
                seglist[segcount]->attr |= SEG_BYTE;
                break;
        case WINF_ALIGN_WORD:
                seglist[segcount]->attr |= SEG_WORD;
                break;
        case WINF_ALIGN_DWORD:
                seglist[segcount]->attr |= SEG_DWORD;
                break;
        case WINF_ALIGN_8:
                seglist[segcount]->attr |= SEG_8BYTE;
                break;
        case WINF_ALIGN_PARA:
                seglist[segcount]->attr |= SEG_PARA;
                break;
        case WINF_ALIGN_32:
                seglist[segcount]->attr |= SEG_32BYTE;
                break;
        case WINF_ALIGN_64:
                seglist[segcount]->attr |= SEG_64BYTE;
                break;
        case 0:
                seglist[segcount]->attr |= SEG_PARA; /* default */
                break;
        default:
                printf("Invalid COFF object file, bad section alignment %08X\n",seglist[segcount]->winFlags);
                exit(1);
        }

	/* mask off flags not valid for Image file sections */
        seglist[segcount]->winFlags &= WINF_IMAGE_FLAGS;
	/* remove .debug sections */
	if(!stricmp(namelist[sectname],".debug"))
	{
		seglist[segcount]->winFlags |= WINF_REMOVE;
	}

        if(seglist[segcount]->length)
        {
            seglist[segcount]->data=(PUCHAR)malloc(seglist[segcount]->length);
            if(!seglist[segcount]->data) ReportError(ERR_NO_MEM);

            seglist[segcount]->datmask=(PUCHAR)malloc((seglist[segcount]->length+7)/8);
            if(!seglist[segcount]->datmask) ReportError(ERR_NO_MEM);

            if(seglist[segcount]->base)
            {
                fseek(objfile,fileStart+seglist[segcount]->base,SEEK_SET);
                if(fread(seglist[segcount]->data,1,seglist[segcount]->length,objfile)
                        !=seglist[segcount]->length)
                {
                        printf("Invalid COFF object file\n");
                        exit(1);
                }
                for(j=0;j<(seglist[segcount]->length+7)/8;j++)
                        seglist[segcount]->datmask[j]=0xff;
            }
            else
            {
                for(j=0;j<(seglist[segcount]->length+7)/8;j++)
                        seglist[segcount]->datmask[j]=0;
            }

        }
        else
        {
                seglist[segcount]->data=NULL;
                seglist[segcount]->datmask=NULL;
        }

        if(numrel) fseek(objfile,fileStart+relofs,SEEK_SET);
        for(j=0;j<numrel;j++)
        {
                if(fread(buf,1,PE_RELOC_SIZE,objfile)!=PE_RELOC_SIZE)
                {
                        printf("Invalid COFF object file, unable to read reloc table\n");
                        exit(1);
                }
                relocs[fixcount]=(PRELOC)malloc(sizeof(RELOC));
                if(!relocs[fixcount]) ReportError(ERR_NO_MEM);
                /* get address to relocate */
                relocs[fixcount]->ofs=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
                relocs[fixcount]->ofs-=relshift;
                /* get segment */
                relocs[fixcount]->segnum=i+minseg;
                relocs[fixcount]->disp=0;
                /* get relocation target external index */
                relocs[fixcount]->target=buf[4]+(buf[5]<<8)+(buf[6]<<16)+(buf[7]<<24);
                if(relocs[fixcount]->target>=numSymbols)
                {
                        printf("Invalid COFF object file, undefined symbol\n");
                        exit(1);
                }
                relocs[fixcount]->target=sym[relocs[fixcount]->target].extnum;
                if(relocs[fixcount]->target<0)
                {
                        printf("Invalid COFF object file, reloc target symbol doesn't have address\n");
                        exit(1);
                }
                /* set relocation frame/target types */
                relocs[fixcount]->ttype=REL_EXTONLY;
                /* frame is current segment (only relevant for non-FLAT output) */
                relocs[fixcount]->ftype=REL_SEGFRAME;
                relocs[fixcount]->frame=i+minseg;
                /* set relocation type */
                switch(buf[8]+(buf[9]<<8))
                {
                case 6:
                        relocs[fixcount]->rtype=FIX_OFS32;
                        break;
                case 0x14:
                        relocs[fixcount]->rtype=FIX_SELF_OFS32;
                        break;
                default:
                        printf("unsupported COFF relocation type %04X\n",buf[8]+(buf[9]<<8));
                        exit(1);
                }
                fixcount++;
        }

        segcount++;
    }
    if(symbolPtr && numSymbols) free(sym);
}
