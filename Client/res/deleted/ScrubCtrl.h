// Lua DDT Scrub Control

#ifndef LUA_DDT_SCRUB_CONTROL_H_DEFINED
#define	LUA_DDT_SCRUB_CONTROL_H_DEFINED

#include <cstdint>
#include <vector>
#include <memory>		// shared_ptr<>

#include "wx/wx.h"
#include "wx/control.h"
#include "wx/timer.h"
#include "wx/graphics.h"

#include "UIConst.h"

// forward declarations
class wxStaticBitmap;

class TopFrame;
class Controller;
class TopNotePanel;

class SourceFileClass;

//---- Scrub Control ----------------------------------------------------------

class ScrubCtrl : public wxControl
{
public:
	// ctor
	ScrubCtrl(wxWindow *parent, int id, TopNotePanel *tnp, TopFrame *tf, Controller *controller);
	// dtor
	virtual ~ScrubCtrl();
	
	void	UpdateScrubBitmap(SourceFileClass *sfc, const std::vector<DDT::SearchHit> &hit_list);
	void	SetSearchResults(const std::vector<DDT::SearchHit> &hit_list);
	void	RequestRefresh(void);
	
private:

	void	OnResizeEvent(wxSizeEvent &e);
	void	OnMouseEvent(wxMouseEvent &e);
	void	OnPaintEvent(wxPaintEvent &e);
	
	void	ClearBitmap(void);
	
	TopNotePanel	*m_TopNotePanel;
	TopFrame	*m_TopFrame;
	Controller	*m_Controller;
	
	SourceFileClass	*m_OwnerSFC;
	wxColour	m_DefaultColor;
	wxBitmap	m_Bitmap;
	bool		m_RefreshRequestedFlag;
	
	wxDECLARE_EVENT_TABLE();
};

#endif // LUA_DDT_SCRUB_CONTROL_H_DEFINED

// nada mas
