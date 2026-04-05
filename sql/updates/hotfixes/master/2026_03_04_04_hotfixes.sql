-- 2026_03_04_04_hotfixes.sql
-- RoleplayCore -- Remove 174,799 orphaned hotfix_data entries
-- These reference records deleted by R1+R2 hotfix redundancy cleanup.
-- Server would try to push nonexistent hotfix records to connecting clients.

-- Hotfix companion cleanup: remove orphaned hotfix_data entries
-- These reference records deleted by R1+R2 hotfix redundancy cleanup.
-- 174,799 orphaned rows across 72 tables.
-- Generated: 2026-03-04

SET innodb_lock_wait_timeout = 600;

-- ============================================================
-- SECTION 1: Delete ALL hotfix_data for fully-emptied tables
-- ============================================================

-- names_profanity: 10,842 rows
-- ?unmapped?: 39 rows
-- ?unmapped?: 1 rows
DELETE FROM `hotfixes`.`hotfix_data` WHERE `TableHash` IN (3666008428, 894855762, 3359787322);
-- Section 1 total: 10,882 rows

-- ============================================================
-- SECTION 2: Delete orphaned RecordIds from partially-cleaned tables
-- ============================================================

-- spell_effect: ~90,535 orphaned rows (hash 4030871717)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_effect` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 4030871717 AND t.ID IS NULL;

-- item_sparse: ~44,578 orphaned rows (hash 2442913102)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_sparse` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2442913102 AND t.ID IS NULL;

-- spell: ~8,062 orphaned rows (hash 3776013982)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3776013982 AND t.ID IS NULL;

-- transmog_set: ~4,874 orphaned rows (hash 356071576)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`transmog_set` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 356071576 AND t.ID IS NULL;

-- sound_kit: ~3,688 orphaned rows (hash 908293937)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`sound_kit` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 908293937 AND t.ID IS NULL;

-- item_search_name: ~3,097 orphaned rows (hash 428746933)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_search_name` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 428746933 AND t.ID IS NULL;

-- spell_misc: ~1,042 orphaned rows (hash 3322146344)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_misc` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3322146344 AND t.ID IS NULL;

-- spell_name: ~935 orphaned rows (hash 1187407512)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_name` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1187407512 AND t.ID IS NULL;

-- modifier_tree: ~900 orphaned rows (hash 2120822102)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`modifier_tree` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2120822102 AND t.ID IS NULL;

-- spell_visual_kit: ~633 orphaned rows (hash 4102286043)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_visual_kit` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 4102286043 AND t.ID IS NULL;

-- item_bonus_tree_node: ~608 orphaned rows (hash 2793276977)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_bonus_tree_node` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2793276977 AND t.ID IS NULL;

-- creature_difficulty: ~563 orphaned rows (hash 3386943305)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`creature_difficulty` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3386943305 AND t.ID IS NULL;

-- curve_point: ~438 orphaned rows (hash 1880017466)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`curve_point` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1880017466 AND t.ID IS NULL;

-- curve: ~320 orphaned rows (hash 1272569722)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`curve` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1272569722 AND t.ID IS NULL;

-- trait_definition: ~286 orphaned rows (hash 2995956864)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`trait_definition` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2995956864 AND t.ID IS NULL;

-- item: ~285 orphaned rows (hash 1344507586)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1344507586 AND t.ID IS NULL;

-- modified_crafting_item: ~281 orphaned rows (hash 3465080266)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`modified_crafting_item` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3465080266 AND t.ID IS NULL;

-- player_condition: ~240 orphaned rows (hash 3108775943)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`player_condition` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3108775943 AND t.ID IS NULL;

-- spell_x_spell_visual: ~228 orphaned rows (hash 666345498)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_x_spell_visual` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 666345498 AND t.ID IS NULL;

-- criteria_tree: ~220 orphaned rows (hash 1255424668)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`criteria_tree` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1255424668 AND t.ID IS NULL;

-- item_x_bonus_tree: ~197 orphaned rows (hash 844464874)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_x_bonus_tree` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 844464874 AND t.ID IS NULL;

-- lfg_dungeons: ~196 orphaned rows (hash 2577119682)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`lfg_dungeons` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2577119682 AND t.ID IS NULL;

-- scene_script_text: ~196 orphaned rows (hash 537670055)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`scene_script_text` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 537670055 AND t.ID IS NULL;

-- spell_reagents: ~144 orphaned rows (hash 2875640223)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_reagents` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2875640223 AND t.ID IS NULL;

-- world_state_expression: ~116 orphaned rows (hash 492039028)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`world_state_expression` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 492039028 AND t.ID IS NULL;

-- item_effect: ~96 orphaned rows (hash 1073915313)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_effect` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1073915313 AND t.ID IS NULL;

-- journal_encounter_section: ~91 orphaned rows (hash 2035710060)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`journal_encounter_section` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2035710060 AND t.ID IS NULL;

-- spell_interrupts: ~79 orphaned rows (hash 1720692227)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_interrupts` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1720692227 AND t.ID IS NULL;

-- creature: ~64 orphaned rows (hash 3386291891)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`creature` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3386291891 AND t.ID IS NULL;

-- spell_label: ~64 orphaned rows (hash 813076512)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_label` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 813076512 AND t.ID IS NULL;

-- content_tuning: ~63 orphaned rows (hash 3174446591)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`content_tuning` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3174446591 AND t.ID IS NULL;

-- item_x_item_effect: ~60 orphaned rows (hash 13330255)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_x_item_effect` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 13330255 AND t.ID IS NULL;

