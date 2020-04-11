// Lua large-string variable memory controller

#include <cassert>
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <random>
#include <map>			// ???

#include "wx/panel.h"
#include "wx/spinctrl.h"

#include "TopFrame.h"
#include "Controller.h"
#include "UIConst.h"

#include "ddt/sharedDefs.h"
#include "ddt/Collected.h"

#include "logImp.h"

#include "lx/xstring.h"

using namespace std;
using namespace LX;
using namespace	DDT_CLIENT;

class MemoryViewCtrl: public wxPanel, public PaneMixIn, private LX::SlotsMixin<LX::CTX_WX>
{
public:
	MemoryViewCtrl(IBottomNotePanel &parent, int id, TopFrame &tf);
	virtual ~MemoryViewCtrl();
	
private:
	
	void	OnShowMemory(const string key, const string val, const string val_type, const bool bin_data_f);
	
	void	ClearVariables(void);
	void	RequestVarReload(void);
	
	void	RefreshHexDump(void);
	void	RefreshAsciiDump(void);
	void	RedrawMemory(void);
	void	SyncSpinCtrls(void);
	void	Randomize(void);
	
	void	OnResizeEvent(wxSizeEvent &e);
	void	OnRefreshMemoryView(wxCommandEvent &e);
	void	OnSpinRangeEvent(wxSpinEvent &e);
	void	OnMouseHover(wxMouseEvent &e);
	
	IBottomNotePanel	&m_BottomNotePanel;
	TopFrame		&m_TopFrame;
	
	wxStaticText		m_VarNameCtrl;
	wxSpinCtrl		m_SpinFrom;
	wxRadioButton		m_HexRadioButton, m_AsciiRadioButton;
	wxButton		m_RefreshButton;
	wxTextCtrl		m_DumpTextCtrl;
	
	wxSize			m_CharPixels;
	wxTextAttr		m_Black, m_Red, m_GreyBack, m_YellowBack;
	
	vector<uint8_t>		m_Buff;
	map<size_t, int>	m_EditToBytePosMap;		// ???
	string			m_LastKey;
	
	int			m_LastBytePos;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MemoryViewCtrl, wxPanel)

	EVT_SIZE(							MemoryViewCtrl::OnResizeEvent)
	EVT_MENU(		MENU_ID_REFRESH_MEMORY_VIEW,		MemoryViewCtrl::OnRefreshMemoryView)
	EVT_BUTTON(		BUTTON_ID_MEMORY_REFRESH,		MemoryViewCtrl::OnRefreshMemoryView)
	EVT_RADIOBUTTON(	RADIO_ID_MEMORY_VIEW_HEX,		MemoryViewCtrl::OnRefreshMemoryView)
	EVT_RADIOBUTTON(	RADIO_ID_MEMORY_VIEW_ASCII,		MemoryViewCtrl::OnRefreshMemoryView)
	EVT_SPINCTRL(		SPIN_ID_MEMORY_HIGHLIGHT_FROM,		MemoryViewCtrl::OnSpinRangeEvent)
	
END_EVENT_TABLE()

const long DUMP_TEXT_STYLE = (wxTE_RICH | wxTE_NOHIDESEL | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxBORDER_SUNKEN);

