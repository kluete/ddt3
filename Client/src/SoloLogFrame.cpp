// Lua ddt output textCtrl

#include <string>
#include <vector>
#include <unordered_map>
#include <thread>

#include "wx/frame.h"
#include "wx/timer.h"
#include "wx/bitmap.h"

#include "lx/ulog.h"
#include "lx/misc/geometry.h"
#include "lx/wx/geometry.h"

#include "TopFrame.h"
#include "Controller.h"
#include "logImp.h"
#include "BottomNotebook.h"

#include "OutputTextCtrl.h"

using namespace std;
using namespace juce;
using namespace LX;
using namespace DDT_CLIENT;

constexpr auto	FIXED_FONT = "FIXED_FONT"_log;

static const
log_def	NO_LEVEL{"<unknown>", RGB_COLOR::BLACK};

// log level/ID definition (label, color) -------------------------------------

#define LOG_DEF(t, clr)	{t, log_def(#t, RGB_COLOR::clr)}

static const
unordered_map<LogLevel, log_def>	s_LogLevelToColorLUT =
{
	LOG_DEF(FATAL,			RED),
	LOG_DEF(LX_ERROR,		RED),
	LOG_DEF(EXCEPTION,		RED),
	LOG_DEF(WARNING,		ORANGY_YELLOW),
	LOG_DEF(LX_MSG,			BLACK),
	LOG_DEF(DTOR,			BROWN),
	
	// lx utils
	LOG_DEF(APP_INIT,		NIGHT_BLUE),
	LOG_DEF(SIG,			ORANGE),
	LOG_DEF(CROSS_THREAD,		DARK_RED),
	LOG_DEF(DELAYER,		DARK_BROWN),
	LOG_DEF(UNIT,			RED),
	LOG_DEF(STOPWATCH,		BROWN),	
	LOG_DEF(FIXED_FONT,		PURPLE),
	LOG_DEF(REALIZED,		CRIMSON_RED),
	
// ddt
	LOG_DEF(MSG_IN,			ORANGE),
	LOG_DEF(MSG_OUT,		GREEN),
	LOG_DEF(MSG_UDP,		DARK_PURPLE),
	LOG_DEF(THREADING,		RED),
	LOG_DEF(LUA_USER,		DARK_GREEN),
	
	// event filter levels
	LOG_DEF(EV_FILTER,		DARK_GREEN),
	LOG_DEF(EV_DRAG,		LIGHT_BLUE),
	LOG_DEF(EV_KEY,			CYAN),
	
	LOG_DEF(FOCUS,			DARK_GREEN),
	
	LOG_DEF(LUA_ERROR,		DARK_RED),
	LOG_DEF(LUA_WARNING,		YELLOW),
	LOG_DEF(LUA_DBG,		PURPLE),
	LOG_DEF(SYNTH,			BLUE),
	LOG_DEF(UI,			KAKI),
	LOG_DEF(LOGIC,			GREEN),
	LOG_DEF(BREAKPOINT_EDIT,	DARK_PURPLE),
	
	LOG_DEF(OVERLAY,		DARK_GREY),
	LOG_DEF(NET_ERROR,		DARK_RED),
	LOG_DEF(NET,			PURPLE),
	LOG_DEF(INIT_LVL,		LIGHT_BLUE),
	LOG_DEF(PANE,			DARK_GREEN),
	LOG_DEF(PREFS,			DARK_BLUE),
	LOG_DEF(STYLED,			DARK_BLUE),
	LOG_DEF(TIMER,			LIGHT_BLUE),

	LOG_DEF(LIST_CTRL,		LIGHT_BLUE),
	LOG_DEF(LIST_BBOX,		DARK_RED),
	LOG_DEF(CELL_DATA,		GREEN),
	LOG_DEF(CONTEXT,		PALE_BLUE),
	LOG_DEF(CLIP,			DARK_GREY),
	LOG_DEF(RENDER,			DARK_BLUE),
	LOG_DEF(COLLISION,		DARK_GREY),
	LOG_DEF(CELL_EDIT,		RED),
	
	LOG_DEF(TEXT_VIEW,		GREEN),
	LOG_DEF(SELECTION,		PURPLE),
	LOG_DEF(PICK_PROFILE,		DARK_RED),
	
	LOG_DEF(SCROLLER,		LIGHT_BLUE),
	LOG_DEF(SCROLLABLE,		ORANGE),
	LOG_DEF(SCROLLER_COLL,		GREEN),
	LOG_DEF(SCROLL_FOCUS,		RED),
	LOG_DEF(BOOKMARK,		DARK_PURPLE),
	LOG_DEF(STC_FUNCTION,		DARK_BROWN),
	
	// daemon
	LOG_DEF(USER_MSG,		CYAN),
	LOG_DEF(RECURSE,		RED),
	LOG_DEF(INTERCEPT,		BLUE),
	LOG_DEF(STACK,			BLACK),
	LOG_DEF(GLUE,			DARK_RED),
	LOG_DEF(BREAKPOINT,		BLACK),
	LOG_DEF(LUA_ERR,		RED),
	LOG_DEF(ASIO,			ORANGY_YELLOW),
	LOG_DEF(COLLECT,		BLACK),
	LOG_DEF(SESSION,		DARK_PURPLE),
	LOG_DEF(PROF,			BLACK),
};

