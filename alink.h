#ifndef ALINK_H
#define ALINK_H

/* comment out any of the following macros that don't apply */

/* are filename case sensitive (e.g on Linux) or not (e.g. on DOS) */
/* #define GOT_CASE_SENSITIVE_FILENAMES */

/* stricmp, strcmpi and strcasecmp are platform-dependent case-insenstive */
/* string compare functions */
#define GOT_STRICMP
/* #define GOT_STRCMPI */
/* #define GOT_STRCASECMP */

/* strdup is sometimes _strdup */
#define GOT_STRDUP
/* #define GOT__STRDUP */

/* strupr is not always available */
#define GOT_STRUPR

/* which of snprintf and _snprintf do we have (we need one) */
#define GOT_SNPRINTF
/* #define GOT__SNPRINTF */

/* end of configuration section */

#define ALINK_MAJOR 1
#define ALINK_MINOR 7

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#define TRUE  (1==1)
#define FALSE (1==0)

#define SWITCHCHAR '-'
#define PATHCHARS "\\/:"

#define ERR_EXTRA_DATA 1
#define ERR_NO_HEADER 2
#define ERR_NO_RECDATA 3
#define ERR_NO_MEM 4
#define ERR_INV_DATA 5
#define ERR_INV_SEG 6
#define ERR_BAD_FIXUP 7
#define ERR_BAD_SEGDEF 8
#define ERR_ABS_SEG 9
#define ERR_DUP_PUBLIC 10
#define ERR_NO_MODEND 11
#define ERR_EXTRA_HEADER 12
#define ERR_UNKNOWN_RECTYPE 13
#define ERR_SEG_TOO_LARGE 14
#define ERR_MULTIPLE_STARTS 15
#define ERR_BAD_GRPDEF 16
#define ERR_OVERWRITE 17
#define ERR_INVALID_COMENT 18
#define ERR_ILLEGAL_IMPORTS 19


#define LIBF_CASESENSITIVE 1

#define EXE_SIGNATURE     0x00
#define EXE_NUMBYTES      0x02
#define EXE_NUMPAGES      0x04
#define EXE_RELCOUNT      0x06
#define EXE_HEADSIZE      0x08
#define EXE_MINALLOC      0x0a
#define EXE_MAXALLOC      0x0c
#define EXE_STACKSEG	  0x0e
#define EXE_STACKOFS      0x10
#define EXE_STARTOFS      0x14
#define EXE_STARTSEG      0x16
#define EXE_RELOCPOS      0x18
#define EXE_HEADERSIZE    0x20

#define WIN32_DEFAULT_BASE              0x00400000
#define WIN32_DEFAULT_FILEALIGN         0x00000200
#define WIN32_DEFAULT_OBJECTALIGN       0x00001000
#define WIN32_DEFAULT_STACKSIZE         0x00100000
#define WIN32_DEFAULT_STACKCOMMITSIZE   0x00001000
#define WIN32_DEFAULT_HEAPSIZE          0x00100000
#define WIN32_DEFAULT_HEAPCOMMITSIZE    0x00001000
#define WIN32_DEFAULT_SUBSYS            PE_SUBSYS_WINDOWS
#define WIN32_DEFAULT_SUBSYSMAJOR       4
#define WIN32_DEFAULT_SUBSYSMINOR       0
#define WIN32_DEFAULT_OSMAJOR           1
#define WIN32_DEFAULT_OSMINOR           0

#define PUB_PUBLIC 1
#define PUB_ALIAS  2
#define PUB_IMPORT 3
#define PUB_COMDEF 4
#define PUB_COMDAT 5
#define PUB_EXPORT 6
#define PUB_LIBSYM 7

#define DIAG_VITAL   0 /* always displayed */
#define DIAG_BASIC   1 /* basic diagnostics (normal) */
#define DIAG_VERBOSE 2 /* verbose level messages */
#define DIAG_EXTRA   3 /* extra-verbose mode */
#define DIAG_FULL    4 /* maximum normal diagnostic level */
#define DIAG_DEBUG   5 /* full diagnostics for debugging */

