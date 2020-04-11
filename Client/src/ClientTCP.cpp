// ddt TCP client

#include <sstream>
#include <atomic>
#include <thread>
#include <future>
#include <chrono>

#include "ClientTCP.h"

// SILENCE warnings under Clang (gcc is a pain in the ass, as usual)
#if (defined(__clang__) && __clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wundef"
	#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#include "asio.hpp"

#if (defined(__clang__) && __clang__)
	#pragma GCC diagnostic pop
#endif

#include "lx/stream/MemoryOutputStream.h"
#include "lx/stream/MemDataOutputStream.h"

#include "logImp.h"
#include "ddt/sharedDefs.h"

using asio::ip::tcp;
using asio::ip::udp;

using namespace std;
using namespace LX;
using namespace DDT;
using namespace DDT_CLIENT;

#include "asio/error.hpp"

static const
unordered_set<asio::error_code>	s_KnownErrorSet
{
	asio::error::connection_refused,
	asio::error::eof,
	asio::error::broken_pipe,
	asio::error::connection_reset
};

/*
// base error enums (in error.hpp)

asio::placeholders::error ???

address_in_use
already_connected
access_denied
connection_aborted
connection_refused
connection_reset
host_unreachable
interrupted
network_down
network_reset
network_unreachable
no_permission
not_connected
shut_down
timed_out
would_block

message_size			// UDP message too long

*/

//---- Stream Status ----------------------------------------------------------

class StreamStatus
{
public:
	// ctor
	StreamStatus(const uint32_t &body_sz)
	{
		m_TotalBytes = body_sz;
		m_DoneBytes = 0;
	}
	
	void	Add(const size_t &sz)	{ m_DoneBytes += sz; }
	bool	IsDone(void) const	{ return (m_TotalBytes == m_DoneBytes); }
	size_t	Index(void) const	{ return m_DoneBytes; }
	size_t	Remain(void) const	{ return (m_TotalBytes - m_DoneBytes); }
	
private:
	
	int	m_TotalBytes;
	int	m_DoneBytes;
};

//---- Async Receiver ---------------------------------------------------------

	// could read SYNCHRONOUSLY here since are in 2nd thread ???

class AsyncTCPReceiver
{
public:
	// ctor
	AsyncTCPReceiver(asio::ip::tcp::socket &sock)
		: m_Socket(sock)
	{
		m_Buff.clear();
		m_ec = asio::error_code();
	}
	// dtor
	~AsyncTCPReceiver()
	{
		asio::io_service	&io_serv = m_Socket.get_io_service();
		
		io_serv.stop();
		
	}
	vector<uint8_t>	Tick(void)
	{
		m_Buff.clear();
		m_ec = asio::error_code();
		
		asio::io_service	&io_serv = m_Socket.get_io_service();
		
		io_serv.reset();
		
		const asio::error_code	ec = ReceiveHeader();
		if (ec)
		{	// on error arrives here
			assert(m_ec);
			uErr("Error in AsyncTCPReceiver::Tick()::bf io_serv.run(), aborting");
			return {};
		}
		
		io_serv.run();
		
		if (m_ec)
		{	uErr("Error in AsyncTCPReceiver::Tick()::bf io_serv.run(), aborting");
			return {};
		}
		
		return m_Buff;
	}
	
	asio::error_code	GetError(void) const
	{
		return m_ec;
	}

private:
	
	asio::error_code	ReceiveHeader(void)
	{	
		m_Header.Zap();
		
		// warning: ASIO can't capture locals in read handler lambda
		asio::async_read(m_Socket, asio::buffer(m_Header.Array()),			// could read SYNCHRONOUSLY since is in 2nd thread ?
			[&](asio::error_code ec, size_t sz)
			{
				if (ec || (sz != sizeof(m_Header.Array())) || !m_Header.IsOK())
				{	// error
					uErr("Error in AsyncTCPReceiver::ReceiveHeader(), (asio msg %s), size %zu vs %zu", ec.message(), sz, m_Buff.size());
					m_ec = ec;
					return;
				}
				
				const size_t	body_sz = m_Header.GetBodySize();
				assert(body_sz > 0);
				
				ReceiveBody(body_sz);
			});
		
		return m_ec;
	}
	
