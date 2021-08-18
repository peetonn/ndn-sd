#include <iostream>
#include <ndn-sd/ndn-sd.hpp>

int main ()
{
	std::cout << "NDN-SD version " << ndnsd::getVersionString() << std::endl;
	return 0;
}