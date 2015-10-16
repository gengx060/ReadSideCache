
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sched.h>
#include <pxcache.h>
#include <sys/stat.h>
#include <RSC.h>
#include <cppx.h>

void CMHDone(cmh_t* cmh)
{
#ifndef PXHSMPOLLING
	PX_LOCK(&cmh->prefechsignal->mt);

	cmh->prefechsignal->done = PX_TRUE;
	PX_ASSERT(pthread_cond_signal(&cmh->prefechsignal->cv)==0);

	PX_UNLOCK(&cmh->prefechsignal->mt);
#endif
}

#ifdef PXHSMPOLLING
void ReadCMHToCache(cmh_t* cmh, off_t filesize)
{
	PX_LOCK(&g_RSC_table_m->mt);
	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, cmh->rscpath);
	PX_UNLOCK(&g_RSC_table_m->mt);
	if (fbt != NULL)
	{
		if(fbt->fh_m->fileoff != filesize)
		{
			Rm_file_block_table(cmh->rscpath);
		}
		else
		{
			if(cmh->rscstatus != NULL)
			{
				cmh->rscstatus->nbytes = filesize;
				cmh->rscstatus->status = 0;
			}
			CMHDone(cmh);
			return;
		}
		if(cmh->rscstatus != NULL)
		{
			cmh->rscstatus->nbytes = 0;
			cmh->rscstatus->status = EBUSY;
		}
	}

	char buf[RSCBLKSIZE+1];
	off_t i = 0;
	while (PX_TRUE)
	{
//		checkbusy();
		if (filesize == 0)
		{
			break;
		}

		if (i + RSCBLKSIZE < filesize)
		{
			bzero(buf, RSCBLKSIZE + 1);
			ssize_t nread = (*cmh->cmctx->fuseops.read)(cmh->path, buf, RSCBLKSIZE, i, &cmh->ffi);
			if (nread!=RSCBLKSIZE)
			{
				CMHDone(cmh);
				if(cmh->rscstatus != NULL)
				{
					cmh->rscstatus->status = errno;
				}
				return;
			}
			Insert_RSC_table(cmh->rscpath, buf, RSCBLKSIZE, i, KEEPCURRENT_F);
			i += RSCBLKSIZE;

			if(cmh->rscstatus != NULL)
			{
				cmh->rscstatus->nbytes = i;
			}
		}
		else
		{
			bzero(buf, RSCBLKSIZE + 1);
			size_t nread = (*cmh->cmctx->fuseops.read)(cmh->path, buf, filesize - i, i, &cmh->ffi);
			if (nread!=filesize - i)
			{
				CMHDone(cmh);

				if(cmh->rscstatus != NULL)
				{
					cmh->rscstatus->status = errno;
				}
				return;
			}
			Insert_RSC_table(cmh->rscpath, buf, filesize - i, i, KEEPCURRENT_F);

			if(cmh->rscstatus != NULL)
			{
				cmh->rscstatus->nbytes = filesize;
			}
			break;
		}
	}

	if(cmh->rscstatus != NULL)
	{
		cmh->rscstatus->status = 0;
	}
	CMHDone(cmh);
}
#else

