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
        mask[i/8]&=!(1<<(i%8));
}

void SetNbit(PUCHAR mask,long i)
{
        mask[i/8]|=(1<<(i%8));
}

char GetNbit(PUCHAR mask,long i)
{
        return (mask[i/8]>>(i%8))&1;
}

int stricmp(const char *s1,const char*s2)
{
        int i;
        for(i=0;s1[i]&&(toupper(s1[i])==toupper(s2[i]));i++);
        if(toupper(s1[i])<toupper(s2[i])) return -1;
        if(toupper(s1[i])>toupper(s2[i])) return +1;
        return 0;
}

char *strupr(char *s)
{
        int i;
        for(i=0;s[i]=toupper(s[i]);i++);
        return s;
}

long GetIndex(PUCHAR buf,long *index)
{
        long i;
        if(buf[*index]&0x80)
        {
                i=((buf[*index]&0x7f)*256)+buf[(*index)+1];
                (*index)+=2;
                return i;
        }
        else
        {
                return buf[(*index)++];
        }
}

void ReportError(long errnum)
{
                printf("\nError in file at %08lX",filepos);
                switch(errnum)
                {
                case ERR_EXTRA_DATA:
                                printf(" - extra data in record\n");
                                break;
                case ERR_NO_HEADER:
                                printf(" - no header record\n");
                                break;
                case ERR_NO_RECDATA:
                                printf(" - record data not present\n");
                                break;
                case ERR_NO_MEM:
                                printf(" - insufficient memory\n");
                                break;
                case ERR_INV_DATA:
                                printf(" - invalid data address\n");
                                break;
                case ERR_INV_SEG:
                                printf(" - invalid segment number\n");
                                break;
                case ERR_BAD_FIXUP:
                                printf(" - invalid fixup record\n");
                                break;
                case ERR_BAD_SEGDEF:
                                printf(" - invalid segment definition record\n");
                                break;
                case ERR_ABS_SEG:
                                printf(" - data emitted to absolute segment\n");
                                break;
                case ERR_DUP_PUBLIC:
                                printf(" - duplicate public definition - %s\n",publics[pubcount]->name);
                                break;
                case ERR_NO_MODEND:
                         printf(" - unexpected end of file (no MODEND record)\n");
                         break;
                case ERR_EXTRA_HEADER:
                         printf(" - duplicate module header\n");
                         break;
                case ERR_UNKNOWN_RECTYPE:
                         printf(" - unknown object module record type %02X\n",rectype);
                         break;
                case ERR_SEG_TOO_LARGE:
                         printf(" - 4Gb Non-Absolute segments not supported.\n");
                         break;
                case ERR_MULTIPLE_STARTS:
                         printf(" - start address defined in more than one module.\n");
                         break;
                case ERR_BAD_GRPDEF:
                         printf(" - illegal group definition\n");
                         break;
                case ERR_OVERWRITE:
                        printf(" - overlapping data regions\n");
                        break;
                case ERR_INVALID_COMENT:
                        printf(" - COMENT record format invalid\n");
                        break;
                case ERR_ILLEGAL_IMPORTS:
                        printf(" - Imports required to link, and not supported by output file type\n");
                        break;
                default:
                                printf("\n");
                }
                exit(1);
}

