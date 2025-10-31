#include "System.h"

System::System(const std::string& name) : name(name)
{

}

const std::string System::getName() const
{
	return name;
}
