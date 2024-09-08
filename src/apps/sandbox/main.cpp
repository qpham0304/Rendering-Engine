
#include "../../core/features/AppWindow.h"
#include "../image-based-rendering/pbr_demo.h"
#include "../deferred-rendering/deferred_render_demo.h"
#include "../area-light/area_light_demo.h"
#include "../SSAO-demo/SSAO_demo.h"
#include "../volumetric-light/volumetricLightDemo.h"
#include "../SSR-demo/SSR_demo.h"
#include "../SSR-view-demo/SSR_view_demo.h"
#include "../deferred-IBL-demo/deferredIBL_demo.h"
#include "../ray-tracing/BasicRayTracing.h"
#include "../particle-demo/ParticleDemo.h"
#include "../../core/Application.h"

int main()
{
		auto demo0 = DemoPBR::run;
		auto demo1 = AreaLightDemo::run;
		auto demo2 = DeferredRender::run;
		auto demo3 = VolumetricLightDemo::run;
		auto demo4 = SSAO_Demo::run;
		auto demo5 = SSR_demo::run;
		auto demo6 = SSR_view_demo::run;
		auto demo7 = DeferredIBLDemo::run;
		auto demo8 = BasicRayTracing::run;
		auto demo9 = ParticleDemo::run;

		std::vector<std::function<int()>>list;
		list.push_back(demo0);
		list.push_back(demo1);	
		list.push_back(demo2);
		list.push_back(demo3);	// broken
		list.push_back(demo4);
		list.push_back(demo5);	// broken
		list.push_back(demo6);	// broken
		list.push_back(demo7);
		list.push_back(demo8);
		list.push_back(demo9);

	/* 
		an option for these apps to run independently, without the editor or layer system
		reason: for simplicity and compatability for apps that are not up to date with the changes
	*/
	#define USE_EDITOR

	#ifdef USE_EDITOR
		Application& app = Application::getInstance();
		app.run();

	#else
	try {
		AppWindow::init(PLATFORM_OPENGL);
		AppWindow::start("Rendering  Engine");
		AppWindow::renderScene(list[8]);
		AppWindow::end();
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Exception caught by main: " << e.what() << std::endl;
	}
	#endif
}