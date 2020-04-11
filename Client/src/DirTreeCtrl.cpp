// lua ddt local directory navigation control

#include "wx/filename.h"
#include "wx/filesys.h"
#include "wx/imaglist.h"
#include "wx/artprov.h"
#include "wx/mstream.h"
#include "wx/uri.h"

#include "MappingsFrame.h"

#include "ClientCore.h"

#include "lx/misc/autolock.h"

#include "DropTarget.h"

#include "Controller.h"
#include "TopFrame.h"

#include "ddt/FileSystem.h"

#include "DirTreeCtrl.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

const wxTreeItemId	NULL_TREE_ITEM_ID = (wxTreeItemId)nil;

using MenuIDtoTitle = unordered_map<int, string>;

static const
MenuIDtoTitle	s_MenuIDtoTitle =
{
	{CONTEXT_MENU_ID_TREE_NEW_DIR,		"New Directory"},
	{CONTEXT_MENU_ID_TREE_RENAME_DIR,	"Rename"},
	{CONTEXT_MENU_ID_TREE_DELETE_DIR,	"Delete"},
	{CONTEXT_MENU_ID_TREE_RENAME_FILE,	"Rename"},
	// {CONTEXT_MENU_ID_TREE_DELETE_FILES,	"Delete"},
	{CONTEXT_MENU_ID_TREE_REMOVE_FILE,	"Remove"},
	{CONTEXT_MENU_ID_TREE_SHOW_IN_EXPLORER,	"Show In Explorer"},
	{CONTEXT_MENU_ID_TREE_BOOKMARK,		"Bookmark"}
};
	
//---- Event Table ------------------------------------------------------------

BEGIN_EVENT_TABLE(DirTreeCtrl, wxGenericTreeCtrl)

	EVT_SET_FOCUS(								DirTreeCtrl::OnSetFocusEvent)
	EVT_KILL_FOCUS(								DirTreeCtrl::OnKillFocusEvent)
	
	EVT_TREE_SEL_CHANGED(		-1,					DirTreeCtrl::OnItemSelected)
	EVT_TREE_ITEM_ACTIVATED(	-1,					DirTreeCtrl::OnItemActivated)

	EVT_TREE_ITEM_MENU(		-1,					DirTreeCtrl::OnContextMenu)
	
	EVT_TREE_ITEM_EXPANDING(	-1,					DirTreeCtrl::OnItemExpanding)
	EVT_TREE_ITEM_COLLAPSING(	-1,					DirTreeCtrl::OnItemCollapsing)
	EVT_TREE_ITEM_EXPANDED(		-1,					DirTreeCtrl::OnItemExpandDone)
	EVT_TREE_ITEM_COLLAPSED(	-1,					DirTreeCtrl::OnItemCollapseDone)
	
	// EVT_TREE_STATE_IMAGE_CLICK(	-1,					DirTreeCtrl::OnStateImageClicked)	// for VISIBLE custom images with Set/AssignImageList()
	
	EVT_TREE_BEGIN_DRAG(		-1,					DirTreeCtrl::OnBeginDrag)
	// EVT_TREE_END_DRAG(		-1,					DirTreeCtrl::OnEndDrag)			// never called ?
	
	EVT_TREE_BEGIN_LABEL_EDIT(	-1,					DirTreeCtrl::OnLabelEditBeginEvent)
	EVT_TREE_END_LABEL_EDIT(	-1,					DirTreeCtrl::OnLabelEditEndEvent)
	EVT_TREE_KEY_DOWN(		-1,					DirTreeCtrl::OnKeyEvent)
	
	EVT_MENU(			CONTEXT_MENU_ID_TREE_DELETE_DIR,	DirTreeCtrl::OnDeleteDirEvent)
	// EVT_MENU(			CONTEXT_MENU_ID_TREE_DELETE_FILES,	DirTreeCtrl::OnDeleteFilesEvent)
	
	EVT_TOOL(			-1,					DirTreeCtrl::OnToolEvent)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	// root must be visible for right-click?

	DirTreeCtrl::DirTreeCtrl(wxWindow *parent, int id, DirTreeListener *listener, const string &root_path, const uint32_t &styl)
		: wxGenericTreeCtrl(parent, id, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS | wxTR_NO_LINES | wxTR_FULL_ROW_HIGHLIGHT | styl)
{
	assert(parent);
	assert(listener);
	
	m_DirTreeListener = listener;
	m_TreeRootPath = root_path;
	m_ImageList = nil;
	m_ExpandedStateImageList = nil;
	m_RedrawRequestedFlag = false;
	m_RedrawingFlag = false;
	
	const wxSize icon_sz = wxSize(ICON_SIZE, ICON_SIZE);
	
	m_ImageList = new wxImageList(ICON_SIZE, ICON_SIZE);
	m_ImageList->Add(wxArtProvider::GetIcon(wxART_NORMAL_FILE, wxART_LIST, icon_sz));
	m_ImageList->Add(wxArtProvider::GetIcon(wxART_NORMAL_FILE, wxART_LIST, icon_sz));	// dupe (selected)
	m_ImageList->Add(wxArtProvider::GetIcon(wxART_FOLDER, wxART_LIST, icon_sz));
	m_ImageList->Add(wxArtProvider::GetIcon(wxART_FOLDER, wxART_LIST, icon_sz));		// dupe (selected)
	m_ImageList->Add(wxArtProvider::GetIcon(wxART_FOLDER_OPEN, wxART_LIST, icon_sz));
	SetImageList(m_ImageList);
	
	SetDropTarget(new DropTarget<DirTreeCtrl>(this));
	
	m_ColorMap.clear();
	
	ClearTree();
	
	// CallAfter(&DirTreeCtrl::RequestRedraw);	// CACA
	// Redraw();					// do NOT call virtual function in ctor
}

