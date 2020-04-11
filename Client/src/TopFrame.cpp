// DDT client topframe

#include <cstdint>
#include <cassert>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cmath>

#include <regex>

#include "wx/txtstrm.h"
#include "wx/regex.h"
#include "wx/sstream.h"
		
#include "wx/app.h"
#include "wx/fileconf.h"
#include "wx/notebook.h"
#include "wx/splitter.h"
#include "wx/filename.h"
#include "wx/filedlg.h"
#include "wx/wfstream.h"
#include "wx/sysopt.h"
#include "wx/notifmsg.h"
#include "wx/richtooltip.h"
#include "wx/gdicmn.h"
#include "wx/sashwin.h"
#include "wx/bannerwindow.h"
#include "wx/graphics.h"

#include "OutputTextCtrl.h"
#include "SourceFileClass.h"
#include "StyledSourceCtrl.h"
#include "VarViewCtrl.h"
#include "SearchResultsPanel.h"
#include "WatchBag.h"

#include "TopNotebook.h"
#include "BottomNotebook.h"
#include "StyledSourceCtrl.h"

#include "SearchBar.h"

#include "ProjectPrefs.h"

#include "ClientState.h"

#include "lx/ModalTextEdit.h"
#include "lx/renderer.h"

#include "Controller.h"

#include "ddt/sharedDefs.h"
#include "ddt/CommonClient.h"

#include "lx/stream/MemDataInputStream.h"
#include "lx/stream/MemDataOutputStream.h"

#include "ddt/FileSystem.h"

#include "Client.h"

#include "TopFrame.h"

#include "sigImp.h"
#include "logImp.h"

#include "lx/xstring.h"

#if RUN_UNIT_TESTS

#define CATCH_BREAK_INTO_DEBUGGER() 	LX::xtrap();		// shouldn't use () version of macro?
#include "catch.hpp"

#endif // RUN_UNIT_TESTS

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

const string	SPINNING_PROGRESS_STRING_LUT = string("/-\\|");

const int	DDT_CLIENT_NET_SYNC_SLEEP_MS	= 5;			// bullshit?
const int	DDT_CLIENT_NET_ASYNC_SLEEP_MS	= 5;

	RemoteServerConfig::RemoteServerConfig()
{
	m_Host.clear();
	m_Port = -1;
	m_Path.clear();
}

bool	RemoteServerConfig::IsOk(void) const
{
	return !m_Host.empty() && (m_Port > 0);
}

string	RemoteServerConfig::Host(void) const
{
	return m_Host;
}

int	RemoteServerConfig::Port(void) const
{
	return m_Port;
}

string	RemoteServerConfig::Path(void) const
{
	return m_Path;
}

//-----------------------------------------------------------------------------

// must be declared OUTSIDE class or is UNINITIALIZED !
const int	SHAPED_FRAME_STYL = wxFRAME_SHAPED | wxSTAY_ON_TOP | wxFRAME_NO_TASKBAR;

class DDT_CLIENT::OverlayFrame : public wxFrame
{
public:

	struct frame
	{
		frame(const wxGraphicsPath &p, const double &a)
		{
			m_path = p;
			m_alpha = a;
		}
		
		wxGraphicsPath	m_path;
		double		m_alpha;
	};

	// ctor
	OverlayFrame(TopFrame *parent, int id)
		: wxFrame(nil/*no parent*/, id, "", wxDefaultPosition, wxSize{OVERLAY_W, OVERLAY_W}, SHAPED_FRAME_STYL), m_Timer((wxFrame*)this)
	{
		assert(parent);
		m_TopFrame = parent;
		m_Layer = 0;
		
		// pre-compute outlines/paths
		wxGraphicsRenderer	*renderer = wxGraphicsRenderer::GetDefaultRenderer();
		assert(renderer);
		
		const double	ox = OVERLAY_W / 2.0;
		const double	oy = OVERLAY_W / 2.0;
		
	// layer 0 (circle)
		m_Outlines[0].clear();
		m_Clr[0] = wxColour(0x80, 0x80, 0xff, 0xff);
		m_DurationMS[0] = MOUSE_CLICK_RADIUS_TIMER_TOTAL_MS;
		
		for (double r = 4.0; r <= MOUSE_HIGHLIGHT_RADIUS_PIXELS; r *= 2.0)
		{
			wxGraphicsPath	p = renderer->CreatePath();
			p.AddCircle(ox, oy, r - 1);
			p.AddCircle(ox, oy, r);
			
			double	a = (r - 4.0) / (MOUSE_HIGHLIGHT_RADIUS_PIXELS - 4);
			a = 1 - a;
			
			frame	f {p, a};
			
			m_Outlines[0].push_back(f);
		}
		
	// layer 1 (cross)
		m_Outlines[1].clear();
		m_Clr[1] = wxColour(0x20, 0xff, 0x20, 0xff);
		m_DurationMS[1] = MOUSE_HIGHLIGHT_TIMER_TOTAL_MS;
		
		const double	thick = 4;
		
		wxGraphicsMatrix	mat = renderer->CreateMatrix();
		
		for (double a = 0; a <= M_PI; a += 10.0 * M_PI / 180)
		{	
			const double	r = sin(a) * MOUSE_HIGHLIGHT_RADIUS_PIXELS;
			const double	rd2 = r / 2.0;
			
			wxGraphicsPath	p = renderer->CreatePath();
			
			/*
			p.MoveToPoint(ox + rd2, oy);
			p.AddLineToPoint(ox, oy);
			p.AddLineToPoint(ox, oy + rd2);
			p.MoveToPoint(ox, oy);
			p.AddLineToPoint(ox + r, oy + r);
			*/
			
			p.AddRectangle(rd2, oy, rd2, thick);
			p.AddRectangle(OVERLAY_W - r, oy, rd2, thick);
			p.AddRectangle(ox, rd2, thick, rd2);
			p.AddRectangle(ox, OVERLAY_W - r, thick, rd2);
			
			mat.Set();
			mat.Translate(ox, oy);
			mat.Rotate(a);
			mat.Translate(-ox, -oy);
			
			p.Transform(mat);
			
			frame	f {p, sin(a)};
			
			m_Outlines[1].push_back(f);
		}
		
		uMsg("OverlayFrame::CTOR(), outlines %zu and %zu", m_Outlines[0].size(), m_Outlines[1].size());
		
 		Hide();
	}
	
	// dtor
	virtual ~OverlayFrame()
	{
		m_Timer.Stop();
	}
	
	void	StartAnimation(const int layer)
	{	
		assert((layer >= 0) && (layer < 2));
		m_Layer = layer;
		
		const wxPoint	pt = ::wxGetMousePosition();
		
		wxRect	rc {pt.x - (OVERLAY_W / 2), pt.y - (OVERLAY_W / 2), OVERLAY_W, OVERLAY_W};
		SetSize(rc);
		
		SetTransparent(m_Outlines[m_Layer][0].m_alpha);
		SetShape(m_Outlines[m_Layer][0].m_path);
		
		// Refresh();
		Update();
		
		Show();
		
		m_TopFrame->SetFocus();
		
		m_StartTimeStamp = timestamp_t();
		m_Timer.Start(MOUSE_CLICK_RADIUS_TIMER_STEP_MS, wxTIMER_CONTINUOUS);
	}
	
private:
	
	void	OnTimerNotify(wxTimerEvent &e)
	{
		uLog(TIMER, "OverlayFrame::OnTimerNotify()");
		assert(m_TopFrame);
		
		const int32_t	elap_ms = m_StartTimeStamp.elap_ms();
		if (elap_ms >= m_DurationMS[m_Layer])
		{	// stop
			m_Timer.Stop();
			Hide();
			return;
		}
		
		wxPoint	pt = ::wxGetMousePosition();
		pt.x -= (OVERLAY_W / 2);
		pt.y -= (OVERLAY_W / 2);
		SetPosition(pt);
		
		// animate
		const double	factor = (double) elap_ms / m_DurationMS[m_Layer];
		const int	index = factor * m_Outlines[m_Layer].size();
		assert((index >= 0) && (index < m_Outlines[m_Layer].size()));
	
		frame	f {m_Outlines[m_Layer][index]};
		
		SetTransparent(0xff * f.m_alpha);
		SetShape(f.m_path);
	
		Refresh();
		Update();
	}
	
	void	OnPaint(wxPaintEvent &e)
	{
		wxPaintDC	dc(this);
		
		dc.SetPen(wxNullPen);
		dc.SetBrush(wxBrush(m_Clr[m_Layer]));
		
		dc.DrawRectangle(0, 0, OVERLAY_W, OVERLAY_W);
	}
	
	TopFrame	*m_TopFrame;
	int		m_Layer;
	vector<frame>	m_Outlines[2];
	wxColour	m_Clr[2];
	uint32_t	m_DurationMS[2];
	timestamp_t	m_StartTimeStamp;
	wxTimer		m_Timer;
	
	wxDECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE(OverlayFrame, wxFrame)

	EVT_TIMER(	-1,			OverlayFrame::OnTimerNotify)
	EVT_PAINT(				OverlayFrame::OnPaint)
	
END_EVENT_TABLE()

//---- TopFrame Events --------------------------------------------------------

BEGIN_EVENT_TABLE(TopFrame, wxFrame)

	EVT_MENU(			MENU_ID_SAVE_PROJECT_AS,			TopFrame::OnSaveProjectAs)
	EVT_MENU(			MENU_ID_CLOSE_ALL_LUA_FILES,			TopFrame::OnCloseAllLuaFiles)
	EVT_MENU(			MENU_ID_CLOSE_PROJECT,				TopFrame::OnCloseProject)
	EVT_CLOSE(									TopFrame::OnCloseEvent)
	
