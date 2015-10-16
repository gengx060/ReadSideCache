#include <RSC.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <cppx.h>

extern void printcachedfiles();

int main(int argc, char **argv)
{
	g_RSC_table_m = (RSC_table_m*) calloc((size_t) 1, sizeof(RSC_table_m));
	PX_ASSERT(g_RSC_table_m != NULL);
//	g_RSC_table_m->tab = g_hash_table_new_full(g_str_hash, g_str_equal, Cleanup,
//			(void*) Destory_file_block_table); // Destory_file_block_table will free key space
	g_RSC_table_m->tab = RSC_tab_init();
	PX_ASSERT(g_RSC_table_m->tab != NULL);

	PX_ASSERT(pthread_mutexattr_init(&g_recursive_mt_attr) == 0);
	PX_ASSERT(
			pthread_mutexattr_settype(&g_recursive_mt_attr,
					PTHREAD_MUTEX_RECURSIVE) == 0);
	PX_ASSERT(pthread_mutex_init(&g_RSC_table_m->mt, &g_recursive_mt_attr) == 0);
	PX_ASSERT(pthread_mutex_init(&g_RSC_freelist_mt, &g_recursive_mt_attr) == 0);

	int numcache;
	readrscconf(CACHE_FILE, g_logfile, NULL, &RSCBLKSIZE, &numcache);
	if (0 != access(CACHE_FILE, F_OK))
	{
		fprintf(stderr, "read side cache file %s does not exist!\n",
		CACHE_FILE);
		PX_ASSERT(0);
	}

#ifdef USEFREAD
	PX_ASSERT(0 == access(CACHE_FILE, F_OK));
	g_cache_fp = fopen(CACHE_FILE, "r+b");
#else
	PX_ASSERT(0 == access(CACHE_FILE, F_OK));
	g_cache_fp = open(CACHE_FILE, O_RDWR);
#endif
#ifdef USINGRSCLOG
	g_log_fp = fopen(g_logfile, "a+");
#endif
	SetGlobalSection();
	ReadWholeDiskToRSCTable(PX_FALSE);
//	PX_ASSERT(fclose(g_cache_fp) == 0);
//	return 0;
//
//	g_cache_fp = fopen(CACHE_FILE, "r+b");

	while (1)
	{
		printf("1. print blockheader\n"
				"2. print whole cache\n"
				"3. print blockheader freelist\n"
				"4. print fileheader freelist\n"
				"5. space left\n"
				"6. print cached filenames\n"
				"7. print cached file\n"
				"8. clear cache\n"
				"0. exit\n");
		int com;
		scanf("%d", &com);
		if (com == 1)
		{
			printf(" printWholeBlockPointer()\n");
			printWholeBlockPointer();
		}
		else if (com == 2)
		{
			printf(" printWholeDisk()\n");
			printWholeDisk();
		}
		else if (com == 3)
		{
			printf(" printbnfreelist()\n");
			printbnfreelist();
		}
		else if (com == 4)
		{
			printf(" printfileheaderfreelist()\n");
			printfileheaderfreelist();
		}
		else if (com == 5)
		{
			printf(" space left\n");
			printspaceleft();
		}
		else if (com == 6)
		{
			printf(" print cached filenames\n");
			printcachedfiles();
		}
		else if (com == 8)
		{
//			printf("size %u\n", RSC_tab_size(g_RSC_table_m->tab));
//			printf("clear cache\n");
////			g_hash_table_remove_all(g_RSC_table_m->tab);
//			GHashTableIter iter;
//			gpointer key, value;
//			g_hash_table_iter_init(&iter, g_RSC_table_m->tab);
//			int i = 0;
////		 	g_hash_table_remove (g_RSC_table_m->tab,"/px/mfs/10.0.10.48/media/gai_NFS1/dircp/100M1");
//			while (g_hash_table_iter_next (&iter, &key, &value))
//			{
//					g_hash_table_iter_remove(&iter);
//					i++;
//			}
//			printf("size %u and i is %d\n", RSC_tab_size(g_RSC_table_m->tab), i);
////			g_hash_table_maybe_resize(g_RSC_table_m->tab);
		}
		else if (com == 9)
		{
			printf("clear cache\n");
//			g_hash_table_remove_all(g_RSC_table_m->tab);
//			g_hash_table_destroy(g_RSC_table_m->tab);
		}
		else if (com == 0)
		{
			break;
		}
		else
			break;
	}

#ifdef USINGRSCLOG
	PX_ASSERT(fclose(g_log_fp) == 0);
#endif
	PX_ASSERT(close(g_cache_fp) == 0);

	return 0;
}
