#ifndef KHTTPD_DAVLIB_H
#define KHTTPD_DAVLIB_H 1
#include <string>
#include <list>
#include <KXmlDocument.h>
class KWebDavFile
{
public:
	std::string path;
	std::string etag;
	bool is_directory;
	int64_t content_length;
};
class KWebDavFileList
{
public:
	~KWebDavFileList()
	{
		clean();
	}
	void clean()
	{
		for (auto it = files.begin(); it != files.end(); it++) {
			delete (*it);
		}
		files.clear();
	}
	KWebDavFile* find(const char* name)
	{
		for (auto it = files.begin(); it != files.end(); it++) {
			if ((*it)->path == name) {
				return (*it);
			}
		}
		return nullptr;
	}
	bool parse(khttpd::KXmlDocument& document,int strip_prefix);
	std::list<KWebDavFile*> files;
};
#endif

