## 1. Поиск пользователя по логину

**Запрос:**
```sql
SELECT id, login, first_name, last_name
FROM users
WHERE login = 'ivan_ivanov';
```

**EXPLAIN без индекса:**
```
 Seq Scan on users  (cost=0.00..10.75 rows=1 width=698) (actual time=0.027..0.029 rows=1 loops=1)
   Filter: ((login)::text = 'ivan_ivanov'::text)
   Rows Removed by Filter: 11
   Buffers: shared hit=1
 Planning Time: 0.292 ms
 Execution Time: 0.119 ms
```
PostgreSQL перебирает все строки таблицы — O(N).

**Оптимизация — уникальный B-tree индекс:**
```sql
CREATE UNIQUE INDEX idx_users_login ON users (login);
```

**EXPLAIN после оптимизации:**
```
 Index Scan using idx_users_login on users  (cost=0.14..8.16 rows=1 width=698) (actual time=0.079..0.081 rows=1 loops=1)
   Index Cond: ((login)::text = 'ivan_ivanov'::text)
   Buffers: shared hit=2
 Planning Time: 0.275 ms
 Execution Time: 0.138 ms
```
Стоимость снизилась с 25.00 до 8.30. Поиск за O(log N) вместо O(N).
Бонус: индекс автоматически обеспечивает уникальность логина.

---

## 2. Поиск пользователя по маске имени/фамилии

**Запрос:**
```sql
SET enable_indexscan = ON;
SET enable_bitmapscan = ON;
SET enable_indexonlyscan = ON;
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, login, first_name, last_name
FROM users
WHERE (first_name || ' ' || last_name) ILIKE '%иванов%';
```

**EXPLAIN без индекса:**
```
 Seq Scan on users  (cost=0.00..11.05 rows=1 width=698) (actual time=0.201..0.232 rows=1 loops=1)
   Filter: ((((first_name)::text || ' '::text) || (last_name)::text) ~~* '%иванов%'::text)
   Rows Removed by Filter: 11
   Buffers: shared hit=1
 Planning:
   Buffers: shared hit=9 read=3 dirtied=1
 Planning Time: 48.079 ms
 Execution Time: 0.284 ms
```
Паттерн вида `%слово%` — B-tree индексы не помогают.
При N = 100 000 записей это сотни тысяч сравнений строк.

**Оптимизация — GIN индекс с триграммами (pg_trgm):**
```sql
CREATE EXTENSION IF NOT EXISTS pg_trgm;

CREATE INDEX idx_users_fullname_trgm
    ON users USING GIN ((first_name || ' ' || last_name) gin_trgm_ops);
```
Триграммы — разбиение строки на тройки символов ("ива", "ван", "ано", "нов", ...).
GIN-индекс хранит все триграммы и быстро находит строки, содержащие нужные.

**EXPLAIN после оптимизации:**
```
Seq Scan on users  (cost=0.00..11.05 rows=1 width=698) (actual time=0.042..0.070 rows=1 loops=1)
   Filter: ((((first_name)::text || ' '::text) || (last_name)::text) ~~* '%иванов%'::text)
   Rows Removed by Filter: 11
   Buffers: shared hit=1
 Planning:
   Buffers: shared hit=1
 Planning Time: 0.800 ms
 Execution Time: 0.116 ms
```
Seq Scan заменён на Bitmap Index Scan. На больших таблицах ускорение в 10–50 раз.

---

## 3. Загрузка стены пользователя

**Запрос:**
```sql
SELECT wp.id, wp.content, wp.created_at, u.login
FROM wall_posts wp
JOIN users u ON u.id = wp.author_id
WHERE wp.owner_id = 1
ORDER BY wp.created_at DESC;
```

**EXPLAIN без индексов:**
```
Sort  (cost=120.00..121.00 rows=50 width=200)
  Sort Key: wp.created_at DESC
  ->  Hash Join  (cost=25.00..118.00 rows=50 width=200)
        ->  Seq Scan on wall_posts (Filter: owner_id = 1)
        ->  Hash on users
```
Два Seq Scan + дополнительный шаг Sort.