//---- DTOR -------------------------------------------------------------------

	DirTreeCtrl::~DirTreeCtrl()
{
	uLog(DTOR, "DirTreeCtrl::DTOR()");
	
	if (GetDropTarget())	SetDropTarget(nil);	// unregister drop target
	
	m_DirTreeListener = nil;
	
	SetImageList(nil);
	wxDELETE(m_ExpandedStateImageList);
}

//---- Is Root Hidden ? -------------------------------------------------------

bool	DirTreeCtrl::HiddenRoot(void) const
{
	return HasFlag(wxTR_HIDE_ROOT);
}

//---- Clear Tree -------------------------------------------------------------

void	DirTreeCtrl::ClearTree(void)
{
	DeleteAllItems();
	
	m_PathToItemIdMap.clear();
	m_ItemIdToPathMap.clear();
	m_ItemIdToType.clear();
	m_BookmarkRootId = NULL_TREE_ITEM_ID;
	
	m_RootId = AddRoot("/", TreeCtrlIcon_Folder, TreeCtrlIcon_FolderSelected);
	// const bool	f = m_RootId.IsOk();
	assert(m_RootId.IsOk());

	m_PathToItemIdMap.insert({ "/", m_RootId });
	m_ItemIdToPathMap.insert({ m_RootId, "/" });
	m_ItemIdToType.insert( {m_RootId, TREE_ITEM_TYPE::DIR_T } );
	
	// (so has arrow when closed)
	SetItemHasChildren(m_RootId, true);		// not shown on Windows !
	
	CollapsePaths();
}

//---- Collapse Paths ---------------------------------------------------------

void	DirTreeCtrl::CollapsePaths(void)
{
	m_ExpandedPathSet.clear();
	// (or won't show anything)
	if (HiddenRoot())
	{	// Expand(m_RootId); 	// triggers assert
		m_ExpandedPathSet.insert("/");
	}
	
	CollapseAll();
}

//---- Has Path ? -------------------------------------------------------------

bool	DirTreeCtrl::TreeContainsPath(const string &path) const
{
	return (m_PathToItemIdMap.count(path) > 0);
}

//---- Get Path Item ----------------------------------------------------------

wxTreeItemId	DirTreeCtrl::GetPathItem(const string &path, const bool &fatal_f) const
{	
	const auto	it = m_PathToItemIdMap.find(path);
	if (it != m_PathToItemIdMap.end())	return it->second;
	
	uErr("DirTreeCtrl::GetPathItem(%S) FAILED", path);		// can happen when deleting a Lua source, i.e. not a real bug
	
	assert(!fatal_f);
	
	return wxTreeItemId();
}

//---- Get Item Path ----------------------------------------------------------

string	DirTreeCtrl::GetItemPath(const wxTreeItemId &item, const bool &fatal_f) const
{	
	const auto	it = m_ItemIdToPathMap.find(item);
	if (it != m_ItemIdToPathMap.end())	return it->second;
	
	uErr("DirTreeCtrl::GetItemPath() FAILED");
	
	assert(!fatal_f);
	
	return string("");
}

//---- get Item Type (from wxTreeItemId) --------------------------------------

