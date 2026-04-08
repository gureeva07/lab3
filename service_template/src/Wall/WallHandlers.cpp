#include "WallHandlers.hpp"
#include "../AuthHelper.hpp"

#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace social {

std::string WallHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& req,
    userver::server::request::RequestContext&) const {

  int author_id = RequireAuth(req);
  int owner_id  = std::stoi(req.GetPathArg("user_id"));

  // Проверяем: существует ли владелец стены
  auto check = pg_->Execute(
      userver::storages::postgres::ClusterHostType::kSlave,
      "SELECT id FROM users WHERE id = $1",
      owner_id);

  if (check.IsEmpty()) {
    req.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
    return R"({"error":"User not found"})";
  }

  // ── POST: добавить запись на стену ───────────────────────────
  if (req.GetMethod() == userver::server::http::HttpMethod::kPost) {
    auto body    = userver::formats::json::FromString(req.RequestBody());
    std::string content    = body["content"].As<std::string>("");
    std::string created_at = body["created_at"].As<std::string>("");

    if (content.empty()) {
      req.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return R"({"error":"content is required"})";
    }

    // Вставляем пост, получаем id и дату создания
    auto result = pg_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO wall_posts (owner_id, author_id, content) "
        "VALUES ($1, $2, $3) "
        "RETURNING id, owner_id, author_id, content, "
        "          to_char(created_at, 'YYYY-MM-DD HH24:MI:SS')",
        owner_id, author_id, content);

    auto row = result[0];
    userver::formats::json::ValueBuilder resp;
    resp["id"]         = row[0].As<int>();
    resp["owner_id"]   = row[1].As<int>();
    resp["author_id"]  = row[2].As<int>();
    resp["content"]    = row[3].As<std::string>();
    resp["created_at"] = row[4].As<std::string>();

    req.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    return userver::formats::json::ToString(resp.ExtractValue());
  }

  // ── GET: загрузить стену пользователя ────────────────────────
  // Свежие посты — первыми (ORDER BY created_at DESC)
  auto result = pg_->Execute(
      userver::storages::postgres::ClusterHostType::kSlave,
      "SELECT wp.id, wp.owner_id, wp.author_id, wp.content, "
      "       to_char(wp.created_at, 'YYYY-MM-DD HH24:MI:SS') "
      "FROM wall_posts wp "
      "WHERE wp.owner_id = $1 "
      "ORDER BY wp.created_at DESC",
      owner_id);

  userver::formats::json::ValueBuilder arr(
      userver::formats::json::Type::kArray);

  for (auto row : result) {
    userver::formats::json::ValueBuilder post;
    post["id"]         = row[0].As<int>();
    post["owner_id"]   = row[1].As<int>();
    post["author_id"]  = row[2].As<int>();
    post["content"]    = row[3].As<std::string>();
    post["created_at"] = row[4].As<std::string>();
    arr.PushBack(post.ExtractValue());
  }

  return userver::formats::json::ToString(arr.ExtractValue());
}

}  // namespace social
