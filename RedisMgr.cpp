
#include "RedisMgr.h"
#include "crc16.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <set>
#include <assert.h>

#define REDIS_SERVER_SLOTS		16384

RedisMgr::RedisMgr(std::string pkfile)
:config_file(pkfile), passwrd_(""), connec_size(-1), rediscovering(false),
maxAttempt(5)
{
}

RedisMgr::~RedisMgr()
{
}

/*
#Config file must Unix LF format
#Value behind symbol "=" NONEED space
#[Redis]
pwd=test123
consize=3
12.34.56.78:6001

#[GreyUrl]
#url=http://example/Api/haode/dawang
url=https://www.baidu.com

*/

void RedisMgr::initialize()
{
	//read config file
	std::ifstream inf(config_file, std::ios::binary | std::ios::in);
	bool isCfg = false;
	while (inf.good() && !inf.eof())
	{
		std::string vss;
		std::getline(inf, vss); //split from \n

		if (vss.find("#[Redis]") == 0)
		{
			isCfg = true;
			continue;
		}
		else if (isCfg && vss.find("#[") == 0)
		{
			isCfg = false;
			break;
		}
		if(isCfg == false || vss.length() <= 3)	
			continue;

		if (vss.find("pwd=") == 0)
		{
			std::size_t cur = vss.find("=");
			std::size_t len = vss.length();
			passwrd_ = vss.substr(cur + 1);
			continue;
		}
		if (vss.find("consize=") != std::string::npos)
		{
			std::size_t cur = vss.find("=");
			std::size_t len = vss.length();
			connec_size = std::atoi( vss.substr(cur + 1, len - cur -1).c_str() );
			continue;
		}

		struct node nod_temp;
		s2node(vss, nod_temp);
		nodes_.push_back(nod_temp);
	}
	inf.close();

	initializeSlotsCache();
}

void RedisMgr::initializeSlotsCache()
{
	//get all master node information
	std::vector<struct nodeMaster> slot_vec_;
	discoverMasterSlots(slot_vec_);

	assignSlotNodeCache(slot_vec_);
}

void RedisMgr::renewClusterSlots()
{
	if (rediscovering) return; //if updating the cluster info ,just return
	
	{
		std::lock_guard<std::mutex> lock(lc_);
		if (rediscovering == false) rediscovering = true;
	}

	std::vector<struct nodeMaster> slot_vec_;
	discoverMasterSlots(slot_vec_);

	assignSlotNodeCache(slot_vec_);

	/*{
		std::lock_guard<std::mutex> lock(lc_);
		rediscovering = false;
	}*/
}

void RedisMgr::s2node(std::string& srcs,struct node& nod_)
{
	size_t lens = srcs.length();
	if (lens <= 0) return;
	size_t i = 0;
	while (i < lens && srcs.at(i) != ':') i++;

	nod_.ip = srcs.substr(0, i);
	i += 1;
	nod_.port = std::atoi(srcs.substr(i,lens-i).c_str());
}

void RedisMgr::discoverMasterSlots(std::vector<struct nodeMaster>& vcslo_)
{
	for (const auto& ndd_ : nodes_)
	{
		try{
			std::unique_ptr<CMasterConnect> clus_tmp_conn_(new CMasterConnect());
			if (!clus_tmp_conn_->Connect(ndd_.ip, ndd_.port, passwrd_))
			{
				std::cout << "Connec " << ndd_.ip << ":" << ndd_.port << " err " << getCurrTime() << std::endl;
				continue;
			}

			clus_tmp_conn_->DiscoverClusterMaster("SLOTS", vcslo_);
			break;

		}
		catch (...)
		{
			continue;
		}
	}//end all redis nodes
}

void RedisMgr::reset()
{
	std::lock_guard<std::mutex> lock(lc_);

	for (auto & pit_ : node_map_)
	{
		pit_.second->destroy();
		if (pit_.second)
		{
			delete pit_.second;
			pit_.second = NULL;
		}
	}

	node_map_.clear();
	slots_map_.clear();
}

connecPool<CMasterConnect>* RedisMgr::setupNewNodePool(const struct nodeMaster& msnode)
{
	//std::lock_guard<std::mutex> lock(lc_); //fix me ??

	connecPool<CMasterConnect>* ptr_ = new connecPool<CMasterConnect>(connec_size);
	ptr_->initialize();
	size_t sizsl = ptr_->size();
	for (size_t i = 0; i < sizsl; i++)
	{
		ptr_->acquire()->Connect(msnode.ip,msnode.port,passwrd_);
	}
	
	return ptr_;
}

