// ddt Lua directory system

#include <cassert>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>		// stream formatting flags (for MD5)

#ifdef WIN32
	// Windows
	// #include <winnt.h>		// do NOT include
	// #pragma message("> DirSystem.cpp bf win-specific #include <windows.h>")
	#include <windows.h>		// include this INSTEAD
	
	// #pragma message("> DirSystem.cpp bf win-specific #include <direct.h>")
	#include <direct.h>
	
	// #pragma message("> DirSystem.cpp bf win-specific #include <fileapi.h>")
	#include <fileapi.h>						// RESPONSIBLE FOR "No Target Architecture" error

	// #pragma message("> DirSystem.cpp bf win-specific #include <handleapi.h>")
	#include <handleapi.h>

	// #pragma message("> DirSystem.cpp AF win-specific includes")

#else
	// Linux
	#include <dirent.h>		// for dir entries enumeration
	#include <unistd.h>		// for change working dir
#endif

// posix
#include <sys/types.h>
#include <sys/stat.h>

// platform-specific
#include "ddt/os_FileSystem.h"

#include "ddt/FileSystem.h"

using namespace LX;
using namespace std;

//=============================================================================

/*
NOTES:

- don't play smartass with trailing SEP serialization
- just serialize however I want (with permissions, has_children bit, etc)

- doesn't return ROOT dir itself

*/

//---- Dir vanilla ctor -------------------------------------------------------

	Dir::Dir()
{
	m_ValidFlag = false;
	m_Flags = DIR_FLAG::DIR_DEFAULT;
	m_DirList.clear();
	m_FileList.clear();
	m_ExpandedDirSet = StringSet{ FileName::PATH_SEPARATOR_STRING };	// has ROOT by default
	m_IllegalFilenameCnt = 0;
}

//---- Is Ok ? ----------------------------------------------------------------

bool	Dir::IsOk(void) const
{
	return m_ValidFlag;
}

//---- CTOR (from hashset) ----------------------------------------------------

	Dir::Dir(const StringSet &expanded_dir_set)
		: Dir()
{
	m_ValidFlag = true;
	m_IllegalFilenameCnt = 0;
	m_ExpandedDirSet = { expanded_dir_set };		// (always in Unix format)
	
	CollectRecursive(FileName::ROOT_PATH, 0/*depth*/);

	// sort files
	sort(m_FileList.begin(), m_FileList.end(), [](const string &s1, const string &s2){ return s1 < s2; });
}

//---- CTOR (from expanded StringList) ----------------------------------------

	Dir::Dir(const StringList &expanded_dir_list)
		: Dir(expanded_dir_list.ToStringSet())
{
}

//---- CTOR and Deserializer --------------------------------------------------

	Dir::Dir(LX::DataInputStream &dis)
		: Dir()
{
	m_DirList = StringList{ dis };
	m_FileList = StringList{ dis };

	m_ValidFlag = true;			// are we SURE ?
}

//---- Get # illegal Filenames ------------------------------------------------

int	Dir::GetNumIllegalFilenames(void) const
{
	return m_IllegalFilenameCnt;
}

//---- Serialize --------------------------------------------------------------

bool	Dir::Serialize(LX::DataOutputStream &dos)
{
	if (!m_ValidFlag)	return false;

	dos << m_DirList << m_FileList;

	return true;
}

//---- Get Dirs ---------------------------------------------------------------

StringList	Dir::GetDirs(void) const
{
	return m_DirList;
}

//---- Get Files --------------------------------------------------------------

StringList	Dir::GetFiles(void) const
{
	return m_FileList;
}

// nada mas
