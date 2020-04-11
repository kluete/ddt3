// lua ddt generic directory tree navigation control

#ifndef LUA_DDT_DIRECTORY_TREE_CONTROL_H_INCLUDED
#define	LUA_DDT_DIRECTORY_TREE_CONTROL_H_INCLUDED

#include <unordered_map>
#include <unordered_set>
#include <string>

#include "wx/wx.h"

#include "wx/treectrl.h"
// #include "wx/treebase.h"
#include "wx/generic/treectlg.h"

#include "ddt/sharedDefs.h"

#ifndef nil
	#define	nil	nullptr
#endif

namespace DDT_CLIENT
{
using namespace DDT;

enum class TREE_ITEM_TYPE : uint8_t
{
	NONE = 0,
	DIR_T,
	FILE_T
};

using PathToItemId = std::unordered_map<std::string, wxTreeItemId>;
using ItemIdToPath = std::unordered_map<wxTreeItemId, std::string, wxPointerHash, wxPointerEqual>;
using ItemIdToType = std::unordered_map<wxTreeItemId, TREE_ITEM_TYPE, wxPointerHash, wxPointerEqual>;
using PathToColor = std::unordered_map<std::string, wxColour>;

// early forward references
class DirTreeCtrl;

class DirTreeListener
{
public:
	// ctor
	DirTreeListener()	{}
	// dtor
	
	// return [ok]
	virtual bool	NotifyDragOriginator(DirTreeCtrl *originator, const StringList &path_list) = 0;
	virtual bool	NotifyDragLoop(DirTreeCtrl *recipient, const std::string &highlight_path, const wxPoint &pt) = 0;
	virtual bool	NotifyDragRecipient(DirTreeCtrl *recipient, const std::string &dest_path, const StringList &opt_path_list) = 0;
	
protected:
	
	void	ResetListener(void)
	{
		m_Originator = nil;
		m_PathList.clear();
		m_PathMask = 0;
	}
	
	class DirTreeCtrl	*m_Originator;
	StringList		m_PathList;
	uint32_t		m_PathMask;
};

enum PATH_LIST_FLAG : uint32_t
{
	PATH_LIST_FILE_SINGLE	= (1L << 0),
	PATH_LIST_FILE_MULTI	= (1L << 1),
	PATH_LIST_DIR_SINGLE	= (1L << 2),
	PATH_LIST_DIR_MULTI	= (1L << 3),
	
	// test masks
	PATH_LIST_FILE_ANY	= PATH_LIST_FILE_SINGLE | PATH_LIST_FILE_MULTI,
	PATH_LIST_DIR_ANY	= PATH_LIST_DIR_SINGLE | PATH_LIST_DIR_MULTI,
	PATH_LIST_ANY_MULTI	= PATH_LIST_FILE_MULTI | PATH_LIST_DIR_MULTI,
	PATH_LIST_ANY_SINGLE	= PATH_LIST_FILE_SINGLE | PATH_LIST_DIR_SINGLE
};

//---- Directory Tree control -------------------------------------------------

const uint32_t	DIR_TREE_DEFAULT_STYLE = wxTR_HAS_BUTTONS | wxTR_NO_LINES | wxTR_FULL_ROW_HIGHLIGHT | wxTR_MULTIPLE;

class DirTreeCtrl: public wxGenericTreeCtrl
{
public:
	// ctor
	DirTreeCtrl(wxWindow *parent, int id, DirTreeListener *listener, const std::string &root_path, const uint32_t &styl = DIR_TREE_DEFAULT_STYLE);
	// dtor
	virtual ~DirTreeCtrl();

	void		RequestRedraw(void);
	void		Redraw(void);
	void		HighlightPath(const std::string &path, const bool &select_f);
	uint32_t	GetPathListMask(const StringList &path_list) const;
	
	// drop target implementations
	virtual bool	OnDragOver(const wxPoint &pt);
	virtual bool	OnDropFiles(const wxPoint &pt, const wxArrayString &filenames);
	
	// PURE VIRTUAL
	virtual wxString	ImpName(void) const = 0;
	virtual	int		StatusIndex(void) const = 0;
	virtual bool		IsLocal(void) const = 0;
	
	bool	IsRemote(void) const
	{	return !IsLocal();
	}
	
protected:
	
