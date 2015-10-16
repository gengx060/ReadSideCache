#include <RSC.h>

#ifdef __cplusplus
extern "C"
#endif
void Init_mfs_rsc_tab();
#ifdef __cplusplus
extern "C"
#endif
void Destroy_mfs_rsc_tab();
#ifdef __cplusplus
extern "C"
#endif
void Enable_mfs_rsc(const char* path);
#ifdef __cplusplus
extern "C"
#endif
void Disable_mfs_rsc(const char* path);
#ifdef __cplusplus
extern "C"
#endif
int Is_Enable_mfs_rsc(const char* path);

#ifdef __cplusplus
extern "C"
#endif
void* RSC_tab_init();
#ifdef __cplusplus
extern "C"
#endif
void RSC_tab_destroy(void* rsctab);
#ifdef __cplusplus
extern "C"
#endif
void* RSC_tab_lookup(void* rsctab, const char* path);
#ifdef __cplusplus
extern "C"
#endif
void RSC_tab_insert(void* rsctab, const char* path, file_block_table_m* fbt);
#ifdef __cplusplus
extern "C"
#endif
file_block_table_m* RSC_tab_remove(void* rsctab, const char* path);
#ifdef __cplusplus
extern "C"
#endif
size_t RSC_tab_size(void* rsctab);

#ifdef __cplusplus
extern "C"
#endif
void* Block_tab_init();
#ifdef __cplusplus
extern "C"
#endif
void Block_tab_destroy(void* blocktab);
#ifdef __cplusplus
extern "C"
#endif
void* Block_tab_lookup(void* blocktab, off_t blockIndex);
#ifdef __cplusplus
extern "C"
#endif
void Block_tab_insert(void* blocktab, off_t blockIndex, BlockPointerNode_m* bpm_p);
#ifdef __cplusplus
extern "C"
#endif
CNode* Block_tab_truncate(off_t blockIndex, void* tab);

#ifdef __cplusplus
extern "C"
#endif
void* Prefetch_tab_init();
#ifdef __cplusplus
extern "C"
#endif
void Prefetch_tab_destroy(void* pretab);
#ifdef __cplusplus
extern "C"
#endif
void* Prefetch_tab_lookup(void* pretab, const char* path);
#ifdef __cplusplus
extern "C"
#endif
void Prefetch_tab_insert(void* pretab, const char* path, FetchQueue* fq);
#ifdef __cplusplus
extern "C"
#endif
void Prefetch_tab_remove(void* pretab, const char* path);
#ifdef __cplusplus
extern "C"
#endif
size_t Prefetch_tab_size(void* pretab);

#ifdef __cplusplus
extern "C"
#endif
void printStaleList();
#ifdef __cplusplus
extern "C"
#endif
void Rm_node_stale_file(StaleNode* sn);
#ifdef __cplusplus
extern "C"
#endif
void Release_cnode_list(CNode* cnlist);
