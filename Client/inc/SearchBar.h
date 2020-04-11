// ddt search bar

#pragma once

#include <string>
#include <vector>
#include <utility>		// for std::pair
#include <unordered_map>

#include "wx/wx.h"
#include "wx/textctrl.h"

#include "StyledSourceCtrl.h"

// forward references
class wxToggleButton;
class wxConfigBase;

namespace DDT_CLIENT
{

class TopFrame;
class Controller;
class ITopNotePanel;
class SearchResultsPanel;

//---- Search Bar class -------------------------------------------------------

class SearchBar: public wxPanel
{
	friend class ITopNotePanel;
	friend class SearchResultsPanel;
public:
	// ctor
	SearchBar(wxWindow *parent, int id, ITopNotePanel &top_note_panel, TopFrame &tf, Controller &controller);
	// dtor
	virtual ~SearchBar();
	
	// functions
	void	Reset(const bool &show_f = true, const std::string &initial_string = "");
	int 	GetSearchStatus(std::vector<SearchHit> &hits) const;				// returns current result index or -1
	void	CycleResultRelative(const int &delta);
	
	void	LoadUI(const wxConfigBase &config);
	void	SaveUI(wxConfigBase &config) const;
	
private:
	
	void	SetSearchResultsPanel(SearchResultsPanel *srp);
	
	void	ClearHits(void);
	void	ResetLastCycler(void);
	
	// events
	void	OnSearchCtrlCharENTERHook(wxKeyEvent &e);
	void	OnSearchCtrlChar(wxCommandEvent &e);
	void	OnToggleButton(wxCommandEvent &e);
	void	OnFunctionDropDownEvent(wxCommandEvent &e);
	
	void	OnKillFocusEvent(wxFocusEvent &e);
	void	OnReSearch_SYNTH(wxThreadEvent &e);
	// void	OnSelectedSFC_SYNTH(wxThreadEvent &e);
	
	void			DoSearch(void);
	std::vector<bool>	GetAllButtonStates(void) const;
	bool			GotoNthResult(const int &result_ind);
	
	ITopNotePanel		&m_TopNotePanel;
	TopFrame		&m_TopFrame;
	Controller		&m_Controller;
	wxTextCtrl		*m_SearchEditCtrl;
	
	SearchResultsPanel	*m_SearchResultsPanel;
	
	std::unordered_map<std::string, int>	m_PrefTagToID;
	std::vector<wxToggleButton*>		m_ToggleButtonList;
	
	std::vector<SearchHit>		m_HitList;
	int				m_HitCycler;
	
	DECLARE_EVENT_TABLE()
};

} // namespace DDT_CLIENT

// nada mas
