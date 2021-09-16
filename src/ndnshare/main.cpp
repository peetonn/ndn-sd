#include <iostream>
#include <csignal>
#include <map>

#include <docopt.h>
#include <fmt/format.h>
#include <ndn-ind/face.hpp>
#include <ndn-ind-tools/micro-forwarder/micro-forwarder.hpp>
#include <cnl-cpp/namespace.hpp>

#include <ndn-sd/ndn-sd.hpp>

using namespace std;

atomic_bool run = true;
void signal_handler(int signal)
{
	run = !(signal == SIGINT);
}

int main (int argc, char **argv)
{
	
    int maxFd = 0;
    
	cout << "NDN-SD version " << ndnsd::NdnSd::getVersion() << endl;

	
	return 0;
}
