// Lua DDT project preferences

#pragma once

#include "wx/filename.h"

#include "sigImp.h"

namespace DDT_CLIENT
{
class TopFrame;
class Controller;

//---- Project Preferences ----------------------------------------------------

class ProjectPrefs : private LX::SlotsMixin<LX::CTX_WX>
{
public:	
	ProjectPrefs(TopFrame &tf, Controller &controller);
	virtual ~ProjectPrefs();
	
	bool		IsLoaded(void) const;
	wxString	GetProjectName(void) const;
	bool		IsDirty(void) const;
	void		SetDirty(void);
	void		OnSavePrefsTimerLapsed(void);
	
	bool		LoadProject(const wxString &full_path);
	void		SaveProject(bool force_f = false);
	bool		PromptSaveNewProject(const wxString &prompt_s);
	void		CloseProject(void);	
	
private:
	
	void		UpdateFrameTitle(void);
	
	TopFrame	&m_TopFrame;
	Controller	&m_Controller;
	
	wxFileName	m_ProjectCfn;
	bool		m_DirtyFlag;
};

} // namespace DDT_CLIENT

// nada mas
