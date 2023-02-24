#ifndef KHTTPD_KXMLATTRIBUTE_H_INCLUDED
#define KHTTPD_KXMLATTRIBUTE_H_INCLUDED
#include <map>
#include <string>
#include "KHttpLib.h"

class KXmlAttribute : public std::map<std::string, std::string>
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
	const char* get_string(const char* key, const char* default_value = nullptr) const {
		auto it = find(key);
		if (it == end()) {
			return default_value;
		}
		return (*it).second.c_str();
	}
	const char* operator()(const char* key, const char* default_value = "") const {
		auto it = find(key);
		if (it == end()) {
			return default_value;
		}
		return (*it).second.c_str();
	}
	const std::string &operator[](const char* key) const {
		auto it = find(key);
		if (it == end()) {
			return empty;
		}
		return (*it).second;
	}
	const std::string &operator[](const std::string& key) const {
		auto it = find(key);
		if (it == end()) {
			return empty;
		}
		return (*it).second;
	}
private:
	static const std::string empty;
};
#endif