	void	ReceiveBody(const size_t &body_sz)
	{
		m_Buff.resize(body_sz, 0);
		
		asio::async_read(m_Socket, asio::buffer(m_Buff),
			[&](asio::error_code ec, size_t sz)
			{
				if (ec || (sz != m_Buff.size()))
				{	// error
					uErr("Error in AsyncTCPReceiver::ReceiveBody(), (asio msg %s), size %zu vs %zu", ec.message(), sz, m_Buff.size());
					m_ec = ec;
					return;
				}
				
				// done
			});
	}

	asio::ip::tcp::socket	&m_Socket;
	TcpHeader		m_Header;
	vector<uint8_t>		m_Buff;
	asio::error_code	m_ec;
};

//---- UDP server -------------------------------------------------------------

class udp_server
{
public:
	udp_server(asio::io_service &io_serv, const int &port)
		: m_sock(io_serv, udp::endpoint(udp::v4(), port))
	{
		m_DaemonMessages.clear();
	}
	
	bool	DoLoop(void)
	{
		uLog(MSG_UDP, "udp_server::DoLoop()");
		
		asio::io_service	&io_serv = m_sock.get_io_service();
		
		io_serv.reset();
		// io_serv.run();	
		
		for (;;)
		{
			asio::error_code	ec;
			const size_t	n = io_serv.poll_one(ec/*&*/);
			(void)n;
			if (ec)		return false;
			
			const bool	ok = ReceiveBulkMessage();
			if (!ok)	return false;
		}

		return true;	// ok
	}
	
	void	ServiceUDPLogs(vector<vector<uint8_t>> &buff)
	{
		buff.clear();
		
		// (in main thread here)
		lock_guard<mutex>	lock(m_Mutex);
		
		for (const auto &it : m_DaemonMessages)
			buff.push_back(it);

		m_DaemonMessages.clear();
	}

private:

	bool	ReceiveBulkMessage(void)
	{
		// uLog(MSG_UDP, "udp_server::ReceiveBulkMessage()");
		
		const asio::socket_base::message_flags	flags = 0;
		udp::endpoint				remote_endpoint;
		UdpHeader				header;
		asio::error_code			ec;
		// if ((m_sock.available(ec/*&*/) < 8))
		
		const size_t	hdr_sz = m_sock.receive_from(asio::buffer(header.Array()), remote_endpoint/*&*/, flags, ec/*&*/);
		if (ec || (hdr_sz != sizeof(header.Array())) || !header.IsOK())
		{	uErr("Error udp_server::ReceiveBulkMessage(), header size %zu vs %zu", hdr_sz, sizeof(header));
			return false;
		}
		
		const size_t	body_sz = header.GetBodySize();
		assert(body_sz > 0);
			
		vector<uint8_t>		buff(body_sz, 0);
		asio::mutable_buffer	muta_buf(&buff[0], body_sz);
		
		const size_t	rcv_sz = m_sock.receive_from(asio::buffer(muta_buf), remote_endpoint/*&*/, flags, ec/*&*/);
		if (ec)
		{	uErr("Error in udp_server::ReceiveBulkMessage(body), generic ASIO %S", ec.message());
			return false;
		}
		if (rcv_sz != body_sz)
		{	uErr("Error in udp_server::ReceiveBulkMessage(), body size %zu vs %zu", rcv_sz, body_sz);
			return false;
		}
		
		vector<vector<uint8_t>>	submessages;
		
		for (auto it = buff.begin(); it < buff.end(); )
		{
			const size_t	msg_sz = (it[0] << 24) + (it[1] << 16) + (it[2] << 8) + (it[3]);
			assert(msg_sz >= 0);
			
			it += 4;
			
			submessages.push_back({it, it + msg_sz});
			
			it += msg_sz;
		}
		
		lock_guard<mutex>	lock(m_Mutex);
		
		m_DaemonMessages.insert(m_DaemonMessages.end(), submessages.begin(), submessages.end());
		
		return true;
	}
	
