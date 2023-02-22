#ifndef KHTTPD_KXMLATTRIBUTE_H_INCLUDED
#define KHTTPD_KXMLATTRIBUTE_H_INCLUDED
#include <map>
#include <string>

class KXmlAttribute : public std::map<std::string, std::string>
{
public:
	int get_int(const char* str) const {
		auto it = find(str);
		if (it == end()) {
			return 0;
		}
		return atoi((*it).second.c_str());
	}
	int get_int(const char* str, size_t len) const {
		return get_int(str);
	}
	const char* get_string(const char* key) const {
		auto it = find(key);
		if (it == end()) {
			return nullptr;
		}
		return (*it).second.c_str();
	}
	const char* operator()(const char* key) const {
		auto it = find(key);
		if (it == end()) {
			return "";
		}
		return (*it).second.c_str();
	}
	std::string operator[](const char* key) const {
		auto it = find(key);
		if (it == end()) {
			return "";
		}
		return (*it).second;
	}
	std::string operator[](const std::string& key) const {
		auto it = find(key);
		if (it == end()) {
			return "";
		}
		return (*it).second;
	}
};
#endif
