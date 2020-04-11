// lua ddt regex operations with std::regex

#include <cassert>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>
#include <regex>

#include "ddt/os_FileSystem.h"
#include "ddt/FileSystem.h"

#include "ddt/sharedLog.h"
#include "lx/xutils.h"

using namespace std;
using namespace DDT;

namespace LX
{

namespace FileName_std
{

//---- Split fullpath with (potential) volume into vol/dir/name/ext -----------

SplitPath	SplitUnixPath_LL(const string &fullpath)
{
	if (fullpath.empty())		return SplitPath{};

	static const regex	SPLIT_VOL_DIR_FILE_EXT_RE{"^(.+/)?([^\\.]+)\\.(.{1,10})$", regex::ECMAScript | regex::optimize};
	
	smatch	matches;
	
	if (!regex_match(fullpath, matches/*&*/, SPLIT_VOL_DIR_FILE_EXT_RE))	return SplitPath{};
	
	const size_t	n = matches.size();
	assert(n == 4);
	
	// remove path trailing separator (or should keep?)
	const string	path = matches[1].str();
	
	// static const regex	STRIP_TRAILING_SEP_RE{"^(.+)/$", regex::ECMAScript | regex::optimize};
	
	const string	path_no_trail_sep = path;		// regex_replace(path, STRIP_TRAILING_SEP_RE, "$1");
	
	return SplitPath(path_no_trail_sep, matches[2], matches[3]);
}

//---- Make full Unix path from dir / filename --------------------------------

void	StripDuplicateSeparators(string &path)
{
	static const regex	UNIX_DUPLICATE_SEP_RE{"/(/+)", regex::ECMAScript | regex::optimize};
	
	const string	clean_path = regex_replace(path, UNIX_DUPLICATE_SEP_RE, "/");
	
	path = clean_path;
}

//---- Normalize Path by resolving env vars -----------------------------------

string	Normalize_LL(const string &path)
{
	if (path.empty())	return path;
	
	string	s = path;
	
	if (s[0] == '~')	s.replace(0, 1, ENV_VAR_HOME);
	
	static const regex	re(ENV_VAR_RE, regex::ECMAScript | regex::optimize);
	
	smatch	matches;
	
	while (regex_search(s, matches/*&*/, re))
	{
		const size_t	n = matches.size();
		assert(n == 3);
		
		const string	env_var_name = matches[2];
		const string	resolved = ResolveEnvVar(env_var_name);
		// (allow empty)
		//  assert(!resolved.empty());
		
		const string	prefix_s = matches.prefix();
		const string	suffix_s = matches.suffix();
		
		const string	replaced_s = prefix_s + resolved + suffix_s;
		s = replaced_s;
	}

	// replace any backslashes for Windows (is NOP otherwise)
	StripDuplicateSeparators(s/*&*/);
	
	return s;
}

//---- Split Subdirs (low-level) ----------------------------------------------

StringList	SplitSubDirs_LL(const string &fullpath)
{
	if (fullpath.empty())	return StringList();		// empty
	
	// assert(DirExists(fullpath));				// do NOT test -- may be REMOTE path
	
	// normalize to TRAILING SEP for INTERNAL logic symmetry
	const bool	ext_f = HasFileExtension(fullpath);
	const bool	trail_f = (fullpath.back() == '/');
	
	const string	buff = (!ext_f && !trail_f) ? (fullpath + '/') : fullpath;
	
	static const regex	UNIX_SUBDIR_RE{"([^/]+)/", regex::ECMAScript | regex::optimize};
	
	auto		it = buff.cbegin();
	smatch		matches;
	StringList	res_list;
	
	while (regex_search(it, buff.cend(), matches/*&*/, UNIX_SUBDIR_RE))
	{
		const int	n = matches.size();
		assert(n == 2);
		
		const string	subdir = matches[1];
		assert(!subdir.empty());
		
		res_list.push_back(subdir);
		
		const size_t	pos = matches.position(1);
		const size_t	len = matches[1].length();
		
		it += pos + len;
		assert(it <= buff.cend());
		
		// const string	s(it, buff.cend());
	}
	
	return res_list;
}

//---- Is Legal Filename ? ----------------------------------------------------

bool	IsLegalFilename_LL(const string &filename)
{
	static const regex	re(LEGAL_FILENAME_RE, regex::ECMAScript | regex::optimize);
	
	const bool	f = regex_match(filename, re);
	
	uLog(UNIT, "FileName::IsLegalFilename_LL(%S) = %c", filename, f);
	
	return f;
}

//---- Has file Extension ? ---------------------------------------------------

bool	HasFileExtension_LL(const string &path)
{
	static const regex	re(HAS_FILE_EXT_RE, regex::ECMAScript | regex::optimize);
	
	const bool	f = regex_match(path, re);
	
	uLog(UNIT, "FileName::HasFileExtension_LL(%S) = %c", path, f);
	
	return f;
}

//---- Is Native Path ? -------------------------------------------------------

bool	IsNativePath_LL(const string &path)
{
	static const regex	re(NATIVE_PATH_HEADER_RE, regex::ECMAScript | regex::optimize);
	
	const bool	f = regex_match(path, re);
	return f;
}

} // namespace FileName_std

} // namespace LX

// nada mas
