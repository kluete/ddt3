// lua ddt common file system (wx-free)

// is POSIX - but how well-supported is it?
// http://www.cs.cf.ac.uk/Dave/C/node20.html#SECTION002010000000000000000
// has dir scanning and permissions

#include <cassert>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>				// stream formatting (for MD5)
#include <regex>

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

static const
StringSet	s_BlockedDirSet { FileName::BLOCKED_PATH_LIST };

namespace FileName
{
	const size_t	FILE_IO_BUFFER_SIZE_DEFAULT	= 2048;
	const size_t	FILE_IO_BUFFER_SIZE_MIN		= 256;
	const size_t	FILE_IO_BUFFER_SIZE_MAX		= 1024 * 1024 * 2;	// 2 MB
	
	static
	vector<uint8_t>		s_FileBuffer(FILE_IO_BUFFER_SIZE_DEFAULT, 0);
	
//---- Set File I/O Buffer Size -----------------------------------------------

void	SetFileIOBufferSize(const size_t &new_buff_sz)
{
	assert(new_buff_sz >= FILE_IO_BUFFER_SIZE_MIN);
	assert(new_buff_sz <= FILE_IO_BUFFER_SIZE_MAX);

	s_FileBuffer.resize(new_buff_sz, 0);
}

//---- Is Directory Blocked ? -------------------------------------------------

bool	IsDirBlocked(const string &path)
{
	if (path.empty())	return false;							// or should be true?
	
	return (s_BlockedDirSet.count(path) > 0);
}

//---- Get Path Sub-Directories -----------------------------------------------

StringList	GetPathSubDirs(const string &fullpath)
{
	if (fullpath.empty())	return StringList();		// empty
	
	const StringList	split_dirs = SplitSubDirs_LL(fullpath);
	
	StringList	res_list;
	
	// hardcoded so has same # entries as recursive paths
	res_list.push_back("/");
	
	for (auto it : split_dirs)	res_list.push_back(it);
	
	return res_list;
}

//---- Get Recursive Path Sub-Dirs --------------------------------------------

	/*
	
	  "/home/user/Desktop" returns
	
		/
		/home
		/home/user
		/home/user/Desktop
	
	*/
	
StringList	GetRecursivePathSubDirs(const string &fullpath)
{
	if (fullpath.empty())	return StringList();	// empty
	
	const StringList	split_dirs = SplitSubDirs_LL(fullpath);
	
	StringList	res_list;
	
	res_list.push_back("/");	// 1st entry is hardcoded due to logic ASYMETRY
	
	string		cumul;
	
	for (const auto subdir : split_dirs)
	{
		// cummulate
		cumul += "/" + subdir;
		
		res_list.push_back(cumul);
	}
	
	return res_list;
}

//---- Load File --------------------------------------------------------------

	// ALMOST SAME AS serialize

bool	LoadFile(const string &fullpath, LX::DataOutputStream &dos)
{
	assert(!fullpath.empty());
	assert(FileExists(fullpath));
	const size_t	file_sz = GetFileSize(fullpath);
	assert(file_sz > 0);
	
	ifstream	ifs(fullpath.c_str(), ios_base::binary);
	if (ifs.fail() || ifs.bad())			return false;		// couldn't open error
	
	size_t	total_sz = 0;
	
	while (!ifs.eof())
	{	
		const size_t	read_sz = ifs.read((char*)&s_FileBuffer[0], s_FileBuffer.size()).gcount();
		if (!ifs.eof() && ifs.fail())		return false;		// read error
		
		dos.WriteRawBuffer(&s_FileBuffer[0], read_sz);
		
		total_sz += read_sz;
	}
	assert(file_sz == total_sz);
	
	return true;		// ok	
}

//---- Serialize File body to DataOutputStream (read) -------------------------

bool	SerializeFile(DataOutputStream &dos, const string &fullpath)
{
	const size_t	file_sz = GetFileSize(fullpath);
	assert(file_sz > 0);
	
	dos << (uint32_t) file_sz;
	
	ifstream	ifs(fullpath.c_str(), ios_base::binary);
	if (ifs.fail() || ifs.bad())			return false;		// couldn't open error
	
	size_t	total_sz = 0;
	
	while (!ifs.eof())
	{	
		const size_t	read_sz = ifs.read((char*)&s_FileBuffer[0], s_FileBuffer.size()).gcount();
		if (!ifs.eof() && ifs.fail())		return false;		// read error
		
		dos.WriteRawBuffer(&s_FileBuffer[0], read_sz);
		
		total_sz += read_sz;
	}
	assert(file_sz == total_sz);
	
	return true;		// ok
}

//---- Unserialize DataInputStream to File (write) ----------------------------

size_t	UnserializeFile(LX::DataInputStream &dis, const string &fullpath)
{	
	const size_t	file_sz = dis.Read32();
	if (file_sz == 0)				return 0;		// illegal size error
	
	// overwrites by default?
	ofstream	ofs(fullpath.c_str(), ios_base::binary | ios::trunc/*overwrite*/);
	if (ofs.fail() || ofs.bad())			return 0;		// couldn't open error
	
	size_t	remain_sz = file_sz;
	
	while (ofs.good() && (remain_sz > 0))
	{	
		const size_t	sz = (s_FileBuffer.size() < remain_sz) ? s_FileBuffer.size() : remain_sz;
		const size_t	read_sz = dis.ReadRawBuffer(&s_FileBuffer[0], sz);
		assert(read_sz == sz);
		
		ofs.write((const char*)&s_FileBuffer[0], sz);		// doesnt have .bytes_written() ?
		if (ofs.fail())				return 0;		// write error
		
		remain_sz -= sz;
	}

	ofs.flush();
	ofs.close();
	
	return file_sz;		// ok
}

//---- Count # Text Lines -----------------------------------------------------

int	CountTextLines(const string &fullpath)
{
	const size_t	file_sz = GetFileSize(fullpath);
	assert(file_sz > 0);
	
	ifstream	ifs(fullpath.c_str(), ios_base::binary);
	if (ifs.fail() || ifs.bad())			return -1;		// couldn't open file error
	
	size_t	total_sz = 0;
	int	nLF = 0, nCR = 0;
	
	while (!ifs.eof())
	{	
		const size_t	read_sz = ifs.read((char*)&s_FileBuffer[0], s_FileBuffer.size()).gcount();
		if (!ifs.eof() && ifs.fail())		return -1;		// read error
		
		// update EOL count
		for (int i = 0; i < read_sz; i++)
		{	nLF += (0x0a == s_FileBuffer[i]);
			nCR += (0x0d == s_FileBuffer[i]);
		}
		
		total_sz += read_sz;
	}
	assert(file_sz == total_sz);
	
	const int	n_lines = (nLF > nCR) ? nLF : nCR;			// BRICO-JARDIN? FIXME
	
	return n_lines + 1;
}

} // namespace FileName

} // namespace LX


// nada mas
