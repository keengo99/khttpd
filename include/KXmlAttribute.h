#ifndef KHTTPD_KXMLATTRIBUTE_H_INCLUDED
#define KHTTPD_KXMLATTRIBUTE_H_INCLUDED
#include <map>
#include <string>
#include "KHttpLib.h"
#include "KStringBuf.h"

class KXmlAttribute : public std::map<KString, KString>
{
public:
	int64_t get_int64(const char* key, int64_t default_value = 0) const {
		auto it = find(key);
		if (it == end() || (*it).second.empty()) {
			return default_value;
		}
		return kgl_atol((u_char*)(*it).second.c_str(), (*it).second.size());
	}
	int get_int(const char* key, int default_value = 0) const {
		auto it = find(key);
		if (it == end() || (*it).second.empty()) {
			return default_value;
		}
		return atoi((*it).second.c_str());
	}
	const char* get_string(const char* key, const char* default_value = "") const {
		auto it = find(key);
		if (it == end()) {
			return default_value;
		}
		return (*it).second.c_str();
	}
	const char* operator()(const char* key, const char* default_value = "") const {
		return get_string(key, default_value);
	}
	const KString &operator[](const char* key) const {
		auto it = find(key);
		if (it == end()) {
			return empty;
		}
		return (*it).second;
	}
	const KString &operator[](const KString& key) const {
		auto it = find(key);
		if (it == end()) {
			return empty;
		}
		return (*it).second;
	}
	KString remove(const KString& key) {
		auto it = find(key);
		if (it == end()) {
			return empty;
		}
		KString ret(std::move((*it).second));
		erase(it);
		return ret;
	}
	static const KString empty;
private:
};
#endif
