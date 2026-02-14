#include "utils.h"
#include <iostream>
#include "restclient-cpp/restclient.h"

std::string utils::latestTag;

int main(int argc, char* argv[]) {
    try {
        RestClient::init();

        // Accept optional version argument: ./MagiskHluda [version]
        std::string preferredVersion = "";
        if (argc > 1) {
            preferredVersion = argv[1];
        }

        utils::latestTag = utils::getRecentTag(preferredVersion);

        utils::createModuleProps();
        utils::createUpdateJson();
        utils::downloadServers();
        RestClient::disable();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        RestClient::disable();
        return 1;
    }
}