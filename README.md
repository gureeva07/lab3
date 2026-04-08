# Social Network API

Вариант 1, 2 лабораторная работа "Разработка REST API сервиса"

Фреймворк: **C++ userver** 

## Описание

REST API сервис для социальной сети с поддержкой аутентификации, поиска пользователей, стены и P2P-сообщений.

**Сущности:**
- **User** — пользователь 
- **Wall** — запись на стене пользователя
- **Chat** — сообщение между пользователями

**Хранилище:** in-memory (без бд)

## Инструкция по запуску

### Запуск

```bash
docker-compose build   # собрать образ
docker-compose up      # запустить сервис
```

Или одной командой:

```bash
docker-compose up --build
```

Сервис будет доступен на `http://localhost:8080`.

## Структура проекта

```
.
├── configs/
│   └── static_config.yaml       # Конфигурация userver (порты, логирование, хендлеры)
├── src/
│   ├── Auth/
│   │   ├── AuthHandlers.hpp     # Обработчики: регистрация, вход, выход
│   │   └── AuthHandlers.cpp
│   ├── Chat/
│   │   ├── ChatHandlers.hpp     # Обработчики сообщений
│   │   └── ChatHandlers.cpp
│   ├── User/
│   │   ├── UserHandlers.hpp     # Обработчики поиска пользователей
│   │   └── UserHandlers.cpp
│   ├── Wall/
│   │   ├── WallHandlers.hpp     # Обработчики стены (посты)
│   │   └── WallHandlers.cpp
│   ├── AuthHelper.hpp           # Проверка Bearer-токена (inline)
│   ├── DTO.hpp                  # Структуры данных: User, WallPost, ChatMessage
│   ├── JsonHelper.hpp           # Сериализация DTO в JSON
│   ├── JsonHelper.cpp
│   ├── Storage.hpp              # In-memory хранилище 
│   ├── Storage.cpp
│   ├── hello.hpp                # Тестовый вариант (заглушка)
│   ├── hello.cpp
│   ├── hello_benchmark.cpp      # Бенчмарки
│   ├── hello_test.cpp           # Юнит-тесты
│   └── main.cpp                 # Точка входа, регистрация компонентов
├── openapi.yaml                 # OpenAPI 3.0 спецификация
├── CMakeLists.txt              # Сборка проекта
├── docker-compose.yaml         # Docker-развёртывание
└── README.md                  

```

## Эндпоинты

| Метод  | URL                          | Описание                            | 
|--------|------------------------------|-------------------------------------|
| POST   | `/api/v1/auth/register`      | Создание нового пользователя        |
| POST   | `/api/v1/auth/login`         | Вход, получение токена              |
| POST   | `/api/v1/auth/logout`        | Выход из системы                    |
| GET    | `/api/v1/users?login=`       | Поиск пользователя по логину        |
| GET    | `/api/v1/users?name=`        | Поиск по маске имени/фамилии        |
| POST   | `/api/v1/wall/{user_id}`     | Добавление записи на стену          |
| GET    | `/api/v1/wall/{user_id}`     | Загрузка стены пользователя         |
| POST   | `/api/v1/messages`           | Отправка сообщения                  | 
| GET    | `/api/v1/messages?user_id=`  | Список сообщений пользователя       |

## HTTP коды ответов

- 200 — OK
- 201 — создано
- 400 — не хватает полей в запросе
- 401 — токен неверный или отсутствует
- 404 — ресурс не найден
- 409 — логин уже занят

## Тесты (curl)

### 1. Регистрация

```bash
curl -X POST http://127.0.0.1:8080/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{"login":"alina_gureeva","password":"bestpassword","first_name":"Alina","last_name":"Gureeva"}'
```

### 2. Логин (получение токена)

```bash
curl -X POST http://127.0.0.1:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"login":"alina_gureeva","password":"bestpassword"}'
```

Скопируйте `token` — он потребуется во всех последующих запросах

### 3. Поиск пользователя по логину

```bash
curl "http://127.0.0.1:8080/api/v1/users?login=alina_gureeva" \
  -H "Authorization: Bearer <TOKEN>"
```

### 4. Поиск по маске имени или фамилии

```bash
curl "http://127.0.0.1:8080/api/v1/users?name=Alina" \
  -H "Authorization: Bearer <TOKEN>"
```

### 5. Выход из системы

```bash
curl -X POST http://127.0.0.1:8080/api/v1/auth/logout \
  -H "Authorization: Bearer <TOKEN>"
```



