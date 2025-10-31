
#include "../vulkan-setup/VulkanSetupDemo.h"
#include "../../core/Application.h"

#define USE_EDITOR
int main()
{
	/* 
		an option for these apps to run independently, without the editor or layer system
		reason: for simplicity and compatability for apps that are not up to date with the changes
	*/

	#ifdef USE_EDITOR
	Application& app = Application::getInstance();
	try {
		app.run();
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