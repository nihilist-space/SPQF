/** 
 *  COPYRIGHT 2014 (C) Jason Volk
 *  COPYRIGHT 2014 (C) Svetlana Tkachenko
 *
 *  DISTRIBUTED UNDER THE GNU GENERAL PUBLIC LICENSE (GPL) (see: LICENSE)
 */


extern const char *const help_vote;             // help.cpp
extern const char *const help_vote_config;      // help.cpp
extern const char *const help_vote_kick;        // help.cpp
extern const char *const help_vote_mode;        // help.cpp


struct DefaultConfig : public Adoc
{
	static size_t configure(Adoc &doc);
	static Adoc configure(Chan &chan);

	DefaultConfig()
	{
		// config.vote
		put("max_active",16);
		put("max_per_user",2);
		put("min_votes",1);
		put("min_yay",1);
		put("min_turnout",0.00);
		put("duration",30);
		put("plurality",0.51);
	}
};


class Vote
{
  public:
	using id_t = size_t;
	enum Stat                                   { ADDED, CHANGED,                                   };
	enum Ballot                                 { YAY, NAY,                                         };

  private:
	const Sess &sess;
	Chans &chans;
	Users &users;

	id_t id;                                    // Index id of this vote
	Adoc cfg;                                   // Configuration of this vote
	time_t began;                               // Time vote was constructed
	std::string chan;                           // Name of the channel
	std::string user;                           // $a name of initiating user
	std::string issue;                          // "Issue" input of the vote
	std::set<std::string> yay;                  // Accounts voting Yes
	std::set<std::string> nay;                  // Accounts voting No

  public:
	virtual const char *type() const            { return "";                                        }
	auto &get_id() const                        { return id;                                        }
	auto &get_cfg() const                       { return cfg;                                       }
	auto &get_chan_name() const                 { return chan;                                      }
	auto &get_user_acct() const                 { return user;                                      }
	auto &get_chan() const                      { return chans.get(get_chan_name());                }
	auto &get_user() const                      { return users.get(get_user_acct());                }
	auto &get_began() const                     { return began;                                     }
	auto &get_issue() const                     { return issue;                                     }
	auto &get_yay() const                       { return yay;                                       }
	auto &get_nay() const                       { return nay;                                       }
	auto get_plurality() const                  { return cfg.get<float>("plurality");               }
	auto get_min_votes() const                  { return cfg.get<size_t>("min_votes");              }
	auto get_min_yay() const                    { return cfg.get<size_t>("min_yay");                }
	auto get_duration() const                   { return cfg.get<time_t>("duration");               }
	auto elapsed() const                        { return time(NULL) - get_began();                  }
	auto remaining() const                      { return get_duration() - elapsed();                }
	auto tally() const -> std::pair<uint,uint>  { return {yay.size(),nay.size()};                   }
	auto total() const                          { return yay.size() + nay.size();                   }
	auto plurality() const -> size_t            { return ceil(float(total()) * get_plurality());    }
	auto minimum() const                        { return std::max(get_min_votes(),get_min_yay());   }
	auto required() const                       { return std::max(get_min_yay(),plurality());       }
	bool disabled() const;

	friend Locutor &operator<<(Locutor &l, const Vote &v);    // Appends formatted #ID to channel stream

  protected:
	static constexpr auto &flush = Locutor::flush;

	auto &get_sess() const                      { return sess;                                      }
	auto &get_users()                           { return users;                                     }
	auto &get_chans()                           { return chans;                                     }

	// Subclass throws from these for abortions
	virtual void passed() {}
	virtual void failed() {}
	virtual void canceled() {}
	virtual void proffer(const Ballot &b, User &u) {}
	virtual void starting() {}

  public:
	// Called by Bot handlers
	Stat vote(const Ballot &ballot, User &user);

	// Called by the asynchronous Voting worker only
	void start(const size_t &num_votes, const size_t &num_for_chan, const size_t &num_for_user);
	void cancel();
	void finish();

	Vote(const id_t &id, const Sess &sess, Chans &chans, Users &users, Chan &chan, User &user, const std::string &issue, Adoc cfg = {});
	virtual ~Vote() = default;
};


inline
Vote::Vote(const id_t &id,
           const Sess &sess,
           Chans &chans,
           Users &users,
           Chan &chan,
           User &user,
           const std::string &issue,
           Adoc cfg):
sess(sess),
chans(chans),
users(users),
id(id),
cfg([&]() -> Adoc
{
	if(cfg.empty())
		return DefaultConfig::configure(chan);

	DefaultConfig::configure(cfg);
	return cfg;
}()),
began(time(NULL)),
chan(chan.get_name()),
user(user.get_acct()),
issue(issue)
{
	if(!user.is_logged_in())
		throw Exception("You must be registered with nickserv to create a vote.");
}


