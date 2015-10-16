#ifndef __RSC_H__
#define __RSC_H__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#ifndef __cplusplus
//#include <glib.h>
#define GHashTable void
#else
#include <iostream>
#define GHashTable void
using namespace std;
#endif
#include <sys/time.h>
#include <linux/limits.h>

//#define USINGRSCLOG
#define CBLKSIZE_REAL 131072l
//1048576
#define TIMELEN 1024
#define MAX_MSG 1024
// this is for formatting timestamp
#define NUMFILE 10
#define INVLOC -1
#define CACHE_FILE_REAL "/media/SamsungSSD/RSC.cache"
#define CACHE_FILE g_rscfile
#define CACHE_FILE1 g_rscfile1
#define RSCBLKSIZE g_cblksize
#define MINNUMFILES 100000l
#define MASTERBLOCKOFF 0
#define PX_BOOL int
#define PX_TRUE 1
#define PX_FALSE 0
#define RSCLOG(msg, flag) rsclog(__FILE__, __LINE__, __FUNCTION__, msg, flag)
#define PX_ASSERT(stmt) {PX_BOOL res = (stmt);if(!(res)){RSCLOG("PX_ASSERT", FATAL_F); assert(res);}}
//#define PX_ASSERT(stmt) {PX_BOOL res = (stmt);if(!(res)){RSCLOG("PX_ASSERT", FATAL_F);} }


#define PERROR(fmt, ...) {\
	char str[MAX_MSG];\
	sprintf(str, fmt, ##__VA_ARGS__);\
	fprintf(stderr, str);}

typedef enum {OVERWRITTEN_F, KEEPCURRENT_F, READDISK_F} INSERTFLAG;
typedef enum {NORMAL_F, WARNING_F, FATAL_F} MSGFLAG;

void rsclog(const char *file, int line, const char *func, const char *msg, MSGFLAG msg_f);
PX_BOOL isexp2(off_t val);



#define BLOCKHEADERINDEX(i) ((i)!=-1? ((i)- g_block_pointer_section) / sizeof(BlockPointerNode):-1)
#define HEADERINDEX(i) ((i)!=-1? ((i)- g_file_header_section) / sizeof(FileHeader):-1)
#define BLOCKINDEX(i) ((i)!=-1? ((i)- g_block_section) / RSCBLKSIZE:-1)

//FILE* g_cache_fp;
extern int g_cache_fp;
extern int g_cache_fp1;
extern FILE* g_log_fp;
extern char g_rscfile[PATH_MAX];
extern char g_rscfile1[PATH_MAX];
extern char g_logfile[PATH_MAX];
extern off_t g_cblksize;
extern pthread_mutex_t g_RSC_freelist_mt;
extern pthread_mutex_t g_readahead_mt;
extern pthread_mutexattr_t g_recursive_mt_attr;

extern off_t g_file_header_section; // use for assert
extern off_t g_block_pointer_section; // use for assert
extern off_t g_block_section; // use for assert
extern off_t g_disk_capacity; // use for assert
extern void* mfs_rsc_tab; // this table will store mfs enabled for RSC

/******** in disk *****************/
typedef struct MasterBlock
{
	off_t file_header_section; // use for assert
	off_t file_header_flist;
	off_t file_header_flist_tail;
	off_t block_pointer_section; // use for assert
	off_t block_pointer_flist;
	off_t block_pointer_flist_tail;
	off_t block_section; // use for assert
	off_t capacity; // disk capacity
	off_t cblksize; // cache block size
} MasterBlock;

typedef struct FileHeader
{
	char path[PATH_MAX];

	off_t next;
	off_t loc; // location of FileHeader in disk file
	off_t nodeHeader; // location of header BlockPointerNode in disk file
	off_t nodeTail; // location of tail BlockPointerNode in disk file
	off_t fileoff; // this is the offset of in real file  // TODO could be removed later
} FileHeader;

typedef struct BlockPointerNode
{
#ifdef __cplusplus
	BlockPointerNode(): next(INVLOC), loc(INVLOC), memloc(INVLOC), fhloc(INVLOC), fileblockoff(INVLOC)
	{
	}
	BlockPointerNode(off_t n, off_t l, off_t ml, off_t fl, off_t fbo):
		next(n), loc(l), memloc(ml), fhloc(fl), fileblockoff(fbo)
	{
	}
	friend ostream& operator<<(ostream& os, const BlockPointerNode& bpn)
	{
		os <<"n:"<<bpn.next << ", l:" << bpn.loc << ", m:" << bpn.memloc << ", fl:" << bpn.fhloc<< ", fo:" << bpn.fileblockoff;
		return os;
	}
	~BlockPointerNode()
	{
//		printf("%ld  ", loc);
	}
#endif
	off_t next;
	off_t loc; // location of FileNode in disk file
	off_t memloc; // location of BlockPointerNode in disk file
	off_t fhloc; // file header location debugging purpose // TODO could be removed later
	off_t fileblockoff; // this is the offset of block in real file  // TODO could be removed later
} BlockPointerNode;

