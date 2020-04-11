// mappings frame

#include "wx/filename.h"
#include "wx/dir.h"

#include "ddt/sharedDefs.h"
#include "ddt/FileSystem.h"

#include "TopFrame.h"
#include "Controller.h"
#include "SourceFileClass.h"
#include "ClientCore.h"

#include "NavigationBar.h"
#include "LocalDirCtrl.h"

#include "MappingsFrame.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

//===== Lua Mappings Dir control ==============================================

	// doesn't support lua sub-maps (pointless anyway?)

// class LuaMapDirCtrl: virtual public DirTreeCtrl
class DDT_CLIENT::LuaMapDirCtrl: public DirTreeCtrl
{
public:
	// ctor
	LuaMapDirCtrl(wxWindow *parent, int id, DirTreeListener *listener, Controller *controller, LocalDirCtrl *local_dir_ctrl)
		: DirTreeCtrl(parent, id, listener, "/")
	{
		assert(parent);
		assert(listener);
		assert(controller);
		assert(local_dir_ctrl);
		
		m_Controller = controller;
		m_LocalDirCtrl = local_dir_ctrl;
	}
	
	// dtor
	virtual ~LuaMapDirCtrl()
	{
		uLog(DTOR, "LuaMapDirCtrl::DTOR()");
	}
	
	// IMPLEMENTATIONS
	virtual bool	IsLocal(void) const
	{
		return false;
	}
	
	virtual bool	OnDirListing(const StringSet &expanded_set)
	{
		// uLog(DIR_CTL, "LuaMapDirCtrl::OnDirListing()");
		
		assert(m_Controller);
		m_DirList.clear();
		m_FileList.clear();
		
		if (expanded_set.count("/") == 0)
		{	uLog(DIR_CTRL, "mappings root is collapsed");
			return true;	// root is collapsed, don't collect dirs
		}

		vector<DirMap>	map_list = m_Controller->GetMapList();
		
		for (int i = 0; i < map_list.size(); i++)
		{
			// map name is SLASH-FREE
			const string	map_name = map_list[i].GetMapName().ToStdString();
			
			// so preprend slash
			// const string	parent_dir = FileName::PATH_SEPARATOR_STRING + map_name;
			const string	parent_dir = string("/") + map_name;
			
			m_DirList.push_back(parent_dir);

			uLog(DIR_CTRL, "adds map name %S", parent_dir);
			
			if (expanded_set.count(parent_dir) == 0)		continue;	// dir is collapsed, don't collect files
			
			const StringList	file_list = m_Controller->GetDirMapFiles(map_name);
			for (const auto fn : file_list)
			{
				const string	full_fn = FileName::MakeFullUnixPath(parent_dir, fn);
				
				m_FileList.push_back(full_fn);
			}
		}
		
		// uLog(DIR_CTRL, " got %zu dirs, %zu files", m_DirList.size(), m_FileList.size());	
		uLog(DIR_CTRL, "  dirs %S", m_DirList.ToFlatString());
		uLog(DIR_CTRL, "  files %S", m_FileList.ToFlatString());
		
		return true;
	}
	
	// OVERLOAD
	virtual bool	OnDoubleClick(const std::string &path)
	{	// - receives Lua-mapped path so don't test for existence
		// - Map names are stored SLASH-FREE internally
		uLog(DIR_CTRL, "LuaMapDirCtrl::OnDoubleClick(%S)", path);
		assert(m_Controller);
		assert(m_LocalDirCtrl);
		
		if (path == "/")	return false;		// let root get expanded
		
		const TREE_ITEM_TYPE	typ = GetPathType(path, true/*fatal*/);
		if (typ == TREE_ITEM_TYPE::DIR_T )
		{	// mapped dir -> show in local view & let collapse/expand
			DirMap	map = m_Controller->GetMapFromMapName(path);
			assert(map.IsOk());
			
			const wxString	native_local_dir = map.GetClientDir();
			assert(!native_local_dir.empty());
			uLog(DIR_CTRL, "  native client path is %S", native_local_dir);
			const string	unix_local_dir = FileName::NativeToUnixPath(native_local_dir.ToStdString());
			assert(!unix_local_dir.empty());
			
			m_LocalDirCtrl->HighlightPath(unix_local_dir, true/*select*/);
		}
		else if (typ == TREE_ITEM_TYPE::FILE_T)
		{
			// mapped file -> open in editor
			const string	short_name = m_Controller->MappedFileToClientShortName(path/*unix format*/);
			assert(!short_name.empty());
			
			SourceFileClass	*sfc = m_Controller->GetSFC(wxString(short_name));
			assert(sfc);
			
			sfc->ShowNotebookPage(true);
		}
		else
		{	uErr("unimplemented double-click on %S", path);
		}
		
		return true;		// handled
	}
	
