#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Mapper.h>
#include <string>

using namespace drogon;
using namespace drogon::orm;

struct User
{
    int id;
    std::string name;
    std::string email;
};