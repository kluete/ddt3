// ddt client TopFrame

#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <memory>

#include "wx/wx.h"
#include "wx/imaglist.h"
#include "wx/overlay.h"
#include "wx/tglbtn.h"

#include "lx/xutils.h"
#include "lx/juce/JuceApp.h"
#include "lx/juce/jfont.h"

#include "ddt/sharedDefs.h"
#include "ddt/CommonClient.h"
#include "ddt/CommonDaemon.h"
#include "ddt/MessageQueue.h"

#include "UIConst.h"
#include "sigImp.h"

#include "VarViewCtrl.h"

#define	LOAD_CRAW(nm)		LoadCraw(nm##_craw.m_Data, nm##_craw.m_Len, (wxBitmapType) nm##_craw.m_Type)
#define	LOAD_CRAW_BUFF(nm)	LoadCrawBuff(nm##_craw.m_Data, nm##_craw.m_Len)

// forward declarations
class wxFileConfig;
class wxSplitterWindow;
class wxSplitterEvent;
class wxNotificationMessage;

namespace LX
{
	class MemoryOutputStream;
	class MemDataOutputStream;
	class Message;
	class Renderer;
	class ModalTextEdit;
	
	class IJuceApp;
	
} // namespace LX

namespace DDT_CLIENT
{
	// import whole namespace
	using namespace DDT;
}

namespace DDT_CLIENT
{

class ILogOutput;
class ISoloLogFrame;
class IPaneMixIn;
class IPaneComponent;
class IClientState;
class IBottomNotePanel;
class IVarViewCtrl;
class IVarViewFlags;

class TopNotePanel;
class Controller;
class ProjectPrefs;
class EditStatusBar;
class SearchBar;
class Client;
class WatchBag;
class SearchResultsPanel;
class LogToVC;
class OverlayFrame;
class LuaTextCtrl;

using std::string;
using std::vector;
using std::unordered_map;
using std::shared_ptr;

using LX::MemDataOutputStream;

class RemoteServerConfig
{
public:
	// ctor
	RemoteServerConfig();
	
	bool	IsOk(void) const;
	string	Host(void) const;
	int	Port(void) const;
	string	Path(void) const;
	
	string	m_Host;
	int	m_Port;
	string	m_Path;
};

//---- Debug Frame ------------------------------------------------------------

class TopFrame: public wxFrame, private LX::SlotsMixin<LX::CTX_WX>
{
	friend class ProjectPrefs;
	friend class Controller;
	friend class Client;
	
public:
	TopFrame(wxApp &app, IJuceApp &japp, const string &appPath);
	virtual ~TopFrame();
	
	// functions
	bool		LoadProjectFile(const wxString &fn);
	wxBitmap	LoadCraw(const uint8_t *dataptr, const size_t &len, const wxBitmapType &typ);
	vector<uint8_t>	GetIconStream(const int &icon_id);
	
	void	DirtyProjectPrefs(void);
	void	SetStatusComment(const wxString &msg);
	
	void	SetMemoryWatch(const Collected &ce);
	void	CheckMemoryWatch(void);
	
	void	RefreshWatches(const bool &save_f = false);
	bool	CheckWatchVariable(wxString &name);			// returns [ok]
	bool	AddWatchVariable(wxString name);			// returns [ok]
	bool	EditWatchVariable(int index, wxString name);		// returns [ok]
	bool	RemoveWatchVariable(int index);				// returns [ok]
	
	Controller&	GetController(void);
	
	bool	SolveVar_ASYNC(const VAR_SOLVE_REQUESTER &requester_id, const string &var_name, const uint32_t &var_flags = VAR_VIEW_XOR_MASK);
	
	bool	SolveLuaFunctions(const VAR_SOLVE_REQUESTER &requester_id);
	
	void	DirtyUIPrefs(const bool immediate_f = false);
	
	shared_ptr<ClientStack>	GetClientStack(void);
	
	WatchBag&	GetWatchBag(void);				// shouldn't be here?
	void		SendRequestReply(MemoryOutputStream &mos);
	
	void	StartNotepageDrag(const vector<wxRect> &hit_list);
	
	void	UpdateVarViewPrefs(void);
	
	SearchResultsPanel*	GetSearchResultsPanel(void);
	
	static bool		IsKnownID(const int &win_id);
	static wxString		GetKnownParent(const wxWindow *w, int depth = 0);
	static string		GetWinIDName(const int &win_id);
	static void		LogSortedWinIDMap(void);
	static void		DumpWindow(const wxWindow *w, wxString name = "", const int indent = 0);
	static void		DumpSplitter(wxSplitterWindow *sw, wxString name = "");
	
