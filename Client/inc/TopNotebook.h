// Lua DDT top notebook

#pragma once

#include <string>
#include <vector>

// forward references
class wxWindow;

namespace DDT
{
	class Collected;

} // namespace DDT

namespace DDT_CLIENT
{
class TopFrame;
class Controller;
class SearchBar;
class BookSplitter;

class ISourceFileClass;
class IEditorCtrl;
class EditorSignals;
class LogicSignals;

enum MARGIN_MARKER_T : int;

using std::string;
using std::vector;

using DDT::Collected;

class ITopNotePanel
{
public:
	virtual ~ITopNotePanel() = default;
	
	virtual wxWindow*		GetWxWindow(void) = 0;
	virtual TopFrame&		GetTopFrame(void) = 0;
	
	virtual EditorSignals&		GetSignals(void) = 0;
	
	virtual	void	RegisterCodeNavigator(LogicSignals &signals) = 0;
	
	virtual ISourceFileClass*	GetEditSFC(void) const = 0;
	virtual void			SetEditSFC(ISourceFileClass *sfc) = 0;
	
	virtual void	ShowSFC(ISourceFileClass *sfc) = 0;
	virtual void	HideSFC(ISourceFileClass *sfc, bool delete_f) = 0;
	virtual void	HideAllSFCs(void) = 0;
	virtual void	UnhighlightSFCs(void) = 0;
	
	virtual IEditorCtrl&	GetEditor(void) = 0;
	
	virtual bool	CanQueryDaemon(void) const = 0;
	virtual bool	SolveVar_ASYNC(const string &key, wxString &res) = 0;		// returns [cached_f]
	virtual void	SetSolvedVariable(const Collected &ce) = 0;
	virtual bool	AddToWatches(const string &watch_name) = 0;

	virtual SearchBar&	GetSearchBar(void) = 0;
	virtual BookSplitter*	GetSplitterTop(void) = 0;
	
	virtual void	EscapeKeyCallback(void) = 0;
	
	virtual void	RequestStatusBarRefresh(void) = 0;
	
	static
	ITopNotePanel*	Create(wxWindow *parent, int id, TopFrame &tf, Controller &controller);
};

} // namespace DDT_CLIENT


// nada mas