	EVT_MENU(			MENU_ID_DUMP_PANEBOOK_VARS,			TopFrame::OnDumpNotebookVars)
	EVT_MENU(			MENU_ID_FORCE_PANEBOOK_RELOAD,			TopFrame::OnForcePaneReload)
	EVT_MENU(			MENU_ID_DUMP_FIXED_FONT,			TopFrame::OnDumpFixedFont)
	EVT_MENU(			MENU_ID_NETWORK_STRESS_TEST,			TopFrame::OnNetStressTest)
	EVT_MENU(			MENU_ID_SERIALIZE_BITMAP_FILE,			TopFrame::OnSerializeBitmapFile)
	EVT_MENU(			MENU_ID_SAVE_SCREENSHOT,			TopFrame::OnSaveScreenshot)
	EVT_MENU(			MENU_ID_CLEAR_LOG,				TopFrame::OnClearLog)
	
	EVT_TOOL(			TOOL_ID_OPEN_FILES,				TopFrame::OnOpenProjectOrLuaFiles)
	EVT_COMMAND_RANGE(		PROJECT_MRU_START, PROJECT_MRU_END, wxEVT_MENU,	TopFrame::OnMRUMenuSelected)
	EVT_TOOL(			PROJECT_MRU_CLEAR,				TopFrame::OnMRUMenuClear)
	EVT_TOOL(			TOOL_ID_SAVE_PROJECT,				TopFrame::OnSaveProject)
	
	EVT_TOOL(			TOOL_ID_CONNECTION,				TopFrame::OnToolConnection)
	
	EVT_TOOL(			TOOL_ID_START_RUN,				TopFrame::OnToolStartRun)
	EVT_TOOL(			TOOL_ID_STEP_INTO,				TopFrame::OnStepInto)
	EVT_TOOL(			TOOL_ID_STEP_OVER,				TopFrame::OnStepOver)
	EVT_TOOL(			TOOL_ID_STEP_OUT,				TopFrame::OnStepOut)
	
	EVT_TOOL(			TOOL_ID_TOGGLE_JIT,				TopFrame::OnToggleJITMode)
	EVT_TOOL(			TOOL_ID_TOGGLE_GLOBAL_BREAKPOINTS,		TopFrame::OnToggleGlobalBreakpoints)
	EVT_TOOL(			TOOL_ID_TOGGLE_LOAD_BREAK_MODE,			TopFrame::OnToggleLoadBreakMode)
	EVT_TOOL(			TOOL_ID_TOGGLE_STEP_MODE,			TopFrame::OnToggleStepMode)
	EVT_TOOL(			TOOL_ID_STOP,					TopFrame::OnToolStop)
	
	EVT_MENU(			TOGGLE_MENU_ID_SHOW_VAR_PATHS,			TopFrame::OnShowVarPaths)
	EVT_MENU(			TOGGLE_MENU_ID_GRID_OVERLAYS,			TopFrame::OnToggleGridOverlays)
	EVT_MENU(			TOGGLE_MENU_ID_SCREENCAST_MODE,			TopFrame::OnScreencastMode)
	
	EVT_MENU(			TOOL_ID_SOLO_LOG_WINDOW,			TopFrame::OnSoloLogWindow)
	EVT_MENU(			TOOL_ID_RESET_NOTE_PANES,			TopFrame::OnResetNotePanes)
	
	EVT_TOOL(			TOOL_ID_ABORT,					TopFrame::OnAbort)
	
	EVT_MENU(			MENU_ID_ESCAPE_KEY,				TopFrame::OnEscapeKey)
	
	EVT_COMBOBOX(			COMBOBOX_ID_DAEMON_HOST_N_PORT,			TopFrame::OnDaemonHostComboBox)
	EVT_COMBOBOX(			COMBOBOX_ID_LUA_MAIN_SOURCE_N_FUNCTION,		TopFrame::OnLuaMainComboBox)
	
	EVT_MOVE(									TopFrame::OnMoveEvent) 
	EVT_SIZE(									TopFrame::OnSizeEvent)
	EVT_SIZING(									TopFrame::OnSizingEvent)
	
	EVT_SPLITTER_SASH_POS_CHANGED(	SPLITTER_ID_MAIN,				TopFrame::OnMainHSplitterSashChanged)
	EVT_SPLITTER_SASH_POS_CHANGED(	SPLITTER_ID_TOP,				TopFrame::OnPaneBookSplitterSashChanged)
	EVT_SPLITTER_SASH_POS_CHANGED(	SPLITTER_ID_BOTTOM,				TopFrame::OnPaneBookSplitterSashChanged)
	
	// (internal)
	EVT_TIMER(			TIMER_ID_ASYNC_READ,				TopFrame::OnAsyncReadTimer)
	EVT_TIMER(			TIMER_ID_UDP_LOG,				TopFrame::OnUdpTimer)
	
	EVT_THREAD(			DBGF_ID_INCOMING_MESSAGE,			TopFrame::OnIncomingMessage)
	EVT_THREAD(			DBGF_ID_LUA_SUSPENDED,				TopFrame::OnLuaSuspended_SYNTH)
	EVT_THREAD(			BROADCAST_ID_STOP_SESSION,			TopFrame::OnStopSession_SYNTH)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

#define	styl	(wxCAPTION | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX | wxCLIP_CHILDREN /* | wxFRAME_TOOL_WINDOW*/)