	bool	IsScreencastMode(void) const;
	void	OnClickCircle(const int layer);
	
	void	SetVisibleStackLevel(const int stack_level);
	int	GetVisibleStackLevel(void) const;
	
	VarViewSignals&	GetSignals(const int varview_id);
	
	wxApp&		GetApp(void)			{return m_wxApp;}
	
	IVarViewFlags&	GetVarViewFlags(void)		{return *m_VarViewPrefs;}
	
private:
	
	void	InitMenu(void);
	
	void	OnAllRealized(void);
	
	void	LoadUIConfig(void);
	void	SaveUIConfig(void);
	
	void	CreateToolbar(void);
	void	InitCairoRenderer(void);
	
	void	UpdateToolbarState(const bool immediate_f = false);
	void	UpdateLEDPosition(void);
	void	UpdateThrobber(void);

// events

	void	OnToolConnection(wxCommandEvent &e);
	void	OnToolStartRun(wxCommandEvent &e);
	void	OnStepInto(wxCommandEvent &e);
	void	OnStepOver(wxCommandEvent &e);
	void	OnStepOut(wxCommandEvent &e);
	void	OnToggleGlobalBreakpoints(wxCommandEvent &e);
	void	OnToolStop(wxCommandEvent &e);
	void	OnAbort(wxCommandEvent &e);
	
	void	OnEscapeKey(wxCommandEvent &e);
	
	void	OnShowVarPaths(wxCommandEvent &e);
	void	OnDumpNotebookVars(wxCommandEvent &e);
	void	OnForcePaneReload(wxCommandEvent &e);
	
	void	OnOpenProjectOrLuaFiles(wxCommandEvent &e);
	
	void	AddProjectMRUEntry(const string &bmk);
	void	OnMRUMenuSelected(wxCommandEvent &e);
	void	OnMRUMenuClear(wxCommandEvent &e);
	void	SetProjectBookmarkList(const StringList &bookmarks);
	void	RebuildProjectMRUMenu(void);
	
	void	OnSaveProject(wxCommandEvent &e);
	void	OnSaveProjectAs(wxCommandEvent &e);
	void	OnCloseProject(wxCommandEvent &e);
	void	OnCloseAllLuaFiles(wxCommandEvent &e);
	
	void	OnCloseEvent(wxCloseEvent &e);
	void	OnMoveEvent(wxMoveEvent &e);
	void	OnSizeEvent(wxSizeEvent &e);
	void	OnSizingEvent(wxSizeEvent &e);
	
	void	OnMainHSplitterSashChanged(wxSplitterEvent &e);
	void	OnPaneBookSplitterSashChanged(wxSplitterEvent &e);
	
	void	OnSoloLogWindow(wxCommandEvent &e);
	void	OnResetNotePanes(wxCommandEvent &e);
	
	void	OnSerializeBitmapFile(wxCommandEvent &e);
	void	OnSaveScreenshot(wxCommandEvent &e);
	void	OnClearLog(wxCommandEvent &e);
	void	OnNetStressTest(wxCommandEvent &e);
	void	OnDumpFixedFont(wxCommandEvent &e);
	
	void	OnToggleJITMode(wxCommandEvent &e);
	void	OnToggleLoadBreakMode(wxCommandEvent &e);
	void	OnToggleStepMode(wxCommandEvent &e);
	
	void	OnComboBoxKeyDown(wxKeyEvent &e);
	void	OnDaemonHostComboBox(wxCommandEvent &e);
	void	OnLuaMainComboBox(wxCommandEvent &e);
	
	void	OnSaveProjectPrefs(void);
	
	void	OnIncomingMessage(wxThreadEvent &e);
	void	OnLuaSuspended_SYNTH(wxThreadEvent &e);
	void	OnStopSession_SYNTH(wxThreadEvent &e);
	
	void	ClearBottomVarViewCtrl(void);
	void	ResumeExecution(const CLIENT_CMD &cmd);
	
	bool	CheckStartupLuaStruct(void);
	
	bool	QueueMessage(const vector<uint8_t> &buff, bool immediate_f);
	bool	CheckConnection(void);
	void	WaitForNewMessage(void);
	void	OnAsyncReadTimer(wxTimerEvent &e);
	
	bool	ProcessCustomQueue(void);
	void	DispatchMessage(const LX::Message &msg);
	