TREE_ITEM_TYPE	DirTreeCtrl::GetItemType(const wxTreeItemId &item) const
{
	const auto	it = m_ItemIdToType.find(item);
	if (it == m_ItemIdToType.end())		return TREE_ITEM_TYPE::NONE;		// missing
	
	return it->second;
}

//---- Get Item Type ----------------------------------------------------------

TREE_ITEM_TYPE	DirTreeCtrl::GetPathType(const std::string &path, const bool &fatal_f) const
{
	return GetItemType(GetPathItem(path, fatal_f));
}

//---- Get PathList Mask ------------------------------------------------------

uint32_t	DirTreeCtrl::GetPathListMask(const StringList &path_list) const
{
	uLog(DIR_CTRL, "%s::GetPathListMask()", ImpName());
	
	int	dir_cnt, file_cnt;
	
	dir_cnt = file_cnt = 0;
	
	for (const auto path : path_list)
	{
		const TREE_ITEM_TYPE	typ = GetPathType(path, true/*fatal*/);
		if (typ == TREE_ITEM_TYPE::DIR_T)		dir_cnt++;
		else if (typ == TREE_ITEM_TYPE::FILE_T)		file_cnt++;
		else	uErr("illegal path type %S", path);
	}
	
	uint32_t	mask = 0;
	
	if (file_cnt == 1)	mask |= PATH_LIST_FILE_SINGLE;
	else if (file_cnt > 1)	mask |= PATH_LIST_FILE_MULTI;

	if (dir_cnt == 1)	mask |= PATH_LIST_DIR_SINGLE;
	else if (dir_cnt > 1)	mask |= PATH_LIST_DIR_MULTI;
	
	return mask;
}

//---- Get Selection List -----------------------------------------------------

StringList	DirTreeCtrl::GetSelectionList(void) const
{
	wxArrayTreeItemIds	selected_ids;
	
	GetSelections(selected_ids/*&*/);
	
	StringList	selection_list;
	
	for (int i = 0; i < selected_ids.Count(); i++)
	{
		const wxTreeItemId	item = selected_ids[i];
		assert(item.IsOk());
		
		const string		path = GetItemPath(item);
		
		selection_list.push_back(path);
	}
	
	selection_list.Sort();
	
	return selection_list;
}

//---- Restore Selection ------------------------------------------------------

void	DirTreeCtrl::RestoreSelection(const StringList &saved_selection)
{
	UnselectAll();
	
	for (const auto path : saved_selection)
	{
		const wxTreeItemId	item = GetPathItem(path, false/*non-fatal*/);
		
		if (item.IsOk())	SelectItem(item);
		// else no longer exists
	}
}

//---- Get Expanded Set from UI -----------------------------------------------

void	DirTreeCtrl::GetExpandedSetFromUI(void)
{
	m_ExpandedPathSet_fromUI.clear();
	
	int	i = 0;
	
	for (auto item : m_DirItemList)
	{
		assert(item.IsOk());
		if (!IsExpanded(item))		continue;		// not expanded
		
		string	path = GetItemPath(item);
		
		uLog(DIR_CTRL, " [%03d] %S", i, path);
		
		i++;
	}
}

//---- Render One Dir ---------------------------------------------------------

void	DirTreeCtrl::RenderOneDir(const string &fullpath)
{
	uLog(DIR_CTRL, "%s::RenderOneDir(%S)", ImpName(), fullpath);
	
	assert(!fullpath.empty());
	
	const StringList	recursive_dirs = FileName::GetRecursivePathSubDirs(fullpath);
	const StringList	dir_list = FileName::GetPathSubDirs(fullpath);
	wxTreeItemId		item = m_RootId;
	
	// uLog(DIR_CTL, "  recursive dirs %S", recursive_dirs.ToFlatString());
	// uLog(DIR_CTL, "  dir_list dirs  %S", dir_list.ToFlatString());
	
	const size_t	n_recurs_dirs = recursive_dirs.size();
	
	for (size_t i = 0; i < n_recurs_dirs; i++)
	{	
		const string path = recursive_dirs[i];
		const string sub_dir = dir_list[i];
		
		auto	it = m_PathToItemIdMap.find(path);
		if (it != m_PathToItemIdMap.end())
		{	// already exists, set new item as new parent
			item = GetPathItem(path);
			assert(item.IsOk());
			continue;		// skip rest
		}
	
		const bool	last_f = (i == n_recurs_dirs - 1);
		
		// node doesn't exist, create with short subdir name (without sep prefix)
		// uLog(DIR_CTL, "  create tree item for path %S, with name %S ", path, sub_dir);
		const wxTreeItemId	parent_item = item;
			
		item = AppendItem(parent_item, sub_dir, TreeCtrlIcon_Folder, TreeCtrlIcon_FolderSelected);
		assert(item.IsOk());
			
		m_PathToItemIdMap.insert( {path, item} );
		m_ItemIdToPathMap.insert( {item, path} );
		m_ItemIdToType.insert( {item, TREE_ITEM_TYPE::DIR_T } );
		
		// save added dir
		if (last_f)	m_DirItemList.push_back(item);
		
		if (parent_item.IsOk() && !IsExpanded(parent_item))		Expand(parent_item);
		
		SetItemHasChildren(item, true);
		
		// highlight bookmarked paths
		const auto	color_it = m_ColorMap.find(path);
		if (color_it != m_ColorMap.end())
		{
			SetItemTextColour(item, color_it->second);
			SetItemBold(item, true);
			// SetItemBackgroundColour
		}
	}
}