	udp::socket		m_sock;
	
	mutex			m_Mutex;
	vector<vector<uint8_t>>	m_DaemonMessages;
};

//---- Client TCP implementation ----------------------------------------------

class ClientTCP: public IClientTCP
{
public:
	
	// ctor
	ClientTCP(Breather &breather, const int async_sleep_ms);
	// dtor
	virtual ~ClientTCP();
	
	bool	IsConnected(void) const override;
	bool	Connect(const string &host, const int port) override;
	void	Disconnect(void) override;
	
	bool	SetOption(const LX::TCP_OPTION typ, const int32_t val32) override;
	
	void	ServiceUDPLogs(vector<vector<uint8_t>> &buff) override;
	
	bool	StartReadMessage_Async(void) override;
	bool	HasPendingAsync(void) const override;
	READ_ASYNC_RES	ReceivedAsyncMessage(vector<uint8_t> &buff) override;
	
	bool	ReadMessage(vector<uint8_t> &buff) override;
	bool	WriteMessage(const MemoryOutputStream &mos) override;
	
	void	ClearCounters(void) override
	{
		m_ReadCnt = m_WriteCnt = 0;
	}
	
	size_t	GetReadCounter(void) const override
	{
		return m_ReadCnt;
	}
	
	size_t	GetWriteCounter(void) const override
	{
		return m_WriteCnt;
	}
	
	void	IncrementReadCnt(const size_t n)
	{
		m_ReadCnt += n;
	}
	
private:

	bool	CheckConnection(void) const;
	bool	IsError(const asio::error_code &ec, const bool log_f = true);
	void	ReadBodyAsync(const size_t sz);
	void	DumpErrorCode(const asio::system_error &e);
	
	asio::io_service	m_IOService;
	asio::ip::tcp::socket	m_Socket;
	
	AsyncTCPReceiver	*m_AsyncReceiver;
	future<vector<uint8_t>>	m_AsyncFuture;
	size_t			m_ReadCnt, m_WriteCnt;
	
	asio::io_service		m_udpService;
	udp_server			m_udp_listener;
	// future<asio::error_code>	m_UDPFuture;
	future<bool>			m_UDPFuture;
	
	vector<uint8_t>		m_OutBuff;				// BS ?
	Breather		&m_Breather;
	int			m_AsyncSleepMS;
};

//---- CTOR -------------------------------------------------------------------

	ClientTCP::ClientTCP(Breather &breather, const int async_sleep_ms)
		: m_Socket(m_IOService),
		m_udp_listener(m_udpService, 3002),
		m_Breather(breather),
		m_AsyncSleepMS(async_sleep_ms)
{
	m_AsyncReceiver = nil;
	
	// asio::socket_base::enable_connection_aborted	opt(true);	// not a settable option
	// m_Socket.set_option(opt);
	
	m_ReadCnt = m_WriteCnt = 0;
	
	m_UDPFuture = async(launch::async, [&](){return m_udp_listener.DoLoop();});
}

//---- DTOR -------------------------------------------------------------------

	ClientTCP::~ClientTCP()
{
	if (IsConnected())	Disconnect();
	
	// m_udp_listener = nil;
}

//---- Dump Error Code --------------------------------------------------------

void	ClientTCP::DumpErrorCode(const asio::system_error &e)
{
	asio::error_code	ec = e.code();
	const auto		e_val = ec.value();		// (is int)
	
	stringstream	ss;
	
	ss << "error code ";
	ss << "val(" << e_val << "), ";
	ss << "msg(" << ec.message() << ")";
	
	const string	ec_s = ss.str();
	
	uLog(NET, "ClientTCP::DumpErrorCode(%S)", ec_s);
}

//---- Is Connected ? ---------------------------------------------------------

bool	ClientTCP::IsConnected(void) const
{
	try
	{	const bool	f = m_Socket.is_open();
	
		return f;
	}
	catch (exception &e)
	{
		const string	what_s = e.what();
		
		uErr("ASIO exception %S in IsConnected()", what_s);
	}
	
	return false;
}