#undef LOG_DEF

struct log_evt
{
public:

	log_evt(const timestamp_t stamp, const LogLevel level, const string &msg, const size_t thread_index)
		: m_Stamp(stamp), m_Lvl(level), m_Msg(msg), m_ThreadIndex(thread_index)
	{
	}
	
	const timestamp_t	m_Stamp;
	const LogLevel		m_Lvl;
	const string		m_Msg;
	const size_t		m_ThreadIndex;
};

//---- Get Level Color --------------------------------------------------------

// static
log_def	ISoloLogFrame::GetLevelDef(const LogLevel &lvl)
{
	return s_LogLevelToColorLUT.count(lvl) ? s_LogLevelToColorLUT.at(lvl) : NO_LEVEL;
}

//-----------------------------------------------------------------------------

class SoloLogFrame : public wxFrame, public ISoloLogFrame, public LX::LogSlot
{
public:
	SoloLogFrame(TopFrame *tf, const int id, const wxBitmap &bm);
	virtual ~SoloLogFrame();
	
	wxWindow*	GetWxWindow(void) override	{return this;}
	
	void	SetSize(const Rect &r) override
	{
		wxFrame::SetSize(ToWxRect(r));
	}
	
	void	Close(const bool force_f) override
	{
		wxFrame::Close(force_f);
	}
	
	Rect	GetScreenRect(void) override
	{
		return Rect(wxFrame::GetScreenRect());
	}
	
	void	ToggleShowHide(const bool show_f) override
	{
		wxFrame::Show(show_f);
		
		if (show_f)	wxFrame::Raise();
	}
	
	void	ClearLog(void) override;
	
	bool	ShouldPreventAppExit() const override	{return false;}
	
private:
	
	void	LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &msg) override
	{
		unique_lock<mutex>	lock(m_ThreadMutex);
	
		const auto	tid = this_thread::get_id();
	
		if (!m_ThreadIndexMap.count(tid))	m_ThreadIndexMap.emplace(tid, m_ThreadIndexMap.size() + 1);
	
		const size_t	thread_index = m_ThreadIndexMap.at(tid);
	
		m_ThreadLogs.emplace_back(stamp, level, msg, thread_index);
		
		CallAfter(&SoloLogFrame::DequeueLogs);
	}
	
	void	DequeueLogs(void)
	{
		unique_lock<mutex>	lock(m_ThreadMutex);
		
		for (const auto &e : m_ThreadLogs)
		{
			SetLevelColor(e.m_Lvl);
			
			const string	s = xsprintf("%s THR[%1d] %s\n", e.m_Stamp.str(), e.m_ThreadIndex, e.m_Msg);
			
			m_TextCtrl.AppendText(s);
		}
		
		m_ThreadLogs.clear();
	}
	
	void	SetLevelColor(const LogLevel lvl)
	{
		const Color	clr = ISoloLogFrame::GetLevelDef(lvl).m_Color;
	
		const auto	wx_clr = clr.ToWxColor();
		assert(wx_clr);
	
		m_TextCtrl.SetDefaultStyle(wxTextAttr(*wx_clr));
	}
	
	void	SaveUI(void);
	
	void	OnCloseEvent(wxCloseEvent &e);
	void	OnMoveEvent(wxMoveEvent &e);
	void	OnResizeEvent(wxSizeEvent &e);
	
	TopFrame	*m_TopFrame;
	wxTextCtrl	m_TextCtrl;
	
	mutable mutex				m_ThreadMutex;
	unordered_map<thread::id, size_t>	m_ThreadIndexMap;
	vector<log_evt>				m_ThreadLogs;
	
	DECLARE_EVENT_TABLE()
};

