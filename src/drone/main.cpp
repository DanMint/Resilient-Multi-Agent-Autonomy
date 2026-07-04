#include <unistd.h>
#include <iostream>
#include <vector>
#include <unordered_map>

#include "utils.h"

int main() {
    // pre defined IPs of all of the drones
    const std::unordered_map<std::string, std::string> droneIps = {
        {"D1", "172.20.0.10"},
        {"D2", "172.20.0.20"}
    };

    // get name of docker container
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    std::string hostnameStr(hostname);

    std::cout << "Im drone number: " << hostname << std::endl;

    Utils::checkValidityOfNetwork(droneIps, hostnameStr);
}