//---- CTOR -------------------------------------------------------------------

	MemoryViewCtrl::MemoryViewCtrl(IBottomNotePanel &parent, int id, TopFrame &tf)
		: wxPanel(parent.GetWxWindow(), id),
		PaneMixIn(parent, id, this),
		m_BottomNotePanel(parent),
		m_TopFrame(tf),
		
		// m_VarNameCtrl(this, CONTROL_ID_MEMORY_VAR_NAME, "", wxDefaultPosition, wxDefaultSize, wxTE_NOHIDESEL | wxTE_READONLY | wxBORDER_SUNKEN),
		m_VarNameCtrl(this, CONTROL_ID_MEMORY_VAR_NAME, "", wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE),		// wxBORDER_STATIC 
		// spin control
		m_SpinFrom(this, SPIN_ID_MEMORY_HIGHLIGHT_FROM, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS | wxALIGN_RIGHT, 0, 1, 0),
		// radio buttons
		m_HexRadioButton(this, RADIO_ID_MEMORY_VIEW_HEX, "Hex", wxDefaultPosition, wxDefaultSize, wxRB_GROUP),
		m_AsciiRadioButton(this, RADIO_ID_MEMORY_VIEW_ASCII, "ASCII", wxDefaultPosition, wxDefaultSize),
		m_RefreshButton(this, BUTTON_ID_MEMORY_REFRESH, "Refresh", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
		// hexdump text view
		m_DumpTextCtrl(this, CONTROL_ID_MEMORY_DUMP_TEXT, "", wxDefaultPosition, wxDefaultSize, DUMP_TEXT_STYLE)
		
{
	m_Buff.clear();
	
	wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT));
	SetFont(ft);
	
	m_CharPixels = ft.GetPixelSize();
	
	m_Black = wxTextAttr(*wxBLACK, *wxWHITE, ft);
	m_Red = wxTextAttr(*wxRED, *wxWHITE, ft);
	m_GreyBack = wxTextAttr(*wxBLACK, wxColour(0xe0, 0xe0, 0xff), ft);
	m_YellowBack = wxTextAttr(*wxBLACK, wxColour(255, 240,  80), ft);
	
	// m_VarNameCtrl.SetWindowStyleFlag(wxBORDER_RAISED);
	
	m_DumpTextCtrl.AlwaysShowScrollbars(false, true);
	
	wxBoxSizer	*top_hsizer = new wxBoxSizer(wxHORIZONTAL);
	
	top_hsizer->Add(&m_VarNameCtrl, wxSizerFlags(1).Border(wxALL, 1).Align(wxALIGN_CENTER_VERTICAL));
	top_hsizer->Add(&m_SpinFrom, wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL));
	top_hsizer->Add(&m_HexRadioButton, wxSizerFlags(0).Border(wxLEFT | wxRIGHT, 2).Align(wxALIGN_CENTER_VERTICAL));
	top_hsizer->Add(&m_AsciiRadioButton, wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL));
	top_hsizer->Add(&m_RefreshButton, wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL));
	
	wxBoxSizer	*v_sizer = new wxBoxSizer(wxVERTICAL);
	
	v_sizer->Add(top_hsizer, wxSizerFlags(0).Border(wxALL, 4).Expand());
	v_sizer->Add(&m_DumpTextCtrl, wxSizerFlags(1).Border(wxALL, 4).Expand());
	
	SetSizer(v_sizer);
	
	// default is hex view
	m_HexRadioButton.SetValue(true);
	
	m_TopFrame.Bind(wxEVT_COMMAND_MENU_SELECTED, &MemoryViewCtrl::OnRefreshMemoryView, this, MENU_ID_REFRESH_MEMORY_VIEW);
	
	MixConnect(this, &MemoryViewCtrl::OnShowMemory, tf.GetController().GetSignals().OnShowMemory);
	
	// post to self
	wxQueueEvent(this, wxCommandEvent(wxEVT_MENU, MENU_ID_REFRESH_MEMORY_VIEW).Clone());
}

//---- DTOR -------------------------------------------------------------------

	MemoryViewCtrl::~MemoryViewCtrl()
{
	uLog(DTOR, "MemoryViewCtrl::DTOR");
	
	// m_DumpTextCtrl.Unbind(wxEVT_MOTION, &MemoryViewCtrl::OnMouseHover, this);
}

//---- Clear Variable (PaneMixIn IMP) -----------------------------------------

void	MemoryViewCtrl::ClearVariables(void)
{
	m_Buff.clear();
	m_EditToBytePosMap.clear();
	
	m_VarNameCtrl.SetLabel("");
	m_DumpTextCtrl.ChangeValue("");	
}

//---- Request Var Reload (PaneMixIn IMP) -------------------------------------

