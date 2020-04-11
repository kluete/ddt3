// lua ddt Local Directory Control

#ifndef LUA_DDT_LOCAL_DIRECTORY_TREE_CONTROL_DEFINED_H
#define	LUA_DDT_LOCAL_DIRECTORY_TREE_CONTROL_DEFINED_H

#include "ddt/sharedDefs.h"

#include "DirTreeCtrl.h"

namespace DDT_CLIENT
{
// forward declarations
class Controller;

//---- Local Directory control ------------------------------------------------

// class LocalDirCtrl: virtual public DirTreeCtrl
class LocalDirCtrl: public DirTreeCtrl
{
public:
	// ctor
	LocalDirCtrl(wxWindow *parent, int id, DirTreeListener *dtl, Controller *controller);
	// dtor
	virtual ~LocalDirCtrl();
	
	// functions
	std::string	GetSingleSelectedDir(void) const;
	uint32_t	GetLocalPathListMask(const StringList &dir) const;
	
	// IMPLEMENTATIONS
	virtual bool	OnDirListing(const StringSet &expanded_set);
	
	virtual bool	OnNewBookmark(const std::string &path);
	virtual bool	OnNewDir(const std::string &parent_dir, const std::string &new_child_dir);
	virtual bool	OnRenameDir(const std::string &old_path, const std::string &new_path);
	virtual bool	OnDeleteDir(const std::string &path);
	virtual bool	OnDeleteFiles(const StringList &path_list);
	virtual std::vector<int>	GetContextMenuIDs(const std::string &path) const;
	
	virtual bool		IsLocal(void) const		{return true;}
	virtual wxString	ImpName(void) const;
	virtual int		StatusIndex(void) const;
	
private:

	Controller			*m_Controller;
	std::vector<std::string>	m_Volumes;
};

} // namespace DDT_CLIENT

#endif // LUA_DDT_LOCAL_DIRECTORY_TREE_CONTROL_DEFINED_H

// nada mas
