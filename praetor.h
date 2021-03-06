/** 
 *  COPYRIGHT 2014 (C) Jason Volk
 *  COPYRIGHT 2014 (C) Svetlana Tkachenko
 *
 *  DISTRIBUTED UNDER THE GNU GENERAL PUBLIC LICENSE (GPL) (see: LICENSE)
 */


class Schedule
{
	std::deque<std::tuple<id_t,time_t>> sched;

	void sort();

  public:
	static constexpr time_t max()   { return std::numeric_limits<int32_t>::max();               }

	bool empty() const              { return sched.empty();                                     }
	auto size() const               { return sched.size();                                      }
	id_t next_id() const            { return !empty()? std::get<id_t>(sched.at(0)) : 0;         }
	time_t next_abs() const         { return !empty()? std::get<time_t>(sched.at(0)) : max();   }
	time_t next_rel() const         { return !empty()? next_abs() - time(NULL) : max();         }

	void add(const id_t &id, const time_t &absolute);
	id_t pop();
};


class Praetor
{
	Bot &bot;
	Vdb &vdb;

	std::mutex mutex;
	std::atomic<bool> interrupted;
	std::condition_variable cond;
	Schedule sched;

  public:
	void add(const id_t &id, const time_t &absolute);
	void add(std::unique_ptr<Vote> &&vote);
	void add(const Adoc &serialized);

  private:
	bool process(Vote &vote) noexcept;
	bool process(const id_t &id);
	void process();
	void worker();
	std::thread thread;

	void init();
	std::thread init_thread;

  public:
	Praetor(Bot &bot, Vdb &vdb);
	~Praetor() noexcept;
};