//---- Check Connection -------------------------------------------------------

bool	ClientTCP::CheckConnection(void) const
{
	if (IsConnected())		return true;
	
	uErr("ASIO socket is closed");
	
	return false;
}

//---- Is Error ? -------------------------------------------------------------

bool	ClientTCP::IsError(const asio::error_code &ec, const bool log_f)

	// may be in SECONDARY thread
{
	if (!ec)				return false;	// ok
	
	const string	ec_s = ec.message();
	
	if (!s_KnownErrorSet.count(ec))
	{	// unknown error code
		uFatal("ClientTCP::IsError() ILLEGAL/unhandled ASIO error type %S", ec_s);
	}
	else if (log_f)
	{	
		uErr("ClientTCP::IsError(%S)", ec_s);
	}
	
	if (m_Socket.is_open())
	{	// must be closed MANUALLY
		m_Socket.close();
		assert(!m_Socket.is_open());
	}
	
	return true;
}

//---- Read UDP Buffer --------------------------------------------------------

void	ClientTCP::ServiceUDPLogs(vector<vector<uint8_t>> &buff)
{
	if (!m_UDPFuture.valid())
	{
		uErr("error - invalid m_UDPFuture");
		return;
	}
	
	m_udp_listener.ServiceUDPLogs(buff/*&*/);
	if (buff.empty())	return;
	
	uLog(MSG_UDP, "ClientTCP::ServiceUDPLogs() got %zu entries", buff.size());
}

//---- Connect to Server ------------------------------------------------------

bool	ClientTCP::Connect(const string &host, const int port)
{
	uLog(NET, "ClientTCP::Connect(%s:%d)", host, port);
	
	/* This function is used to connect a socket to the specified remote endpoint.
	* The function call will block until the connection is successfully made or an error occurs.
	*
	* The socket is automatically opened if it is not already open. If the
	* connect fails, and the socket was automatically opened, the socket is
	* not returned to the closed state.*/
	
	// ec code prevents EXCEPTIONS :)
	
	try
	{	asio::error_code	ec;
		
		tcp::resolver	resolver(m_IOService);
		asio::connect(m_Socket, resolver.resolve({ tcp::v4(), host, to_string(port)}), ec);
		if (IsError(ec))	return false;
		
		uLog(NET, "client connected ok!");
		
		m_AsyncReceiver = new AsyncTCPReceiver{m_Socket};
		
		return true;		// ok
	}
	catch (asio::system_error &e)
	{
		if (IsError(e.code()))	return false;
		
		assert(0);
	}
	catch (exception &e)
	{
		const string	what_s = e.what();
		
		uFatal("ASIO exception %S in Connect()", what_s);
		
		// must be closed MANUALLY
		m_Socket.close();
		assert(!m_Socket.is_open());
	}
	
	return false;
}

//---- Disconnect from Server -------------------------------------------------

void	ClientTCP::Disconnect(void)
{
	uMsg("ClientTCP::Disconnect(m_AsyncReceiver is %c)", (nil != m_AsyncReceiver));
	uLog(NET, "ClientTCP::Disconnect()");
	
	if (IsConnected())
	{
		// shutdown both read & write
		asio::error_code	ec;

		m_Socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec/*&*/);
		// if (ec && (ec != asio::error::not_connected))			// if invert ec compare, MSVC barfs with: error C2039: 'not_connected' : is not a member of 'std::error_code' THOUGH NOT USING STD HERE!!
		if (ec)
		{	const string	ec_s = ec.message();
			uErr("error %S on m_Socket.shutdown()", ec_s);
		}
		
		ec = m_Socket.close(ec/*&*/);
		assert(!ec);
	
		const bool	f = m_Socket.is_open();
		assert(!f);
	}
	
	if (m_AsyncReceiver)
	{	delete m_AsyncReceiver;
		m_AsyncReceiver = nil;
	}
}

