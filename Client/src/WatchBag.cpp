// ddt client watch bag

#include "TopFrame.h"
#include "ProjectPrefs.h"

#include "WatchBag.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

//---- CTOR -------------------------------------------------------------------

	WatchBag::WatchBag(TopFrame *tf, ProjectPrefs *pprefs)
{
	assert(tf);
	assert(pprefs);
	
	m_TopFrame = tf;
	m_ProjectPrefs = pprefs;
	
	m_WatchSet.clear();
}

void	WatchBag::Clear(void)
{
	m_WatchSet.clear();
}

void	WatchBag::Set(DataInputStream &dis)
{
	m_WatchSet = StringList(dis).ToStringSet();
}

StringList	WatchBag::Get(void) const
{
	return m_WatchSet.ToStringList().Sort();
}

size_t	WatchBag::Count(void) const
{
	return m_WatchSet.size();
}

//---- Trim whitespaces -------------------------------------------------------

void	WatchBag::Trim(string &s)
{
	while (isspace(s.front()))	s.erase(s.begin());
	while (isspace(s.back()))	s.erase(s.end());
}

//---- Edit Watch Variable ----------------------------------------------------

void	WatchBag::Edit(string old_name, string name)
{
	Trim(old_name/*&*/);
	Trim(name/*&*/);
	
	if (!old_name.empty())	m_WatchSet.erase(old_name);
	if (!name.empty())	m_WatchSet.insert(name);
	
	m_ProjectPrefs->SetDirty();
}

//---- Add Watch --------------------------------------------------------------

bool	WatchBag::Add(const string &name)
{
	const size_t	bf = Count();
	
	Edit("", name);
	
	return (bf < Count());
}

bool	WatchBag::Remove(const string &name)
{
	const size_t	bf = Count();
	
	Edit(name, "");
	
	return (bf > Count());
}

// nada mas
