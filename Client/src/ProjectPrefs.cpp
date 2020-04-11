// Lua DDT project preferences

#include <cassert>
#include <vector>

#include "wx/wfstream.h"

#include "lx/stream/MemDataInputStream.h"
#include "lx/stream/MemDataOutputStream.h"

#include "TopFrame.h"
#include "Controller.h"

#include "ClientState.h"

#include "logImp.h"

#include "WatchBag.h"

#include "ProjectPrefs.h"

#include "lx/misc/autolock.h"

#include "lx/ulog.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

//---- CTOR -------------------------------------------------------------------

	ProjectPrefs::ProjectPrefs(TopFrame &tf, Controller &controller)
		: m_TopFrame(tf), m_Controller(controller)
{
	controller.SetProjectPrefs(this);
	
	m_ProjectCfn.Clear();
	m_DirtyFlag = false;
	
	// international clusterfuck
	uLog(APP_INIT, "locale name: %s", wxLocale::GetSystemEncodingName());
}

//---- DTOR -------------------------------------------------------------------

	ProjectPrefs::~ProjectPrefs()
{
	uLog(DTOR, "ProjectPrefs::DTOR");
}

//---- Has Dirty Prefs ? ------------------------------------------------------

bool	ProjectPrefs::IsDirty(void) const
{
	return m_DirtyFlag;
}

//---- Flag as Dirty ----------------------------------------------------------

void	ProjectPrefs::SetDirty(void)
{
	// refresh UI IMMEDIATELY, or save timer will make it look sluggish
	m_Controller.BroadcastProjectRefresh();
	
	if (m_DirtyFlag)	return;		// already dirty
	
	uLog(PREFS, "ProjectPrefs::SetDirty()");
	
	m_DirtyFlag = true;
	
	MixDelay("proj_prefs", SAVE_PROJECT_PREFS_DELAY_MS, &m_TopFrame, &TopFrame::OnSaveProjectPrefs);
}

//---- On Save Prefs Timer Lapsed ---------------------------------------------

void	ProjectPrefs::OnSavePrefsTimerLapsed(void)
{
	if (!IsLoaded())
	{	// no loaded project, just change frame title
		UpdateFrameTitle();
	}
	else
	{	SaveProject(false/*only if dirty*/);
	}
}

//---- Update Frame Title -----------------------------------------------------

void	ProjectPrefs::UpdateFrameTitle(void)
{
	const wxString	frame_title = wxString(TOP_FRAME_TITLE) + " - \"" + GetProjectName() + "\"" + (IsDirty() ? "*" : "");
	
	m_TopFrame.SetTitle(frame_title);
	m_TopFrame.UpdateToolbarState();
}

//---- Is Loaded ? ------------------------------------------------------------

bool	ProjectPrefs::IsLoaded(void) const
{
	return m_ProjectCfn.IsOk();
}

//---- Get Project Name -------------------------------------------------------

wxString	ProjectPrefs::GetProjectName(void) const
{
	const wxString	proj_name = m_ProjectCfn.IsOk() ? m_ProjectCfn.GetFullName() : wxString("<untitled>");
	
	return proj_name;
}

//---- Prompt Save New Project -------------------------------------------------

