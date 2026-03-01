#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;
namespace fs = std::filesystem;

struct DbConnectionConfig
{
    std::string rdbms;
    std::string host;
    unsigned short port{0};
    std::string dbname;
    std::string user;
    std::string passwd;
    std::string filename;
    size_t connectionNum{1};
};

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

static std::vector<fs::path> getPathCandidates(const std::string &fileName)
{
    std::vector<fs::path> candidates = {fileName, "../" + fileName};

    std::error_code ec;
    const fs::path exePath = fs::read_symlink("/proc/self/exe", ec);
    if (!ec && !exePath.empty())
    {
        const fs::path exeDir = exePath.parent_path();
        candidates.push_back(exeDir / fileName);
        candidates.push_back(exeDir / ("../" + fileName));
    }

    return candidates;
}

static std::optional<std::string> readDotEnvValue(const std::string &key)
{
    for (const auto &path : getPathCandidates(".env"))
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

static std::string resolveDbConfigPath()
{
    std::error_code ec;
    for (const auto &path : getPathCandidates("models/model.json"))
    {
        if (fs::exists(path, ec) && !ec)
        {
            return path.string();
        }
    }

    // Fallback for older setup.
    for (const auto &path : getPathCandidates("config.json"))
    {
        if (fs::exists(path, ec) && !ec)
        {
            return path.string();
        }
    }

    throw std::runtime_error(
        "DB config file not found. Expected models/model.json or config.json");
}

static DbConnectionConfig loadDbConfig(const std::string &configPath)
{
    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Unable to open config file: " + configPath);
    }

    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::string errors;
    if (!Json::parseFromStream(readerBuilder, configFile, &root, &errors))
    {
        throw std::runtime_error("Invalid DB config json: " + errors);
    }

    DbConnectionConfig config;

    if (root.isMember("db_clients") && root["db_clients"].isArray() &&
        !root["db_clients"].empty())
    {
        const Json::Value &dbClients = root["db_clients"];
        const Json::Value *selected = &dbClients[0];
        for (const auto &entry : dbClients)
        {
            if (entry.isMember("name") && entry["name"].isString() &&
                entry["name"].asString() == "default")
            {
                selected = &entry;
                break;
            }
        }

        config.rdbms = trim(selected->get("rdbms", "").asString());
        config.host = trim(selected->get("host", "").asString());
        config.port = static_cast<unsigned short>(selected->get("port", 0).asUInt());
        config.dbname = trim(selected->get("dbname", "").asString());
        config.user = trim(selected->get("user", "").asString());
        config.passwd = trim(selected->get("passwd", "").asString());
        config.filename = trim(selected->get("filename", "").asString());
        config.connectionNum = static_cast<size_t>(
            selected->get("number_of_connections", Json::Value(1)).asUInt64());
    }
    else
    {
        // model.json shape
        config.rdbms = trim(root.get("rdbms", "").asString());
        config.host = trim(root.get("host", "").asString());
        config.port = static_cast<unsigned short>(root.get("port", 0).asUInt());
        config.dbname = trim(root.get("dbname", "").asString());
        config.user = trim(root.get("user", "").asString());
        config.passwd = trim(root.get("passwd", "").asString());
        config.filename = trim(root.get("filename", "").asString());
        config.connectionNum = static_cast<size_t>(
            root.get("number_of_connections", Json::Value(1)).asUInt64());
        if (config.connectionNum == 0)
        {
            config.connectionNum = 1;
        }
    }
    if (config.connectionNum == 0)
    {
        config.connectionNum = 1;
    }

    if (config.rdbms.empty())
    {
        throw std::runtime_error("rdbms is required in DB config file");
    }

    return config;
}

static DbClientPtr createDbClientFromConfig(const DbConnectionConfig &config)
{
    if (config.rdbms == "mysql")
    {
        const auto port = config.port == 0 ? 3306 : config.port;
        const std::string connInfo = "host=" + config.host + " port=" +
                                     std::to_string(port) + " dbname=" +
                                     config.dbname + " user=" + config.user +
                                     " password=" + config.passwd;
        return DbClient::newMysqlClient(connInfo, config.connectionNum);
    }

    if (config.rdbms == "postgresql" || config.rdbms == "postgres")
    {
        const auto port = config.port == 0 ? 5432 : config.port;
        const std::string connInfo = "host=" + config.host + " port=" +
                                     std::to_string(port) + " dbname=" +
                                     config.dbname + " user=" + config.user +
                                     " password=" + config.passwd;
        return DbClient::newPgClient(connInfo, config.connectionNum);
    }

    if (config.rdbms == "sqlite3")
    {
        const std::string connInfo = "filename=" + config.filename;
        return DbClient::newSqlite3Client(connInfo, config.connectionNum);
    }

    throw std::runtime_error("Unsupported db type in DB config: " + config.rdbms);
}

int main()
{
    try
    {
        const auto emailOpt = getConfigValue("SUPER_ADMIN_EMAIL");
        const auto passwordOpt = getConfigValue("SUPER_ADMIN_PASSWORD");
        const auto nameOpt = getConfigValue("SUPER_ADMIN_NAME");

        const std::string email = emailOpt ? trim(*emailOpt) : "";
        const std::string password = passwordOpt ? trim(*passwordOpt) : "";
        const std::string name =
            (nameOpt && !trim(*nameOpt).empty()) ? trim(*nameOpt) : "Super Admin";

        if (email.empty() || password.empty())
        {
            std::cerr << "Set SUPER_ADMIN_EMAIL and SUPER_ADMIN_PASSWORD in .env\n";
            return 1;
        }

        if (password.size() < 8)
        {
            std::cerr << "SUPER_ADMIN_PASSWORD must be at least 8 characters\n";
            return 1;
        }

        const auto configPath = resolveDbConfigPath();
        const auto dbConfig = loadDbConfig(configPath);
        auto client = createDbClientFromConfig(dbConfig);

        const auto passwordHash = drogon::utils::getMd5(password);
        const auto existing =
            client->execSqlSync("SELECT id FROM users WHERE email=? LIMIT 1", email);

        if (!existing.empty())
        {
            const auto id = existing[0]["id"].as<long long>();
            client->execSqlSync("UPDATE users SET name=?, password=? WHERE id=?",
                                name,
                                passwordHash,
                                id);
            std::cout << "Super admin updated for email: " << email << '\n';
        }
        else
        {
            const auto result = client->execSqlSync(
                "INSERT INTO users(name,email,password) VALUES(?,?,?)",
                name,
                email,
                passwordHash);
            std::cout << "Super admin inserted with id: " << result.insertId() << '\n';
        }
    }
    catch (const DrogonDbException &e)
    {
        std::cerr << "Database error: " << e.base().what() << '\n';
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
