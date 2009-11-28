#include "BasicNamespace.h"

#include "debugging/debugging.h"
#include <list>

BasicNamespace::~BasicNamespace (void)
{
	ASSERT_MESSAGE(m_names.empty(), "namespace: names still registered at shutdown");
}

void BasicNamespace::attach (const NameCallback& setName, const NameCallbackCallback& attachObserver)
{
	std::pair<Names::iterator, bool> result = m_names.insert(Names::value_type(setName, m_uniqueNames));
	ASSERT_MESSAGE(result.second, "cannot attach name");
	attachObserver(NameObserver::NameChangedCaller((*result.first).second));
}

void BasicNamespace::detach (const NameCallback& setName, const NameCallbackCallback& detachObserver)
{
	Names::iterator i = m_names.find(setName);
	ASSERT_MESSAGE(i != m_names.end(), "cannot detach name");
	detachObserver(NameObserver::NameChangedCaller((*i).second));
	m_names.erase(i);
}

void BasicNamespace::makeUnique (const std::string& name, const NameCallback& setName) const
{
	char buffer[1024];
	name_write(buffer, m_uniqueNames.make_unique(name_read(name.c_str())));
	setName(buffer);
}

void BasicNamespace::mergeNames (const BasicNamespace& other) const
{
	typedef std::list<NameCallback> SetNameCallbacks;
	typedef std::map<std::string, SetNameCallbacks> NameGroups;
	NameGroups groups;

	UniqueNames uniqueNames(other.m_uniqueNames);

	for (Names::const_iterator i = m_names.begin(); i != m_names.end(); ++i) {
		groups[(*i).second.getName()].push_back((*i).first);
	}

	for (NameGroups::iterator i = groups.begin(); i != groups.end(); ++i) {
		name_t uniqueName(uniqueNames.make_unique(name_read((*i).first.c_str())));
		uniqueNames.insert(uniqueName);

		char buffer[1024];
		name_write(buffer, uniqueName);

		SetNameCallbacks& setNameCallbacks = (*i).second;

		for (SetNameCallbacks::const_iterator j = setNameCallbacks.begin(); j != setNameCallbacks.end(); ++j) {
			(*j)(buffer);
		}
	}
}

BasicNamespace g_defaultNamespace;
BasicNamespace g_cloneNamespace;
