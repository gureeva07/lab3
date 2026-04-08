#include "ChatHandlers.hpp"
#include "../AuthHelper.hpp"

#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace social {

std::string ChatHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& req,
    userver::server::request::RequestContext&) const {

  int sender_id = RequireAuth(req);

  // ── POST: отправить сообщение ────────────────────────────────
  if (req.GetMethod() == userver::server::http::HttpMethod::kPost) {
    auto body       = userver::formats::json::FromString(req.RequestBody());
    int receiver_id = body["receiver_id"].As<int>(0);
    std::string text = body["text"].As<std::string>("");

    if (receiver_id == 0 || text.empty()) {
      req.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return R"({"error":"receiver_id and text are required"})";
    }

    // Проверяем: существует ли получатель
    auto check = pg_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id FROM users WHERE id = $1",
        receiver_id);

    if (check.IsEmpty()) {
      req.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
      return R"({"error":"Receiver not found"})";
    }

    // Сохраняем сообщение
    auto result = pg_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO chat_messages (sender_id, receiver_id, text) "
        "VALUES ($1, $2, $3) "
        "RETURNING id, sender_id, receiver_id, text, "
        "          to_char(created_at, 'YYYY-MM-DD HH24:MI:SS')",
        sender_id, receiver_id, text);

    auto row = result[0];
    userver::formats::json::ValueBuilder resp;
    resp["id"]          = row[0].As<int>();
    resp["sender_id"]   = row[1].As<int>();
    resp["receiver_id"] = row[2].As<int>();
    resp["text"]        = row[3].As<std::string>();
    resp["created_at"]  = row[4].As<std::string>();

    req.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    return userver::formats::json::ToString(resp.ExtractValue());
  }

  // ── GET: получить сообщения пользователя ────────────────────
  // Все сообщения где пользователь — отправитель ИЛИ получатель
  if (req.HasArg("user_id")) {
    int uid = std::stoi(req.GetArg("user_id"));

    auto result = pg_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        "SELECT id, sender_id, receiver_id, text, "
        "       to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') "
        "FROM chat_messages "
        "WHERE sender_id = $1 OR receiver_id = $1 "
        "ORDER BY created_at DESC",
        uid);

    userver::formats::json::ValueBuilder arr(
        userver::formats::json::Type::kArray);

    for (auto row : result) {
      userver::formats::json::ValueBuilder msg;
      msg["id"]          = row[0].As<int>();
      msg["sender_id"]   = row[1].As<int>();
      msg["receiver_id"] = row[2].As<int>();
      msg["text"]        = row[3].As<std::string>();
      msg["created_at"]  = row[4].As<std::string>();
      arr.PushBack(msg.ExtractValue());
    }

    return userver::formats::json::ToString(arr.ExtractValue());
  }

  req.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
  return R"({"error":"Provide 'user_id' query parameter or POST to send a message"})";
}

}  // namespace social