	TopFrame::TopFrame(wxApp &app, IJuceApp &japp, const string &appPath)
		: wxFrame(nil, FRAME_ID_TOP_FRAME, wxString(TOP_FRAME_TITLE), wxDefaultPosition, wxDefaultSize, styl),
		SlotsMixin(CTX_WX, "TopFrame"),
		m_wxApp(app),
		m_jApp(japp),
		// m_FixedFont(IFixedFont::CreateFreeType("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf", 10, 96)),
		// m_FixedFont(IFixedFont::CreateFreeType("/usr/share/fonts/truetype/ttf-liberation/LiberationMono-Regular.ttf",12, 96)),
		m_FixedFont(IFixedFont::CreateFreeType("/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf", 9, 96)),
		m_VarViewPrefs(IVarViewFlags::Create(*this)),
		m_ShowScopeFlag(true)
{
	m_UIConfig = nil;
	m_ToolBar = nil;
	m_ProjectViewCtrl = nil;
	m_LocalsVarViewCtrl = nil;
	m_GlobalsVarViewCtrl = nil;
	m_WatchVarViewCtrl = nil;
	m_TraceBackCtrl = nil;
	m_BreakpointsListCtrl = nil;
	m_MemoryViewCtrl = nil;
	
	m_jLocalsCtrl = nil;
	
	m_FileMenu = m_EditMenu = m_ToolsMenu = m_OptionsMenu = m_WindowsMenu = nil;
	m_DaemonHostNPortComboBox = m_LuaStartupComboBox = nil;
	m_ClientStack = nil;
	m_ProjectPrefs = nil;
	
	m_OverlayFrame = nil;
	m_OutputTextCtrl = nil;
	m_SoloLogFrame = nil;
	
	m_CairoRenderer = nil;
	m_ModalTextEdit = nil;
	
	m_StatusBar = nil;
	m_BitmapLED = nil;
	m_WatchBag = nil;
	
	m_ThrobberBMFrames.clear();
	m_BitmapThrobber = nil;
	
	m_VisibleStackLevel = 0;
	m_ApplyingUIPrefsFlags = false;
	m_StartupLuaStruct.Clear();
	
	m_LastUserOpenDir = wxGetCwd();
	
	// DISABLE update UI events
	wxUpdateUIEvent::SetMode(wxUPDATE_UI_PROCESS_SPECIFIED);
	
	m_Timers[ASYNC_READ].SetOwner(this, TIMER_ID_ASYNC_READ);
	m_Timers[UDP_REFRESH].SetOwner(this, TIMER_ID_UDP_LOG);
	
	m_NotificationMessage = new wxNotificationMessage();
	m_NotificationMessage->SetParent(this);
	
	// image handlers FIRST (may not all be needed)
	
	// wxImage::AddHandler(new wxBMPHandler);		// (initialized by default)
	wxImage::AddHandler(new wxXPMHandler);
	wxImage::AddHandler(new wxPNGHandler);
	wxImage::AddHandler(new wxGIFHandler);
	wxImage::AddHandler(new wxICOHandler);
	
	// create toolbar BEFORE other controls (why?)
	CreateToolbar();
	
	InitCairoRenderer();
	
	m_OverlayFrame = new OverlayFrame(this, FRAME_ID_OVERLAY);
	
	m_ClientState.reset(IClientState::Create(*this));
	
	m_Controller = new Controller(*this, *m_ClientState, m_FixedFont, m_AuxBitmaps, *m_CairoRenderer, m_ModalTextEdit);
	
	m_Client = new Client(*m_Controller, *this, DDT_CLIENT_NET_SYNC_SLEEP_MS, DDT_CLIENT_NET_ASYNC_SLEEP_MS);
	
	m_LogOutput.reset(ILogOutput::Create(m_FixedFont));
	
	m_MainSplitterWindow = new wxSplitterWindow(this, SPLITTER_ID_MAIN, wxDefaultPosition, wxSize(-1, -1), wxNO_BORDER | wxSP_3D | wxSP_LIVE_UPDATE);
	
	m_TopNotePanel.reset(ITopNotePanel::Create(m_MainSplitterWindow, CONTROL_ID_TOP_NOTEPANEL, *this, *m_Controller));
	
	m_BottomNotePanel.reset(IBottomNotePanel::Create(m_MainSplitterWindow, CONTROL_ID_BOTTOM_NOTE_PANEL, *m_TopNotePanel, *this, *m_Controller));
	
	m_ProjectPrefs = new ProjectPrefs(*this, *m_Controller);
	
	// set minimum splitter size so doesn't get collapsed (then only solution is to delete a key in the registry!)
	m_MainSplitterWindow->SetMinimumPaneSize(80);

	m_MainSplitterWindow->SplitHorizontally(m_TopNotePanel->GetWxWindow(), m_BottomNotePanel->GetWxWindow());
	
	m_WatchBag = new WatchBag(this, m_ProjectPrefs);
	
	m_DynVarViewSigs.Add(SOLVE_REQUESTER_LOCALS);
	m_DynVarViewSigs.Add(SOLVE_REQUESTER_GLOBALS);
	m_DynVarViewSigs.Add(SOLVE_REQUESTER_WATCHES);
	m_DynVarViewSigs.Add(SOLVE_REQUESTER_JUCE_LOCALS);
	m_DynVarViewSigs.Add(PANE_ID_ALL_VAR_VIEWS);
	
	m_FunctionsComponent.reset(IPaneComponent::CreateFunctionsComponent(*m_BottomNotePanel, *m_TopNotePanel));
	m_VarViewComponent.reset(IPaneComponent::CreateVarViewComponent(*m_BottomNotePanel, PANEL_ID_JUCE_LOCALS_LIST_CONTROL, *m_TopNotePanel));
	
	// log
	m_OutputTextCtrl = IPaneMixIn::CreatePlaceholderPanel(*m_BottomNotePanel, PANEL_ID_OUTPUT_TEXT_CTRL, *m_TopNotePanel);
	// project
	m_ProjectViewCtrl = IPaneMixIn::CreateProjectListCtrl(*m_BottomNotePanel, PANEL_ID_PROJECT_LIST, *m_Controller);
	// locals
	m_LocalsVarViewCtrl = IVarViewCtrl::Create(*m_BottomNotePanel, PANEL_ID_LOCALS_LIST_CONTROL, *this, *m_Controller, *m_VarViewPrefs, true/*show scope*/, nil/*no edit*/);
	// globals	
	m_GlobalsVarViewCtrl = IVarViewCtrl::Create(*m_BottomNotePanel, PANEL_ID_GLOBALS_LIST_CONTROL, *this, *m_Controller, *m_VarViewPrefs, true/*DO show scope*/, nil/*no edit*/);
	// watch
	m_WatchVarViewCtrl = IVarViewCtrl::Create(*m_BottomNotePanel, PANEL_ID_WATCH_LIST_CONTROL, *this, *m_Controller, *m_VarViewPrefs, true/*show scope*/, m_WatchBag/*editable*/);
	// traceback
	m_TraceBackCtrl = IPaneMixIn::CreateTraceBackListCtrl(*m_BottomNotePanel, PANEL_ID_TRACE_BACK_LIST_CONTROL, *this, *m_Controller);
	// breakpoints
	m_BreakpointsListCtrl = IPaneMixIn::CreateBreakpointListCtrl(*m_BottomNotePanel, PANEL_ID_BREAKPOINTS_LIST_CONTROL, *m_Controller);
	// memory
	m_MemoryViewCtrl = IPaneMixIn::CreateMemoryViewCtrl(*m_BottomNotePanel, PANEL_ID_MEMORY_VIEW_CONTROL, *m_TopNotePanel);
	// search results
	m_SearchResultsPanel = new SearchResultsPanel(*m_BottomNotePanel, PANEL_ID_SEARCH_RESULTS, m_TopNotePanel->GetSearchBar());
	// Lua functions
	m_FunctionsPanel = IPaneMixIn::CreatePlaceholderPanel(*m_BottomNotePanel, PANEL_ID_LUA_FUNCTIONS_CONTROL, *m_TopNotePanel);

	// juce locals ctrl
	m_jLocalsCtrl = IPaneMixIn::CreatePlaceholderPanel(*m_BottomNotePanel, PANEL_ID_JUCE_LOCALS_LIST_CONTROL, *m_TopNotePanel);
	
	InitMenu();
	
	// menu accelerators/shortcuts -- FIXED SIZE is # customizable commands
	m_KeyboardShortcuts.clear();
	
	m_KeyboardShortcuts =
	{
		{wxACCEL_NORMAL,		WXK_F5,		TOOL_ID_START_RUN},
		{wxACCEL_NORMAL,		WXK_F9,		TOOL_ID_TOGGLE_BREAKPOINT},
		{wxACCEL_NORMAL,		WXK_F10,	TOOL_ID_STEP_OVER},
		{wxACCEL_NORMAL,		WXK_F11,	TOOL_ID_STEP_INTO},
		{wxACCEL_SHIFT,			WXK_F11,	TOOL_ID_STEP_OUT},
		{wxACCEL_SHIFT,			WXK_F5,		TOOL_ID_STOP},
		
		{wxACCEL_CTRL,			'O',		TOOL_ID_OPEN_FILES},
		{wxACCEL_CTRL,			'S',		EDIT_MENU_ID_SAVE_FILE},
		
		{wxACCEL_CTRL,			'F',		TOOL_ID_FIND},
		{wxACCEL_ALT,			'F',		TOOL_ID_FIND},
		{wxACCEL_CTRL | wxACCEL_SHIFT,	'F',		TOOL_ID_FIND_ALL},
		{wxACCEL_ALT | wxACCEL_SHIFT,	'F',		TOOL_ID_FIND_ALL},
		{wxACCEL_CTRL,			'E',		TOOL_ID_SET_FIND_STRING},
		{wxACCEL_ALT,			'E',		TOOL_ID_SET_FIND_STRING},
		{wxACCEL_CTRL,			'G',		TOOL_ID_FIND_NEXT},
		{wxACCEL_ALT,			'G',		TOOL_ID_FIND_NEXT},
		{wxACCEL_CTRL | wxACCEL_SHIFT,	'G',		TOOL_ID_FIND_PREVIOUS},
		{wxACCEL_ALT | wxACCEL_SHIFT,	'G',		TOOL_ID_FIND_PREVIOUS},
		
		{wxACCEL_CTRL,			'Q',		TOOL_ID_ABORT},
		
		{wxACCEL_ALT,			'B',		TOOL_ID_BOOKMARK_TOOGLE},
		{wxACCEL_ALT,			',',		TOOL_ID_BOOKMARK_PREV},
		{wxACCEL_ALT,			'.',		TOOL_ID_BOOKMARK_NEXT},
		
		{wxACCEL_CTRL,			WXK_NUMLOCK,	MENU_ID_SAVE_SCREENSHOT},
		
		{wxACCEL_NORMAL,		WXK_ESCAPE,	MENU_ID_ESCAPE_KEY},
	};
	
	wxAcceleratorTable accel(m_KeyboardShortcuts.size(), &m_KeyboardShortcuts[0]);
	
	SetAcceleratorTable(accel);
	
	// UI config/prefs (on Windows saves in working dir)
	m_UIConfig = new wxFileConfig("ddt3", "PeterL", "ddt.conf", "", wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH);
	
	Show();
	
	LoadUIConfig();
	
	m_BottomNotePanel->ClearVarViews();
	
	m_BottomNotePanel->RaisePageID(PANE_ID_OUTPUT, false/*reload?*/);
	
	// must DELAY
	CallAfter(&TopFrame::OnAllRealized);
	
	uLog(APP_INIT, "TopFrame::ctor done");
}

VarViewSignals&	TopFrame::GetSignals(const int varview_id)
{
	return m_DynVarViewSigs.Get(varview_id);
}

//---- On All Realized (hopefully) --------------------------------------------

void	TopFrame::OnAllRealized(void)
{
	uLog(APP_INIT, "TopFrame::OnAllRealized()");
	
	m_Timers[UDP_REFRESH].Start(UDP_REFRESH_PERIOD_MS, wxTIMER_CONTINUOUS);

	UpdateToolbarState();
	
	m_LogOutput->BindToPanel(*m_OutputTextCtrl, JWX_BIND_RETRY_MS);
	m_FunctionsComponent->BindToPanel(*m_FunctionsPanel, JWX_BIND_RETRY_MS);
	m_VarViewComponent->BindToPanel(*m_jLocalsCtrl, JWX_BIND_RETRY_MS);
	
	m_FixedFont->Dump();
}

//---- Init Menu --------------------------------------------------------------

void	TopFrame::InitMenu(void)
{
	// file
	m_FileMenu = new wxMenu();
	m_FileMenu->Append(TOOL_ID_OPEN_FILES, "Open File(s)");
	m_FileMenu->AppendSeparator();
	m_FileMenu->Append(EDIT_MENU_ID_SAVE_FILE, "Save Source");
	m_FileMenu->Append(EDIT_MENU_ID_REVERT_FILE, "Revert Source");
	m_FileMenu->Append(MENU_ID_SAVE_PROJECT_AS, "Save Project As");
	m_FileMenu->AppendSeparator();
	m_FileMenu->Append(MENU_ID_CLOSE_ALL_LUA_FILES, "Close All Lua Files");
	m_FileMenu->Append(MENU_ID_CLOSE_PROJECT, "Close Project");
	m_FileMenu->AppendSeparator();
	m_FileMenu->Append(TOOL_ID_ABORT, "Quit");
	
	m_EditMenu = new wxMenu();
	m_EditMenu->Append(TOOL_ID_FIND, "Find");
	m_EditMenu->Append(TOOL_ID_FIND_ALL, "Find All");
	m_EditMenu->Append(TOOL_ID_FIND_PREVIOUS, "Find Previous");
	m_EditMenu->Append(TOOL_ID_FIND_NEXT, "Find Next");
	m_EditMenu->Append(TOOL_ID_SET_FIND_STRING, "Set Find String");
	m_EditMenu->Append(MENU_ID_GOTO_LINE, "Goto Line");
	
	// tools menu
	m_ToolsMenu = new wxMenu();
	m_ToolsMenu->Append(MENU_ID_DUMP_PANEBOOK_VARS, "Dump panebooks");
	m_ToolsMenu->Append(MENU_ID_FORCE_PANEBOOK_RELOAD, "Force Pane Reload");
	m_ToolsMenu->AppendSeparator();
	m_ToolsMenu->Append(MENU_ID_NETWORK_STRESS_TEST, "Network Stress Test");
	m_ToolsMenu->Append(MENU_ID_DUMP_FIXED_FONT, "Dump Fixed Font");
	m_ToolsMenu->AppendSeparator();
	m_ToolsMenu->Append(MENU_ID_SERIALIZE_BITMAP_FILE, "Serialize Bitmap File");
	m_ToolsMenu->Append(MENU_ID_SAVE_SCREENSHOT, "Save Screenshot");
	m_ToolsMenu->Append(MENU_ID_CLEAR_LOG, "Clear Log");
	m_ToolsMenu->AppendSeparator();
	m_ToolsMenu->Append(MENU_ID_REFRESH_MEMORY_VIEW, "Refresh Memory Pane");
	
	// options menu
	m_OptionsMenu = new wxMenu();
	m_OptionsMenu->AppendCheckItem(TOGGLE_MENU_ID_SHOW_VAR_PATHS, "Show Var Paths");
	m_OptionsMenu->AppendCheckItem(TOGGLE_MENU_ID_GRID_OVERLAYS, "Grid Overlays");
	m_OptionsMenu->AppendSeparator();
	m_OptionsMenu->AppendCheckItem(TOGGLE_MENU_ID_SCREENCAST_MODE, "Screencast mode");
	m_OptionsMenu->Check(TOGGLE_MENU_ID_SCREENCAST_MODE, DDT_SCREENCAST_DEFAULT_FLAG);
	
	m_WindowsMenu = new wxMenu();
	m_WindowsMenu->Append(TOOL_ID_SOLO_LOG_WINDOW, "Solo Log");
	m_WindowsMenu->AppendSeparator();
	m_WindowsMenu->Append(TOOL_ID_RESET_NOTE_PANES, "Reset Notepanes");
	
	// append menus to the menu bar
	m_MenuBar = new wxMenuBar();
	m_MenuBar->Append(m_FileMenu, "File");
	m_MenuBar->Append(m_EditMenu, "Edit");
	m_MenuBar->Append(m_ToolsMenu, "Tools");
	m_MenuBar->Append(m_OptionsMenu, "Options");
	m_MenuBar->Append(m_WindowsMenu, "Windows");
	
	// ... and attach this menu bar to the frame
	SetMenuBar(m_MenuBar);
}

//---- DTOR -------------------------------------------------------------------

