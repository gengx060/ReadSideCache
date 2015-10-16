#include <RSC.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <pxcache.h>
#include <cppx.h>

inline off_t* off_tdup(off_t v)
{
	off_t *key = malloc(sizeof(off_t));
	*key = v;

	return key;
}

void* Voiddup(const void*from, size_t size)
{
	void *to = malloc(size);
	if (to != NULL)
		return memcpy(to, from, size);
	else
		return NULL;
}

inline void Cleanup(void* addr)
{
	free(addr);
}

inline file_block_table_m* Init_file_block_table()
{
	file_block_table_m *fbt = (file_block_table_m *) calloc(1,
			sizeof(file_block_table_m));
	/* below is redundant */
	fbt->tab = NULL;
//	fbt->path = NULL;
	fbt->tail = NULL;
	fbt->header = NULL;
	fbt->fh_m = NULL;
	fbt->sn = NULL;
	PX_ASSERT(pthread_mutex_init(&fbt->mt, &g_recursive_mt_attr) == 0);

	return fbt;
}

// this function is for rename since, we need to keep the value but modify the key
//void Destory_file_block_table(void* addr)
//{
//	PX_LOCK(&g_RSC_table_m->mt);
//
//	file_block_table_m* fbt = (file_block_table_m*) addr;
//
//	PX_LOCK(&fbt->mt);
//	PX_UNLOCK(&g_RSC_table_m->mt);
//
////	g_hash_table_destroy(fbt->tab);
////	free(fbt->path); // already freed by clean up keys
//	free(fbt->fh_m);
//	Rm_node_stale_file(fbt->sn);
//
////	free(fbt->tail_m); // already freed by g_hash_table_destroy(fbt->tab);
//	PX_UNLOCK(&fbt->mt);
//// fbt->keyaddr = NULL;// no need since when value destory function called, key destory function also called
//
//	PX_LOCK(&g_RSC_table_m->mt);
//	PX_ASSERT(pthread_mutex_destroy(&fbt->mt) == 0);
//	free(fbt);
//
//	PX_UNLOCK(&g_RSC_table_m->mt);
//}

PX_BOOL init_RSC_table_f = PX_FALSE;
// only 1 thread call this to init
int Init_RSC_table_m(void *cmctx)
{
	// only init RSC_table once
	if (!init_RSC_table_f)
		init_RSC_table_f = PX_TRUE;
	else
		return 0;

	cmctx = (cmctx_t*)cmctx;
	CMTRACE(cmctx, "cmctx 0x%lx\n", cmctx);
	int err = 0;

	mfs_rsc_tab = NULL;
	Init_mfs_rsc_tab();

	g_RSC_table_m = (RSC_table_m*) calloc(1l, sizeof(RSC_table_m));
	PX_ASSERT(g_RSC_table_m != NULL);
	g_RSC_table_m->tab = RSC_tab_init();
//	g_RSC_table_m->tab = g_hash_table_new_full(g_str_hash, g_str_equal, Cleanup,
//			(void*) Destory_file_block_table); // Destory_file_block_table will free key space
	PX_ASSERT(g_RSC_table_m->tab != NULL);

	PX_ASSERT(pthread_mutexattr_init(&g_recursive_mt_attr) == 0);
	PX_ASSERT(pthread_mutexattr_settype(&g_recursive_mt_attr, PTHREAD_MUTEX_RECURSIVE) == 0);
	PX_ASSERT(pthread_mutex_init(&g_RSC_table_m->mt, &g_recursive_mt_attr) == 0);
	PX_ASSERT(pthread_mutex_init(&g_RSC_freelist_mt, &g_recursive_mt_attr) == 0);
	PX_ASSERT(pthread_mutex_init(&g_readahead_mt, NULL) == 0);
	PX_ASSERT(pthread_mutex_init(&g_stale_list.mt, &g_recursive_mt_attr) == 0);

	int numcachefile;
// Implement a default and over ride for cache file path
	err = readrscconf(CACHE_FILE, g_logfile, NULL, &RSCBLKSIZE, &numcachefile);
	if (0 != err)
	{
		CMLOG(cmctx, "error %d accessing cache file %s\n",
			err, CACHE_FILE);
		return err;
	}

#ifdef USEFREAD
	PX_ASSERT(0 == access(CACHE_FILE, F_OK));
	g_cache_fp = fopen(CACHE_FILE, "r+b");
#else
	PX_ASSERT(0 == access(CACHE_FILE, F_OK));
	g_cache_fp = open(CACHE_FILE, O_RDWR);
#endif
	if (0 == g_cache_fp) {
		CMLOG(cmctx, "error %d accessing cache file %s\n",
			errno, CACHE_FILE);
		return errno;
	}

#ifdef USINGRSCLOG
	g_log_fp = fopen(g_logfile, "a+");
	if (NULL == g_log_fp) {
		CMLOG(cmctx, "error %d accessing log file %s\n",
			errno, g_logfile);
		return errno;
	}
#endif
	SetGlobalSection();

	g_stale_list.header = NULL; // init stale list to null
	g_stale_list.tail = NULL; // init stale list to null
	ReadWholeDiskToRSCTable(PX_TRUE);

	Init_fetch_thread();
//	fetchDir(MOUNTPOINT);
	RSCLOG("log master block on destory/finit\n", NORMAL_F);
	logMB(NULL);
	return 0;
}