void	MemoryViewCtrl::RequestVarReload(void)
{
	// if (!m_TopFrame.IsLuaListening())		return;		// not needed
		
	m_TopFrame.CheckMemoryWatch();
}

//---- On Resize event --------------------------------------------------------

void	MemoryViewCtrl::OnResizeEvent(wxSizeEvent &e)
{
	uLog(UI, "MemoryViewCtrl::OnResizeEvent()");
	
	CallAfter(&MemoryViewCtrl::RedrawMemory);
	
	e.Skip();
}

//---- On Mouse Hover event ---------------------------------------------------

void	MemoryViewCtrl::OnMouseHover(wxMouseEvent &e)
{
	e.Skip();
	
	const auto	pt(e.GetPosition());
	long		edit_pos = -1;
	
	const wxTextCtrlHitTestResult	res = m_DumpTextCtrl.HitTest(pt, &edit_pos);
	if ((res != wxTE_HT_ON_TEXT) || (-1 == edit_pos))	return;
	
	// if (!m_EditToBytePosMap.count(edit_pos))		return;
	// const size_t	byte_pos = m_EditToBytePosMap.at(edit_pos);
	
	/*
	const auto	range = m_EditToBytePosMap.equal_range(edit_pos);
	if (m_EditToBytePosMap.end() == range.first)		return;
	const size_t	byte_pos = range.first->second;
	*/
	const auto	it = m_EditToBytePosMap.lower_bound(edit_pos);
	if (m_EditToBytePosMap.end() == it)			return;
	const size_t	byte_pos = it->second;
	
	if (byte_pos == m_LastBytePos)	return;
	m_LastBytePos = byte_pos;
	
	// const size_t	ed_start_pos = it->first;
	
	// uMsg("MemoryViewCtrl::OnMouseHover() pos: edit(%d), line(%d)", ed_start_pos, byte_pos);
}

//---- On Refresh Memory View (menu/button/radio) event -----------------------

void	MemoryViewCtrl::OnRefreshMemoryView(wxCommandEvent &e)
{
	// Randomize();
	
	// should update title & such -- FIXME
	
	RedrawMemory();
	
	e.Skip();
}

//---- Sync Spin Controls -----------------------------------------------------

void	MemoryViewCtrl::SyncSpinCtrls(void)
{
	m_SpinFrom.SetRange(0, m_Buff.size() - 1);
}

void	MemoryViewCtrl::OnSpinRangeEvent(wxSpinEvent &e)
{
	/*
	const int	id = e.GetId();
	const bool	from_f = (id == SPIN_ID_MEMORY_HIGHLIGHT_FROM);
	const int	pos = e.GetPosition();
	*/
	
	// m_SpinTo.SetRange(m_SpinFrom.GetMax(), len);
	
	CallAfter(&MemoryViewCtrl::RedrawMemory);
	
	e.Skip();
}

//---- Refresh Hex Dump (canonical format) ------------------------------------

