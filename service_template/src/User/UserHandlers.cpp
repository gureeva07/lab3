#include "UserHandlers.hpp"
#include "../AuthHelper.hpp"

#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace social {

std::string UsersHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& req,
    userver::server::request::RequestContext&) const {

  RequireAuth(req);

  // ── Поиск по логину ──────────────────────────────────────────
  if (req.HasArg("login")) {
    auto result = pg_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, login, first_name, last_name "
        "FROM users WHERE login = $1",
        req.GetArg("login"));

    if (result.IsEmpty()) {
      req.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
      return R"({"error":"User not found"})";
    }

    auto row = result[0];
    userver::formats::json::ValueBuilder resp;
    resp["id"]         = row[0].As<int>();
    resp["login"]      = row[1].As<std::string>();
    resp["first_name"] = row[2].As<std::string>();
    resp["last_name"]  = row[3].As<std::string>();
    return userver::formats::json::ToString(resp.ExtractValue());
  }

  // ── Поиск по маске имени и фамилии ───────────────────────────
  if (req.HasArg("name")) {
    // ILIKE '%маска%' — поиск без учёта регистра по подстроке
    // $1 — параметр с %маска% (добавляем % с обеих сторон)
    std::string mask = "%" + req.GetArg("name") + "%";

    auto result = pg_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, login, first_name, last_name "
        "FROM users "
        "WHERE (first_name || ' ' || last_name) ILIKE $1 "
        "ORDER BY last_name, first_name",
        mask);

    userver::formats::json::ValueBuilder arr(
        userver::formats::json::Type::kArray);

    for (auto row : result) {
      userver::formats::json::ValueBuilder user;
      user["id"]         = row[0].As<int>();
      user["login"]      = row[1].As<std::string>();
      user["first_name"] = row[2].As<std::string>();
      user["last_name"]  = row[3].As<std::string>();
      arr.PushBack(user.ExtractValue());
    }

    return userver::formats::json::ToString(arr.ExtractValue());
  }

  req.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
  return R"({"error":"Provide 'login' or 'name' query parameter"})";
}

}  // namespace social