void ReadCMHToCache(cmh_t* cmh, off_t filesize)
{
	PX_LOCK(&g_RSC_table_m->mt);
//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, cmh->rscpath);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, cmh->rscpath);
	PX_UNLOCK(&g_RSC_table_m->mt);
	if (fbt != NULL)
	{
		if(fbt->fh_m->fileoff != filesize)
		{
			Rm_file_block_table(cmh->rscpath);
		}
		else
		{
			if(cmh->rscstatus != NULL)
			{
				cmh->rscstatus->nbytes = filesize;
				cmh->rscstatus->status = 0;
				cmh->rscstatus = NULL;
			}
			CMHDone(cmh);
			return;
		}
		if(cmh->rscstatus != NULL)
		{
			cmh->rscstatus->nbytes = 0;
			cmh->rscstatus->status = EBUSY;
		}
	}

	char buf[RSCBLKSIZE+1];
	off_t i = 0;

//	int fh = open(cmh->rscpath, O_RDONLY);
	while (!cmh->rscstatus->flags)
	{
//		checkbusy();
		if (filesize == 0)
		{
			break;
		}

		if (i + RSCBLKSIZE < filesize)
		{
			bzero(buf, RSCBLKSIZE + 1);
			ssize_t nread = (*cmh->cmctx->fuseops.read)(cmh->path, buf, RSCBLKSIZE, i, &cmh->ffi);
//			ssize_t nread = pread(fh, buf, RSCBLKSIZE, i);
			if (nread!=RSCBLKSIZE)
			{
				if(cmh->rscstatus != NULL)
				{
					cmh->rscstatus->status = errno;
					cmh->rscstatus = NULL;
				}
				CMHDone(cmh);
				return;
			}
			Insert_RSC_table(cmh->rscpath, buf, RSCBLKSIZE, i, KEEPCURRENT_F);
			i += RSCBLKSIZE;

			if(cmh->rscstatus != NULL)
			{
				cmh->rscstatus->nbytes = i;
			}
		}
		else
		{
			bzero(buf, RSCBLKSIZE + 1);
			size_t nread = (*cmh->cmctx->fuseops.read)(cmh->path, buf, filesize - i, i, &cmh->ffi);
			if (nread!=filesize - i)
			{
				if(cmh->rscstatus != NULL)
				{
					cmh->rscstatus->status = errno;
					cmh->rscstatus = NULL;
				}
				CMHDone(cmh);
				return;
			}
			Insert_RSC_table(cmh->rscpath, buf, filesize - i, i, KEEPCURRENT_F);

			if(cmh->rscstatus != NULL)
			{
				cmh->rscstatus->nbytes = filesize;
			}
			break;
		}
	}

	if(cmh->rscstatus != NULL)
	{
		cmh->rscstatus->status = 0;
		cmh->rscstatus = NULL;
	}
	CMHDone(cmh);
//	PX_ASSERT(close(fh) == 0);
}
#endif

void QueueFileToRSC(const struct stat *stbuf, const char* path)
{
	if (S_ISREG(stbuf->st_mode))
	{
		FetchQueue* fq = NULL;

		PX_LOCK(&g_fetchworker->g_fft->mt);
		if (Prefetch_tab_lookup(g_fetchworker->g_fft->tab, path) == NULL)
		{
			char* rscpath = strdup(path);
			fq = calloc(1l, sizeof(FetchQueue));
//			fq->rscpath = rscpath;
//			fq->cmh = cmh;
//			fq->flag = fq;
//			fq->flag = 	calloc(1l, sizeof(char));
			fq->filesize = stbuf->st_size;
//			g_hash_table_insert(g_fetchworker->g_fft->tab, rscpath, fq);
			Prefetch_tab_insert(g_fetchworker->g_fft->tab, rscpath, fq);
		}
		PX_UNLOCK(&g_fetchworker->g_fft->mt);

		if (fq != NULL)
		{
			int i = Prefetch_tab_size(g_fetchworker->g_fft->tab) % NUMFETCHWORKER;
			FetchWorker* fw = g_fetchworker + i;

			PX_LOCK(&fw->mt);
			if(fw->head == NULL)
			{
				fw->head = fq;
				fw->tail = fq;
			}
			else
			{
				fw->tail->next = fq;
				fw->tail = fw->tail->next;
			}
			pthread_cond_signal(&fw->cv);
			PX_UNLOCK(&fw->mt);
		}
	}
}

/* queuec cmh with filesize
 * check if cmh is new
 * if cmh is new, then queue it to prefetch thread */