	// OVERLOAD
	vector<int>	GetContextMenuIDs(const std::string &path) const
	{
		const TREE_ITEM_TYPE	typ = GetPathType(path, true/*fatal*/);
		
		if (typ == TREE_ITEM_TYPE::DIR_T )
		{	// dir
			return { /*CONTEXT_MENU_ID_TREE_NEW_DIR,*/ CONTEXT_MENU_ID_TREE_RENAME_DIR, CONTEXT_MENU_ID_TREE_BOOKMARK, CONTEXT_MENU_ID_TREE_sep, CONTEXT_MENU_ID_TREE_DELETE_DIR };
		}
		else if (typ == TREE_ITEM_TYPE::FILE_T )
		{	// file
			return { CONTEXT_MENU_ID_TREE_RENAME_FILE, CONTEXT_MENU_ID_TREE_sep, CONTEXT_MENU_ID_TREE_REMOVE_FILE };
		}
		else
		{	uErr("illegal TREE_ITEM_TYPE");
			assert(0);
			return { };
		}
	}
	
	virtual bool	OnRenameDir(const std::string &old_path, const std::string &new_path)
	{
		assert(m_Controller);
		
		// uses SLASH-FREE names
		wxFileName	cfn1(wxString(old_path), "");
		wxFileName	cfn2(wxString(new_path), "");
		
		// get last dir name, so will also prevent subdirs
		wxString	map1 = cfn1.GetDirs().Last();
		wxString	map2 = cfn2.GetDirs().Last();
		
		// uMsg("LuaMapDirCtrl::OnRenameDir(%S, %S)", map1, map2);
		
		bool	ok = m_Controller->RenameMapping(map1, map2);
		assert(ok);
		
		return ok;
	}
	
	virtual	bool	OnDeleteDir(const string &mapped_path)
	{
		assert(m_Controller);
		
		wxFileName	cfn(wxString(mapped_path), "");
		assert(cfn.IsOk());
			
		// get last dir name, will also prevent subdirs		-- LAME, FIXME
		const wxString	map_name = cfn.GetDirs().Last();
		
		// returns false if contained files
		bool	ok = m_Controller->DeleteMap(map_name);
		
		return ok;
	}
	
	wxString	ImpName(void) const
	{
		return "MappingsDirCtrl";
	}
	
	int	StatusIndex(void) const
	{
		return 1;
	}
	
private:
	
	Controller	*m_Controller;
	LocalDirCtrl	*m_LocalDirCtrl;
};

//=============================================================================

