#include "KWebDavFileList.h"
#include "KXml.h"
#include "kstring.h"
#include "KHttpLib.h"


bool KWebDavFileList::parse(KXmlDocument& document,int strip_prefix)
{
	KXmlNode* node = document.getRootNode();
	if (node==nullptr || node->getTag() != "multistatus") {
		return false;
	}	
	for (auto response = node->getChild("response"); response != nullptr; response = response->getNext()) {
		KXmlNode* href = response->getChild("href");
		if (href == nullptr) {
			continue;
		}
		std::string path = href->getCharacter();
		if (path.size() <= strip_prefix) {
			continue;
		}
		KWebDavFile* file = new KWebDavFile;
		file->path = path.substr(strip_prefix);
		size_t pos = file->path.size();
		if (pos > 1) {
			std::string tail = file->path.substr(pos - 1);
			if (tail == "/") {
				//cut last char /
				file->path = file->path.substr(0, pos - 1);
			}
		}
		char* path_buf = strdup(file->path.c_str());
		url_decode(path_buf, (int)file->path.size(), NULL, false);
		file->path = path_buf;
		xfree(path_buf);
		auto propstat = response->getChild("propstat");
		if (propstat == nullptr) {
			delete file;
			continue;
		}
		auto status = propstat->getChild("status");
		if (status==nullptr || status->getCharacter().find_first_of(kgl_expand_string("200")) == std::string::npos) {
			delete file;
			continue;
		}
		auto content_length = propstat->getChild("prop/getcontentlength");
		if (content_length) {
			file->content_length = kgl_atol((u_char *)content_length->getCharacter().c_str(), content_length->getCharacter().size());
		} else {
			file->content_length = 0;
		}
		auto collection = propstat->getChild("prop/resourcetype/collection");
		if (collection) {
			file->is_directory = true;
		} else {
			file->is_directory = false;
		}
		auto etag = propstat->getChild("prop/getetag");
		if (etag != nullptr) {
			file->etag = etag->getCharacter();
		}
		files.push_back(file);
	}
	return true;
}
