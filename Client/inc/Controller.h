// Lua DDT Controller

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "ddt/sharedDefs.h"
#include "lx/juce/jfont.h"

#include "wx/wx.h"
#include "wx/dnd.h"
#include "wx/imaglist.h"

#include "sigImp.h"

namespace LX
{
	class Renderer;
	class ModalTextEdit;
	
} // namespace LX

namespace juce
{
	class LookAndFeel;
	
} // namespace juce

namespace DDT_CLIENT
{
// forward declarations
class IClientState;

class TopFrame;
class ITopNotePanel;
class IBottomNotePanel;
class ISourceFileClass;
class ProjectPrefs;

class Controller;		// (early use)

enum class INIT_LEVEL : int;

// import into namespace
using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::unique_ptr;
using std::shared_ptr;

using LX::Renderer;
using LX::ModalTextEdit;
using LX::IFixedFont;

using juce::LookAndFeel;

using DDT::StringList;

//---- Controller Drop Target -------------------------------------------------

class ControllerDropTarget : public wxFileDropTarget
{
public:
	ControllerDropTarget(Controller &controller);
	
	bool	OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &filenames) override;
	
private:
	
	Controller	&m_Controller;
};

//---- Controller -------------------------------------------------------------

class Controller : private LX::SlotsMixin<LX::CTX_WX> 
{	
public:	
	Controller(TopFrame &tf, IClientState &client_state, shared_ptr<LX::IFixedFont> ff, wxImageList &img_list, Renderer &cairo_renderer, ModalTextEdit *modal_tedit);
	virtual ~Controller();
	
	LogicSignals&	GetSignals(void)	{return m_Signals;}
	
	void		DestroyController(void);
	
	IClientState&	GetClientState(void);
	wxImageList&	GetImageList(void);
	Renderer&	GetCairoRenderer(void);
	ModalTextEdit*	GetModalTextEdit(void);
	
	void		SetTopNotePanel(ITopNotePanel *top_notebook_imp);
	void		SetBottomNotePanel(IBottomNotePanel *bottom_notepanel);
	void		SetProjectPrefs(ProjectPrefs *prefs);
	
	void		ResetProject(void);
	
	void		BroadcastProjectRefresh(void);
	void		ShowSourceAtLine(const string &shortname, const int ln, const bool focus_f);
	
	void		RemoveAllSourceHighlights(void);
	void		SetDirtyProject(void);
	void		DirtyUIPrefs(void);
	
	// edit lock/unlock
	void		SetAllSFCEditLockFlag(const bool f);
	bool		OnReadOnlyEditAttempt(const string &edited_src);
	bool		HasDirtySFCEdit(void) const;
	
	// drag & drop
	bool		OnDroppedFiles(const StringList &sl);
	
	// SFCs
	bool			HasDoppleganger(const string &shortpath) const;		// (could have done a normal LUT)
	ISourceFileClass*	NewSFC(const string &fullpath);
	bool			DeleteSFC(ISourceFileClass *sfc);
	void			DestroyAllSFCs(void);
	ISourceFileClass*	GetSFC(const string &shortpath) const;
	StringList		GetSFCPaths(const bool fullpaths_f) const;
	StringList		GetRelativeSFCPaths(const string &base_path) const;
	vector<ISourceFileClass*>	GetSFCInstances(void) const;
	
	// breakpoints
	void		SetLoadedBreakpoints(const StringList &bp_list);
	void		SetSourceBreakpoint(const string &fn, const int ln, const bool f);
	void		ToggleBreakpoint(ISourceFileClass &sfc, const int ln);
	StringList	GetSortedBreakpointList(const int ln_offset) const;
	vector<int>	GetSFCBreakpoints(const ISourceFileClass &sfc) const;
	int		DeleteSFCBreakpoints(ISourceFileClass &sfc);
	void		SetSFCBreakpoints(ISourceFileClass &sfc, const vector<int> &line_list);
	
	// local file bookmarks
	void			SetLocalPathBookmarkList(const StringList &bookmark_list);	// called from UI load
	StringList		GetLocalPathBookmarkList(void) const;
	void			UpdateLocalPathBookmark(const string &bookmark);		// called from local file browser
	void			ClearLocalPathBookmarks(void);					// called from navigation bar
	
	// init level
	INIT_LEVEL		GetInitLevel(void) const;
	string			GetInitLevelString(void) const;
	void			SetInitLevel(const INIT_LEVEL &lvl);
	bool			IsClientActive(void) const;
	bool			IsLuaVMResumable(void) const;
	bool			IsLuaStarted(void) const;
	bool			IsLuaListening(void) const;
	
	void			StepDaemonTick(void);
	int			GetDaemonTick(void) const;
	
	void			LoadProjectDone(void);
	
	TopFrame&		GetTopFrame(void)	{return m_TopFrame;}
	
	shared_ptr<IFixedFont>	GetFixedFont(void)	{return m_FixedFont;}
	
	LookAndFeel*		CreateLookNFeel(void);
	
private:
	
	void			SortSFCs(const bool up_f);
	void			OnEditorBreakpointClick(ISourceFileClass *sfc, int ln);
	void			OnBreakpointUpdated(ISourceFileClass &sfc);
	
	string			SanitizeShortName(const string &fn) const;
	void			SetLoadedBreakpoints_LL(const StringList &bp_list);
	
	bool			HasBreakpoint_LL(ISourceFileClass &sfc, const int ln) const;
	void			SetBreakpoint_LL(ISourceFileClass &sfc, const int ln, const bool f);
	void			AddSFCBreakpoints(ISourceFileClass &sfc, const vector<int> &line_list);
	
	TopFrame		&m_TopFrame;
	IClientState		&m_ClientState;
	shared_ptr<IFixedFont>	m_FixedFont;
	wxImageList		&m_ImageList;
	Renderer		&m_CairoRenderer;
	ModalTextEdit		*m_ModalTextEdit;
	unique_ptr<LookAndFeel>	m_MyLookNFeel;
	
	ITopNotePanel		*m_TopNotePanel;
	IBottomNotePanel	*m_BottomNotePanel;
	
	ProjectPrefs		*m_ProjectPrefs;
	
	vector<string>					m_SourceFileOrder;
	unordered_map<string, ISourceFileClass*>	m_SourceFileMap;		// keyed on short name
	unordered_map<string, string>			m_ShortToLongMap;
	
	unordered_set<string>	m_BreakpointSet;
	
	StringList		m_LocalPathBookmarks;
	
	INIT_LEVEL		m_InitLevel;
	int			m_DaemonTick;
	
	LogicSignals		m_Signals;
};

} // namespace DDT_CLIENT

// nada mas