typedef char CHAR,*PCHAR,**PPCHAR;
typedef unsigned char UCHAR,*PUCHAR;
typedef unsigned long UINT;
typedef signed long INT;
typedef unsigned short USHORT;
typedef signed short SHORT;
typedef int BOOL;
typedef FILE *PFILE;

typedef struct symbol SYMBOL,*PSYMBOL,**PPSYMBOL;
typedef struct datablock DATABLOCK,*PDATABLOCK;
typedef struct segment SEG,*PSEG,**PPSEG;
typedef struct content CONTENT,*PCONTENT;
typedef struct linenum LINENUM,*PLINENUM;
typedef struct reloc RELOC,*PRELOC;
typedef struct extref EXTREF,*PEXTREF,**PPEXTREF;
typedef struct module MODULE,*PMODULE,**PPMODULE;
typedef struct comdatrec COMDATREC, *PCOMDATREC,**PPCOMDATREC;
typedef struct inputfmt INPUTFMT,*PINPUTFMT;
typedef struct outputfmt OUTPUTFMT,*POUTPUTFMT;
typedef struct switchentry SWITCHENTRY,*PSWITCHENTRY;
typedef struct switchparam SWITCHPARAM,*PSWITCHPARAM;
typedef const struct inputfmt CINPUTFMT,*PCINPUTFMT;
typedef const struct outputfmt COUTPUTFMT,*PCOUTPUTFMT;
typedef const struct switchentry CSWITCHENTRY,*PCSWITCHENTRY;
typedef struct impentry IMPENTRY, *PIMPENTRY;
typedef struct impdll IMPDLL, *PIMPDLL;
typedef struct coffsym COFFSYM, *PCOFFSYM;
typedef struct resource RESOURCE, *PRESOURCE;
typedef struct libmod LIBMOD, *PLIBMOD;
typedef struct exportrec EXPORTREC, *PEXPORTREC,**PPEXPORTREC;
typedef struct scriptblock SCRIPTBLOCK, *PSCRIPTBLOCK;

typedef int (*PCOMPAREFUNC)(const void *x1,const void *x2);

typedef BOOL (DETECTFUNC)(PFILE f,PCHAR name);
typedef BOOL (LOADFUNC)(PFILE f,PMODULE name);
typedef BOOL (INITFUNC)(PSWITCHPARAM options);
typedef BOOL (FINALFUNC)(PCHAR name);
typedef DETECTFUNC *PDETECTFUNC;
typedef LOADFUNC *PLOADFUNC;
typedef INITFUNC *PINITFUNC;
typedef FINALFUNC *PFINALFUNC;

struct inputfmt
{
    PCHAR name;
    PCHAR extension;
    PDETECTFUNC detect;
    PLOADFUNC load;
    PCHAR description;
};

struct outputfmt
{
    PCHAR name;
    PCHAR extension;
    PINITFUNC initialise;
    PFINALFUNC finalise;
    PCSWITCHENTRY switches;
    PCHAR description;
};

struct switchentry
{
    PCHAR name;
    UINT count;
    PCHAR description;
};

struct switchparam
{
    PCHAR name;
    UINT count;
    PPCHAR params;
};

struct extref
{
    PCHAR name;
    INT typenum;
    PSYMBOL pubdef;
    PMODULE mod;
    BOOL local;
};

struct reloc
{
    enum {REL_OFS8,REL_OFS16,REL_OFS32,REL_SEG,REL_PTR16,REL_PTR32,
	  REL_BYTE2} rtype;
    enum {REL_DEFAULT,REL_ABS,REL_RVA,REL_FRAME,REL_SELF,REL_FILEPOS} base;
    UINT ofs;
    PSEG tseg;
    PSEG fseg;
    PEXTREF text;
    PEXTREF fext;
    UINT disp;
};

struct datablock
{
    UINT offset;
    UINT length;
    UINT align;
    PUCHAR data;
};

struct content
{
    enum {DATA,SEGMENT} flag;
    PSEG seg;
    PDATABLOCK data;
};

struct linenum
{
    UINT sourceFile;
    UINT num;
    UINT offset;
};

struct scriptblock
{
    UINT dummy;
};

