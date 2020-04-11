// platform-dependent file system defs for WINDOWS
// WARNING: encapsulated namespaces require SUPER CLEAN code

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#ifndef nil
	#define nil	nullptr
#endif

// ddt internal defines
#define __DDT_WIN32__

namespace LX
{
namespace FileName
{
// import into namespace
using std::string;
using std::vector;

	const string		ROOT_PATH			= "C:\\";
	const string::value_type	PATH_SEPARATOR_CHAR		= '\\';
	const string		PATH_SEPARATOR_STRING		= R"___(\\)___";
	const string		VOLUME_RE			= "^(?i)([c-z]):.*$";
	const string		SUBDIR_RE			= "(?i:[c-z]:)?\\([^\\]+)";				// needs QUADRUPLE escaping
	const string		SPLIT_VOL_DIR_FILE_EXT_RE	= R"___(^(?i)([c-z]:)?(.*)\\(.*)\.(.{1,10})$)___";	// optional volume, case-insensitive
	const string		SPLIT_FILE_EXT_RE		= R"___(^(.*)\.(.{1,10})$)___";
	const string		DUPLICATE_SEP_RE		= R"___(\\\\+)___";
	const string		ABSOLUTE_PATH_RE		= "\\.+";
	const string		ENV_VAR_RE			= R"___(\\%([A-Z_][A-Z0-9_]+)%)___";
	const string		GET_PATH_EXT_RE			= R"___(^(?i:\.\\|[c-z]:\\).*\.(.{1,10})$)___";		// accepts relative path
	
	const string		LEGAL_FILENAME_RE		= "([^-_+\\w\\. ?=]+)";					// (includes space) - FIXME

	const string		NATIVE_PATH_HEADER_RE		= R"___(^(?i)([c-z]:\\).*$)___";			// WHY do I need 2 backslashes here???

	const string		OPTIONAL_VOLUME_SUBDIR_RE	= R"(^(?i:[c-z]:)?\\([^\\]+).*$)";			// case-(i)nsensitive, (R)aw string so backslash only twice-escaped
	
	// paths in LOWER-case
	const vector<string>	BLOCKED_PATH_LIST = {	"c:\\program files", "c:\\program files (x86)", "c:\\windows", "c:\\chocolatey"	 };

	const string		ENV_VAR_HOME			= "%USERPROFILE%";

} // namespace FileName

} // namespace LX

// nada max

