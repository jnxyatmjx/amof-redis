
#include <vector>
#include <cstdlib>
#include <iostream>
#include <cstring>

#include "RedisConnect.h"
#include "hiredis/hiredis.h"

#define REDIS_KEYWORD_IP		"ip"
#define REDIS_KEYWORD_PORT		"port"
#define REDIS_KEYWORD_NAME		"name"
#define REDIS_KEYWORD_SLAVES	"num-slaves"

#define REDIS_CMD_PING			"PING"
#define REDIS_CMD_PONG			"PONG"
#define REDIS_CMD_AUTH			"AUTH"
#define REDIS_CMD_GET			"GET"
#define REDIS_CMD_SET			"SET"
#define REDIS_CMD_DEL			"DEL"
#define REDIS_CMD_EXPIRE		"EXPIRE"

#define REDIS_CMD_SENTINEL		"SENTINEL"
#define REDIS_CMD_MASTER		"MASTER"

#define REDIS_CLUSTER			"CLUSTER"


#ifdef _MSC_VER
	#define strncasecmp _strnicmp
#else

#endif

__inline bool freeReplyObjSafe(redisReply* p)
{
	bool bReturn = p != NULL;
	if (p) freeReplyObject(p);
	return bReturn;
}

//从Reply中 读取Key的值到Val
bool ReadRedisReply(const redisReply* ReplyStringArray, const string& Key, string& Val)
{
	if (!ReplyStringArray)
		return false;

	//redisReply 类型必须是ARRAY
	if (REDIS_REPLY_ARRAY != ReplyStringArray->type)
		return false;

	for (size_t i = 0; i < ReplyStringArray->elements; ++i)
	{
		//ARRAY element 类型必须是STRING
		if (REDIS_REPLY_STRING == ReplyStringArray->element[i]->type)
		{
			if (Key == ReplyStringArray->element[i]->str && (i + 1)<ReplyStringArray->elements)
			{
				Val = ReplyStringArray->element[i + 1]->str;
				break;
			}
		}
	}

	return true;
}

bool ReadRedisReplyInt(const redisReply* ReplyStringArray, const string& Key, int& Val)
{
	string strVal;
	if (!ReadRedisReply(ReplyStringArray, Key, strVal))
		return false;

	Val = std::atoi(strVal.c_str());
	return Val >= 0;
}

CRedisConnect::CRedisConnect()
	: m_pCtx(NULL)
{

}

CRedisConnect::~CRedisConnect()
{
	std::cout << "destroy:" << m_pCtx << std::endl;
	RedisFreeConnect();
}

void CRedisConnect::RedisFreeConnect()
{
	if (m_pCtx)
	{
		redisFree(m_pCtx);
		m_pCtx = NULL;
	}
}

bool CRedisConnect::Heartbeat()
{
	return freeReplyObjSafe(RedisCommand(REDIS_CMD_PING));
}

bool CRedisConnect::IsConnected()
{
	return m_pCtx != NULL && m_pCtx->err == 0;
}

bool CRedisConnect::IsEnable()
{
	return m_pCtx != NULL && m_pCtx->err == 0;
}

void CRedisConnect::Disconnect()
{
	RedisFreeConnect();
}

bool CRedisConnect::RedisConnect(const string& ip, int port)
{
	if (m_pCtx)
		RedisFreeConnect();

	struct timeval tv = { 3, 0 };
	m_pCtx = redisConnectWithTimeout(ip.c_str(), port, tv);
	return m_pCtx != NULL && m_pCtx->err == 0;
}

redisReply* CRedisConnect::RedisCommand(const char* cmd, const string& arg1, const string& arg2)
{
	if (m_pCtx == NULL) return NULL;
	size_t ArgvLen[3] = { 0 };
	const char* Argv[3] = { 0 };
	int nArgc = 0;

	if (!cmd) return NULL;
	Argv[nArgc] = cmd;
	ArgvLen[nArgc] = strlen(cmd);
	nArgc++;

	if (!arg1.empty())
	{
		Argv[nArgc] = arg1.c_str();
		ArgvLen[nArgc] = arg1.length();
		nArgc++;
	}

	if (!arg2.empty())
	{
		Argv[nArgc] = arg2.c_str();
		ArgvLen[nArgc] = arg2.length();
		nArgc++;
	}

	redisReply* p = (redisReply*)redisCommandArgv(m_pCtx, nArgc, Argv, ArgvLen);

	if (NULL == p)
	{
		return NULL;
	}
	else if (p->type == REDIS_REPLY_ERROR)
	{
		freeReplyObject(p);
		return NULL;
	}

	return p;
}