/*------------------ lock, unlock macro -------------------------*/
#include <sys/syscall.h>
//FILE *mtlogfp;
//FILE *testfp;
/*#define PX_LOCK(mt_addr) fprintf(mtlogfp, "mutex id is %d, owner is %d, try lock thid is %d, at line %d, function %s \n", (mt_addr)->__data.__lock, (mt_addr)->__data.__owner, (int)syscall(SYS_gettid), __LINE__,__FUNCTION__); \
fflush(mtlogfp); */
#define PX_LOCK(mt_addr) assert(0 == pthread_mutex_lock(mt_addr));
//fprintf(mtlogfp, "mutex id is %d, owner is %d, locked thid is %d, at line %d, function %s \n", (mt_addr)->__data.__lock, (mt_addr)->__data.__owner, (int)syscall(SYS_gettid), __LINE__,__FUNCTION__);
//fflush(mtlogfp);

/*#define PX_UNLOCK(mt_addr) fprintf(mtlogfp, "mutex id is %d, owner is %d , try unlock thid is %d at line %d, function %s \n",(mt_addr)->__data.__lock,(mt_addr)->__data.__owner, (int)syscall(SYS_gettid), __LINE__,__FUNCTION__); \
fflush(mtlogfp);\*/
#define PX_UNLOCK(mt_addr) assert(0 == pthread_mutex_unlock(mt_addr));
// fprintf(mtlogfp, "mutex id is %d, owner is %d , unlocked thid is %d at line %d, function %s \n",(mt_addr)->__data.__lock,(mt_addr)->__data.__owner, (int)syscall(SYS_gettid), __LINE__,__FUNCTION__);
//fflush(mtlogfp);

#define PX_INIT_MUTEX(mt_addr) assert(0 == pthread_mutex_init(mt_addr, NULL))
#define PX_INIT_COND(cv_addr) assert(0 == pthread_cond_init(cv_addr, NULL))
#define PX_WAIT(cv_addr, mt_addr) assert(0 == pthread_cond_wait((cv_addr), (mt_addr)))
#define PX_RESUME(cv_addr) assert(0 == pthread_cond_signal(cv_addr))

void Fsync_cache();

void Read_cache_off(void* buf, off_t size, off_t off);

void Write_cache_off(void* buf, off_t size, off_t off);

void Return_file_block(BlockPointerNode* bn_p);

void Alloc_file_block(BlockPointerNode* bn_p);

void Return_file_header(FileHeader* fh_p);

void Alloc_file_header(FileHeader* fh_p, const char* path);

void SetGlobalSection();

void printbn(BlockPointerNode* bn);

void printfh(FileHeader* fh);

void printbn_d(FileHeader* fh);

//void printStaleList();

void printWholeDisk();

size_t printspaceleft();

void printWholeBlockPointer();

void printfileheaderfreelist();

void printbnfreelist();

// printf master block info into str, if NULL print to stdout
void printMB(MasterBlock mb, char* prstr);
// if out not NULL print out to log file else print to stdout
void logMB(void* out);

void ReadFileToCache(const char *path, off_t size);
//void ReadFileToCache(const char *path);

void ReadFileFromEndToCache(const char *path);

void ReadFileOffToCache(const char *path, void* buf, size_t len, off_t off);

void Read_cache_to_RSC_table(FileHeader* fh_p, BlockPointerNode* bpn_p);

void ReadWholeDiskToRSCTable(PX_BOOL flag);

/******** in ram *****************/
struct file_block_table_m;
typedef struct StaleNode
{
	struct StaleNode* pre;
	struct StaleNode* next;
	struct file_block_table_m* fbt;
}StaleNode;

typedef struct CNode
{
#ifdef __cplusplus
	CNode(): val(NULL), next(NULL)
	{
	}
	CNode(void* v): val(v), next(NULL)
	{
	}
#endif
	void* val;
#ifndef __cplusplus
	struct
#endif
	CNode* next;
}CNode;

/* global stale list, when RSC is full remove the file which is the header stale node */
typedef struct StaleList
{
	struct StaleNode* header;
	struct StaleNode* tail;
	pthread_mutex_t mt;
}StaleList;
extern StaleList g_stale_list;

