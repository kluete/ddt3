// Windows FileSystem

#include <cstdint>
#include <cassert>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>			// stream formatting flags (for MD5)

// #include "hl_md5.h"

// platform-specific
#include "ddt/FileSystem_Windows.h"

// Windows
// #include <winnt.h>		// do NOT include
// #pragma message("> FileSystem_Win.cpp bf win-specific #include <windows.h>")
#include <windows.h>		// include this INSTEAD

// #pragma message("> FileSystem_Win.cpp bf win-specific #include <direct.h>")
#include <direct.h>

// #pragma message("> FileSystem_Win.cpp bf win-specific #include <fileapi.h>")
#include <fileapi.h>		// RESPONSIBLE FOR "No Target Architecture" error

// #pragma message("> FileSystem_Win.cpp bf win-specific #include <handleapi.h>")
#include <handleapi.h>

#include <Shlwapi.h>		// for PathUnExpandEnvStrings(), also needs Shlwapi.lib

#pragma comment(lib, "Shlwapi.lib")

// #pragma message("> FileSystem_Win.cpp AF win-specific includes")

/*
	// posix
	#include <sys/types.h>
	#include <sys/stat.h>
*/

#include "ddt/FileSystem.h"

using namespace std;

namespace LX
{

StringList	FileName::GetVolumeList(void)
{
	const size_t	MAX_DOS_PATHS = 80;
	const size_t	MAX_DOS_PATHS_BUFFER = MAX_PATH * MAX_DOS_PATHS;

	vector<string::value_type>	buffer(MAX_DOS_PATHS_BUFFER, 0);

	const int	n_chars = QueryDosDeviceA(NULL/*query all*/, &buffer[0], MAX_DOS_PATHS_BUFFER);

	StringList	path_list;

	if (n_chars == 0)	return path_list;		// ERROR, return empty list

	for (int i = 0; i < n_chars; )
	{	
		const int len = ::strlen(&buffer[i]);
		if (len == 0)	break;

		path_list.push_back(string(&buffer[i], &buffer[i + len]));

		i += len + 1;
	}

	return path_list;
}

// Win flags
enum XSTAT_MASK : uint32_t
{	
	STAT_FILE		= FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE | FILE_SHARE_READ,
	STAT_DIR		= FILE_ATTRIBUTE_DIRECTORY,
	STAT_HIDDEN		= FILE_ATTRIBUTE_HIDDEN,
	STAT_MOUNT_POINT	= FILE_ATTRIBUTE_REPARSE_POINT			// file pointing to a volume (!)
	// PERM_READ_ONLY	= FILE_ATTRIBUTE_READONLY
};

static
bool	StatDuJour(const string &path, const uint32_t &mask32)
{
	WIN32_FILE_ATTRIBUTE_DATA	info;

	// contains FILETIMEs, FileSize, attributes
	const BOOL	ok = GetFileAttributesExA(path.c_str(), GetFileExInfoStandard/*enum*/, &info);
	if (!ok)	return false;				// ERROR

	const bool	f = (info.dwFileAttributes & static_cast<uint32_t>(mask32));

	return f;
}

//---- Win32 RIIA handle ------------------------------------------------------

class cHandle
{
public:
	// ctors
	cHandle()
		: m_Handle(INVALID_HANDLE_VALUE)
	{
	}
	cHandle(const std::string &fullpath, const bool write_f = false)
	{
		m_Handle = CreateFileA(	fullpath.c_str(),
					write_f ? FILE_GENERIC_WRITE : FILE_GENERIC_READ,
					0,							// share mode
					NULL,							// security attributes
					write_f ? CREATE_ALWAYS : OPEN_EXISTING,		// dwCreationDisposition
					FILE_ATTRIBUTE_NORMAL,					// dwFlagsAndAttributes			// MAY BE "archive" too
					NULL);							// hTemplateFile
		if (INVALID_HANDLE_VALUE == m_Handle)
		{
			// GetLastError()
		}
	}
	// dtor
	~cHandle()
	{
		if (INVALID_HANDLE_VALUE != m_Handle)
		{	CloseHandle(m_Handle);
			m_Handle = INVALID_HANDLE_VALUE;
		}
	}
	bool	IsOk(void) const
	{
		return (INVALID_HANDLE_VALUE != m_Handle);
	}

