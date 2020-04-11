// DDT icon toolbar

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

#include "wx/mstream.h"
#include "wx/wfstream.h"
#include "wx/txtstrm.h"
#include "wx/filename.h"
#include "wx/datstrm.h"
#include "wx/fileconf.h"
#include "wx/splitter.h"
#include "wx/display.h"

#include "TopNotebook.h"
#include "SearchBar.h"
#include "OutputTextCtrl.h"

#include "TopFrame.h"
#include "Client.h"
#include "ProjectPrefs.h"

#include "ClientState.h"

#include "Controller.h"
#include "UIConst.h"

#include "logImp.h"

#include "lx/misc/autolock.h"

#include "ddt/FileSystem.h"

#include "lx/renderer.h"
#include "lx/ModalTextEdit.h"

#include "lx/wx/geometry.h"

// icons as raw C serialized
#include "empty.c_raw"
#include "cancel.c_raw"
#include "play.c_raw"
#include "red_dot.c_raw"
#include "save.c_raw"
#include "step_into.c_raw"
#include "step_out.c_raw"
#include "step_over.c_raw"
#include "stop.c_raw"
#include "step_lines.c_raw"
#include "step_VM.c_raw"
#include "minus.c_raw"
#include "plus.c_raw"
#include "fileopen.xpm"
#include "close.c_raw"
#include "luajit.c_raw"
#include "load_break.c_raw"
#include "load_continue.c_raw"
#include "quit.xpm"
#include "connect.c_raw"
#include "disconnect.c_raw"
#include "arrow_right.c_raw"
#include "arrow_down.c_raw"
#include "sync.c_raw"
#include "flash.c_raw"
#include "green_led.xpm"
#include "red_led.xpm"
#include "ubuntu_throbber.c_raw"

#include "log_frame_icon.c_raw"

// search icons
#include "find.c_raw"
#include "up.c_raw"
#include "down.c_raw"
#include "hl_matched_16.c_raw"
#include "reg_ex_16.c_raw"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

static const
unordered_map<int, string>	s_WinIDtoLabel
{
	{0,						"* ZERO WINDOW ID *"},
	
	{PANEL_ID_OUTPUT_TEXT_CTRL,			"OutputLog"},
	{PANEL_ID_PROJECT_LIST,				"ProjectListCtrl"},
	{PANEL_ID_LOCALS_LIST_CONTROL,			"LocalsPanel"},
	{PANEL_ID_GLOBALS_LIST_CONTROL,			"GlobalsPanel"},
	{PANEL_ID_WATCH_LIST_CONTROL,			"WatchesPanel"},
	{PANEL_ID_TRACE_BACK_LIST_CONTROL,		"StackPanel"},
	{PANEL_ID_BREAKPOINTS_LIST_CONTROL,		"BreakpointsPanel"},
	{PANEL_ID_MEMORY_VIEW_CONTROL,			"MemoryViewPanel"},
	{PANEL_ID_SEARCH_RESULTS,			"SearchPanel"},
	{PANEL_ID_LUA_FUNCTIONS_CONTROL,		"LuaFunctionsPanel"},
	{PANEL_ID_JUCE_LOCALS_LIST_CONTROL,		"jLocalsPanel"},
	
	// {BROADCAST_ID_PAGE_ID_EMERGED,			"page ID emerged"},
	{TOGGLE_ID_FIRST,				"toggle first"},
	{TOGGLE_ID_LAST,				"toggle last"},
	{BUTTON_ID_BASE,				"button id base"},
	
	{FRAME_ID_TOP_FRAME,				"topframe"},
	{CONTROL_ID_TOP_NOTEPANEL,			"top notepanel"},
	{CONTROL_ID_TOP_NOTEBOOK_IMP,			"top notebook imp"},
	{CONTROL_ID_BOTTOM_NOTE_PANEL,			"bottom notepanel"},
	{STATUSBAR_ID_TOOLBAR,				"toolbar statusbar"},
	
	{SPLITTER_ID_MAIN,				"spliter main"},	
	{SPLITTER_ID_TOP,				"splitter top"},
	{SPLITTER_ID_BOTTOM,				"splitter bottom"},
	
	{CONTROL_ID_PANE_BOOK_BASE,			"panebook 0"},
	{CONTROL_ID_PANE_BOOK_BASE + 1,			"panebook 1"},
	{CONTROL_ID_PANE_BOOK_BASE + 2,			"panebook 2"},
	
	{CONTROL_ID_BOTTOM_NOTEBOOK_BASE,		"bottom notebook 0"},
	{CONTROL_ID_BOTTOM_NOTEBOOK_BASE + 1,		"bottom notebook 1"},
	{CONTROL_ID_BOTTOM_NOTEBOOK_BASE + 2,		"bottom notebook 2"},
	
	{FRAME_ID_SOLO_LOG,				"solo log frame"},
	
	{MAIN_TOOLBAR_ID,				"MAIN_TOOLBAR_ID"},
	{COMBOBOX_ID_DAEMON_HOST_N_PORT,		"COMBOBOX_ID_DAEMON_HOST_N_PORT"},
	{COMBOBOX_ID_LUA_MAIN_SOURCE_N_FUNCTION,	"COMBOBOX_ID_LUA_MAIN_SOURCE_N_FUNCTION"},
	
	{CONTROL_ID_PANEBOOK_STATIC_BITMAP,		"CONTROL_ID_PANEBOOK_STATIC_BITMAP"},
	
	{CONTROL_ID_EDITOR_STATUSBAR,			"CONTROL_ID_EDITOR_STATUSBAR"},
	{CONTROL_ID_SEARCH_PANEL,			"CONTROL_ID_SEARCH_PANEL"},
	{CONTROL_ID_SEARCH_EDIT_CONTROL,		"CONTROL_ID_SEARCH_EDIT_CONTROL"},
	{CONTROL_ID_SCRUBBER,				"CONTROL_ID_SCRUBBER"},
	{CONTROL_ID_SCRUBBER_STATIC_BITMAP,		"CONTROL_ID_SCRUBBER_STATIC_BITMAP"},
	{CONTROL_ID_MEMORY_VAR_NAME,			"CONTROL_ID_MEMORY_VAR_NAME"},
	{BUTTON_ID_MEMORY_REFRESH,			"BUTTON_ID_MEMORY_REFRESH"},
	{RADIO_ID_MEMORY_VIEW_HEX,			"RADIO_ID_MEMORY_VIEW_HEX"},
	{RADIO_ID_MEMORY_VIEW_ASCII,			"RADIO_ID_MEMORY_VIEW_ASCII"},
	{CONTROL_ID_MEMORY_DUMP_TEXT,			"CONTROL_ID_MEMORY_DUMP_TEXT"},
	{FRAME_ID_OVERLAY,				"FRAME_ID_OVERLAY"},
	{LISTCTRL_ID_VAR_VIEW,				"LISTCTRL_ID_VAR_VIEW"},
	{TEXTCTRL_ID_LUA_OUTPUT,			"TEXTCTRL_ID_LUA_OUTPUT"},
	{EDITCTRL_ID_LUA_COMMAND,			"EDITCTRL_ID_LUA_COMMAND"},
	{BUTTON_ID_SEND_LUA_COMMAND,			"BUTTON_ID_SEND_LUA_COMMAND"},
	
	// could use a #DEFINE inside UIConst.h and unroll it here !!!
};

