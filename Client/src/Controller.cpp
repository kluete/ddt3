// Lua DDT Controller

#include <vector>
#include <cassert>
#include <algorithm>

#include "wx/filename.h"
#include "wx/dnd.h"

#include "TopFrame.h"

#include "SourceFileClass.h"
#include "TopNotebook.h"
#include "BottomNotebook.h"

#include "ProjectPrefs.h"

#include "StyledSourceCtrl.h"

#include "ClientState.h"

#include "logImp.h"
#include "Controller.h"

#include "lx/ulog.h"

#include "pragma_debugger.h"		// ddt prama debugger

#include "ddt/FileSystem.h"

#if (wxUSE_DRAG_AND_DROP == 0)
	#error "wxUSE_DRAG_AND_DROP is not enabled"
#endif

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

static const
unordered_set<INIT_LEVEL, EnumClassHash>	s_ClientActiveSet
{
	INIT_LEVEL::DISCONNECTED,
	INIT_LEVEL::CONNECTED,
	INIT_LEVEL::LUA_PAUSED,
	INIT_LEVEL::LUA_FATAL_ERROR
};

static const
unordered_set<INIT_LEVEL, EnumClassHash>	s_InitLevelResumableSet
{
	INIT_LEVEL::LUA_STARTED,
	// INIT_LEVEL::LUA_RUNNING,
	INIT_LEVEL::LUA_PAUSED,
	// INIT_LEVEL::WAIT_REPLY,
	// INIT_LEVEL::LUA_FATAL_ERROR,
};

static const
unordered_map<INIT_LEVEL, std::string, EnumClassHash>	s_InitLevelNames
{
	{INIT_LEVEL::DISCONNECTED,	"disconnected"},
	{INIT_LEVEL::CONNECTED,		"connected"},
	{INIT_LEVEL::LUA_STARTED,	"lua_started"},
	{INIT_LEVEL::LUA_RUNNING,	"lua_running"},
	{INIT_LEVEL::LUA_PAUSED,	"lua_paused"},
	{INIT_LEVEL::WAIT_REPLY,	"wait_reply"},
	{INIT_LEVEL::LUA_FATAL_ERROR,	"lua_fatal_error"},
};

// controller drop target
	ControllerDropTarget::ControllerDropTarget(Controller &controller)
		: m_Controller(controller)
{
}

bool	ControllerDropTarget::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &filenames)
{
	StringList	sl;

	for (int i = 0; i < filenames.GetCount(); i++)		sl.push_back(filenames[i].ToStdString());
	
	const bool	ok = m_Controller.OnDroppedFiles(sl);
	return ok;
}

//---- CTOR -------------------------------------------------------------------

	Controller::Controller(TopFrame &tf, IClientState &client_state, shared_ptr<IFixedFont> ff, wxImageList &img_list, Renderer &cairo_renderer, ModalTextEdit *modal_tedit)
		: m_TopFrame(tf),
		m_ClientState(client_state),
		m_FixedFont(ff),
		m_ImageList(img_list),
		m_CairoRenderer(cairo_renderer),
		m_ModalTextEdit(modal_tedit),
		m_MyLookNFeel(CreateLookNFeel())
{
	// delayed set
	m_TopNotePanel = nil;
	m_BottomNotePanel = nil;
	m_ProjectPrefs = nil;
	
	m_InitLevel = INIT_LEVEL::DISCONNECTED;
	
	ResetProject();
}

//---- DTOR -------------------------------------------------------------------

	Controller::~Controller()
{
	uLog(DTOR, "Controller::DTOR");
}

//---- Destroy ----------------------------------------------------------------

void	Controller::DestroyController(void)
{
	uLog(DTOR, "Controller::DestroyController");
	
	delete this;
}

IClientState&	Controller::GetClientState(void)	{return m_ClientState;}
wxImageList&	Controller::GetImageList(void)		{return m_ImageList;}
Renderer&	Controller::GetCairoRenderer(void)	{return m_CairoRenderer;}
ModalTextEdit*	Controller::GetModalTextEdit(void)	{return m_ModalTextEdit;}

//---- Set Top Notepanel ------------------------------------------------------

void	Controller::SetTopNotePanel(ITopNotePanel *top_notebook)
{
	assert(top_notebook);
	
	m_TopNotePanel = top_notebook;
	
	MixConnect(this, &Controller::OnEditorBreakpointClick, top_notebook->GetSignals().OnEditorBreakpointClick);
}