//---- Redraw -----------------------------------------------------------------
	
void	DirTreeCtrl::Redraw(void)
{	
	// uLog(DIR_CTL, "%s::Redraw(%zu dirs, %zu files)", ImpName(), m_DirList.size(), m_FileList.size());
	
	assert(!m_RedrawingFlag);
	
	AutoLock	lock(m_RedrawingFlag);
	
	const StringList	saved_selection = GetSelectionList();
	// const wxPoint	saved_scroll_pos = GetViewStart();
	// const wxPoint	saved_scroll_pos = wxScrolled<>GetViewStart();

	// clear & rebuild whole tree, except for hardcoded root
	ClearTree();
	
	m_DirItemList.clear();
	
	// render directories
	for (const string dir : m_DirList)
	{	RenderOneDir(dir);
	}
	
	// render files (finds node where should be added)
	for (const string filepath : m_FileList)
	{
		uLog(DIR_CTRL, "  file %S", filepath);
		assert(!filepath.empty());
				
		const auto	split = FileName::SplitUnixPath(filepath);			// use Unix splitter
		if (!split)
		{	uLog(WARNING, "ignored file %S (couldn't split path)", filepath);
			continue;				// ignore
		}
		
		string	parent_dir = split.Path();
		string	shortname = split.GetFullName();

		assert(!parent_dir.empty());
		
		wxTreeItemId	parent_id = GetPathItem(parent_dir);
		if (!parent_id.IsOk())
		{
			uErr("bad parent_dir %S, fn %S", parent_dir, shortname);
		}
		assert(parent_id.IsOk());
		
		// expand parent since obviously has children
		if (!IsExpanded(parent_id))		Expand(parent_id);
		
		wxTreeItemId	item = AppendItem(parent_id, shortname, TreeCtrlIcon_File, TreeCtrlIcon_FileSelected);
		assert(item.IsOk());

		SetItemHasChildren(parent_id, true);
		
		m_PathToItemIdMap.insert( {filepath, item} );
		m_ItemIdToPathMap.insert( {item, filepath} );
		m_ItemIdToType.insert( {item, TREE_ITEM_TYPE::FILE_T } );
	}
	
	RestoreSelection(saved_selection);
	
	m_RedrawRequestedFlag = false;
	
	// ScrollTo(first_visible_item);	// (item-level doesn't work)
	// Scroll(saved_scroll_pos);		// low-level works
}

//---- Request Redraw ---------------------------------------------------------

void	DirTreeCtrl::RequestRedraw(void)
{
	uLog(DIR_CTRL, "%s::RequestRedraw()", ImpName());
	
	// call implementation BEFORE resets expanded set
	assert(!m_RedrawingFlag);
	
	// test
	GetExpandedSetFromUI();
	
	const bool	redraw_f = OnDirListing(m_ExpandedPathSet);
	
	if (redraw_f && !m_RedrawRequestedFlag)
	{	m_RedrawRequestedFlag = true;
		
		CallAfter(&DirTreeCtrl::Redraw);
	}
	// else if (m_RedrawRequestedFlag)		uErr("m_RedrawRequestedFlag already set");
}

//---- Toggle Branch ----------------------------------------------------------

