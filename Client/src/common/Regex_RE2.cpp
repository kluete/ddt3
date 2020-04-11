#include <cassert>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>
#include <regex>

// regex with RE2
#include "re2/re2.h"

#include "hl_md5.h"

#include "ddt/os_FileSystem.h"
#include "ddt/FileSystem.h"

#include "ddt/sharedLog.h"
#include "lx/xutils.h"

using namespace std;
using namespace DDT;

namespace LX
{
	
namespace FileName_RE2
{

//---- Is Legal Filename ? ----------------------------------------------------

bool	IsLegalFilename_LL(const string &filename)
{
	const RE2	re(LEGAL_FILENAME_RE2);
	assert(re.ok());
		
	string	matched_s;
		
	const bool	f = RE2::FullMatch(filename, re, &matched_s);
	
	uLog(UNIT, "FileName::IsLegalFilename(%S) = %c", filename, f);
	
	return f;
}

//---- Split fullpath with (potential) volume into vol/dir/name/ext -----------

SplitPath	SplitUnixPath_LL(const string &fullpath)
{
	if (fullpath.empty())		return SplitPath{};

	string	vol, dir, name, ext;

	const string	SPLIT_VOL_DIR_FILE_EXT_RE2	= "^(?i)([c-z]:)?(.*)/(.*)\\.(.{1,10})$";		// optional volume
	
	const bool	ok = RE2::FullMatch(fullpath, SPLIT_VOL_DIR_FILE_EXT_RE2, &vol, &dir, &name, &ext);
	return ok ? SplitPath(dir, name, ext) : SplitPath{};
}

//---- Make full Unix path from dir / filename --------------------------------

void	StripDuplicateSeparators(string &path)
{
	const string	UNIX_DUPLICATE_SEP_RE		= "/+";
	
	// add SEP twice to make sure a replacement occurs
	string	combo = path;
	
	const int	n_rep = RE2::GlobalReplace(&combo, UNIX_DUPLICATE_SEP_RE, "/");
	assert(n_rep >= 0);
	
	path = combo;
}

//---- Normalize Path by resolving env vars -----------------------------------

string	Normalize_LL(const string &path)
{
	if (path.empty())	return path;
	
	string	s = path;
	
	if (s[0] == '~')	s.replace(0, 1, ENV_VAR_HOME);
	
	string	envelope, env_var_name;
	
	while (RE2::PartialMatch(s, ENV_VAR_RE2, &envelope, &env_var_name))
	{
		assert(!env_var_name.empty());
		
		const string	resolved = ResolveEnvVar(env_var_name);
		
		const int	ind = s.find(envelope);
		assert(ind != string::npos);
		
		s.replace(ind, envelope.size(), resolved);
	}

	// replace any backslashes for Windows (is NOP otherwise)
	RE2::GlobalReplace(&s, "/", PATH_SEPARATOR_STRING);
	
	StripDuplicateSeparators(s);
	
	uLog(UNIT, "FileName::Normalize(%S) = %S", path, s);
	
	return s;
}

//---- Split Subdirs (low-level) ----------------------------------------------

StringList	SplitSubDirs_LL(const string &fullpath)
{
	if (fullpath.empty())	return StringList();		// empty
	
	// assert(DirExists(fullpath));				// do NOT test -- may be REMOTE path
	
	// normalize to TRAILING SEP for INTERNAL logic symmetry
	string	normalized_dir = fullpath;
	
	if (normalized_dir.back() != '/')	normalized_dir.push_back('/');
	
	const string	UNIX_SUBDIR_RE			= "/([^/]+)";
	
	re2::StringPiece	input(normalized_dir);
	string			subdir;
	StringList		res_list;
	
	while (RE2::FindAndConsume(&input, UNIX_SUBDIR_RE, &subdir))	res_list.push_back(subdir);
	
	return res_list;
}

//---- Has file Extension ? ---------------------------------------------------

bool	HasFileExtension_LL(const string &path)
{
	string	ext_s;
	
	const bool	f = RE2::FullMatch(path, HAS_FILE_EXT_RE2, &ext_s);
	return f;

	uLog(UNIT, "FileName::HasFileExtension_LL(%S) = %c", path, f);
	
	return f;
}

//---- Is Native Path ? -------------------------------------------------------

bool	IsNativePath_LL(const string &path)
{
	string	vol_path;
	
	const bool	f = RE2::FullMatch(path, NATIVE_PATH_HEADER_RE2, &vol_path);
	return f;
}

} // namespace FileName_RE2

} // namespace LX

// nada mas
