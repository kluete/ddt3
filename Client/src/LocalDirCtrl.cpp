// lua ddt Local Directory Control
#include "wx/filename.h"
#include "wx/volume.h"

#include "ClientCore.h"
#include "ddt/FileSystem.h"
#include "UIConst.h"
#include "Controller.h"

#include "LocalDirCtrl.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

//---- CTOR -------------------------------------------------------------------

	LocalDirCtrl::LocalDirCtrl(wxWindow *parent, int id, DirTreeListener *listener, Controller *controller)
		: DirTreeCtrl(parent, id, listener, FileName::ROOT_PATH, wxTR_MULTIPLE | wxTR_HIDE_ROOT)
{
	assert(controller);
	m_Controller = controller;
	
	m_Volumes.clear();
	
	#ifdef __DDT_WIN32__

		const int	vol_flags = wxFS_VOL_MOUNTED | wxFS_VOL_REMOVABLE;			// wxFS_VOL_REMOTE is crash-prone?
		const int	vol_not_flags = wxFS_VOL_READONLY;					// ignore CDs, DVDs

		wxArrayString	volume_list = wxFSVolume::GetVolumes(vol_flags, vol_not_flags);

		for (int i = 0; i < volume_list.Count(); i++)
		{
			const string	vol = volume_list[i];
			
			uMsg("[%d] volume %S", i, vol);
			
			m_Volumes.push_back(vol);
		}
	#endif
}

//---- DTOR -------------------------------------------------------------------

	LocalDirCtrl::~LocalDirCtrl()
{
	uLog(DTOR, "LocalDirCtrl::DTOR()");
}

//---- On Directory Listing IMPLEMENTATION ------------------------------------

bool	LocalDirCtrl::OnDirListing(const StringSet &expanded_set)
{
	uLog(DIR_CTRL, "LocalDirCtrl::OnDirListing()");
	
	m_DirList.clear();
	m_FileList.clear();

	// if collapsed root (and not hidden), don't return anything
	if (!HiddenRoot() && expanded_set.count("/") == 0)		return true;
	
	uLog(DIR_CTRL, "cdir() expanded set %S", expanded_set.ToFlatString());

	// (never returns root)
	LX::Dir		cdir(expanded_set);
	
	m_DirList = cdir.GetDirs();
	assert(m_DirList.size() > 0);
	m_FileList = cdir.GetFiles();
	
	const int	n_bad_files = cdir.GetNumIllegalFilenames();
	if (n_bad_files > 0)
	{
		uErr("dir had %d illegal files", n_bad_files);
	}

	uLog(DIR_CTRL, " got %lu dirs, %lu files", m_DirList.size(), m_FileList.size());	
	uLog(DIR_CTRL, "  dirs %S", m_DirList.ToFlatString());
	uLog(DIR_CTRL, "  files %S", m_FileList.ToFlatString());
			
	return true;
}

//---- Get Context Menu IDs ---------------------------------------------------

vector<int>	LocalDirCtrl::GetContextMenuIDs(const string &path) const
{
	const TREE_ITEM_TYPE	typ = GetPathType(path, true/*fatal*/);
	
	if (typ == TREE_ITEM_TYPE::DIR_T)
	{	// dir
		return { CONTEXT_MENU_ID_TREE_SHOW_IN_EXPLORER, /*CONTEXT_MENU_ID_TREE_BOOKMARK,*/ CONTEXT_MENU_ID_TREE_sep, CONTEXT_MENU_ID_TREE_NEW_DIR, CONTEXT_MENU_ID_TREE_RENAME_DIR, CONTEXT_MENU_ID_TREE_sep, CONTEXT_MENU_ID_TREE_DELETE_DIR };
	}
	else if (typ == TREE_ITEM_TYPE::FILE_T)
	{	// file
		return { CONTEXT_MENU_ID_TREE_SHOW_IN_EXPLORER, CONTEXT_MENU_ID_TREE_sep, CONTEXT_MENU_ID_TREE_RENAME_FILE, /*CONTEXT_MENU_ID_TREE_sep, CONTEXT_MENU_ID_TREE_DELETE_FILES*/ };
	}
	else
	{	uErr("illegal TREE_ITEM_TYPE");
		return {};
	}
}

