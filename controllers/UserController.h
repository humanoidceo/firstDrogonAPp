#pragma once
#include <drogon/drogon.h>
#include <drogon/HttpController.h>
using namespace drogon;

class UserController : public drogon::HttpController<UserController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UserController::echoFullName, "/full-name", Post);
    METHOD_LIST_END

    void echoFullName(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("name") || !json->isMember("full_name") ||
            !(*json)["name"].isString() || !(*json)["full_name"].isString())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "name and full_name are required string fields";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        const auto name = (*json)["name"].asString();
        const auto fullName = (*json)["full_name"].asString();
        if (name.empty() || fullName.empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "name and full_name cannot be empty";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        Json::Value data;
        data["status"] = "ok";
        data["full_name"] = fullName;
        auto resp = HttpResponse::newHttpJsonResponse(data);
        resp->setStatusCode(k200OK);
        callback(resp);
    }
};
