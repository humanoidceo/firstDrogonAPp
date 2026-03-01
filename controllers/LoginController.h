#pragma once
#include <drogon/HttpController.h>
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/utils/Utilities.h>

using namespace drogon;
using namespace drogon::orm;

class LoginController : public drogon::HttpController<LoginController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LoginController::loginUser, "/login", Post);
    METHOD_LIST_END

    void loginUser(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("email") || !json->isMember("password") ||
            !(*json)["email"].isString() || !(*json)["password"].isString())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "email and password are required string fields";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        const auto email = (*json)["email"].asString();
        const auto password = (*json)["password"].asString();
        if (email.empty() || password.empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "email and password cannot be empty";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        const auto passwordHash = drogon::utils::getMd5(password);
        auto client = app().getDbClient();
        client->execSqlAsync(
            "SELECT id,name,email FROM users WHERE email=? AND password=? LIMIT 1",
            [callback](const Result &result) {
                if (result.empty())
                {
                    Json::Value data;
                    data["status"] = "error";
                    data["message"] = "invalid email or password";
                    auto resp = HttpResponse::newHttpJsonResponse(data);
                    resp->setStatusCode(k401Unauthorized);
                    callback(resp);
                    return;
                }

                const auto &row = result[0];
                Json::Value data;
                data["status"] = "ok";
                data["message"] = "login successful";
                data["user"]["id"] = static_cast<Json::UInt64>(row["id"].as<long long>());
                data["user"]["name"] = row["name"].as<std::string>();
                data["user"]["email"] = row["email"].as<std::string>();
                auto resp = HttpResponse::newHttpJsonResponse(data);
                resp->setStatusCode(k200OK);
                callback(resp);
            },
            [callback](const DrogonDbException &e) {
                Json::Value data;
                data["status"] = "error";
                data["msg"] = e.base().what();
                auto resp = HttpResponse::newHttpJsonResponse(data);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
            },
            email,
            passwordHash);
    }
};