void	MemoryViewCtrl::RefreshHexDump(void)
{
	const int	client_w = m_DumpTextCtrl.GetClientSize().GetWidth();		// - 20;		// any scrollbar width ?
	if (client_w <= 0)	return;
	
	int		horiz_chars = client_w / m_CharPixels.GetWidth();
	if (horiz_chars <= 0)	return;
	
	const size_t	buff_len = m_Buff.size();
	
	// minimize address width
	const size_t	nybbles = ((std::log2(buff_len + 1) / 4) + 1);
	const string	address_fmt{xsprintf("0x%%0%dx: ", nybbles)};
	
	const size_t	address_chars = 2 + nybbles + 2;
	const size_t	spacing_chars = 4;
	const size_t	chars_per_hex_byte = 2 + 1;			// 2 nybbles, 1 space
	const size_t	chars_per_byte = chars_per_hex_byte + 1;	// 1 char
	const size_t	align_bytes = 8;
	
	horiz_chars -= address_chars;
	horiz_chars -= spacing_chars;
	if (horiz_chars <= 0)	return;
	
	const int	bytes_per_line = (horiz_chars / chars_per_byte) & ~(align_bytes - 1);
	if (bytes_per_line <= 0)	return;
	
	ostringstream	ss;
	vector<int>	zero_markers, select_markers;
	
	int		ln = 0;
	
	const size_t	select_byte_ind = m_SpinFrom.GetValue();
	
	for (size_t i = 0; i < buff_len; /*nop*/)
	{
		m_EditToBytePosMap.insert({ss.tellp(), ln});
		
		// address
		ss << xsprintf(address_fmt.c_str(), i);
		
		ostringstream	ascii_ss;
		
		// line
		for (int j = 0; (j < bytes_per_line) && (i < buff_len); i++, j++)
		{	
			const char	c = (char) m_Buff[i];
			
			const size_t	dest_pos = ss.tellp();
			
			// save marker pos in output stream/string
			if (c == 0)	zero_markers.push_back(dest_pos);
			
			if (select_byte_ind == i)
			{	select_markers.push_back(dest_pos);
				select_markers.push_back(dest_pos + 1);
			}
			
			// m_EditToBytePosMap.insert({dest_pos, i});
			// m_EditToBytePosMap.insert({dest_pos + 1, i});
			
			// m_EditToBytePosMap.insert({dest_pos spacing_chars
				
			// hex
			ss << xsprintf("%02x ", m_Buff[i]);
			
			// ascii
			const bool	ok = (c == ' ') || (isprint(c) && !isspace(c) && !iscntrl(c));
			ascii_ss << (ok ? c : '.');
		}
		
		m_EditToBytePosMap.insert({ss.tellp(), ln});
		
		if (buff_len == i)
		{	// pad last line
			const size_t	pad_bytes = bytes_per_line - (buff_len % bytes_per_line);
			
			ss << string(chars_per_hex_byte * pad_bytes, ' ');
		}
		
		ss << string(spacing_chars, ' ') << ascii_ss.str() << "\n";
		
		ln++;
	}
	
	m_DumpTextCtrl.WriteText(wxString(ss.str()));
	
	// color zero bytes
	for (const int &pos : zero_markers)	m_DumpTextCtrl.SetStyle(pos, pos + 2, m_Red);
		
	// color user selection bytes
	for (const int &pos : select_markers)	m_DumpTextCtrl.SetStyle(pos, pos + 1, m_YellowBack);
}

//---- Refresh ASCII Dump -----------------------------------------------------

void	MemoryViewCtrl::RefreshAsciiDump(void)
{
	const size_t	buff_len = m_Buff.size();
	
	const unordered_set<char>	good_set {' ', '\n', '\t'};
	const unordered_set<char>	bad_set {'\r'};
	
	ostringstream	ss;
	vector<int>	conv_markers;
	
	for (size_t i = 0; i < buff_len; i++)
	{
		const char	c = (char) m_Buff[i];
		
		const size_t	dest_pos = ss.tellp();
		
		m_EditToBytePosMap.insert({dest_pos, i});
		
		const bool	ok = good_set.count(c) || (isprint(c) && !isspace(c) && !bad_set.count(c));
		
		// save pos in output stream/string
		if (!ok)	conv_markers.push_back(dest_pos);
			
		ss << (ok ? c : '.');
	}
	
	m_DumpTextCtrl.WriteText(wxString(ss.str()));
	
	// convert single positions to spans [pos1; pos2]
	vector<pair<int, int>>	span_list;
	
	for (const auto &pos : conv_markers)
	{
		if ((span_list.size() && (pos == span_list.back().second)))
			span_list.back().second++;
		else	span_list.push_back({pos, pos + 1});
	}
				
	// color converted spans
	for (const auto &span : span_list)	m_DumpTextCtrl.SetStyle(span.first, span.second, m_GreyBack);
}

//---- Redraw Memory View -----------------------------------------------------