//---- Solo LogFrame ----------------------------------------------------------

BEGIN_EVENT_TABLE(SoloLogFrame, wxFrame)

	EVT_CLOSE(		SoloLogFrame::OnCloseEvent)
	EVT_MOVE(		SoloLogFrame::OnMoveEvent)
	EVT_SIZE(		SoloLogFrame::OnResizeEvent)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

#define	styl	(wxCAPTION | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX /*| wxFRAME_TOOL_WINDOW*/)

	SoloLogFrame::SoloLogFrame(TopFrame *tf, const int id, const wxBitmap &bm)
		: wxFrame(nil, id, "Log", wxDefaultPosition, wxDefaultSize, styl),
		m_TopFrame(tf),
		m_TextCtrl(this, -1, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_RICH | wxTE_READONLY | wxTE_NOHIDESEL)
{
	assert(tf);
	
	// (uses COW)
	wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT));

	m_TextCtrl.SetDefaultStyle(wxTextAttr(*wxBLACK, *wxWHITE, ft));
	
	wxIcon	ico;
	
	ico.CopyFromBitmap(bm);
	assert(ico.IsOk());
	
	wxFrame::SetIcon(ico);
	
	rootLog::Get().Connect(this);

	Show();
}

//---- DTOR -------------------------------------------------------------------

	SoloLogFrame::~SoloLogFrame()
{
	uLog(DTOR, "SoloLogFrame::DTOR()");
	
	// notify TopFrame since has ptr to me?
	m_TopFrame = nil;
}

//---- Save UI ----------------------------------------------------------------

void	SoloLogFrame::SaveUI(void)
{
	if (!IsBeingDeleted() && m_TopFrame && !m_TopFrame->IsBeingDeleted())		m_TopFrame->DirtyUIPrefs();
}

//---- On Move & Size events --------------------------------------------------

void	SoloLogFrame::OnMoveEvent(wxMoveEvent &e)
{
	SaveUI();
	e.Skip();
}

void	SoloLogFrame::OnResizeEvent(wxSizeEvent &e)
{
	SaveUI();
	e.Skip();
}

//---- On Close event ---------------------------------------------------------

void	SoloLogFrame::OnCloseEvent(wxCloseEvent &e)
{
	// should check if TOPFRAME is being deleted?
	// if (!IsBeingDeleted() && e.CanVeto() && m_TopFrame && !m_TopFrame->IsBeingDeleted())
	if (!IsBeingDeleted() && e.CanVeto())
	{	// user operation, just hide window and veto
		// uLog(APP_INIT, "  VETOED");
		Hide();
		return e.Veto();
	}
	
	uLog(APP_INIT, "SoloLogFrame::OnCloseEvent()");
	
	LogSlot::DisconnectSelf();
	
	m_TopFrame = nil;
	
	// called by TopFrame, let it get closed
	e.Skip();
}

//---- Clear Log IMP ----------------------------------------------------------

void	SoloLogFrame::ClearLog(void)
{
	if (!wxThread::IsMain())		return;		// not main thread
	
	m_TextCtrl.Clear();
}

//---- INSTANTIATE ------------------------------------------------------------

// static
ISoloLogFrame*	ISoloLogFrame::Create(TopFrame *tf, const int id, const wxBitmap &bm)
{
	uLog(APP_INIT, "SoloLogFrame::Create()");
	
	return new SoloLogFrame(tf, id, bm);
}
