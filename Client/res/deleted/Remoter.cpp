// split panel local/remote file navigator wxFrame

#include <cstdint>

#include "TopFrame.h"
#include "UIConst.h"

#include "NavigationBar.h"
#include "DirTreeCtrl.h"

#include "ddt/sharedDefs.h"
#include "ddt/FileSystem.h"
#include "ddt/MemoryInputStream.h"
#include "ddt/MemoryOutputStream.h"

#include "ClientCore.h"

#include "ClientTCP.h"
#include "Controller.h"

#include "LocalDirCtrl.h"

#include "Remoter.h"

using namespace std;
using namespace LX;
using namespace DDT;

//===== Remote Dir Control ====================================================

class RemoteDirCtrl: public DirTreeCtrl
{
public:
	// ctor
	RemoteDirCtrl(wxWindow *parent, int id, DirTreeListener *dtl, ClientTCP_Abstract *client_tcp)
		: DirTreeCtrl(parent, id, dtl, "/"), m_ClientTCP(client_tcp)
	{
		assert(parent);
		assert(dtl);
		assert(client_tcp);
	}
	// dtor
	virtual ~RemoteDirCtrl()
	{
		uLog(DTOR, "RemoteDirCtrl::DTOR()");
	}
	
	// IMPLEMENTATIONS
	virtual bool	IsLocal(void) const
	{
		return false;
	}
	
	virtual bool	OnDirListing(const StringSet &expanded_set)
	{
		uLog(DIR_CTRL, "RemoteDirCtrl::OnDirListing()");
		
		assert(m_ClientTCP);
		
		m_DirList.clear();
		m_FileList.clear();
		
		if (!m_ClientTCP->IsConnected())	return false;
		
		// if collapsed root, don't return anything
		if (expanded_set.count("/") == 0)	return true;
	
		// send request (doesn't pass path)
		{	MemoryOutputStream 	mos;
			
			mos << CLIENT_MSG::REMOTER_PULL_DIR_LISTING << expanded_set.ToStringList();
			
			bool	ok = m_ClientTCP->WriteMessage(mos);
			assert(ok);
		}
		
		// get reply
		{	vector<uint8_t>		msg_buff;
			
			bool	ok = m_ClientTCP->ReadMessage(msg_buff/*&*/);
			assert(ok);
			
			MemoryInputStream	mis(msg_buff);
			
			DAEMON_MSG	msg_t;
			
			mis >> msg_t;
			assert(DAEMON_MSG::REMOTER_REPLY_DIR_LISTING == msg_t);
			
			LX::Dir	dir(mis);
			
			m_DirList = dir.GetDirs();
			assert(m_DirList.size() > 0);
			
			m_FileList = dir.GetFiles();
		}
		
		uLog(DIR_CTRL, " got %lu dirs, %lu files", m_DirList.size(), m_FileList.size());	
		uLog(DIR_CTRL, "  dirs %S", m_DirList.ToFlatString());
		uLog(DIR_CTRL, "  files %S", m_FileList.ToFlatString());
		
		return true;
	}
	
	vector<int>	GetContextMenuIDs(const std::string &path) const
	{
		const TREE_ITEM_TYPE	typ = GetPathType(path, true/*fatal*/);
		
		if (typ == TREE_ITEM_TYPE::DIR_T)
		{	// dir (add recursive dir deleted before enabling option)
			return { CONTEXT_MENU_ID_TREE_sep, /*CONTEXT_MENU_ID_TREE_NEW_DIR, CONTEXT_MENU_ID_TREE_sep, CONTEXT_MENU_ID_TREE_DELETE_DIR*/ };
		}
		// else if (typ == TREE_ITEM_TYPE::FILE)		return { CONTEXT_MENU_ID_TREE_sep, CONTEXT_MENU_ID_TREE_DELETE_FILES };
		// else if (typ == TREE_ITEM_TYPE::BOOKMARK)	return { CONTEXT_MENU_ID_TREE_BOOKMARK, CONTEXT_MENU_ID_TREE_sep };
		else
		{	uErr("illegal TREE_ITEM_TYPE");
			return {};
		}
	}
	
