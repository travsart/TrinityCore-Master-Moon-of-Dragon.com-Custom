-- ============================================================
-- NPC Subtitle Revert: Remove false-positive subnames
-- 
-- An earlier, unfiltered version of npc_subname_fixes.sql was
-- applied to the world database. It contained ~863 false-positive
-- UPDATE statements that set subnames from Wowhead dev/internal
-- data (tier labels like 'T1 Melee', 'T0 Swarmer', etc.) on
-- entries that previously had empty subnames.
--
-- This script reverts only the entries that were actually changed
-- (had empty subname in LoreWalkerTDB baseline, now have a
-- false-positive value from the unfiltered Wowhead import).
--
-- Generated: 2026-03-01
-- Total UPDATE statements: 243
--
-- Breakdown:
--   tier_label: 243
-- ============================================================
USE `world`;
-- === tier_label (243 entries) ===
-- [Nerubian Fighter, Physical] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 211852;
-- [Nerubian Caster, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 211853;
-- [Nerubian Trueform, Civilian] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212068;
-- [Nerubian Priest, Royal] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212081;
-- [Nerubian Caster, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212082;
-- [Nerubian Fighter, Physical] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212083;
-- [Nerubian Priest, Royal] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212084;
-- [Larva, Bloodfeaster] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212103;
-- [Larva, Leech] was: 'T0 Swarmer (.3/.3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212105;
-- [Larva, Demon] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212106;
-- [Nerubian Crypt Lord, Physical] was: 'T2 (1.2/2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212509;
-- [Larva, Leech] was: 'T0 Swarmer (.3/.3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212514;
-- [Larva, Demon] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212516;
-- [Larva, Bloodfeaster] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212518;
-- [Spider, Underground] was: 'T0 Swarmer (.3/.3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212522;
-- [Nerubian Swarmite - Melee, Physical] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212538;
-- [Nerubian Caster, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212539;
-- [Nerubian Fighter, Physical] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212541;
-- [Spider, Underground] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212546;
-- [Nerubian Fighter, Physical] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212549;
-- [Nerubian Caster, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212550;
-- [Nerubian Evolved, Civilian] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212551;
-- [Spider, Underground] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212561;
-- [Nerubian Swarmite - Melee, Physical] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212931;
-- [Nerubian Priest, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212949;
-- [Nerubian Priest, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212950;
-- [Nerubian Priest, Dark] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212951;
-- [Nerubian Priest, Dark] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212952;
-- [Giant Slime, Void] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212960;
-- [Giant Slime, Ochre] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 212961;
-- [Nerubian Priest, Dark] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213372;
-- [Nerubian Priest, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213373;
-- [Nerubian Priest, Dark] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213374;
-- [Nerubian Priest, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213375;
-- [Kaheti Hulk, Physical] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213842;
-- [Nerubian Beetle, Swarmer] was: 'T0 (.3/.3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213845;
-- [Nerubian Beetle, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213847;
-- [Nerubian Beetle, Poison] was: 'T2 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213848;
-- [Stagshell, Physical] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213850;
-- [Stagshell, Physical] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213851;
-- [Stagshell, Swarmer] was: 'T0 (.3/.3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213852;
-- [Giant Slime, Void] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213854;
-- [Sureki Hulk, Physical] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 213874;
-- [Nerubian Swarmite - Caster, Bomber] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 214578;
-- [Spider, Underground] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 214805;
-- [Nerubian Swarmite - Melee, Physical] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215453;
-- [Nerubian Swarmite - Caster, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215455;
-- [Nerubian Swarmite - Melee, Physical] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215457;
-- [Nerubian Skitterling, Civilian] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215832;
-- [Nerubian Skitterling, Fighter] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215835;
-- [Nerubian Skitterling, Civilian] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215851;
-- [Nerubian Caster, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215856;
-- [Nerubian Caster, Frost] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215857;
-- [Nerubian Fighter, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215858;
-- [Nerubian Fighter, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215859;
-- [Nerubian Fighter, Frost] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215860;
-- [Nerubian Fighter, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215861;
-- [Nerubian Fighter, Frost] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215862;
-- [Nerubian Fighter, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215863;
-- [Nerubian Caster, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215865;
-- [Nerubian Caster, Frost] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215866;
-- [Nerubian Crypt Lord, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215867;
-- [Nerubian Swarmite - Caster, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215871;
-- [Nerubian Swarmite - Caster, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215872;
-- [Nerubian Swarmite - Melee, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215873;
-- [Nerubian Swarmite - Melee, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215874;
-- [Nerubian Swarmite - Melee, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215875;
-- [Nerubian Swarmite - Melee, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215876;
-- [Nerubian Swarmite - Caster, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215884;
-- [Nerubian Swarmite - Caster, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215886;
-- [Nerubian Beetle, Physical] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215887;
-- [Nerubian Beetle, Physical] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215888;
-- [Nerubian Hulk, Evolved - Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215895;
-- [Nerubian Hulk, Evolved - Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215896;
-- [Nerubian Skitterling, Fighter] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215901;
-- [Nerubian Skitterling, Civilian] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215902;
-- [Nerubian Caster, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215912;
-- [Nerubian Caster, Frost] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215914;
-- [Nerubian Fighter, Frost] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215916;
-- [Nerubian Fighter, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215917;
-- [Nerubian Fighter, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215918;
-- [Nerubian Swarmite - Caster, Bomber] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215919;
-- [Nerubian Swarmite - Caster, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215920;
-- [Nerubian Swarmite - Caster, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215921;
-- [Nerubian Swarmite - Melee, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215922;
-- [Nerubian Swarmite - Melee, Web] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215923;
-- [Nerubian Skitterling, Civilian] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215924;
-- [Nerubian Spiderling, Swarmer] was: 'T0 (.3/.3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215933;
-- [Nerubian Threadling, Swarmer] was: 'T0 (.3/.3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215938;
-- [Stagshell, Poison] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215940;
-- [Stagshell, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215942;
-- [Nerubian Caster, Frost] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215949;
-- [Nerubian Caster, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215950;
-- [Nerubian Fighter, Frost] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215953;
-- [Nerubian Fighter, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215954;
-- [Nerubian Fighter, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215955;
-- [Nerubian Swarmite - Caster, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215957;
-- [Nerubian Swarmite - Melee, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215959;
-- [Nerubian Swarmite - Melee, Web] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215960;
-- [Kaheti Hulk, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215962;
-- [Sureki Hulk, Poison] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215964;
-- [Nerubian Hulk, Evolved - Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215965;
-- [Nerubian Hulk, Evolved - Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 215966;
-- [Nerubian Hulk, Evolved - Fighter] was: 'T3 (4/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 218096;
-- [Nerubian Hulk, Evolved - Caster] was: 'T3 (3/4)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 218097;
-- [Nerubian Trueform, Civilian] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 218140;
-- [Nerubian Sageform, Civilian] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 218141;
-- [Nerubian Sageform, Civilian] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 218142;
-- [Nerubian Skitterling, Civilian] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 218143;
-- [Giant Slime, Void] was: 'T3 (5/2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221590;
-- [Stagshell, Poison] was: 'T3 (6/2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221591;
-- [Nerubian Fighter, Physical] was: 'T3 (4/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221594;
-- [Nerubian Caster, Web] was: 'T3 (3/4)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221595;
-- [Nerubian Swarmite - Caster, Poison] was: 'T3 (3/4)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221596;
-- [Nerubian Fighter, Web] was: 'T3 (4/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221607;
-- [Nerubian Fighter, Poison] was: 'T3 (4/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221609;
-- [Nerubian Fighter, Frost] was: 'T3 (4/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221611;
-- [Nerubian Caster, Poison] was: 'T3 (3/4)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221613;
-- [Nerubian Caster, Frost] was: 'T3 (3/4)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221614;
-- [Nerubian Swarmite - Melee, Poison] was: 'T3 (4/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221616;
-- [Spider, Underground] was: 'T3 (2.5/4)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 221619;
-- [Nerubian Skitterling, Fighter] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 225203;
-- [Larva, Bloodfeaster] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 225233;
-- [Giant Slime, Void] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 225235;
-- [Warp Stalker, Ghost] was: 'T1 (1.0/1.0)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 230515;
-- [Talbuk, Ghost] was: 'T1 (1.0/1.0)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 230516;
-- [Slateback, Ghost] was: 'T1 (1.0/1.0)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 230517;
-- [Flyer, Ghost] was: 'T1 (1.0/1.0)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 230520;
-- [Flyer, Ghost] was: 'T1 (1.0/1.0)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 230967;
-- [Beast, Tamable] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235507;
-- [Beast, Tamable] was: 'T0 Swarmer (.3/.3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235508;
-- [Beast, Tamable] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235509;
-- [Beast, Tamable] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235510;
-- [Beast, Tamable] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235511;
-- [Amani Soldier, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235536;
-- [Amani Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235538;
-- [Amani Soldier, Fighter] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235539;
-- [Amani Soldier, Caster] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235543;
-- [Amani Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235545;
-- [Amani Soldier, Caster] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235546;
-- [Amani Militia, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235548;
-- [Hex Soldier, Caster] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235551;
-- [Hex Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235552;
-- [Hex Soldier, Caster] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235553;
-- [Hex Soldier, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235554;
-- [Hex Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235555;
-- [Hex Soldier, Fighter] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235556;
-- [Amani Soldier, Scout] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235678;
-- [Amani Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235679;
-- [Amani Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235680;
-- [Hex Soldier, Scout] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235681;
-- [Hex Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235682;
-- [Hex Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235684;
-- [Amani Soldier, Scout] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235937;
-- [Amani Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235938;
-- [Amani Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235939;
-- [Amani Soldier, Fighter] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235940;
-- [Amani Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235941;
-- [Amani Soldier, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235942;
-- [Amani Soldier, Caster] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235943;
-- [Amani Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235944;
-- [Amani Soldier, Caster] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235945;
-- [Amani Militia, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235947;
-- [Vilebranch Soldier, Scout] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235953;
-- [Vilebranch Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235954;
-- [Vilebranch Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235955;
-- [Vilebranch Soldier, Fighter] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235956;
-- [Vilebranch Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235957;
-- [Vilebranch Soldier, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235958;
-- [Vilebranch Soldier, Caster] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235959;
-- [Vilebranch Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235960;
-- [Vilebranch Soldier, Caster] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 235961;
-- [Amani Soldier, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 236193;
-- [Amani Soldier, Caster] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 236194;
-- [Amani Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 236196;
-- [Amani Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 236197;
-- [Amani Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 236198;
-- [Amani Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 236199;
-- [Mountain Giant, Amani] was: 'T4 (8/4)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 236459;
-- [Mountain Giant, Amani] was: 'T4 (8/4)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 236463;
-- [Amani, Mounted (Bear)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 237049;
-- [Amani, Mounted (Eagle)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 237050;
-- [Amani, Mounted (Mammoth)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 237051;
-- [TB Mortal, Mounted (Cosmic Flyer)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 237052;
-- [Skeleton, Troll] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 237321;
-- [Giant Eagle, Undead] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 240618;
-- [Giant Eagle, Undead] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 240619;
-- [Giant Eagle, Undead] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 240620;
-- [Giant Eagle, Undead] was: 'T0 (.5/.5)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 240621;
-- [Swamp Worm, Emerged] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 245029;
-- [Amani Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 246338;
-- [Amani Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 246340;
-- [Witherbark Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247677;
-- [Witherbark Soldier, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247678;
-- [Witherbark Soldier, Caster] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247679;
-- [Witherbark Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247680;
-- [Witherbark Soldier, Caster] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247681;
-- [Witherbark Militia, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247682;
-- [Witherbark Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247684;
-- [Witherbark Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247685;
-- [Witherbark Soldier, Scout] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247686;
-- [Witherbark Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247687;
-- [Witherbark Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247688;
-- [Witherbark Soldier, Fighter] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247689;
-- [Revantusk Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247691;
-- [Revantusk Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247692;
-- [Revantusk Soldier, Scout] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247693;
-- [Revantusk Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247694;
-- [Revantusk Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247695;
-- [Revantusk Soldier, Fighter] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247696;
-- [Revantusk Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247697;
-- [Revantusk Soldier, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247698;
-- [Revantusk Soldier, Caster] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247699;
-- [Revantusk Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247700;
-- [Revantusk Soldier, Caster] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247701;
-- [Revantusk Militia, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247702;
-- [Shadowpine Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247704;
-- [Shadowpine Soldier, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247705;
-- [Shadowpine Soldier, Caster] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247706;
-- [Shadowpine Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247707;
-- [Shadowpine Soldier, Caster] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247708;
-- [Shadowpine Militia, Fighter] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247709;
-- [Shadowpine Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247711;
-- [Shadowpine Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247712;
-- [Shadowpine Soldier, Scout] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247713;
-- [Shadowpine Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247714;
-- [Shadowpine Soldier, Scout] was: 'T1 (1/1)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247715;
-- [Shadowpine Soldier, Fighter] was: 'T3 (5/3)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247716;
-- [Shadowpine, Mounted (Mammoth)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247717;
-- [Shadowpine, Mounted (Eagle)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247720;
-- [Shadowpine, Mounted (Bear)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247721;
-- [Revantusk, Mounted (Mammoth)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247722;
-- [Revantusk, Mounted (Eagle)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247723;
-- [Revantusk, Mounted (Bear)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247724;
-- [Witherbark, Mounted (Mammoth)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247725;
-- [Witherbark, Mounted (Eagle)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247726;
-- [Witherbark, Mounted (Bear)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 247727;
-- [Vilebranch, Mounted (Mammoth)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 248731;
-- [Vilebranch, Mounted (Bear)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 248732;
-- [Vilebranch, Mounted (Eagle)] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 248738;
-- [Shadowpine Soldier, Scout] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 258994;
-- [Shadowpine Soldier, Fighter] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 258995;
-- [Shadowpine Soldier, Caster] was: 'T2 (2/1.2)'
UPDATE `creature_template` SET `subname` = '' WHERE `entry` = 258996;

