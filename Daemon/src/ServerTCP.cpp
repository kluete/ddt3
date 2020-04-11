// lua ddt daemon SERVER

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include <thread>
#include <chrono>
#include <future>
#include <array>
#include <mutex>
#include <atomic>

const int	DDT_UDP_MAX_MESSAGE_SIZE		= 65535;					// hardcoded by IP protocol ?

const int	DDT_UDP_LOG_REFRESH_INTERVAL_MS		= 1000;
const int	DDT_UDP_LOG_MAX_QUEUED_SIZE		= DDT_UDP_MAX_MESSAGE_SIZE * 0.5;
const int	DDT_UDP_LOG_MAX_QUEUED_COUNT		= 200;		

// silence warnings under Clang
#ifdef __clang__
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wundef"
	#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#include "asio.hpp"

#ifdef __clang__
	#pragma GCC diagnostic pop
#endif

#include "lx/stream/MemDataOutputStream.h"

#include "lx/xutils.h"

#include "ddt/sharedDefs.h"
#include "ddt/sharedLog.h"

#include "ServerTCP.h"

using namespace std;
using asio::ip::tcp;
using asio::ip::udp;

using namespace LX;
using namespace DDT;

//---- Server UDP -------------------------------------------------------------

class ServerUDP
{
public:
	// ctor
	ServerUDP()
		: m_Socket(m_IOService),
		m_CuedLogCnt(0),
		m_TimerCnt(0)
	{
		uLog(DTOR, "Daemon UDP CTOR");
		
		m_OutStreamBuff.reserve(DDT_UDP_MAX_MESSAGE_SIZE);
		
		// (resolution synchronous here)
		udp::resolver	resolv(m_IOService/*&*/);
		m_EndpointUDP = *resolv.resolve({udp::v4(), "localhost", "3002"});
		m_Socket.open(udp::v4());
	}
	
	~ServerUDP()
	{
		uLog(DTOR, "Daemon UDP DTOR");
		
		m_IOService.stop();
		m_Socket.close();
	}
	
	void	QueueLog(const timestamp_t stamp, const LogLevel level, const string &msg);
	
private:
	
	void	DoBroadcastNetLogs(void);

	asio::io_service	m_IOService;
	udp::socket		m_Socket;
	udp::endpoint		m_EndpointUDP;

	future<void>		m_TimerFuture;			// not used but still NECESSARY for async() spawn
	
	mutex			m_Mutex;
	vector<uint8_t>		m_OutStreamBuff;
	int			m_CuedLogCnt;
	
	int			m_TimerCnt;
};

// Stream Status (bullshit?)
class StreamStatus
{
public:
	// ctor
	StreamStatus(const uint32_t &body_sz)
	{
		m_TotalBytes = body_sz;
		m_DoneBytes = 0;
	}
	
	void	Add(const size_t &sz)
	{
		m_DoneBytes += sz;
		assert(m_DoneBytes <= m_TotalBytes);	// can't have read more than wanted
	}
	
	bool	IsDone(void) const
	{
		return (m_TotalBytes == m_DoneBytes);
	}
	
	size_t	Index(void) const
	{
		return m_DoneBytes;
	}
	
	size_t	Remain(void) const
	{
		const int	dB = (m_TotalBytes - m_DoneBytes);
		assert(dB >= 0);
		
		return dB;
	}
	
private:
	
	int	m_TotalBytes;
	int	m_DoneBytes;
};

//---- Server TCP -------------------------------------------------------------

class ServerTCP : public IServerTCP
{
public:
	// ctor
	ServerTCP(const int port, Breather &breather)
		: m_Port(port),
		m_Socket(m_IOService),
		m_Breather(breather)
	{
		uLog(DTOR, "Daemon TCP CTOR");
		
		ostringstream	os;
		os << this_thread::get_id();
		uLog(ASIO, "Thread id = %s", os.str());
	}
	
	// dtor
	virtual ~ServerTCP()
	{	uLog(DTOR, "ServerTCP DTOR");
	
		if (IsConnected())	Disconnect();
	}

	bool	IsConnected(void) const;

	string	GetHostname(void)
	{
		return asio::ip::host_name();
	}
	
	bool	WaitForConnection(void);
	bool	Disconnect(void);
	bool	SetOption(const LX::TCP_OPTION typ, const int32_t val32);
	