void QueueCMHToRSC_fsize(const off_t filesize, void* cmht)
{

	cmh_t* cmh = (cmh_t*)cmht;
	FetchQueue* fq = NULL;

	PX_LOCK(&g_fetchworker->g_fft->mt);
//		check if cmh is new
	void* keyfound = Prefetch_tab_lookup(g_fetchworker->g_fft->tab, cmh->rscpath);
	if (keyfound == NULL)
	{
//		char* rscpath = strdup(cmh->rscpath);
		fq = calloc(1l, sizeof(FetchQueue));
		fq->cmh = cmh;
		fq->filesize = filesize;
//		g_hash_table_insert(g_fetchworker->g_fft->tab, rscpath, fq);
		Prefetch_tab_insert(g_fetchworker->g_fft->tab, cmh->rscpath, fq);
	}
	PX_UNLOCK(&g_fetchworker->g_fft->mt);

//		if cmh is new, then queue it to prefetch thread
	if (fq != NULL)
	{
		int i = Prefetch_tab_size(g_fetchworker->g_fft->tab) % NUMFETCHWORKER;
		FetchWorker* fw = g_fetchworker + i;

		PX_LOCK(&fw->mt);
		if(fw->head == NULL)
		{
			fw->head = fq;
			fw->tail = fq;
		}
		else
		{
			fw->tail->next = fq;
			fw->tail = fw->tail->next;
		}
		pthread_cond_signal(&fw->cv);
		PX_UNLOCK(&fw->mt);
	}
	else // this file is already cached
	{
		CMHDone(cmh);
	}
}

/* queuec cmh with stat
 * check if cmh is new
 * if cmh is new, then queue it to prefetch thread */
void QueueCMHToRSC(const struct stat *stbuf, void* cmht)
{
	if (S_ISREG(stbuf->st_mode))
	{
		QueueCMHToRSC_fsize(stbuf->st_size, cmht);
	}
}


void* fetchworkercmh(void* arg)
{
	pthread_attr_t thAttr;
	int policy = 0;
	pthread_attr_init(&thAttr);
	pthread_attr_getschedpolicy(&thAttr, &policy);
	pthread_setschedprio(pthread_self(), sched_get_priority_min(policy));

	FetchWorker* fw = (FetchWorker*)arg;

	while(!fw->exit)
	{
		FetchQueue* mfsitem = NULL;

		PX_LOCK(&fw->mt);
		while(fw->head == NULL)
		{
			pthread_cond_wait(&fw->cv, &fw->mt);
			if(fw->exit)
			{
				PX_UNLOCK(&fw->mt);
				break;
			}
		}

		mfsitem = fw->head;
		if (mfsitem != NULL)
		{
			fw->head = fw->head->next;
			PX_UNLOCK(&fw->mt);

			/*---------process write back queue---------*/
			ReadCMHToCache(mfsitem->cmh, mfsitem->filesize);

			free(mfsitem);
		}
		else
		{
			fw->tail = NULL;
			PX_UNLOCK(&fw->mt);
		}

		continue;

	}
	pthread_exit(NULL);
}

void checkbusy()
{
	PX_LOCK(&g_fetchworker->mt);
	while(g_fetchworker->used)
		pthread_cond_wait(&g_fetchworker->cv, &g_fetchworker->mt);
	PX_UNLOCK(&g_fetchworker->mt);
}

void ReadFileToCache(const char *path, off_t size)
{
	if(size < 1)
	{
		return;
	}

	int fh = open(path, O_RDONLY);

	if (fh == -1)
	{
		printf("path %s not found!\n", path);
		return;
	}

	PX_ASSERT(fh != -1);

	PX_LOCK(&g_RSC_table_m->mt);
//	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, path);
	file_block_table_m* fbt = RSC_tab_lookup(g_RSC_table_m->tab, path);
	PX_UNLOCK(&g_RSC_table_m->mt);
	if (fbt != NULL)
	{
		return;
	}

	size_t filesize = size;

	void* buf = malloc(RSCBLKSIZE + 1);

	off_t i = 0;
	while (PX_TRUE)
	{
		checkbusy();
		if (filesize == 0)
		{
			break;
		}

		if (i + RSCBLKSIZE < filesize)
		{
			bzero(buf, RSCBLKSIZE + 1);
			size_t nread = pread(fh, buf, RSCBLKSIZE, i);
			PX_ASSERT(nread==RSCBLKSIZE);
			Insert_RSC_table(path, buf, RSCBLKSIZE, i, KEEPCURRENT_F);
			i += RSCBLKSIZE;
		}
		else
		{
			bzero(buf, RSCBLKSIZE + 1);
			size_t nread = pread(fh, buf, filesize - i, i);
			PX_ASSERT(nread==filesize - i);
			Insert_RSC_table(path, buf, filesize - i, i, KEEPCURRENT_F);
			break;
		}
	}
	free(buf);
	PX_ASSERT(close(fh) == 0);
}

