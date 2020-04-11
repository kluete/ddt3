// ddt UDP client

#include <cstring>
#include <stdexcept>

#include "wx/wx.h"

#include "udpClient.h"

#include "asio.hpp"

using asio::ip::udp;

const size_t	MAX_RECEIVE_SIZE = 1024;

//---- ASIO udp ---------------------------------------------------------------

class SenderSocketAsio: public SenderSocket
{
public:
	// ctor
	SenderSocketAsio(const wxString &host, const int &port)
		: SenderSocket(), m_Socket(m_IOService, udp::endpoint(udp::v4(), 0))
	{
		wxLogMessage("SenderSocketAsio::CTOR");
		
		const wxString	port_s = wxString::Format("%d", port);
		
		// resolution is sync or async ?
		udp::resolver	resolv(m_IOService);
		
		// MUST convert wxString or compiler will (misleadingly) complain "there's no initializer list"
		m_Endpoint = *resolv.resolve({udp::v4(), host.ToStdString(), port_s.ToStdString()});
	}
	
	// dtor
	virtual ~SenderSocketAsio()
	{
		// no need for any explicit release?
	}
	
	// IMPLEMENTATION
	bool	Send(const void	*buff, const size_t &sz)
	{
		wxASSERT(buff);
		wxASSERT(sz > 0);
		
		try
		{	// (doesn't wait for send completion)
			m_Socket.send_to(asio::buffer(buff, sz), m_Endpoint);
		}
		catch (std::exception &e)
		{
			const wxString	s = wxString::Format("SenderSocketAsio::Send() exception \"%s\"", e.what());
			wxFAIL_MSG(s);
			
			return false;
		}
		
		return true;
	}
	
	// BRICO-JARDIN
	size_t	Receive(void)
	{
		m_InBuffer.resize(MAX_RECEIVE_SIZE, 0);
		
		asio::ip::udp::endpoint	sender_endpoint;
		
		size_t	reply_length = m_Socket.receive_from(asio::buffer(&m_InBuffer[0], m_InBuffer.size()), sender_endpoint);
		
		wxLogMessage("Reply is: \"%s\"", wxString(&m_InBuffer[0], reply_length));

		return reply_length;	
	}
	
private:
	
	std::vector<uint8_t>		m_InBuffer;
	asio::io_service		m_IOService;
	asio::ip::udp::socket		m_Socket;
	asio::ip::udp::endpoint		m_Endpoint;
};

//---- Sender Socket NEW() instantiator ---------------------------------------

// static
SenderSocket*	SenderSocket::New(const wxString &host, const int &port)
{
	SenderSocket	*send_sock = new SenderSocketAsio(host, port);
	wxASSERT(send_sock);
	
	return send_sock;
}

#if 0

#include <array>
#include <future>
#include <iostream>
#include <thread>
#include <asio/io_service.hpp>
#include <asio/ip/udp.hpp>
// #include <asio/use_future.hpp>	// not found ?

using namespace std;

using asio::ip::udp;

void	do_request(asio::io_service &io_service, const string server, const string port_s)
{
	try
	{
		udp::resolver	resolver(io_service);
	
	// async DNS with future
	// - I need to solve DNS only once ? 

		// this async_resolve returns the endpoint iterator as a future value that isn't retrieved (locked) until...
		std::future<udp::resolver::iterator>	iter = resolver.async_resolve( {udp::v4(), server, port_s}, asio::use_future);

		udp::socket		socket(io_service, udp::v4());
		
		// sends 1 byte for request ?
		std::array<char, 1>	send_buf = {{ 0 }};
	
	// sends 1 byte to host(s)
	// can block until DNS resolved ?
	// receives other future
	
		// ... here. This call may block
		std::future<std::size_t>	send_length = socket.async_send_to(asio::buffer(send_buf), *iter.get(), asio::use_future);

		// (do other stuff while send completes)

	// waits on future until send complete

		send_length.get();	// blocks until send complete (throws any errors)
		
	// starts read, gets another future

		std::array<char, 128>	recv_buf;
		udp::endpoint		sender_endpoint;

		std::future<std::size_t> recv_length = socket.async_receive_from(asio::buffer(recv_buf), sender_endpoint, asio::use_future);

		// (do other stuff while send completes)
	
	// waits on future
	// blocks until all data received
	// writes incoming to cout
	
		std::cout.write(recv_buf.data(), recv_length.get());	// block until receive complete
	}
	catch (std::system_error& e)
	{
		std::cerr << e.what() << std::endl;
	}
}

//---- Init Request -----------------------------------------------------------

bool	InitRequest(const string server, const string port_s)
{
	asio::io_service		m_IOService;
	asio::ip::udp::socket		m_Socket;
	asio::ip::udp::endpoint		m_Endpoint;
	try
	{
		// we run the io_service off its own thread so it operates completely asynchronous with respect to the rest of the program
		asio::io_service		io_service;
		asio::io_service::work		work(io_service);
		std::thread			thread([&io_service](){ io_service.run(); });
		
		// could loop here?
		do_request(io_service, server, port_s);

		io_service.stop();
		
		// wait for threads to complete
		thread.join();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return true;
}

#endif


// nada mas
