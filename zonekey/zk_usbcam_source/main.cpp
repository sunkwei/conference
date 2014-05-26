#include <stdio.h>
#include <stdlib.h>
#include "USBCameraZqpktSource.h"
#include <cc++/thread.h>

int main(int argc, char **argv)
{
	USBCameraZqpktSource *source = new USBCameraZqpktSource(0, 4000);
	int rc = source->Start();
	if (rc == 0) {
		ost::Thread::sleep(1000000);	// 10Ãë
		source->Stop();
	}

	return 0;
}
