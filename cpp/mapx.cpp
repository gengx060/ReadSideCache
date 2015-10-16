#include <string>
#include <unordered_map>
#include <iostream>
#include <RSC.h>
#include <cppx.h>
#include <stdlib.h>

using namespace std;

typedef unordered_map<off_t, BlockPointerNode_m*> blockmap;
typedef unordered_map<string, file_block_table_m*> rscmap;
typedef unordered_map<string, FetchQueue*> prefetchtab;

void FreeBPM(BlockPointerNode_m* bpm_p)
{
	free(bpm_p->bpn);
	free(bpm_p);
}

extern "C" void* RSC_tab_init()
{
	return new rscmap();
}

extern "C" void RSC_tab_destroy(void* rsctab)
{
	rscmap* tab = (rscmap*) rsctab;

//	for (auto it : tab)
	for (auto it=tab->begin(); it!=tab->end(); ++it)
	{
		file_block_table_m* fbt = (file_block_table_m*)it->second;
		Block_tab_destroy(fbt->tab);
		free(fbt->fh_m);
		Rm_node_stale_file(fbt->sn);
		assert(pthread_mutex_destroy(&fbt->mt) == 0);
		free(fbt);
	}
	delete tab;
}

extern "C" void* RSC_tab_lookup(void* rsctab, const char* path)
{
	rscmap* tab = (rscmap*) rsctab;

	auto it = tab->find(path);
	if (it == tab->end())
	{
		return NULL;
	}
	else
	{
		return it->second;
	}
}

extern "C" void RSC_tab_insert(void* rsctab, const char* path, file_block_table_m* fbt)
{
	rscmap* tab = (rscmap*) rsctab;

	tab->insert({path, fbt});
}

extern "C" file_block_table_m* RSC_tab_remove(void* rsctab, const char* path)
{
	rscmap* tab = (rscmap*) rsctab;
	auto it = tab->find(path);

	if (it != tab->end())
	{
		file_block_table_m* fbt = (file_block_table_m*) (it->second);
		tab->erase(path);
//		free(fbt->fh_m);
//		free(fbt->path);
		return fbt;
	}
	return NULL;

}

extern "C" size_t RSC_tab_size(void* rsctab)
{
	rscmap* tab = (rscmap*) rsctab;
	if (tab == NULL)
	{
		return 0;
	}
	else
	{
		return tab->size();
	}
}

extern "C" void* Block_tab_init()
{
	return new blockmap();
}

extern "C" void Block_tab_destroy(void* blocktab)
{
	blockmap* tab = (blockmap*) blocktab;
//	for (auto it : tab)
	for (auto it=tab->begin(); it!=tab->end(); ++it)
	{
		FreeBPM(it->second);
	}
	delete tab;
}

extern "C" void* Block_tab_lookup(void* blocktab, off_t blockIndex)
{
	blockmap* tab = (blockmap*) blocktab;

	auto it = tab->find(blockIndex);
	if (it == tab->end())
	{
		return NULL;
	}
	else
	{
		return it->second;
	}
}

extern "C" void Block_tab_insert(void* blocktab, off_t blockIndex, BlockPointerNode_m* bpm_p)
{
	blockmap* tab = (blockmap*) blocktab;

//	cout<<"blockIndex: "<<*(bpm_p->bpn);
	tab->insert({blockIndex, bpm_p});
//	cout<<": "<< tab->size()<<'\n';
}

extern "C" void Block_tab_remove(void* blocktab, off_t blockIndex)
{
	blockmap* tab = (blockmap*) blocktab;

	auto it = tab->find(blockIndex);
	if (it != tab->end())
	{
		BlockPointerNode_m* bpm_p = it->second;
		FreeBPM(bpm_p);
		tab->erase(it);
	}
}

extern "C" void Release_cnode_list(CNode* cnlist)
{
	while (cnlist != NULL)
	{
		CNode* node = cnlist;
		cnlist = cnlist->next;
		delete node;
	}
}

//extern "C" void Block_tab_truncate(void* blocktab, off_t blockIndex, file_block_table_m* fbt)
extern "C" CNode* Block_tab_truncate(off_t blockIndex, void* blocktab)
{
	CNode* re_node = NULL;
	CNode* node = NULL;
	blockmap* tab = (blockmap*) blocktab;

//	cout<< tab->size()<<endl;
	for (auto it = tab->cbegin(); it != tab->cend() /* not hoisted */; /* no increment */)
	{
		if (it->first > blockIndex)
		{
			BlockPointerNode_m* bpm_p = (BlockPointerNode_m*) it->second;
//			node = new CNode(bpm_p);
			if (re_node == NULL)
			{
				node = new CNode(bpm_p);
				re_node = node;
			}
			else
			{
				node->next = new CNode(bpm_p);
				node = node->next;
			}
			tab->erase(it++);
		}
		else
		{
			it++;
		}
	}

	return re_node;
}

