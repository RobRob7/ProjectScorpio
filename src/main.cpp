#include "application.h"

#include "constants.h"

#include <iostream>
#include <exception>

// window settings
unsigned int width = 1600;
unsigned int height = 1200;

// main driver
int main()
{
	try
	{
		Application app(width, height, Backend::Vulkan);
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
} // end of main
