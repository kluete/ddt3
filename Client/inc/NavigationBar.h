// navigation icon bar

#ifndef LUA_DDT_NAVIGATION_ICON_BAR_H_INCLUDED
#define	LUA_DDT_NAVIGATION_ICON_BAR_H_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>

#include "wx/toolbar.h"

#ifndef nil
	#define	nil	nullptr
#endif // nil

namespace DDT_CLIENT
{

using NavIDtoBitmap = std::unordered_map<int, wxBitmap>;

// forward declarations
class TopFrame;
class Controller;
class wxEvtHandler;

//---- Navigation Bar ---------------------------------------------------------

class NavigationBar: public wxToolBar
{
public:
	// ctor
	NavigationBar(wxWindow *parent, int id, TopFrame *tf, Controller *controller, wxEvtHandler *evt_hdl);
	
	void	RequestUpdateBookmarks(void);
	
private:
	
	void	OnClearMRUMenuEvent(wxCommandEvent &e);
	void	OnToolEventForward(wxCommandEvent &e);
	void	DoUpdateMRUList(void);
	
	TopFrame	*m_TopFrame;
	Controller	*m_Controller;
	wxEvtHandler	*m_EvtRecipient;
	NavIDtoBitmap	m_NavIDtoBitmapLUT;
	
	DECLARE_EVENT_TABLE()
};

} // namespace DDT_CLIENT

#endif // LUA_DDT_NAVIGATION_ICON_BAR_H_INCLUDED

// nada mas