// static
bool	TopFrame::IsKnownID(const int &win_id)
{
	return (s_WinIDtoLabel.count(win_id) > 0);
}

// static
string	TopFrame::GetWinIDName(const int &win_id)
{
	if (s_WinIDtoLabel.count(win_id))
		return s_WinIDtoLabel.at(win_id) + string(" (") + to_string(win_id) + ")";
	else	return string("(") + to_string(win_id) + ")";
}

//---- Log Sorted WinID Map ---------------------------------------------------

// static
void	TopFrame::LogSortedWinIDMap(void)
{
	// dump known IDs
	struct imap
	{	int		id;
		wxString	name;
	};
	
	vector<imap>	ilist;
	
	for (const auto &it : s_WinIDtoLabel)		ilist.push_back({it.first, wxString(it.second)});
	
	sort(ilist.begin(), ilist.end(), [](const imap &m1, const imap &m2){return (m1.id < m2.id);});
	
	for (const auto &m : ilist)	uMsg(" id %4d = %s", m.id, m.name);
	
	uMsg("");
}

//---- Load CRAW --------------------------------------------------------------

wxBitmap	TopFrame::LoadCraw(const uint8_t *dataptr, const size_t &len, const wxBitmapType &typ)
{
	// make sure image handlers were setup before
	wxMemoryInputStream	mis(dataptr, len);
	assert(mis.IsOk());
	
	wxImage	img(mis, typ);
	assert(img.IsOk());
	
	wxBitmap	bm(img);
	assert(bm.IsOk());
	
	return bm;
}

//---- Load CRAW Buffer -------------------------------------------------------

static
vector<uint8_t>	LoadCrawBuff(const uint8_t *dataptr, const size_t &len)
{
	vector<uint8_t>	buff (dataptr, dataptr + len);
	
	return buff;
}

//---- Bitmap to C-RAW serializer ---------------------------------------------

bool	TopFrame::SerializeOneBitmapFile(const wxString &src_fn, const wxString &dest_fn)
{	
	const wxString	cwd = wxFileName::GetCwd();
	
	// input
	wxFileName	cfn(src_fn);
	assert(cfn.IsOk());
	cfn.Normalize();
	assert(cfn.FileExists());
	
	wxFileInputStream	fis(cfn.GetFullPath());
	wxASSERT_MSG(fis.IsOk(), wxString::Format("SerializePNG() couldn't load file %S", src_fn));
	
	struct bmInfo
	{
		string	m_TypeStr;
		int	m_Depth;
	};
	
	const
	unordered_map<wxBitmapType, bmInfo, hash<int>>	bmTypeToStr =	{	{wxBITMAP_TYPE_PNG,	bmInfo{"wxBITMAP_TYPE_PNG",	32}},
										{wxBITMAP_TYPE_GIF,	bmInfo{"wxBITMAP_TYPE_GIF",	8}}
										};
	
	// output
	wxFileName	dfn(dest_fn);
	assert(dfn.IsOk());
	dfn.Normalize();
	wxFileOutputStream	fos(dfn.GetFullPath());
	assert(fos.IsOk());
	wxTextOutputStream	tos(fos);
	
	wxImage		img(cfn.GetFullPath());
	assert(img.IsOk());
	
	// check has corresponding image handler
	wxImageHandler	*img_handler = wxImage::FindHandler(img.GetType());
	wxASSERT_MSG(img_handler, "missing image handler");
	
	const wxBitmapType	typ = img.GetType();
	assert(bmTypeToStr.count(typ) > 0);
	
	const bmInfo	bm_info = bmTypeToStr.at(typ);
	
	const wxString	short_fn = cfn.GetName();
	
	const size_t	sz = cfn.GetSize().GetLo();
	const int	w = img.GetWidth();
	const int	h = img.GetHeight();
	
	const wxString	struct_name = short_fn + "_craw";
	
	tos << "// serialized \"" << cfn.GetFullName() << "\"" << endl;
	tos << "static" << endl;
	tos << "struct" /*<< struct_name*/ << endl;
	tos << "{" << endl;
	tos << "\tint\t\tm_Width;" << endl;
	tos << "\tint\t\tm_Height;" << endl;
	tos << "\tint\t\tm_Depth;" << endl;
	tos << "\twxBitmapType\tm_Type;" << endl;
	tos << "\tsize_t\t\tm_Len;" << endl;
	tos << "\tconst uint8_t\tm_Data[" << (int)sz << "];" << endl;
	tos << "} " << struct_name << " =" << endl;
	tos << "{" << endl;
	tos << "\t" << w << ", " << h << ", " << bm_info.m_Depth << ", " << bm_info.m_TypeStr << ", " << (int) sz << "," << endl;
	tos << "\t{";
	
	wxDataInputStream	dis(fis);
	const int		BYTES_PER_LINE = 32;
	
	size_t	i = sz;
	
	while (i)
	{
		tos << endl << "\t";
		
		for (int j = 0; j < BYTES_PER_LINE; j++)
		{
			unsigned char	c = dis.Read8();
			tos << wxString::Format("0x%02X", c);
		
			if (--i > 0)
				tos << ",";
			else
			{
				tos << endl << "\t}" << endl;
				break;
			}
		}
	}
	
	tos << "};" << endl;
	
	return true;		// ok
}

//---- On Serialize PNG -------------------------------------------------------