//---- Set Bottom Notepanel ---------------------------------------------------

void	Controller::SetBottomNotePanel(IBottomNotePanel *bottom_notepanel)
{
	assert(bottom_notepanel);
	
	m_BottomNotePanel = bottom_notepanel;
}

//---- Set Project Prefs ------------------------------------------------------

void	Controller::SetProjectPrefs(ProjectPrefs *prefs)
{
	assert(prefs);
	
	m_ProjectPrefs = prefs;
}

//---- Dirty UI Prefs ---------------------------------------------------------

void	Controller::DirtyUIPrefs(void)
{
	m_TopFrame.DirtyUIPrefs();
}

//---- Set Dirty Project ------------------------------------------------------

void	Controller::SetDirtyProject(void)
{
	if (!m_ProjectPrefs)	return;		// no yet set on 1st clear?
	
	assert(m_ProjectPrefs);
	
	m_ProjectPrefs->SetDirty();
}

int	Controller::GetDaemonTick(void) const
{
	return m_DaemonTick;
}

void	Controller::StepDaemonTick(void)
{
	m_DaemonTick++;
}

//---- Get Init Level ---------------------------------------------------------

INIT_LEVEL	Controller::GetInitLevel(void) const
{	
	return m_InitLevel;
}

//---- Get Init Level String --------------------------------------------------

string	Controller::GetInitLevelString(void) const
{
	assert(s_InitLevelNames.count(m_InitLevel));
	
	const string	init_s = s_InitLevelNames.at(m_InitLevel);
	
	return init_s;
}

//---- Set Init Level ---------------------------------------------------------

void	Controller::SetInitLevel(const INIT_LEVEL &lvl)
{	
	if (lvl == m_InitLevel)		return;			// no change
	
	// changed level
	const string	level_bf = s_InitLevelNames.at(m_InitLevel);
	const string	level_af = s_InitLevelNames.at(lvl);
		
	m_InitLevel = lvl;
		
	uLog(INIT_LVL, "INIT_LEVEL changed from %S to %S", level_bf, level_af);
	
	// lock/unlock based on level
	const bool	was_started_f = (m_InitLevel >= INIT_LEVEL::LUA_STARTED);
	const bool	is_started_f = (lvl >= INIT_LEVEL::LUA_STARTED);
	if (was_started_f != is_started_f)	return;
	
	SetAllSFCEditLockFlag(is_started_f);
}

//---- Is Lua Listening ? -----------------------------------------------------

bool	Controller::IsLuaListening(void) const
{
	if (!IsClientActive())		return false;
	return IsLuaStarted();
}

//---- Is Client Active ? -----------------------------------------------------

bool	Controller::IsClientActive(void) const
{	
	return (s_ClientActiveSet.count(GetInitLevel()) > 0);
}

//---- Is Lua VM Resumable ? --------------------------------------------------

bool	Controller::IsLuaVMResumable(void) const
{	
	return (s_InitLevelResumableSet.count(GetInitLevel()) > 0);
}

//---- Has Lua VM Started ? ---------------------------------------------------

bool	Controller::IsLuaStarted(void) const
{
	const bool	f = (GetInitLevel() >= INIT_LEVEL::LUA_STARTED);
	
	return f;
}

//---- Set All SFCs' Edit Lock Flags ------------------------------------------

void	Controller::SetAllSFCEditLockFlag(const bool f)
{
	vector<ISourceFileClass*>	sfc_list = GetSFCInstances();
	
	for (auto sfc : sfc_list)	sfc->SetLockFlag(f);
	
	// m_TopNotePanel->
	// remove highlight on lock and unlock?
	// sfc->RemoveAllHighlights();
}

//---- Has Any Dirty SFC Editors ? --------------------------------------------

bool	Controller::HasDirtySFCEdit(void) const
{
	vector<ISourceFileClass*>	sfc_list = GetSFCInstances();
	
	bool	dirty_sfc_f = false;
	
	for (auto sfc : sfc_list)
	{
		dirty_sfc_f |= sfc->IsDirty();
	}
	
	return dirty_sfc_f;
}

//---- On Read-Only Edit Attempt ----------------------------------------------

