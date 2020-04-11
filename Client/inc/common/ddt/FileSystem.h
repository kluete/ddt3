// ddt common file system class (wx-free for daemon)

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <unordered_set>

#include "ddt/os_FileSystem.h"

#include "ddt/sharedDefs.h"

// forward declarations

namespace LX
{

class DataInputStream;
class DataOutputStream;

using DDT::StringList;
using DDT::StringSet;

namespace FileName
{	
// import into namespace
using std::string;
using std::vector;
using LX::DataInputStream;
using LX::DataOutputStream;

	class SplitPath
	{
	public:

		SplitPath()
			: m_ok(false), m_path(""), m_name(""), m_ext("")
		{}
			
		SplitPath(const string &path, const string &name, const string &ext)
			: m_ok(true), m_path(path), m_name(name), m_ext(ext)
		{}
		
		SplitPath(const string &name, const string &ext)
			: SplitPath("", name, ext)
		{}
		
		bool	IsOK(void) const	{return m_ok;}
		operator bool() const		{return IsOK();}
		
		string	Path(void) const	{return m_path;}
		string	Name(void) const	{return m_name;}
		string	Ext(void) const		{return m_ext;}
		
		bool	IsAbsolute() const;
		string	GetFullPath(void) const	{return m_path + m_name + "." + m_ext;}
		string	GetFullName(void) const	{return m_name + "." + m_ext;}
		
	private:

		bool	m_ok;
		string	m_path;
		string	m_name;
		string	m_ext;
	};

	size_t		GetFileSize(const string &fullpath);
	uint32_t	GetFileModifyStamp(const string &fullpath);
	int		CountTextLines(const string &fullpath);
	
	void	SetFileIOBufferSize(const size_t &new_buff_sz);
	string	ToLower(string s);
	bool	IsNativePath(const string &path);
	string	UnixToNativePath(const string &path);
	string	NativeToUnixPath(const string &path);
	
	bool	SetCwd(const string &path);
	string	GetCwd(void);
	bool	DirExists(const string &path);
	bool	MkDir(const string &path);
	bool	RmDir(const string &path);
	bool	IsDirBlocked(const string &path);
	bool	IsAbsolutePath(const string &path);
	bool	IsLegalFilename(const string &filename);
	bool	HasFileExtension(const string &path);
	
	string	MakeFullUnixPath(const string &dir, const string &filename);
	string	GetDirWithSep(const string &path);
	string	PopDirSubDir(const string &path);
	
	bool		RenamePath(const string &old_path, const string &new_path);
	bool		FileExists(const string &fullpath);
	bool		RemoveFile(const string &fullpath);					// must be RENAMED cause Windows has a RETARDED MACRO on DeleteFile()
	
	string		Normalize(const string &path);
	string		ConcatDir(const string &path, const string &subdir);
	
	string		GetTemporaryDir(void);
	
	string		ExpandEnvVars(const string &path);
	StringList	Normalize(const StringList &path_list);
	
	bool		LoadFile(const string &fullpath, DataOutputStream &dos);
	bool		SerializeFile(DataOutputStream &dos, const string &fullpath);
	size_t		UnserializeFile(DataInputStream &dis, const string &fullpath);
	size_t		UnserializeToString(DataInputStream &dis, string &buff_s);
	
	SplitPath	SplitUnixPath(const string &fullpath);
	SplitPath	SplitFullPath(const string &fullpath);					// should be PLATFORM-SPECIFIC
	
	string		MakeFullUnixPath(const string &dir, const string &filename);
	
	StringList	GetRecursivePathSubDirs(const string &fullpath);
	StringList	GetPathSubDirs(const string &fullpath);
	
	string		ResolveEnvVar(const string &env_var_name);

	#ifdef __DDT_IOS__
		extern
		string     s_TemporaryDirectory;
	#endif
	
	#ifdef __DDT_WIN32__
		StringList	GetVolumeList(void);
		string		WinToUnixPath(const string &path);
	#endif

	
} // namespace FileName

namespace FileName_RE2
{
	
// import into namespace
using namespace FileName;
using std::string;

	void		StripDuplicateSeparators(string &path);
	
	SplitPath	SplitUnixPath_LL(const string &fullpath);
	bool		SplitFullPath_LL(const string &fullpath, string &vol, string &dir, string &name, string &ext);
	
	StringList	SplitSubDirs_LL(const string &fullpath);
	
	string		Normalize_LL(const string &path);
	
	bool		IsLegalFilename_LL(const string &filename);
	bool		HasFileExtension_LL(const string &path);
	bool		IsNativePath_LL(const string &path);
	
} // namespace FileName_RE2

//---- std::regex based -------------------------------------------------------

namespace FileName_std
{	
// import into namespace
using namespace FileName;
using std::string;

	void		StripDuplicateSeparators(string &path);
	
	SplitPath	SplitUnixPath_LL(const string &fullpath);
	SplitPath	SplitFullPath_LL(const string &fullpath);					// should be PLATFORM-SPECIFIC
	
	StringList	SplitSubDirs_LL(const string &fullpath);
	
	string		Normalize_LL(const string &path);
	
	bool		IsLegalFilename_LL(const string &filename);
	bool		HasFileExtension_LL(const string &path);
	bool		IsNativePath_LL(const string &path);
	
} // namespace FileName_std

//---- Directory --------------------------------------------------------------

enum DIR_FLAG : uint8_t			// (usage is hardcoded right now)
{
	DIR_LIST_DIRS			= 1L << 0,
	DIR_LIST_FILES			= 1L << 1,
	DIR_LIST_INVISIBLES		= 1L << 2,
	
	DIR_DEFAULT			= DIR_LIST_DIRS | DIR_LIST_FILES
};

// NOTES:
// - always collects from ROOT
// - but never returns ROOT (/) itself

class Dir
{
	friend class Dir_native;
public:
	// ctors
	Dir();
	Dir(const StringSet &expanded_dir_set);
	Dir(const StringList &expanded_dir_list);
	Dir(LX::DataInputStream &dis);
	// dtor
	virtual ~Dir()		{}
	
	bool	Serialize(LX::DataOutputStream &mos);
	
	bool	IsOk(void) const;
	
	StringList	GetDirs(void) const;
	StringList	GetFiles(void) const;
	int		GetNumIllegalFilenames(void) const;

private:

	void	CollectRecursive(std::string parent_path, const int depth);
	
	bool		m_ValidFlag;
	uint8_t		m_Flags;		// (hardcoded right now)
	StringList	m_DirList;
	StringList	m_FileList;
	StringSet	m_ExpandedDirSet;
	int		m_IllegalFilenameCnt;
};

} // namespace LX

// nada mas
