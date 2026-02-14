#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <algorithm>
#include "restclient-cpp/connection.h"
#include "restclient-cpp/restclient.h"
#include "rapidjson/document.h"
#include "utils.h"
#include <cstdlib>

namespace fs = std::filesystem;
const std::string basePath = "./module_template/";

static std::string getEnvOrDefault(const char* name, const std::string& fallback)
{
    const char* val = std::getenv(name);
    return (val && val[0] != '\0') ? std::string(val) : fallback;
}

// Helper: checks if a release JSON object has florida-server assets
static bool hasServerAssets(const rapidjson::Value& release)
{
    if (!release.HasMember("assets") || !release["assets"].IsArray())
        return false;

    const auto& assets = release["assets"];
    for (rapidjson::SizeType j = 0; j < assets.Size(); ++j)
    {
        if (assets[j].HasMember("name") && assets[j]["name"].IsString())
        {
            std::string assetName = assets[j]["name"].GetString();
            if (assetName.find("florida-server-") != std::string::npos)
                return true;
        }
    }
    return false;
}

std::string utils::getRecentTag(const std::string& preferredVersion)
{
    // If a preferred version was given, try it first
    if (!preferredVersion.empty())
    {
        std::cout << "Preferred version specified: " << preferredVersion << std::endl;

        const std::string url = "https://api.github.com/repos/Ylarod/Florida/releases/tags/" + preferredVersion;
        RestClient::Response response = RestClient::get(url);

        if (response.code == 200)
        {
            rapidjson::Document d;
            d.Parse(response.body.c_str());

            if (d.IsObject() && hasServerAssets(d))
            {
                std::cout << "Preferred version " << preferredVersion << " found with server assets!" << std::endl;
                std::ofstream("currentTag.txt") << preferredVersion;
                return preferredVersion;
            }
            else
            {
                std::cerr << "Warning: Version " << preferredVersion << " exists but has no server assets, falling back to auto-pick" << std::endl;
            }
        }
        else
        {
            std::cerr << "Warning: Version " << preferredVersion << " not found (HTTP " << response.code << "), falling back to auto-pick" << std::endl;
        }
    }

    // Auto-pick: fetch recent releases and find one with server assets
    std::cout << "Auto-picking latest version with server assets..." << std::endl;
    const std::string url = "https://api.github.com/repos/Ylarod/Florida/releases?per_page=10";
    RestClient::Response response = RestClient::get(url);

    if (response.code != 200)
    {
        throw std::runtime_error("HTTP Error fetching releases: " + std::to_string(response.code) + " " + response.body);
    }

    rapidjson::Document d;
    d.Parse(response.body.c_str());

    if (!d.IsArray() || d.Empty())
    {
        throw std::runtime_error("Invalid JSON response: expected non-empty array of releases");
    }

    // Iterate through releases to find one with florida-server assets
    for (rapidjson::SizeType i = 0; i < d.Size(); ++i)
    {
        const auto& release = d[i];
        if (!release.HasMember("tag_name") || !release["tag_name"].IsString())
            continue;

        if (hasServerAssets(release))
        {
            std::string tag = release["tag_name"].GetString();
            std::cout << "Found release with server assets: " << tag << std::endl;
            std::ofstream("currentTag.txt") << tag;
            return tag;
        }
        else
        {
            std::string skippedTag = release["tag_name"].GetString();
            std::cout << "Skipping release " << skippedTag << " (no server assets)" << std::endl;
        }
    }

    throw std::runtime_error("No recent release found with florida-server assets");
}

void download(const std::string& aarch)
{
    auto start = std::chrono::system_clock::now();

    std::cout << "Starting To Downloaded florida for arch: " + aarch + "\n";

    std::string url = "https://github.com/Ylarod/Florida/releases/download/" + utils::latestTag +
        "/florida-server-" + utils::latestTag + "-android-" + aarch + ".gz";

    std::unique_ptr<RestClient::Connection> pConnection(new RestClient::Connection(url));
    pConnection->FollowRedirects(true);
    RestClient::Response response = pConnection->get("/");

    if (response.code != 200)
    {
        throw std::runtime_error("Download failed: " + std::to_string(response.code) + " " + response.body);
    }
    std::string filename;
    if (aarch == "x86_64")
        filename = "bin/florida-x64.gz";
    else
        filename = "bin/florida-" + aarch + ".gz";

    std::ofstream downloadedFile(filename, std::ios::out | std::ios::binary);

    if (!downloadedFile)
    {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    downloadedFile.write(response.body.c_str(), response.body.length());

    auto end = std::chrono::system_clock::now();

    std::chrono::duration<double> elapsed_seconds = end - start;
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    std::cout
        << "Successfully Downloaded florida for arch: " + aarch + ". Took " + to_string(elapsed_seconds.count()) +
        "s\n";
}

void utils::downloadServers()
{
    fs::create_directories("./bin");
    const std::vector<std::string> archs = {"arm", "arm64", "x86", "x86_64"};

    for (const auto& aarch : archs)
    {
        try
        {
            download(aarch);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error downloading " << aarch << ": " << e.what() << std::endl;
            throw std::runtime_error("Error downloading " + aarch + ": " + e.what());
        }
    }
}

void utils::createModuleProps()
{
    std::ofstream moduleProps(basePath + "module.prop");

    if (!moduleProps)
    {
        throw std::runtime_error("Failed to open module.prop for writing");
    }

    string versionCode = latestTag;
    versionCode.erase(std::remove(versionCode.begin(), versionCode.end(), '.'), versionCode.end());
    std::string repo = getEnvOrDefault("GITHUB_REPOSITORY", "znmn/magiskhluda");
    std::string author = getEnvOrDefault("GITHUB_ACTOR", "The Community");

    moduleProps << "id=magisk-hluda\n"
        << "name=Frida(Florida) Server on Boot\n"
        << "version=" << latestTag.substr(0, latestTag.find('-')) << '\n'
        << "versionCode=" << versionCode << '\n'
        << "author=" << author << " - Ylarod - Exo1i\n"
        << "description=Runs a stealthier frida-server on boot\n"
        << "updateJson=https://github.com/" << repo << "/releases/latest/download/update.json";
}

void utils::createUpdateJson()
{
    string versionCode = latestTag;
    versionCode.erase(std::remove(versionCode.begin(), versionCode.end(), '.'), versionCode.end());

    std::ofstream updateJson("update.json");
    if (!updateJson)
    {
        throw std::runtime_error("Failed to open update.json for writing");
    }

    std::string repo = getEnvOrDefault("GITHUB_REPOSITORY", "znmn/magiskhluda");

    updateJson << "{\n"
        << R"(  "version": ")" << latestTag << "\",\n"
        << "  \"versionCode\": " << versionCode << ",\n"
        << R"(  "zipUrl": "https://github.com/)" << repo << "/releases/download/"
        << latestTag << "/Magisk-Florida-Universal-" << latestTag << ".zip\",\n"
        << R"(  "changelog": "https://gist.githubusercontent.com/znmn/d18d6bcbb4a8a0dbfe24e243c19b4195/raw/077a548931b8101c31722bc52b1a34b4bbf206c0/gistfile1.txt")"
        << "\n}\n";
}
