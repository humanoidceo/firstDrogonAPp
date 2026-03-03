#pragma once

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace appenv
{
namespace fs = std::filesystem;

inline std::string trim(const std::string &input)
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

inline std::string unquote(const std::string &value)
{
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\'')))
    {
        return value.substr(1, value.size() - 2);
    }

    return value;
}

inline std::vector<fs::path> getDotEnvCandidates()
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

inline std::optional<std::string> readDotEnvValue(const std::string &key)
{
    for (const auto &path : getDotEnvCandidates())
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

inline std::optional<std::string> getConfigValue(const std::string &key)
{
    const char *envValue = std::getenv(key.c_str());
    if (envValue && *envValue != '\0')
    {
        return std::string(envValue);
    }

    return readDotEnvValue(key);
}

inline std::optional<uint16_t> getPortValue(const std::string &key)
{
    const auto value = getConfigValue(key);
    if (!value)
    {
        return std::nullopt;
    }

    const auto trimmed = trim(*value);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    try
    {
        const auto parsed = std::stoul(trimmed);
        if (parsed > std::numeric_limits<uint16_t>::max())
        {
            return std::nullopt;
        }

        return static_cast<uint16_t>(parsed);
    }
    catch (...)
    {
        return std::nullopt;
    }
}
}  // namespace appenv
