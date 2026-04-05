CREATE DATABASE `world` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE DATABASE `characters` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE DATABASE `auth` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE DATABASE `hotfixes` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE DATABASE `roleplay` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE DATABASE `playerbot` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

GRANT ALL PRIVILEGES ON `world` . * TO 'trinity'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON `characters` . * TO 'trinity'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON `auth` . * TO 'trinity'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON `hotfixes` . * TO 'trinity'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON `roleplay` . * TO 'trinity'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON `playerbot` . * TO 'trinity'@'localhost' WITH GRANT OPTION;