	virtual bool	OnDeleteFiles(const StringList &path_list)
	{
		if (!m_ClientTCP->IsConnected())	return false;		// check upstream -- FIXME
		
		MemoryOutputStream 	mos;
			
		mos << CLIENT_MSG::REMOTER_DELETE_FILES << path_list;
			
		bool	ok = m_ClientTCP->WriteMessage(mos);
		assert(ok);
		
		return true;
	}
	
	virtual bool	OnDeleteDir(const std::string &path)
	{
		if (!m_ClientTCP->IsConnected())	return false;		// check upstream -- FIXME
		
		MemoryOutputStream 	mos;
			
		mos << CLIENT_MSG::REMOTER_DELETE_DIR << path;
		
		bool	ok = m_ClientTCP->WriteMessage(mos);
		assert(ok);
		
		return true;
	}
	
	wxString	ImpName(void) const
	{
		return "RemoteDirCtrl";
	}
	
	int	StatusIndex(void) const
	{
		return 1;
	}
	
private:
	
	ClientTCP_Abstract	*m_ClientTCP;
};

//===== REMOTER FRAME =========================================================

BEGIN_EVENT_TABLE(RemoterFrame, wxFrame)

	EVT_CLOSE(							RemoterFrame::OnCloseEvent)
	// EVT_THREAD(		NOTIFY_ID_START_REMOTER,		RemoterFrame::OnStartBrowsing)
	// EVT_THREAD(		BROADCAST_ID_PROJECT_REFRESH,		RemoterFrame::OnRefreshBroadcast)
	EVT_CHOICE(		CONTROL_ID_LOCAL_NAVIGATION_BAR,	RemoterFrame::OnLocalFilterChoice)
	EVT_CHOICE(		CONTROL_ID_REMOTE_FILTER_LIST,		RemoterFrame::OnRemoteFilterChoice)
	
	EVT_MOVE(							RemoterFrame::OnMoveEvent)
	EVT_SIZE(							RemoterFrame::OnResizeEvent)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	#define	styl	(wxCAPTION | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)
		// warning: wxFRAME_TOOL_WINDOW would remove from taskbar

	RemoterFrame::RemoterFrame(TopFrame *tf, int id, Controller *controller, ClientTCP_Abstract *client_tcp)
		: wxFrame(tf, id, "Remoter", wxDefaultPosition, wxDefaultSize, styl)
{
	assert(tf);
	assert(controller);
	assert(client_tcp);
	
	m_TopFrame = tf;
	m_Controller = controller;
	m_ClientTCP = client_tcp;

	m_LeftNavigationBar = m_RightNavigationBar = nil;
	m_LeftFilterChoice = m_RightFilterChoice = nil;
	m_LeftPanel = m_RightPanel = nil;
	m_TopHSizer = nil;
	
	#ifdef __WIN32__
		#define sb_styl	wxSB_NORMAL
	#else
		#define	sb_styl	wxSB_SUNKEN
	#endif

	wxStatusBar	*status_bar = CreateStatusBar(2, sb_styl, STATUSBAR_ID_REMOTER_FRAME);
	assert(status_bar);
	
	int	styles[] = {sb_styl, sb_styl};
	status_bar->SetStatusStyles(WXSIZEOF(styles), styles);
	
// left panel
	m_LeftPanel = new wxPanel(this);
	m_LeftPanel->SetName("left panel");
	m_LocalDirCtrl = new LocalDirCtrl(m_LeftPanel, CONTROL_ID_LOCAL_DIR, this, controller);
	m_LocalDirCtrl->SetName("LocalDirCtrl");
	m_LeftNavigationBar = new NavigationBar(m_LeftPanel, CONTROL_ID_LOCAL_NAVIGATION_BAR, tf, controller, m_LocalDirCtrl);
	
	wxString	filters[] = {"Lua files (*.lua)|*.lua", "All files (*.*)|*.*"};
	
	m_LeftFilterChoice = new wxChoice(m_LeftPanel, CONTROL_ID_LOCAL_FILTER_LIST, wxDefaultPosition, wxDefaultSize, 2, filters);
	
	wxBoxSizer	*left_v_sizer = new wxBoxSizer(wxVERTICAL);
	
	left_v_sizer->Add(m_LeftNavigationBar, wxSizerFlags(0).Expand());
	left_v_sizer->Add(m_LocalDirCtrl, wxSizerFlags(1).Expand().Border(wxALL, 1));
	left_v_sizer->Add(m_LeftFilterChoice, wxSizerFlags(0).Expand());
	
	m_LeftPanel->SetSizer(left_v_sizer);
	
// right panel
	m_RightPanel = new wxPanel(this);
	m_RightPanel->SetName("right panel");
	m_RemoteDirCtrl = new RemoteDirCtrl(m_RightPanel, CONTROL_ID_REMOTE_TREE, this, client_tcp);
	m_RemoteDirCtrl->SetName("RemoteDirCtrl");
	m_RightNavigationBar = new NavigationBar(m_RightPanel, CONTROL_ID_REMOTE_NAVIGATION_BAR, tf, controller, m_RemoteDirCtrl);
	m_RightFilterChoice = new wxChoice(m_RightPanel, CONTROL_ID_REMOTE_FILTER_LIST, wxDefaultPosition, wxDefaultSize, 2, filters);
	
	wxBoxSizer	*right_v_sizer = new wxBoxSizer(wxVERTICAL);
	
	right_v_sizer->Add(m_RightNavigationBar, wxSizerFlags(0).Expand());
	right_v_sizer->Add(m_RemoteDirCtrl, wxSizerFlags(1).Expand().Border(wxALL, 1));
	right_v_sizer->Add(m_RightFilterChoice, wxSizerFlags(0).Expand());
	
	m_RightPanel->SetSizer(right_v_sizer);
	
	m_TopHSizer = new wxBoxSizer(wxHORIZONTAL);
	
	// glue panes together
	m_TopHSizer->Add(m_LeftPanel, wxSizerFlags(1).Expand().Border(wxALL, 2));
	m_TopHSizer->Add(m_RightPanel, wxSizerFlags(1).Expand().Border(wxALL, 2));
	
	SetSizer(m_TopHSizer);
	
	// do NOT show, gets in the way of UI prefs
}

