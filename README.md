# Social Network API

Лабораторная работа №3 "Проектирование и оптимизация реляционной базы данных", 1 вариант

База данных: **PostgreSQL 16**


## Схема базы данных

Три основные таблицы:

- `users` — хранит пользователей. Логин уникален, поиск по логину и по маске
- `wall_posts` — записи на стене. Каждая запись привязана к владельцу стены (`owner_id`) и автору (`author_id`)
- `chat_messages` — личные сообщения. Хранят отправителя (`sender_id`), получателя (`receiver_id`) и текст

Все связи между таблицами через внешние ключи (FK). Индексы созданы на внешние ключи и колонки, используемые в поиске.

## Запуск

### Требования

- Docker
- Docker Compose

### Запустить всё одной командой

```bash
docker-compose up --build
```

Это запустит:
1. PostgreSQL — создаст базу, применит `schema.sql` и `data.sql`
2. API-сервис — поднимется на `http://localhost:8080`

### Остановить

```bash
docker-compose down
```

Чтобы удалить и данные базы:

```bash
docker-compose down -v
```

## Подключение к базе данных вручную

```bash
docker exec -it service_template-postgres-1 psql -U social_user -d social_network
```

После входа можно выполнять SQL-запросы, например:

```sql
SELECT * FROM users;
SELECT * FROM wall_posts;
SELECT * FROM chat_messages;
```

## Файлы базы данных

| Файл | Описание |
|------|----------|
| `service_template/schema.sql` | создание таблиц и индексов |
| `service_template/data.sql` | тестовые данные |
| `service_template/queries.sql` | SQL-запросы для всех операций API |
| `service_template/optimization.md` | анализ и оптимизация запросов с EXPLAIN |

## Эндпоинты API (ручки)

| Метод | URL | Описание |
|-------|-----|----------|
| POST | `/api/v1/auth/register` | создание нового пользователя |
| POST | `/api/v1/auth/login` | вход, получение токена |
| POST | `/api/v1/auth/logout` | выход из системы |
| GET | `/api/v1/users?login=` | поиск пользователя по логину |
| GET | `/api/v1/users?name=` | поиск по маске имени и фамилии |
| POST | `/api/v1/wall/{user_id}` | добавление записи на стену |
| GET | `/api/v1/wall/{user_id}` | загрузка стены пользователя |
| POST | `/api/v1/messages` | отправка сообщения пользователю |
| GET | `/api/v1/messages?user_id=` | получение списка сообщений |

## Примеры запросов

### Регистрация

```bash
curl -X POST http://127.0.0.1:8080/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{"login":"alina","password":"pass123","first_name":"Alina","last_name":"Gureeva"}'
```

### Вход (получение токена)

```bash
curl -X POST http://127.0.0.1:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"login":"alina","password":"pass123"}'
```

Скопируйте `token` из ответа — он нужен для всех последующих запросов.

### Поиск пользователя по логину

```bash
curl "http://127.0.0.1:8080/api/v1/users?login=alina" \
  -H "Authorization: Bearer <TOKEN>"
```

### Поиск по маске имени или фамилии

```bash
curl "http://127.0.0.1:8080/api/v1/users?name=Alina" \
  -H "Authorization: Bearer <TOKEN>"
```

### Добавить запись на стену

```bash
curl -X POST http://127.0.0.1:8080/api/v1/wall/1 \
  -H "Authorization: Bearer <TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{"content":"Привет, мир!"}'
```

### Получить стену пользователя

```bash
curl http://127.0.0.1:8080/api/v1/wall/1 \
  -H "Authorization: Bearer <TOKEN>"
```

### Отправить сообщение

```bash
curl -X POST http://127.0.0.1:8080/api/v1/messages \
  -H "Authorization: Bearer <TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{"receiver_id":2,"text":"Привет!"}'
```

### Получить список сообщений

```bash
curl "http://127.0.0.1:8080/api/v1/messages?user_id=1" \
  -H "Authorization: Bearer <TOKEN>"
```

## HTTP коды ответов

- `200` — OK
- `201` — ресурс создан
- `400` — неверный запрос (не хватает полей)
- `401` — токен неверный или отсутствует
- `404` — ресурс не найден
- `409` — логин уже занят