	bool	WriteMessage(const MemDataOutputStream &mos);
	bool	ReadMessage(vector<uint8_t> &buff);
	
	// imp
	void	QueueUDPLog(const timestamp_t stamp, const LogLevel level, const string &msg) override
	{
		m_UDP.QueueLog(stamp, level, msg);
	}
	
private:

	int			m_Port;
	asio::io_service	m_IOService;			// safe concurrency
	asio::ip::tcp::socket	m_Socket;
	vector<uint8_t>		m_OutBuff;			// bullshit?
	Breather		&m_Breather;
	
	ServerUDP		m_UDP;
};

//----- Wait for Connection ---------------------------------------------------

bool	ServerTCP::WaitForConnection(void)
{
	uLog(ASIO, "ServerTCP::WaitForConnection()");
		
	try
	{	uLog(ASIO, "  before acceptor instantiated");
		
		// may throw
		//   asio::error::address_in_use
		tcp::acceptor	acceptor(m_IOService, tcp::endpoint(tcp::v4(), m_Port));
		
		uLog(ASIO, "  after acceptor instantiated");
		
		asio::error_code	ec;
		
		acceptor.accept(m_Socket, ec/*&*/);
		if (ec)
		{	uErr("ASIO error %S on WaitForConnection::accept", ec.message());
			return false;
		}
		
		uLog(ASIO, "  received connection!");
		
		return true;
	}
	catch (exception &e)
	{
		uLog(EXCEPTION, "ASIO exception %S in WaitForConnection()", e.what());
	}
	
	return false;
}

//---- Is Connected ? ---------------------------------------------------------

bool	ServerTCP::IsConnected(void) const
{
	return m_Socket.is_open();
}

//---- Disconnect -------------------------------------------------------------

bool	ServerTCP::Disconnect(void)
{
	if (!IsConnected())	return false;
	
	uLog(ASIO, "ServerTCP::Disconnect()");
	
	// shutdown both read & write
	asio::error_code	ec;
	
	m_Socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec/*&*/);
	if (ec && (asio::error::not_connected != ec))
	{
		const string	ec_s = ec.message();
		uErr("error %S on m_Socket.shutdown()", ec_s);
	}
	
	ec = m_Socket.close(ec/*&*/);
	assert(!ec);
	
	const bool	f = m_Socket.is_open();
	assert(!f);
	
	return !f;
}

//---- Set Option -------------------------------------------------------------

bool	ServerTCP::SetOption(const LX::TCP_OPTION typ, const int32_t val32)
{
	uLog(ASIO, "ServerTCP::SetOption()");
	if (!IsConnected())	return false;
	
	asio::error_code	ec;
	
	switch (typ)
	{
		case TCP_OPTION::REUSE_ADDRESS:
		
			m_Socket.set_option(asio::socket_base::reuse_address(val32 != 0), ec/*&*/);	// (bool)
			break;
		
		case TCP_OPTION::RECEIVE_BUFFER_SIZE:
		
			m_Socket.set_option(asio::socket_base::receive_buffer_size(val32), ec/*&*/);
			break;
		
		case TCP_OPTION::SEND_BUFFER_SIZE:
		
			m_Socket.set_option(asio::socket_base::send_buffer_size(val32), ec/*&*/);
			break;
		
		case TCP_OPTION::NO_DELAY:
		
			
			m_Socket.set_option(asio::ip::tcp::no_delay(val32 != 0), ec/*&*/);		// (bool)
			break;
					
		default:
			
			// not implemented
			assert(false);
			return false;
			break;
	}
		
	if (!ec)	return true;		// ok

	uErr("ASIO in SetOption() %S", ec.message());
	return false;
}

//---- Read Message -----------------------------------------------------------

