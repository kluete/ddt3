// filesystem OS dispatcher

#pragma once

#ifdef __APPLE__

	// OSX & iOS
	#include "ddt/FileSystem_OSX.h"

#elif defined(WIN32)

	// Windows
	#include "ddt/FileSystem_Windows.h"
	
#elif defined(__linux__)

	// Linux
	#include "ddt/FileSystem_Linux.h"
	
	#define DDT_PLATFORM_OS_NAME	"Linux"
	
	#if (defined(__x86_64) && !defined(__i386__))
		#define DDT_PLATFORM_OS_ARCH	64
	#else
		#define DDT_PLATFORM_OS_ARCH	32
	#endif

#else
	
	// platform detection FAILED
	static_assert(false, "ddt/os_FileSystem.h platform detection FAILED");
	
#endif

#if 0
	// in gcc
	#define __BYTE_ORDER__		__ORDER_LITTLE_ENDIAN__
	
	// in gcc headers
	/* Define to 1 if you have the <machine/endian.h> header file. */
	/* #undef _GLIBCXX_HAVE_MACHINE_ENDIAN_H */
#endif

// nada mas


