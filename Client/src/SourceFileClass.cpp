// source file class

#include <cassert>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "wx/filename.h"

#include "TopFrame.h"
#include "ProjectBreakPoints.h"
#include "StyledSourceCtrl.h"

#include "ddt/FileSystem.h"

#include "TopNotebook.h"
#include "Controller.h"

#include "sigImp.h"
#include "logImp.h"

#include "SourceFileClass.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

// static
Bookmark	Bookmark::Decode(const string &snl)
{
	const size_t	sep = snl.find(":");
	assert(sep != string::npos);
		
	const string	shortname = snl.substr(0, sep);
	const int	ln = Soft_stoi(snl.substr(sep + 1), -1);
	assert(ln != -1);
	
	return {shortname, ln};
}

//---- Source File Class ------------------------------------------------------

class SourceFileClass : public ISourceFileClass
{
public:
	SourceFileClass(ITopNotePanel &parent, Controller &controller, const string &fullpath)
		: m_TopNotePanel(parent),
		m_Controller(controller),
		m_FullPath(fullpath),
		m_DirtyFlag(false),
		m_LockFlag(false),
		m_CursorPos(0, 0)
	{	
		assert(!fullpath.empty());
		
		wxFileName	cfn(wxString{fullpath});
		assert(cfn.FileExists());
		
		m_ShortName = cfn.GetFullName().ToStdString();
		
		const int	n_lines = Load(fullpath);
		assert(n_lines >= 0);
	}
	
	virtual ~SourceFileClass()
	{
		uLog(DTOR, "SourceFileClass::DTOR");
	}
	
	bool	DestroySFC(void) override
	{
		uLog(DTOR, "SourceFileClass::DestroySFC()");
		
		m_TopNotePanel.HideSFC(this, true/*delete associated wxWindow*/);
		
		return m_Controller.DeleteSFC(this);
	}
	
	IEditorCtrl&	GetEditor(void) override
	{
		return m_TopNotePanel.GetEditor();
	}
	
	void	ShowNotebookPage(const bool f) override
	{
		uLog(UI, "SourceFileClass::ShowNotebookPage(%c)", f);
	
		if (f)
		{	m_TopNotePanel.ShowSFC(this);
			
			// refresh?
			GetProperties();
		}
		else	m_TopNotePanel.HideSFC(this, false/*delete?*/);
	}
	
	bool	IsDirty(void) const override		{return m_DirtyFlag;}
	
	void	SetDirty(const bool f) override
	{
		// uWarn("SourceFileClass::SetDirty(%c)", f);
		
		m_DirtyFlag = f;
	}
	
	void	SetLockFlag(const bool f) override
	{
		m_LockFlag = f;
	}
	
	bool	IsWritableFile(void) const override
	{
		assert(wxFileName::FileExists(wxString{m_FullPath}));
		
		const bool	f = wxFileName::IsFileWritable(wxString{m_FullPath});
		return f;
	}
	
	void	SaveFromEditor(IEditorCtrl &ed) override
	{
		m_TextLines = ed.GetAllTextLines();
		m_DirtyFlag = ed.IsEditorDirty();
		m_CursorPos = ed.GetCursorPos();
		m_BreakpointLines = ed.GetBreakpointLines();
	}
	
	void	RestoreToEditor(IEditorCtrl &ed) override
	{
		// uWarn("SourceFileClass::RestoreToEditor()");
		
		ed.SetAllTextLines(m_TextLines);
		ed.SetEditorDirty(m_DirtyFlag);
		
		const vector<int>	bp_lines = m_Controller.GetSFCBreakpoints(*this);
		
		ed.SetBreakpointLines(bp_lines);
		// ed.SetBreakpointLines(m_BreakpointLines);
		ed.SetLockFlag(m_LockFlag);
		ed.SetCursorPos(m_CursorPos);		
	}
	
	bool	SaveToDisk(IEditorCtrl &ed) override
	{
		SaveFromEditor(ed);
		
		if (!IsDirty())		return true;
		if (!IsWritableFile())
		{
			uErr("error: file %S is not writable", m_FullPath);
			return false;
		}
		
		ofstream	ofs(m_FullPath, ios::trunc/*overwrite*/);
		if (ofs.fail() || ofs.bad())			return false;		// couldn't open error
		
		for (const string &ln : m_TextLines)
		{	
			ofs << ln << endl;
			if (ofs.fail() || !ofs.good())		return false;		// write error
		}
		
		m_DirtyFlag = false;
		
		// will notify of project update
		ed.SetEditorDirty(false);
		
		return true;
	}
	
