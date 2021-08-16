
#ifndef H_REdisCONNCEScc_H
#define H_REdisCONNCEScc_H

#include <string>

using namespace std;

struct redisReply;
struct redisContext;

struct nodeMaster{
	int slot_begin;
	int slot_end;
	int port;
	std::string ip;
};

class CRedisConnect
{
public:
	CRedisConnect();
	virtual ~CRedisConnect();

	virtual bool Heartbeat();
	virtual bool IsConnected();
	virtual bool IsEnable();
	virtual void Disconnect();
	virtual bool Connect(std::string strIp, int nPort, std::string passwd);
protected:
	void RedisFreeConnect();
	bool RedisConnect(const string& ip, int port);
	redisReply* RedisCommand(const char* cmd, const string& arg1="", const string& arg2="");
	redisContext* m_pCtx;
};

class CMasterConnect : public CRedisConnect
{
private:
	std::string ip_;
	int port_;
	std::string psaswd_;
public:
	bool Get(const string& Key, string& Val);
	bool Set(const string& Key, const string& Val);
    bool Set(const string& Key, const string& Val, int interval);
	bool Del(const string& Key);
	bool Expire(const string& Key, int interval);
	bool Auth(const string& pwd);
	virtual bool Connect(std::string strIp, int nPort, std::string passwd);

	void DiscoverClusterMaster(const std::string& key, std::vector<struct nodeMaster>& vcslo_/* int& slotbe, int&sloten, std::string& ip, int& port*/);
	int PipelineInsert(std::string& command, std::string& key,std::vector<std::pair<std::string, std::string>> &);
	int PipelineInsert_Result();
};

#endif