	HANDLE	GetHandle(void) const
	{
		return m_Handle;
	}

private:

	HANDLE	m_Handle;
};

//---- Get File Size ----------------------------------------------------------

size_t	FileName::GetFileSize(const string &fullpath)
{
	if (fullpath.empty())		return 0;	// error
	
	// DWORD WINAPI	GetFileSizeEx(_In_ HANDLE hFile, _Out_opt_ LPDWORD lpFileSizeHigh);
	
	cHandle		fhandle(fullpath);
	LARGE_INTEGER	sz = {0};					// 64-bit signed integer
	
	BOOL	ok = ::GetFileSizeEx(fhandle.GetHandle(), &sz);
	if (!ok)	return 0;				// ERROR

	return sz.QuadPart;
}

//---- Get File Modification Timestamp ----------------------------------------

uint32_t	FileName::GetFileModifyStamp(const string &fullpath)
{
	if (fullpath.empty())		return 0;	// error
	
	// BOOL WINAPI	GetFileTime(_In_ HANDLE hFile, _Out_opt_ LPFILETIME lpCreationTime, _Out_opt_ LPFILETIME lpLastAccessTime, _Out_opt_ LPFILETIME lpLastWriteTime);

	cHandle		fhandle(fullpath);
	FILETIME	create_time, last_access_time, last_write_time;

	BOOL	ok = ::GetFileTime(fhandle.GetHandle(), &create_time, &last_access_time, &last_write_time);
	if (!ok)	return 0;				// ERROR

	return last_write_time.dwLowDateTime;			// getting just low 32-bit ok?
}

//---- Expand Environment Variables -------------------------------------------

string	FileName::ExpandEnvVars(const string &path)
{
	if (path.empty())	return "";

	vector<string::value_type>	buffer(MAX_PATH, 0);

	// const int	written_sz = ::ExpandEnvironmentStringsA(path.c_str(), &buffer[0], buffer.size());
	// to get all env vars: GetEnvironmentStrings()
	// or ::PathCanonicalize()?
	BOOL	ok = ::PathUnExpandEnvStringsA(path.c_str(), &buffer[0], buffer.size());
	if (!ok)
	{
		return "";
	}
	
	string	res(&buffer[0]);

	return res;
}

//---- Unix to Native (i.e. Windows) Path -------------------------------------

string	FileName::UnixToNativePath(const string &path)
{
	if (path.empty())	return "";			// should be error?
	assert(!IsNativePath(path));
	assert(path[0] == '/');
	
	// strip 1st char
	string	converted = { path, 1, string::npos };
	
	// forward to back-slashes
	const int	n_rep = RE2::GlobalReplace(&converted, "/", "\\\\");
	assert(n_rep > 0);
	
	return converted;
}

//---- Native To Unix Path ----------------------------------------------------

string	FileName::NativeToUnixPath(const string &path)
{
	return WinToUnixPath(path);
}

//---- File Exists ? ----------------------------------------------------------

bool	FileName::FileExists(const string &path)
{
	if (path.empty())	return false;

	const bool	f = StatDuJour(path, STAT_FILE);		// separate error from result -- FIXME

	return f;
}

//---- Remove File ------------------------------------------------------------

bool	FileName::RemoveFile(const string &fullpath)
{
	if (fullpath.empty())	return false;

	const BOOL	ok = DeleteFileA(fullpath.c_str());
	return (ok == TRUE);
}

//---- Directory Exists ? -----------------------------------------------------

bool	FileName::DirExists(const string &dir_path)
{
	if (dir_path.empty())	return false;

	bool	f = StatDuJour(dir_path, STAT_DIR);		// separate error from result -- FIXME

	return f;
}

//---- Create Directory -------------------------------------------------------

bool	FileName::MkDir(const string &path)
{
	const int	res = mkdir(path.c_str());
	
	bool	ok = (res == 0);
	return ok;
}

//---- Delete Directory (what happens to files in it?) ------------------------

bool	FileName::RmDir(const string &path)
{
	const BOOL	ok = RemoveDirectoryA(path.c_str());		// non-unicode solution, i.e. will NOT work with UTF8 paths
	return (ok == TRUE);
}

//---- Get Temporary Directory ------------------------------------------------

string	FileName::GetTemporaryDir(void)
{
	// or ::GetTempPath(buff_sz, out_buff_char_ptr)
	return string(getenv("TEMP"));
}

//---- convert (native) Windows to Unix path ----------------------------------

string	FileName::WinToUnixPath(const string &path)
{
	assert(IsNativePath(path));
	
	// prepend / as first char
	string	converted = string("\\") + path;	// ToLower(path);
	
	// back to forward-slashes
	const int	n_rep = RE2::GlobalReplace(&converted, "\\\\", "/");
	assert(n_rep > 0);
	
	return converted;
}

//---- Rename Path (file or dir) ----------------------------------------------

bool	FileName::RenamePath(const string &old_path, const string &new_path)
{
	if (old_path.empty())		return false;
	if (new_path.empty())		return false;

	const BOOL	ok = ::MoveFileExA(old_path.c_str(), new_path.c_str(), MOVEFILE_WRITE_THROUGH/*wait til done*/);
	return (ok == TRUE);
}

//---- Collect Recursive ------------------------------------------------------

void	Dir::CollectRecursive(string parent_path, const int depth)
{
	// normalize to TRAILING SEP
	if (parent_path.back() != FileName::PATH_SEPARATOR_CHAR)	parent_path.push_back(FileName::PATH_SEPARATOR_CHAR);

	StringList	temp_dirs;
	const bool	no_invisible_f = !(DIR_LIST_INVISIBLES & m_Flags);

	string			find_path = parent_path + "*";
	WIN32_FIND_DATAA	entry;					// (ASCII info)

	HANDLE	d = ::FindFirstFileA(find_path.c_str(), &entry);	// (ASCII version)
	if (INVALID_HANDLE_VALUE == d)		return;			// should be error ???

	while (::FindNextFileA(d, &entry) == TRUE)
	{
		const string	fn(entry.cFileName);
		assert(!fn.empty());

		const string	fullpath = parent_path + fn;

		if ((fn == ".") || (fn == ".."))					continue;	// skip dotdot
		if (no_invisible_f && (fn[0] == '.'))					continue;	// skip dot invisibles (even though not formally invisible on Windows)
		if (no_invisible_f && (entry.dwFileAttributes & STAT_HIDDEN))		continue;	// skip Windows invisibles

		if (entry.dwFileAttributes & STAT_DIR)
		{	// dir
			if (FileName::IsDirBlocked(FileName::ToLower(fullpath)))	continue;	// skip BLOCKED dir (retest below from Unix path)

			temp_dirs.push_back(fullpath);
		}
		else if (entry.dwFileAttributes & STAT_FILE)
		{	// normal or "archive" file
			m_FileList.push_back(FileName::WinToUnixPath(fullpath));
		}
	}

	FindClose(d);

	// sort dirs
	sort(temp_dirs.begin(), temp_dirs.end(), [](const string &s1, const string &s2){ return s1 < s2; });

	for (auto sorted_path : temp_dirs)
	{
		assert(!sorted_path.empty());

		const string	unix_dir = FileName::WinToUnixPath(sorted_path);

		m_DirList.push_back(unix_dir);

		// if expanded...					- RETESTING for block redundant? FIXME
		if (m_ExpandedDirSet.count(unix_dir) > 0)		// && !FileName::IsDirBlocked(unix_dir))
		{	// ...recurse
			CollectRecursive(sorted_path, depth + 1);
		}
	}
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

} // namespace LX

// nada mas