void	TopFrame::OnSerializeBitmapFile(wxCommandEvent &e)
{
	// can multi-select
	wxFileDialog	dlg(	this, "Open Bitmap File(s)", wxGetCwd(), "", "PNG files (*.png)|*.png|GIF files (*.gif)|*.gif", wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
				
	const int	res = dlg.ShowModal();
	if (res == wxID_CANCEL)		return;
	
	wxArrayString	path_list;
	
	dlg.GetPaths(path_list/*&*/);
	if (path_list.Count() == 0)	return;
	
	for (int i = 0; i < path_list.Count(); i++)
	{
		wxFileName	cfn(path_list[i]);
		uMsg("serializing %S", cfn.GetFullPath());
		assert(cfn.IsOk() && cfn.FileExists());
		
		const wxString	org_full_path = cfn.GetFullPath();
		
		cfn.SetExt("c_raw");			// should replace SPACES by UNDERLINES?
		const wxString	dest_full_path = cfn.GetFullPath();
		
		bool	ok = SerializeOneBitmapFile(org_full_path, dest_full_path);
		if (!ok)	uErr("failed to serialize %S", org_full_path);
	}
	
	uMsg("Serializations done");
}

//---- On Save Screenshot -----------------------------------------------------

void	TopFrame::OnSaveScreenshot(wxCommandEvent &e)
{
	#if 0
	{	
		uMsg("testing synthetic exception");
		
		string	res;
		
		try
		{
			res = xsprintf("%lu", 1234);
			assert(res.empty());
	
		}
		catch (std::exception &ec)
		{
			string	what_s = ec.what();
			
			uMsg("std::exception in xsprintf(%s)", what_s);
		}
		catch (...)
		{
			std::exception_ptr	ep = std::current_exception();
			
			uMsg("opaque exception in xsprintf()");
		}
	}
	#endif
	
	CallAfter(&TopFrame::DoSaveScreenshot);
	
	e.Skip();
}

//---- Do Save Screenshot -----------------------------------------------------

void	TopFrame::DoSaveScreenshot(void)
{
	const wxRect	rc = wxFrame::GetScreenRect();
	
	wxBitmap	bm(rc.GetSize());
	
	wxMemoryDC	mdc;
	
	mdc.SelectObject(bm);
	
	wxScreenDC	screen_dc;
	
	// (hide cursor)
	wxSetCursor(wxNullCursor);
	
	mdc.Blit(wxPoint(), rc.GetSize(), &screen_dc, rc.GetPosition());
	mdc.SelectObject(wxNullBitmap);
	
	// restore cursor
	wxSetCursor(*wxSTANDARD_CURSOR);
	
	const wxString	templ = wxString::Format(" _%dx%d.png", rc.width, rc.height);
	
	wxFileDialog	dlg(	this, "Save Screenshot", wxGetCwd(), templ, "PNG files (*.png)|*.png", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
				
	const int	res = dlg.ShowModal();
	if (res == wxID_CANCEL)		return;
	
	bm.SaveFile(dlg.GetPath(), wxBITMAP_TYPE_PNG);
}

//---- Load UI Group Entries --------------------------------------------------

StringList	TopFrame::LoadUIGroupEntries(const wxString &group)
{
	assert(m_UIConfig);
	
	StringList	entry_list;
	
	if (!m_UIConfig->HasGroup(group))		return entry_list;
	
	m_UIConfig->SetPath(group);
	
	// enumeration variables
	wxString	key;
	long		cookie;
	
	bool	cont_f = m_UIConfig->GetFirstEntry(key/*&*/, cookie/*&*/);
	while (cont_f)
	{
		wxString	val;
		
		m_UIConfig->Read(key, &val, "");
		assert(!val.IsEmpty());
		
		entry_list.push_back(val.ToStdString());
		
		cont_f = m_UIConfig->GetNextEntry(key/*&*/, cookie/*&*/);
	}
	
	// restore path
	m_UIConfig->SetPath("/");
	
	return entry_list;
}

//---- Save UI Group Entries --------------------------------------------------

void	TopFrame::SaveUIGroupEntries(const wxString &group, const StringList &entries)
{
	assert(m_UIConfig);
	
	// clear previous entries
	m_UIConfig->DeleteGroup(group);
	
	// write to config
	for (int i = 0; i < entries.size(); i++)
	{
		const wxString		key = wxString::Format("%s/entry%d", group, i);
		const std::string	val = entries[i];
		
		m_UIConfig->Write(key, wxString{val});
	}
	
	m_UIConfig->Flush();
}

//---- Clip Window Rect -------------------------------------------------------

void	TopFrame::ClipWindowRect(const vector<wxRect> &displayRects, wxRect &rc) const
{
	bool	intersect_f = false;
	
	for (const wxRect display_rc : displayRects)
	{
		if (!display_rc.Intersects(rc))		continue;
		
		intersect_f = true;
		rc.Intersect(display_rc);
	}

	const double	area = rc.GetWidth() * rc.GetHeight();
	if ((area < 100) || (!intersect_f))
	{
		const wxRect	d_rc = displayRects.front();

		rc = d_rc.Deflate(d_rc.width * 0.2, d_rc.height * 0.2);
	}
}

//---- Load UI Config ---------------------------------------------------------

void	TopFrame::LoadUIConfig(void)
{
	if (!m_UIConfig)		return;		// silent return
	if (IsBeingDeleted())		return;
	
	assert(!m_ApplyingUIPrefsFlags);
	
	AutoLock	lock(m_ApplyingUIPrefsFlags);
	
	/*
	// NONE of these work with proper negative coords!
	int	screen_x, screen_y;

	screen_x = wxSystemSettings::GetMetric(wxSYS_SCREEN_X);
	screen_y = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y);

	wxSize	disp_sz = wxGetDisplaySize();
	
	const wxRect	sys_rect = ::wxGetClientDisplayRect();
	*/

	// collect all display client areas
	vector<wxRect>	display_rects;
	
	for (int i = 0; i < wxDisplay::GetCount(); i++)
	{
		display_rects.push_back(wxDisplay(i).GetClientArea());
	}

	wxRect	frame_rc;
	
	m_UIConfig->Read("/Window/x", &frame_rc.x, 100);
	m_UIConfig->Read("/Window/y", &frame_rc.y, 100);
	m_UIConfig->Read("/Window/w", &frame_rc.width, 400);
	m_UIConfig->Read("/Window/h", &frame_rc.height, 400);
	long	l = 0;
	m_UIConfig->Read("/Window/vsash", &l, 200);
	ClipWindowRect(display_rects, frame_rc/*&*/);
	
	assert(m_TopNotePanel);
	SearchBar	&search_bar = m_TopNotePanel->GetSearchBar();
	search_bar.LoadUI(*m_UIConfig);
	
	wxRect	solo_log_rc;
	m_UIConfig->Read("/SoloLog/x", &solo_log_rc.x, 20);
	m_UIConfig->Read("/SoloLog/y", &solo_log_rc.y, 20);
	m_UIConfig->Read("/SoloLog/w", &solo_log_rc.width, 800);
	m_UIConfig->Read("/SoloLog/h", &solo_log_rc.height, 600);
	ClipWindowRect(display_rects, solo_log_rc/*&*/);
	
	const StringList	project_mru_list = LoadUIGroupEntries("ProjectBookmarks");
	const StringList	local_dir_bookmarks = LoadUIGroupEntries("LocalDirBookmarks");
	
	// apply
	{	SetSize(frame_rc);
		m_MainSplitterWindow->SetSashPosition(l);
		
		if (m_SoloLogFrame)	m_SoloLogFrame->SetSize(solo_log_rc);
		
		SetProjectBookmarkList(project_mru_list);
		
		m_Controller->SetLocalPathBookmarkList(local_dir_bookmarks);
		
		// apply AFTER top frame has been computed
		assert(m_BottomNotePanel);
		m_BottomNotePanel->LoadUI(*m_UIConfig);
	}
	
	UpdateToolbarState();
}

//---- Save UI Config -------------------------------------------------------

void	TopFrame::SaveUIConfig(void)
{
	if (!m_UIConfig)		return;		// silent return
	if (m_ApplyingUIPrefsFlags)	return;		// no save-during-load
	if (IsBeingDeleted())		return;
	assert(m_Controller);
	
	wxRect	rc = wxFrame::GetScreenRect();			// no equivalent SetScreenRect() ?
	
	m_UIConfig->Write("/Window/x", rc.GetX());
	m_UIConfig->Write("/Window/y", rc.GetY());
	m_UIConfig->Write("/Window/w", rc.GetWidth());
	m_UIConfig->Write("/Window/h", rc.GetHeight());
	
	m_UIConfig->Write("/Window/vsash", m_MainSplitterWindow->GetSashPosition());
	
	assert(m_BottomNotePanel);
	m_BottomNotePanel->SaveUI(*m_UIConfig);
	
	assert(m_TopNotePanel);
	SearchBar	&search_bar = m_TopNotePanel->GetSearchBar();
	search_bar.SaveUI(*m_UIConfig);
	
	if (m_SoloLogFrame)
	{	rc = ToWxRect(m_SoloLogFrame->GetScreenRect());
		m_UIConfig->Write("/SoloLog/x", rc.GetX());
		m_UIConfig->Write("/SoloLog/y", rc.GetY());
		m_UIConfig->Write("/SoloLog/w", rc.GetWidth());
		m_UIConfig->Write("/SoloLog/h", rc.GetHeight());
	}
	
	SaveUIGroupEntries("ProjectBookmarks", m_ProjectBookmarks);
	
	const StringList	local_path_bookmarks = m_Controller->GetLocalPathBookmarkList();
	SaveUIGroupEntries("LocalDirBookmarks", local_path_bookmarks);
	
	m_UIConfig->Flush();
}

//---- Dirty UI Config --------------------------------------------------------

void	TopFrame::DirtyUIPrefs(const bool immediate_f)
{
	if (m_ApplyingUIPrefsFlags || IsBeingDeleted())		// no save-during-load (or deletion)
	{	return;
	}
	
	if (immediate_f)
	{
		// for crashing juce panes
		SaveUIConfig();
	}
	else
	{
		// (re-)start
		MixDelay("ui_prefs", SAVE_UI_PREFS_DELAY_MS, this, &TopFrame::SaveUIConfig);
	}
}

//----- Set Dirty Project Prefs -----------------------------------------------

void	TopFrame::DirtyProjectPrefs(void)
{
	// uMsg("TopFrame::DirtyProjectPrefs()");
	
	assert(m_ProjectPrefs);
	
	m_ProjectPrefs->SetDirty();
}

//---- On Move Event ----------------------------------------------------------

void	TopFrame::OnMoveEvent(wxMoveEvent &e)
{
	DirtyUIPrefs();
	
	e.Skip();
}

//---- On Size Event ----------------------------------------------------------

void	TopFrame::OnSizeEvent(wxSizeEvent &e)
{
	DirtyUIPrefs();
	
	// must delay here
	CallAfter(&TopFrame::UpdateLEDPosition);
	
	e.Skip();
}

//---- On SizING Event (called more often) ------------------------------------

void	TopFrame::OnSizingEvent(wxSizeEvent &e)
{
	// no need to delay here?
	UpdateLEDPosition();
	
	e.Skip();
}

//---- On Main Horizontal Splitter Sash Changed -------------------------------

void	TopFrame::OnMainHSplitterSashChanged(wxSplitterEvent &e)
{
	// const int	id = e.GetId();
	// const int	pos = e.GetSashPosition();
	
	DirtyUIPrefs();
	
	e.Skip();
}

//---- On PaneBook Vertical Splitter Sash Changed -----------------------------

void	TopFrame::OnPaneBookSplitterSashChanged(wxSplitterEvent &e)
{
	// const int	id = e.GetId();
	// const int	pos = e.GetSashPosition();
	
	DirtyUIPrefs();
	
	e.Skip();
}

//---- On Toggle LuaJIT Mode --------------------------------------------------

void	TopFrame::OnToggleJITMode(wxCommandEvent &e)
{
	m_ClientState->SetJitFlag(!m_ClientState->GetJitFlag());
	
	// PROJECT prefs (not UI)
	DirtyProjectPrefs();
	
	e.Skip();
}

//---- On Toggle Load-Break Mode ----------------------------------------------------

void	TopFrame::OnToggleLoadBreakMode(wxCommandEvent &e)
{
	m_ClientState->SetLoadBreakFlag(!m_ClientState->GetLoadBreakFlag());
	
	// PROJECT prefs (not UI)
	DirtyProjectPrefs();
	
	e.Skip();
}

//---- On Toggle Step Mode ----------------------------------------------------

void	TopFrame::OnToggleStepMode(wxCommandEvent &e)
{
	m_ClientState->SetStepModeLinesFlag(!m_ClientState->GetStepModeLinesFlag());
	
	// PROJECT prefs (not UI)
	DirtyProjectPrefs();
	
	e.Skip();
}

//---- On Toggle Global Breakpoints -------------------------------------------

void	TopFrame::OnToggleGlobalBreakpoints(wxCommandEvent &e)
{
	m_ClientState->SetGlobalBreakpointsFlag(!m_ClientState->GetGlobalBreakpointsFlag());		// (not saved to prefs)
	
	e.Skip();
}

//---- On ComboBox KeyDown event ----------------------------------------------

void	TopFrame::OnComboBoxKeyDown(wxKeyEvent &e)
{
	e.Skip();
	
	// save prefs
	DirtyProjectPrefs();
}

//---- On ComboBox for Daemon <host:port> Change ------------------------------

void	TopFrame::OnDaemonHostComboBox(wxCommandEvent &e)
{
	DirtyProjectPrefs();
	
	e.Skip();
}

//---- On ComboBox for Lua main <source:function> Change ----------------------

void	TopFrame::OnLuaMainComboBox(wxCommandEvent &e)
{
	DirtyProjectPrefs();
	
	e.Skip();
}

//---- Create Toolbar ---------------------------------------------------------

void	TopFrame::CreateToolbar(void)
{
	uLog(APP_INIT, "TopFrame::CreateToolbar()");
	
	// uses COW (utf8 FAILS on Windows)
	wxFont	mono_ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT));
	
	wxBitmap	bm = LOAD_CRAW(log_frame_icon);
	assert(bm.IsOk());
	
	m_SoloLogFrame = ISoloLogFrame::Create(this, FRAME_ID_SOLO_LOG, bm);
	
	CreateToolBar(wxHORIZONTAL | wxTB_FLAT, MAIN_TOOLBAR_ID);
	
	m_ToolBar = GetToolBar();
	// m_ToolBar = new wxToolBar(this, MAIN_TOOLBAR_ID, wxDefaultPosition, wxDefaultSize, wxHORIZONTAL | wxTB_FLAT);
	assert(m_ToolBar);
	
	m_ToolBar->SetMargins(ICON_SIZE, ICON_SIZE);		// (in pixels)
	
	// load icons
	m_ToolBarBitmaps[BITMAP_ID_OPEN_FILES] = wxBitmap(fileopen_xpm);
	m_ToolBarBitmaps[BITMAP_ID_SAVE_PROJECT] = LOAD_CRAW(save);

	m_ToolBarBitmaps[BITMAP_ID_CONNECT] = LOAD_CRAW(connect);
	m_ToolBarBitmaps[BITMAP_ID_DISCONNECT] = LOAD_CRAW(disconnect);
	
	m_ToolBarBitmaps[BITMAP_ID_STOP] = LOAD_CRAW(stop);
	m_ToolBarBitmaps[BITMAP_ID_START] = LOAD_CRAW(play);
	m_ToolBarBitmaps[BITMAP_ID_STEP_INTO] = LOAD_CRAW(step_into);
	m_ToolBarBitmaps[BITMAP_ID_STEP_OVER] = LOAD_CRAW(step_over);
	m_ToolBarBitmaps[BITMAP_ID_STEP_OUT] = LOAD_CRAW(step_out);
	
	m_ToolBarBitmaps[BITMAP_ID_TOGGLE_LOAD_BREAK] = LOAD_CRAW(load_break);
	m_ToolBarBitmaps[BITMAP_ID_TOGGLE_LOAD_CONTINUE] = LOAD_CRAW(load_continue);
	
	m_ToolBarBitmaps[BITMAP_ID_TOGGLE_STEP_MODE_LINES] = LOAD_CRAW(step_lines);
	m_ToolBarBitmaps[BITMAP_ID_TOGGLE_STEP_MODE_VM] = LOAD_CRAW(step_VM);
	m_ToolBarBitmaps[BITMAP_ID_BREAKPOINTS_ENABLED] = LOAD_CRAW(red_dot);	// (duplicate)
	m_ToolBarBitmaps[BITMAP_ID_TOGGLE_JIT_ENABLED] = LOAD_CRAW(luajit);
	m_ToolBarBitmaps[BITMAP_ID_TOGGLE_JIT_DISABLED] = LOAD_CRAW(luajit).ConvertToDisabled();
	
	m_ToolBarBitmaps[BITMAP_ID_ABORT] =  wxBitmap(quit_xpm);
	
	m_ToolBarBitmaps[BITMAP_ID_SEARCH_APPLY] = LOAD_CRAW(find);
	m_ToolBarBitmaps[BITMAP_ID_SEARCH_UP] = LOAD_CRAW(up);
	m_ToolBarBitmaps[BITMAP_ID_SEARCH_DOWN] = LOAD_CRAW(down);
	m_ToolBarBitmaps[BITMAP_ID_SEARCH_HIGHLIGHT_ALL] = LOAD_CRAW(hl_matched_16);
	m_ToolBarBitmaps[BITMAP_ID_SEARCH_REGEX] = LOAD_CRAW(reg_ex_16);
	
	m_ToolBar->SetToolBitmapSize(wxSize(ICON_SIZE, ICON_SIZE));
	
	// NOTE: we don't care about SetToolLongHelp(), is shown in built-in status bar
	m_ToolBar->AddTool(TOOL_ID_OPEN_FILES, "Open", m_ToolBarBitmaps[BITMAP_ID_OPEN_FILES], "Open Project or Lua Files", wxITEM_DROPDOWN);
	m_ToolBar->AddTool(TOOL_ID_SAVE_PROJECT, "Save", m_ToolBarBitmaps[BITMAP_ID_SAVE_PROJECT], "Save Project");
	m_ToolBar->AddSeparator();
	
	m_ToolBar->AddTool(TOOL_ID_CONNECTION, "Connection", m_ToolBarBitmaps[BITMAP_ID_CONNECT], "Connect");
	m_ToolBar->AddSeparator();
	
	wxArrayString	dummy_choices;
	
	dummy_choices.Add("");

	// sorted combobox bombs on OSX!!!
	#define	combo_box_styl	(/*wxCB_SORT |*/ wxTE_PROCESS_ENTER)
	
	m_DaemonHostNPortComboBox = new wxComboBox(m_ToolBar, COMBOBOX_ID_DAEMON_HOST_N_PORT, wxEmptyString, wxDefaultPosition, wxSize(200, -1), dummy_choices, combo_box_styl);
	m_DaemonHostNPortComboBox->Bind(wxEVT_KEY_DOWN, &TopFrame::OnComboBoxKeyDown, this, COMBOBOX_ID_DAEMON_HOST_N_PORT);	
	m_DaemonHostNPortComboBox->SetOwnFont(mono_ft);
	m_ToolBar->AddControl(m_DaemonHostNPortComboBox, "Network");
	m_ToolBar->SetToolShortHelp(COMBOBOX_ID_DAEMON_HOST_N_PORT, "daemon hostname and port");
	
	m_LuaStartupComboBox = new wxComboBox(m_ToolBar, COMBOBOX_ID_LUA_MAIN_SOURCE_N_FUNCTION, wxEmptyString, wxDefaultPosition, wxSize(300, -1), dummy_choices, combo_box_styl);
	m_LuaStartupComboBox->Bind(wxEVT_KEY_DOWN, &TopFrame::OnComboBoxKeyDown, this, COMBOBOX_ID_LUA_MAIN_SOURCE_N_FUNCTION);
	m_LuaStartupComboBox->SetOwnFont(mono_ft);
	m_ToolBar->AddControl(m_LuaStartupComboBox, "Startup");
	m_ToolBar->SetToolShortHelp(COMBOBOX_ID_LUA_MAIN_SOURCE_N_FUNCTION, "daemon Lua startup is 'file.lua:function()'");
	
	// (doesn't trigger update events)
	m_DaemonHostNPortComboBox->ChangeValue(wxString(DEFAULT_HOSTNAME_AND_PORT_STRING));
	m_LuaStartupComboBox->ChangeValue(wxString(DEFAULT_LUA_STARTUP_STRING));
	
	m_ToolBar->AddSeparator();
	
	m_ToolBar->AddTool(TOOL_ID_START_RUN, "Start / Run", m_ToolBarBitmaps[BITMAP_ID_START], "Run");
	m_ToolBar->AddSeparator();
	m_ToolBar->AddTool(TOOL_ID_STEP_OVER, "Step Over", m_ToolBarBitmaps[BITMAP_ID_STEP_OVER], "Step Over");	// not implemented
	m_ToolBar->AddTool(TOOL_ID_STEP_INTO, "Step Into", m_ToolBarBitmaps[BITMAP_ID_STEP_INTO], "Step Into");
	m_ToolBar->AddTool(TOOL_ID_STEP_OUT, "Step Out", m_ToolBarBitmaps[BITMAP_ID_STEP_OUT], "Step Out");
	m_ToolBar->AddSeparator();
	
	m_ToolBar->AddCheckTool(TOOL_ID_TOGGLE_JIT, "Toggle JIT", m_ToolBarBitmaps[BITMAP_ID_TOGGLE_JIT_ENABLED], m_ToolBarBitmaps[BITMAP_ID_TOGGLE_JIT_DISABLED], "Toggle JIT on/off");
	m_ToolBar->AddTool(TOOL_ID_TOGGLE_LOAD_BREAK_MODE, "Toggle Load-Break", m_ToolBarBitmaps[BITMAP_ID_TOGGLE_LOAD_BREAK], "Load-Break");
	m_ToolBar->AddTool(TOOL_ID_TOGGLE_STEP_MODE, "Toggle Step Mode", m_ToolBarBitmaps[BITMAP_ID_TOGGLE_STEP_MODE_LINES], "Step Lines");
	m_ToolBar->AddCheckTool(TOOL_ID_TOGGLE_GLOBAL_BREAKPOINTS, "Toggle Breakpoints", m_ToolBarBitmaps[BITMAP_ID_BREAKPOINTS_ENABLED], wxNullBitmap, "Toggle Breakpoints On/Off");
	m_ToolBar->AddSeparator();
	m_ToolBar->AddTool(TOOL_ID_STOP, "Stop", m_ToolBarBitmaps[BITMAP_ID_STOP], "Stop Execution");
	m_ToolBar->AddSeparator();
	m_ToolBar->AddStretchableSpace();
	
	m_ToolBar->AddTool(TOOL_ID_ABORT, "Abort", m_ToolBarBitmaps[BITMAP_ID_ABORT], "Abort");
	
	m_ToolBar->SetToolPacking(16);
	m_ToolBar->SetToolSeparation(20);

	// after adding buttons to the toolbar, must call Realize() to reflect the changes
	// will FAIL on windows?
	m_ToolBar->Realize();
	
// add auxiliary bitmaps
	
	// create the list since vanilla CTOR leaves it invalid
	// some PNGs have alpha, others get a mask like when they use too few pixels (PNG lib behavior)
	//  NOPE, was Gtk wxMemoryDC bug
	m_AuxBitmaps.Create(ICON_SIZE, ICON_SIZE, true/*no mask*/);
	
	// kindof lame, can't assign at specific position
	int	ind = m_AuxBitmaps.Add(LOAD_CRAW(empty));	// fully transparent placeholder for stable icons column width
	assert(ind == BITMAP_ID_EMPTY_PLACEHOLDER);
	ind = m_AuxBitmaps.Add(LOAD_CRAW(red_dot));
	assert(ind == BITMAP_ID_RED_DOT);
	ind = m_AuxBitmaps.Add(LOAD_CRAW(plus));
	assert(ind == BITMAP_ID_PLUS);
	ind = m_AuxBitmaps.Add(LOAD_CRAW(minus));
	assert(ind == BITMAP_ID_MINUS);
	ind = m_AuxBitmaps.Add(LOAD_CRAW(close));
	assert(ind == BITMAP_ID_CLOSE_TAB);
	ind = m_AuxBitmaps.Add(LOAD_CRAW(arrow_right));
	assert(ind == BITMAP_ID_ARROW_RIGHT);
	ind = m_AuxBitmaps.Add(LOAD_CRAW(arrow_down));
	assert(ind == BITMAP_ID_ARROW_DOWN);
	ind = m_AuxBitmaps.Add(LOAD_CRAW(flash));
	assert(ind == BITMAP_ID_FLASH);
	
	assert(m_AuxBitmaps.GetImageCount() == BITMAP_ID_AUX_BITMAP_LAST);
	
// status bar & LED
	
	m_GreenRedLED[0] = wxIcon(green_xpm);
	m_GreenRedLED[1] = wxIcon(red_xpm);
	
	// wxSTB_SHOW_TIPS ?
	#ifdef __WIN32__
		#define sb_styl	wxSB_NORMAL
	#else
		#define	sb_styl	wxSB_SUNKEN
	#endif
	
	m_StatusBar = CreateStatusBar(4, sb_styl, STATUSBAR_ID_TOOLBAR);
	assert(m_StatusBar);

	const int	margin = 2;
	const int	led_w = m_GreenRedLED[0].GetWidth();
	const int	led_h = m_GreenRedLED[0].GetHeight();
	
	int	widths[] = {-1, led_w + (2 * margin), -1, THROB_ICON_WIDTH + (2 * margin)};
	int	styles[] = {sb_styl, wxSB_NORMAL, sb_styl, wxSB_NORMAL};
	
	// set dimensions BEFORE style
	m_StatusBar->SetStatusWidths(WXSIZEOF(styles), widths);
	m_StatusBar->SetMinHeight(led_h);
	m_StatusBar->SetStatusStyles(WXSIZEOF(styles), styles);
	
	m_BitmapLED = new wxStaticBitmap(m_StatusBar, -1, m_GreenRedLED[0]);
}