void	DirTreeCtrl::ToggleBranch(const string &path)
{
	const bool	was_expanded_f = (m_ExpandedPathSet.count(path) > 0);
	
	uLog(DIR_CTRL, "%s::ToggleBranch(path %S, was %s)", ImpName(), path, was_expanded_f ? "expanded -> collapsing" : "collapsed -> expanding");
	uLog(DIR_CTRL, "  expanded set bf %S", m_ExpandedPathSet.ToFlatString());
	
	if (was_expanded_f)
		m_ExpandedPathSet.erase(path);							// collapse
	else	m_ExpandedPathSet.Insert(LX::FileName::GetRecursivePathSubDirs(path));		// expand
	
	const bool	is_expanded = (m_ExpandedPathSet.count(path) > 0);
	
	// confirm toggled
	assert(was_expanded_f != is_expanded);
	
	uLog(DIR_CTRL, "  expanded set af %S", m_ExpandedPathSet.ToFlatString());
}

//---- On Item Expanding ------------------------------------------------------

void	DirTreeCtrl::OnItemExpanding(wxTreeEvent &e)
{
	const wxTreeItemId	item = e.GetItem();
	const string		path = GetItemPath(item);
	
	uLog(DIR_CTRL, "%s::OnItemExpanding(%S)", ImpName(), path);
	
	assert(!IsExpanded(item));
	
	ToggleBranch(path);
	
	e.Skip();
}

//---- On Item Collapsing -----------------------------------------------------

void	DirTreeCtrl::OnItemCollapsing(wxTreeEvent &e)
{
	const wxTreeItemId	item = e.GetItem();
	const string		path = GetItemPath(item);
	
	uLog(DIR_CTRL, "%s::OnItemCollapsing(%S)", ImpName(), path);
	
	assert(IsExpanded(item));
	
	ToggleBranch(path);
	
	e.Skip();
}

//---- On Item Expand DONE ----------------------------------------------------

void	DirTreeCtrl::OnItemExpandDone(wxTreeEvent &e)
{
	e.Skip();
	
	if (m_RedrawingFlag)		return;			// MUTED ???
	
	const wxTreeItemId	item = e.GetItem();
	const string		path = GetItemPath(item);
	uLog(DIR_CTRL, "%s::OnItemExpandDone(%S)", ImpName(), path);
	
	RequestRedraw();
}

//---- On Item Collapse DONE --------------------------------------------------

void	DirTreeCtrl::OnItemCollapseDone(wxTreeEvent &e)
{
	e.Skip();
	
	if (m_RedrawingFlag)		return;			// MUTED ???
	
	const wxTreeItemId	item = e.GetItem();
	const string		path = GetItemPath(item);
	uLog(DIR_CTRL, "%s::OnItemCollapseDone(%S)", ImpName(), path);
	
	RequestRedraw();
}

//---- On Activated (double-click / enter) ------------------------------------

void	DirTreeCtrl::OnItemActivated(wxTreeEvent &e)
{
	const wxTreeItemId	item = e.GetItem();
	const string		path = GetItemPath(item);
	
	uLog(DIR_CTRL, "%s::OnItemActivated(%S)", ImpName(), path);
	
	// call possible implementation
	const bool	handled_f = OnDoubleClick(path);
	if (handled_f)
	{	e.Veto();
		return;
	}
	
	// let focus run its course
	e.Skip();
	
	// ToggleBranch(path);
	
	RequestRedraw();
}

//---- On Set Focus event ---------------------------------------------------------

void	DirTreeCtrl::OnSetFocusEvent(wxFocusEvent &e)
{
	// uLog(DIR_CTRL, "%s::OnSetFocusEvent()", ImpName());
	DumpFocus("DirTreeCtrl::OnSetFocusEvent()");
	
	SetOwnBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
	
	e.Skip();
}

//---- On Kill Focus event ----------------------------------------------------

void	DirTreeCtrl::OnKillFocusEvent(wxFocusEvent &e)
{
	// uLog(DIR_CTRL, "%s::OnKillFocusEvent()", ImpName());
	DumpFocus("DirTreeCtrl::OnKillFocusEvent()");
	
	// if not caused by context menu or top-level (used for warnings)
	wxWindow	*focused_w = e.GetWindow();
	if (focused_w && !IsDescendant(focused_w) && !focused_w->IsTopLevel())
	{	const wxColour	clr = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW).ChangeLightness(97);
		SetOwnBackgroundColour(clr);
	}
	else
	{	// AVOID log on focus, happens too often and locks scroll windows
		// uLog(EV_FOCUS, "%s::OnKillFocusEvent() VETOED", ImpName());
	}
		
	e.Skip();
}

//---- On Selected ------------------------------------------------------------

void	DirTreeCtrl::OnItemSelected(wxTreeEvent &e)
{	
	e.Skip();

	const wxTreeItemId	item = e.GetItem();
	const string		path = GetItemPath(item);
	
	uLog(DIR_CTRL, "path %S selected", path);
}