BEGIN_EVENT_TABLE(MappingsFrame, wxFrame)

	
	EVT_CLOSE(										MappingsFrame::OnCloseEvent)
	// EVT_THREAD(		BROADCAST_ID_PROJECT_REFRESH,					MappingsFrame::OnRefreshBroadcast)
	
	EVT_TOOL(		NAV_ID_STAR,							MappingsFrame::OnNewBookmarkToolEvent)
	EVT_COMMAND_RANGE(	LOCAL_PATH_MRU_START, LOCAL_PATH_MRU_END,	wxEVT_MENU,	MappingsFrame::OnMRUMenuEvent)

	EVT_MOVE(										MappingsFrame::OnMoveEvent)
	EVT_SIZE(										MappingsFrame::OnResizeEvent)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	#define	styl	(wxCAPTION | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)		// (wxFRAME_TOOL_WINDOW would remove from taskbar)

	MappingsFrame::MappingsFrame(TopFrame *tf, int id, Controller *controller)
		: wxFrame(tf, id, "File Mappings", wxDefaultPosition, wxDefaultSize, styl)
{
	assert(tf);
	assert(controller);
	
	m_TopFrame = tf;
	m_Controller = controller;
	
	// status bar
	#ifdef __WIN32__
		#define sb_styl	wxSB_NORMAL
	#else
		#define	sb_styl	wxSB_SUNKEN
	#endif

	m_StatusBar = CreateStatusBar(1, sb_styl, STATUSBAR_ID_MAPPINGS_FRAME);
	assert(m_StatusBar);
	
	int	styles[] = {sb_styl};
	m_StatusBar->SetStatusStyles(WXSIZEOF(styles), styles);
	
	m_NavigationBar = new NavigationBar(this, CONTROL_ID_LOCAL_NAVIGATION_BAR, tf, controller, this/*evt receiver*/);
	m_LocalDirCtrl = new LocalDirCtrl(this, CTRL_ID_CLIENT_DIR_CTRL, this, controller);
	m_LuaMapDirCtrl = new LuaMapDirCtrl(this, CTRL_ID_LUA_DIR_TREE, this, controller, m_LocalDirCtrl);
	
	wxBoxSizer	*h_sizer = new wxBoxSizer(wxHORIZONTAL);

	h_sizer->Add(m_LocalDirCtrl, wxSizerFlags(1).Expand().Border(wxRIGHT, 1));
	h_sizer->Add(m_LuaMapDirCtrl, wxSizerFlags(1).Expand());
	
	wxBoxSizer	*top_v_sizer = new wxBoxSizer(wxVERTICAL);
	
	top_v_sizer->Add(m_NavigationBar, wxSizerFlags(0).Expand());
	top_v_sizer->Add(h_sizer, wxSizerFlags(1).Expand().Border(wxALL, 4));
	
	SetSizer(top_v_sizer);
		
	controller->SetMappingsFrame(this);
	
	m_NavigationBar->RequestUpdateBookmarks();
	
	// do NOT show, gets in the way of UI prefs
	
	ResetListener();
}

//---- DTOR -------------------------------------------------------------------

	MappingsFrame::~MappingsFrame()
{
	uLog(DTOR, "MappingsFrame::DTOR()");
	
	m_TopFrame = nil;
}

bool	MappingsFrame::ShouldPreventAppExit() const
{
	uLog(DTOR, "MappingsFrame::ShouldPreventAppExit()");
	
	return false;
}

//---- On Close event ---------------------------------------------------------

void	MappingsFrame::OnCloseEvent(wxCloseEvent &e)
{
	uLog(DTOR, "MappingsFrame::OnCloseEvent()");
	
	if (e.CanVeto() && m_TopFrame && !m_TopFrame->IsBeingDeleted() && !IsBeingDeleted())
	{	// user op, just hide window and veto
		uLog(DTOR, "  MappingsFrame::OnClose(VETOED)");
		Hide();
		m_TopFrame->DirtyUIPrefs();
		return e.Veto();
	}
	
	Destroy();		// caca
	// m_TopFrame = nil;
	
	// called by TopFrame, let it get closed
	e.Skip();
}

//---- On Move & Size events --------------------------------------------------

void	MappingsFrame::OnMoveEvent(wxMoveEvent &e)
{
	assert(m_TopFrame);
	m_TopFrame->DirtyUIPrefs();
	e.Skip();
}

void	MappingsFrame::OnResizeEvent(wxSizeEvent &e)
{
	assert(m_TopFrame);
	m_TopFrame->DirtyUIPrefs();
	e.Skip();
}

//---- On Refresh Broadcast (synthetic event) ---------------------------------

