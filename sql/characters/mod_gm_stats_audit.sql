-- ============================================================
-- GM Stats module: журнал использования ГМ-команд (characters DB)
-- Идемпотентный скрипт: безопасно запускать повторно.
-- ============================================================

CREATE TABLE IF NOT EXISTS `mod_gm_stats_audit` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `gm_guid` INT UNSIGNED NOT NULL COMMENT 'GM character GUID (low part)',
    `gm_name` VARCHAR(12) NOT NULL COMMENT 'GM character name (snapshot)',
    `command` VARCHAR(32) NOT NULL COMMENT 'Subcommand: server/player/top/zone/log',
    `arguments` VARCHAR(255) NOT NULL DEFAULT '' COMMENT 'Raw arguments (if enabled in config)',
    `used_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_gm_guid` (`gm_guid`),
    KEY `idx_used_at` (`used_at`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = 'GM Stats: command audit trail';