struct segment
{
    PCHAR name;
    PCHAR class;
    PCHAR sortKey;
    PMODULE mod;
    UINT length;
    UINT base;
    UINT align;
    enum {SEGF_PRIVATE=0,SEGF_PUBLIC,SEGF_COMMON,SEGF_STACK} combine;
    UINT group:1,addressspace:1,absolute:1,use32:1,fpset:1,
	moveable:1,discardable:1,shared:1,
	code:1,initdata:1,uninitdata:1,read:1,write:1,execute:1,
	discard:1,nocache:1,nopage:1;
    INT section;
    UINT filepos;
    UINT contentCount;
    PCONTENT contentList;
    PSEG parent;
    UINT lineCount;
    PLINENUM lines;
    UINT relocCount;
    PRELOC relocs;
    UINT scriptCount;
    PSCRIPTBLOCK scriptList;
};

struct symbol
{
    PCHAR name;
    PMODULE mod;
    INT type;
    UINT refCount;
    PSEG seg;
    INT grpnum;
    INT typenum;
    UINT ofs;
    PCHAR aliasName;
    PCHAR dllname;
    PCHAR impname;
    USHORT ordinal;
    UINT length;
    BOOL isfar;
    PPCOMDATREC comdatList;
    UINT comdatCount;
    UINT filepos;
    PLOADFUNC modload;
    PEXPORTREC export;
};

struct comdatrec
{
    PPSEG segList;
    UINT segCount;
    enum {COMDAT_UNIQUE,COMDAT_LARGEST,
	  COMDAT_SAMESIZE,COMDAT_ANY,COMDAT_EXACT} combine;
};

struct module
{
    PCHAR name;
    PCHAR file;
    PCHAR compiler;
    PPCHAR dependencies;
    UINT depCount;
    PPCHAR sourceFiles;
    UINT sourceFileCount;
    PPCHAR comments;
    UINT commentCount;
    PCINPUTFMT fmt;
    PUCHAR formatSpecificData;
};

struct impentry
{
    PUCHAR name;
    USHORT ordinal;
    PPSYMBOL publist;
    UINT pubcount;
};

struct impdll
{
    PUCHAR name;
    PIMPENTRY entry;
    UINT entryCount;
};

struct coffsym {
    PUCHAR name;
    UINT value;
    SHORT section;
    USHORT type;
    UCHAR class;
    INT extnum;
    UINT numAuxRecs;
    PUCHAR auxRecs;
    BOOL isComDat;
    INT sourceFile;
};

struct resource
{
    BOOL is32;
    PUCHAR typename;
    PUCHAR name;
    PUCHAR data;
    UINT length;
    USHORT typeid;
    USHORT id;
    USHORT languageid;
};

struct libmod
{
    PMODULE mod;
    UINT filepos;
};

struct exportrec
{
    PCHAR int_name;
    PCHAR exp_name;
    UINT ordinal;
    PEXTREF intsym;
    BOOL isResident;
    BOOL noData;
    UINT numParams;
};


int sortCompare(const void *x1,const void *x2);
void ClearNbit(PUCHAR mask,long i);
void SetNbit(PUCHAR mask,long i);
char GetNbit(PUCHAR mask,long i);
int wstricmp(const char *s1,const char*s2);
int wstrlen(const char *s);
unsigned short wtoupper(unsigned short a);
int getBitCount(UINT a);

void *checkMalloc(size_t x);
void *checkRealloc(void *p,size_t x);
char *checkStrdup(const char *s);
void checkFree(void *p);

PDATABLOCK createDataBlock(PUCHAR p,UINT offset,UINT length,UINT align);
void freeDataBlock(PDATABLOCK d);
PSEG createSection(PCHAR name,PCHAR class,PCHAR sortKey,PMODULE mod,UINT length,
		   UINT align);
PSEG createDuplicateSection(PSEG old);
void freeSection(PSEG s);
PSEG addSeg(PSEG s,PSEG c);
PSEG addCommonSeg(PSEG s,PSEG c);
PSEG addData(PSEG s,PDATABLOCK c);
PSEG addFixedData(PSEG s,PDATABLOCK c);
PSEG removeContent(PSEG s,UINT i);
UINT getInitLength(PSEG s);
BOOL writeSeg(FILE *f,PSEG s);

