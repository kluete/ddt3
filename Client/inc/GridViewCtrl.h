// Lua large-string variable memory controller

#ifndef LUA_DDT_GRID_VIEW_CONTROL_H_INCLUDED
#define	LUA_DDT_GRID_VIEW_CONTROL_H_INCLUDED

#include "wx/wx.h"
#include "wx/panel.h"

#include "ddt/sharedDefs.h"

#include "BottomNotebook.h"

#include "lx/GridImp.h"

class wxStatusBar;

// forward references
class TopFrame;
class Controller;
class BottomNotePanel;

namespace LX
{
	class MemoryInputStream;
	class DataInputStream;
	class Renderer;
	class CellData;
}

//---- Grid View Control ------------------------------------------------------

class GridViewCtrl: public wxWindow, public PaneMixIn, public LX::GridClient
{
public:
	// ctor
	GridViewCtrl(BottomNotePanel *parent, int id, TopFrame *tf, Controller *controller, LX::Renderer *rend, wxImageList *img_list);
	// dtor
	virtual ~GridViewCtrl();
	
	void	SetCellDataList(const std::vector<LX::CellData> &cell_data_list);
	
	// PaneMixIn IMPLEMENTATIONS
	void	ClearVariables(void);
	void	RequestVarReload(void);
	
	// GridClient IMPLEMENTATIONS
	virtual	void		OnGridClientInit(GridImp &grid) override;
	virtual	bool		IsGridShown(GridImp &grid) override;
	virtual LX::Color	GetRowColor(GridImp &grid, const size_t &row) override;
	virtual void		OnGridEvent(GridImp &grid, LX::GridEvent &e) override;
	
private:
	
	BottomNotePanel		*m_BottomNotePanel;
	TopFrame		*m_TopFrame;
	Controller		*m_Controller;
	
	LX::Renderer		*m_CairoRenderer;
	wxImageList		*m_ImageList;
	
	GridPanel		*m_GridPanel;
	wxStatusBar		*m_StatusBar;
	
	bool			m_InitFlag;
	
	DECLARE_EVENT_TABLE()
};

#endif // LUA_DDT_GRID_VIEW_CONTROL_H_INCLUDED

// nada mas