//---- Get Icon memory Stream -------------------------------------------------

vector<uint8_t>	TopFrame::GetIconStream(const int &icon_id)
{
	switch (icon_id)
	{
		case BITMAP_ID_EMPTY_PLACEHOLDER:	return LOAD_CRAW_BUFF(empty); break;
		case BITMAP_ID_RED_DOT:			return LOAD_CRAW_BUFF(red_dot); break;
		case BITMAP_ID_PLUS:			return LOAD_CRAW_BUFF(plus); break;
		case BITMAP_ID_MINUS:			return LOAD_CRAW_BUFF(minus); break;
		case BITMAP_ID_CLOSE_TAB:		return LOAD_CRAW_BUFF(close); break;
		case BITMAP_ID_ARROW_RIGHT:		return LOAD_CRAW_BUFF(arrow_right); break;
		case BITMAP_ID_ARROW_DOWN:		return LOAD_CRAW_BUFF(arrow_down); break;
		case BITMAP_ID_FLASH:			return LOAD_CRAW_BUFF(flash); break;
			
		default:
		
			assert(false);	// error
			break;
	}
	
	vector<uint8_t>	buff;
	return buff;	// ok
}

//---- Init Cairo Renderer ----------------------------------------------------

void	TopFrame::InitCairoRenderer(void)
{
	m_CairoRenderer = Renderer::NewCairoRenderer();
	assert(m_CairoRenderer);
	
	// load resources
	Renderer	&rend = *m_CairoRenderer;
	assert(rend.IsOK());
	
	// create fonts
	rend.CreateFont(VAR_UI_COLUMN_HEADER, "sans", 11, Color(20, 20, 20, 0xff), FONT_STYLE_NONE);					// very dark grey
	rend.CreateFont(VAR_UI_DEFAULT, "monospace", 9, Color(RGB_COLOR::SOFT_BLACK));							// black
	rend.CreateFont(VAR_UI_CHANGED_VAL, "monospace", 9, Color(255, 10, 100, 0xff));							// red
	rend.CreateFont(VAR_UI_EDITOR_URL, "monospace", 9, Color(10, 10, 255, 0xff), FONT_STYLE_UNDERLINED);				// blue
	rend.CreateFont(VAR_UI_MEMORY_URL, "sans", 9, Color(10, 120, 10, 0xff), FONT_STYLE_UNDERLINED);					// green
	rend.CreateFont(VAR_UI_METATABLE_URL, "monospace", 9, Color(10, 10, 255, 0xff), FONT_STYLE_UNDERLINED | FONT_STYLE_BOLD);	// blue
	
	rend.CreateFont(VAR_UI_DEFAULT_MONOSPACE, "monospace", 9, Color(RGB_COLOR::SOFT_BLACK));
	
	// const auto	ft = rend.GetFont(VAR_UI_DEFAULT_MONOSPACE);
	// uMsg("DYNLIST font family(%S), sz(%g), backend(%S)", ft->GetFamily(), ft->GetSize(), ft->GetBackEndType());
	
	// create icons
	for (int i = 0; i < m_AuxBitmaps.GetImageCount(); i++)
	{
		rend.CreateSurfaceFromMemory(i, GetIconStream(i));
	}
	
	m_ModalTextEdit = new ModalTextEdit(this, DIALOG_ID_GRID_EDIT);
}

