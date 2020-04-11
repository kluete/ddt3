// DDT3 client

#include <cassert>
#include <vector>
#include <iostream>
#include <iosfwd>
#include <fstream>
#include <cwctype>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <thread>
#include <stdexcept>

#include <iostream>

#include "wx/wx.h"
#include "wx/app.h"
#include "wx/cmdline.h"
#include "wx/snglinst.h"
#include "wx/filename.h"
#include "wx/config.h"

#include "EvtFilter.h"
#include "logImp.h"

#include "lx/xstring.h"

#ifdef WIN32
	#include <Windows.h>
	#include <debugapi.h>
#endif // WIN32

#include "lx/juce/JuceApp.h"
#include "lx/juce/jeometry.h"

#include "lx/event/xsig_slot.h"
#include "lx/event/xdelay_slot.h"

#include "TopFrame.h"

#define CATCH_CONFIG_RUNNER
#define CATCH_BREAK_INTO_DEBUGGER() 	LX::xtrap();		// shouldn't use () version of macro?
#include "catch.hpp"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

constexpr auto	FIXED_FONT = "FIXED_FONT"_log;

static
const	wxCmdLineEntryDesc CommandLineDescriptor[] =
{
	{wxCMD_LINE_OPTION,	"l",	"load",		"load ddt project",	wxCMD_LINE_VAL_STRING,		wxCMD_LINE_VAL_NONE	},
	
	{wxCMD_LINE_NONE, nil, nil, nil, wxCMD_LINE_VAL_NONE, wxCMD_LINE_VAL_NONE}
};

//---- VisualStudio Log -------------------------------------------------------

class LogToVC : public LX::LogSlot
{
public:
	// ctor
	LogToVC()
	{}
	
	// LogSlot IMPLEMENTATION
	void	LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &msg) override
	{	
		#ifdef __WIN32__
			// send to VC, needs manual LF
			const string	s = stamp.str() + msg + "\n";
			OutputDebugString(s.c_str());
		#endif
	}
};

//---- My App -----------------------------------------------------------------

class MyApp: public wxApp
{	
public:
	// ctor
	MyApp(IJuceApp &japp)
		: m_JuceApp(japp)
	{
		m_TopFrame = nil;
	}

	bool	OnInit() override
	{
		// wx's global font prefs (bullshit)
		wxConfigBase::DontCreateOnDemand();
	
		wxFileName	app_cfn(wxString{argv[0]});
		assert(app_cfn.IsOk() && app_cfn.FileExists());
		
		uMsg("CWD was %S", wxGetCwd());
		
		string	res;
		
		try
		{
			// res = xsprintf("test arg overflow %s", "caca", 1234);
			// assert(!res.empty());
	
		}
		catch (std::exception &ec)
		{
			auto	what_s = ec.what();
			
			uMsg("std::exception in xsprintf(%s)", what_s);
		}
		catch (...)
		{
			std::exception_ptr	ep = std::current_exception();
			
			uMsg("opaque exception in xsprintf()");
		}
		
		uErr("uErr() test");
		
		// m_MyEventFilter.AddSelf(this);
		
		// check single instance (doesn't differentiate between Debug/Release builds)
		wxString	AppName = wxString::Format("DDT_client_inst_%s", wxGetUserId());

		m_SingleInstChecker = make_unique<wxSingleInstanceChecker>(AppName);
		if (m_SingleInstChecker->IsAnotherRunning())
		{	
			::wxMessageBox("Another application instance is currently running!", "Error", wxCANCEL | wxICON_ERROR);
			return false;		// abort
		}
		
		#ifdef WIN32
			m_VCLog = new LogToVC();
			rootLog::Get().Connect(m_VCLog);
		#endif

		// wxFAIL_MSG("test assert handler");

		// parse command line
		wxCmdLineParser	CmdLine(CommandLineDescriptor, argc, argv);

		switch (CmdLine.Parse())
		{
			case 0:

				// processed ok
				break;

			case -1:

				uMsg("Help was given, terminating");
				return false;
				break;

			default:

				// error
				uErr("Syntax error in command line - couldn't parse");
				return false;
				break;
		}
		
		wxString	project_filename;
		
		bool	ok = CmdLine.Found("l", &project_filename);
		// if (ok)
		
		ICrossThreader::RegisterWx();
	
		m_TopFrame = new TopFrame(*this, m_JuceApp, app_cfn.GetPath().ToStdString());
				
		if (!project_filename.empty())
		{	wxFileName	cfn(project_filename);
			if (cfn.IsOk() && cfn.FileExists())
			{	cfn.Normalize();
				ok = m_TopFrame->LoadProjectFile(cfn.GetFullPath());
				if (!ok)	uErr("couldn't load project file %S from cmd-line", project_filename);
			}
		}
		
		return true;		
	}
	
	int	OnRun() override
	{
		uLog(APP_INIT, "MyApp::OnRun()");
		
		m_MyEventFilter.Toggle(true);
		
		/*
		uMsg("TEST UPPER-CASE 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		uMsg("TEST UPPER-CASE 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		uMsg("test lower-case 0123456789abcdefghijklmnopqrstuvwxyz");
		uMsg("test lower-case 0123456789abcdefghijklmnopqrstuvwxyz");
		uMsg("213 123");
		uMsg("213 123 gggyyyyjjj___");
		*/
		
		uLog(APP_INIT, "MyApp::OnRun() about to start wx event loop");
		
		return wxApp::OnRun();
	}