//---- On Context Menu --------------------------------------------------------

void	DirTreeCtrl::OnContextMenu(wxTreeEvent &e)
{
	const wxTreeItemId	selected_item = e.GetItem();
	const string		path = GetItemPath(selected_item);
	
	// get context menu IDs (maybe from implementation)
	vector<int>	menu_id_list = GetContextMenuIDs(path);
	
	// build context menu from IDs
	wxMenu	menu;
	
	for (const int menu_id : menu_id_list)
	{
		if (menu_id == CONTEXT_MENU_ID_TREE_sep)
		{
			menu.AppendSeparator();
			continue;
		}
		
		// look up static def
		const auto	it = s_MenuIDtoTitle.find(menu_id);
		assert(it != s_MenuIDtoTitle.end());
		
		const wxString	menu_title = it->second;
		assert(!menu_title.empty());
		
		menu.Append(menu_id, menu_title);
	}
	
	// immediate popup menu -- will send FOCUS event
	const int	id = GetPopupMenuSelectionFromUser(menu);
	
	e.Skip();	// release mouse
	
	if (id < 0)					return;		// canceled
	
	switch (id)
	{
		case CONTEXT_MENU_ID_TREE_BOOKMARK:
		{
			const bool	ok = OnNewBookmark(path);
			(void)ok;
		}	break;
		
		case CONTEXT_MENU_ID_TREE_SHOW_IN_EXPLORER:
		{
			const wxString	native_path(FileName::UnixToNativePath(path));
			assert(!native_path.empty());
			
			wxFileName	cfn{native_path};
			assert(cfn.IsOk());
			
			const wxString	uri_s = wxFileSystem::FileNameToURL(cfn);
			assert(!uri_s.empty());
			
			uLog(DIR_CTRL, "show in explorer %S", uri_s);
			
			wxLaunchDefaultBrowser(uri_s, wxBROWSER_NOBUSYCURSOR);
			return;
		}	break;
		
		case CONTEXT_MENU_ID_TREE_NEW_DIR:
		{
			// (can't create node & edit in one go, so prompt)
			const wxString	new_dir = wxGetTextFromUser("New Dir", "Create New Directory", ""/*default*/, this);
			if (new_dir.IsEmpty())		return;		// canceled
			
			uLog(DIR_CTRL, "%s::OnNewDir(parent dir %S, child dir %S)", ImpName(), path, new_dir);
			
			// call implementation
			bool	ok = OnNewDir(path/*parent dir*/, new_dir.ToStdString());
			assert(ok);
		}	break;
		
		case CONTEXT_MENU_ID_TREE_RENAME_DIR:
		
			EditLabel(selected_item);
			// wait for edit completion
			return;
			break;
			
		case CONTEXT_MENU_ID_TREE_REMOVE_FILE:
		{
			// (don't preserve selection)
			UnselectAll();
	
			// remove without warning
			const bool	ok = OnDeleteFiles(StringList{path});
			assert(ok);
		}	break;

		// case CONTEXT_MENU_ID_TREE_DELETE_FILES:
		case CONTEXT_MENU_ID_TREE_DELETE_DIR:
		{
			// post to self
			wxPostEvent(this, wxMenuEvent(wxEVT_COMMAND_MENU_SELECTED, id));
			return;
		}	break;
		
		default:
		
			uErr("unhandled context menu ID %d", id);
			return;
			break;
	}
	
	RequestRedraw();
}

//---- On Delete Directory event ----------------------------------------------

void	DirTreeCtrl::OnDeleteDirEvent(wxCommandEvent &e)
{
	const StringList	selections = GetSelectionList();
	assert(selections.size() == 1);
	
	string	path = selections.front();
	
	const wxString	prompt = wxString::Format("Delete Directory \"%s\" ?", wxString(path));
	const int	res = wxMessageBox(prompt, "Confirm", wxYES_NO | wxCANCEL | wxICON_WARNING, this);
	if (res != wxYES)		return;		// canceled
	
	// (don't preserve selection)
	UnselectAll();
	
	const bool	ok = OnDeleteDir(path);
	if (!ok)			return;		// canceled, maybe because had children
	
	RequestRedraw();
}

//---- On Delete Files event --------------------------------------------------

