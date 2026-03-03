#pragma once
#include <string>
#include <drogon/HttpController.h>
#include <drogon/drogon.h>
#include "utils/EnvUtils.h"

using namespace drogon;

class HugginfacechatterController : public drogon::HttpController<HugginfacechatterController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HugginfacechatterController::askQuestion, "/hugginfacechatter", Post);
    ADD_METHOD_TO(HugginfacechatterController::handlePreflight, "/hugginfacechatter", Options);
    METHOD_LIST_END

    void handlePreflight(const HttpRequestPtr &,
                         std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto resp = HttpResponse::newHttpResponse();
        addCorsHeaders(resp);
        resp->setStatusCode(k204NoContent);
        callback(resp);
    }

    void askQuestion(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("question") || !(*json)["question"].isString())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "question is a required string field";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            addCorsHeaders(resp);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        const auto question = (*json)["question"].asString();
        if (question.empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "question cannot be empty";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            addCorsHeaders(resp);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        const auto model = appenv::getConfigValue("OLLAMA_MODEL");
        if (!model || model->empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "Set OLLAMA_MODEL in .env";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            addCorsHeaders(resp);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
            return;
        }

        const auto modelValue = appenv::trim(*model);
        if (modelValue.empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "Set OLLAMA_MODEL in .env";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            addCorsHeaders(resp);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
            return;
        }

        const auto baseUrlValue = appenv::getConfigValue("OLLAMA_BASE_URL");
        std::string baseUrl = "http://127.0.0.1:11434";
        if (baseUrlValue && !appenv::trim(*baseUrlValue).empty())
        {
            baseUrl = appenv::trim(*baseUrlValue);
        }
        if (!baseUrl.empty() && baseUrl.back() == '/')
        {
            baseUrl.pop_back();
        }

        auto ollamaRequest = HttpRequest::newHttpRequest();
        ollamaRequest->setMethod(Post);
        ollamaRequest->setPath("/api/generate");
        ollamaRequest->addHeader("Accept", "application/json");
        ollamaRequest->setContentTypeCode(CT_APPLICATION_JSON);

        Json::Value requestBody;
        requestBody["model"] = modelValue;
        requestBody["prompt"] = question;
        requestBody["stream"] = false;
        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "";
        ollamaRequest->setBody(Json::writeString(writerBuilder, requestBody));

        auto client = HttpClient::newHttpClient(baseUrl);
        client->sendRequest(
            ollamaRequest,
            [callback](ReqResult result, const HttpResponsePtr &ollamaResponse) {
                if (result != ReqResult::Ok || !ollamaResponse)
                {
                    Json::Value data;
                    data["status"] = "error";
                    data["message"] = "failed to connect to ollama";
                    auto resp = HttpResponse::newHttpJsonResponse(data);
                    addCorsHeaders(resp);
                    resp->setStatusCode(k502BadGateway);
                    callback(resp);
                    return;
                }

                if (ollamaResponse->statusCode() != k200OK)
                {
                    Json::Value data;
                    data["status"] = "error";
                    data["message"] = "ollama request failed";
                    data["upstream_status"] =
                        static_cast<int>(ollamaResponse->statusCode());
                    data["details"] = std::string(ollamaResponse->body());
                    auto resp = HttpResponse::newHttpJsonResponse(data);
                    addCorsHeaders(resp);
                    resp->setStatusCode(k502BadGateway);
                    callback(resp);
                    return;
                }

                std::string answer;
                auto ollamaJson = ollamaResponse->getJsonObject();
                if (ollamaJson && ollamaJson->isObject() &&
                    ollamaJson->isMember("response") &&
                    (*ollamaJson)["response"].isString())
                {
                    answer = (*ollamaJson)["response"].asString();
                }

                if (answer.empty())
                {
                    answer = std::string(ollamaResponse->body());
                }

                Json::Value data;
                data["status"] = "ok";
                data["answer"] = answer;
                auto resp = HttpResponse::newHttpJsonResponse(data);
                addCorsHeaders(resp);
                resp->setStatusCode(k200OK);
                callback(resp);
            });
    }

private:
    static void addCorsHeaders(const HttpResponsePtr &resp)
    {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        resp->addHeader("Access-Control-Max-Age", "86400");
    }
};
