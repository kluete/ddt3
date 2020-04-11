// Lua DirMap control - virtual directories

#ifndef LUA_DDT_LUA_DIRECTORY_MAPPER_CONTROL_H_INCLUDED
#define	LUA_DDT_LUA_DIRECTORY_MAPPER_CONTROL_H_INCLUDED

#include "wx/treectrl.h"

#ifndef nil
	#define nil	nullptr
#endif

// forward declarations
class MappingsFrame;
class Controller;
class ClientDirCtrl;

class StringNode;

//---- Lua Dir-Mapping tree control -------------------------------------------

class LuaDirCtrl: public wxTreeCtrl
{
public:
	// ctor
	LuaDirCtrl(wxWindow *parent, int id, MappingsFrame *mf, Controller *controller);
	
	void		Reset(void);
	void		RequestRedraw(const bool reset_selection_f = false);
	wxTreeItemId	RenderRecursive(wxTreeItemId parent_item, StringNode *snp);
	void		Redraw(void);
	
	StringNode*	GetItemStringNode(const wxTreeItemId &item);
	void		OnItemSelectedEvent(wxTreeEvent &e);
	void		OnRightClickEvent(wxTreeEvent &e);
	void		OnLabelEditBeginEvent(wxTreeEvent &e);
	void		OnLabelEditEndEvent(wxTreeEvent &e);
	void		OnKeyEvent(wxTreeEvent &e);	
	void		OnDragOver(const wxPoint pt);
	
private:
	
	MappingsFrame	*m_MappingsFrame;
	Controller	*m_Controller;
	
	wxImageList	*m_ImageList;
	wxImageList	*m_ButtonImageList;
	
	wxTreeItemId	m_LuaRootItem;
	StringNode	*m_TopNode;
	
	StringNode	*m_BoldItem;
	
	DECLARE_EVENT_TABLE()
};

#endif // LUA_DDT_LUA_DIRECTORY_MAPPER_CONTROL_H_INCLUDED

// nada mas