//---- Update Toolbar State ---------------------------------------------------

void	TopFrame::UpdateToolbarState(const bool immediate_f)
{	
	if (!m_ToolBar)		return;
	
	uLog(UI, "TopFrame::UpdateToolbarState(immediate_f = %c)", immediate_f);
	
	assert(m_Client);
	assert(m_ProjectPrefs);
	assert(m_StatusBar);
	assert(m_BitmapLED);
	assert(m_Controller);
	assert(m_ToolsMenu);
	
	IClientState	&client_state = m_Controller->GetClientState();
	
	const INIT_LEVEL	lvl = m_Controller->GetInitLevel();
	
	const bool	project_loaded_f = m_ProjectPrefs->IsLoaded();
	const bool	connected_f = m_Client->IsConnected();
	const bool	can_connect_or_disconnect_f = project_loaded_f;
	const bool	lua_started_f = (lvl >= INIT_LEVEL::LUA_STARTED);
	const bool	client_active_f = m_Controller->IsClientActive();
	const bool	resumable_f = m_Controller->IsClientActive();
	const bool	load_break_f = client_state.GetLoadBreakFlag();
	const bool	step_line_f = client_state.GetStepModeLinesFlag();
	const bool	global_breakpoints_f = client_state.GetGlobalBreakpointsFlag();
	const bool	dirty_prefs_f = m_ProjectPrefs->IsDirty();
	const bool	can_jit_f = m_DaemonPlatform.HasJIT();
	const bool	jit_f = client_state.GetJitFlag() && can_jit_f;
	const bool	setup_f = (lvl < INIT_LEVEL::LUA_STARTED);		// setup phase
	
	m_ToolBar->EnableTool(TOOL_ID_SAVE_PROJECT, dirty_prefs_f);
	
	m_ToolBar->SetToolNormalBitmap(TOOL_ID_CONNECTION, m_ToolBarBitmaps[connected_f ? BITMAP_ID_DISCONNECT : BITMAP_ID_CONNECT]);
	m_ToolBar->SetToolShortHelp(TOOL_ID_CONNECTION, connected_f ? "Disconnect" : "Connect");
	
	m_ToolBar->SetToolNormalBitmap(TOOL_ID_START_RUN, m_ToolBarBitmaps[BITMAP_ID_START]);
	m_ToolBar->SetToolShortHelp(TOOL_ID_START_RUN, connected_f ? (lua_started_f ? "Resume" : "START") : "Stop");
	
	m_ToolBar->SetToolNormalBitmap(TOOL_ID_TOGGLE_LOAD_BREAK_MODE, m_ToolBarBitmaps[load_break_f ? BITMAP_ID_TOGGLE_LOAD_BREAK : BITMAP_ID_TOGGLE_LOAD_CONTINUE]);
	m_ToolBar->SetToolShortHelp(TOOL_ID_TOGGLE_LOAD_BREAK_MODE, load_break_f ? "Break On Load" : "Continue On Load");
	
	m_ToolBar->SetToolNormalBitmap(TOOL_ID_TOGGLE_STEP_MODE, m_ToolBarBitmaps[step_line_f ? BITMAP_ID_TOGGLE_STEP_MODE_LINES : BITMAP_ID_TOGGLE_STEP_MODE_VM]);
	m_ToolBar->SetToolShortHelp(TOOL_ID_TOGGLE_STEP_MODE, step_line_f ? "Step Line" : "Step Instruction");
	
	m_ToolBar->ToggleTool(TOOL_ID_TOGGLE_JIT, jit_f);
	m_ToolBar->ToggleTool(TOOL_ID_TOGGLE_GLOBAL_BREAKPOINTS, global_breakpoints_f);
	
	m_ToolBar->EnableTool(TOOL_ID_CONNECTION, can_connect_or_disconnect_f);
	m_ToolBar->EnableTool(TOOL_ID_START_RUN, resumable_f && connected_f);
	m_ToolBar->EnableTool(TOOL_ID_STEP_OVER, resumable_f);
	m_ToolBar->EnableTool(TOOL_ID_STEP_INTO, resumable_f);
	m_ToolBar->EnableTool(TOOL_ID_STEP_OUT, resumable_f);
	
	m_ToolBar->EnableTool(TOOL_ID_TOGGLE_GLOBAL_BREAKPOINTS, lua_started_f);
	m_ToolBar->EnableTool(TOOL_ID_TOGGLE_STEP_MODE, true/*always enabled*/);
	m_ToolBar->EnableTool(TOOL_ID_STOP, client_active_f && lua_started_f);
	
	// (disable after setup done)
	m_ToolBar->EnableTool(TOOL_ID_TOGGLE_JIT, can_jit_f && setup_f);
	// m_ToolBar->EnableTool(TOOL_ID_TOGGLE_LOAD_BREAK_MODE, setup_f);
	
	m_BitmapLED->SetBitmap(m_GreenRedLED[client_active_f ? 0 : 1]);

	if (immediate_f)
	{	m_ToolBar->Update();
		m_StatusBar->Update();
	}
	else
	{	m_ToolBar->Refresh();
		m_StatusBar->Refresh();
	}
	
	m_ToolsMenu->Enable(MENU_ID_NETWORK_STRESS_TEST, connected_f);
}

