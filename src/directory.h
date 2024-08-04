#ifndef _DIRECTORY_
#define _DIRECTORY_

#include <vector>

/** Directory class for listing files in a directory */

class Directory {

	public:

		enum FileType { FILE, DIRECTORY };

		Directory (const char* path=".");
		~Directory();

		bool contains(const char* file);						/** Test if a file is in this directory */

		const char* path() const { return m_path; }				/** Get the current path */

		struct File { char name[128]; int ext; int type; };		/// Iterator ///

		typedef std::vector<File>::const_iterator iterator;

		iterator begin()       { scan(); return m_files.begin(); }
		iterator end() const   { return m_files.end(); }
		
	protected:

		int scan();
		char m_path[2048];
		std::vector<File> m_files;
};

bool isDirectory(const char* path);

#endif

