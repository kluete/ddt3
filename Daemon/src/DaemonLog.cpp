// DDT daemon logs

#include <cassert>
#include <iostream>
#include <unordered_map>

#ifdef __APPLE__
	#include "TargetConditionals.h"
	
	#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		#define __DDT_IOS__
	#endif
#endif

#ifdef __DDT_IOS__
	extern void SetLogView(const char *text);
	
	#include "ddt/LoadBMP.h"

#endif

#include "lx/misc/termLog.h"

#include "ddt/sharedLog.h"
#include "Daemon.h"

using namespace std;
using namespace LX;
using namespace DDT;

static const
unordered_map<uint32_t, string>	s_TermColorMap
{
	{FATAL,		TermSeq({TERM::BLINK, TERM::BG_RED, TERM::BLACK})},
	{ERROR,		TermSeq({TERM::BLINK, TERM::RED})},
	{EXCEPTION,	TermSeq({TERM::BLINK, TERM::HI_YELLOW})},
	{WARNING,	TermSeq({TERM::HI_YELLOW})},
	
	{USER_MSG,	TermSeq({TERM::CYAN})},
	{RECURSE,	TermSeq({TERM::BLACK, TERM::BG_WHITE})},
	{INTERCEPT,	TermSeq({TERM::HI_MAGENTA})},
	{STACK,		TermSeq({TERM::HI_BLUE})},
	{GLUE,		TermSeq({TERM::HI_CYAN})},
	{BREAKPOINT,	TermSeq({TERM::GREEN})},
	{MSG_IN,	TermSeq({TERM::HI_YELLOW})},
	{MSG_OUT,	TermSeq({TERM::HI_GREEN})},
	{MSG_UDP,	TermSeq({TERM::RED})},
	{LUA_ERR,	TermSeq({TERM::RED})},
	{ASIO,		TermSeq({TERM::HI_BLUE})},
};

//---- Terminal Log -----------------------------------------------------------

class TerminalLog : public LogSlot
{
public:
	TerminalLog(const bool console_f, const STAMP_FORMAT &fmt)
		: m_StampFormat(fmt), m_ConsoleFlag(console_f)
	{
	}

	virtual ~TerminalLog() = default;
	
	// IMP
	void	LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &msg) override
	{
		if (!m_ConsoleFlag)	return;
	
		const string	s = stamp.str(m_StampFormat) + " " + msg;
		
		if (s_TermColorMap.count(level))
			cerr << s_TermColorMap.at(level) << s << TermDefault() << endl;
		else	cerr << s << endl;
	}
	
	void	ClearLog(void) override
	{
		// clear & home
		cerr << TermPrefix() << "2J" << TermPrefix() << "H" << flush;
	}
	
private:
	
	const STAMP_FORMAT	m_StampFormat;
	bool			m_ConsoleFlag;
};

//---- iOS Log ----------------------------------------------------------------

class iosLog : public LogSlot
{
public:
	iosLog(const STAMP_FORMAT &fmt);
	virtual ~iosLog() = default;
	
	// IMP
	void	LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &msg) override;
	
private:
	
	const STAMP_FORMAT	m_StampFormat;
	vector<string>		m_Entries;
	int			m_LastVisibleLines;
};

	iosLog::iosLog(const STAMP_FORMAT &fmt)
		: m_StampFormat(fmt)
{
	m_Entries.clear();
	m_LastVisibleLines = 40;
	
	#ifdef __DDT_IOS__
		// pre-store OSX/iOS temp dir - should write to an ENV VAR?
		LX::FileName::s_TemporaryDirectory = string(tmp_dir);
	#endif
}

void	iosLog::LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &msg)
{
	const string	s = stamp.str(m_StampFormat) + " " + msg;
	
	// buffer new line & concat last # entries to simulate scroll on UIView
	m_Entries.push_back(s);
	
	const int	n_entries = m_Entries.size();
	const int	first_ln = (n_entries > m_LastVisibleLines) ? (n_entries - m_LastVisibleLines) : 0;
	
	string	log_s;
	
	for (int i = first_ln; i < n_entries; i++)
	{
		log_s += m_Entries[i] + "\n";
	}
	
	#ifdef __DDT_IOS__
		SetLogView(log_s.c_str());
	#endif
}

//---- INSTANTIATE ------------------------------------------------------------

// static
LogSlot*	IDaemon::CreateTermLog(const bool console_f, const STAMP_FORMAT &fmt)
{
	return new TerminalLog(console_f, fmt);
}

// static
LogSlot*	IDaemon::CreateIosLog(const bool console_f, const STAMP_FORMAT &fmt)
{
	return new iosLog(fmt);
}

// nada mas