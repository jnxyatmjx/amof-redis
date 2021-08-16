
#include <stdlib.h>
#include "RedisMgr.h"

int main(int argc, char* argv[])
{
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
	RedisMgr* res_ = new RedisMgr("config.in");
	res_->initialize();

	//res_->pipeInsert_compatible();

	//hear beat
	while (1)
	{
		res_->idlehearbt();
	}

	return 0;
}