//---- DTOR -------------------------------------------------------------------
	
	RemoterFrame::~RemoterFrame()
{
	uLog(DTOR, "RemoterFrame::DTOR()");
	
	m_TopFrame = nil;
}

bool	RemoterFrame::ShouldPreventAppExit() const
{
	uLog(DTOR, "RemoterFrame::ShouldPreventAppExit()");
	return false;
}

//---- On Close event ---------------------------------------------------------

void	RemoterFrame::OnCloseEvent(wxCloseEvent &e)
{
	uLog(DTOR, "RemoterFrame::OnCloseEvent()");
	
	if (e.CanVeto() && m_TopFrame && !m_TopFrame->IsBeingDeleted() && !IsBeingDeleted())
	{	// user op, just hide window and veto
		uLog(DTOR, "RemoterFrame::OnClose(VETOED)");
		Hide();
		m_TopFrame->DirtyUIPrefs();
		return e.Veto();
	}
	
	m_TopFrame = nil;
	
	// called by TopFrame, let it get closed
	e.Skip();
}

//---- Drag Origin IMPLEMENTATION ---------------------------------------------

bool	RemoterFrame::NotifyDragOriginator(DirTreeCtrl *originator, const StringList &path_list)
{
	ResetListener();
	assert(originator);
	if (!m_ClientTCP->IsConnected())	return false;		// can't drag until is connected
	
	m_Originator = originator;
	m_PathList = path_list;
	m_PathMask = originator->GetPathListMask(path_list);
	
	// unselect other pane
	if (originator == m_LocalDirCtrl)
		m_RemoteDirCtrl->UnselectAll();
	else	m_LocalDirCtrl->UnselectAll();
	
	const wxString	origin = originator->ImpName();
	
	uLog(DIR_CTRL, "RemoterFrame::NotifyDragOriginator(%s, %d files)", origin, (int)path_list.size());
	
	return true;
}

//---- Drag Loop IMPLEMENTATION -----------------------------------------------