	TopFrame::~TopFrame()
{
	uLog(DTOR, "TopFrame::DTOR()");

}

//---- On Close ---------------------------------------------------------------

void	TopFrame::OnCloseEvent(wxCloseEvent &e)
{
	uLog(DTOR, "TopFrame::OnCloseEvent(%s)", e.CanVeto() ? "can-veto" : "force");
	
	#if 0
	if (e.CanVeto() && !IsBeingDeleted())
	{	// called by user, just hide window and veto
		uLog(DTOR, "  VETOed!");
		Hide();
		e.Veto();
		return;
	}
	#endif
	
	// stop ALL timers
	for (int i = 0; i < TIMER_ID::TIMER_MAX; i++)
	{
		if (m_Timers[i].IsRunning())
		{
			uLog(DTOR, "WARNING:: timer %d was running", i);
		}
		
		m_Timers[i].Stop();
	}
	
	m_jApp.QuitJuceMsgThread();
	
	ICrossThreader::AbortAll();
	
	if (m_BottomNotePanel)		m_BottomNotePanel->DestroyDanglingPanes();		// ???
	
	 //DestroyController
	
	if (m_OverlayFrame)
	{	m_OverlayFrame->Close(true);
		m_OverlayFrame = nil;
	}
	
	if (m_SoloLogFrame)
	{	// DumpWindow(m_SoloLogFrame);
		m_SoloLogFrame->Close(true);
		m_SoloLogFrame = nil;
	}
	
	if (m_ProjectPrefs)
	{	delete m_ProjectPrefs;
		m_ProjectPrefs = nil;
	}
	
	if (m_CairoRenderer)
	{	delete m_CairoRenderer;
		m_CairoRenderer = nil;
	}
	
	// called by TopFrame DTOR, let it get closed
	e.Skip();
}

Controller&	TopFrame::GetController(void)
{
	assert(m_Controller);
	
	return *m_Controller;
}

//---- On Abort ---------------------------------------------------------------

void	TopFrame::OnAbort(wxCommandEvent &e)
{
	uLog(DTOR, "TopFrame::OnAbort()");
	
	Close(true);
}

void	TopFrame::OnEscapeKey(wxCommandEvent &e)
{
	uLog(UI, "TopFrame::OnEscapeKey()");
	assert(m_TopNotePanel);
	
	m_TopNotePanel->EscapeKeyCallback();
	
	e.Skip();
}

//---- On Solo Log Window -----------------------------------------------------

void	TopFrame::OnSoloLogWindow(wxCommandEvent &e)
{
	if (m_SoloLogFrame)
	{
		m_SoloLogFrame->ToggleShowHide(true);
	}
}

//---- On Reset Notepanes -----------------------------------------------------

void	TopFrame::OnResetNotePanes(wxCommandEvent &e)
{
	assert(m_BottomNotePanel);
	
	m_BottomNotePanel->ResetNotesPanes();
}

//---- Update Var View Prefs --------------------------------------------------

void	TopFrame::UpdateVarViewPrefs(void)
{
	const uint32_t	mask = m_VarViewPrefs->GetMask();
	
	uLog(UI, "TopFrame::UpdateVarViewPrefs(mask = 0x%08x)", mask);
	
	GetSignals(PANE_ID_ALL_VAR_VIEWS).OnVarCheckboxes(CTX_WX, mask);
}

//---- On Toggle lx::Grid Overlays --------------------------------------------

void	TopFrame::OnToggleGridOverlays(wxCommandEvent &e)
{
	const bool	f = e.IsChecked();
	
	GetSignals(PANE_ID_ALL_VAR_VIEWS).OnGridOverlays(CTX_WX, f);
	
	e.Skip();
}

//---- On Toggle Show Variable Path ------------------------------------------

void	TopFrame::OnShowVarPaths(wxCommandEvent &e)
{
	const bool	f = e.IsChecked();
	
	const uint32_t	base_flags = (f ? VAR_FLAG_SHOW_PATH : 0) | (m_ShowScopeFlag ? VAR_FLAG_SHOW_SCOPE : 0);;
	
	GetSignals(PANE_ID_ALL_VAR_VIEWS).OnVarColumns(CTX_WX, base_flags);
	
	e.Skip();
}

//---- On Lua Suspended / Client WakeUp ---------------------------------------

void	TopFrame::OnLuaSuspended_SYNTH(wxThreadEvent &e)
{
	uLog(SYNTH, "TopFrame::OnLuaSuspended_SYNTH()");
	uLog(LOGIC, "Lua suspended");
	
	assert(m_Controller);
	assert(m_Client);
	
	m_Controller->SetInitLevel(INIT_LEVEL::LUA_PAUSED);
	
	m_VisibleStackLevel = 0;								// how OFTEN resets stack level?
	
	// show line
	m_Controller->ShowSourceAtLine(m_SuspendedState.m_Source, m_SuspendedState.m_Line, true/*focus?*/);
	
	// update local/global vars, callstack, etc.
	m_BottomNotePanel->UpdateVisiblePages();						// TRIGGERS more events?
	
	UpdateToolbarState();
	
	// Raise();
}

//---- Get Visible Stack Level ------------------------------------------------

int	TopFrame::GetVisibleStackLevel(void) const
{
	return m_VisibleStackLevel;
}

//---- Set Visible Stack Level ------------------------------------------------

void	TopFrame::SetVisibleStackLevel(const int stack_level)
{
	uLog(LUA_DBG, "new visible stack level %d", stack_level);
	assert(m_BottomNotePanel);
	
	m_VisibleStackLevel = stack_level;
	
	// update local/global vars, callstack, etc.
	m_BottomNotePanel->UpdateVisiblePages();						// TRIGGERS more events?
}	

//---- Check if is STILL Connected --------------------------------------------

bool	TopFrame::CheckConnection(void)
{
	assert(m_Client);
	assert(m_Controller);
	assert(m_TopNotePanel);
	assert(m_BottomNotePanel);
	
	if (m_Controller->GetInitLevel() == INIT_LEVEL::DISCONNECTED)		return false;		// wasn't connected
	
	const bool	was_connected_f = (m_Controller->GetInitLevel() >= INIT_LEVEL::CONNECTED);
	const bool	connected_f = m_Client->IsConnected();
	if (connected_f || (!was_connected_f))	return connected_f;
	
	uErr("SPURIOUS disconnection");

	// ShowNetworkToolTip("spurious disconnection", wxICON_ERROR);
	
	m_Controller->SetInitLevel(INIT_LEVEL::DISCONNECTED);
	
	m_TopNotePanel->UnhighlightSFCs();
	
	m_BottomNotePanel->ClearVarViews();
	
	UpdateToolbarState();
	
	return false;
}

//---- On Tool Connection Button ----------------------------------------------

void	TopFrame::OnToolConnection(wxCommandEvent &e)
{
	uLog(NET, "ClientFrame::OnToolConnection()");
	assert(m_Controller);
	assert(m_Client);
	assert(m_ToolBar);
	if (!m_ToolBar->GetToolEnabled(TOOL_ID_CONNECTION))	return;		// ignore (function keys are always wired)
	
	m_DaemonPlatform.Defaults();
	
	if (!m_Client->IsConnected())
	{	assert((m_Controller->GetInitLevel() == INIT_LEVEL::DISCONNECTED));
	
		if (m_Controller->GetSFCInstances().size() == 0)
		{	uErr("project contains no scripts");
			return;
		}
		
		RemoteServerConfig	remote = GetRemoteServerConfig();
		if (!remote.IsOk())
		{	uErr("RemoteServerConfig is not ok");
			return;
		}
		
		// uMsg("Trying to connect to daemon");
		
		const bool	ok = m_Client->ServerHandshake(remote);
		if (ok)
		{	uLog(NET, "  connection OK");
			ShowNetworkToolTip("Connected", wxICON_INFORMATION);
		}
		else
		{	uErr("  connection FAILED");
			ShowNetworkToolTip("Connection failed", wxICON_EXCLAMATION);
		}
		
		m_Controller->SetInitLevel(ok ? INIT_LEVEL::CONNECTED : INIT_LEVEL::DISCONNECTED);
		
		if (ok)
		{
			MemDataOutputStream	mos;
			
			mos << CLIENT_MSG::REQUEST_PLATFORM;
			
			m_Client->SendMessage(mos);
			
			WaitForNewMessage();
		}
	}
	else
	{	uLog(NET, "Disconnecting");
		m_Client->CloseConnection();
		m_Controller->SetInitLevel(INIT_LEVEL::DISCONNECTED);
		ShowNetworkToolTip("Disconnected", wxICON_INFORMATION);
	}
	
	UpdateToolbarState();
}

//---- Get Remote Server configuration ----------------------------------------

RemoteServerConfig	TopFrame::GetRemoteServerConfig(void)
{
	assert(m_DaemonHostNPortComboBox);
	
	RemoteServerConfig	remote;
	
	const string	host_n_nport = m_DaemonHostNPortComboBox->GetValue().ToStdString();
	const size_t	colon_pos = host_n_nport.find(':');
	if (colon_pos == string::npos)		return remote;		// incomplete
	
	const string	host = host_n_nport.substr(0, colon_pos);
	if (host.empty())			return remote;		// incomplete
	
	const string	remain = host_n_nport.substr(colon_pos + 1);
	if (remain.empty())			return remote;		// incomplete
	
	const int	port = Soft_stoi(remain, -1);
	if (port == -1)				return remote;		// incomplete
	
	remote.m_Host = host;
	remote.m_Port = port;
	
	return remote;
}

//---- Check Startup struct -----------------------------------------------------

bool	TopFrame::CheckStartupLuaStruct(void)
{
	assert(m_LuaStartupComboBox);
	assert(m_Controller);
	
	m_StartupLuaStruct.Clear();
	
	const string	s = m_LuaStartupComboBox->GetValue().ToStdString();
	if (s.empty())		return false;
	
	const regex	STARTUP_RE {"([^:]+):(.*)(\\(\\)?)"};
	smatch		matches;
	
	if (!regex_match(s, matches/*&*/, STARTUP_RE))		return false;		// not matched
	
	const size_t	n_matches = matches.size();
	if ((n_matches < 3) || (n_matches > 4))				return false;		// caca
	
	const string	start_script = matches[1];
	const string	start_func = matches[2];
	
	if (!m_Controller->GetSFC(wxString(start_script)))	return false;		// script not found
	
	m_StartupLuaStruct.m_Source = start_script;
	m_StartupLuaStruct.m_Function = start_func;
	m_StartupLuaStruct.m_JITFlag = m_ClientState->GetJitFlag();
	m_StartupLuaStruct.m_LoadBreakFlag = m_ClientState->GetLoadBreakFlag();
	m_StartupLuaStruct.m_NumSourceFiles = m_Controller->GetSFCInstances().size();
	
	return m_StartupLuaStruct.IsOk();
}

//---- On Start/Run Tool Button -----------------------------------------------

void	TopFrame::OnToolStartRun(wxCommandEvent &e)
{
	assert(m_Controller);
	assert(m_ToolBar);
	if (!m_ToolBar->GetToolEnabled(TOOL_ID_START_RUN))	return;		// ignore spurious function keys (are always wired)
	assert(m_Client && m_Client->IsConnected());
	
	if (m_Controller->HasDirtySFCEdit())
	{	::wxMessageBox("Save all source files first", "WARNING", wxCENTER | wxOK | wxICON_EXCLAMATION, this);
		return;
	}
	
	if (m_Controller->GetInitLevel() == INIT_LEVEL::CONNECTED)
	{	bool	ok = CheckStartupLuaStruct();
		if (!ok)
		{	// incomplete
			uErr("Startup field is INCOMPLETE");
			return;					
		}
	
		if (!m_StartupLuaStruct.IsOk())
		{	
			uErr("Startup field FAILED");
			return;
		}
		
		rootLog::Get().ClearLogAll();
		
		// send breakpoints FIRST (?)
		if (!CheckConnection())		return;
		ok = m_Client->SendBreakpointList();
		if (!CheckConnection())		return;
		assert(ok);
		
		// THEN send startup
		if (!CheckConnection())		return;
		ok = m_Client->SendSyncAndStartLua(m_StartupLuaStruct);
		if (!CheckConnection())		return;
		assert(ok);
		
		WaitForNewMessage();
	}
	else if (m_Controller->IsLuaVMResumable())
	{	
		CallAfter(&TopFrame::ResumeExecution, CLIENT_CMD::RUN);
	}
	else	uErr("illegal init level in TopFrame::OnToolStartStop()");
}

//---- Resume Execution -------------------------------------------------------

void	TopFrame::ResumeExecution(const CLIENT_CMD &cmd)
{
	uLog(LOGIC, "TopFrame::ResumeExecution(%s)", CommandName(cmd));
	
	assert(m_TopNotePanel);
	assert(m_Controller);
	assert(m_ToolBar);
	
	if (!CheckConnection())			return;
	if (!m_Controller->IsClientActive())	return;
	
	const bool	terminate_f = ((cmd == CLIENT_CMD::ABORT) || (cmd == CLIENT_CMD::END_SESSION));
	if (terminate_f)
	{	// nop ?
	}
	else
	{	// resume
		if (!m_ToolBar->GetToolEnabled(TOOL_ID_START_RUN))			return;		// ignore (function keys are always wired)
	}
	
	// breakpoints
	bool	ok = m_Client->SendBreakpointList(m_ClientState->GetGlobalBreakpointsFlag() && !terminate_f);		// don't send breakpoints on quit()
	if (!CheckConnection())		return;
	assert(ok);

	m_Controller->StepDaemonTick();

	// remove highlights on SFCs
	m_TopNotePanel->UnhighlightSFCs();
	
	// post RESUME message
	ResumeVM		resume_msg(cmd);
	MemDataOutputStream	mos;
	
	mos << CLIENT_MSG::REQUEST_VM_RESUME << resume_msg;
	m_Client->SendMessage(mos);
	
	WaitForNewMessage();
}

//---- Queue network message from daemon locally ------------------------------

bool	TopFrame::QueueMessage(const vector<uint8_t> &buff, bool immediate_f)
{
	uLog(NET, "TopFrame::QueueMessage(sz %zu)", buff.size());
	
	Message		msg(buff);
	
	// blocking
	m_ClientQueue.Put(msg);
	
	if (!immediate_f)
	{	// delayed, wake up own thread
		wxQueueEvent(this, new wxThreadEvent(wxEVT_THREAD, DBGF_ID_INCOMING_MESSAGE));
		return true;
	}
	else
	{	// process DIRECTLY
		return ProcessCustomQueue();
	}
}

//---- On Incoming Message thread event ---------------------------------------

void	TopFrame::OnIncomingMessage(wxThreadEvent &evt)
{
	int	n_pending = m_ClientQueue.NumPending();
	if (n_pending == 0)	return;		// shouldn't happen?
	
	uLog(NET, "TopFrame::OnIncomingMessage()");
	
	bool	ok = ProcessCustomQueue();
	if (!ok)	uErr("ProcessCustomQueue() FAILED");
	
	// re-count pending after processed
	n_pending = m_ClientQueue.NumPending();
	
	// repost to self if has more pending
	if (n_pending > 0)
	{	wxThreadEvent	e(wxEVT_THREAD, DBGF_ID_INCOMING_MESSAGE);

		wxQueueEvent(this, e.Clone());
	}
}

//---- Process Custom Queue ---------------------------------------------------

bool	TopFrame::ProcessCustomQueue(void)
{
	if (!m_ClientQueue.HasPending())	return false;
	
	Message	msg;
	
	if (!m_ClientQueue.Get(msg/*&*/))	return false;
	
	DispatchMessage(msg);
	
	return true;	// ok
}

//---- Wait for new message ---------------------------------------------------

void	TopFrame::WaitForNewMessage(void)
{		
	uLog(NET, "ClientFrame::WaitForNewMessage()");
	assert(m_Client);
	assert(m_Controller);
	
	m_Controller->SetInitLevel(INIT_LEVEL::WAIT_REPLY);
	
	UpdateToolbarState(true/*immediate*/);
	
	bool	ok = m_Client->StartReadMessage_Async();
	assert(ok);
	
	using namespace std::chrono;
	
	// start with FIRST GEAR
	m_AsyncTimerStartStamp = timestamp_t();

	uLog(TIMER, "TopFrame::WaitForNewMessage() start m_AsyncReadTimer");
	m_Timers[ASYNC_READ].Start(ASYNC_READ_GEAR_1_TIMER_MS, wxTIMER_ONE_SHOT);
}

//---- On UDP refresh Timer ---------------------------------------------------

void	TopFrame::OnUdpTimer(wxTimerEvent &e)
{
	uLog(TIMER, "TopFrame::OnUdpTimer()");
	assert(m_Client);
	
	m_Client->CheckUDPQueue();
}

//---- On Async Read Timer event ----------------------------------------------