void fetchDir(const char *dirname)
{
	DIR *dir;
	struct dirent *entry;

	if (!(dir = opendir(dirname)))
		return;
	if (!(entry = readdir(dir)))
		return;

	do
	{
		checkbusy();
		char path[PATH_MAX];
		if (entry->d_type == DT_DIR)
		{
			int len = snprintf(path, sizeof(path) - 1, "%s/%s", dirname,
					entry->d_name);
			path[len] = 0;
			if (strcmp(entry->d_name, ".") == 0
					|| strcmp(entry->d_name, "..") == 0)
				continue;
			fetchDir(path);
		}
		else if(entry->d_type == DT_REG)
		{
			sprintf(path, "%s/%s", dirname, entry->d_name);
			ReadFileToCache(path, 0);
		}

		printf("%s\n", path);
//		usleep(200);
	} while ( (entry = readdir(dir)) != NULL);
	closedir(dir);
}

static PathEntry* parseentry(char* buf, PathType pathtype, char*tmp)
{
	PathEntry* pet = NULL;

	if (pathtype == PATHDIR_T)
		readentry(buf, "DIR", tmp);
	else if (pathtype == PATHFILE_T)
		readentry(buf, "FILE", tmp);

	if (strlen(tmp) > 0)
	{
		pet = (PathEntry*)calloc(1l, sizeof(PathEntry));
		pet->pt = pathtype;
		strcpy(pet->path, tmp);
		pet->next = NULL;
	}

	return pet;
}

PathEntry* readfetchconf() // (PathEntry* pe_p)
{
	FILE* fp = fopen("/px/conf/fetch.conf", "r");
	if(fp == NULL)
	{
		return NULL;
	}

	PathEntry* pe = NULL;
	PathEntry* petail = NULL;

	char buf[PATH_MAX];
	while (readLine(fp, buf))
	{
		char tmp[PATH_MAX];
		PathEntry* pet = NULL;
		if((pet = parseentry(buf, PATHDIR_T, tmp)) == NULL)
			pet = parseentry(buf, PATHFILE_T, tmp);

		if(pe == NULL)
		{
			pe = pet;
			petail = pet;
		}
		else
		{
			if(petail)
			{
				petail->next = pet;
				petail = pet;
			}
		}
		continue;
	}

	PX_ASSERT(fclose(fp) == 0);

	return pe;
}

void Block_fetch_thread()
{
	PX_LOCK(&g_fetchworker->mt);
	if(!g_fetchworker->used)
	{
		g_fetchworker->used = 1;
	}
}

void Wakeup_fetch_thread()
{
	g_fetchworker->used = 0;
	pthread_cond_signal(&g_fetchworker->cv);
	PX_UNLOCK(&g_fetchworker->mt);
}

static void* fetchworker(void* arg)
{
	pthread_attr_t thAttr;
	int policy = 0;
	pthread_attr_init(&thAttr);
	pthread_attr_getschedpolicy(&thAttr, &policy);
	pthread_setschedprio(pthread_self(), sched_get_priority_min(policy));
	PX_ASSERT(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)==0);


	FetchWorker* fw = (FetchWorker*)arg;