// only 1 thread call this to finit
// this function is for rename since, we need to keep the value but modify the key
void Destory_RSC_table_m()
{
	RSCLOG("log master block on destory/finit\n", NORMAL_F);
	logMB(NULL);

	Finit_fetch_thread();

	PX_ASSERT(g_RSC_table_m != NULL);
	PX_LOCK(&g_RSC_table_m->mt);
	RSC_tab_destroy(g_RSC_table_m->tab);
//	g_hash_table_destroy(g_RSC_table_m->tab);
	PX_UNLOCK(&g_RSC_table_m->mt);
// fbt->keyaddr = NULL;// no need since when value destory function called, key destory function also called
	free(g_fetchworker);
	free(g_RSC_table_m);
	Destroy_mfs_rsc_tab();
	PX_ASSERT(pthread_mutexattr_destroy(&g_recursive_mt_attr) == 0);

#ifdef USINGRSCLOG
	PX_ASSERT(fclose(g_log_fp) == 0);
#endif

#ifdef USEFREAD
		PX_ASSERT(fclose(g_cache_fp) == 0);
#else
	PX_ASSERT(close(g_cache_fp) == 0);
#endif
}

//static void Return_file_block_m(void* addr)
//{
//	BlockPointerNode_m* bpm_p = (BlockPointerNode_m*) addr;
//	free(bpm_p->bpn);
//	free(addr);
//}

// this will release the old file cache if file name present
void Read_cache_to_RSC_table(FileHeader* fh_p, BlockPointerNode* bpn_p)
{
	if (0 == g_cache_fp) {
		CMLOG(NULL, "error %d accessing cache file %s\n",
			errno, CACHE_FILE);
		return;
	}
	PX_ASSERT(bpn_p->fileblockoff % RSCBLKSIZE == 0); // off and len cannot cross two cache blocks!

	/*------------- ram section update -------------------------*/
	PX_LOCK(&g_RSC_table_m->mt);
//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, fh_p->path);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, fh_p->path);
	if (fbt != NULL)
	{
//		BlockPointerNode_m* bpn_t = g_hash_table_lookup(fbt->tab, &bpn_p->fileblockoff);
		BlockPointerNode_m* bpn_t = Block_tab_lookup(fbt->tab, bpn_p->fileblockoff);
		// currently we dont do anything for write function when realoff is not present
		// write function only update present block
		if (bpn_t == NULL)
		{
			BlockPointerNode* insert_bn_p = Voiddup(bpn_p, sizeof(BlockPointerNode));
			PX_ASSERT((insert_bn_p->memloc-g_block_section)%RSCBLKSIZE == 0); // TODO remove PX_ASSERT
			BlockPointerNode_m* bpm_p = (BlockPointerNode_m*) calloc(1l,
					sizeof(BlockPointerNode_m));

//			update BlockPointerNode list
			bpm_p->bpn = insert_bn_p;
			bpm_p->pre = fbt->tail;
			bpm_p->next = NULL;
			fbt->tail->next = bpm_p;
			fbt->tail = bpm_p;

//			off_t *key = Voiddup(&bpn_p->fileblockoff, sizeof(off_t));
//			g_hash_table_insert(fbt->tab, key, bpm_p);
			Block_tab_insert(fbt->tab, bpn_p->fileblockoff, bpm_p);
		}
	}
	else
	{
		/* create new file block table which will contains all the data with the off set */
		file_block_table_m* fbt = Init_file_block_table();
		fbt->tab = Block_tab_init();
//		fbt->tab = g_hash_table_new_full(g_int64_hash, g_int64_equal,
//				(void*) Cleanup, (void*) Return_file_block_m);

		FileHeader* insert_fh_p = Voiddup(fh_p, sizeof(FileHeader));
		PX_ASSERT((insert_fh_p->loc-g_file_header_section)%sizeof(FileHeader) == 0); // TODO remove PX_ASSERT

		// alloc block pointer from disk
		BlockPointerNode* insert_bn_p = Voiddup(bpn_p, sizeof(BlockPointerNode));
		PX_ASSERT((insert_bn_p->memloc-g_block_section)%RSCBLKSIZE == 0); // TODO remove PX_ASSERT

		BlockPointerNode_m* bpm_p = (BlockPointerNode_m*) calloc(1l, sizeof(BlockPointerNode_m));
		bpm_p->bpn = insert_bn_p;
		bpm_p->pre = NULL;
		bpm_p->next = NULL;

		// update file header and block info in ram
//		off_t* key = Voiddup(&bpn_p->fileblockoff, sizeof(off_t));
//		g_hash_table_insert(fbt->tab, key, bpm_p);
		Block_tab_insert(fbt->tab, bpn_p->fileblockoff, bpm_p);

//		char* filename = strdup(fh_p->path);
//		fbt->path = filename;
		fbt->fh_m = insert_fh_p;
		fbt->header = bpm_p;
		fbt->tail = bpm_p;
		/* insert file block talbe into RSC_table */
//		g_hash_table_insert(g_RSC_table_m->tab, filename, fbt);
		RSC_tab_insert(g_RSC_table_m->tab, fh_p->path, fbt);
	}
	PX_UNLOCK(&g_RSC_table_m->mt);

	/*------------- disk section update ---------TODO use a thread doing this----------------*/

}