	// (just the flimsy wx timer here, magic happends with ASIO)

void	TopFrame::OnAsyncReadTimer(wxTimerEvent &e)
{
	uLog(TIMER, "TopFrame::OnAsyncReadTimer()");
	
	assert(m_Client);
	assert(m_StatusBar);
	assert(m_Controller);
	
	if (!m_Client->HasPendingAsync())
	{	SetStatusError("WARNING: has no pending async read");
		return;
	}
	
	if (!CheckConnection())
	{	SetStatusError("DISCONNECTED during OnAsyncReadTimer()");
		return;
	}
	
	const uint32_t	elapsed_ms = m_AsyncTimerStartStamp.elap_ms();
	const int	gear = (elapsed_ms < ASYNC_READ_GEAR_SWITCH_DELAY_MS) ? 1 : 2;
	const int	gear_speed = (elapsed_ms < ASYNC_READ_GEAR_SWITCH_DELAY_MS) ? ASYNC_READ_GEAR_1_TIMER_MS : ASYNC_READ_GEAR_2_TIMER_MS;
	
	vector<uint8_t>	buff;
	
	const READ_ASYNC_RES	res = m_Client->ReceivedAsyncMessage(buff/*&*/);
	if (res != READ_ASYNC_RES::DONE)
	{
		if ((res == READ_ASYNC_RES::FAILED) || !CheckConnection())
		{	// network error
			uWarn("Network error in TopFrame::OnAsyncReadTimer()");
			m_Timers[ASYNC_READ].Stop();
			if (!m_Client->IsConnected())
			{	ShowNetworkToolTip("spurious disconnection during OnAsyncReadTimer()", wxICON_ERROR);
				m_StatusBar->SetStatusText("", 2);
				m_Controller->SetInitLevel(INIT_LEVEL::DISCONNECTED);
				m_BottomNotePanel->ClearVarViews();
				UpdateToolbarState();
			}
			// could be other error?
			return;
		}	
		
		if (gear > 1)
		{
			const uint32_t	elapsed_tick = elapsed_ms / 500;
			
			stringstream	ss;
			
			const timestamp_t	stamp(timestamp_t::FromDMS(elapsed_ms));
		
			ss << "async wait " << stamp.str(STAMP_FORMAT::HMS) << " " << SPINNING_PROGRESS_STRING_LUT[elapsed_tick % SPINNING_PROGRESS_STRING_LUT.length()];
		
			m_StatusBar->SetStatusText(wxString(ss.str()), 2);
		}
		
		uLog(TIMER, "TopFrame::OnAsyncReadTimer() start m_AsyncReadTimer, gear %d", gear_speed);
		m_Timers[ASYNC_READ].Start(gear_speed, wxTIMER_ONE_SHOT);
		return;
	}
	
	Message	msg(buff);
	
	DispatchMessage(msg);
	
	UpdateToolbarState();
	
	if (m_Controller->IsClientActive())
	{	// done
		m_Timers[ASYNC_READ].Stop();
		m_StatusBar->SetStatusText("", 2);
		return;
	}
	
	// restart async read
	bool	ok = m_Client->StartReadMessage_Async();
	assert(ok);
	
	// restart timer
	uLog(TIMER, "TopFrame::OnAsyncReadTimer() restart m_AsyncReadTimer, gear %d", gear_speed);
	m_Timers[ASYNC_READ].Start(gear_speed, wxTIMER_ONE_SHOT);
}

//---- On Tool Stop -----------------------------------------------------------

void	TopFrame::OnToolStop(wxCommandEvent &e)
{
	uMsg("TopFrame::OnToolStop()");
	assert(m_ToolBar);
	assert(m_Controller);
	// assert(m_Controller->IsLuaStarted());
	
	if (!m_ToolBar->GetToolEnabled(TOOL_ID_STOP))		return;		// ignore (function keys are always wired)
	
	ResumeExecution(CLIENT_CMD::END_SESSION);
}

//---- On Stop Lua Session ----------------------------------------------------

void	TopFrame::OnStopSession_SYNTH(wxThreadEvent &e)
{
	uLog(SYNTH, "TopFrame::OnStopSession_SYNTH()");
	assert(m_Controller);
	assert(m_Controller->IsLuaStarted());
	
	// tell daemon to stop & reloop
	ResumeExecution(CLIENT_CMD::END_SESSION);
}

//---- Dispatch Message -------------------------------------------------------

void	TopFrame::DispatchMessage(const Message &msg)
{
	assert(m_Controller);
	
	const size_t	sz = msg.GetSize();
	const uint8_t	*p = msg.GetPtr();
	
	MemDataInputStream	mis(p, sz);
	
	DAEMON_MSG	msg_t;
	
	mis >> msg_t;
	
	assert(msg_t != DAEMON_MSG::ILLEGAL_DAEMON_MSG);
	
	if (msg_t != DAEMON_MSG::NOTIFY_USER_LOG_SINGLE)
		uLog(MSG_IN, "dispatching %s", MessageName(msg_t));
	
	VAR_SOLVE_REQUESTER	requester_id = SOLVE_REQUESTER_ILLEGAL;
	
	switch (msg_t)
	{
		case DAEMON_MSG::NOTIFY_USER_LOG_SINGLE:
		{
			// unfold daemon user log
			timestamp_t	stamp;
			uint32_t	daemon_level;
			string		s;
			
			mis >> stamp >> daemon_level >> s;
				
			m_LogOutput->DaemonLog(stamp, daemon_level, s);
		}	break;
		
		case DAEMON_MSG::NOTIFY_USER_LOG_BATCH:
		{
			const uint32_t	n_logs(mis.Read32());
			
			for (int i = 0; i < n_logs; i++)
			{	
				// unfold daemon user log
				timestamp_t	stamp;
				uint32_t	daemon_level;
				string		s;
				
				mis >> stamp >> daemon_level >> s;
				
				m_LogOutput->DaemonLog(stamp, daemon_level, s);
			}
		}	break;
		
		case DAEMON_MSG::REPLY_PLATFORM:
		{
			const PlatformInfo	info(mis);
			
			uMsg("Daemon platform");
			uMsg("  OS %s (%d-bit)", info.OS(), info.Architecture());
			uMsg("  hostname %S", info.Hostname());
			uMsg("  %s", info.Lua());
			uMsg("  %s", info.LuaJIT());
			
			m_DaemonPlatform = info;
			// m_JITModeFlag = false;		// disabled by default
			
			const string	s = xsprintf("connected to %s, %s", info.Hostname(), info.HasJIT() ? info.LuaJIT() : info.Lua());
			SetStatusComment(s);
			
			// (reset init level so stops looping in WaitMessage())
			m_Controller->SetInitLevel(INIT_LEVEL::CONNECTED);
			
			UpdateToolbarState();
		}	break;
		
		case DAEMON_MSG::UNEXPECTED_CLIENT_MESSAGE_DURING_SYNC:
		case DAEMON_MSG::UNEXPECTED_CLIENT_MESSAGE_DURING_DEBUG:
		{	
			CLIENT_MSG	client_msg_t;
			
			mis >> client_msg_t;
		
			const string	original_msg_s = MessageName(client_msg_t);
			const string	err_msg = xsprintf("Daemon received %S at unexpected time (race condition?)", original_msg_s);
			
			uErr(err_msg);
			
			SetStatusError(err_msg, "Daemon Error");
		}	break;
		
		case DAEMON_MSG::NOTIFY_SYNC_FAILED:
		{
			const string	startup_err_msg = mis.ReadString();
			const string	err_msg = xsprintf("sync & start FAILED: %S", startup_err_msg);
			
			uErr(err_msg);
			
			SetStatusError(err_msg, "Daemon Error");
				
			m_Controller->SetInitLevel(INIT_LEVEL::CONNECTED);
		}	break;
		
		case DAEMON_MSG::NOTIFY_SCRIPT_LOADED:
		{
			// will also receive fillenames NOT in project
			const StringList	script_names {mis};
			
			uLog(LUA_DBG, "loaded %zu scripts", script_names.size());
			
			for (auto it : script_names)
			{
				uLog(LUA_DBG, "  script %S", it);
			}
		}	break;
		
		case DAEMON_MSG::NOTIFY_VM_SUSPENDED:
		{	
			const SuspendState	suspended(mis);
			
			// store in globals
			m_SuspendedState = suspended;
			
			if (suspended.HasError())
			{	// Lua error
				m_Controller->SetInitLevel(INIT_LEVEL::LUA_FATAL_ERROR);
				
				// OnShowLocation
				
				ISourceFileClass	*sfc = m_Controller->GetSFC(suspended.m_Source);		// caca - use SIGNAL
				if (sfc)
				{	sfc->ShowNotebookPage(true);
					
					const int	ln = suspended.m_Line;
					
					sfc->GetEditor().ShowLine(ln, true/*focus?*/);
					// sfc->SetRedAnnotation(ln, suspended.m_ErrorString);
				}
				else
				{	uErr("Error: source %S is not part of project", suspended.m_Source);
				}
			}
			else	m_Controller->SetInitLevel(INIT_LEVEL::LUA_PAUSED);
			
			// post to self
			wxThreadEvent	e(wxEVT_THREAD, DBGF_ID_LUA_SUSPENDED);			// WILL RESET STATE!
			
			wxQueueEvent(this, e.Clone());
		}	break;
		
		case DAEMON_MSG::REPLY_STACK:
		{
			assert(m_TraceBackCtrl);
			
			m_ClientStack.reset(new ClientStack(mis));
			
		}	break;
		
		case DAEMON_MSG::REPLY_LOCALS:
		
			requester_id = (VAR_SOLVE_REQUESTER) mis.Read32();
			
			GetSignals(requester_id).OnCollectedMessage(CTX_WX, msg);
			
			break;
		
		case DAEMON_MSG::REPLY_GLOBALS:
		
			requester_id = (VAR_SOLVE_REQUESTER) mis.Read32();
			
			GetSignals(requester_id).OnCollectedMessage(CTX_WX, msg);
			break;
		
		case DAEMON_MSG::REPLY_WATCHES:
		
			requester_id = (VAR_SOLVE_REQUESTER) mis.Read32();
			
			GetSignals(requester_id).OnCollectedMessage(CTX_WX, msg);
			break;
		
		case DAEMON_MSG::REPLY_SOLVED_VARIABLE:
		{	
			requester_id = (VAR_SOLVE_REQUESTER) mis.Read32();
			
			const CollectedList	collist{mis};
			assert(collist.size() >= 1);
	
			Collected	ce {collist[0]};
				
			switch (requester_id)
			{
				case SOLVE_REQUESTER_MEMORY_VIEW:
			
					m_Controller->GetSignals().OnShowMemory(CTX_WX, ce.GetKey(), ce.GetVal(), ce.GetTypeString(), ce.IsBinaryData());
					break;
				
				case SOLVE_REQUESTER_HOVER:
					
					m_TopNotePanel->SetSolvedVariable(ce);
					break;
				
				default:
					
					assert(false);
					break;
			}
		}	break;
		
		case DAEMON_MSG::NOTIFY_SESSION_ENDED:
		{
			const uint32_t	res = mis.Read32();
			
			uLog(LUA_DBG, "session ended with 0x%04x", res);				// should check for any daemon error
			
			// m_Controller->SetInitLevel(INIT_LEVEL::SESSION_ENDED);
			
			m_Controller->SetInitLevel(INIT_LEVEL::CONNECTED);		// RELOOPs
			SetStatusComment("SESSION ENDED, relooping");
			
			// enable editing
			m_Controller->SetAllSFCEditLockFlag(false);
			
			// reset any Lua error annotations
			m_TopNotePanel->UnhighlightSFCs();
			
			m_BottomNotePanel->ClearVarViews();
			
			UpdateToolbarState();
		}	break;
			
		default:
		{	const string	unknown_msg_s = MessageName(msg_t);
			
			uErr("error - unknown DAEMON_MSG %S", unknown_msg_s);
			assert(false);
		}	break;
	}
}

//---- Set Status Error -------------------------------------------------------

void	TopFrame::SetStatusError(const string &msg, const string &title)
{
	uErr("SetStatusError(%s)", msg);
	
	assert(m_StatusBar);
	
	m_StatusBar->SetForegroundColour(*wxRED);
	m_StatusBar->SetStatusText(wxString(msg), 0);
	m_StatusBar->Refresh();
	
	PopNotificationBubble(msg, title);
}

//---- Show Network ToolTip ---------------------------------------------------

void	TopFrame::ShowNetworkToolTip(const string &msg, const uint32_t icon_id)
{
	PopNotificationBubble(msg, "Network", icon_id);
}

//---- Pop Notification Error -------------------------------------------------

void	TopFrame::PopNotificationBubble(const string &msg, const string &title, const uint32_t icon_id)
{
	assert(m_NotificationMessage);
	
	m_NotificationMessage->SetTitle(wxString(title));
	m_NotificationMessage->SetMessage(wxString(msg));
	m_NotificationMessage->SetFlags(icon_id);
	m_NotificationMessage->Show();
	
	// to auto-hide (otherwise will stay on)
	MixDelay("notification_msg", NOTIFICATION_MESSAGE_TIMEOUT_MS, this, &TopFrame::OnNotificationTimer);
}

//---- On Notification Timer  -------------------------------------------------

void	TopFrame::OnNotificationTimer(void)
{
	assert(m_NotificationMessage);
	
	m_NotificationMessage->Close();
}

//---- Set Status Comment -----------------------------------------------------

void	TopFrame::SetStatusComment(const wxString &msg)
{
	assert(m_StatusBar);
	
	m_StatusBar->SetForegroundColour(*wxBLACK);
	m_StatusBar->SetStatusText(msg, 0);
	m_StatusBar->Refresh();
}

//---- Load Project File (manually) -------------------------------------------

bool	TopFrame::LoadProjectFile(const wxString &fn)
{
	assert(m_Controller);
	uMsg("TopFrame::LoadProjectFile(%S)", fn);
	
	bool	ok = m_ProjectPrefs->LoadProject(fn);
	
	return ok;
}

//---- On Open Project or Lua Files -------------------------------------------

void	TopFrame::OnOpenProjectOrLuaFiles(wxCommandEvent &e)
{
	assert(m_Controller);
	
	// can multi-select
	wxFileDialog	dlg(	this, "Open Lua File(s)", m_LastUserOpenDir, "",
				"ddt & lua files (*.ddt; *.lua)|*.ddt;*.lua|ddt files (*.ddt)|*.ddt|lua files (*.lua)|*.lua|All files (*.*)|*.*",
				wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
				
	const int	res = dlg.ShowModal();
	if (res == wxID_CANCEL)		return;
	
	wxArrayString	path_list, file_list;
	dlg.GetPaths(path_list/*&*/);
	dlg.GetFilenames(file_list/*&*/);
	if (path_list.Count() == 0)	return;
	
	wxFileName	cdn(dlg.GetPath());
	assert(cdn.IsOk());
	m_LastUserOpenDir = cdn.GetPath();
	
	// count # projects & lua files
	int	n_proj = 0, n_lua = 0;
	
	for (int i = 0; i < path_list.Count(); i++)
	{
		wxFileName	cfn(path_list[i]);
		assert(cfn.IsOk() && cfn.FileExists());
		
		const wxString	ext = cfn.GetExt().Lower();
		
		if (ext == "ddt")	n_proj++;
		else if (ext == "lua")	n_lua++;
		else
		{	uErr("illegal file type %S", cfn.GetFullPath());
			return;
		}
	}
	
	// check didn't select projects AND lua files, or multiple projects
	if ((n_proj * n_lua != 0) || (n_proj > 1))
	{
		uErr("illegal selection");
		return;
	}
	
	if (n_proj == 1)
	{	// project
		bool	ok = m_ProjectPrefs->LoadProject(path_list[0]);
		assert(ok);
		
		// add MRU entry
		AddProjectMRUEntry(path_list[0].ToStdString());
	}
	else if (n_lua > 0)
	{	// lua files, open all
		for (int i = 0; i < path_list.Count(); i++)
		{
			const wxString	path = path_list[i];
			const wxString	fn = file_list[i];
			
			const bool	f = m_Controller->HasDoppleganger(fn);
			if (f)
			{	uErr("skipping duplicate %S", path);
				continue;
			}
			
			// instantiate new
			ISourceFileClass	*sfc = m_Controller->NewSFC(path);
			assert(sfc);
			
			sfc->ShowNotebookPage(true);
		}
	}
	else	uErr("failed ddt/lua file selection");
}

//---- On MRU Menu Selected event ---------------------------------------------

void	TopFrame::OnMRUMenuSelected(wxCommandEvent &e)
{
	assert(m_ToolBar);
	
	const int	id = e.GetId() - PROJECT_MRU_START;
	assert((id >= 0) && (id < PROJECT_MRU_MAX));
	
	const string	bmk = m_ProjectBookmarks.at(id);
	assert(!bmk.empty());
	
	// uMsg("TopFrame::OnMRUMenuSelected(\"%s\")", wxString(bmk));
	
	bool	ok = m_ProjectPrefs->LoadProject(wxString(bmk));
	if (!ok)
	{	uErr("failed to load prefs!");
		return;
	}
	
	AddProjectMRUEntry(bmk);
}

//---- On Clear MRU Menu event ------------------------------------------------

void	TopFrame::OnMRUMenuClear(wxCommandEvent &e)
{
	assert(m_ToolBar);
	
	m_ProjectBookmarks.clear();
	
	DirtyUIPrefs();
	
	// HAS to be delayed
	CallAfter(&TopFrame::RebuildProjectMRUMenu);
}

//---- Add Project MRU Entry --------------------------------------------------

void	TopFrame::AddProjectMRUEntry(const string &bmk)
{
	assert(m_ToolBar);
	
	// delete any existing
	m_ProjectBookmarks.Erase(bmk);
	
	// insert new at front
	m_ProjectBookmarks.insert(m_ProjectBookmarks.begin(), bmk);
	
	// clamp (foireux?)
	while (m_ProjectBookmarks.size() > PROJECT_MRU_MAX)	m_ProjectBookmarks.pop_back();
	
	DirtyUIPrefs();
	
	// HAS to be delayed
	CallAfter(&TopFrame::RebuildProjectMRUMenu);
}

//---- Rebuild Project MRU menu -----------------------------------------------

void	TopFrame::RebuildProjectMRUMenu(void)
{
	assert(m_ToolBar);
	
	// rebuild menu
	wxMenu	*mru_menu = new wxMenu();
	
	for (int i = 0; i < m_ProjectBookmarks.size(); i++)
	{
		const wxString	title{m_ProjectBookmarks[i]};
		
		mru_menu->Append(PROJECT_MRU_START + i, title);		// split setMRU / update menu events?
	}
	
	mru_menu->AppendSeparator();
	mru_menu->Append(PROJECT_MRU_CLEAR, "Clear");
	
	m_ToolBar->SetDropdownMenu(TOOL_ID_OPEN_FILES, mru_menu);
	
	// m_ToolBar->Realize();
}

//---- Set Project MRU Bookmarks (from UI load) -------------------------------

void	TopFrame::SetProjectBookmarkList(const StringList &bookmarks)
{
	m_ProjectBookmarks.clear();
	
	for (const auto file_path : bookmarks)
	{
		const string	native_path = FileName::UnixToNativePath(file_path);

		// make sure paths exists
		if (!wxFileName::FileExists(wxString{native_path}))		continue;	// skip project files that no longer exist
		
		m_ProjectBookmarks.push_back(file_path);
	}
	
	CallAfter(&TopFrame::RebuildProjectMRUMenu);
}

//---- On Close All Lua Files -------------------------------------------------

void	TopFrame::OnCloseAllLuaFiles(wxCommandEvent &e)
{
	assert(m_TopNotePanel);
	
	m_TopNotePanel->HideAllSFCs();
}

//---- On Close Project -------------------------------------------------------

void	TopFrame::OnCloseProject(wxCommandEvent &e)
{
	assert(m_ProjectPrefs);
	
	if (m_ProjectPrefs->IsLoaded())
	{	if (wxYES != ::wxMessageBox("Close this project?", "Confirm", wxYES_NO | wxICON_QUESTION)) 	return;
	}
	
	m_ProjectPrefs->CloseProject();	
}

//---- On Save Project --------------------------------------------------------

void	TopFrame::OnSaveProject(wxCommandEvent &e)
{
	assert(m_ProjectPrefs);
	
	m_ProjectPrefs->SaveProject(true/*force*/);
}

//---- On Save Project As event -----------------------------------------------

void	TopFrame::OnSaveProjectAs(wxCommandEvent &e)
{
	assert(m_ProjectPrefs);
	
	bool	ok = m_ProjectPrefs->PromptSaveNewProject("Save Project As");
	if (!ok)	return;		// canceled
	
	m_ProjectPrefs->SaveProject(true/*force*/);
}

//---- On Save Dirty Project Prefs  -------------------------------------------

void	TopFrame::OnSaveProjectPrefs(void)
{
	assert(m_ProjectPrefs);
	
	m_ProjectPrefs->OnSavePrefsTimerLapsed();
	
	const wxString	s = m_DaemonHostNPortComboBox->GetValue();
	bool	ok = s.Contains(":");
	m_DaemonHostNPortComboBox->SetBackgroundColour(*((ok ? STARTUP_COMBO_COLOR_OK : STARTUP_COMBO_COLOR_FAIL).ToWxColor()));
	
	ok = CheckStartupLuaStruct();
	m_LuaStartupComboBox->SetBackgroundColour(*((ok ? STARTUP_COMBO_COLOR_OK : STARTUP_COMBO_COLOR_FAIL).ToWxColor()));
}

//---- On Force Pane Reload ---------------------------------------------------

void	TopFrame::OnForcePaneReload(wxCommandEvent &e)
{
	uMsg("TopFrame::OnNetStressTest()");
	
	m_BottomNotePanel->UpdateVisiblePages();
}

//---- On Network Stress Test -------------------------------------------------

void	TopFrame::OnNetStressTest(wxCommandEvent &e)
{
	uMsg("TopFrame::OnNetStressTest()");
	
	assert(m_Client);
	if (!m_Client->IsConnected())
	{
		uErr(" error - client isn't connected");
		return;
	}
	
	const double	throughput = m_Client->NetStressTest(10/*rounds*/, 1'000'000/*sz*/, 0.10/*variance*/, 0/*frame_sz*/);
	if (throughput <= 0)
	{
		wxMessageBox("FAILED", "Network Stress Test", wxOK | wxCENTER, this);
	}
	else
	{			
		const double	mbps = throughput / ((1024 * 1024) / 8);
		
		uMsg("  throughput = %g Bytes/s (%g mb/s)", throughput, mbps);
		
		PopNotificationBubble("success", "Network Stress Test", wxICON_EXCLAMATION);
	}
}

//---- On Dump Fixed Font -----------------------------------------------------

void	TopFrame::OnDumpFixedFont(wxCommandEvent &e)
{
	uMsg("TopFrame::OnDumpFixedFont()");
	
	m_Controller->GetFixedFont()->Dump();
}

//---- On Clear Log -----------------------------------------------------------

void	TopFrame::OnClearLog(wxCommandEvent &e)
{
	rootLog::Get().ClearLogAll();
}

//---- Send Request / Reply ---------------------------------------------------

void	TopFrame::SendRequestReply(MemoryOutputStream &mos)
{
	uLog(NET, "TopFrame::SendRequestReply()");
	
	assert(m_Controller);
	assert(m_Client);
	
	assert(m_Controller->IsLuaListening());

	m_Client->RequestReply(mos);
}

//---- Solve STC Variable (ASYNCHRONOUS) --------------------------------------

bool	TopFrame::SolveVar_ASYNC(const VAR_SOLVE_REQUESTER &requester_id, const string &var_name, const uint32_t &var_flags)
{
	assert(m_Controller);
	assert(m_Client);
	
	if (!m_Controller->IsLuaListening())		return false;
	
	MemDataOutputStream	mos;
	
	mos << CLIENT_MSG::REQUEST_SOLVE_VARIABLE;
	mos << VariableRequest{requester_id, var_name};
	
	bool	ok = m_Client->RequestReply(mos);
	return ok;
}

//---- Get Watch Bag ----------------------------------------------------------

WatchBag&	TopFrame::GetWatchBag(void)
{
	assert(m_WatchBag);
	
	return *m_WatchBag;
}

//---- Get Search Results Panel -----------------------------------------------

SearchResultsPanel*	TopFrame::GetSearchResultsPanel(void)
{
	assert(m_SearchResultsPanel);
	
	return m_SearchResultsPanel;
}

//---- Set Memory Watch -------------------------------------------------------

void	TopFrame::SetMemoryWatch(const Collected &ce)
{
	assert(m_MemoryViewCtrl);
	
	m_MemoryWatch = ce.GetKey();
	
	m_Controller->GetSignals().OnShowMemory(CTX_WX, ce.GetKey(), ce.GetVal(), ce.GetTypeString(), ce.IsBinaryData());
	
	// CheckMemoryWatch();
}

//---- Chech Memory Watch -----------------------------------------------------

void	TopFrame::CheckMemoryWatch(void)
{
	assert(m_MemoryViewCtrl);
	assert(m_Client);
	
	if (m_MemoryWatch.empty())	return;
	if (!m_Client->IsConnected())	return;		// not connected - ignore
	
	SolveVar_ASYNC(SOLVE_REQUESTER_MEMORY_VIEW, m_MemoryWatch);
}

//---- Get ClientStack --------------------------------------------------------

shared_ptr<ClientStack>	TopFrame::GetClientStack(void)
{
	return m_ClientStack;
}

//---- On Step Into -----------------------------------------------------------

void	TopFrame::OnStepInto(wxCommandEvent &e)
{
	assert(m_ToolBar);
	if (!m_ToolBar->GetToolEnabled(TOOL_ID_STEP_INTO))	return;		// ignore (function keys are always wired)
	
	ResumeExecution(m_ClientState->GetStepModeLinesFlag() ? CLIENT_CMD::STEP_INTO_LINE : CLIENT_CMD::STEP_INTO_INSTRUCTION);
}

//---- On Step Over -----------------------------------------------------------

void	TopFrame::OnStepOver(wxCommandEvent &e)
{
	assert(m_ToolBar);
	if (!m_ToolBar->GetToolEnabled(TOOL_ID_STEP_OVER))	return;		// ignore (function keys are always wired)
	
	ResumeExecution(CLIENT_CMD::STEP_OVER);
}	

//---- On Step Out ------------------------------------------------------------

void	TopFrame::OnStepOut(wxCommandEvent &e)
{
	assert(m_ToolBar);
	if (!m_ToolBar->GetToolEnabled(TOOL_ID_STEP_OUT))	return;		// ignore (function keys are always wired)
	
	ResumeExecution(CLIENT_CMD::STEP_OUT);
}

//---- Is Screencast Mode ? ---------------------------------------------------

bool	TopFrame::IsScreencastMode(void) const
{
	assert(m_OptionsMenu);
	
	const bool	f = m_OptionsMenu->IsChecked(TOGGLE_MENU_ID_SCREENCAST_MODE);

	return f;
}

//---- On Click Circle --------------------------------------------------------

void	TopFrame::OnClickCircle(const int layer)
{
	if (!m_OverlayFrame || !IsScreencastMode())		return;
	
	m_OverlayFrame->StartAnimation(layer);
}

//---- On Screencast Mode check-menu event ------------------------------------

void	TopFrame::OnScreencastMode(wxCommandEvent &e)
{
	e.Skip();
}

#if RUN_UNIT_TESTS

//==== UNIT TEST ==============================================================

TEST_CASE("check FileName ops", "[filename]")
{
	uLog(UNIT, "TEST_CASE_xxx begin");
	
	SECTION("SplitUnixPath full path", "")
	{
		const auto	split = FileName::SplitUnixPath("/home/plaufenb/development/inhance_depot/DDT3/lua/ddt_demo.lua");
		
		REQUIRE(split);
		REQUIRE(split.Path() == "/home/plaufenb/development/inhance_depot/DDT3/lua/");
		REQUIRE(split.Name() == "ddt_demo");
		REQUIRE(split.Ext() == "lua");
		REQUIRE(split.IsAbsolute());
	}
	
	SECTION("SplitUnixPath no dir", "")
	{
		const auto	split = FileName::SplitUnixPath("ddt_demo.lua");
		
		REQUIRE(split);
		REQUIRE(split.Path() == "");
		REQUIRE(split.Name() == "ddt_demo");
		REQUIRE(split.Ext() == "lua");
		REQUIRE(!split.IsAbsolute());
	}
	
	SECTION("SplitUnixPath rel dir", "")
	{
		const auto	split = FileName::SplitUnixPath("rel_dir/ddt_demo.lua");
		
		REQUIRE(split);
		REQUIRE(split.Path() == "rel_dir/");
		REQUIRE(split.Name() == "ddt_demo");
		REQUIRE(split.Ext() == "lua");
		REQUIRE(!split.IsAbsolute());
	}
	
	SECTION("GetPathSubDirs full path", "")
	{
		const auto	dirs = FileName::GetPathSubDirs("/home/plaufenb/development/inhance_depot/DDT3/lua/ddt_demo.lua");
		
		uLog(UNIT, " %zu dirs", dirs.size());
		uLog(UNIT, " last dir = %S", dirs.back());
		
		REQUIRE(dirs.size() == 7);
		REQUIRE(dirs[0] == "/");
		REQUIRE(dirs[1] == "home");
		REQUIRE(dirs[2] == "plaufenb");
		REQUIRE(dirs[3] == "development");
		REQUIRE(dirs[4] == "inhance_depot");
		REQUIRE(dirs[5] == "DDT3");
		REQUIRE(dirs[6] == "lua");
	}
	
	SECTION("FileExtension", "")
	{
		REQUIRE(FileName::HasFileExtension("/home/plaufenb/development/inhance_depot/DDT3/lua/ddt_demo.lua"));
		REQUIRE(!FileName::HasFileExtension("/home/plaufenb/development/inhance_depot/DDT3/lua/"));
		REQUIRE(!FileName::HasFileExtension("/home/plaufenb/development/inhance_depot/DDT3/lua"));
	}
	
	uLog(UNIT, "TEST_CASE_xxx end");
}

#endif // RUN_UNIT_TESTS

// nada mas