	void	SetStatusError(const string &msg, const string &title = "Error");
	void	PopNotificationBubble(const string &msg, const string &title, const uint32_t icon_id = wxICON_ERROR);
	void	ShowNetworkToolTip(const string &msg, const uint32_t icon_id);
	void	OnNotificationTimer(void);
	
	void	OnUdpTimer(wxTimerEvent &e);
	
	void	OnScreencastMode(wxCommandEvent &e);
	void	OnToggleGridOverlays(wxCommandEvent &e);
	
	bool	SerializeOneBitmapFile(const wxString &src_fn, const wxString &dest_fn);
	void	DoSaveScreenshot(void);
	
	StringList	LoadUIGroupEntries(const wxString &group);
	void		SaveUIGroupEntries(const wxString &group, const StringList &entries);
	void		ClipWindowRect(const vector<wxRect> &displayRects, wxRect &frame_rc) const;
	
	RemoteServerConfig	GetRemoteServerConfig(void);
	
	wxApp				&m_wxApp;
	IJuceApp			&m_jApp;
	shared_ptr<IFixedFont>		m_FixedFont;
	wxImageList			m_AuxBitmaps;
	
	Controller			*m_Controller;
	
	Client				*m_Client;	
	wxSplitterWindow		*m_MainSplitterWindow;
	
	unique_ptr<ITopNotePanel>	m_TopNotePanel;
	unique_ptr<IBottomNotePanel>	m_BottomNotePanel;
	
	wxStatusBar			*m_StatusBar;
	
	LX::Renderer			*m_CairoRenderer;
	
	ISoloLogFrame			*m_SoloLogFrame;

	wxComboBox			*m_DaemonHostNPortComboBox, *m_LuaStartupComboBox;
	
	wxString			m_LastUserOpenDir;
	
	unique_ptr<ILogOutput>		m_LogOutput;
	unique_ptr<IPaneComponent>	m_FunctionsComponent, m_VarViewComponent;
	
	IPaneMixIn			*m_OutputTextCtrl;
	IPaneMixIn			*m_ProjectViewCtrl;
	IVarViewCtrl			*m_LocalsVarViewCtrl, *m_GlobalsVarViewCtrl, *m_WatchVarViewCtrl;
	IPaneMixIn			*m_TraceBackCtrl;
	IPaneMixIn			*m_BreakpointsListCtrl;
	IPaneMixIn			*m_MemoryViewCtrl;
	SearchResultsPanel		*m_SearchResultsPanel;
	IPaneMixIn			*m_FunctionsPanel;
	
	IPaneMixIn			*m_jLocalsCtrl;
	
	wxFileConfig			*m_UIConfig;
	unique_ptr<IVarViewFlags>	m_VarViewPrefs;
	bool				m_ShowScopeFlag;
	wxMenuBar			*m_MenuBar;
	wxMenu				*m_FileMenu, *m_EditMenu, *m_ToolsMenu, *m_OptionsMenu, *m_WindowsMenu;
	wxToolBar			*m_ToolBar;
	StringList			m_ProjectBookmarks;
	
	vector<wxAcceleratorEntry>	m_KeyboardShortcuts;
	
	wxBitmap			m_ToolBarBitmaps[BITMAP_ID_LAST];	// should be hashmap?
	wxIcon				m_LogFrameIcon, m_GreenRedLED[2];
	wxStaticBitmap			*m_BitmapLED;
	
	vector<wxBitmap>		m_ThrobberBMFrames;
	wxStaticBitmap			*m_BitmapThrobber;
	
	shared_ptr<ClientStack>		m_ClientStack;
	
	SuspendState			m_SuspendedState;
	LX::MessageQueue		m_ClientQueue;
	
	ProjectPrefs			*m_ProjectPrefs;
	
	unique_ptr<IClientState>	m_ClientState;
	
	WatchBag			*m_WatchBag;
	string				m_MemoryWatch;
	
	PlatformInfo			m_DaemonPlatform;
	RequestStartupLua		m_StartupLuaStruct;
	
	wxTimer				m_Timers[TIMER_ID::TIMER_MAX];			// timer LIST
	
	LX::timestamp_t			m_AsyncTimerStartStamp;
	
	int32_t				m_VisibleStackLevel;
	
	bool				m_ApplyingUIPrefsFlags;
	
	wxNotificationMessage		*m_NotificationMessage;
	
	OverlayFrame			*m_OverlayFrame;
	
	LX::ModalTextEdit		*m_ModalTextEdit;
	
	LX::DynSigMap<VarViewSignals>	m_DynVarViewSigs;
	
	DECLARE_EVENT_TABLE()
};

} // namespace DDT_CLIENT

// nada mas
