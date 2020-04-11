// ddt client network "bridge"

#include <cstring>
#include <thread>
#include <chrono>
#include <random>

#include "wx/wx.h"
#include "wx/filename.h"
#include "wx/wfstream.h"

#include "ddt/sharedDefs.h"
#include "ddt/MessageQueue.h"

#include "lx/stream/MemDataInputStream.h"
#include "lx/stream/MemDataOutputStream.h"

#include "lx/misc/stopwatch.h"

#include "ddt/FileSystem.h"

#include "logImp.h"
#include "TopFrame.h"
#include "Controller.h"
#include "ClientState.h"

#include "SourceFileClass.h"

#include "ClientTCP.h"
#include "Client.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

//---- CTOR -------------------------------------------------------------------

	Client::Client(Controller &controller, TopFrame &tf, const int sync_sleep_ms, const int async_sleep_ms)
		: m_Controller(controller), m_TopFrame(tf),
		m_ClientTCP_Ptr(IClientTCP::Create(*this/*breather*/, async_sleep_ms)),
		m_ClientTCP(*m_ClientTCP_Ptr),
		m_SyncSleepMS(sync_sleep_ms)
{
	/*
	bool	ok = m_ClientTCP.SetOption(TCP_OPTION::CAN_ABORT_CONNECTION, 1);
	assert(ok);
	
	bool	ok = m_ClientTCP.SetOption(TCP_OPTION::SOCKET_LINGER, 0);
	assert(ok);
	*/
}

//---- DTOR -------------------------------------------------------------------

	Client::~Client()
{
	uLog(DTOR, "Client::DTOR");
}

void	Client::ClearCounters(void)
{
	m_ClientTCP.ClearCounters();
}

size_t	Client::GetReadCounter(void) const
{
	return m_ClientTCP.GetReadCounter();
}

size_t	Client::GetWriteCounter(void) const
{
	return m_ClientTCP.GetWriteCounter();
}

//---- Server Handshake -------------------------------------------------------

bool	Client::ServerHandshake(const RemoteServerConfig &remote)
{
	assert(!m_ClientTCP.IsConnected());
	
	uLog(NET, "Client::ServerHandshake() ENTERED");
	
	bool	ok = m_ClientTCP.Connect(remote.Host(), remote.Port());
	
	return ok;
}

//---- Close Connection -------------------------------------------------------

bool	Client::CloseConnection(void)
{
	uLog(NET, "Client::CloseConnection()");
	if (!m_ClientTCP.IsConnected())	return true;		// already disconnected
	
	#if 0	// caca
	{	MemoryOutputStream	mos;
					
		mos << CLIENT_MSG::NOTIFY_CLIENT_ABORTED << (uint32_t) 0;
					
		bool	ok = SendMessage(mos);
		return ok;
	}
	#endif
	
	m_ClientTCP.Disconnect();
	
	{	bool	ok = !m_ClientTCP.IsConnected();
		return ok;
	}
}

//---- Breathe IMPLEMENTATION -------------------------------------------------

bool	Client::Breathe(void)
{
	this_thread::sleep_for(chrono::milliseconds{m_SyncSleepMS});			// bullshit ?
	// this_thread::yield();
	
	return true;    // ok;
}

//---- Get Daemon Message Type ------------------------------------------------

DAEMON_MSG	Client::GetDaemonMessageType(const vector<uint8_t> &buff) const
{
	assert(buff.size() >= 4);
	
	// copy 4 header bytes
	vector<uint8_t>	header_buff(buff.begin(), buff.begin() + 4);
	
	MemDataInputStream	mis(header_buff);
	
	DAEMON_MSG	msg_t;
	mis >> msg_t;
	
	return msg_t;
}

//---- Log Outgoing Message Type ----------------------------------------------

bool	Client::LogOutgoingMessageType(const MemoryOutputStream &mos, const string &prefix) const
{
	// USED TO CRASH?
	
	vector<uint8_t>	header_buff(4u, 0);
	assert(header_buff.size() == 4);

	bool	ok = mos.CloneTo(&header_buff[0], header_buff.size());			// tortuous!
	assert(ok);
	
	MemoryInputStream	mis(header_buff);
	assert(mis.IsOk());
	
	DataInputStream		dis(mis);
	
	CLIENT_MSG	msg_t;
	
	dis >> msg_t;
	
	uLog(MSG_OUT, "%s%s", prefix, MessageName(msg_t));
	
	return true;
}

//---- Send Sync & Start Lua --------------------------------------------------

bool	Client::SendSyncAndStartLua(const RequestStartupLua &req_startup)
{
	uLog(NET, "Client::SendSyncAndStartLua()");
	assert(m_ClientTCP.IsConnected());
	assert(req_startup.IsOk());
	
	MemDataOutputStream	mos;
	
	mos << CLIENT_MSG::REQUEST_SYNC_N_START_LUA << req_startup;
	
	GetSerializedSources(mos/*&*/);
	
	bool	ok = SendMessage(mos);
	return ok;
}