-- spell_target_restrictions: ~59 orphaned rows (hash 3764692828)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_target_restrictions` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3764692828 AND t.ID IS NULL;

-- spell_categories: ~58 orphaned rows (hash 3689412649)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_categories` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3689412649 AND t.ID IS NULL;

-- chr_customization_choice: ~57 orphaned rows (hash 1746734909)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`chr_customization_choice` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1746734909 AND t.ID IS NULL;

-- item_bonus: ~55 orphaned rows (hash 2199425034)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_bonus` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2199425034 AND t.ID IS NULL;

-- item_spec_override: ~53 orphaned rows (hash 345681529)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_spec_override` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 345681529 AND t.ID IS NULL;

-- quest_v2: ~50 orphaned rows (hash 986198281)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`quest_v2` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 986198281 AND t.ID IS NULL;

-- spell_aura_options: ~44 orphaned rows (hash 4096770149)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_aura_options` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 4096770149 AND t.ID IS NULL;

-- item_currency_cost: ~39 orphaned rows (hash 1876974313)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_currency_cost` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1876974313 AND t.ID IS NULL;

-- spell_scaling: ~33 orphaned rows (hash 1089023333)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_scaling` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1089023333 AND t.ID IS NULL;

-- trait_node_group_x_trait_node: ~28 orphaned rows (hash 447132635)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`trait_node_group_x_trait_node` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 447132635 AND t.ID IS NULL;

-- expected_stat_mod: ~25 orphaned rows (hash 671633915)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`expected_stat_mod` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 671633915 AND t.ID IS NULL;

-- world_effect: ~24 orphaned rows (hash 3664936999)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`world_effect` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3664936999 AND t.ID IS NULL;

-- trait_node: ~24 orphaned rows (hash 3779276131)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`trait_node` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3779276131 AND t.ID IS NULL;

-- spell_cooldowns: ~23 orphaned rows (hash 4193483863)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_cooldowns` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 4193483863 AND t.ID IS NULL;

-- wmo_area_table: ~21 orphaned rows (hash 3343891947)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`wmo_area_table` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3343891947 AND t.ID IS NULL;

-- skill_line_ability: ~18 orphaned rows (hash 4282664694)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`skill_line_ability` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 4282664694 AND t.ID IS NULL;

-- currency_types: ~17 orphaned rows (hash 793851790)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`currency_types` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 793851790 AND t.ID IS NULL;

-- item_extended_cost: ~15 orphaned rows (hash 3146089301)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_extended_cost` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3146089301 AND t.ID IS NULL;

-- trait_edge: ~13 orphaned rows (hash 2496704385)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`trait_edge` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2496704385 AND t.ID IS NULL;

-- conditional_content_tuning: ~11 orphaned rows (hash 1913965383)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`conditional_content_tuning` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1913965383 AND t.ID IS NULL;

-- global_strings: ~10 orphaned rows (hash 3205218938)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`global_strings` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3205218938 AND t.ID IS NULL;

-- item_appearance: ~8 orphaned rows (hash 1109793673)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`item_appearance` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1109793673 AND t.ID IS NULL;

-- spell_casting_requirements: ~8 orphaned rows (hash 1627543382)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_casting_requirements` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1627543382 AND t.ID IS NULL;

-- phase: ~7 orphaned rows (hash 4040235363)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`phase` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 4040235363 AND t.ID IS NULL;

-- adventure_journal: ~6 orphaned rows (hash 2398034583)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`adventure_journal` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2398034583 AND t.ID IS NULL;

-- broadcast_text: ~5 orphaned rows (hash 35137211)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`broadcast_text` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 35137211 AND t.ID IS NULL;

-- map: ~5 orphaned rows (hash 3179597154)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`map` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3179597154 AND t.ID IS NULL;

-- trait_node_entry: ~5 orphaned rows (hash 2196297174)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`trait_node_entry` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2196297174 AND t.ID IS NULL;

-- creature_model_data: ~4 orphaned rows (hash 2137612197)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`creature_model_data` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2137612197 AND t.ID IS NULL;

-- spell_aura_restrictions: ~3 orphaned rows (hash 3130494798)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_aura_restrictions` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3130494798 AND t.ID IS NULL;

-- vehicle: ~3 orphaned rows (hash 178219711)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`vehicle` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 178219711 AND t.ID IS NULL;

-- spell_power: ~2 orphaned rows (hash 2712461791)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`spell_power` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 2712461791 AND t.ID IS NULL;

-- achievement: ~1 orphaned rows (hash 3538824359)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`achievement` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3538824359 AND t.ID IS NULL;

-- tact_key: ~1 orphaned rows (hash 3744420815)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`tact_key` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3744420815 AND t.ID IS NULL;

-- chr_races: ~1 orphaned rows (hash 1408333884)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`chr_races` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1408333884 AND t.ID IS NULL;

-- area_table: ~1 orphaned rows (hash 1918102339)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`area_table` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 1918102339 AND t.ID IS NULL;

-- scene_script_package: ~1 orphaned rows (hash 3905641993)
DELETE hd FROM `hotfixes`.`hotfix_data` hd
  LEFT JOIN `hotfixes`.`scene_script_package` t ON hd.RecordId = t.ID
  WHERE hd.TableHash = 3905641993 AND t.ID IS NULL;

-- Section 2 total: ~163,917 rows
-- Grand total: ~174,799 rows