//---- Start Read Message ASYNC -----------------------------------------------

bool	ClientTCP::StartReadMessage_Async(void)
{
	if (!CheckConnection())		return false;
	assert(m_AsyncReceiver);
	
	m_AsyncFuture = async(launch::async,
		[&]()
		{
			// should set any error on future here?
			return m_AsyncReceiver->Tick();
		});
	
	return true;
}

//---- Has Pending Async operation ? ------------------------------------------

bool	ClientTCP::HasPendingAsync(void) const
{
	return m_AsyncFuture.valid();
}

//---- Received Async Message ? -----------------------------------------------

READ_ASYNC_RES	ClientTCP::ReceivedAsyncMessage(vector<uint8_t> &buff)
{
	if (!CheckConnection())		return READ_ASYNC_RES::FAILED;
	
	const auto	res = m_AsyncFuture.wait_for(chrono::milliseconds(m_AsyncSleepMS));
	if (res == future_status::ready)
	{	// future is ready, get result
		buff = m_AsyncFuture.get();
		asio::error_code	ec = m_AsyncReceiver->GetError();
		bool	ok = ((buff.size() > 0) && (!ec));
		if (ok)
		{	// done
			m_ReadCnt += buff.size();
			return READ_ASYNC_RES::DONE;
		}
		
		// error, identify if known
		IsError(ec);
		
		uErr("Error in ClientTCP::ReceivedAsyncMessage() (asio msg %S), size %zu", string(ec.message()), buff.size());
			
		// disconnect manually
		Disconnect();
		
		return READ_ASYNC_RES::FAILED;
	}
	else if (res == future_status::timeout)
	{	// not yet
		// const asio::error_code	ec = m_AsyncReceiver->GetError();
		// assert(!ec);
		
		return READ_ASYNC_RES::NOT_YET;
	}
	else
	{	uErr("UNKNOWN ERROR in ClientTCP::ReceivedAsyncMessage()");
		
		return READ_ASYNC_RES::FAILED;
	}
}

//---- Read Message (SYNCHRONOUS) ---------------------------------------------

bool	ClientTCP::ReadMessage(vector<uint8_t> &buff)
{
	buff.clear();								// could optimize mem alloc - FIXME
	
	if (!CheckConnection())		return false;
	
	try
	{	
		// header is synchronous
		asio::error_code	ec;
		
		while ((m_Socket.available(ec/*&*/) < 8) && !ec)
		{
			// check for close
			if (!IsConnected())		return false;			// socket was CLOSED (move to breathe() so can reloop?)
			
			// call breathe IMPLEMENTATION
			// bool	cont_f = m_Breather.Breathe();
			// if (!cont_f)			return false;
		}
		if (IsError(ec))	return false;
		
		TcpHeader	header;
		
		size_t	read_sz = asio::read(m_Socket, asio::buffer(header.Array()), ec/*&*/);
		if (IsError(ec))	return false;
		
		assert(sizeof(header.Array()) == read_sz);
		assert(header.IsOK());
		
		m_ReadCnt += read_sz;
		
		const uint32_t	body_sz = header.GetBodySize();
		
		StreamStatus	status(body_sz);
		
		while (!status.IsDone())
		{	if (!IsConnected())	return false;
			
			// get avail without blocking
			const size_t	avail = m_Socket.available();
			const int	to_read = (avail > body_sz) ? body_sz : avail;
			
			if (to_read > 0)
			{
				vector<uint8_t>	some_buff(to_read, 0);
			
				asio::error_code	err;
			
				read_sz = m_Socket.read_some(asio::buffer(&some_buff[0], to_read), err/*&*/);
				if (IsError(ec))	return false;
				assert(to_read == read_sz);
		
				buff.insert(buff.end(), some_buff.begin(), some_buff.end());		// BULLSHIT -- should MOVE buffer
				
				m_ReadCnt += read_sz;
				
				status.Add(read_sz);
				if (status.IsDone())	break;
			
				// uLog(NET, "enters sleep with %d read (%d remaining)", to_read, (int)status.Remain());
			}
			else
			{
				// uLog(NET, "entering sleep with ZERO read (%d remaining)", (int)status.Remain());
			}
			
			bool	ok = m_Breather.Breathe();
			assert(ok);
		}
		
		return true;
	}
	catch (exception &e)
	{
		const string	what_s = e.what();
		
		uFatal("ASIO exception %S in ReadMessage()", what_s);
	}
	
	return false;
}