/* insert filename and its blocks infor into RSC_m table
 * if cache not full alloc blocks from freelist
 * else try to remove the stales cache file from cache then realloc the blocks
 *  	if cache full and every cached file open then dont cache the file/read directly from nas
 */
void Insert_RSC_table(const char *path, char* buf, size_t len, off_t off, INSERTFLAG insertflag)
{
	if (0 == g_cache_fp) {
		CMLOG(NULL, "error %d accessing cache file %s\n",
			errno, CACHE_FILE);
		return;
	}
	BlockPointerNode_m* bpm_p = NULL;
	BlockPointerNode* bn_p = NULL; // (BlockPointerNode*)calloc(1l, sizeof(BlockPointerNode));
	FileHeader* fh_p = NULL;
	BlockPointerNode* fh_tail_p = NULL;

	off_t realoff = off;
	off_t left = 0;
	PX_ASSERT(off - off % RSCBLKSIZE + RSCBLKSIZE >= off +len); // off and len cannot cross two cache blocks!
	//	KEEPCURRENT_F is for read from nas, if off is not multiple of cblksize, we will round it to multiple of cblksize
	//	and read whole block, this will happen after *NULL->fuseops.read)

	/*------------- ram section update -------------------------*/
	PX_LOCK(&g_RSC_table_m->mt);
//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, path);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, path);
	if (fbt != NULL)
	{
//// release file node in here
		if(insertflag == KEEPCURRENT_F)
		{
			// since we already round the off to multiple of cblksize in KEEPCURRENT_F
			PX_ASSERT(realoff % RSCBLKSIZE == 0);
		}
		else if(insertflag == OVERWRITTEN_F)
		{
			left = realoff % RSCBLKSIZE;
			realoff = realoff- left;
		}
		else
		{
			PX_ASSERT(insertflag != READDISK_F); // TODO remove redundant,
			assert(realoff % RSCBLKSIZE == 0);
		}

//		BlockPointerNode_m* bpn_t = g_hash_table_lookup(fbt->tab, &realoff);
		BlockPointerNode_m* bpn_t = Block_tab_lookup(fbt->tab, realoff);
		// currently we dont do anything for write function when realoff is not present
		// write function only update present block
		if (bpn_t == NULL && (insertflag == KEEPCURRENT_F || insertflag == OVERWRITTEN_F))
		{
			bn_p = (BlockPointerNode*) calloc(1l, sizeof(BlockPointerNode));
			Alloc_file_block(bn_p);
			if (bn_p->loc == INVLOC)
			{
				if(g_stale_list.header == NULL) // all files are currently being open or accessed
				{
					RSCLOG("NO RSC cache blocks avaiable! and every cached file is being open", WARNING_F);
					logMB(NULL);

					free(bn_p);
					PX_UNLOCK(&g_RSC_table_m->mt);
					return; // no blocks available from here, so no cache until blocks available
				}
				else
				{
					logMB(NULL);

					StaleNode* oldestnode = g_stale_list.header;
					file_block_table_m* oldestfbt = oldestnode->fbt;
					char msg[MAX_MSG];
					sprintf(msg, "cache full but reusing file %s !\n", oldestfbt->fh_m->path);
					RSCLOG(msg, WARNING_F);
					printf("%s\n", msg);
//					Rm_node_stale_file(oldestnode);
					Rm_file_block_table_fbt(oldestfbt); // this will Rm_node_stale_file(oldestnode);
//					free(oldestnode); //this will be free in Rm_node_stale_file (oldestnode)

					logMB(NULL);

					Alloc_file_block(bn_p); // now there must be cache blocks available!
					PX_ASSERT(bn_p->loc != INVLOC);
				}
			}
			PX_ASSERT((bn_p->memloc-g_block_section)%RSCBLKSIZE == 0); // TODO remove PX_ASSERT
			bpm_p = (BlockPointerNode_m*) calloc(1l,
					sizeof(BlockPointerNode_m));
			bpm_p->bpn = bn_p;

			fh_p = fbt->fh_m;
			if(fbt->tail == NULL) // this case is file truncate to 0
			{
				PX_ASSERT(insertflag == OVERWRITTEN_F);
				PX_ASSERT(fbt->header == NULL);
//				fh_tail_p = fbt->tail->bpn; // here dont need to update tail pointer since its the first 1
				fbt->header = bpm_p;
				fbt->tail = bpm_p;
				bn_p->next = INVLOC;
				bn_p->fileblockoff = realoff;
				bn_p->fhloc = fh_p->loc;
				fh_p->nodeHeader = bn_p->loc;
				fh_p->nodeTail = bn_p->loc;
				off_t filelength = realoff + len;
				if (fh_p->fileoff < filelength)
				{
					fh_p->fileoff = filelength;
				}
//				off_t *key = off_tdup(realoff);
//				g_hash_table_insert(fbt->tab, key, bpm_p);
				Block_tab_insert(fbt->tab, realoff, bpm_p);
			}
			else //if(insertflag == KEEPCURRENT_F)
			{
//				PX_ASSERT(insertflag == KEEPCURRENT_F);
				fh_tail_p = fbt->tail->bpn;

				// update fh_p tail block info
				PX_ASSERT(fh_tail_p->loc != 0); // TODO remove PX_ASSERT
				PX_ASSERT(fh_tail_p->fhloc == fh_p->loc); // TODO remove PX_ASSERT

				off_t filelength = realoff + len;
				if (fh_p->fileoff < filelength)
				{
					fh_p->fileoff = filelength;
				}
				fh_p->nodeTail = bn_p->loc;
				fh_tail_p->next = bn_p->loc;

		//			update BlockPointerNode list
				bpm_p->pre = fbt->tail;
				bpm_p->next = NULL;
				fbt->tail->next = bpm_p;
				fbt->tail = bpm_p;

				bn_p->fhloc = fh_p->loc;
				bn_p->next = INVLOC;
				bn_p->fileblockoff = realoff;

//				off_t *key = off_tdup(realoff);
//				g_hash_table_insert(fbt->tab, key, bpm_p);
				Block_tab_insert(fbt->tab, realoff, bpm_p);
			}
		}
		 // this is for write function which will upate the cache block content
		else if(bpn_t && insertflag == OVERWRITTEN_F)
		{
			PX_ASSERT(len <= RSCBLKSIZE); // TODO remove redundant,
			off_t filelength = off + len;
			fh_p = fbt->fh_m;
			if (fh_p->fileoff < filelength)
			{
				fh_p->fileoff = filelength;
//				fbt->fh_m->nodeTail = bpn_t->bpn->loc;
			}
//			fbt->fh_m->nodeTail
			Write_cache_off(buf, len, bpn_t->bpn->memloc+left);
		}
	}
	else if(insertflag == KEEPCURRENT_F) // here write function will not trigger this function
	{
		/* create new file block table which will contains all the data with the off set */
		file_block_table_m* fbt = Init_file_block_table();
//		fbt->tab = g_hash_table_new_full(g_int64_hash, g_int64_equal,
//				(void*) Cleanup, (void*) Return_file_block_m);
		fbt->tab = Block_tab_init();

		// alloc file header from disk
		fh_p = (FileHeader*) calloc(1l, sizeof(FileHeader));
		Alloc_file_header(fh_p, path);
		if (fh_p->loc == INVLOC)
		{
			RSCLOG("NO RSC file headers avaiable!", WARNING_F);
			free(fh_p);
			PX_UNLOCK(&g_RSC_table_m->mt);
			return; //
		}
		PX_ASSERT((fh_p->loc-g_file_header_section)%sizeof(FileHeader) == 0); // TODO remove PX_ASSERT

		// alloc block pointer from disk
		bn_p = (BlockPointerNode*) calloc(1l, sizeof(BlockPointerNode));
		Alloc_file_block(bn_p);
		if (bn_p->loc == INVLOC)
		{
			if(g_stale_list.header == NULL) // all files are currently being open or accessed
			{
				RSCLOG("first file block, NO RSC cache blocks avaiable! and every cached file is bing open", WARNING_F);
				logMB(NULL);

				free(bn_p);
				Return_file_header(fh_p);
				free(fh_p);
				Block_tab_destroy(fbt->tab);
				assert(pthread_mutex_destroy(&fbt->mt) == 0);
				free(fbt);
				PX_UNLOCK(&g_RSC_table_m->mt);
				return; // no blocks available from here, so no cache until blocks available
			}
			else
			{
				StaleNode* oldestnode = g_stale_list.header;
				file_block_table_m* oldestfbt = oldestnode->fbt;

				char msg[MAX_MSG];
				sprintf(msg, "first file block,  cache full but reusing file %s !\n", oldestfbt->fh_m->path);
				RSCLOG(msg, WARNING_F);
				logMB(NULL);

				printf("%s\n", msg);
//				Rm_node_stale_file(oldestnode);
				Rm_file_block_table_fbt(oldestfbt); // this will Rm_node_stale_file (oldestnode);
//				RSC_tab_remove_wrap(g_RSC_table_m->tab, oldestfbt->fh_m->path);
//				free(oldestnode); //this will be free in Rm_node_stale_file (oldestnode)
				logMB(NULL);

				Alloc_file_block(bn_p); // now there must be cache blocks available!
				PX_ASSERT(bn_p->loc != INVLOC);
			}
//			RSCLOG("NO RSC cache blocks avaiable!", WARNING_F);
//			free(bn_p);
//			Return_file_header(fh_p);
//			free(fh_p);
//			PX_UNLOCK(&g_RSC_table_m->mt);
//			return; // no blocks available from here, so no cache until blocks available
		}
		PX_ASSERT((bn_p->memloc-g_block_section)%RSCBLKSIZE == 0); // TODO remove PX_ASSERT
		bpm_p = (BlockPointerNode_m*) calloc(1l, sizeof(BlockPointerNode_m));
		bpm_p->bpn = bn_p;
		bpm_p->pre = NULL;
		bpm_p->next = NULL;
//		Write_cache_off(buf, len, bn_p->memloc); // write buf into cache block
//		if(len<RSCBLKSIZE)
//			Write_cache_off(buf, len, bn_p->memloc); // this will remove old block data

		// update file header and block info in ram
		off_t filelength = off + len;
		if (fh_p->fileoff < filelength)
		{
			fh_p->fileoff = filelength;
		}
		fh_p->nodeHeader = bn_p->loc;
		fh_p->nodeTail = bn_p->loc;
		fh_p->next = INVLOC;
		strcpy(fh_p->path, path);

		bn_p->fhloc = fh_p->loc;
		bn_p->next = INVLOC;
		bn_p->fileblockoff = realoff;

//		off_t *key = off_tdup(off);
//		g_hash_table_insert(fbt->tab, key, bpm_p);
		Block_tab_insert(fbt->tab, off, bpm_p);

//		char* filename = strdup(path);
//		fbt->path = filename;
//		fbt->path = fh_p->path;
		fbt->fh_m = fh_p;
//		fbt->tail_m = bn_p;
		fbt->header = bpm_p;
		fbt->tail = bpm_p;
		/* insert file block talbe into RSC_table */
//		g_hash_table_insert(g_RSC_table_m->tab, filename, fbt);
		RSC_tab_insert(g_RSC_table_m->tab, fh_p->path, fbt);
	}
//	PX_UNLOCK(&g_RSC_table_m->mt); // uncomment this makes dirty read.....

	/*------------- disk section update ---------TODO use a thread doing this----------------*/
	// update file header and block info in ram
	PX_BOOL syncflag = PX_FALSE;
	if (bn_p)
	{
		Write_cache_off(bn_p, sizeof(BlockPointerNode), bn_p->loc); // write into block pointer
		Write_cache_off(buf, len, bn_p->memloc);
		syncflag = PX_TRUE;
	}
	if (fh_tail_p)
	{
		Write_cache_off(fh_tail_p, sizeof(BlockPointerNode), fh_tail_p->loc); // write into block pointer
		syncflag = PX_TRUE;
	}
	if (fh_p) // only when create new file block table / new path comes
	{
		Write_cache_off(fh_p, sizeof(FileHeader), fh_p->loc); // write into file header
		syncflag = PX_TRUE;
	}
	if(syncflag)
		Fsync_cache();
	PX_UNLOCK(&g_RSC_table_m->mt);
}