bool	ProjectPrefs::PromptSaveNewProject(const wxString &prompt_s)
{	
	wxString	new_projpath = ::wxFileSelector(prompt_s, ""/*dir*/, ""/*name*/, "ddt", "ddt files (*.ddt)|*.ddt|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT, &m_TopFrame);
	if (new_projpath.IsEmpty())	return false;
	
	wxFileName	cfn(new_projpath);
	assert(cfn.IsOk());
	
	// reassign extension if user hadn't
	cfn.SetExt("ddt");
	
	m_ProjectCfn = cfn;
	
	return true;
}	

//---- Close Project ----------------------------------------------------------

void	ProjectPrefs::CloseProject(void)
{
	uLog(PREFS, "ProjectPrefs::CloseProject()");
	
	m_ProjectCfn.Clear();
	
	m_Controller.ResetProject();
	
	m_TopFrame.m_WatchBag->Clear();
	// (doesn't trigger event update)
	m_TopFrame.m_DaemonHostNPortComboBox->ChangeValue(wxString(DEFAULT_HOSTNAME_AND_PORT_STRING));
	m_TopFrame.m_LuaStartupComboBox->ChangeValue(wxString(DEFAULT_LUA_STARTUP_STRING));
	
	m_DirtyFlag = false;
	UpdateFrameTitle();
	
	// broadcast refresh
	m_Controller.BroadcastProjectRefresh();
}

//---- Load Project -----------------------------------------------------------

bool	ProjectPrefs::LoadProject(const wxString &new_projpath)
{
	uLog(PREFS, "ProjectPrefs::LoadProject(%s)", new_projpath);
	assert(!new_projpath.IsEmpty());
	
	wxFileName	proj_cfn(new_projpath);
	assert(proj_cfn.IsOk());
	CloseProject();
	
	m_ProjectCfn = proj_cfn;
	
	// block save-during-load
	AutoLock	lock(m_DirtyFlag);
	
	wxFileInputStream	fis(new_projpath);
	assert(fis.IsOk());
	
	const wxString	base_path = proj_cfn.GetPathWithSep();	
	
	IClientState	&client_state = m_Controller.GetClientState();
	
// load all to RAM
	int	sz = fis.GetSize();
	
	vector<uint8_t>	buff(sz, 0);
	
	fis.ReadAll(&buff[0], sz);
	
	MemDataInputStream	mis(buff);
	
// signature

	const string	sig { mis.getc(), mis.getc(), mis.getc(), mis.getc() };		// (don't add zero char)
	assert(sig == "ddt3");
	
// version
	const int	pos = mis.Tell();
	uint32_t	version = 0;
	
	const string	version_tag = mis.ReadString();
	if (version_tag != "version")
	{	// rewind
		mis.Seek(pos, SEEK_T::FROM_START);
	}
	else
	{	version = mis.Read32();
		
		if (version > PREFS_VERSION)
		{	uErr("incompatible forward version 0x%08x", version);
			return false;
		}
	}
	
// list of [rel_paths]
	if (mis.ReadString() != "sources")			return false;	// bad tag
	
	StringList	sl(mis);
	
	uLog(PREFS, " loading %zu sources", sl.size());
	
	for (int i = 0; i < sl.size(); i ++)
	{
		const wxString	rel_path = wxString(sl[i]);
		assert(!rel_path.empty());
		
		wxFileName	cfn(rel_path);
		if (!cfn.IsAbsolute())
		{
			cfn.MakeAbsolute(base_path);
			assert(cfn.IsAbsolute());
		}
		
		const string	full_path = cfn.GetFullPath().ToStdString();
		
		uLog(PREFS, "  [%d] src %S -> %S", i, rel_path, full_path);
		
		if (!cfn.FileExists())
		{
			uErr("    error - file %S doesn't exist", full_path);
			continue;		// skip missing
		}

		ISourceFileClass	*sfc = m_Controller.NewSFC(full_path);
		assert(sfc);
	}
	
// misc remaining prefs
	while (mis.IsOk())
	{	const string	k = mis.ReadString();
		assert(!k.empty());
		
		if (k == "breakpoints")			m_Controller.SetLoadedBreakpoints(StringList{ mis });
		else if (k == "watches")		m_TopFrame.m_WatchBag->Set(mis);
		else if (k == "VarViewMask")		m_TopFrame.m_VarViewPrefs->SetMask(mis.Read32());
		else if (k == "step_lines_flag")	client_state.SetStepModeLinesFlag(mis.ReadBool());
		else if (k == "load_break_flag")	client_state.SetLoadBreakFlag(mis.ReadBool());
		else if (k == "network")		m_TopFrame.m_DaemonHostNPortComboBox->SetValue(mis.ReadString());
		else if (k == "lua_startup")		m_TopFrame.m_LuaStartupComboBox->SetValue(mis.ReadString());
		else uErr("illegal pref entry %S", wxString(k));
		
		client_state.SetJitFlag(false);		// not currently stored
	}
	
	m_DirtyFlag = false;
	
	m_Controller.LoadProjectDone();
	
	// broadcast refresh
	m_Controller.BroadcastProjectRefresh();
	
	UpdateFrameTitle();
	
	return true;
}

//---- Save Project -----------------------------------------------------------

void	ProjectPrefs::SaveProject(bool force_f)
{
	if (!m_DirtyFlag && !force_f)	return;
	
	if (!m_ProjectCfn.IsOk())
	{	// prompt to save prefs
		bool	ok = PromptSaveNewProject("Save New Project");
		if (!ok)	return;
	}
	
	IClientState	&client_state = m_Controller.GetClientState();

	const string	base_path = m_ProjectCfn.GetPathWithSep().ToStdString();
	
	MemDataOutputStream	prefs;
	
	// signature
	uint32_t	sig = ('d' << 24) | ('d' << 16) | ('t' << 8) | ('3') ;
	prefs << sig;
	
	prefs << "version" << PREFS_VERSION;
	
	prefs << "sources" << m_Controller.GetRelativeSFCPaths(base_path);
	prefs << "breakpoints" << m_Controller.GetSortedBreakpointList(0);
	prefs << "watches" << m_TopFrame.m_WatchBag->Get();
	prefs << "VarViewMask" << m_TopFrame.m_VarViewPrefs->GetMask();
	prefs << "step_lines_flag" << client_state.GetStepModeLinesFlag();
	prefs << "load_break_flag" << client_state.GetLoadBreakFlag();
	prefs << "network" << m_TopFrame.m_DaemonHostNPortComboBox->GetValue().ToStdString();
	prefs << "lua_startup" << m_TopFrame.m_LuaStartupComboBox->GetValue().ToStdString();
	
	// save to disk
	const size_t	sz = prefs.GetLength();
	
	vector<uint8_t>	buff(sz, 0);
	
	prefs.CloneTo(&buff[0], sz);
	
	wxFileOutputStream	fos(m_ProjectCfn.GetFullPath());
	assert(fos.IsOk());
	
	fos.WriteAll(&buff[0], sz);
	
	fos.Close();
	
	m_DirtyFlag = false;
	
	UpdateFrameTitle();
	
	// add to MRU menu
	m_TopFrame.AddProjectMRUEntry(m_ProjectCfn.GetFullPath().ToStdString());
}

// nada mas
