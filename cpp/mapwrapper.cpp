//#include <unordered_map>
//#include <unordered_set>
#include <set>
#include <iostream>
#include <string>
#include <cppx.h>
#include <unistd.h>


using namespace std;
//typedef map<string, BlockPointerNode> filemap_t;
void* mfs_rsc_tab; // this table will store mfs enabled for RSC
typedef set<string> mfs_rsc_tab_t;
#include <string.h>
#include <linux/limits.h>

bool readLine(FILE* fp, char* buf, int len)
{
	char c, prec='\n';
	int i = 0;
	memset(buf, 0, len);
	bool notend_f = false;
	bool valid_f = true;

	while ((c = fgetc(fp)) != EOF)
	{
		if (c == '\n')
		{
			notend_f = true;
			valid_f = true;
			break;
		}
		else if (c == '#')
		{
			notend_f = true;
			valid_f = false;
//			break;
		}
		else if (valid_f)
		{
			buf[i++] = c;
		}
		prec = c;
	}

	if (c == EOF && prec != '\n')
		notend_f = true;

	return notend_f;
}


extern "C" void Init_mfs_rsc_tab()
{
	mfs_rsc_tab = new mfs_rsc_tab_t();
//	Enable_mfs_rsc("slf");
	const char* fn = "/px/conf/mfs.rsc.conf";
	if (access(fn, F_OK) != -1)
	{
		FILE* fp = fopen(fn, "r");
		char buf[PATH_MAX];
		while (readLine(fp, buf, PATH_MAX))
		{
			Enable_mfs_rsc(buf);
		}
	}
}

extern "C" void Destroy_mfs_rsc_tab()
{
	mfs_rsc_tab_t* tab = (mfs_rsc_tab_t*)mfs_rsc_tab;
	delete tab;
}

extern "C" int Is_Enable_mfs_rsc(const char* path)
{
	mfs_rsc_tab_t* tab = (mfs_rsc_tab_t*)mfs_rsc_tab;
	if (tab->find(path) != tab->end())
		return 1;
	else
		return 0;
}

extern "C" void Enable_mfs_rsc(const char* path)
{
	mfs_rsc_tab_t* tab = (mfs_rsc_tab_t*)mfs_rsc_tab;
	tab->insert(path);
}

extern "C" void Disable_mfs_rsc(const char* path)
{
	mfs_rsc_tab_t* tab = (mfs_rsc_tab_t*)mfs_rsc_tab;
	tab->erase(path);
}
