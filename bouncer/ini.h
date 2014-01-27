#ifndef INI_H
#define INI_H

#include <string>
#include <list>
#include <map>

namespace INI {
	struct Section {
		std::string title;
		std::map<std::string, std::string> data;
	};

	std::list<Section> load(const char *path);
	bool save(const char *path, const std::list<Section> &sections);
}

#endif /* INI_H */