void	MemoryViewCtrl::RedrawMemory(void)
{
	m_DumpTextCtrl.Freeze();
	m_DumpTextCtrl.ChangeValue("");			// don't use Clear(), is SetValue() wrapper
	m_DumpTextCtrl.SetDefaultStyle(m_Black);
	
	const bool	hex_f = m_HexRadioButton.GetValue();
	
	if (hex_f)	RefreshHexDump();
	else		RefreshAsciiDump();
	
	m_DumpTextCtrl.Thaw();
}

//---- Set Memory Variable from daemon ----------------------------------------

void	MemoryViewCtrl::OnShowMemory(const string key, const string val, const string type_s, const bool bin_data_f)
{
	const uint8_t	*data_p = (const uint8_t*) val.data();
	assert(data_p);
	const size_t	data_sz = val.length();
	
	uLog(LOGIC, "MemoryViewCtrl::SetMemoryVariable(%S, %zu bytes)", key, data_sz);
	
	// detect var name changed & set new
	const bool	changed_name_f = (key != m_LastKey);
	
	m_LastKey = key;
	
	const string	label_s = xsprintf("%s %s (%zu bytes)", type_s, key, data_sz);
	
	m_VarNameCtrl.SetLabel(wxString(label_s));
	
	m_Buff.clear();
	m_EditToBytePosMap.clear();
	
	if (!data_p || (data_sz == 0))
	{	// no data
		m_DumpTextCtrl.ChangeValue("n/a");
		return;
	}
	
	// copy to own buffer (begin/end iterators)
	m_Buff.assign(&data_p[0], &data_p[data_sz]);
	
	if (changed_name_f)	
	{	// auto select hex/ascii
		if (bin_data_f)
			m_HexRadioButton.SetValue(true);		// setting value to false doesn't work on Gtl
		else	m_AsciiRadioButton.SetValue(true);
		
		m_SpinFrom.SetValue(0);
	}
	
	m_SpinFrom.SetRange(0, data_sz - 1);
	
	RedrawMemory();
	
	// scroll to top
	if (changed_name_f)		m_DumpTextCtrl.ShowPosition(1);
	
	// resize based on num digits
	const size_t	spin_w = (std::log(data_sz) + 1) * m_SpinFrom.GetFont().GetPixelSize().GetWidth();
	m_SpinFrom.SetMinSize(wxSize(spin_w, -1));
	Layout();
	
	m_BottomNotePanel.RaisePageID(PANE_ID_MEMORY, false/*do NOT reload*/);
}

//---- Randomize --------------------------------------------------------------

void	MemoryViewCtrl::Randomize(void)
{
	m_Buff.clear();
	
	const bool	bin_f = m_HexRadioButton.GetValue();
	
	const size_t	len = rand() * 10000ul / RAND_MAX;
	
	if (bin_f)
	{	// auto	rnd = bind(uniform_int_distribution<>(0, 255), default_random_engine{xtimestamp() & 0xffff/*seed*/});
		auto	rnd = bind(uniform_int_distribution<>(0, 255), default_random_engine{0});
		
		for (size_t i = 0; i < len; i++)	m_Buff.push_back(rnd());
	}
	else
	{	const string	pool_s {"01234567890abcdefghi\r\r\r\r\r\r\r\r\r\r\r\rlskjnsd \n\n\n\n\n\n"};
		
		// auto	rnd = bind(uniform_int_distribution<>(0, pool_s.length()-1), default_random_engine{xtimestamp_ms() & 0xffff/*seed*/});
		auto	rnd = bind(uniform_int_distribution<>(0, pool_s.length()-1), default_random_engine{0});
		
		for (size_t i = 0; i < len; i++)	m_Buff.push_back(pool_s[rnd()]);
	}
	
	SyncSpinCtrls();
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IPaneMixIn*	IPaneMixIn::CreateMemoryViewCtrl(IBottomNotePanel &bottom_book, int id, ITopNotePanel &top_book)
{
	Controller&	controller = bottom_book.GetController();
	TopFrame&	tf = controller.GetTopFrame();
	
	return new MemoryViewCtrl(bottom_book, id, tf);
}


// nada mas	