void	MappingsFrame::OnRefreshBroadcast(wxThreadEvent &e)
{
	uLog(UI, "MappingsFrame::OnRefreshBroadcast()");
	assert(m_NavigationBar);
	assert(m_LocalDirCtrl);
	assert(m_LuaMapDirCtrl);
	
	if (!IsShown())	
	{	// don't show now
		uLog(DIR_CTRL, "MappingsFrame::OnRefreshBroadcast() NOT SHOWN");
		return;
	}
	uLog(DIR_CTRL, "MappingsFrame::OnRefreshBroadcast() is shown");
	
	m_NavigationBar->RequestUpdateBookmarks();
	
	m_LocalDirCtrl->RequestRedraw();
	m_LuaMapDirCtrl->RequestRedraw();
}

//---- On New Bookmark Tool event ---------------------------------------------

void	MappingsFrame::OnNewBookmarkToolEvent(wxCommandEvent &e)
{
	uLog(LOGIC, "MappingsFrame::OnNewBookmarkToolEvent()");
	assert(m_Controller);
	assert(m_LocalDirCtrl);
	assert(m_NavigationBar);
	
	const string	bmk = m_LocalDirCtrl->GetSingleSelectedDir();
	if (bmk.empty())		return;		// not single dir selection
	
	// pop notification tip?
	
	// update MRU
	m_Controller->UpdateLocalPathBookmark(bmk);
	
	// update navigation bar
	m_NavigationBar->RequestUpdateBookmarks();
}

//---- On MRU Menu event ------------------------------------------------------

void	MappingsFrame::OnMRUMenuEvent(wxCommandEvent &e)
{
	uLog(UI, "MappingsFrame::OnMRUMenuEvent()");
	
	assert(m_Controller);
	assert(m_LocalDirCtrl);
	assert(m_NavigationBar);
	
	const int	id = e.GetId() - LOCAL_PATH_MRU_START;
	assert((id >= 0) && (id < LOCAL_PATH_MRU_MAX));
	
	const StringList	bookmark_list = m_Controller->GetLocalPathBookmarkList();
	
	const string	bmk = bookmark_list.at(id);
	
	uLog(UI, "MappingsFrame::OnMRUMenuEvent(%S)", wxString(bmk));
	
	m_LocalDirCtrl->HighlightPath(bmk, true/*select*/);
	
	// update MRU
	m_Controller->UpdateLocalPathBookmark(bmk);
	
	// update navigation bar
	m_NavigationBar->RequestUpdateBookmarks();
}

//---- Drag Start -------------------------------------------------------------

// DirTreeListener IMPLEMENTATIONS
bool	MappingsFrame::NotifyDragOriginator(DirTreeCtrl *originator, const StringList &path_list)
{
	uLog(UI, "MappingsFrame::NotifyDragOriginator()");
	ResetListener();
	
	assert(originator);
	if (!originator->IsLocal())							return false;	// veto drag from lua mappings
	
	const uint32_t	path_mask = originator->GetPathListMask(path_list);
	
	if ((path_mask & PATH_LIST_FILE_ANY) && (path_mask & PATH_LIST_DIR_ANY))	return false;	// veto BOTH files & dirs
	if (path_mask & PATH_LIST_DIR_MULTI)						return false;	// veto multiple dirs
	
	// store
	m_Originator = originator;
	m_PathList = path_list;
	m_PathMask = path_mask;
	
	return true;
}

//---- Drag Loop --------------------------------------------------------------

bool	MappingsFrame::NotifyDragLoop(DirTreeCtrl *recipient, const string &highlight_path, const wxPoint &pt)
{	
	assert(recipient);
	
	uLog(UI, "NotifyDragLoop(%s %S)", recipient->ImpName(), highlight_path);
	
	// SetStatusText(msg, 0);
	
	return true;
}

//---- Drag End ---------------------------------------------------------------