//---- Get Serialized Lua Sources ---------------------------------------------

void	Client::GetSerializedSources(MemoryOutputStream &mos)
{
	vector<ISourceFileClass*>	sfc_list = m_Controller.GetSFCInstances();
	
	const size_t	n_files = sfc_list.size();
	
	DataOutputStream	dos(mos);
	
	string	buff;
	size_t	total_data_sz = 0;
	
	for (int i = 0; i < n_files; i++)
	{
		ISourceFileClass	*sfc = sfc_list[i];
		assert(sfc);
		
		wxFileName	cfn(sfc->GetFullPath());
		assert(cfn.IsOk());
		assert(cfn.FileExists());
		
		const string	name{cfn.GetName().ToStdString()};
		const string	ext{cfn.GetExt().ToStdString()};
		
		// overwrite any shebang on 1st line, but DON'T change the eol so linecount stays the same
		wxFileInputStream	fis(cfn.GetFullPath());
		assert(fis.IsOk());
		
		const size_t	sz = cfn.GetSize().GetLo();
		
		buff.resize(sz, ' ');
		
		fis.Read(&buff[0], buff.size());
		assert(fis.LastRead() == sz);
		
		// skip any utf8 BOM
		if ((buff.size() >= 3) && ('\xEF' == buff[0]) && ('\xBB' == buff[1]) && ('\xBF' == buff[2]))
		{	buff.erase(0, 3);
		}
	
		// count lines
		int	n_lines = 0;
		
		for (const auto c : buff)
		{
			n_lines += (c == '\n') ? 1 : 0;
		}
	
		// overwite any comment on 1st line
		if ((n_lines > 0) && ('#' == buff[0]))
		{
			const size_t	ln_pos = buff.find('\n');
			assert(ln_pos != string::npos);
			
			// replace up to end-of-line, which we keep so line numbers don't change
			buff.replace(buff.begin(), buff.begin() + ln_pos, ln_pos, ' ');
		}
		
		NetSourceFile	net_src_file{name, ext, n_lines, buff};
		
		// RE-serialize (pas terroche-terroche)
		dos << net_src_file;
		
		total_data_sz += sz;
	}
	
	uLog(NET, "serialized %zu files, %zu total bytes", n_files, total_data_sz);
}

//---- Send Breakpoint List ---------------------------------------------------

bool	Client::SendBreakpointList(const bool &enable_f)
{
	// pass serialized breakpoints to daemon
	StringList	bp_list;
	
	if (enable_f && m_Controller.GetClientState().GetGlobalBreakpointsFlag())			// bullshit!
		bp_list = m_Controller.GetSortedBreakpointList(1);
		
	// else breakpoints will be empty

	// post BREAKPOINTS message
	NotifyBreakpoints	notify_bp(bp_list);
	MemDataOutputStream	mos;
	
	mos << CLIENT_MSG::NOTIFY_BREAKPOINTS << notify_bp;
		
	bool	ok = SendMessage(mos);
	
	return ok;
}

//---- Is Connected ? ---------------------------------------------------------

bool	Client::IsConnected(void) const
{
	return m_ClientTCP.IsConnected();
}

//---- Check Connection -------------------------------------------------------

bool	Client::CheckConnection(void) const
{
	if (!m_ClientTCP.IsConnected())
	{	uErr("socket is already closed");
		return false;
	}
	
	return true;
}

//---- Check UDP Queue --------------------------------------------------------
	
void	Client::CheckUDPQueue(void)
{
	vector<vector<uint8_t>> buff;
	
	m_ClientTCP.ServiceUDPLogs(buff/*&*/);
	if (buff.empty())	return;
	
	// already sliced, post to message queue
	for (const auto &it : buff)
	{
		const bool	ok = m_TopFrame.QueueMessage(it, false/*delayed*/);
		if (!ok)
		{	uErr("error in Client::CheckUDPQueue(), QueueMessage() failed");
			return;
		}
	}
}
	
//---- Start Read Message ASYNC -----------------------------------------------

bool	Client::StartReadMessage_Async(void)
{
	if (!CheckConnection())		return false;
	
	const bool	ok = m_ClientTCP.StartReadMessage_Async();
	
	return ok;
}

//---- Has Pending Async ? ----------------------------------------------------

bool	Client::HasPendingAsync(void) const
{
	if (!CheckConnection())		return false;
	
	const bool	ok = m_ClientTCP.HasPendingAsync();
	
	return ok;
}
	
//---- Received Async Message ? -----------------------------------------------