/* try to read file block with its offset from cache file
 * success return the data length
 * failure return -1
 */
int Read_RSC_table(const char *path, char* buf, size_t len, off_t off)
{
	int ret = -1;
	if (0 == g_cache_fp) {
		CMLOG(NULL, "error %d accessing cache file %s\n",
			errno, CACHE_FILE);
		return ret;
	}
	BlockPointerNode_m* bpm_p = NULL;
	off_t realoff = off;
	off_t left = 0;

	PX_LOCK(&g_RSC_table_m->mt);
//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, path);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, path);
	if (fbt != NULL) // found file path in RSC_table
	{
		// calculate the offset make sure if it start from 0 offset inside cacheblock
		left = realoff % RSCBLKSIZE;
		if(left != 0)
		{
			realoff = realoff- left;
		}
//		bpm_p = g_hash_table_lookup(fbt->tab, &realoff);
		bpm_p = Block_tab_lookup(fbt->tab, realoff);

		if (bpm_p != NULL)
		{
			PX_LOCK(&g_RSC_freelist_mt);
			if(off > fbt->fh_m->fileoff)
			{
				ret = 0;
			}
			else if (off + len > fbt->fh_m->fileoff)
			{
				len = fbt->fh_m->fileoff - off;
			}
		}
	}
	PX_UNLOCK(&g_RSC_table_m->mt);

	if (bpm_p != NULL) //  if found
	{
		PX_ASSERT(len + left <= RSCBLKSIZE); // TODO remove redundant,
		PX_ASSERT(left <= RSCBLKSIZE); // TODO remove redundant,
		if(len + left > RSCBLKSIZE)
		{
			len = RSCBLKSIZE - left;
		}

		Read_cache_off(buf, len, bpm_p->bpn->memloc+left);
		PX_UNLOCK(&g_RSC_freelist_mt);

		ret = len;
	}

	return ret;
}

