#include "VulkanSetupDemo.h"
#include "FollowDemo.h"
#include "example_glfw_vulkan.h"

int VulkanSetupDemo::run()
{
//#define RUN_GUI
#ifdef RUN_GUI
    EXAMPLE_GLFW_VULKAN example;
    example.run();

#else
    Demo app;
    app.run();

#endif

    return 0;
}