//---- Update LED position ----------------------------------------------------

void	TopFrame::UpdateLEDPosition(void)
{
	if (!m_StatusBar || !m_BitmapLED)	return;
	
	const wxSize	sz = m_BitmapLED->GetSize();
	
	wxRect	rc;
	
	m_StatusBar->GetFieldRect(1, rc/*&*/);
	
	wxPoint	pt;
	
	pt.x = rc.x + ((rc.width - sz.x) / 2);
	pt.y = rc.y + ((rc.height - sz.y) / 2);
	
	m_BitmapLED->Move(pt);
}

///////////////////////////////////////////////////////////////////////////////

//---- Get Known Parent window ------------------------------------------------

// static
wxString	TopFrame::GetKnownParent(const wxWindow *w, int depth)
{
	if (!w)		return "nil";
	
	const int	id = w->GetId();
	
	if (IsKnownID(id))	return GetWinIDName(id) + " (depth: " + to_string(depth) + ")";
	
	// recurse
	return GetKnownParent(w->GetParent(), depth + 1);
}

//---- Dump/Log wxWindow ------------------------------------------------------

// static
void	TopFrame::DumpWindow(const wxWindow *w, wxString name, const int indent)
{
	const wxString	tab = wxString(' ', indent * 2);
	
	uMsg("%s  wxWindow: %S", tab, name);
	uMsg("%s  Window(%s)", tab, w ? "true" : "nil");
	if (!w)
	{	uMsg("");
		return;
	}
	
	const int	id = w->GetId();
	name = TopFrame::GetWinIDName(id);
	uMsg("%s  known name: \"%s\"", tab, name);
	
	uMsg("%s  known parent: \"%s\"", tab, GetKnownParent(w->GetParent()));

	wxSize	sz;
	
	sz = w->GetSize();
	uMsg("%s  GetSize(%d, %d)", tab, sz.x, sz.y);

	sz = w->GetClientSize();
	uMsg("%s  GetClientSize(%d, %d)", tab, sz.x, sz.y);
	
	sz = w->GetMinSize();
	uMsg("%s  GetMinSize(%d, %d)", tab, sz.x, sz.y);
	
	sz = w->GetMinClientSize();
	uMsg("%s  GetMinClientSize(%d, %d)", tab, sz.x, sz.y);
	
	sz = w->GetBestSize();
	uMsg("%s  GetBestSize(%d, %d)", tab, sz.x, sz.y);
	
	sz = w->GetEffectiveMinSize();
	uMsg("%s  GetEffectiveMinSize(%d, %d)", tab, sz.x, sz.y);
	
	sz = w->GetWindowBorderSize();
	uMsg("%s  GetWindowBorderSize(%d, %d)", tab, sz.x, sz.y);
	
	// uMsg("%s  CanScroll(wxHORIZONTAL: %s)", ids, w->CanScroll(wxHORIZONTAL) ? "true" : "false");
	// uMsg("%s  CanScroll(wxVERTICAL: %s)", ids, w->CanScroll(wxVERTICAL) ? "true" : "false");
	
	uMsg("");
}