/* this function is for rename
 * if found in cache then rename it
 * obsolete...
 */
void Rename_file_block_table(const char *oldfn, const char *newfn)
{
//	if (0 == g_cache_fp) {
//		CMLOG(NULL, "error %d accessing cache file %s\n",
//			errno, CACHE_FILE);
//		return;
//	}
//	PX_LOCK(&g_RSC_table_m->mt);
//
//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, oldfn);
//	if (fbt != NULL)
//	{
//		/*------------- ram section update -------------------------*/
//		char* oldkey = fbt->path;
//		/* insert file block talbe into RSC_table */
//		g_hash_table_steal(g_RSC_table_m->tab, oldfn);
//		free(oldkey);
//
////		char* newname = strdup(newfn);
//		fbt->path = strdup(newfn);
//		strcpy(fbt->fh_m->path, fbt->path);
//
//		g_hash_table_insert(g_RSC_table_m->tab, fbt->path, fbt);
//		PX_UNLOCK(&g_RSC_table_m->mt);
//
//		/*------------- disk section update --------------TODO use a thread doing this-----------*/
//		Write_cache_off(fbt->fh_m, sizeof(FileHeader), fbt->fh_m->loc); // write into file header
//	}
//	else
//	{
//		PX_UNLOCK(&g_RSC_table_m->mt);
//	}

}

