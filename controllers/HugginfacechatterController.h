#pragma once
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <drogon/HttpController.h>
#include <drogon/drogon.h>

using namespace drogon;
namespace fs = std::filesystem;

class HugginfacechatterController : public drogon::HttpController<HugginfacechatterController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HugginfacechatterController::askQuestion, "/hugginfacechatter", Post);
    METHOD_LIST_END

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
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        const auto model = getConfigValue("OLLAMA_MODEL");
        if (!model || model->empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "Set OLLAMA_MODEL in .env";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
            return;
        }

        const auto modelValue = trim(*model);
        if (modelValue.empty())
        {
            Json::Value data;
            data["status"] = "error";
            data["message"] = "Set OLLAMA_MODEL in .env";
            auto resp = HttpResponse::newHttpJsonResponse(data);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
            return;
        }

        const auto baseUrlValue = getConfigValue("OLLAMA_BASE_URL");
        std::string baseUrl = "http://127.0.0.1:11434";
        if (baseUrlValue && !trim(*baseUrlValue).empty())
        {
            baseUrl = trim(*baseUrlValue);
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
                resp->setStatusCode(k200OK);
                callback(resp);
            });
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

    static std::optional<std::string> readDotEnvValue(const std::string &key)
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

        for (const auto &path : candidates)
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