bool	ServerTCP::ReadMessage(vector<uint8_t> &buff)
{
	uLog(ASIO, "ServerTCP::ReadMessage()");
	
	try
	{	asio::error_code	ec;
		TcpHeader		header;
		
		#if 0
		while (m_Socket.available(ec/*&*/) < sizeof(header_data))
		{
			if (!IsConnected())		return false;			// socket was CLOSED (move to breathe() so can reloop?)
			
			// call breathe IMPLEMENTATION
			bool	cont_f = m_Breather->Breathe();
			if (!cont_f)			return false;
		}
		#endif
		
		// replaced read_some()
		size_t	len = asio::read(m_Socket, asio::buffer(header.Array()), ec/*&*/);			// compile FAIL under clang / libstdc++ ??
		if ((asio::error::eof == ec) || (asio::error::connection_reset == ec))
		{	// peer disconnected
			uLog(ASIO, "peer disconnected");
			Disconnect();
			return false;
		}
		else if (ec)
		{
			const string	ec_s = ec.message();
			
			uErr("ERROR ServerTCP::ReadMessage(%S)", ec_s);
			return false;
		}
		assert((sizeof(header.Array()) == len) && header.IsOK());
	
		const uint32_t	body_sz = header.GetBodySize();
		
		// read body directly into recipient
		buff.clear();
		
		// uLog(ASIO, "  expecting %d bytes", (int)body_sz);
				
		StreamStatus	status(body_sz);
		
		while (!status.IsDone())
		{	
			// get avail without blocking
			const size_t	avail = m_Socket.available();
			int	to_read = avail;
			if (to_read > status.Remain())	to_read = status.Remain();
			if (to_read <= 0)		to_read = 1;			// read at least 1 byte ??? -- FIXME
			
			if (to_read > 0)
			{
				vector<uint8_t>	some_buff(to_read, 0);
			
				asio::error_code	err;
			
				const size_t	read_sz = m_Socket.read_some(asio::buffer(&some_buff[0], to_read), err/*&*/);
				assert(to_read == read_sz);
		
				buff.insert(buff.end(), some_buff.begin(), some_buff.end());			// BULLSHIT
		
				status.Add(read_sz);
				if (status.IsDone())	break;
			
				uLog(ASIO, " enters sleep with %zu read (%zu remaining)", read_sz, status.Remain());
			}
			else
			{
				uLog(ASIO, " enters sleep with ZERO avail (%zu remaining)", status.Remain());
			}
			
			if (!IsConnected())		return false;			// socket was CLOSED (move to breathe() so can reloop?)
			
			// call IMPLEMENTATION
			bool	cont_f = m_Breather.Breathe();
			if (!cont_f)			return false;
		}
		
		return true;
	}
	catch (exception &e)
	{	// error
		const string	what_s = e.what();
		
		uLog(EXCEPTION, "ASIO exception %S in ReadMessage()", what_s);
	}
	
	return false;
}

//---- Write TCP Message ------------------------------------------------------

bool	ServerTCP::WriteMessage(const MemDataOutputStream &mos)
{
	try
	{	if (!IsConnected())		return false;
	
		const uint32_t	body_sz = mos.GetLength();
	
		uLog(ASIO, "ServerTCP::WriteMessage(size %d)", body_sz);
	
		const TcpHeader	header(body_sz);
		
		// write header
		asio::error_code	ec;
		
		size_t	written_sz = asio::write(m_Socket, asio::buffer(header.Array()), ec/*&*/);
		if ((asio::error::broken_pipe == ec) || (asio::error::connection_reset == ec))
		{	uErr("  connection dropped");
			Disconnect();
			return false;
		}
		assert(!ec);
		assert(sizeof(header.Array()) == written_sz);
		
		// copy message to own buffer - why ???
		m_OutBuff.resize(body_sz, 0);
	
		mos.CloneTo(&m_OutBuff[0], body_sz);
	
		// write body
		StreamStatus	status(body_sz);
		
		while (!status.IsDone())
		{	if (!IsConnected())		return false;
			
			// write body chunk
			written_sz = m_Socket.write_some(asio::buffer(&m_OutBuff[status.Index()], status.Remain()), ec/*&*/);
			if ((asio::error::broken_pipe == ec) || (asio::error::connection_reset == ec))
			{	uErr("  connection dropped");
				Disconnect();
				return false;
			}
			if (ec)
			{
				const string	ec_s = ec.message();
				uErr("  WriteMessage() failed with %S", ec_s);
			}
			assert(!ec);
			
			status.Add(written_sz);
			if (status.IsDone())		break;
			
			uLog(ASIO, " enters sleep with %zu written", written_sz);
			
			// call IMPLEMENTATION
			bool	cont_f = m_Breather.Breathe();
			if (!cont_f)			return false;
		}
		
		return true;
	}
	catch (exception &e)
	{
		const string	what_s = e.what();
		
		uFatal("ASIO exception %S in WriteMessage()", what_s);
	}
	
	return false;
}

//---- Queue UDP Log ----------------------------------------------------------

