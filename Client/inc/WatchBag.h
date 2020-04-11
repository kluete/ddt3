// ddt client watch bag

#ifndef LUA_DDT_WATCH_BAG_H_INCLUDED
#define	LUA_DDT_WATCH_BAG_H_INCLUDED

#include <cstdint>

#include "ddt/sharedDefs.h"

namespace LX
{
	class DataInputStream;
	
} // namespace LX

namespace DDT_CLIENT
{

// forward references
class TopFrame;
class ProjectPrefs;

//---- Watch Bag --------------------------------------------------------------

class WatchBag
{
public:
	// ctors
	WatchBag(TopFrame *tf, ProjectPrefs *pprefs);
	
	void	Clear(void);
	void	Set(LX::DataInputStream &dis);
	StringList	Get(void) const;
	size_t		Count(void) const;
	
	bool	Add(const std::string &name);
	bool	Remove(const std::string &name);
	void	Edit(std::string old_name, std::string name);

private:

	void	Trim(std::string &s);
	
	TopFrame	*m_TopFrame;
	ProjectPrefs	*m_ProjectPrefs;
	
	StringSet	m_WatchSet;
};

} // namespace DDT_CLIENT

#endif // LUA_DDT_WATCH_BAG_H_INCLUDED

// nada mas
