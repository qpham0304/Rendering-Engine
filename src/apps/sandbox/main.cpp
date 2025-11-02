
//#include "../vulkan-setup/VulkanSetupDemo.h"
#include "../../core/Application.h"
#include "../../src/apps/deferred-IBL-demo/deferredIBL_demo.h"

#define USE_EDITOR
int main()
{
	/* 
		an option for these apps to run independently, without the editor or layer system
		reason: for simplicity and compatability for apps that are not up to date with the changes
	*/

	#ifdef USE_EDITOR
	Application app;
	app.init();

	try {
		app.start();

		//TODO: problem: app layer can only be added after
		// everything is started so check that before allow adding layer
		app.pushLayer(new DeferredIBLDemo("demo"));

		app.run();
		app.end();
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Exception caught by main: " << e.what() << std::endl;
	}

	#else
	try {
		VulkanSetupDemo::run();
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Exception caught by main: " << e.what() << std::endl;
	}
	#endif
}