/*
 * GM Stats for AzerothCore (master)
 * ------------------------------------------------
 * Config_GMStats.h — конфигурация модуля расширенной ГМ-статистики.
 *
 * Настройки: минимальный уровень доступа, лимиты топ-листов,
 * включение аудита команд.
 */

#ifndef GM_STATS_CONFIG_H
#define GM_STATS_CONFIG_H

namespace GMStats
{
    // ==================== Доступ ====================
    namespace Access
    {
        // Минимальный security level для всех команд модуля
        // (0 = игрок, 1 = модератор, 2 = ГМ, 3 = разработчик, 4 = админ).
        // По умолчанию 2 — обычные ГМ-и.
        constexpr uint32 MinSecurityLevel = 2;
    }

    // ==================== Топ-листы ====================
    namespace Top
    {
        // Сколько записей возвращают .gstats top-команды.
        constexpr uint32 Limit = 5;
    }

    // ==================== Аудит ====================
    namespace Audit
    {
        // Записывать ли использование команд модуля в mod_gm_stats_audit.
        constexpr bool Enabled = true;

        // Оставлять ли в логе текст аргументов (может содержать имена игроков).
        constexpr bool LogArguments = false;
    }
}

#endif // GM_STATS_CONFIG_H