//---- On New Bookmark --------------------------------------------------------

bool	LocalDirCtrl::OnNewBookmark(const string &path)
{
	assert(m_Controller);
	
	// update MRU
	m_Controller->UpdateLocalPathBookmark(path);
	
	return true;
}

//---- On New Directory -------------------------------------------------------
	
bool	LocalDirCtrl::OnNewDir(const string &parent_dir, const string &new_child_dir)
{
	string	new_fulldir = FileName::ConcatDir(parent_dir, new_child_dir);
	assert(!new_fulldir.empty());
	
	// check duplicate
	if (FileName::DirExists(new_fulldir))
	{	wxMessageBox("Duplicate Directory", "Error", wxOK | wxICON_ERROR, this);
		return false;					// canceled
	}
			
	return FileName::MkDir(new_fulldir);
}

//---- On Rename Directory ----------------------------------------------------

bool	LocalDirCtrl::OnRenameDir(const string &old_path, const string &new_path)
{
	// if (!FileName::IsFullPath(old_path))	return false;		// error
		
	string	new_full_path;
	
	if (!FileName::IsAbsolutePath(new_path))
	{	string	parent_dir = FileName::PopDirSubDir(old_path);
		
		new_full_path = FileName::ConcatDir(parent_dir, new_path);
	}
	else	new_full_path = new_path;
	
	return FileName::RenamePath(old_path, new_full_path);
}

//---- On Delete Directory ----------------------------------------------------

bool	LocalDirCtrl::OnDeleteDir(const string &path)
{
	return FileName::RmDir(path);
}

//---- On Delete Files --------------------------------------------------------

bool	LocalDirCtrl::OnDeleteFiles(const StringList &path_list)
{
	// Windows' BULLSHIT DeleteFile() macro forced renaming my own function
	return FileName::RemoveFile(path_list.front());
}

//---- Get Implementation instance Name ---------------------------------------

wxString	LocalDirCtrl::ImpName(void) const
{
	return "LocalDirCtrl";
}

//---- Get status bar text Index ----------------------------------------------

int	LocalDirCtrl::StatusIndex(void) const
{
	return 0;
}

//---- Get Single Selected Directory ------------------------------------------

string	LocalDirCtrl::GetSingleSelectedDir(void) const
{
	const StringList	select_list = GetSelectionList();
	if (select_list.size() != 1)			return "";		// not single selection
	
	const string		selected_path = select_list.front();
	
	const TREE_ITEM_TYPE	typ = GetPathType(selected_path, false/*non-fatal*/);
	if (typ != TREE_ITEM_TYPE::DIR_T)		return "";		// not dir selected
	
	return selected_path;
}

// could use generic version ?

uint32_t	LocalDirCtrl::GetLocalPathListMask(const StringList &path_list) const
{
	int	dir_cnt, file_cnt;
	
	dir_cnt = file_cnt = 0;
	
	for (const auto path : path_list)
	{
		wxFileName	cfn{wxString(path)};
		assert(cfn.IsOk());
		
		if (cfn.FileExists())		file_cnt++;
		else if (cfn.DirExists())	dir_cnt++;
		else
		{
			uErr("LocalDirCtrl::GetLocalPathListMask() error - neither file nor dir %S", path);
		}
	}
	
	uint32_t	mask = 0;
	
	if (file_cnt == 1)	mask |= PATH_LIST_FILE_SINGLE;
	else if (file_cnt > 1)	mask |= PATH_LIST_FILE_MULTI;

	if (dir_cnt == 1)	mask |= PATH_LIST_DIR_SINGLE;
	else if (dir_cnt > 1)	mask |= PATH_LIST_DIR_MULTI;
	
	return mask;
}

// nada mas
