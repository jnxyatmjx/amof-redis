#ifndef H_RedisMgrSDF_H
#define H_RedisMgrSDF_H

#include <string>
#include <vector>
#include <unordered_map>

#include "connecPool.hpp"
#include "RedisConnect.h"

class RedisMgr
{
private:
	struct node{
		int port;
		std::string ip;
	};

public:
	explicit RedisMgr(std::string file);
	~RedisMgr();

	void initialize();
	void renewClusterSlots();
	bool pipeInsert_compatible(int, std::string& command, std::string& key, std::vector<std::pair<std::string, std::string>> &);
	void idlehearbt();
public:
	template<typename E>
	bool RedisExpress(size_t slot,const E& exe)
	{
		bool bReturn = false;

		connecPool<CMasterConnect>* pool = slots_map_[slot];
		auto pConn = pool->acquire();

		do
		{
			if (!pConn || !exe(pConn)) break;
			bReturn = true;
		} while (0);

		return bReturn;
	}
	std::string getCurrTime();
private:
	size_t getSlotFromKey(std::string key);
	std::unique_ptr<CMasterConnect, std::function<void(CMasterConnect*)> > getConncFromSlot(size_t slot);

private:
	void initializeSlotsCache();

	void s2node(std::string&,struct node&); //splic string ip:port to node struct
	void discoverMasterSlots(std::vector<struct nodeMaster>&);

	void reset();
	connecPool<CMasterConnect>* setupNewNodePool(const struct nodeMaster&);
	void assignSlotNodeCache(std::vector<struct nodeMaster>&);
private:
	std::string config_file;
	std::string passwrd_;
	int connec_size;
	bool rediscovering;//process updating the culster info
	int maxAttempt;
	std::vector<struct node> nodes_;

	std::unordered_map<std::string, connecPool<CMasterConnect>* > node_map_;
	std::unordered_map<int, connecPool<CMasterConnect>* > slots_map_;

	std::mutex lc_;
};


#endif