bool	MappingsFrame::NotifyDragRecipient(DirTreeCtrl *recipient, const string &dest_path, const StringList &opt_path_list)
{
	uLog(UI, "MappingsFrame::NotifyDragRecipient(dest path %S)", dest_path);
	assert(recipient);
	assert(m_Controller);
	
	SetStatusText("", 0);
	
	// veto drag to local
	if (recipient->IsLocal())		return false;					// could use for BOOKMARK set ?
	
	const wxString	map_name = m_Controller->GetMapNameFromDir(wxString(dest_path));
	// (may be empty string)
	
	uint32_t	path_mask = m_PathMask;
	
	if (0 == path_mask)
	{	// from Finder, reassign path list from opt_path_list, recompute mask
		m_PathList = opt_path_list;
		
		path_mask = m_LocalDirCtrl->GetLocalPathListMask(opt_path_list);
		
		uLog(DIR_CTRL, "  FINDER drag %S to %S", m_PathList.ToFlatString(), dest_path);
	}
	
	StringList	path_list;
	
	if (PATH_LIST_DIR_SINGLE & path_mask)
	{	// dragged DIR
		const string	local_dir = m_PathList.front();
		uLog(DIR_CTRL, "  dragged dir %S", local_dir);
	
		assert(FileName::DirExists(local_dir));
		
		// get recursive files
		wxArrayString	all_lua_files;
		
		const size_t	n = wxDir::GetAllFiles(wxString(local_dir), &all_lua_files, "*.lua", wxDIR_DIRS | wxDIR_FILES);
		for (int i = 0; i < n; i++)
		{
			const string	path = all_lua_files[i].ToStdString();
			// assert(FileName::IsLegalFilename(path));
			
			path_list.push_back(path);
		}
	}
	else if (PATH_LIST_FILE_ANY & path_mask)
	{	// dragged FILES
		uLog(DIR_CTRL, "  dragged FILES %S", m_PathList.ToFlatString());
		
		path_list = m_PathList;
	}
	else
	{	uErr("path_mask no handled");
		
		return false;
	}
	
	int	n_added, n_dupe, n_not_lua;
	
	n_added = n_dupe = n_not_lua = 0;

	for (const auto fpath : path_list)
	{	
		// (convert std::string)
		wxString	fullpath(fpath);
		assert(!fullpath.empty());
		
		// auto-add Lua files to project
		wxFileName	cfn(fullpath);
		assert(cfn.IsOk() && cfn.FileExists());
		
		if (cfn.GetExt().Lower() != "lua")
		{	n_not_lua++;
			continue;						// ignore non-lua files
		}
		
		if (!FileName::IsLegalFilename(cfn.GetFullName().ToStdString()))
		{
			uErr("illegal filename %S", cfn.GetFullName());
			continue;
		}
		
		if (m_Controller->HasDoppleganger(fullpath))
		{	n_dupe++;
			continue;						// ignore existing lua files
		}
		
		const wxString	local_dir = cfn.GetPath();
		assert(wxFileName::DirExists(local_dir));
		
		if (!m_Controller->HasClientMap(local_dir))
		{	// create Map with given name so is sure to find it on SFC instantiation
			const DirMap	dm = m_Controller->NewMap(local_dir, map_name);
			if (!dm.IsOk())
			{	uErr("Map instantiation FAILED for local_dir %S, map_name %S", local_dir, map_name);
				return false;
			}
		}
		
		// uLog(DIR_CTL, "  adding %S to map %S", fullpath, map_name);
		
		// SFC will automatically be assigned to DirMap with associated client dir
		SourceFileClass	*sfc = m_Controller->NewSFC(fullpath);
		if (!sfc)
		{	uErr("SFC instantiation FAILED for %S", fullpath);
			return false;
		}
		
		n_added++;
	}
	
	const wxString	msg = wxString::Format("Lua files: added %d, dupe %d, non-lua %d", n_added, n_dupe, n_not_lua);
	
	uMsg(msg);
	
	SetStatusText(msg, 0);
	
	ResetListener();
	
	return true;
}

// nada mas
