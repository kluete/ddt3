// ddt UDP client

#ifndef LUA_DDT_UDP_CLIENT_H_DEFINED
#define	LUA_DDT_UDP_CLIENT_H_DEFINED

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdint>

#include "asio.hpp"

#include "wx/wx.h"

#ifndef nil
	#define nil	nullptr
#endif

#if 0

//---- Sender Socket PURE VIRTUAL ---------------------------------------------	

class SenderSocket
{
public:
	// instantiator
	static SenderSocket*	New(const wxString &host, const int &port);
	
	// dtor
	virtual ~SenderSocket()	{}
	
	virtual bool	Send(const void	*buff, const size_t &sz) = 0;
	
protected:

	// PROTECTED ctor
	SenderSocket()		{}
};

#endif

#if 0

class AyncRequestor
{
public:
	// ctor
	AyncRequestor();
	// dtor
	virtual ~AyncRequestor();
	
	
};

void	do_request(asio::io_service &io_service, const string server, const string port_s)
#endif



#endif // LUA_DDT_UDP_CLIENT_H_DEFINED

// nada mas
