
#include "directory.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// File list sorting functor //

struct SortFiles {

	Directory::File* s;
	SortFiles (Directory::File* s) : s(s) {}

	bool operator()(const Directory::File& a, const Directory::File& b) const {

		if (a.type!=b.type) return a.type>b.type;	// List folders at the top?

		// Case insensitive matching
		//int r = strncmp(a.name, b.name, 8);
		const char* u=a.name;
		const char* v=a.name;

		while (*u && (*u == *v || *u+32==*v || *u==*v+32)) ++u, ++v;

		return *u - *v;
	}
};

Directory::Directory (const char* path) {

	strncpy (m_path, path, 2048);
	m_path[2047] = 0;
}

Directory::~Directory() {}

int Directory::scan() {								/** Scan directory for files */

	m_files.clear();

	DIR* dp;
	struct stat st;
	struct dirent *dirp;
	char buffer[2304];

	if ((dp = opendir (m_path))) {

		while ((dirp = readdir (dp))) {

			File file;
			strcpy (file.name, dirp->d_name);

			snprintf (buffer, 2304, "%s/%s", m_path, dirp->d_name);		// Is it a file or directory?

			stat (buffer, &st);

			if (S_ISDIR (st.st_mode)) file.type = DIRECTORY;
			else file.type = Directory::FILE;


			for (file.ext=0; file.name[file.ext]; ++file.ext) {			// Extract extension

				if(file.ext && file.name[file.ext-1]=='.') break;
			}

			m_files.push_back (file);
		}
	}

	std::sort (m_files.begin(), m_files.end(), SortFiles (&m_files[0]));

	return m_files.size();
}

bool Directory::contains (const char* file ) {

	if (m_files.empty()) scan();

	for (iterator i=m_files.begin(); i!=m_files.end(); i++) {

		if (strcmp (i->name, file)==0) return true;
	}

	return false;
}

bool isDirectory (const char* path) {

	struct stat st;
	stat (path, &st);
	return S_ISDIR (st.st_mode);
}