void RSC_tab_remove_wrap(void* rsctab, const char* path)
{
	file_block_table_m* fbt = RSC_tab_remove(rsctab, path);
	if (fbt != NULL)
	{
		PX_LOCK(&fbt->mt);

		Block_tab_destroy(fbt->tab);
		free(fbt->fh_m);
		Rm_node_stale_file(fbt->sn);

		PX_UNLOCK(&fbt->mt);

		assert(pthread_mutex_destroy(&fbt->mt) == 0);
		free(fbt);
	}
}
/* this function is for rm
 * if found then remove it from cache
 */
void Rm_file_block_table_fbt(file_block_table_m* fbt)
{
	if (0 == g_cache_fp) {
		CMLOG(NULL, "error %d accessing cache file %s\n",
			errno, CACHE_FILE);
		return;
	}
	PX_LOCK(&g_RSC_table_m->mt);

	if (fbt != NULL)
	{
		/*------------- ram section update -------------------------*/
//		PX_ASSERT(strcmp(path, fbt->path) == 0); // TODO remove later

		FileHeader fh;
		memcpy(&fh, fbt->fh_m, sizeof(FileHeader));
		if (fbt->tail)
		{
			if (fbt->fh_m->nodeTail != fbt->tail->bpn->loc) // TODO remove later
			{
				char msg[MAX_MSG];
				sprintf(msg, "nodeTail %ld, tail_m->loc %ld\n", fbt->fh_m->nodeTail,
						fbt->tail->bpn->loc);
				RSCLOG(msg, NORMAL_F);
			}
			PX_ASSERT(fbt->fh_m->nodeTail == fbt->tail->bpn->loc); // TODO remove later
		}
		else
		{
			PX_ASSERT(fbt->fh_m->nodeTail == INVLOC);
		}

//		g_hash_table_remove(g_RSC_table_m->tab, fbt->path);
//		RSC_tab_remove(g_RSC_table_m->tab, fbt->fh_m->path);
		RSC_tab_remove_wrap(g_RSC_table_m->tab, fbt->fh_m->path);
		PX_UNLOCK(&g_RSC_table_m->mt);

		/*------------- disk section update -------TODO use a thread doing this------------------*/
//		Write_cache_off(fbt->fh_m, sizeof(FileHeader), fbt->fh_m->loc); // write into file header
		off_t i = fh.nodeHeader;
		Return_file_header(&fh);

		// return all block pointer to free list on disk, TODO use a thread doing this
		BlockPointerNode bn;
		while (i != INVLOC)
		{
			off_t loc = i;
			Read_cache_off(&bn, sizeof(BlockPointerNode), loc);
			i = bn.next;
			Return_file_block(&bn);
		}
		/*------ here we will not write zero data to cache block ------------*/
	}
	else
	{
		PX_UNLOCK(&g_RSC_table_m->mt);
	}
}

