-- ============================================================================
-- Companion Squad System — Auth DB RBAC
-- Run against: auth
-- ============================================================================
USE `auth`;

INSERT IGNORE INTO `rbac_permissions` (`id`, `name`) VALUES
(3008, 'Command: .comp');

-- Grant to all players (secId 0 = default player)
INSERT IGNORE INTO `rbac_default_permissions` (`secId`, `permissionId`) VALUES
(0, 3008);
