#pragma once
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <drogon/HttpController.h>
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/utils/Utilities.h>

using namespace drogon;
using namespace drogon::orm;
namespace fs = std::filesystem;

class RegisterController : public drogon::HttpController<RegisterController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(RegisterController::registerUser, "/register", Post);
    METHOD_LIST_END

    void registerUser(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto callbackPtr =
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

        auto json = req->getJsonObject();
        if (!json || !json->isMember("name") || !json->isMember("email") ||
            !json->isMember("password") || !(*json)["name"].isString() ||
            !(*json)["email"].isString() || !(*json)["password"].isString())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "name, email and password are required string fields";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k400BadRequest);
            (*callbackPtr)(resp);
            return;
        }

        const auto name = (*json)["name"].asString();
        const auto email = (*json)["email"].asString();
        const auto password = (*json)["password"].asString();
        if (name.empty() || email.empty() || password.empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "name, email and password cannot be empty";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k400BadRequest);
            (*callbackPtr)(resp);
            return;
        }

        if (password.size() < 8)
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "password must be at least 8 characters";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k400BadRequest);
            (*callbackPtr)(resp);
            return;
        }

        const auto superAdminEmailOpt = getConfigValue("SUPER_ADMIN_EMAIL");
        const auto superAdminEmail = superAdminEmailOpt ? trim(*superAdminEmailOpt) : "";
        if (superAdminEmail.empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "SUPER_ADMIN_EMAIL is not configured";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k500InternalServerError);
            (*callbackPtr)(resp);
            return;
        }

        const auto adminEmail = trim(req->getHeader("x-admin-email"));
        const auto adminPassword = req->getHeader("x-admin-password");
        if (adminEmail.empty() || adminPassword.empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] =
                "x-admin-email and x-admin-password headers are required";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k403Forbidden);
            (*callbackPtr)(resp);
            return;
        }

        if (adminEmail != superAdminEmail)
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "only super admin can register new users";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k403Forbidden);
            (*callbackPtr)(resp);
            return;
        }

        const auto newUserPasswordHash = drogon::utils::getMd5(password);
        const auto adminPasswordHash = drogon::utils::getMd5(adminPassword);
        auto client = app().getDbClient();
        client->execSqlAsync(
            "SELECT id FROM users WHERE email=? AND password=? LIMIT 1",
            [callbackPtr, client, name, email, newUserPasswordHash](const Result &adminResult) {
                if (adminResult.empty())
                {
                    Json::Value data;
                    data["status"] = "error";
                    data["message"] = "invalid super admin credentials";
                    auto resp = HttpResponse::newHttpJsonResponse(data);
                    resp->setStatusCode(k403Forbidden);
                    (*callbackPtr)(resp);
                    return;
                }

                client->execSqlAsync(
                    "INSERT INTO users(name,email,password) VALUES(?,?,?)",
                    [callbackPtr](const Result &result) {
                        Json::Value data;
                        data["status"] = "ok";
                        data["id"] = static_cast<Json::UInt64>(result.insertId());
                        auto resp = HttpResponse::newHttpJsonResponse(data);
                        resp->setStatusCode(k201Created);
                        (*callbackPtr)(resp);
                    },
                    [callbackPtr](const DrogonDbException &e) {
                        Json::Value data;
                        data["status"] = "error";
                        data["msg"] = e.base().what();
                        auto resp = HttpResponse::newHttpJsonResponse(data);
                        resp->setStatusCode(k500InternalServerError);
                        (*callbackPtr)(resp);
                    },
                    name,
                    email,
                    newUserPasswordHash);
            },
            [callbackPtr](const DrogonDbException &e) {
                Json::Value data;
                data["status"] = "error";
                data["msg"] = e.base().what();
                auto resp = HttpResponse::newHttpJsonResponse(data);
                resp->setStatusCode(k500InternalServerError);
                (*callbackPtr)(resp);
            },
            adminEmail,
            adminPasswordHash);
    }

private:
    static std::string trim(const std::string &input)
    {
        size_t start = 0;
        while (start < input.size() &&
               std::isspace(static_cast<unsigned char>(input[start])))
        {
            ++start;
        }

        size_t end = input.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(input[end - 1])))
        {
            --end;
        }

        return input.substr(start, end - start);
    }

    static std::string unquote(const std::string &value)
    {
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                  (value.front() == '\'' && value.back() == '\'')))
        {
            return value.substr(1, value.size() - 2);
        }

        return value;
    }

    static std::vector<fs::path> envPathCandidates()
    {
        std::vector<fs::path> candidates = {".env", "../.env"};

        std::error_code ec;
        const fs::path exePath = fs::read_symlink("/proc/self/exe", ec);
        if (!ec && !exePath.empty())
        {
            const fs::path exeDir = exePath.parent_path();
            candidates.push_back(exeDir / ".env");
            candidates.push_back(exeDir / "../.env");
        }

        return candidates;
    }

    static std::optional<std::string> readDotEnvValue(const std::string &key)
    {
        for (const auto &path : envPathCandidates())
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
                continue;
            }

            std::string line;
            while (std::getline(file, line))
            {
                const auto cleaned = trim(line);
                if (cleaned.empty() || cleaned.front() == '#')
                {
                    continue;
                }

                const auto pos = cleaned.find('=');
                if (pos == std::string::npos)
                {
                    continue;
                }

                const auto parsedKey = trim(cleaned.substr(0, pos));
                if (parsedKey != key)
                {
                    continue;
                }

                return unquote(trim(cleaned.substr(pos + 1)));
            }
        }

        return std::nullopt;
    }

    static std::optional<std::string> getConfigValue(const std::string &key)
    {
        const char *envValue = std::getenv(key.c_str());
        if (envValue && *envValue != '\0')
        {
            return std::string(envValue);
        }

        return readDotEnvValue(key);
    }
};