	virtual bool	OnDirListing(const StringSet &expanded_set) = 0;
	
	virtual std::vector<int>	GetContextMenuIDs(const std::string &path) const = 0;

	virtual bool	OnNewBookmark(const std::string &path)						{return false;}
	virtual bool	OnNewDir(const std::string &parent_dir, const std::string &new_child_dir)	{return false;}
	virtual bool	OnDeleteFiles(const StringList &path_list)					{return false;}
	virtual bool	OnDeleteDir(const std::string &path)						{return false;}
	virtual bool	OnRenameFile(const std::string &old_path, const std::string &new_path)		{return false;}
	virtual bool	OnRenameDir(const std::string &old_path, const std::string &new_path)		{return false;}
	
	// virtuals
	virtual bool	OnDoubleClick(const std::string &path)
	{	// returns [event_handled]
		return false;
	}
	
	StringList	GetSelectionList(void) const;
	TREE_ITEM_TYPE	GetPathType(const std::string &path, const bool &fatal_f = false) const;
	bool		HiddenRoot(void) const;
	
	std::string	m_TreeRootPath;
	StringList	m_DirList;
	StringList	m_FileList;
	StringSet	m_ExpandedPathSet, m_ExpandedPathSet_fromUI;
	PathToColor	m_ColorMap;
	
private:
	
	void		ClearTree(void);
	void		CollapsePaths(void);
	void		RenderRecursive(const wxTreeItemId &base_item, std::string path, const int &depth = 0);
	void		ColorPath(const wxString &path);
	
	void		GetExpandedSetFromUI(void);
	
	void		RenderOneDir(const std::string &fullpath);
	
	bool		TreeContainsPath(const std::string &path) const;
	
	wxTreeItemId	GetPathItem(const std::string &path, const bool &fatal_f = true) const;
	std::string	GetItemPath(const wxTreeItemId &item, const bool &fatal_f = true) const;
	TREE_ITEM_TYPE	GetItemType(const wxTreeItemId &item) const;
	
	wxTreeItemId	PickDragTargetItem(const wxPoint &pt) const;
	
	void		ToggleBranch(const std::string &path);
	
	void		RestoreSelection(const StringList &selected_paths);
	
	// events
	// void	OnCloseEvent(wxCloseEvent &e);		// (never called)
	void	OnItemSelected(wxTreeEvent &e);
	void	OnItemActivated(wxTreeEvent &e);
	void	OnContextMenu(wxTreeEvent &e);
	void	OnKeyEvent(wxTreeEvent &e);
	
	void	OnToolEvent(wxCommandEvent &e);

	void	OnItemExpanding(wxTreeEvent &e);
	void	OnItemCollapsing(wxTreeEvent &e);
	void	OnItemExpandDone(wxTreeEvent &e);
	void	OnItemCollapseDone(wxTreeEvent &e);

	void	OnSetFocusEvent(wxFocusEvent &e);
	void	OnKillFocusEvent(wxFocusEvent &e);
	
	void	OnBeginDrag(wxTreeEvent &e);
	// void	OnEndDrag(wxTreeEvent &e);
	
	void	OnLabelEditBeginEvent(wxTreeEvent &e);
	void	OnLabelEditEndEvent(wxTreeEvent &e);
	
	void	OnDeleteDirEvent(wxCommandEvent &e);
	void	OnDeleteFilesEvent(wxCommandEvent &e);
	
	DirTreeListener			*m_DirTreeListener;
	wxImageList			*m_ImageList, *m_ExpandedStateImageList;
	
	wxTreeItemId			m_RootId, m_BookmarkRootId;
	wxTreeItemId			m_LastDragTargetItem;
	std::vector<wxTreeItemId>	m_DirItemList;
	bool				m_RedrawRequestedFlag;
	bool				m_RedrawingFlag;
	
	// bi-dir hash LUTs
	PathToItemId		m_PathToItemIdMap;
	ItemIdToPath		m_ItemIdToPathMap;
	ItemIdToType		m_ItemIdToType;
	
	DECLARE_EVENT_TABLE()
};

} // namespace DDT_CLIENT

#endif // LUA_DDT_DIRECTORY_TREE_CONTROL_H_INCLUDED

// nada mas
