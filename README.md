# GM Stats / Расширенная ГМ-статистика

**EN** | [RU ниже](#расширенная-гм-статистика--gm-stats-ru)

---

## EN

AzerothCore master module: extended statistics commands for Game Masters + command audit log.

### Commands (`.gstats`)

| Command | Description |
|---|---|
| `.gstats server` | Uptime, active sessions, player count, queue, online peak (since restart) |
| `.gstats player <name>` | Per-player summary: level, zone, played time (total/level), money, honor kills. Works for offline players too (basic data from characters DB) |
| `.gstats top` | Top-N (default 5): played time, richest, honor kills |
| `.gstats zone` | Current zone stats: players, creatures alive/dead, game objects |
| `.gstats log [days]` | Audit trail of `.gstats` usage (last N days, max 30, default 1) |

### Configuration (`src/Config_GMStats.h`)

| Setting | Default | Description |
|---|---|---|
| `Access::MinSecurityLevel` | `2` (SEC_GAMEMASTER) | Minimum access level for all commands |
| `Top::Limit` | `5` | Entries returned by top commands |
| `Audit::Enabled` | `true` | Write command usage to `mod_gm_stats_audit` |
| `Audit::LogArguments` | `false` | Store raw command arguments (may contain player names) |

### Structure

```
src/
├── Config_GMStats.h        All settings (constexpr, C++17)
└── GMStats.cpp             ChatCommandScript (commands) + WorldScript (online peak)

sql/characters/
└── mod_gm_stats_audit.sql  Audit table (characters DB, idempotent)
```

### Installation

1. Clone into `modules/` of your AzerothCore master build, recompile the core.
2. Apply `sql/characters/mod_gm_stats_audit.sql` to the **characters** DB.
3. Verify command API signatures (`ChatCommandTable`, `CommandScript`) against your core commit.
4. Tune access level / limits in `Config_GMStats.h` + rebuild.

### Design notes

- Heavy DB queries run **only on explicit GM request** — never in `OnUpdate`.
- Online peak is sampled once per minute (negligible overhead).
- Audit writes are fire-and-forget async (`Execute`), never block the world thread.

---

## Расширенная ГМ-статистика / GM Stats — RU

Модуль AzerothCore master: расширенные команды статистики для ГМов + журнал использования команд.

### Команды (`.gstats`)

| Команда | Описание |
|---|---|
| `.gstats server` | Аптайм, активные сессии, игроков онлайн, очередь, пик онлайна (с момента рестарта) |
| `.gstats player <имя>` | Сводка по игроку: уровень, зона, время в игре (всего/на уровне), золото, почётные убийства. Работает и для офлайн-игроков (базовые данные из characters DB) |
| `.gstats top` | Топ-N (по умолчанию 5): время в игре, богатейшие, почётные убийства |
| `.gstats zone` | Статистика текущей зоны: игроки, существа (живые/мёртвые), игровые объекты |
| `.gstats log [дней]` | Журнал использования `.gstats` (последние N дней, максимум 30, по умолчанию 1) |

### Настройка (`src/Config_GMStats.h`)

| Параметр | По умолчанию | Описание |
|---|---|---|
| `Access::MinSecurityLevel` | `2` (SEC_GAMEMASTER) | Минимальный уровень доступа для всех команд |
| `Top::Limit` | `5` | Число записей в топ-листах |
| `Audit::Enabled` | `true` | Записывать использование команд в `mod_gm_stats_audit` |
| `Audit::LogArguments` | `false` | Сохранять аргументы команд (могут содержать имена игроков) |

### Структура

```
src/
├── Config_GMStats.h        Все настройки (constexpr, C++17)
└── GMStats.cpp             ChatCommandScript (команды) + WorldScript (пик онлайна)

sql/characters/
└── mod_gm_stats_audit.sql  Таблица аудита (characters DB, идемпотентный)
```

### Установка

1. Склонировать модуль в `modules/` сборки AzerothCore master, пересобрать ядро.
2. Применить `sql/characters/mod_gm_stats_audit.sql` к **characters DB**.
3. Сверить сигнатуры командного API (`ChatCommandTable`, `CommandScript`) с вашим коммитом ядра.
4. Настройка уровня доступа/лимитов — в `Config_GMStats.h` + пересборка.

### Архитектурные решения

- Тяжёлые запросы к БД выполняются **только по явному запросу ГМа** — никогда в `OnUpdate`.
- Пик онлайна сэмплируется раз в минуту (незначительная нагрузка).
- Записи аудита асинхронные fire-and-forget (`Execute`), не блокируют мировой поток.