//	PathEntry* pe = NULL;
	while(!fw->exit)
	{
		FetchQueue* mfsitem;

		PX_LOCK(&fw->mt);
		while(fw->head == NULL)
		{
			pthread_cond_wait(&fw->cv, &fw->mt);
			if(fw->exit)
			{
				PX_UNLOCK(&fw->mt);
				break;
			}
		}

		mfsitem = fw->head;
		if (mfsitem != NULL)
		{
			fw->head = fw->head->next;
			PX_UNLOCK(&fw->mt);

			/*---------process write back queue---------*/
			ReadFileToCache(mfsitem->rscpath, mfsitem->filesize);
//			*(mfsitem->flag) = '1' ;

			free(mfsitem);
		}
		else
		{
			fw->tail = NULL;
			PX_UNLOCK(&fw->mt);
		}

		continue;

#ifdef USEFETCHCONF
		pe = readfetchconf();
		while (pe != NULL)
		{
			if (pe->pt == PATHDIR_T)
			{
				fetchDir(pe->path);
			}
			else if (pe->pt == PATHFILE_T)
			{
				ReadFileToCache(pe->path);
			}
			PathEntry* freepe = pe;
			pe = pe->next;
			free(freepe);
		}

		printf("done fetchworker!\n");
		sleep(FETCHWORKERSLEEPTIME);
#endif
	}
	pthread_exit(NULL);
}


void Finit_fetch_thread()
{
	FetchWorker* fw = g_fetchworker;
//	fw->que = NULL;

	int i=0;
	while(i < NUMFETCHWORKER)
	{
//		PX_ASSERT(pthread_cancel(fw->tid)==0); // dont use pthread cancel in here
		PX_LOCK(&fw->mt);
		fw->exit = 1;
		pthread_cond_signal(&fw->cv);
		PX_UNLOCK(&fw->mt);
		fw++;
		i++;
	}

//	g_hash_table_destroy(g_fft->tab);
	Prefetch_tab_destroy(g_fft->tab);
	free(g_fft);
	i = 0;
	fw = g_fetchworker;
	while(i < NUMFETCHWORKER)
	{
		PX_ASSERT(pthread_join(fw->tid, NULL) == 0);
//		PX_ASSERT(pthread_mutex_destroy(&fw->mt) == 0);
//		PX_ASSERT(pthread_cond_destroy(&fw->cv) == 0);
		fw++;
		i++;
	}
//	free(g_fetchworker); //called in Destory_RSC_table_m function
}

void Init_fetch_thread()
{
	g_fetchworker = (FetchWorker*) calloc(NUMFETCHWORKER, sizeof(FetchWorker));

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	g_fft = (Fetch_files_table*) calloc(1l, sizeof(Fetch_files_table));
//	g_fft->tab = g_hash_table_new_full(g_str_hash, g_str_equal, free,
//			NULL /*use NULL because value points to key ...*/); // Destory_file_block_table will free key space
	g_fft->tab = Prefetch_tab_init(); // Destory_file_block_table will free key space
	PX_ASSERT(pthread_mutex_init(&g_fft->mt, NULL) == 0);

	FetchWorker* fw = g_fetchworker;
	int i=0;
	while(i < NUMFETCHWORKER)
	{
#ifdef READFILETORSC
		PX_ASSERT(pthread_create(&fw->tid, &attr, fetchworker, fw) == 0);
#else
		PX_ASSERT(pthread_mutex_init(&fw->mt, NULL) == 0);
		PX_ASSERT(pthread_cond_init(&fw->cv, NULL) == 0);
		fw->used = 0;
		fw->exit = 0;
		fw->id = i;
		fw->g_fft = g_fft;
		PX_ASSERT(pthread_create(&fw->tid, &attr, fetchworkercmh, fw) == 0);
#endif

		fw++;
		i++;
	}
}

void Init_fetch_thread1()
{
	g_fetchworker = (FetchWorker*) calloc(1l, sizeof(FetchWorker));
	PX_ASSERT(pthread_create(&g_fetchworker->tid, NULL, fetchworker, g_fetchworker) == 0);
	PX_ASSERT(pthread_mutex_init(&g_fetchworker->mt, NULL) == 0);
	PX_ASSERT(pthread_cond_init(&g_fetchworker->cv, NULL) == 0);
	g_fetchworker->used = 0;
}
//#endif
