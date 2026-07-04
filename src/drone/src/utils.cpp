#include "../include/utils.h"

void Utils::checkValidityOfNetwork(const std::unordered_map<std::string, std::string> &ips, const std::string &hostName) {
    std::cout << "Checking network validity via pings" << std::endl;

    for (const auto &IP : ips) {
        if (IP.first == hostName) {
            std::cout << "Skipping ping because its current CPS" << std::endl;
            continue;
        }

        // command sends ICMP packet
        const std::string command = "ping -c 1 -W 1 " + IP.second + " > /dev/null 2>&1";

        // get status after ping
        int status = system(command.c_str());

        if (status == 0) 
            std::cout << "Ping to drone no." << IP.first << " with IP: " << IP.second << " successfull!" << std::endl;
        else {
            std::cerr << "Ping to drone no." << IP.first << " with IP: " << IP.second << " FAILED!" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
}