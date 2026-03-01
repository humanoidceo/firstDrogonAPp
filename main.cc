#include <drogon/drogon.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include "controllers/HugginfacechatterController.h"
#include "controllers/LoginController.h"
#include "controllers/RegisterController.h"
#include "controllers/UserController.h"

using namespace drogon;
namespace fs = std::filesystem;

static std::string resolveConfigPath()
{
    std::vector<fs::path> candidates = {"config.json", "../config.json"};

    std::error_code ec;
    const fs::path exePath = fs::read_symlink("/proc/self/exe", ec);
    if (!ec && !exePath.empty())
    {
        const fs::path exeDir = exePath.parent_path();
        candidates.push_back(exeDir / "config.json");
        candidates.push_back(exeDir / "../config.json");
    }

    for (const auto &path : candidates)
    {
        if (fs::exists(path, ec) && !ec)
        {
            return path.string();
        }
    }

    throw std::runtime_error("Config file config.json not found in cwd or project root.");
}

int main()
{
    app().loadConfigFile(resolveConfigPath());

    app().registerHandler("/", [](const HttpRequestPtr &req,
                                  std::function<void (const HttpResponsePtr &)> &&callback)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody("Hello World");
        callback(resp);
    });

    app().registerHandler("/favicon.ico",
                          [](const HttpRequestPtr &,
                             std::function<void (const HttpResponsePtr &)> &&callback) {
                              auto resp = HttpResponse::newHttpResponse();
                              resp->setStatusCode(k204NoContent);
                              callback(resp);
                          });

    app().addListener("0.0.0.0", 8080);

    app().run();
}