bool	RemoterFrame::NotifyDragLoop(DirTreeCtrl *recipient, const std::string &highlight_path, const wxPoint &pt)
{	
	assert(recipient);
	
	const wxString	msg = wxString::Format("NotifyDragLoop(%s \"%s\")", wxString(recipient->ImpName()), wxString(highlight_path));
	uLog(UI, msg);
	
	SetStatusText(msg, recipient->StatusIndex());
	
	return true;
}

//---- Drag Recipient IMPLEMENTATION ------------------------------------------

bool	RemoterFrame::NotifyDragRecipient(DirTreeCtrl *recipient, const string &dest_path, const StringList &opt_path_list)
{
	uLog(DIR_CTRL, "RemoterFrame::NotifyDragRecipient()");
	assert(m_Originator);
	assert(recipient);
	assert(m_ClientTCP);
	assert(!dest_path.empty());
	
	SetStatusText("", recipient->StatusIndex());
	
	const wxString	origin = m_Originator->ImpName();
	const wxString	dest = recipient->ImpName();
	
	if (m_Originator->IsLocal())
	{	// PUSH
		uLog(DIR_CTRL, " PUSH from %s -> %s:%s)", origin, dest, dest_path);
		
		for (int i = 0; i < m_PathList.size(); i++)
		{
			const string	org_fullpath = m_PathList[i];
			const string	filename = FileName::GetFileFileName(org_fullpath);
			const string	dest_fullpath = FileName::MakeFullPath(dest_path, filename);
			
			uLog(DIR_CTRL, "  [%d] %S -> %S", i, org_fullpath, dest_fullpath);
			
			// send message
			MemoryOutputStream	mos;
			
			mos << CLIENT_MSG::REMOTER_PUSH_FILE;
			mos << dest_fullpath;
			
			bool	ok = FileName::SerializeFile(mos, org_fullpath);
			assert(ok);
			
			uLog(DIR_CTRL, "  sending %lu bytes", mos.GetLength());
			
			ok = m_ClientTCP->WriteMessage(mos);
			return ok;
		}
	}
	else
	{	// PULL
		uLog(DIR_CTRL, " PULL from %s -> %s:%s NOT IMPLEMENTED)", origin, dest, dest_path);
		return false;	
	}
	
	return true;		// ok
}

//---- On Move & Size events --------------------------------------------------

void	RemoterFrame::OnMoveEvent(wxMoveEvent &e)
{
	assert(m_TopFrame);
	m_TopFrame->DirtyUIPrefs();
	e.Skip();
}

void	RemoterFrame::OnResizeEvent(wxSizeEvent &e)
{
	assert(m_TopFrame);
	m_TopFrame->DirtyUIPrefs();
	e.Skip();
}

//---- On Remote Filter Choice event ------------------------------------------

void	RemoterFrame::OnLocalFilterChoice(wxCommandEvent &e)
{
	uLog(DIR_CTRL, "RemoterFrame::OnLocalFilterChoice(%d)", e.GetInt());		// remove
}

//---- On Remote Filter Choice event ------------------------------------------

void	RemoterFrame::OnRemoteFilterChoice(wxCommandEvent &e)
{
	uLog(DIR_CTRL, "RemoterFrame::OnRemoteFilterChoice(%d)", e.GetInt());		// remove
}

//---- On Refresf Broadcast from TopFrame -------------------------------------

void	RemoterFrame::OnRefreshBroadcast(wxThreadEvent &e)
{
	// uLog(DIR_CTRL, "RemoterFrame::OnRefreshBroadcast()");
	
	assert(m_Controller);
	assert(m_LocalDirCtrl);
	assert(m_RemoteDirCtrl);
	
	if (!IsShown())			return;		// don't show now
	
	// m_Controller->GetLocalDirBookmarks();
	
	m_LocalDirCtrl->RequestRedraw();
	m_RemoteDirCtrl->RequestRedraw();
}
	
//---- On Start Browsing event ------------------------------------------------

void	RemoterFrame::OnStartBrowsing(wxThreadEvent &e)
{
	uLog(DIR_CTRL, "RemoterFrame::OnStartBrowsing()");
	
	Show();
	Raise();
	
	m_RemoteDirCtrl->RequestRedraw();
}

// nada mas