bool	Controller::OnReadOnlyEditAttempt(const string &edited_src)
{
	assert(IsLuaStarted());
	
	const int	res = ::wxMessageBox("Stop Debug Session?", "Edit Lua Source", wxCENTER | wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION, &m_TopFrame);
	if (res != wxYES)		return false;			// canceled
	
	wxThreadEvent	e(wxEVT_THREAD, BROADCAST_ID_STOP_SESSION);
	const wxString	payload_s = wxString(edited_src);
	e.SetString(payload_s);
	
	// send to top frame
	wxQueueEvent(&m_TopFrame, e.Clone());				// BULLSHIT
	
	return true;
}

//---- Show Source And Line (intra) -------------------------------------------

void	Controller::ShowSourceAtLine(const string &shortname, const int ln, const bool focus_f)
{
	// assert(ln != -1);
	
	m_Signals.OnShowLocation(CTX_WX, shortname, ln);
}

//---- Get (sorted) Breakpoints List ------------------------------------------

StringList	Controller::GetSortedBreakpointList(const int ln_offset) const
{
	struct bp
	{
		bp(const string &name, const int ln)
			: m_file(name), m_line(ln)
		{}
		
		string	m_file;
		int	m_line;
	};
	
	vector<bp>	bp_list;
	
	for (const string &bp_s : m_BreakpointSet)
	{
		const Bookmark	bmk = Bookmark::Decode(bp_s);
		
		bp_list.emplace_back(bmk.m_Name, bmk.m_Line + ln_offset);
	}
	
	// sort names & lines
	sort(bp_list.begin(), bp_list.end(), [](const bp &p1, const bp &p2){return (p1.m_file == p2.m_file) ? (p1.m_line < p2.m_line) : (p1.m_file < p2.m_file);});
	
	vector<string>	res;
	
	for (const bp &elm : bp_list)
		res.push_back(elm.m_file + ":" + to_string(elm.m_line));
		
	return res;
}

//---- On Editor Breakpoint Click ---------------------------------------------

void	Controller::OnEditorBreakpointClick(ISourceFileClass *sfc, int ln)
{
	uLog(BREAKPOINT_EDIT, "Controller::OnEditorBreakpointClick(ln %d)", ln);
	
	assert(sfc);
	
	ToggleBreakpoint(*sfc, ln);
}

//---- Toggle Breakpoint ------------------------------------------------------

void	Controller::ToggleBreakpoint(ISourceFileClass &sfc, const int ln)
{
	const bool	toggled_f  = !HasBreakpoint_LL(sfc, ln);
	
	SetBreakpoint_LL(sfc, ln, toggled_f);
}

//---- Set Breakpoint ---------------------------------------------------------

void	Controller::SetBreakpoint_LL(ISourceFileClass &sfc, const int ln, const bool f)
{
	const string	key = xsprintf("%s:%d", sfc.GetShortName(), ln);
	
	if (f)
	{	// set
		auto	res = m_BreakpointSet.insert(key);
		assert(res.second);
	}
	else
	{	// remove
		int	num_erased = m_BreakpointSet.erase(key);
		assert(num_erased == 1);
	}
	
	// uLog(UI, "Controller::SetBreakPoint(%s:%d = %s)", short_fn, ln, f ? "true" : "false");
	
	// dirty prefs event
	SetDirtyProject();
	
	// broadcast change
	OnBreakpointUpdated(sfc);
}

//---- On Breakpoint Updated --------------------------------------------------

void	Controller::OnBreakpointUpdated(ISourceFileClass &sfc)
{
	m_Signals.OnRefreshSfcBreakpoints(CTX_WX, &sfc, GetSFCBreakpoints(sfc));
}

//---- On Dropped Files -------------------------------------------------------

bool	Controller::OnDroppedFiles(const StringList &sl)
{
	size_t	existing_files = 0, new_files = 0;
	
	for (auto fullpath : sl)
	{
		wxFileName	cfn(fullpath);
		assert(cfn.IsOk());
		
		if (cfn.IsDir())			continue;	// ignore directories - should add files recursively? FIXME
		if (cfn.GetExt().Lower() != "lua")	continue;	// ignore non-lua files
		
		assert(cfn.FileExists());
		
		// make sure escaped MSVC path is fake
		assert(fullpath == cfn.GetFullPath().ToStdString());

		uLog(LOGIC, "drag & dropped file %S", fullpath);
		
		if (HasDoppleganger(fullpath))
		{	// skip existing files
			existing_files++;
			continue;
		}
		
		ISourceFileClass	*sfc = NewSFC(fullpath);
		if (!sfc)
		{	uErr("instantiation FAILED");
			continue;
		}
		
		new_files++;
	}
	
	if (existing_files > 0)		uLog(WARNING, "ignored %zu existing files", existing_files);
	
	const string	msg = xsprintf("added %zu files", new_files);
	uLog(LOGIC, msg);
	
	m_TopFrame.PopNotificationBubble(msg, "Drag & Drop", wxICON_INFORMATION);			// should be SIGNAL ?
	
	m_Signals.OnRefreshProject(CTX_WX);
	
	SetDirtyProject();
	
	return true;
}

