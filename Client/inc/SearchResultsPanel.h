// search results panel

#ifndef LUA_DDT_SEARCH_RESULTS_CONTROL_H_INCLUDED
#define	LUA_DDT_SEARCH_RESULTS_CONTROL_H_INCLUDED

#include <cstdint>
#include <vector>
#include <unordered_map>

#include "wx/wx.h"
#include "wx/panel.h"

#include "ddt/sharedDefs.h"

#include "BottomNotebook.h"
#include "UIConst.h"

// forward references
class wxTextCtrl;

namespace DDT_CLIENT
{

class IBottomNotePanel;
class SearchBar;

using std::vector;

//---- Search Results Panel ---------------------------------------------------

class SearchResultsPanel: public wxPanel, public PaneMixIn, private LX::SlotsMixin<LX::CTX_WX>
{
public:
	// ctor
	SearchResultsPanel(IBottomNotePanel &parent, int id, SearchBar &search_bar);
	// dtor
	virtual ~SearchResultsPanel();
	
	void	ClearResults(void);
	void	SetResults(const vector<SearchHit> &hit_list);
	
private:

	void	OnLeftDownEvent(wxMouseEvent &e);
	void	OnLeftDoubleClickEvent(wxMouseEvent &e);
	
	SearchBar	&m_SearchBar;
	
	wxTextCtrl	m_TextCtrl;
	wxTextAttr	m_DefaultAttr, m_UriAttr, m_HighlightAttr;
	
	vector<int>	m_UriLenLUT;
	
	DECLARE_EVENT_TABLE()
};

} // namespace DDT_CLIENT

#endif // LUA_DDT_SEARCH_RESULTS_CONTROL_H_INCLUDED


// nada mas