bool CRedisConnect::Connect(std::string strIp, int nPort, std::string passwd)
{
	struct timeval timeout = { 1, 500000 };
	m_pCtx = redisConnectWithTimeout(strIp.c_str(), nPort, timeout);
	if (m_pCtx == NULL || m_pCtx->err)
	{
		printf("conne erros\n");
		if (m_pCtx)
		{
			redisFree(m_pCtx);
			m_pCtx = NULL;
		}
		return false;
	}

	if (!passwd.empty())
		return freeReplyObjSafe(RedisCommand(REDIS_CMD_AUTH, passwd));
	else
		return true;
}

bool CMasterConnect::Connect(std::string strIp, int nPort, std::string passwd)
{
	bool bReturn = false;
	redisReply* reply = NULL;

	ip_ = strIp;
	port_ = nPort;
	psaswd_ = passwd;
	do 
	{
		//链接MASTER
		if (!RedisConnect(strIp, nPort))
		{
			printf("REDIS-MASTER(%s:%d) CONNECT ERROR\n", strIp.c_str(), nPort);
			break;
		}

		//登陆MASTER
		if (!Auth(passwd))
		{
			printf("REDIS-MASTER(%s:%d) AUTH ERROR\n", strIp.c_str(), nPort);
			break;
		}

		bReturn = true;
	} while (0);
	
	if (reply) freeReplyObject(reply);
	return bReturn;
}

bool CMasterConnect::Get(const string& Key, string& Val)
{
	redisReply* p = RedisCommand(REDIS_CMD_GET, Key, Val);
	Val = (p && p->str) ? p->str : "";
	return freeReplyObjSafe(p);
}

bool CMasterConnect::Set(const string& Key, const string& Val)
{
	return freeReplyObjSafe(RedisCommand(REDIS_CMD_SET, Key, Val));
}

bool CMasterConnect::Set(const string& Key, const string& Val, int interval)
{
    return Set(Key, Val) && Expire(Key, interval);
}

bool CMasterConnect::Del(const string& Key)
{
	return freeReplyObjSafe(RedisCommand(REDIS_CMD_DEL, Key));
}

bool CMasterConnect::Expire(const string& Key, int intVal)
{
	string Val = std::to_string(intVal);
	return freeReplyObjSafe(RedisCommand(REDIS_CMD_EXPIRE, Key, Val));
}

bool CMasterConnect::Auth(const string& pwd)
{
	return freeReplyObjSafe(RedisCommand(REDIS_CMD_AUTH, pwd));
}

void CMasterConnect::DiscoverClusterMaster(const std::string& Key, std::vector<struct nodeMaster>& vcslo_/*int& slotbe,int&sloten,std::string& ip,int& port*/)
{	
	redisReply* ReplyStringArray = RedisCommand(REDIS_CLUSTER, Key, "");
	if (ReplyStringArray == NULL) return;
	if (ReplyStringArray->type != REDIS_REPLY_ARRAY) return;
	

	for (size_t i = 0; i < ReplyStringArray->elements; ++i)
	{
		if (REDIS_REPLY_ARRAY != ReplyStringArray->element[i]->type) continue;
		
		struct nodeMaster master_nodetemp;
		redisReply * subArrElem = ReplyStringArray->element[i];
	
		for (size_t j = 0; j < subArrElem->elements; j++)
		{	
			switch (j)
			{
				case 0:			//slot begin
					if (subArrElem->element[j]->type == REDIS_REPLY_INTEGER)
						master_nodetemp.slot_begin = static_cast<int>(subArrElem->element[j]->integer);
					break;
				case 1:			//slot end
					if (subArrElem->element[j]->type == REDIS_REPLY_INTEGER)
						master_nodetemp.slot_end = static_cast<int>(subArrElem->element[j]->integer);
					break;
				case 2:			//master info IP + Port
				{
					redisReply * subsubarr = subArrElem->element[j];
					for (size_t m = 0; m < subsubarr->elements; m++)
					{
						if (m == 0 && subsubarr->element[m]->type == REDIS_REPLY_STRING)
							master_nodetemp.ip = subsubarr->element[m]->str;

						if (m == 1 && subsubarr->element[m]->type == REDIS_REPLY_INTEGER)
							master_nodetemp.port = static_cast<int>(subsubarr->element[m]->integer);
					}
					break;
				}
				case 3:			//slave info 
					break;
			}
			
		}//end subarray	
		vcslo_.push_back(master_nodetemp);
	}

	//free resource
	freeReplyObjSafe(ReplyStringArray);
}