void	DirTreeCtrl::OnDeleteFilesEvent(wxCommandEvent &e)
{
	const StringList	selections = GetSelectionList();
	
	wxString	prompt = "Delete File(s)\n\n";
	
	for (const auto fn : selections)
	{
		prompt += wxString::Format("\t\"%s\"\n", wxString(fn));
	}
	
	prompt += "\n?";
	
	uLog(DIR_CTRL, "%s::OnDeleteFileEvent(%S)", ImpName(), prompt);
	
	const int	res = wxMessageBox(prompt, "Confirm", wxYES_NO | wxCANCEL | wxICON_WARNING, this);
	if (res != wxYES)		return;		// canceled
	
	// (don't preserve selection)
	UnselectAll();
	
	const bool	ok = OnDeleteFiles(selections);
	assert(ok);
	
	RequestRedraw();
}

//---- On Key Event -----------------------------------------------------------

void	DirTreeCtrl::OnKeyEvent(wxTreeEvent &e)
{
	e.Skip();
	
	const StringList	selections = GetSelectionList();
	const int		n_selected = selections.size();
	if (n_selected == 0)	return;
	
	const string		path = selections.front();
	const TREE_ITEM_TYPE	typ = GetPathType(path, true/*fatal*/);
	assert((typ == TREE_ITEM_TYPE::DIR_T ) || (typ == TREE_ITEM_TYPE::FILE_T ));
	
	switch (e.GetKeyCode())
	{	case WXK_F2:
			
			if (n_selected == 1)	EditLabel(GetPathItem(path));
			else			uErr("can't rename multiple selection");
			break;
		
		case WXK_DELETE:
		{	
			wxMenuEvent	me(wxEVT_COMMAND_MENU_SELECTED, CONTEXT_MENU_ID_TREE_DELETE_DIR);
			
			// if (typ == TREE_ITEM_TYPE::FILE)	me.SetId(CONTEXT_MENU_ID_TREE_DELETE_FILES);
	
			/*if (n_selected == 1)*/	wxPostEvent(this, me);
			// else			uErr("multiple selection delete NOT IMPLEMENTED");
		}	break;
			
		default:
			
			break;
	}
}

//---- On Edit BEGIN ----------------------------------------------------------

void	DirTreeCtrl::OnLabelEditBeginEvent(wxTreeEvent &e)
{
	const StringList	selections = GetSelectionList();
	if (selections.size() != 1)	return e.Veto();
	
	const wxTreeItemId	item = GetPathItem(selections.front());
	if (item == m_RootId)	return e.Veto();				// use GENERIC PERMISSIONS - fixme
	const bool		dir_f = (m_ItemIdToType.at(item) == TREE_ITEM_TYPE::DIR_T );
	if (!dir_f)		return e.Veto();				// can't rename a file ???
	
	e.Skip();
}

//---- On Edit END ------------------------------------------------------------

void	DirTreeCtrl::OnLabelEditEndEvent(wxTreeEvent &e)
{
	if (e.IsEditCancelled())	return;
	
	const wxTreeItemId	item = e.GetItem();
	if (!item.IsOk())	return e.Veto();
	const string		old_path = GetItemPath(item);
	
	const string	new_path_name = e.GetLabel().ToStdString();
	
	// check duplicate
	if (TreeContainsPath(new_path_name))
	{	wxMessageBox("Duplicate Path", "Error", wxOK | wxICON_ERROR, this);
		return e.Veto();					// canceled
	}
	
	// (don't preserve selection)
	UnselectAll();
	
	uLog(DIR_CTRL, "%s::OnRename %S to %S", ImpName(), old_path, new_path_name);
	
	const bool	ok = OnRenameDir(old_path, new_path_name);
	if (!ok)
	{	// error
		uErr("  OnRenameDir(%S, %S) failed", old_path, new_path_name);
		return;
	}
	
	// need to update expandedSet?
	RequestRedraw();
}

//---- Select/Highlight Path (called from LuaDir map) -------------------------

void	DirTreeCtrl::HighlightPath(const string &path, const bool &select_f)
{
	// manually get expandedSet...
	StringSet	expanded_set(LX::FileName::GetRecursivePathSubDirs(path).ToStringSet());
	
	// ... and fetch dir
	const bool	redraw_f = OnDirListing(expanded_set);
	assert(redraw_f);
	
	// must be immediate
	Redraw();
	
	const wxTreeItemId	item = GetPathItem(path);
	
	// (doesn't automatically expand)
	ScrollTo(item);
	
	if (select_f)
	{	UnselectAll();
		SelectItem(item);
	}
	
	// ! doesn't scroll into view on Gtk... because renders AFTER scrolling?
	EnsureVisible(item);
}

//---- Pick Drag Target Item --------------------------------------------------