inline
void Vote::start(const size_t &num_votes,
                 const size_t &num_for_chan,
                 const size_t &num_for_user)
{
	using namespace colors;
	using limits = std::numeric_limits<size_t>;

	if(disabled())
		throw Exception("Votes of this type are disabled by the configuration.");

	if(num_for_chan > cfg.get("max_active",limits::max()))
		throw Exception("Too many active votes for this channel.");

	if(num_for_user > cfg.get("max_per_user",limits::max()))
		throw Exception("Too many active votes started by you on this channel.");

	starting();

	auto &chan = get_chan();
	chan << BOLD << "Voting has started!" << OFF << " Issue " << BOLD << "#" << get_id() << OFF << ": "
	     << "You have " << BOLD << get_duration() << OFF << " seconds to vote! "
	     << "Type: "
	     << BOLD << FG::GREEN << "!vote y" << OFF << " " << BOLD << get_id() << OFF
	     << " or "
	     << BOLD << FG::RED << "!vote n" << OFF << " " << BOLD << get_id() << OFF
	     << flush;
}


inline
void Vote::finish()
try
{
	using namespace colors;

	auto &chan = get_chan();

	if(total() < get_min_votes())
	{
		chan << (*this) << ": "
		     << "Failed to reach minimum number of votes: "
		     << BOLD << total() << OFF
		     << " of "
		     << BOLD << get_min_votes() << OFF
		     << " required."
		     << flush;

		failed();
		return;
	}

	if(yay.size() < get_min_yay())
	{
		chan << (*this) << ": "
		     << "Failed to reach minimum number of yes votes: "
		     << FG::GREEN << yay.size() << OFF
		     << " of "
		     << FG::GREEN << BOLD << get_min_yay() << OFF
		     << " required."
		     << flush;

		failed();
		return;
	}

	if(yay.size() < required())
	{
		chan << (*this) << ": "
		     << FG::WHITE << BG::RED << BOLD << "The nays have it." << OFF << "."
		     << " Yays: " << FG::GREEN << yay.size() << OFF << "."
		     << " Nays: " << FG::RED << BOLD << nay.size() << OFF << "."
		     << " Required at least: " << BOLD << required() << OFF << " yays."
		     << flush;

		failed();
		return;
	}

	chan << (*this) << ": "
	     << FG::WHITE << BG::GREEN << BOLD << "The yays have it." << OFF
	     << " Yays: " << FG::GREEN << BOLD << yay.size() << OFF << "."
	     << " Nays: " << FG::RED << nay.size() << OFF << "."
	     << flush;

	passed();
}
catch(const Exception &e)
{
	auto &chan = get_chan();
	chan << "The vote " << (*this) << " was rejected: " << e << flush;
	return;
}


inline
void Vote::cancel()
{
	canceled();

	Chan &chan = get_chan();
	chan << "The vote " << (*this) << " has been canceled." << flush;
}


inline
Vote::Stat Vote::vote(const Ballot &ballot,
                      User &user)
{
	if(!user.is_logged_in())
		throw Exception("You must be registered with nickserv to vote.");

	proffer(ballot,user);

	switch(ballot)
	{
		case YAY:
			if(!yay.emplace(user.get_acct()).second)
				throw Exception("You have already voted yay.");

			return nay.erase(user.get_acct())? CHANGED : ADDED;

		case NAY:
			if(!nay.emplace(user.get_acct()).second)
				throw Exception("You have already voted nay.");

			return yay.erase(user.get_acct())? CHANGED : ADDED;

		default:
			throw Exception("Ballot type not accepted.");
	}
}


inline
bool Vote::disabled()
const
{
	if(!cfg.has_child(type()))
		return false;

	return cfg.get_child(type()).get("disable","0") == "1";
}


inline
Locutor &operator<<(Locutor &locutor,
                    const Vote &vote)
{
	using namespace colors;

	locutor << "#" << BOLD << vote.get_id() << OFF;
	return locutor;
}


inline
Adoc DefaultConfig::configure(Chan &chan)
{
	Adoc cfg = chan.get("config.vote");

	if(configure(cfg))
	{
		Adoc chan_cfg;
		chan_cfg.put_child("config.vote",cfg);
		chan.set(chan_cfg);
	}

	return cfg;
}


inline
size_t DefaultConfig::configure(Adoc &cfg)
{
	static const DefaultConfig default_config;

	const std::function<size_t (const Adoc &def, Adoc &cfg)> recurse = [&]
	(const Adoc &def, Adoc &cfg) -> size_t
	{
		size_t ret = 0;
		for(const auto &pair : def)
		{
			const std::string &key = pair.first;
			const auto &subtree = pair.second;

			if(!subtree.empty())
			{
				Adoc sub = cfg.get_child(key,Adoc());
				ret += recurse(subtree,sub);
				cfg.put_child(key,sub);
			}
			else if(!cfg.has_child(key))
			{
				cfg.put_child(key,subtree);
				ret++;
			}
		}

		return ret;
	};

	return recurse(default_config,cfg);
}
