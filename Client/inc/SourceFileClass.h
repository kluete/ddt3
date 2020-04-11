// source file class

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace DDT_CLIENT
{
// forward references
class Controller;
class IEditorCtrl;
class ITopNotePanel;

// import into namespace
using std::string;
using std::vector;
using std::unordered_map;

enum SFC_PROP : int
{
	SHORT_NAME = 0,
	FILE_SIZE,
	MODIFY_DATE,
	SOURCE_LINES,
	FULL_PATH
};

using SFC_props = unordered_map<SFC_PROP, string>;		// BULLSHIT

//---- Bookmark (user or function) --------------------------------------------

class Bookmark
{
public:
	Bookmark(const string &name, const int ln)
		: m_Name(name), m_Line(ln)
	{}
	
	const string	m_Name;
	const int	m_Line;
	
	static
	Bookmark	Decode(const string &snl);
};

//---- SourceFile interface ---------------------------------------------------

class ISourceFileClass
{
public:
	virtual ~ISourceFileClass() = default;
	
	virtual bool	DestroySFC(void) = 0;
	virtual void	ShowNotebookPage(const bool f) = 0;
	
	virtual IEditorCtrl&	GetEditor(void) = 0;
	
	virtual bool	IsDirty(void) const = 0;
	virtual void	SetDirty(const bool dirty_f) = 0;
	
	virtual void	SetLockFlag(bool lock_f) = 0;
	
	virtual void	SaveFromEditor(IEditorCtrl &ed) = 0;
	virtual void	RestoreToEditor(IEditorCtrl &ed) = 0;
	
	virtual bool	IsWritableFile(void) const = 0;
	virtual bool	SaveToDisk(IEditorCtrl &ed) = 0;

	virtual void	SetBreakpointLines(const vector<int> &bps) = 0;
	
	virtual void	OnClickedBreakpoint(const int ln) = 0;
	virtual bool	OnReadOnlyEditAttempt(void) = 0;
	
	virtual string	GetFullPath(void) const = 0;
	virtual string	GetRelativePath(const string &base_path) const = 0;
	virtual string	GetShortName(void) const = 0;
	
	virtual SFC_props	GetProperties(void) const = 0;
	
	static
	ISourceFileClass*	Create(ITopNotePanel &parent, Controller &controller, const string &fullpath);
};

} // namespace DDT_CLIENT

// nada mas