// Lua DDT message queue (much ado about nothing - remove?)

#include "lx/stream/MemDataOutputStream.h"

#include "ddt/MessageQueue.h"

using namespace std;
using namespace LX;

//---- Message CTOR vanilla  --------------------------------------------------

	Message::Message()
{
	m_Buff.clear();
}

//---- Message CTOR from MemoryOutputStream -----------------------------------

	Message::Message(const MemDataOutputStream &mos)
{
	// deep-copy
	const uint32_t	len = mos.GetLength();
	
	m_Buff.resize(len, 0);
	
	bool	ok = mos.CloneTo(&m_Buff[0], len);
	assert(ok);
}

//---- Message CTOR from vector<uint8_t> --------------------------------------

	Message::Message(const vector<uint8_t> &buff)
{
	size_t	src_sz = buff.size();
	assert(src_sz > 0);			// zero-size messages exist -- FIXME
	
	// is apparently deep-copy
	m_Buff = buff;
}

//---- CTOR -------------------------------------------------------------------

	MessageQueue::MessageQueue()
{
	m_Queue.clear();
}

//---- DTOR -------------------------------------------------------------------

	MessageQueue::~MessageQueue()
{		
}

//---- Put --------------------------------------------------------------------

void	MessageQueue::Put(const Message &m)
{
	unique_lock<mutex>	lock(m_Mutex);
	
	m_Queue.push_back(m);
}

//---- Count Pending Messages -------------------------------------------------

int	MessageQueue::NumPending(void)
{
	unique_lock<mutex>	lock{m_Mutex, try_to_lock_t{} };
	if (!lock.owns_lock())	return -1;			// couldn't lock mutex
	
	return m_Queue.size();
}

//---- Has Pending Message ? --------------------------------------------------

bool	MessageQueue::HasPending(void)
{
	int	n = NumPending();
	if (n == -1)	return false;				//  UNDEFINED
	
	return (n > 0);
}

//---- Get --------------------------------------------------------------------

bool	MessageQueue::Get(Message &m)
{
	unique_lock<mutex>	lock(m_Mutex);
	
	if (m_Queue.size() == 0)	return false;		// FAILED
	
	m = m_Queue.front();
	
	m_Queue.pop_front();
	
	return true;
}

// nada mas
