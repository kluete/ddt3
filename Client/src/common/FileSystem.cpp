// lua ddt common file system (wx-free)

// is POSIX - but how well-supported is it?
// http://www.cs.cf.ac.uk/Dave/C/node20.html#SECTION002010000000000000000
// has dir scanning and permissions

#include <cassert>
#include <algorithm>
#include <sstream>
#include <string>
#include <iomanip>				// stream formatting (for MD5)
#include <regex>
#include <random>

#include "ddt/sharedLog.h"
#include "lx/xutils.h"

#include "lx/stream/DataInputStream.h"
#include "lx/stream/DataOutputStream.h"

#include "ddt/os_FileSystem.h"
#include "ddt/FileSystem.h"

using namespace std;
using namespace DDT;
using namespace LX::FileName_std;

namespace LX
{

const int	RND_NUM_DIGITS = 8;

//---- To Lower case ----------------------------------------------------------

string	FileName::ToLower(string s)
{
	// (don't get the syntax)
	transform(s.cbegin(), s.cend(), s.begin(), ::tolower);
	
	return s;
}

//---- Get Random List --------------------------------------------------------

static
vector<uint8_t>	GetRandomList(const int n_digits)
{
	auto	rnd = bind(uniform_int_distribution<>(0, 255), default_random_engine{0x123c5678});		// inits once?
	
	vector<uint8_t>	rnd_list;
	
	for (int i = 0; i < n_digits; i++)	rnd_list.push_back((uint8_t)rnd());
	
	return rnd_list;
}

//---- Byte List to Hex String ------------------------------------------------

static
string	ByteListToHexString(const vector<uint8_t> &int_list)
{
	ostringstream	ss;
	
	// convert integers to lowercase hex string (dunno why format gets reset)
	for (const uint8_t ui8 : int_list)
		ss << hex << setfill('0') << setw(2) << (int) ui8;
	
	const string	s = ss.str();
	
	return s;
}

//---- Resolve Env Var --------------------------------------------------------

string	FileName::ResolveEnvVar(const string &env_var_name)
{
	assert(!env_var_name.empty());
	
	const char	*resolved_ascii = getenv(env_var_name.c_str());
	
	// env has priority
	if (resolved_ascii)			return string(resolved_ascii);
	
	// resolve $TMP
	// - OSX has pre-fetched string
	//   - should store in ENV VAR instead?
	if (env_var_name == "TMP")		return GetTemporaryDir();
	
	if (env_var_name == "TIMESTAMP")	return timestamp_t{}.str(STAMP_FORMAT::YMD);
	
	if (env_var_name == "RND")		return ByteListToHexString(GetRandomList(RND_NUM_DIGITS));
	
	uLog(UNIT, "couldn't resolve ENV VAR %S", env_var_name);
	
	// allow empty
	return "";
}	

//---- Normalize path list (overload) -----------------------------------------

StringList	FileName::Normalize(const StringList &path_list)
{
	StringList	res;
	
	for (const auto path : path_list)
	{
		res.push_back(Normalize(path));
	}
	
	return res;
}

namespace FileName
{

//---- Is absolute path ? -----------------------------------------------------

bool	IsAbsolutePath(const string &path)
{
	const auto	split = SplitUnixPath(path);
	return split.IsAbsolute();
}

//---- Get directory with trailing SEParator ----------------------------------

string	GetDirWithSep(const string &path)
{
	if (path.empty())	return "";
	
	if (path.back() != PATH_SEPARATOR_CHAR)
		return path + PATH_SEPARATOR_CHAR;
	else	return path;
}

SplitPath	SplitFullPath(const string &fullpath)
{
	return SplitUnixPath(fullpath);
}

//---- Pop 1 subdirectory from dir --------------------------------------------

string	PopDirSubDir(const string &path)
{
	StringList	dir_list = SplitSubDirs_LL(path);
	
	if (dir_list.size() > 0)	dir_list.pop_back();
	
	const string	popped = dir_list.ToFlatString(PATH_SEPARATOR_STRING, StringList::SEPARATOR_POS::FRONT);
	
	return popped;
}

//---- Concatenate an existing dir with 1 subdir ------------------------------

string	ConcatDir(const string &path, const string &subdir)
{
	// add SEP twice to make sure a replacement occurs
	string	combo = path + PATH_SEPARATOR_STRING + PATH_SEPARATOR_STRING + subdir;
	
	StripDuplicateSeparators(combo/*&*/);
	
	return combo;
}

//---- Unserialize DataInputStream to String buffer (write) -------------------

size_t	UnserializeToString(DataInputStream &dis, string &buff_s)
{	
	buff_s.clear();
	
	const size_t	sz = dis.Read32();
	if (!sz)					return 0;		// illegal size error
	
	buff_s.resize(sz, 0);
	
	const size_t	read_sz = dis.ReadRawBuffer((uint8_t*)&buff_s[0], sz);
	assert(read_sz == sz);
	
	return sz;		// ok
}

//---- Is Native Path ? -------------------------------------------------------

bool	IsNativePath(const string &path)
{
	if (path.empty())		return false;		// should be error?
	
	return IsNativePath_LL(path);
}

//---- Split Unix Path --------------------------------------------------------

SplitPath	SplitUnixPath(const string &fullpath)
{
	return SplitUnixPath_LL(fullpath);
}

//---- Make full Unix path from dir / filename --------------------------------

string	MakeFullUnixPath(const string &dir, const string &filename)
{
	// add SEP twice to make sure a replacement occurs
	string	s = dir + "//" + filename;
	
	StripDuplicateSeparators(s/*&*/);
	
	return s;
}

//---- Normalize Path by resolving env vars -----------------------------------

string	Normalize(const string &path)
{
	if (path.empty())	return path;
		
	return Normalize_LL(path);
}

//---- Is Legal Filename ? ----------------------------------------------------

bool	IsLegalFilename(const string &filename)
{
	return IsLegalFilename_LL(filename);
}

//---- Has file Extension ? ---------------------------------------------------

bool	HasFileExtension(const string &path)
{
	return HasFileExtension_LL(path);
}

bool	SplitPath::IsAbsolute() const
{
	return IsOK() && !m_path.empty() && (m_path[0] == '/');
}

//---- Get Temporary Directory ------------------------------------------------

string	GetTemporaryDir(void)
{
	#ifdef __DDT_IOS__
		// NSFileManager	*file_mgr;
		// file_mgr = [NSFileManager defaultManager];
		// NSString	tmp_dir = NSTemporaryDirectory();
    
		return LX::FileName::s_TemporaryDirectory;
	#else
		// Linux
		return std::string(getenv("HOME"));
	#endif
}

} // namespace FileName

} // namespace LX


// nada mas