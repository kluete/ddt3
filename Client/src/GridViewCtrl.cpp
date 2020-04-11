// LX::Grid test controller

#include <string>
#include <vector>
#include <sstream>

#include "TopFrame.h"
#include "Controller.h"
#include "ProjectPrefs.h"
#include "BottomNotebook.h"
#include "UIConst.h"

#include "ddt/sharedDefs.h"
#include "ddt/Collected.h"

#include "ClientCore.h"

#include "GridViewCtrl.h"

#include "lx/renderer.h"
#include "lx/GridImp.h"

const int	DEMO_ICON_MARGIN = 4;

using namespace std;
using namespace LX;
using namespace DDT;

BEGIN_EVENT_TABLE(GridViewCtrl, wxWindow)

END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	GridViewCtrl::GridViewCtrl(BottomNotePanel *parent, int id, TopFrame *tf, Controller *controller, Renderer *cairo_renderer, wxImageList *img_list)
		: wxWindow(parent, id), PaneMixIn(parent, id, this)
{
	assert(parent);
	assert(tf);
	assert(controller);
	assert(cairo_renderer);
	assert(img_list);
	
	m_CairoRenderer = cairo_renderer;
	
	m_InitFlag = false;
	
	m_BottomNotePanel = parent;
	m_TopFrame = tf;
	m_Controller = controller;
	m_ImageList = img_list;
	
	m_GridPanel = new GridPanel(this, CONTROL_ID_GRID_IMP_PANEL, cairo_renderer, this, -1/*client id*/, "prims");
	
	m_StatusBar = new wxStatusBar(this, -1, wxSTB_SHOW_TIPS | wxFULL_REPAINT_ON_RESIZE);		// no size grip!
	
	wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_SWISS).Encoding(wxFONTENCODING_DEFAULT));
	m_StatusBar->SetFont(ft);
	m_StatusBar->SetFieldsCount(2);
	// EXPLICITLY set style on sub-fields on Gtk
	int	styles[2] = {0, 0};
	m_StatusBar->SetStatusStyles(2, styles);
	m_StatusBar->SetStatusText("dummy", 0);
	m_StatusBar->SetMinHeight(10);
	m_StatusBar->Show();
	
	wxSizer	*v_sizer = new wxBoxSizer(wxVERTICAL);
	
	v_sizer->Add(m_GridPanel, wxSizerFlags(1).Expand());
	v_sizer->Add(m_StatusBar, wxSizerFlags(0).Expand());
	
	SetSizer(v_sizer);
}

//---- DTOR -------------------------------------------------------------------

	GridViewCtrl::~GridViewCtrl()
{
	uLog(DTOR, "GridViewCtrl::DTOR()");
}

//---- Set CellData List ------------------------------------------------------

void	GridViewCtrl::SetCellDataList(const vector<CellData> &cell_data_list)
{
	if (!m_InitFlag)	return;			// ignore until initialized
		
	Grid	&grid = m_GridPanel->GetGrid();
	
	grid.SetCellDataList(cell_data_list);
	
	grid.FlushAll().RequestRefresh();
	
	/*
	grid.DeleteAllCells();
	
	for (int i = 0; i < cell_data_list.size(); i++)
	{
		const CellData	&cdata = cell_data_list[i];
		
		grid.SetCellData(cdata.row(), cdata.col(), cdata.GetText(), cdata.GetFontID(), cdata.GetIconID());
	}
	
	grid.FlushAll().RequestRefresh();
	*/
}

//---- On Grid Init IMPLEMENTATION --------------------------------------------

void	GridViewCtrl::OnGridClientInit(GridImp &grid)
{
	const int	client_id = grid.GetGridClientID();
	
	m_InitFlag = true;
	
	assert(grid.HasRenderContext());
	Context		&ctx = grid.GetRenderContext();
	assert(ctx.IsOK());
	
	Renderer	&rend = ctx.GetRenderer();
	assert(rend.IsOK());
	
	grid.DeleteAllColumns();
	
	grid.SetGlobalColumnFlags(COL_PLACEHOLDER_CELL_ALL);
	
	grid.SetHeaderFontID(VAR_UI_COLUMN_HEADER);
	grid.SetHeaderHeight(26);
	grid.SetRowHeight(-2);
	grid.SetDefaultFontID(VAR_UI_DEFAULT);
	
	grid.AddColumn("key");
	grid.AddColumn("value");
	grid.AddColumn("type").AlignRight();
	grid.AddColumn("scope");
	
	// grid.SetCellOverlayFlag(true);
	
	grid.FlushAll();
}

//---- Grid GetRowColor IMPLEMENTATION ----------------------------------------

Color	GridViewCtrl::GetRowColor(GridImp &grid, const size_t &row)
{
	return NO_COLOR;
}

//---- On Grid Event IMPLEMENTATION -------------------------------------------

void	GridViewCtrl::OnGridEvent(GridImp &grid, GridEvent &e)
{
	HitItem		hit = e.GetHitItem();
	const int	row = hit.IsHit() ? hit.row() : e.GetInt();
	const int	col = hit.col();
	const string	hit_type_s = hit.GetTypeString();
	const bool	hyperlink_f = hit.HasFont() && (grid.GetFontStyle(hit.GetFontID()) & FONT_STYLE_UNDERLINED);
	
	switch (e.Type())
	{
		case GRID_EVENT::CELL_HOVER:
		{
			if (hit.empty())
			{	m_StatusBar->SetStatusText("", 1);
				grid.SetCursor(wxCURSOR_IBEAM);
				return;
			}
			
			const string	msg = xsprintf("@%d:%d %s", row, col, hit_type_s);
			m_StatusBar->SetStatusText(wxString(msg), 1);
			grid.SetCursor(hyperlink_f ? wxCURSOR_HAND : wxCURSOR_ARROW);
		}	break;
		
		case GRID_EVENT::LEAVE_WINDOW:
		
			m_StatusBar->SetStatusText("", 0);
			m_StatusBar->SetStatusText("", 1);
			break;
		
		case GRID_EVENT::CELL_LEFT_CLICK:
		{
			// m_StatusBar-> "%s::OnCellLeftClick(%d, %d)", title, row, col);
			if (hyperlink_f)
			{	// TO DO
			
			}
			else	e.Skip();
		}	break;
		
		default:
		
			break;
	}
}

//---- Request Var Reload IMPLEMENTATIONS -------------------------------------

void	GridViewCtrl::RequestVarReload(void)
{
	assert(m_TopFrame);
}

//---- Clear View -------------------------------------------------------------

void	GridViewCtrl::ClearVariables(void)
{
	
}

// nada mas