READ_ASYNC_RES	Client::ReceivedAsyncMessage(vector<uint8_t> &buff)
{
	buff.clear();
	if (!CheckConnection())		return READ_ASYNC_RES::FAILED;			// should be LOUDER
	
	const READ_ASYNC_RES	res = m_ClientTCP.ReceivedAsyncMessage(buff/*&*/);
	return res;
}

//---- Read Message (blocking) ------------------------------------------------

bool	Client::ReadMessage(vector<uint8_t> &buff)
{
	return m_ClientTCP.ReadMessage(buff/*&*/);
}

//---- Send Message -----------------------------------------------------------
	
bool	Client::SendMessage(const MemoryOutputStream &mos)
{
	if (!CheckConnection())		return false;
	
	const bool	ok = LogOutgoingMessageType(mos, "sending ");
	assert(ok);
	
	return m_ClientTCP.WriteMessage(mos);
}

//---- Request & Reply (round-trip) -------------------------------------------

bool	Client::RequestReply(const MemoryOutputStream &mos)
{
	if (!CheckConnection())		return false;
	
	bool	ok = LogOutgoingMessageType(mos, "GetRequestReply() ");
	assert(ok);
	
	ok = m_ClientTCP.WriteMessage(mos);
	if (!ok)
	{	uErr("write failed");
		return false;
	}
	
	vector<uint8_t>	buff;
	
	ok = m_ClientTCP.ReadMessage(buff/*&*/);		// should use ASYNC here too?
	if (!ok)
	{	uErr("read failed");
		return false;
	}
	
	// post directly to message queue
	ok = m_TopFrame.QueueMessage(buff, false/*delayed*/);
	if (!ok)
	{	uErr("QueueMessage() failed");
		return false;
	}
	
	return ok;
}

//---- Network Stress Test ----------------------------------------------------

double	Client::NetStressTest(const size_t n, const size_t lo_sz, const double rnd_variance, const size_t frame_sz)
{	
	if (!CheckConnection())		return -1;
	
	assert(n > 0);
	assert(lo_sz > 0);
	assert((rnd_variance >= 0.0) && (rnd_variance < 1.0));
	
	const size_t	hi_sz = lo_sz * (1.0 + rnd_variance);
	
	if (frame_sz > 0)
	{	bool	ok = m_ClientTCP.SetOption(TCP_OPTION::RECEIVE_BUFFER_SIZE, frame_sz);
		assert(ok);
		ok = m_ClientTCP.SetOption(TCP_OPTION::SEND_BUFFER_SIZE, frame_sz);
		assert(ok);
		ok = m_ClientTCP.SetOption(TCP_OPTION::NO_DELAY, 1);
		assert(ok);
		if (!ok)		return -1;
	}
	
	static
	auto	rnd = bind(uniform_int_distribution<>(lo_sz, hi_sz), default_random_engine{});
	
	StopWatch	sw;
	int64_t		tot_data = 0;
	
	vector<uint8_t>	buff(hi_sz, 0);			// MAX BUFF
	
	for (int round = 0; round < n; round++)
	{
		const size_t	sz = rnd();
		
		uLog(NET, "StressTest(%zu / %zu) %zu bytes (0x%0X)", round, n, sz, sz);
		
		tot_data += sz;
	
		for (size_t i = 0; i < sz; i++)		buff[i] = i;
		
		// write test
		{	MemDataOutputStream	mos;
			
			mos << CLIENT_MSG::REQUEST_ECHO_DATA << (uint32_t) sz;
			mos.WriteSlice(&buff[0], &buff[0] + sz);
			
			const bool	ok = m_ClientTCP.WriteMessage(mos);
			assert(ok);
			if (!ok)	return -1;
			
			uLog(NET, " wrote out %zu", sz);
		}
		
		if (1)
		{	// read test
			vector<uint8_t>		msg_buff;		// TEMP BUFF
		
			bool	ok = ReadMessage(msg_buff/*&*/);
			assert(ok);
			if (!ok)	return -1;
			
			MemDataInputStream	mis(msg_buff);
			
			DAEMON_MSG	msg_t;
		
			mis >> msg_t;
			assert(DAEMON_MSG::REPLY_ECHO_DATA == msg_t);
		
			const size_t	reply_sz = mis.Read32();
			assert(reply_sz == sz);
			
			const vector<uint8_t>	data_buff = mis.ReadSTLBuffer();
			const size_t	data_sz = data_buff.size();
			assert(reply_sz == data_sz);
			
			for (int i = 0; i < sz; i++)
			{
				const uint8_t	c = buff[i];
				const uint8_t	check = data_buff[i];
				
				ok = (c == check);
				assert(ok);
				if (!ok)	return -1;
			}
			
			uLog(NET, " read back");
		}
	}
	
	const double	tot_secs = sw.elap_secs();
	const double	throughput = (double) tot_data / tot_secs;			// read/write = TWICE the throughput
	
	uLog(NET, "StressTest() DONE");
	
	return throughput;				// ok
}


// nada mas
