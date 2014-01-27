#include "ini.h"
#include <stdio.h>
#include <string.h>


std::list<INI::Section> INI::load(const char *path) {
	std::list<Section> sections;

	FILE *f = fopen(path, "r");
	if (!f)
		return sections;

	Section *section = 0;

	for (;;) {
		char line[4096];
		if (!fgets(line, sizeof(line), f))
			break;

		int length = strlen(line);

		// Get rid of the newline
		if ((length > 0) && (line[length - 1] == '\n')) {
			line[length - 1] = 0;
			--length;
		}

		// Ignore empty lines, comments
		if (!length)
			continue;
		if (line[0] == ';')
			continue;
		if (line[0] == '#')
			continue;

		// New section?
		if ((length > 2) && (line[0] == '[') && (line[length - 1] == ']')) {
			line[length - 1] = 0;

			sections.push_back(Section());
			section = &sections.back();
			section->title = &line[1];

		} else if (section) {
			// We can only add values to a section if we have one :p

			char *eq = strchr(line, '=');
			if (eq) {
				*eq = 0;
				char *value = eq + 1;

				section->data[line] = value;
			}
		}
	}

	fclose(f);

	return sections;
}


bool INI::save(const char *path, const std::list<INI::Section> &sections) {
	FILE *f = fopen(path, "w");
	if (!f)
		return false;


	for (auto &section : sections) {
		fputc('[', f);
		fputs(section.title.c_str(), f);
		fputs("]\n\n", f);

		for (auto &i : section.data) {
			fputs(i.first.c_str(), f);
			fputc('=', f);
			fputs(i.second.c_str(), f);
			fputc('\n', f);
		}

		fputc('\n', f);
	}


	fclose(f);

	return true;
}



/*
int main(int argc, char **argv) {
	INI::Section a, b;

	a.title = "boop";
	a.data["a"] = "b";
	a.data["c"] = "d";

	b.title = "boop2";
	b.data["a"] = "b";
	b.data["c"] = "d";

	std::list<INI::Section> sections = { a, b };
	INI::save("tryme.ini", sections);

	std::list<INI::Section> munged = INI::load("tryme.ini");
	INI::save("tryme2.ini", munged);
}
*/