//---- Dump Splitter ----------------------------------------------------------

// static
void	TopFrame::DumpSplitter(wxSplitterWindow *sw, wxString name)
{	
	assert(sw);
	uMsg("wxSplitterWindow: \"%s\"", name);
	
	wxSize	sz;
	int	i;
	double	d;
	
	i = sw->GetSashPosition();
	uMsg("  GetSashPosition(%d)", i);
	
	i = sw->GetSashSize();
	uMsg("  GetSashSize(%d)%s", i, (i == 0) ? " invisible" : "");
	
	// in ./generic/splitter.cpp gets it from wxRenderNative::Get().GetSplitterParams(this).widthSash
	i = sw->GetDefaultSashSize();
	uMsg("  GetDefaultSashSize(%d)", i);
	
	i = sw->GetMinimumPaneSize();
	uMsg("  GetMinimumPaneSize(%d)", i);
	
	i = sw->GetBorderSize();
	uMsg("  GetBorderSize(%d)", i);
	
	d = sw->GetSashGravity();
	uMsg("  GetSashGravity(%f)", d);
	
	uMsg("  IsSplit(%s)", sw->IsSplit() ? "true" : "false");
	
	// protected member ?
	// sz = sw->GetWindowSize();
	// uMsg("  GetWindowSize(%d, %d)", sz.x, sz.y);
	
	DumpWindow(sw, name, 0);
	
	DumpWindow(sw->GetWindow1(), "GetWindow1()", 2);
	DumpWindow(sw->GetWindow2(), "GetWindow2()", 2);
	
	uMsg("");
}

//---- On Dump Notebook Vars --------------------------------------------------

void	TopFrame::OnDumpNotebookVars(wxCommandEvent &e)
{
	assert(m_BottomNotePanel);
	uMsg("TopFrame::OnDumpNotebookVars()");
	
	// wx crashes if window not realized!
	
	wxPoint	pt = ::wxGetMousePosition();			// global coords
	wxWindow	*win = wxFindWindowAtPoint(pt);
	
	vector<wxWindow*>	stack;
	
	while (win)
	{
		stack.push_back(win);
		
		win = win->GetParent();
	}
	
	for (int i = stack.size() -1; i >= 0; i--)
	{
		win = stack[i];
		int	id = win->GetId();
		string	id_name = GetWinIDName(id);
		
		DumpWindow(win, wxString(id_name), (stack.size() -1 - i) * 2);

		uMsg("");
	}
}

// nada mas