void	ServerUDP::QueueLog(const timestamp_t stamp, const LogLevel level, const string &msg)
{
	if (m_IOService.stopped())	return;			// was terminated
	
	MemDataOutputStream	mos;
	
	mos << DAEMON_MSG::NOTIFY_USER_LOG_SINGLE << stamp << level << msg;
	
	const uint32_t	len = mos.GetLength();
	
	lock_guard<mutex>	producer_lock(m_Mutex);
	
	{
		MemDataOutputStream	huge_mos(m_OutStreamBuff/*&*/);
	
		// append mos length
		huge_mos << len;
	
		// append mos
		huge_mos.Append(mos);
	}
	
	const size_t	queued_sz = m_OutStreamBuff.size();
	const int	queued_cnt = ++m_CuedLogCnt;
	
	const bool	force_flush_f = ((queued_sz > DDT_UDP_LOG_MAX_QUEUED_SIZE) || (queued_cnt > DDT_UDP_LOG_MAX_QUEUED_COUNT));
	
	uLog(MSG_UDP, "ServerTCP::QueueUDPLog(logs queued: %d, %zu bytes)", queued_cnt, queued_sz);			// log-within-log ???
	
	// check max UDP message size or will trigger asio::error::message_size
	if (force_flush_f)
	{	// force write
		// uLog(MSG_UDP, "  FORCED FLUSH");
		// ... while mutex is still locked
		DoBroadcastNetLogs();
		
		return;
	}
	
	// doesn't work unless do a get() on it first, hence is pointless?
	// if (!m_TimerFuture.valid())
	
	// const bool	restart_f = (0 == queued_cnt) && (!force_flush_f);
	if (0 == m_TimerCnt)
	{	// restart timer
		uLog(MSG_UDP, "  restarting async timer (cnt %d)", m_TimerCnt);
		
		m_TimerCnt++;
		
		// future must be STORED, even if unused
		m_TimerFuture = async(launch::async,
			[&]()
			{
				this_thread::sleep_for(chrono::milliseconds{DDT_UDP_LOG_REFRESH_INTERVAL_MS});
				
				lock_guard<mutex>	consumer_lock(m_Mutex);
				
				DoBroadcastNetLogs();
				
				m_TimerCnt--;
			});
	}
}

//---- Broadcasr UDP Logs -----------------------------------------------------

	// blocks on write but in 2nd thread so don't care :)
	
void	ServerUDP::DoBroadcastNetLogs(void)
{
	if (m_IOService.stopped())	return;			// was terminated
	
	const size_t	chunk_sz = m_OutStreamBuff.size();
	if (!chunk_sz)
	{	assert(0 == m_CuedLogCnt);
		return;
	}
	
	try
	{	uLog(MSG_UDP, "UDP broadcast (%d cued)", m_CuedLogCnt);
	
		const UdpHeader				header(chunk_sz);
		const asio::socket_base::message_flags	flags = 0;	// asio::socket_base::message_do_not_route;		// defined in asio/socket_base.hpp
		asio::error_code			ec;
		
		// write header
		size_t	written_sz = m_Socket.send_to(asio::buffer(header.Array()), m_EndpointUDP, flags, ec/*&*/);
		if (ec)
		{	const string	ec_s = ec.message();
			uErr("  m_SocketUDP.send_to(header) failed with %S", ec_s);
		}
		
		// sends to possibly UNCONNECTED socket (?)
		//   should cast to CONST since buffer is immutable ?
		written_sz = m_Socket.send_to(asio::buffer(m_OutStreamBuff, chunk_sz), m_EndpointUDP, flags, ec/*&*/);
		if (ec)
		{	uErr("  m_SocketUDP.send_to(body) failed with ASIO error %S", ec.message());
		}
		if (chunk_sz != written_sz)
		{	uErr("  m_SocketUDP.send_to(body) failed with size discrepancy");
		}
		
		m_OutStreamBuff.clear();
		
		uLog(MSG_UDP, "  wrote %zu bytes", written_sz);
	}
	catch (std::exception &e)
	{
		uLog(EXCEPTION, "ASIO exception in BroadcastNetLogs(): %S", e.what());
		return;
	}
	
	m_CuedLogCnt = 0;
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IServerTCP*	IServerTCP::Create(const int port, Breather &breather)
{
	return new ServerTCP(port, breather);
}

// nada mas