//---- Write Message (SYNCHRONOUS) --------------------------------------------

bool	ClientTCP::WriteMessage(const MemoryOutputStream &mos)
{
	if (!IsConnected())	return false;
	
	const uint32_t	len = mos.GetLength();
	uLog(NET, "ClientTCP::WriteMessage(%d)", len);
	
	try
	{	asio::error_code	ec;
		const TcpHeader		header(len);
		
		// write header
		size_t	written_sz = asio::write(m_Socket, asio::buffer(header.Array()), ec/*&*/);
		if (IsError(ec))	return false;
		assert(sizeof(header.Array()) == written_sz);
		
		m_WriteCnt += written_sz;
		
		// copy message to own buffer (not needed unless async?)				// BULLSHIT - slow & not thread-safe
		m_OutBuff.resize(len, 0);
		
		mos.CloneTo(&m_OutBuff[0], len);							// not thread-safe ???
		
		StreamStatus	status(len);
		
		while (!status.IsDone())
		{	if (!IsConnected())	return false;
			
			// write body chunk
			written_sz = m_Socket.write_some(asio::buffer(&m_OutBuff[status.Index()], status.Remain()), ec/*&*/);
			if (IsError(ec))	return false;
			
			m_WriteCnt += written_sz;
			
			status.Add(written_sz);
			if (status.IsDone())	break;
			
			// uLog(NET, "enters sleep with %zu written", written_sz);
			
			const bool	ok = m_Breather.Breathe();
			assert(ok);
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

//---- Set Option -------------------------------------------------------------

bool	ClientTCP::SetOption(const LX::TCP_OPTION typ, const int32_t val32)
{
	// if (!IsConnected())	return false;
	
	try
	{	asio::error_code	ec;
	
		switch (typ)
		{
			case TCP_OPTION::RECEIVE_BUFFER_SIZE:
			{
				asio::socket_base::receive_buffer_size	opt(val32);
				m_Socket.set_option(opt, ec/*&*/);
			}	break;
			
			case TCP_OPTION::SEND_BUFFER_SIZE:
			{
				asio::socket_base::send_buffer_size	opt(val32);
				m_Socket.set_option(opt, ec/*&*/);
			}	break;
			
			case TCP_OPTION::NO_DELAY:
			{
				asio::ip::tcp::no_delay		opt(val32 != 0);	// (bool)
				m_Socket.set_option(opt, ec/*&*/);
			}	break;
			
			#if 0
			// not a settable socket options - returns "bad file descriptor error"
			
			case TCP_OPTION::CAN_ABORT_CONNECTION:
			{
				asio::socket_base::enable_connection_aborted	opt(val32 != 0);	
				
				m_Socket.set_option(opt, ec/*&*/);
			}	break;
			
			
			case TCP_OPTION::SOCKET_LINGER:
			{	
				asio::socket_base::linger	opt((val32 > 0), val32/*timeout*/);
				
				m_Socket.set_option(opt, ec/*&*/);
			}	break;
	
			#endif
			
			default:
				
				return false;
				break;
		}
		
		if (!ec)	return true;		// ok
		
		const string	ec_s = ec.message();
			
		uErr("ClientTCP::SetOption() error %S", ec_s);
			
		return false;
	}
	catch (exception &e)
	{
		const string	what_s = e.what();
		
		uErr("ASIO exception %S in SetOption()", what_s);
	}
	
	return false;
}

//---- INSTANTIATE ------------------------------------------------------------

IClientTCP*	IClientTCP::Create(Breather &breather, const int async_sleep_ms)
{
	return new ClientTCP(breather, async_sleep_ms);
}

// nada mas