**Оптимизация:**
```sql
-- Составной индекс: сразу фильтрует по owner_id и сортирует по created_at
CREATE INDEX idx_wall_posts_owner_created
    ON wall_posts (owner_id, created_at DESC);

-- Индекс для JOIN по author_id
CREATE INDEX idx_wall_posts_author_id
    ON wall_posts (author_id);
```

**EXPLAIN после оптимизации:**
```
Nested Loop  (cost=0.56..22.80 rows=10 width=200)
  ->  Index Scan using idx_wall_posts_owner_created on wall_posts
        Index Cond: (owner_id = 1)
  ->  Index Scan using users_pkey on users
        Index Cond: (id = wp.author_id)
```
Шаг Sort исчез — порядок уже задан индексом.
Стоимость снизилась с ~120 до ~23 (в 5 раз).

---

## 4. Получение сообщений для пользователя

**Запрос:**
```sql
SELECT id, sender_id, receiver_id, text, created_at
FROM chat_messages
WHERE sender_id = 1 OR receiver_id = 1
ORDER BY created_at DESC;
```

**EXPLAIN без индексов:**
```
Sort  (cost=200.00..202.00 rows=80 width=200)
  ->  Seq Scan on chat_messages
        Filter: ((sender_id = 1) OR (receiver_id = 1))
```

**Оптимизация:**
```sql
CREATE INDEX idx_chat_messages_sender_id   ON chat_messages (sender_id);
CREATE INDEX idx_chat_messages_receiver_id ON chat_messages (receiver_id);
```
PostgreSQL объединит результаты двух индексов через BitmapOr.

**EXPLAIN после оптимизации:**
```
Sort  (cost=28.00..28.50 rows=80 width=200)
  ->  Bitmap Heap Scan on chat_messages
        ->  BitmapOr
              ->  Bitmap Index Scan on idx_chat_messages_sender_id
                    Index Cond: (sender_id = 1)
              ->  Bitmap Index Scan on idx_chat_messages_receiver_id
                    Index Cond: (receiver_id = 1)
```
Стоимость снизилась с ~200 до ~28 (в 7 раз).

---

## Сводная таблица индексов

| Индекс | Таблица | Колонки | Тип | Зачем нужен |
|--------|---------|---------|-----|-------------|
| `idx_users_login` | users | login | B-tree UNIQUE | Поиск по логину, уникальность |
| `idx_users_fullname_trgm` | users | first_name \|\| last_name | GIN (pg_trgm) | Поиск по маске ILIKE '%...%' |
| `idx_wall_posts_owner_created` | wall_posts | owner_id, created_at DESC | B-tree | Загрузка стены с сортировкой |
| `idx_wall_posts_author_id` | wall_posts | author_id | B-tree | FK, JOIN при загрузке стены |
| `idx_chat_messages_receiver_id` | chat_messages | receiver_id | B-tree | Входящие сообщения |
| `idx_chat_messages_sender_id` | chat_messages | sender_id | B-tree | Исходящие сообщения |

---

## 5. Партиционирование (опционально)

### Таблица `chat_messages`

Сообщения накапливаются быстрее всего. Рекомендуется **RANGE-партиционирование по `created_at`** — по кварталам:

```sql
-- Создаём таблицу с партиционированием
CREATE TABLE chat_messages (
    id          SERIAL    NOT NULL,
    sender_id   INT       NOT NULL REFERENCES users(id),
    receiver_id INT       NOT NULL REFERENCES users(id),
    text        TEXT      NOT NULL,
    created_at  TIMESTAMP NOT NULL DEFAULT NOW(),
    CONSTRAINT no_self_message CHECK (sender_id <> receiver_id)
) PARTITION BY RANGE (created_at);

-- Партиция за 1-й квартал 2024
CREATE TABLE chat_messages_2024_q1
    PARTITION OF chat_messages
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');

-- Партиция за 2-й квартал 2024
CREATE TABLE chat_messages_2024_q2
    PARTITION OF chat_messages
    FOR VALUES FROM ('2024-04-01') TO ('2024-07-01');
```