	void	SetBreakpointLines(const vector<int> &bps) override
	{
		uLog(BREAKPOINT_EDIT, "SourceFileClass<%S>::SetBreakpointLines(%zu bps)", GetShortName(), bps.size());
	}
	
	void	OnClickedBreakpoint(const int ln) override
	{
		// ignores state from UI, uses only logic
		m_Controller.ToggleBreakpoint(*this, ln);
	}
	
	bool	OnReadOnlyEditAttempt(void) override;
	
	string	GetFullPath(void) const override	{return m_FullPath;}
	string	GetShortName(void) const override	{return m_ShortName;}

	string	GetRelativePath(const string &base_path) const override
	{
		wxFileName	cfn(m_FullPath);
		assert(cfn.IsOk() && cfn.FileExists());
			
		const wxString	x_base_path(wxString{base_path});
		assert(wxFileName::DirExists(x_base_path));
		
		cfn.MakeRelativeTo(x_base_path);
		assert(cfn.IsRelative());
			
		return cfn.GetFullPath().ToStdString();
	}	
	
	SFC_props	GetProperties(void) const override;
	
private:
	
	int	Load(const string &fullpath)
	{
		ifstream	ifs(fullpath, ios_base::in);
		if (ifs.fail() || ifs.bad())			return -1;		// couldn't open file error
		
		while (!ifs.eof())
		{	
			string	ln;
			
			std::getline(ifs, ln);
			// ifs >> ln;
			if (!ifs.eof() && ifs.fail())		return -1;		// read error
			
			m_TextLines.push_back(ln);
		}
		
		return m_TextLines.size();
	}
	
	ITopNotePanel	&m_TopNotePanel;	// (parent)
	Controller	&m_Controller;
	
	const string	m_FullPath;
	string		m_ShortName;			// should be CONST
	
	SFC_props	m_Props;
	
	vector<string>	m_TextLines;
	bool		m_DirtyFlag;
	bool		m_LockFlag;
	vector<int>	m_BreakpointLines;
	iPoint		m_CursorPos;
};
	
//---- Get Properties ---------------------------------------------------------

SFC_props	SourceFileClass::GetProperties(void) const
{
	wxFileName	cfn(wxString{m_FullPath});
	assert(cfn.IsOk() && cfn.FileExists());						// should use internal functions - FIXME
	
	SFC_props	res_map;
	
	res_map[SHORT_NAME] = m_ShortName;						// is ID key
	res_map[FILE_SIZE] =  to_string(cfn.GetSize().GetLo());
	res_map[MODIFY_DATE] = cfn.GetModificationTime().Format("%Y-%m-%d %H:%M:%S (%Z)").ToStdString();			// should use internal functions - FIXME (see above)
	// res_map[SOURCE_LINES] = to_string(m_EditorCtrl.GetStcPosition().TotalLines());	// GetTotalLines();
	res_map[SOURCE_LINES] = to_string(FileName::CountTextLines(m_FullPath));
	
	res_map[FULL_PATH] = m_FullPath;
	
	return res_map;
}

//---- On Read-Only Edit Attempt ----------------------------------------------

bool	SourceFileClass::OnReadOnlyEditAttempt(void)
{
	uLog(STYLED, "SourceFileClass::OnReadOnlyEditAttempt()");
	
	if (!IsWritableFile())
	{	const string	msg = xsprintf("File %S is Read-Only", m_ShortName);
		::wxMessageBox(wxString(msg), "WARNING", wxCENTER | wxOK | wxICON_EXCLAMATION, &m_Controller.GetTopFrame());		// ???
		return false;
	}
	
	// if became writable & Lua wasn't started, continue silently
	if (IsWritableFile() && !m_Controller.IsLuaStarted())		return true;
	
	const bool	edit_f = m_Controller.OnReadOnlyEditAttempt(m_ShortName);
	
	return edit_f;
}

//---- INSTANTIATE ------------------------------------------------------------

// static
ISourceFileClass*	ISourceFileClass::Create(ITopNotePanel &parent, Controller &controller, const string &fullpath)
{
	return new SourceFileClass(parent, controller, fullpath);
}

// nada mas