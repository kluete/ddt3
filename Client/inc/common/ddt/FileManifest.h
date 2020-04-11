// ddt common File Manifest class (wx-free for daemon)

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <unordered_set>

#include "ddt/sharedDefs.h"

#include "lx/stream/DataInputStream.h"
#include "lx/stream/DataOutputStream.h"

#include "ddt/FileSystem.h"

#ifndef nil
	#define nil	nullptr
#endif

// forward declarations

namespace LX
{
using std::string;
	
//---- File Manifest ----------------------------------------------------------

class FileManifest
{
public:
	// ctors
	FileManifest(const string &full_path = "");
	FileManifest(DataInputStream &dis);		// deserialized only on client
	
	static
	FileManifest	MissingFile(const string &fullname);
	
	bool	IsOk(void) const;
	bool	FillProperties(void);
	
	string	GetFileName(void) const;
	
	uint32_t	m_Size;
	time_t		m_ModifyStamp;
	uint32_t	m_NumLines;		// zero-indexed
	
private:

	FileName::SplitPath	m_Split;
};

DataOutputStream& operator<<(DataOutputStream &dos, const FileManifest &fm);

} // namespace LX



// nada mas