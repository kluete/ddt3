// platform-dependent file system defs for LINUX
// WARNING: encapsulated namespaces require SUPER CLEAN code

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#ifndef nil
	#define nil	nullptr
#endif

// ddt internal defines
#define __DDT_LINUX__

namespace LX
{
namespace FileName
{
// import into namespace
using std::string;
using std::vector;

	const string::value_type	PATH_SEPARATOR_CHAR	= '/';
	const string		PATH_SEPARATOR_STRING		= "/";
	const string		ROOT_PATH			= "/";
	const string		SUBDIR_RE			= "/([^/]+)";
	const string		DUPLICATE_SEP_RE		= "/+";
	const string		ABSOLUTE_PATH_RE		= "/.+";
	
	const string		ENV_VAR_RE			= R"___((\$([A-Z_][A-Z0-9_]+)))___";
	const string		ENV_VAR_RE2			= R"___((\$([A-Z_][A-Z0-9_]+)))___";				// (used to have double backslashes)
	
	const string		GET_PATH_EXT_RE			= R"___(^(?:\./|/).*\.(.{1,10})$)___";
	const string		LEGAL_FILENAME_RE2		= R"___(^([\w _\+\.\?=\-]+)$)___";				// (includes space)
	
	const string		LEGAL_FILENAME_RE		= R"___(^([\w _\+\.\?=\-]+)$)___";
	
	const string		NATIVE_PATH_HEADER_RE2		= R"___(^(/).*$)___";
	const string		NATIVE_PATH_HEADER_RE		= R"___(^(/).*$)___";
	
	const string		HAS_FILE_EXT_RE2		= "^.+\\.(.{1,10})$";
	const string		HAS_FILE_EXT_RE			= "^(.+)\\.(.{1,10})$";
	
	const vector<string>	BLOCKED_PATH_LIST = {		"/root", "/bin", "/boot", "/dev", "/lib", "/lib64", "/lost+found", "/srv", "/sbin", "/proc", "/dev",
								"/src", "/opt", "/run", "/sys", "/selinux", "/usr", "/var", "/home/lost+found" };
	
	const string		ENV_VAR_HOME			= "$HOME";
	
} // namespace FileName

} // namespace LX

// nada mas
