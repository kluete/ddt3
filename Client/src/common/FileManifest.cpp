// ddt common File Manifest class (wx-free for daemon)

#include <cassert>
#include <algorithm>
#include <iomanip>		// stream formatting flags (for MD5)
#include <sstream>
#include <fstream>

#include "ddt/FileSystem.h"

#include "ddt/FileManifest.h"

using namespace std;
using namespace LX;

//---- vanilla CTOR -----------------------------------------------------------

	FileManifest::FileManifest(const string &full_path)
		: m_Split(FileName::SplitFullPath(full_path))
{
	m_Size = 0;
	m_ModifyStamp = 0;
	m_NumLines = 0;
}

//---- InputStream CTOR -------------------------------------------------------

	FileManifest::FileManifest(DataInputStream &dis)
		: FileManifest(dis.ReadString())
{
	// don't care about correctness here, is only for display in Client
	
	m_Size = dis.Read32();
	m_NumLines = dis.Read32();
	m_ModifyStamp = (time_t) dis.Read32();
}

//---- Empty / Missing File ---------------------------------------------------

// static
FileManifest	FileManifest::MissingFile(const string &fullname)
{
	return FileManifest(fullname);
}

//---- Get FileName (name + ext) ----------------------------------------------

string	FileManifest::GetFileName(void) const
{
	return m_Split.GetFullName();
}

//---- Fill Properties --------------------------------------------------------

bool	FileManifest::FillProperties(void)
{
	const string	fullpath = m_Split.GetFullPath();
	if (fullpath.empty())				return false;		// empty path
	
	m_Size = FileName::GetFileSize(fullpath);
	m_ModifyStamp = FileName::GetFileModifyStamp(fullpath);
	m_NumLines = FileName::CountTextLines(fullpath);
	
	return true;	// ok
}

//---- Serialize FileManifest -------------------------------------------------

	// LX:: must be EXPLICIT here or won't link

LX::DataOutputStream& operator<<(LX::DataOutputStream &dos, const LX::FileManifest &fm)
{
	dos << fm.GetFileName() << fm.m_Size << fm.m_NumLines << (uint32_t) fm.m_ModifyStamp ;
	
	return dos;
}

// nada mas