	int	FilterEvent(wxEvent &e) override
	{
		if (!m_TopFrame)	return Event_Skip;
		
		const wxEventType	t = e.GetEventType();
		if ((t == wxEVT_LEFT_DOWN) || (t == wxEVT_RIGHT_DOWN))	// || (t == wxEVT_TOOL) || (t == wxEVT_MENU))
		{	
			m_TopFrame->OnClickCircle(0/*layer*/);
			return Event_Skip;
		}
		
		if ((t == wxEVT_CHAR_HOOK) && (wxGetMouseState().RawControlDown() && wxGetMouseState().AltDown()))
		{
			m_TopFrame->OnClickCircle(1/*layer*/);
		}
		
		return Event_Skip;
	}
	
	int	OnExit() override
	{
		m_MyEventFilter.RemoveSelf();
		
		m_TopFrame = nil;
		
		uLog(DTOR, "MyApp::OnExit()");
		
		return true;
	}

	void	OnAssertFailure(const wxChar *file, int line, const wxChar *func, const wxChar *cond, const wxChar *msg) override
	{
		// log bf break
		uErr("OnAssertFailure() ASSERT at %s:%d condition %S: %s",	file ? wxString(file) : _("<nil>"),
										line,
										cond ? wxString(cond) : _("nil>"),
										msg ? wxString(msg) : _("<nil>"));
		
		// release capture bf breakpoint or can't use mouse!
		wxWindow	*win = wxWindow::GetCapture();
		if (win)	win->ReleaseMouse();		// what if re-throws ?
		
		assert(false);
	}
	
	void	OnFatalException() override
	{
		uErr("OnFatalException()");
		
		wxWindow	*win = wxWindow::GetCapture();
		if (win)	win->ReleaseMouse();
		
		assert(false);
	}
	
	void	OnUnhandledException() override
	{
		uErr("OnUnhandledException()");
		
		wxWindow	*win = wxWindow::GetCapture();
		if (win)	win->ReleaseMouse();
		
		// check under GDB if exception came from my code
		
		assert(false);
	}
	
	bool	OnExceptionInMainLoop() override
	{
		// if under Clang, check if is bad xsprintf()
		uErr("OnExceptionInMainLoop()");
		
		wxWindow	*win = wxWindow::GetCapture();
		if (win)	win->ReleaseMouse();
		
		assert(false);
		
		return false;
	}
	
private:

	IJuceApp				&m_JuceApp;
	unique_ptr<wxSingleInstanceChecker>	m_SingleInstChecker;
	#ifdef WIN32
		LogToVC				*m_VCLog;
	#endif
	TopFrame				*m_TopFrame;
	MyEventFilter				m_MyEventFilter;
};

//---- MAIN -------------------------------------------------------------------

int	main(int argc, char *argv[])
{
	rootLog	log;
	
	log.EnableLevels({	APP_INIT,
				LUA_USER, LUA_ERROR, LUA_WARNING,
			
			});	
	
	// log.EnableLevels({DTOR});
	// log.EnableLevels({REALIZED});
	
	// log.EnableLevels({PREFS});
	// log.EnableLevels({NET});
	// log.EnableLevels({MSG_IN, MSG_OUT});
	// log.EnableLevels({OVERLAY});
	
	// log.EnableLevels({LIST_CTRL});
	// log.EnableLevels({CELL_DATA});
	// log.EnableLevels({CELL_EDIT});
	// log.EnableLevels({RENDER});
	
	// log.EnableLevels({UI});
	// log.EnableLevels({PANE});
	// log.EnableLevels({FOCUS});
	// log.EnableLevels({FIXED_FONT});
	
	// log.EnableLevels({PICK_PROFILE});
	// log.EnableLevels({COLLISION});
	
	// log.EnableLevels({SCROLLER});
	// log.EnableLevels({SCROLLABLE, SCROLLER_COLL});
	
	// log.EnableLevels({SYNTH});
	// log.EnableLevels({LOGIC});
	// log.EnableLevels({BREAKPOINT_EDIT});
	
	// log.EnableLevels({MSG_UDP});
	
	log.EnableLevels({LUA_DBG, LUA_USER});
	
	// log.EnableLevels({STYLED});
	// log.EnableLevels({BOOKMARK});
	
	// log.EnableLevels({TIMER});
	// log.EnableLevels({STC_FUNCTION});
	// log.EnableLevels({STOPWATCH});
	
	#if RUN_UNIT_TESTS
		log.EnableLevels({UNIT});
	#endif
	
	unique_ptr<LogSlot>	file_log(LogSlot::Create(LOG_TYPE_T::STD_FILE, "client.log", STAMP_FORMAT::MICROSEC));
	log.Connect(file_log.get());
	
	uLog(APP_INIT, "main()");
	
	try
	{
		unique_ptr<IJuceApp>	japp(IJuceApp::Create(argc, argv));
		assert(japp);
		
		japp->CallSync([](){ICrossThreader::RegisterJuceRaw();});
		
		uLog(APP_INIT, "main(), juce app created, creating wx app");
		
		MyApp	*app_imp = new MyApp(*japp);
		
		uLog(APP_INIT, "main(), wxApp instantiated");
		
		#if RUN_UNIT_TESTS
		{	ICrossThreader::RegisterPassThru();
		
			const int	err = Catch::Session().run(argc, argv);
			if (err)	LX::xtrap();
		}
		#endif
		
		const int	res = wxEntry(argc, argv);
		// wx app has finished
		
		uLog(APP_INIT, "main() af wxEntry");
	}
	catch (...)
	{	// error
		assert(0);
	}
	
	uLog(APP_INIT, "main(), about to exit main()");
	
	return 0;
}

// nada mas
