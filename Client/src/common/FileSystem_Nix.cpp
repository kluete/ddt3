// Unix
#include <cassert>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>			// stream formatting flags (for MD5)

// platform-specific (could be Linux or OSX)
#include "ddt/os_FileSystem.h"

// Linux
#include <dirent.h>		// for dir entries enumeration
#include <unistd.h>		// for change working dir

// posix
#include <sys/types.h>
#include <sys/stat.h>

#include "ddt/FileSystem.h"

// expose different platform API versions (total pain)
//   man 7 feature_test_macros

using namespace std;
using namespace LX::FileName;

namespace LX
{

#ifdef __DDT_IOS__
	string  FileName::s_TemporaryDirectory;
#endif

//---- Unix to Native Path ----------------------------------------------------

string	FileName::UnixToNativePath(const string &path)
{
	// IDENTITY on *Nix
	return path;
}

//---- Native To Unix Path ----------------------------------------------------

string	FileName::NativeToUnixPath(const string &path)
{
	// IDENTITY on *Nix
	return path;
}

//---- Get File Size ----------------------------------------------------------

size_t	FileName::GetFileSize(const string &fullpath)
{
	if (fullpath.empty())		return 0;	// error
	
	// stat()
	struct stat	f_stat;
	
	const int	res = stat(fullpath.c_str(), &f_stat);
	if (res == -1)			return 0;	// error
	
	const size_t	sz = (size_t) f_stat.st_size;
	return sz;
}

//---- Get File Modification Timestamp ----------------------------------------

uint32_t	FileName::GetFileModifyStamp(const string &fullpath)
{
	if (fullpath.empty())		return 0;	// error
	
	// stat()
	struct stat	f_stat;
	
	const int	res = stat(fullpath.c_str(), &f_stat);
	if (res == -1)			return 0;	// error
	
	// get modify stamp -- haphazard struct
	#ifdef __APPLE__
		const uint32_t	modify_stamp = (uint32_t) f_stat.st_mtime;
	#else
		// Debian / Gtk
		const uint32_t	modify_stamp = (uint32_t) f_stat.st_mtim.tv_sec;	// (time_t)
	#endif
	
	return modify_stamp;
}

//---- Set current working directory ------------------------------------------

bool	FileName::SetCwd(const string &path)
{
	const int	res = chdir(path.c_str());
	bool	ok = (res == 0);
	return ok;
}

//---- Get current working directory ------------------------------------------

string	FileName::GetCwd(void)
{
	// will malloc() the directory
	char	*ascii_p = getcwd(nil, 0/*as big as needed*/);		// may THROW
	assert(ascii_p);

	string	wd(ascii_p);

	::free(ascii_p);

	return wd;
}

//---- Directory Exists ? -----------------------------------------------------

bool	FileName::DirExists(const string &path)
{
	if (path.empty())	return false;
	
	DIR	*d = opendir(path.c_str());
	
	if (d)
	{	closedir(d);
		return true;
	}
	else	return false;
}

//---- Delete Directory (what happens to files in it?) ------------------------

bool	FileName::RmDir(const string &path)
{
	const int	res = rmdir(path.c_str());	// may THROW
	bool	ok = (res == 0);
	return ok;
}

//---- Create Directory -------------------------------------------------------

bool	FileName::MkDir(const string &path)
{
	const int	res = mkdir(path.c_str(), 0777);
	bool	ok = (res == 0);
	return ok;
}

//---- File Exists ? ----------------------------------------------------------

bool	FileName::FileExists(const string &fullpath)
{
	if (fullpath.empty())	return false;
	
	struct stat	f_stat;
	
	const int	res = stat(fullpath.c_str(), &f_stat);
	return (res == 0);
}

//---- Remove File ------------------------------------------------------------

bool	FileName::RemoveFile(const string &path)				// should use stdio ?
{
	if (path.empty())	return false;
	
	// gets deleted if is last link and no process uses the file
	// use remove() ?
	const int	res = unlink(path.c_str());
	bool	ok = (res == 0);
	return ok;
}

//---- Rename Path (file & dir) -----------------------------------------------

bool	FileName::RenamePath(const string &old_path, const string &new_path)
{	
	if (old_path.empty())	return false;
	if (new_path.empty())	return false;
	
	const int	res = rename(old_path.c_str(), new_path.c_str());
	bool	ok = (res == 0);
	return ok;
}

//---- Collect Recursive ------------------------------------------------------

void	Dir::CollectRecursive(string parent_path, const int depth)
{
	StringList	temp_dirs;
	const bool	no_invisible_f = !(DIR_LIST_INVISIBLES & m_Flags);

	DIR	*d = ::opendir(parent_path.c_str());
	assert(d);

	// normalize to TRAILING SEP
	if (parent_path.back() != FileName::PATH_SEPARATOR_CHAR)	parent_path.push_back(FileName::PATH_SEPARATOR_CHAR);

	// unsigned char d_type;
	struct dirent	*entry = nil;

	while ((entry = readdir(d)) != nil)
	{
		const string	fn(entry->d_name);

		if ((fn == ".") || (fn == ".."))			continue;	// skip dotdot
		if (no_invisible_f && (fn[0] == '.'))			continue;	// skip invisibles

		const string	fullpath = parent_path + fn;
		
		const SplitPath	split = SplitUnixPath(fullpath);

		const int	typ = entry->d_type;

		switch (typ)
		{	case 4:
			{	// dir
				if (FileName::IsDirBlocked(fullpath))	continue;	// skip blocked dir (could render but prevent recursion)

				temp_dirs.push_back(fullpath);
			}	break;

			case 8:
			{	// file
				if (FileName::IsDirBlocked(fn))			continue;	// skip blocked dir (could render but prevent recursion)
				if (!FileName::IsLegalFilename(fn))
				{	m_IllegalFilenameCnt++;
					continue;						// skip illegal filenames
				}
				
				if (split.Ext().empty())	continue;	// skip files w/out extension
				
				m_FileList.push_back(fullpath);
			}	break;

			case 10:
				// link
				continue;							// skip link
				break;

			default:
				// unknown
				break;
		}
	}

	::closedir(d);

	// sort dirs
	sort(temp_dirs.begin(), temp_dirs.end(), [](const string &s1, const string &s2){ return s1 < s2; });

	// recurse into sorted
	for (auto sorted_path : temp_dirs)
	{
		assert(!sorted_path.empty());

		m_DirList.push_back(sorted_path);

		// if expanded and not blocked ...
		if (m_ExpandedDirSet.count(sorted_path) && !FileName::IsDirBlocked(sorted_path))
		{	// ...recurse
			CollectRecursive(sorted_path, depth + 1);
		}
	}
}


} // namespace LX

// nada mas
