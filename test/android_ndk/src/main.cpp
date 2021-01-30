

#include <wawo.h>

int main(int argc, char** argv) {
	{
		printf("print app init begin");
		netp::app _app;
		printf("print app init done");
		//NETP_INFO("[main]begin ...");

		//while(1) {
			//netp::this_thread::sleep(1);
		//}
		
		_app.run();
		
		NETP_INFO("[main]exit ...");
	}
	NETP_INFO("[main]exit done");
	return 0;
}