//---- Sanitize Shortname / Key -----------------------------------------------

string	Controller::SanitizeShortName(const string &fn) const
{
	assert(!fn.empty());
	
	string	shortpath;
	
	if (fn[0] == '@')	shortpath = fn.substr(1);		// what's the @-prefix on the CLIENT ???
	else			shortpath = fn;
	
	wxFileName	cfn(wxString{shortpath});
	if (cfn.IsRelative())	cfn.Normalize();
	
	shortpath = cfn.GetFullName().Lower().ToStdString();
	assert(!shortpath.empty());
	
	return shortpath;
}

//---- Has Doppleganger ? -----------------------------------------------------

bool	Controller::HasDoppleganger(const string &org_path) const
{
	const string	shortpath = SanitizeShortName(org_path);
	
	if (m_SourceFileMap.count(shortpath))	return true;	// yep
	
	return false;
}

//---- Instantiate SourceFileClass --------------------------------------------

ISourceFileClass*	Controller::NewSFC(const string &fullpath)
{
	assert(m_TopNotePanel);
	
	assert(!fullpath.empty());
	
	{
		wxFileName	cfn(wxString{fullpath});
		assert(cfn.IsOk() && cfn.FileExists());
	}
	
	// IDENTITY from caseless shortpath
	const string	shortpath = SanitizeShortName(fullpath);
	
	// make sure doesn't already have lowest common denominator
	assert(m_SourceFileMap.count(shortpath) == 0);
	
	// not found -> create
	ISourceFileClass	*sfc = ISourceFileClass::Create(*m_TopNotePanel, *this, fullpath);
	assert(sfc);
	
	// and store
	m_SourceFileMap.emplace(shortpath, sfc);
	
	// save short -> long path map
	m_ShortToLongMap.emplace(shortpath, fullpath);
	
	// keep sorted order
	m_SourceFileOrder.push_back(shortpath);
	
	SetDirtyProject();
	
	// (UI is "PULL", don't update here)
	
	return sfc;
}

//---- Get (existing) SourceFileClass from FullPath ---------------------------

ISourceFileClass*	Controller::GetSFC(const string &org_path) const
{
	const string	sanitized = SanitizeShortName(org_path);
	
	const auto it = m_SourceFileMap.find(sanitized);
	if (it != m_SourceFileMap.end())	return it->second;	// return existing
		
	return nil;
}

//---- Delete SFC -------------------------------------------------------------

bool	Controller::DeleteSFC(ISourceFileClass *sfc)
{
	assert(sfc);
	const string	short_name = sfc->GetShortName();
	uLog(DTOR, "Controller::DeleteSFC(%S)", short_name);
	
	// delete any related breakpoints
	const int	n = DeleteSFCBreakpoints(*sfc);
	uLog(LOGIC, "Controller::DeleteSFC() deleted %d breakpoints related to %S", n, short_name);
	
	// remove mappings
	const auto	it = m_ShortToLongMap.erase(short_name);
	assert(it);
	
	// delete entry in sort order list
	m_SourceFileOrder.erase(std::remove(m_SourceFileOrder.begin(), m_SourceFileOrder.end(), short_name), m_SourceFileOrder.end());
	
	const int	num_erased = m_SourceFileMap.erase(short_name);
	assert(num_erased == 1);
	
	delete sfc;
	
	SetDirtyProject();
	
	return true;	// ok
}

//---- Reset Project ----------------------------------------------------------

void	Controller::ResetProject(void)
{
	uLog(LOGIC, "Controller::ResetProject()");
	DestroyAllSFCs();
	
	m_BreakpointSet.clear();
	m_DaemonTick = 0;
}

