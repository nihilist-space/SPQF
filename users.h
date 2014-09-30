/** 
 *  COPYRIGHT 2014 (C) Jason Volk
 *  COPYRIGHT 2014 (C) Svetlana Tkachenko
 *
 *  DISTRIBUTED UNDER THE GNU GENERAL PUBLIC LICENSE (GPL) (see: LICENSE)
 */


class Users
{
	Sess &sess;
	std::unordered_map<std::string, User> users;

  public:
	using ConstClosure = std::function<void (const User &)>;
	using Closure = std::function<void (User &)>;

	const User &get(const std::string &nick) const;
	bool has(const std::string &nick) const            { return users.count(nick);                  }
	size_t num() const                                 { return users.size();                       }

	void for_each(const ConstClosure &c) const;
	void for_each(const Closure &c);

	User &get(const std::string &nick);
	User &add(const std::string &nick);
	bool del(const User &user);

	Users(Sess &sess);

	friend std::ostream &operator<<(std::ostream &s, const Users &u);
};


inline
Users::Users(Sess &sess):
sess(sess)
{


}


inline
bool Users::del(const User &user)
{
	const std::string &nick = user.get_nick();
	return users.erase(nick);
}


inline
User &Users::add(const std::string &nick)
{
	const auto &iit = users.emplace(std::piecewise_construct,
	                                std::forward_as_tuple(nick),
	                                std::forward_as_tuple(sess,nick));
	User &user = iit.first->second;

	if(iit.second)
		user.who();

	return user;
}


inline
User &Users::get(const std::string &nick)
try
{
	return users.at(nick);
}
catch(const std::out_of_range &e)
{
	throw Exception("User not found");
}


inline
const User &Users::get(const std::string &nick)
const try
{
	return users.at(nick);
}
catch(const std::out_of_range &e)
{
	throw Exception("User not found");
}


inline
void Users::for_each(const Closure &closure)
{
	for(auto &userp : users)
	{
		User &user = userp.second;
		closure(user);
	}
}


inline
void Users::for_each(const ConstClosure &closure) const
{
	for(const auto &userp : users)
	{
		const User &user = userp.second;
		closure(user);
	}
}


inline
std::ostream &operator<<(std::ostream &s,
                         const Users &u)
{
	s << "Users(" << u.num() << ")" << std::endl;
	for(const auto &userp : u.users)
	{
		const User &user = userp.second;
		s << user << std::endl;
	}

	return s;
}