// this function is for rm
void Rm_file_block_table(const char *path)
{
	if (0 == g_cache_fp) {
		CMLOG(NULL, "error %d accessing cache file %s\n",
			errno, CACHE_FILE);
		return;
	}
	PX_LOCK(&g_RSC_table_m->mt);

//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, path);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, path);
	if (fbt != NULL)
	{
		PX_ASSERT(strcmp(path, fbt->fh_m->path) == 0); // TODO remove later
		Rm_file_block_table_fbt(fbt);
	}

	PX_UNLOCK(&g_RSC_table_m->mt);

}

/* this function is for truncate
 * if found
 */
void Truncate_file_block_table(const char *path, off_t off)
{
	if (0 == g_cache_fp) {
		CMLOG(NULL, "error %d accessing cache file %s\n",
			errno, CACHE_FILE);
		return;
	}
	PX_LOCK(&g_RSC_table_m->mt);

//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, path);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, path);
	if (fbt != NULL)
	{
		/*------------- ram section update -------------------------*/
//		PX_ASSERT(strcmp(path, fbt->path) == 0); // TODO remove later
		if (off <= 0l)
		{
			PX_ASSERT(strcmp(path, fbt->fh_m->path) == 0);
			Rm_file_block_table_fbt(fbt);
			PX_UNLOCK(&g_RSC_table_m->mt);
			return;
		}

		if (fbt->fh_m->fileoff < off) // truncate is greater than current file
		{
			PX_UNLOCK(&g_RSC_table_m->mt);
			return;
		}

		PX_UNLOCK(&g_RSC_table_m->mt);

		PX_LOCK(&fbt->mt);

		if (fbt->tail)
		{
			if (fbt->fh_m->nodeTail != fbt->tail->bpn->loc) // TODO remove later
			{
				char msg[MAX_MSG];
				sprintf(msg, "nodeTail %ld, tail->loc %ld\n", fbt->fh_m->nodeTail,
						fbt->tail->bpn->loc);
				RSCLOG(msg, NORMAL_F);
			}
			PX_ASSERT(fbt->fh_m->nodeTail == fbt->tail->bpn->loc); // TODO remove later
		}
		else
		{
			PX_ASSERT(fbt->fh_m->nodeTail == INVLOC);
		}

		// update file lenght in file header
		fbt->fh_m->fileoff = off;
		if (off <= 0l)
		{
			fbt->fh_m->nodeHeader = INVLOC;
//			fbt->tail_m->loc = INVLOC;
			fbt->fh_m->nodeTail = INVLOC;
		}

		// release file node in here
		CNode* cnlist = Block_tab_truncate(off, fbt->tab);
		CNode* node = cnlist;
		while (node != NULL)
		{
			BlockPointerNode_m* bpm_p = node->val;
			if (fbt->header == bpm_p) // this means header is removed, we need to update it
			{
				fbt->header = bpm_p->next;
				if (fbt->header)
				{
					fbt->header->pre = NULL;
					fbt->fh_m->nodeHeader = fbt->header->bpn->loc;
				}
				else // only 1 node
				{
					fbt->tail = NULL;
					fbt->fh_m->nodeHeader = INVLOC;
					fbt->fh_m->nodeTail = INVLOC;
				}
			}

			if (fbt->tail == bpm_p) // this means tail is removed, we need to update it
			{
				fbt->tail = bpm_p->pre;
				if (fbt->tail)
				{
					fbt->tail->next = NULL;
					fbt->fh_m->nodeTail = fbt->tail->bpn->loc;
					fbt->tail->bpn->next = INVLOC;
//						update blockpointer in disk
					Write_cache_off(fbt->tail->bpn,
							sizeof(BlockPointerNode), fbt->tail->bpn->loc);
				}
				else // only 1 node
				{
					fbt->header = NULL;
					fbt->fh_m->nodeTail = INVLOC;
					fbt->fh_m->nodeHeader = INVLOC;
				}
			}

			BlockPointerNode_m* pre = bpm_p->pre;
			BlockPointerNode_m* next = bpm_p->next;
			if (bpm_p->pre)
				pre->next = next;
			if (bpm_p->next)
				next->pre = pre;

			Return_file_block(bpm_p->bpn);
			free(bpm_p->bpn);
			free(bpm_p);

			node = node->next;
		}
		Release_cnode_list(cnlist);
		PX_UNLOCK(&fbt->mt);

		/*------------- disk section update -------TODO use a thread doing this------------------*/
		Write_cache_off(fbt->fh_m, sizeof(FileHeader), fbt->fh_m->loc); // write into file header

		/*------ here we will not write zero data to cache block ------------*/
	}
	else
	{
		PX_UNLOCK(&g_RSC_table_m->mt);
	}

}