//---- Destroy All SFCs --------------------------------------------------------

void	Controller::DestroyAllSFCs(void)
{
	uLog(DTOR, "Controller::DestroyAllSFCs()");
	
	// reloop since delete affects iterator
	while (m_SourceFileMap.size() > 0)
	{
		auto	it = m_SourceFileMap.begin();
		ISourceFileClass	*sfc = it->second;
		assert(sfc);
		
		sfc->DestroySFC();
	}
	
	m_SourceFileOrder.clear();
	m_SourceFileMap.clear();
	m_ShortToLongMap.clear();
	
	SetDirtyProject();
}

//---- Get List of SFC file paths ---------------------------------------------

StringList	Controller::GetSFCPaths(const bool fullpaths_f) const
{
	StringList	path_list;
	
	// get in sorted order
	for (const string &shortname : m_SourceFileOrder)
	{
		assert(m_SourceFileMap.count(shortname));
		
		const ISourceFileClass	*sfc = m_SourceFileMap.at(shortname);
		assert(sfc);
		
		path_list.push_back(fullpaths_f ? sfc->GetFullPath() : sfc->GetShortName());
	}
	
	return path_list;
}

//---- Get Relative SFC Paths -------------------------------------------------

StringList	Controller::GetRelativeSFCPaths(const string &base_path) const
{
	StringList	path_list;
	
	uLog(PREFS, " saving %zu sources", m_SourceFileMap.size());
	
	int	i = 0;
	
	for (const string &shortname : m_SourceFileOrder)
	{
		assert(m_SourceFileMap.count(shortname));
		
		const ISourceFileClass	*sfc = m_SourceFileMap.at(shortname);
		assert(sfc);
		
		const string	rel_path = sfc->GetRelativePath(base_path);
		path_list.push_back(rel_path);
		
		uLog(PREFS, " [%d] %S", i, rel_path);
		i++;
	}
	
	return path_list;
}

//---- Get List of SFC Instances ----------------------------------------------

vector<ISourceFileClass*>	Controller::GetSFCInstances(void) const
{
	vector<ISourceFileClass*>	ptr_list;
	
	for (const string &shortname : m_SourceFileOrder)
	{
		assert(m_SourceFileMap.count(shortname));
		
		ISourceFileClass	*sfc = m_SourceFileMap.at(shortname);
		assert(sfc);
		
		ptr_list.push_back(sfc);
	}
	
	return ptr_list;
}

//---- Sort SFC Names ---------------------------------------------------------

void	Controller::SortSFCs(const bool up_f)
{
	std::sort(m_SourceFileOrder.begin(), m_SourceFileOrder.end());
	
	if (!up_f)	std::reverse(m_SourceFileOrder.begin(), m_SourceFileOrder.end());
}

//---- Load Project Done ------------------------------------------------------

void	Controller::LoadProjectDone(void)
{
	uLog(PREFS, "Controller::LoadProjectDone()");
	
	SortSFCs(true/*up?*/);
	
	BroadcastProjectRefresh();
}

//---- Broadcast Project Refresh ----------------------------------------------

void	Controller::BroadcastProjectRefresh(void)
{
	uLog(UI, "Controller::BroadcastProjectRefresh()");
	
	m_Signals.OnRefreshProject(CTX_WX);
	
	m_TopFrame.UpdateToolbarState();		// move out
}

//---- Remove All Source Highlights -------------------------------------------

void	Controller::RemoveAllSourceHighlights(void)
{
	assert(m_TopNotePanel);
	
	m_TopNotePanel->GetEditor().RemoveAllHighlights();
}

//---- Set Loaded Breakpoints from prefs --------------------------------------

void	Controller::SetLoadedBreakpoints(const StringList &bp_list)
{
	m_BreakpointSet.clear();
	
	for (const string &bp : bp_list)
	{
		// check format
		assert(bp.find(':') != string::npos);
		
		auto	res = m_BreakpointSet.insert(bp);
		assert(res.second);
	}	
}

//---- Has Breakpoint? --------------------------------------------------------

bool	Controller::HasBreakpoint_LL(ISourceFileClass &sfc, const int ln) const
{
	const string	key = xsprintf("%s:%d", sfc.GetShortName(), ln);
	
	return m_BreakpointSet.count(key);
}

//---- Delete SFC Breakpoints -------------------------------------------------