// this is for truncate function to certain size
typedef struct BlockPointerNode_m
{
#ifdef __cplusplus
//	friend ostream& operator<<(ostream& os, const BlockPointerNode_m& bpm)
//	{
//		os <<"n:"<<bpm.bpn->next << ", l:" << bpm.bpn->loc << ", m:" << bpm.bpn->memloc << ", fl:" << bpm.bpn->fhloc<< ", fo:" << bpm.bpn->fileblockoff;
//		return os;
//	}
#endif
	BlockPointerNode* bpn;
	struct BlockPointerNode_m* next;
	struct BlockPointerNode_m* pre;
} BlockPointerNode_m;

typedef struct file_block_table_m
{
	GHashTable* tab; // key index of cache block, value
//	char * path; // for rename function, this frees stolen key but keep value in hash table
	pthread_mutex_t mt;
	FileHeader* fh_m; // this FileHeader is in ram
//	BlockPointerNode* tail_m; // this BlockPointerNode is in ram
	BlockPointerNode_m* header; // this BlockPointerNode_m header is in ram
	BlockPointerNode_m* tail; // this BlockPointerNode_m tail is in ram
	StaleNode* sn; // file on double linked stale list
} file_block_table_m;

typedef struct RSC_table_m
{
	GHashTable* tab; // key file mfs path, value file_block_table_m
	pthread_mutex_t mt;
} RSC_table_m;

#ifdef __cplusplus
extern "C"
#endif
void* Init_RSC_tab();

extern RSC_table_m* g_RSC_table_m;

off_t* off_tdup(off_t v);

void Cleanup(void* addr);

file_block_table_m* Init_file_block_table();

// this function is for rename since, we need to keep the value but modify the key
void Destory_file_block_table(void* addr);

// only 1 thread call this to init
// convert *cmctx into cmctx_t tpye
int Init_RSC_table_m(void *cmctx);

// only 1 thread call this to finit
// this function is for rename since, we need to keep the value but modify the key
void Destory_RSC_table_m();

// this will release the old file cache if file name present
void Insert_RSC_table(const char *path, char* buf, size_t len, off_t off, INSERTFLAG insertflag);

// return 1 if found else 0
int Read_RSC_table(const char *path, char* buf, size_t len, off_t off);

// this function is for rename
void Rename_file_block_table(const char *oldfn, const char *newfn);

// this function is for rm
void Rm_file_block_table(const char *path);
void Rm_file_block_table_fbt(file_block_table_m* fbt);

// this function is for truncate
void Truncate_file_block_table(const char *path, off_t off);

//void Append_to_stale_list(StaleNode* sn);
void Append_to_stale_list(const char* path);

//void Remove_from_stale_list(StaleNode* sn);
//void Rm_node_stale_file(StaleNode* sn);
void Rm_file_from_stale_list(const char* path);

void printfh_m();

void* Voiddup(const void*from, size_t size);


/*--------------utils------------*/

PX_BOOL startswith(char* str, char* substr);
void trim(char* str);

size_t strtoint(char* tmp);

PX_BOOL readLine(FILE* fp, char* buf);

void readentry(char* entry, char* name, char*tmp);
int readrscconf(char* path, char* log, size_t *size, off_t *cblksize, int* numcache);


/*------------- prefetch-------------*/
typedef enum
{
	PATHDIR_T, PATHFILE_T
} PathType;

typedef struct PXSignal
{
	pthread_mutex_t mt;
	pthread_cond_t cv;
	PX_BOOL done;
	void* msg;
}PXSignal;

typedef struct PathEntry
{
	PathType pt;
	char path[PATH_MAX];
	struct PathEntry* next;
} PathEntry;

typedef struct FetchQueue
{
	const char* rscpath;
	void* cmh; // convert this to cmh
	void* flag;
	off_t filesize;
	struct FetchQueue* next;
} FetchQueue;

typedef struct Fetch_files_table
{
	GHashTable* tab; // key index of cache block, value
	pthread_mutex_t mt;
} Fetch_files_table;
extern Fetch_files_table* g_fft;


typedef struct FetchWorker
{
	pthread_cond_t cv;
	pthread_mutex_t mt;
	pthread_t tid;
	int id;
	int used;
	int exit;
	FetchQueue *head;
	FetchQueue *tail;
	Fetch_files_table* g_fft;

}FetchWorker;
extern FetchWorker* g_fetchworker;
extern int g_fetchworker_exit;
#define NUMFETCHWORKER 4l
#define FETCHWORKERSLEEPTIME 20

void fetchDir(const char *name);
void Init_fetch_thread();
void Finit_fetch_thread();
void Block_fetch_thread();
void Wakeup_fetch_thread();
void QueueFileToRSC(const struct stat *stbuf, const char* path);
void QueueCMHToRSC_fsize(const off_t filesize, void* cmht); // must convert cmh to cmh_t type
void QueueCMHToRSC(const struct stat *stbuf, void* cmht); // must convert cmh to cmh_t type


#endif

