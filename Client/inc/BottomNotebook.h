// Lua DDT bottom notebook

#pragma once

#include <vector>
#include <unordered_map>

#include "wx/panel.h"
#include "wx/notebook.h"
#include "wx/splitter.h"

#include "sigImp.h"

#include "UIConst.h"

// forward references
class wxConfigBase;
class wxStaticBitmap;

namespace DDT_CLIENT
{

class TopFrame;
class Controller;
class ITopNotePanel;
class IBottomNotePanel;
class BottomNotePanel;

using namespace LX;

using std::string;
using std::vector;
using std::unordered_map;

enum PANE_ID : int;
class PageInfo;

//---- Pane mix-in Interface --------------------------------------------------

class IPaneMixIn
{
public:
	virtual ~IPaneMixIn() = default;
	
	virtual	bool		IsPaneShown(void) const = 0;					// (only purpose is to override wx bug... for VarViewCtrl::IsGridShown()...)
	virtual wxWindow*	GetWxWindow(void) = 0;
	virtual PANE_ID		GetPageID_LL(void) const = 0;
	
	static	IPaneMixIn*	CreatePlaceholderPanel(IBottomNotePanel &bottom_book, int id, ITopNotePanel &top_book);
	
	static	IPaneMixIn*	CreateProjectListCtrl(IBottomNotePanel &bottom_book, int id, Controller &controller);
	static	IPaneMixIn*	CreateBreakpointListCtrl(IBottomNotePanel &bottom_book, int id, Controller &controller);
	static	IPaneMixIn*	CreateTraceBackListCtrl(IBottomNotePanel &bottom_book, int id, TopFrame &tf, Controller &controller);
	static	IPaneMixIn*	CreateFunctionListCtrl(IBottomNotePanel &bottom_book, int id, ITopNotePanel &top_book);
	static	IPaneMixIn*	CreateMemoryViewCtrl(IBottomNotePanel &bottom_book, int id, ITopNotePanel &top_book);
};

//---- Pane MixIn (imp) -------------------------------------------------------

class PaneMixIn : public IPaneMixIn
{
public:

	PaneMixIn(IBottomNotePanel &bnp, const int win_id, wxWindow *win);
	virtual ~PaneMixIn();
	
	bool		IsPaneShown(void) const override;						// move to IMP / make in factory ?
	wxWindow*	GetWxWindow(void) override		{return m_ClientWin;}
	PANE_ID		GetPageID_LL(void) const override	{return m_PageID;}
	
private:
	
	IBottomNotePanel	&m_Parent;
	wxWindow		*m_ClientWin;
	const PANE_ID		m_PageID;
};

//---- Notepanel interface ----------------------------------------------------

class IBottomNotePanel
{
public:	
	virtual ~IBottomNotePanel() = default;
	
	virtual wxWindow*	GetWxWindow(void) = 0;
	
	virtual NotepageSignals&	GetSignals(const PANE_ID page_id) = 0;
	virtual Controller&		GetController(void) = 0;
	
	// move to IMP?
	virtual PANE_ID	RegisterPage(const int win_id, IPaneMixIn *page_mixin) = 0;		// returns [page_id]
	virtual void	UnregisterPage(IPaneMixIn *page_mixin) = 0;
	
	virtual void	DestroyDanglingPanes(void) = 0;
	virtual void	LoadUI(const wxConfigBase &config) = 0;
	virtual bool	SaveUI(wxConfigBase &config) = 0;
	
	virtual void	ClearVarViews(void) = 0;
	virtual bool	RaisePageID(const PANE_ID page_id, const bool reload_f) = 0;
	virtual void	UpdateVisiblePages(void) = 0;
	
	virtual bool	IsNotePaneShown(const PANE_ID page_id) const = 0;
	virtual void	ResetNotesPanes(void) = 0;	

	virtual const PageInfo&	GetPageInfo(const PANE_ID id) const = 0;
	virtual PageInfo&	GetPageInfoRW(const PANE_ID id) = 0;
	
	static
	IBottomNotePanel*	Create(wxWindow *parent, int id, ITopNotePanel &top_notepanel, TopFrame &tf, Controller &controller);
	
	static
	PANE_ID			WinToPaneID(const int win_id);
};

//---- Pane Component interface -----------------------------------------------

class IPaneComponent
{
public:
	virtual ~IPaneComponent() = default;
	
	virtual void	BindToPanel(IPaneMixIn &panel, const size_t retry_ms) = 0;
	
	static	IPaneComponent*	CreateFunctionsComponent(IBottomNotePanel &bottom_book, ITopNotePanel &top_book);
	static	IPaneComponent*	CreateVarViewComponent(IBottomNotePanel &bottom_book, const int win_id, ITopNotePanel &top_book);
};

//---- Book Splitter ----------------------------------------------------------

class BookSplitter : public wxSplitterWindow
{
public:
	BookSplitter(wxWindow *parent, int id, int index);
	virtual ~BookSplitter();

	void	Split(wxWindow *w1, wxWindow *w2, int w, const bool swap_f = false);
	int	GetPos(void) const;
	int	GetFullWidth(void) const;
	
	static
	vector<wxWindow*>	s_WindowList;				// static ???
};

} // namespace DDT_CLIENT

// nada mas

