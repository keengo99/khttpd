#include "KWebDavFileList.h"
#include "KXml.h"
#include "kstring.h"
#include "KHttpLib.h"


bool KWebDavFileList::parse(KXmlDocument& document,int strip_prefix)
{
	KXmlNode* node = document.getRootNode();
	if (node==nullptr || node->get_tag() != "multistatus") {
		return false;
	}
	auto response = node->find_child("response");
	if (!response) {
		return false;
	}
	for (uint32_t index = 0;;index++) {
		auto body = response->get_body(index);
		if (!body) {
			break;
		}
		KXmlNode* href = body->find_child("href");
		if (href == nullptr) {
			continue;
		}
		std::string path = href->get_text();
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
		auto propstat = body->find_child("propstat");
		if (propstat == nullptr) {
			delete file;
			continue;
		}
		auto status = propstat->find_child("status");
		if (status==nullptr || status->get_character().find_first_of(kgl_expand_string("200")) == std::string::npos) {
			delete file;
			continue;
		}
		auto content_length = propstat->find_child("prop/getcontentlength");
		if (content_length) {
			file->content_length = kgl_atol((u_char *)content_length->get_character().c_str(), content_length->get_character().size());
		} else {
			file->content_length = 0;
		}
		auto collection = propstat->find_child("prop/resourcetype/collection");
		if (collection) {
			file->is_directory = true;
		} else {
			file->is_directory = false;
		}
		auto etag = propstat->find_child("prop/getetag");
		if (etag != nullptr) {
			file->etag = etag->get_character();
		}
		files.push_back(file);
	}
	return true;
}
