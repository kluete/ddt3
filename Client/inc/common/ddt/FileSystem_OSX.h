// platform-dependent file system defs for MacOS X
// WARNING: encapsulated namespaces require SUPER CLEAN code

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#ifndef nil
	#define nil	nullptr
#endif

#include "TargetConditionals.h"

// Apple-specific
#define	__DDT_APPLE__

#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	#define __DDT_IOS__
#endif


namespace LX
{
namespace FileName
{
// import into namespace
using std::string;
using std::vector;

	const string::value_type	PATH_SEPARATOR_CHAR		= '/';
	const string		PATH_SEPARATOR_STRING		= "/";
	const string		ROOT_PATH			= "/";
	const string		SUBDIR_RE			= "/([^/]+)";
	const string		SPLIT_FILE_EXT_RE		= "^(.*)\\.(.{3,5})$";
	const string		SPLIT_VOL_DIR_FILE_EXT_RE	= R"___(^(?i)([c-z]:)?(.*)/(.*)\.(.{1,10})$)___";
	const string		DUPLICATE_SEP_RE		= "/+";
	const string		NATIVE_PATH_HEADER_RE		= R"___(^(/).*$)___";
	const string		ABSOLUTE_PATH_RE		= "/.+";
	const string		ENV_VAR_RE			= "(\\$([A-Z_][A-Z0-9_]+))";
	const string		GET_PATH_EXT_RE		= "^(\\./|/?)[^/]*\\.(.{3})$";
	const string		LEGAL_FILENAME_RE		= "([^-_+\\w\\. ?=]+)";		// (includes space)
	
	const vector<string>	BLOCKED_PATH_LIST = {		"/Applications", "/Network", "/System", "/bin",  "/cores", "/dev", "/etc", "/home", "mach_kernel"/*file*/,
									"/net", "/private", "/sbin", "/tmp", "/var"};
	
	const string		ENV_VAR_HOME			= "$HOME";

	extern
	string             	s_TemporaryDirectory;		// on iOS must be retrieved from NSTemporaryDirectory() in ObjectiveC, then passed to ddt on init

} // namespace FileName

} // namespace LX

// nada max