wxTreeItemId	DirTreeCtrl::PickDragTargetItem(const wxPoint &pt) const
{
	int		flags = 0;
	wxTreeItemId	target_item = HitTest(pt, flags/*&*/);
	
	if (!target_item.IsOk() || !(wxTREE_HITTEST_ONITEM & flags))		return NULL_TREE_ITEM_ID;	// no hit
	
	const TREE_ITEM_TYPE	typ = GetItemType(target_item);
	assert(typ != TREE_ITEM_TYPE::NONE);
	
	if (typ == TREE_ITEM_TYPE::DIR_T )					return target_item;		// return hit DIR
	
	// hit FILE, return parent container
	target_item = GetItemParent(target_item);
	assert(target_item.IsOk() && (GetItemType(target_item) == TREE_ITEM_TYPE::DIR_T ));
	
	return target_item;
}

//---- On Begin Drag ----------------------------------------------------------

void	DirTreeCtrl::OnBeginDrag(wxTreeEvent &e)
{
	uLog(DIR_CTRL, "%s::OnBeginDrag()", ImpName());
	
	assert(m_DirTreeListener);
	
	// can APPARENTLY try to drag the NULL item !?
	const wxTreeItemId	item = e.GetItem();
	if (!item.IsOk() || (item == m_RootId))		return e.Veto();
	
	// get multiple selection
	const StringList	path_list = GetSelectionList();
	wxFileDataObject	data;
	
	// copy to dragSource data
	for (const auto path : path_list)	data.AddFile(wxString(path));
	
	wxDropSource	dragSource(this/*initiating window*/);
	
	dragSource.SetData(data);
	
	// DRAG START
	bool	ok = m_DirTreeListener->NotifyDragOriginator(this, path_list);
	if (!ok)
	{	uLog(WARNING, "  VETOED");
		e.Skip();
		return;
	}
	
	m_LastDragTargetItem = NULL_TREE_ITEM_ID;
	e.Allow();
	
	// blocks until drag done/mouse released
	wxDragResult	res = dragSource.DoDragDrop(wxDrag_CopyOnly);			// ALLOW MOVE?
	uLog(DIR_CTRL, "  %s", DropTarget<DirTreeCtrl>::ResultString(res));		// crash-prone?
	
	// (release mouse)
	e.Skip();
}

//---- On Drag Over -----------------------------------------------------------

	// returns [ok]

bool	DirTreeCtrl::OnDragOver(const wxPoint &pt)
{
	assert(m_DirTreeListener);
	
	const wxTreeItemId	target_item = PickDragTargetItem(pt);
	if (target_item == m_LastDragTargetItem)	return true;		// no change
	
	// disable any previous highlight
	if (m_LastDragTargetItem.IsOk())	SetItemDropHighlight(m_LastDragTargetItem, false);
	
	m_LastDragTargetItem = target_item;
	
	string	new_highlight;
	
	if (target_item.IsOk())
	{	// new highlight
		SetItemDropHighlight(target_item, true);
	
		new_highlight = GetItemPath(target_item);
	}
	else
	{	// no new highlight
		new_highlight = "";
	}
	
	// DRAG LOOP
	bool	ok = m_DirTreeListener->NotifyDragLoop(this, new_highlight, pt);
	return ok;
}

//---- On Drop Files ----------------------------------------------------------

bool	DirTreeCtrl::OnDropFiles(const wxPoint &pt, const wxArrayString &array_source_paths)
{
	uLog(DIR_CTRL, "%s::NotifyDragRecipient()", ImpName());
	assert(m_DirTreeListener);
	
	// get target item
	const wxTreeItemId	target_item = PickDragTargetItem(pt);
	if (!target_item.IsOk())
	{	uErr("PickDragTargetItem() target item failed");
		return false;
	}
	
	// rebuild path_list
	StringList	path_list;
	
	for (int i = 0; i < array_source_paths.Count(); i++)
	{
		const string	path = array_source_paths[i].ToStdString();
		
		path_list.push_back(path);
	}
	
	const string	drag_to = GetItemPath(target_item);
	
	// DRAG END
	bool	ok = m_DirTreeListener->NotifyDragRecipient(this, drag_to, path_list);
	if (!ok)
	{	uErr("m_DirTreeListener->NotifyDragRecipient() operation refused");
		return false;
	}
	
	RequestRedraw();
	
	return true;		// ok
}

//---- On Tool event ----------------------------------------------------------

void	DirTreeCtrl::OnToolEvent(wxCommandEvent &e)
{
	uLog(DIR_CTRL, "%s::OnToolEvent(%d)", ImpName(), e.GetId());
}

// nada mas