int CMasterConnect::PipelineInsert(std::string& command, std::string& key, std::vector<std::pair<std::string, std::string> >& kksevy_val)
{
	int inserNum = -1;
	if (kksevy_val.size() <= 0) return inserNum;

	std::size_t sitt = 0; 
	bool is2 = false;
	if (kksevy_val[0].second.empty()) // LPUSH KEY val1,val2,....
		sitt = 1 + 1 + kksevy_val.size();
	else
	{
		is2 = true;
		sitt = 1 + 1 + 2 * kksevy_val.size(); //HSET KEY  key1,val1, key2,val2, ....
	}

	std::size_t* ArgvLen = (std::size_t*)calloc(sitt, sizeof(std::size_t));
	const char** Argv = (const char**)calloc(sitt, sizeof(char*));
	int nArgc = 0;

	Argv[nArgc] = command.c_str();
	ArgvLen[nArgc] = command.length();
	nArgc++;

	Argv[nArgc] = key.c_str();
	ArgvLen[nArgc] = key.length();
	nArgc++;

	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = kksevy_val.begin(); it != kksevy_val.end(); it++)
	{
		Argv[nArgc] = it->first.c_str();
		ArgvLen[nArgc] = it->first.length();
		nArgc++;

		if (is2)
		{
			Argv[nArgc] = it->second.c_str();
			ArgvLen[nArgc] = it->second.length();
			nArgc++;
		}
	}

	inserNum = redisAppendCommandArgv(m_pCtx, nArgc, Argv, ArgvLen);

	//release
	if (ArgvLen){free(ArgvLen); ArgvLen = NULL;}
	if (Argv){free(Argv); Argv = NULL;}

	return inserNum;
}

/*
	-1 : that old cnonection is closed and other new connection has establish,reuse the connection owner class
	-2 : slot has "MOVED" to other node , should refresh the node-slot-connection information
	-3 : slot has "ASK"   slot is move fron node A to node B
	 
	 0 : success
	-5 : all others error , the caller just retry
*/
int CMasterConnect::PipelineInsert_Result()
{

	void* r = NULL; redisReply* reply = NULL;
	int res = redisGetReply(m_pCtx, &r);

	if (res != REDIS_OK || !r) {
		std::cout << "RedisCli::getCommAppendBatchResult Append error:"<< res << std::endl;
		if (r) 
		{
			printf("RedisCli::getCommAppendBatchResult Append libErr:%s\n", ((redisReply*)r)->str);
		}
		goto fail;
	}

	reply = ((redisReply*)r);
	if (reply->type == REDIS_REPLY_ERROR)
	{
		int errCode = 0;
		if (strncasecmp(reply->str, "MOVED", 5) == 0)
		{
			printf("MOVED!!!!! err:%s\n", reply->str);
			errCode = -2;
		}
		else
		{
			printf("pipeResultGet error type:%d err:%s\n", reply->type, reply->str);
			errCode = -5;
		}

		if (reply) { freeReplyObject(reply); reply = NULL; r = NULL; }
		return errCode;
	}
	/*else if (reply->type != REDIS_REPLY_STATUS || strncasecmp(reply->str, "OK",2) != 0)
	{
		std::cout << "RedisCli::getCommAppendBatchResult Append Type:" << reply->type << " error:" << reply->str << std::endl;
		if (reply) { freeReplyObject(reply); reply = NULL; r = NULL; }
	}*/

	if (reply) { freeReplyObject(reply); reply = NULL; r = NULL; }
	return 0;

fail:
	if (reply) 
	{
		freeReplyObject(reply); reply = NULL;
	}
	r = NULL;
	Connect(ip_, port_, psaswd_);
	return -1;
}