extern "C" void* Prefetch_tab_init()
{
	return new prefetchtab();
}

//extern "C" void Prefetch_tab_destroy(void* pretab)
//{
//	prefetchtab* tab = (prefetchtab*) pretab;
//	delete tab;
//}

extern "C" void* Prefetch_tab_lookup(void* pretab, const char* path)
{
	prefetchtab* tab = (prefetchtab*) pretab;

	auto it = tab->find(path);
	if (it == tab->end())
	{
		return NULL;
	}
	else
	{
		return it->second;
	}
}

extern "C" void Prefetch_tab_insert(void* pretab, const char* path, FetchQueue* fq)
{
	prefetchtab* tab = (prefetchtab*) pretab;
//	cout<<"\n";
	tab->insert({path, fq});
//	cout<<tab->size()<<" | "<<fq->filesize;
//	cout<<"\n";
}

extern "C" void Prefetch_tab_remove(void* pretab, const char* path)
{
	prefetchtab* tab = (prefetchtab*) pretab;
	auto it = tab->find(path);

	if (it != tab->end())
	{
		FetchQueue* fq = (FetchQueue*) (it->second);
		free(fq);
		tab->erase(it);
	}
}

extern "C" void Prefetch_tab_destroy(void* pretab)
{
	prefetchtab* tab = (prefetchtab*) pretab;
//	int i = 0;
//	for (auto it = tab->cbegin(); it != tab->cend() /* not hoisted */; it++)
//	{
//		cout<<"\n"<<i++<<"  || ";
//		FetchQueue* fq = (FetchQueue*) (it->second);
//		cout<<it->first<<" | "<< fq->filesize <<endl;
//	}
//	for (auto it = tab->begin(); it != tab->end() /* not hoisted */; /* no increment */)
//	{
//		FetchQueue* fq = (FetchQueue*) (it->second);
//		free(fq);
//		tab->erase(it++);
//	}
	delete tab;
}

extern "C" size_t Prefetch_tab_size(void* pretab)
{
	prefetchtab* tab = (prefetchtab*) pretab;
	if (tab == NULL)
	{
		return 0;
	}
	else
	{
		return tab->size();
	}
}

extern "C" void printStaleList()
{
#ifdef DBSTALELIST
	PX_LOCK(&g_stale_list.mt);
	printf("\n\n------------------------start----------------------------\n");
	StaleNode* sn = g_stale_list.header;
	while(sn != g_stale_list.tail)
	{
		printf("appended file name is %s\n", sn->fbt->path);
		sn = sn->next;
	}
	if (sn == g_stale_list.tail && sn != NULL)
	{
		printf("appended file name is %s\n", sn->fbt->path);
	}
//	printspaceleft();
	printf("---------------------------end-------------------------\n\n");
	PX_UNLOCK(&g_stale_list.mt);
#endif
}

extern "C" void Rm_node_stale_file(StaleNode* sn)
{
	if (sn == NULL)
		return;
	PX_LOCK(&g_stale_list.mt);

	//	StaleNode* sn = fbt->sn;
	assert(sn->fbt != NULL);
	printf("\nafter Rm_file_from_stale_list ");
	printStaleList();

	sn->fbt = NULL; // this will mark stale node is not in stale list

	if(g_stale_list.tail == g_stale_list.header)
	{
		if(g_stale_list.tail != NULL)
		{
			assert(g_stale_list.tail->next == NULL);
			assert(g_stale_list.header->pre == NULL);
			g_stale_list.tail = NULL;
			g_stale_list.header = NULL;
		}
		free(sn);

		PX_UNLOCK(&g_stale_list.mt);
//		PX_UNLOCK(&g_RSC_table_m->mt);
		return;
	}

	if(g_stale_list.tail == sn)
	{
		g_stale_list.tail = g_stale_list.tail->pre;
		g_stale_list.tail->next = NULL;
	}
	else
	{
		sn->next->pre = sn->pre;
	}

	if(g_stale_list.header == sn)
	{
		g_stale_list.header = sn->next;
		g_stale_list.header->pre = NULL;
	}
	else
	{
		sn->pre->next = sn->next;
	}

	free(sn);

	PX_UNLOCK(&g_stale_list.mt);
}