//void Append_to_stale_list(StaleNode* sn)
//void Append_to_stale_list(file_block_table_m *fbt)
void Append_to_stale_list(const char* path)
{
	PX_LOCK(&g_RSC_table_m->mt);
//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, path);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, path);
	if (fbt == NULL)
	{
		printf("\nnot exisit Append_to_stale_list %s", path);
		printStaleList();
		PX_UNLOCK(&g_RSC_table_m->mt);
		return;
	}
	else
	{
		if(fbt->sn == NULL)
		{
			fbt->sn = (StaleNode*)calloc(1l, sizeof(StaleNode));
		}
		else // fbt->sn->fbt != NULL, already in stale list
		{
			PX_UNLOCK(&g_RSC_table_m->mt);
			return;
		}
		PX_ASSERT(fbt->sn->fbt == NULL); // we need to always make sure stale node is not in stale list when we try to append it
		PX_UNLOCK(&g_RSC_table_m->mt);
	}

	PX_LOCK(&g_stale_list.mt);

	StaleNode* sn = fbt->sn;

//	StaleNode* sn = (StaleNode*)calloc(1l, sizeof(StaleNode));
//	fbt->sn = sn;

	sn->pre = g_stale_list.tail;
	sn->next = NULL;
	if(g_stale_list.header == NULL)
	{
		g_stale_list.header = sn;
	}
	if(g_stale_list.tail != NULL)
	{
		g_stale_list.tail->next = sn;
	}
	g_stale_list.tail = sn;

	sn->fbt = fbt; // sn->fbt is not NULL
//	printf("\nafter Append_to_stale_list %s", path);
//	printStaleList();

	PX_UNLOCK(&g_stale_list.mt);
}

//void Remove_from_stale_list(StaleNode* sn)
//void Remove_from_stale_list(file_block_table_m *fbt)
// when file is open or reopen then we need to remove it from stale list
// and when file is
void Rm_file_from_stale_list(const char* path)
{
	PX_LOCK(&g_RSC_table_m->mt);
//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, path);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, path);
	if (fbt == NULL || fbt->sn == NULL)
	{
		PX_UNLOCK(&g_RSC_table_m->mt);
		printf("\nnot exisit Rm_file_from_stale_list %s", path);
		printStaleList();
		return;
	}
//	else
//	{
//		PX_ASSERT(fbt->sn != NULL);
//	}

	Rm_node_stale_file(fbt->sn);

//	PX_ASSERT(free(fbt->sn) == 0);
	fbt->sn = NULL;

	PX_UNLOCK(&g_RSC_table_m->mt);
}

void printfh_m()
{
//	GHashTableIter iter1;
//	gpointer key1, value1;
//	g_hash_table_iter_init(&iter1, g_RSC_table_m->tab);
//
//	while (g_hash_table_iter_next(&iter1, &key1, &value1))
//	{
//		file_block_table_m* fbt = (file_block_table_m*) value1;
//		if (fbt != NULL)
//		{
//			printf("%s\n", fbt->path);
//			printfh(fbt->fh_m);
//			GHashTableIter iter;
//			gpointer key, value;
//			g_hash_table_iter_init(&iter, fbt->tab);
//			while (g_hash_table_iter_next(&iter, &key, &value))
//			{
//				printbn(((BlockPointerNode_m*) value)->bpn);
//			}
//		} else
//		{
//			printf("not found!");
//		}
//		printf("\n");
//	}
//	printf(" ");
}