void RedisMgr::assignSlotNodeCache(std::vector<struct nodeMaster>& slot_vec_)
{
	std::lock_guard<std::mutex> lock(lc_);

	for (auto & pit_ : node_map_)
	{
		if (pit_.second != NULL)
		{
			pit_.second->destroy();
			delete pit_.second;
			pit_.second = NULL;
		}
	}

	node_map_.clear();
	slots_map_.clear();

	for (const auto& msnode : slot_vec_)
	{
		std::string mkey = msnode.ip + ":" + std::to_string(msnode.port);
		
		//IP:Port --> pool . normally 1 IP:Port may associate with multiple nodes
		//BUT let 1 IP:Port only associate with 1 node enforcement
		if (node_map_.count(mkey) <= 0)
		{
			connecPool<CMasterConnect>* ptr_ = setupNewNodePool(msnode);
			node_map_.insert(std::pair<std::string, connecPool<CMasterConnect>*>(mkey, ptr_));
		}

		//slot --> pool . 1 slot must only associate with 1 node
		assert(node_map_.count(mkey) > 0);
		connecPool<CMasterConnect>* ptr_slot_ = node_map_[mkey];
		for (int i = msnode.slot_begin; i <= msnode.slot_end; i++)
			slots_map_[i] = ptr_slot_;
	}

	rediscovering = false;
}

bool RedisMgr::pipeInsert_compatible(int slotsx, std::string& command, std::string& key, std::vector<std::pair<std::string, std::string>> & kksevy_val)
{
	std::size_t slot = getSlotFromKey(key);
	int attemptLeft = maxAttempt;

	for (; attemptLeft > 0; attemptLeft--)
	{
		auto pTyx = getConncFromSlot(slot);

		//having updating the slotnode information just wait a moment
		if (pTyx == NULL)
		{
			attemptLeft++;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		int iedNum = pTyx->PipelineInsert(command, key, kksevy_val);
		int resCod = pTyx->PipelineInsert_Result();

		if (resCod == 0) break;
		else if (resCod == -1)
		{
			continue;
		}
		else if (resCod == -2) //MOVED
		{
			pTyx.reset();
			std::cout << __FUNCTION__ << " MOVED start " << getCurrTime() << std::endl;
			renewClusterSlots();
			std::cout << __FUNCTION__ << " MOVED success " << getCurrTime() << std::endl;
			continue;
		}

	}//end retry

	if (attemptLeft > 0) 
		return true;
	else
		return false;
}


size_t RedisMgr::getSlotFromKey(std::string key)
{
	return crc16(key.c_str(), strlen(key.c_str())) % (REDIS_SERVER_SLOTS);
}


std::unique_ptr<CMasterConnect, std::function<void(CMasterConnect*)>>  RedisMgr::getConncFromSlot(size_t slot)
{
	std::lock_guard<std::mutex> losck(lc_);

	if (slots_map_.count(slot) <= 0)
	{
		std::cout << __FUNCTION__ << " pipeInsert_compatible Slot error " << getCurrTime() << std::endl;
		return NULL;
	}

	connecPool<CMasterConnect>* pool = slots_map_[slot];
	std::unique_ptr<CMasterConnect, std::function<void(CMasterConnect*)>> pTyx = pool->acquire();

	if (pTyx == NULL)
	{
		std::cout << __FUNCTION__ << " the connecPool may be update cluster info ,wait a moment and try again later " << getCurrTime() << std::endl;
		return NULL;
	}

	return std::move(pTyx);
}

void RedisMgr::idlehearbt()
{
	std::lock_guard<std::mutex> losck(lc_);

	std::unordered_map<std::string, connecPool<CMasterConnect>* >::const_iterator it = node_map_.begin();
	for (; it != node_map_.end(); it++)
	{
		std::vector< std::unique_ptr<CMasterConnect, std::function<void(CMasterConnect*)>> > rce_tmp =
			it->second->acquireIdls();

		for (const auto& m : rce_tmp)
		{
			m->Heartbeat();
		}
	}
	std::cout << __FUNCTION__ << " Heart Beat " << getCurrTime() << std::endl;
}

std::string RedisMgr::getCurrTime()
{
	struct tm Tm;
	time_t curSec = time(0);
	localtime_r(&curSec, &Tm);
	char tbuf[128] = { 0 };
	sprintf(tbuf, "%04d/%02d/%02d %02d:%02d:%02d", Tm.tm_year + 1900, Tm.tm_mon + 1, Tm.tm_mday,
		Tm.tm_hour,Tm.tm_min,Tm.tm_sec);
	return std::move(std::string(tbuf));
}