int	Controller::DeleteSFCBreakpoints(ISourceFileClass &sfc)
{
	const string	prefix = sfc.GetShortName() + ":";
	
	// not sure if deletion invalidates iterator from Stroustrup so do read-sort and move-back
	unordered_set<string>	rescued;
	
	for (const string &bp : m_BreakpointSet)
	{
		if (bp.find(prefix) != 0)	rescued.insert(bp);
	}
	
	const int	num_deleted = m_BreakpointSet.size() - rescued.size();
	
	m_BreakpointSet = std::move(rescued);
	
	return num_deleted;
}

//---- Add SFC Breakpoints ----------------------------------------------------

void	Controller::AddSFCBreakpoints(ISourceFileClass &sfc, const vector<int> &line_list)
{
	const string	prefix = sfc.GetShortName() + ":";
	
	vector<string>	bp_str_list;
	
	for (int ln : line_list)	bp_str_list.push_back(prefix + to_string(ln));
	
	m_BreakpointSet.insert(bp_str_list.begin(), bp_str_list.end());
}

//---- (re-)Set SFC Breakpoints (replacing earlier) ---------------------------

void	Controller::SetSFCBreakpoints(ISourceFileClass &sfc, const vector<int> &line_list)
{
	DeleteSFCBreakpoints(sfc);
	AddSFCBreakpoints(sfc, line_list);
}

//---- Set Breakpoint from shortname ------------------------------------------

void	Controller::SetSourceBreakpoint(const string &shortname, const int ln, const bool f)
{
	assert(m_SourceFileMap.count(shortname));
	
	ISourceFileClass	*sfc = m_SourceFileMap.at(shortname);
	assert(sfc);
	
	SetBreakpoint_LL(*sfc, ln, f);
}

//---- Get Breakpoint Lines for this source -----------------------------------

vector<int>	Controller::GetSFCBreakpoints(const ISourceFileClass &sfc) const
{
	const string	prefix = sfc.GetShortName() + ":";
	
	vector<int>	bp_lines;
	
	for (const string &bp : m_BreakpointSet)
	{
		if (bp.find(prefix) != 0)		continue;
		
		const string	ln_s = bp.substr(prefix.length());
		
		const int	ln = Soft_stoi(ln_s, -1);
		assert(ln != -1);
		
		bp_lines.push_back(ln);
	}
	
	// sort
	std::sort(bp_lines.begin(), bp_lines.end(), [](int i1, int i2){return (i1 < i2);});
	
	return bp_lines;
}

//===== BOOKMARKS =============================================================

//---- Set Local Path browser MRU Bookmarks (from UI load) --------------------

void	Controller::SetLocalPathBookmarkList(const StringList &bookmarks)
{
	// make sure paths exist
	m_LocalPathBookmarks.clear();
	
	for (const auto dir_path : bookmarks)
	{
		const string	native_path = FileName::UnixToNativePath(dir_path);

		if (!wxFileName::DirExists(wxString{native_path}))		continue;	// skip dirs that no longer exist
		
		m_LocalPathBookmarks.push_back(dir_path);
	}
	
	// (bookmarks will get pulled from local filesystem browser)
	BroadcastProjectRefresh();
}

//---- Get Local Dir browser Bookmark List ------------------------------------

StringList	Controller::GetLocalPathBookmarkList(void) const
{
	return m_LocalPathBookmarks;
}

//---- Update Local Path MRU Bookmark -----------------------------------------

void	Controller::UpdateLocalPathBookmark(const string &bmk)
{
	uLog(PREFS, "Controller::UpdateLocalPathBookmark(%S)", bmk);
	
	assert(!bmk.empty());
	
	// delete any existing
	m_LocalPathBookmarks.Erase(bmk);
	
	// insert at front
	m_LocalPathBookmarks.insert(m_LocalPathBookmarks.begin(), bmk);
	
	// clamp (lame loop)
	while (m_LocalPathBookmarks.size() > LOCAL_PATH_MRU_MAX)
		m_LocalPathBookmarks.pop_back();
	
	// will get pulled on UI prefs save
	DirtyUIPrefs();
}

//---- Clear LocalPath MRU Bookmarks ------------------------------------------

void	Controller::ClearLocalPathBookmarks(void)
{
	m_LocalPathBookmarks.clear();
	
	// will get pulled on UI prefs save
	DirtyUIPrefs();
}


// nada mas
