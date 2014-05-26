#include <stdio.h>
#include <stdlib.h>
#include "server.h"

int main()
{
#ifdef _DEBUG
	putenv("xmpp_domain=qddevsrv.zonekey");
#endif // 

	fprintf(stdout, "zonekey AudioConferenceServer run....\n");

	Server server;
	server.run();

	fprintf(stdout, "===== End ==== \n");
	return 0;
}