PMODULE createModule(PCHAR filename);
PSWITCHPARAM processArgs(UINT argc,PCHAR *argv,UINT depth,PSWITCHENTRY switchList,UINT switchCount);
BOOL combineSegments(void);

PSYMBOL findSymbol(PCHAR key);
PSYMBOL createSymbol(PCHAR name,INT type,PMODULE mod,...);
void emitCommonSymbols(void);
BOOL addGlobalSymbol(PSYMBOL p);
void resolveExterns(void);
void performFixups(PSEG s);

INITFUNC EXEInitialise;
FINALFUNC EXEFinalise;
extern CSWITCHENTRY EXESwitches[];

INITFUNC PEInitialise;
FINALFUNC PEFinalise;
extern CSWITCHENTRY PESwitches[];
extern char PEExtension[];

INITFUNC BINInitialise;
INITFUNC COMInitialise;
FINALFUNC BINFinalise;
extern CSWITCHENTRY BINSwitches[];

DETECTFUNC OMFDetect;
LOADFUNC loadOMFModule;
DETECTFUNC COFFDetect;
LOADFUNC MSCOFFLoad;
LOADFUNC DJGPPLoad;
DETECTFUNC Res32Detect;
LOADFUNC Res32Load;
DETECTFUNC OMFLibDetect;
LOADFUNC OMFLibLoad;
DETECTFUNC COFFLibDetect;
LOADFUNC MSCOFFLibLoad;
LOADFUNC DJGPPLibLoad;

void generateMap(PCHAR mapname);

BOOL getStub(PCHAR stubName,PUCHAR *pstubData,UINT *pstubSize);

void buildPEFile(void);
void buildNEFile(void);

void diagnostic(int level,char *msg,...);
void addError(char *msg,...);
void listErrors(void);

/* alias strdup for _strdup if one provided but not the other */
#ifndef GOT_STRDUP
#ifdef GOT__STRDUP
#define strdup _strdup
#endif
#endif

/* declare alias for stricmp */
#ifndef GOT_STRICMP
#ifdef GOT_STRCMPI
#define stricmp strcmpi
#else
#ifdef GOT_STRCASECMP
#define stricmp strcasecmp
#endif
#endif
#endif

/* declare prototype for strupr if not present */
#ifndef GOT_STRUPR
char *strupr(char *s);
#endif

/* prototypes for snprintf and vsnprintf if they aren't already defined */
#ifndef GOT_SNPRINTF
int snprintf(char *buf,size_t count,const char *fmt,...);
int vsnprintf(char *buf,size_t count,const char*fmt,va_list ap);
#endif

extern BOOL case_sensitive;
extern BOOL padsegments;
extern BOOL relocsRequired;
extern UINT frameAlign;
extern BOOL dosSegOrdering;
extern BOOL noDefaultLibs;

extern BOOL defaultUse32;

extern BOOL gotstart;
extern RELOC startaddr;

extern UINT errcount;

extern PPMODULE fileNames;
extern UINT fileCount;

extern PPSEG spaceList;
extern UINT spaceCount;
extern PPSEG globalSegs;
extern UINT globalSegCount;
extern PPEXTREF globalExterns;
extern UINT globalExternCount;
extern PPSYMBOL globalSymbols;
extern UINT globalSymbolCount;
extern PPEXPORTREC globalExports;
extern UINT globalExportCount;
extern PPSYMBOL localSymbols;
extern UINT localSymbolCount;
extern PPEXTREF localExterns;
extern UINT localExternCount;

extern PRESOURCE globalResources;
extern UINT globalResourceCount;

extern UINT moduleCount;
extern PPMODULE modules;

extern CINPUTFMT inputFormats[];
extern COUTPUTFMT outputFormats[];

extern int buildDll;
extern UINT libPathCount;
extern PCHAR *libPath;
extern char *entryPoint;
extern PIMPDLL importDll;
extern UINT dllCount;

extern PSEG absoluteSegment;

extern PPCHAR errorList;
extern UINT errorCount;
extern UINT diagnosticLevel;

#endif
