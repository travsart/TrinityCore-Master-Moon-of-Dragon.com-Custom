-- ============================================================
-- NPC Subtitle (subname) Fixes
-- Source: Wowhead tooltip data cross-referenced with creature_template
-- Generated: 2026-03-01
-- Missing subtitles to add: 421
-- Mismatch fixes: 95
-- Total UPDATE statements: 516
-- ============================================================

-- ============================================================
-- SECTION 1: Mismatch fixes (DB has wrong/outdated subname)
-- ============================================================
USE `world`;
-- [remove-dev-tag] was: 'Weaponsmith <Temp>'
UPDATE `creature_template` SET `subname` = 'Weaponsmith' WHERE `entry` = 1467;
-- [remove-dev-tag] was: 'Miner <Temp>'
UPDATE `creature_template` SET `subname` = 'Miner' WHERE `entry` = 1567;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 33626;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 37742;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 39101;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 40109;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 46233;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 46234;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 46247;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 48622;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 50631;
-- [spelling-fix] was: 'Transmogrify'
UPDATE `creature_template` SET `subname` = 'Transmogrifier' WHERE `entry` = 62821;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 67716;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 67724;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 68275;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 68312;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 68328;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 68519;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 68740;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 68741;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 68945;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 68973;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 73474;
-- [remove-dev-tag] was: 'Iron Horde <PH VISUAL>'
UPDATE `creature_template` SET `subname` = 'Iron Horde' WHERE `entry` = 79103;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 85285;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 85757;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 88501;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 88502;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 88503;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 88679;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 88682;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 92223;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 99840;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105029;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105030;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105031;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105032;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105033;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105034;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105035;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105036;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 105037;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 108072;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 119411;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 119430;
-- [remove-dev-tag] was: 'Highkeeper <PH MODEL>'
UPDATE `creature_template` SET `subname` = 'Highkeeper' WHERE `entry` = 119885;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 130675;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 130676;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 131777;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 132583;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 132739;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 132800;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 136526;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 152501;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 152502;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 152503;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 152633;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 154747;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 155924;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 155932;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 155933;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 155934;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 155955;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 155956;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 157909;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 157911;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 157912;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 157913;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 157914;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 157915;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 161738;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 164196;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176591;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176592;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176593;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176594;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176597;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176599;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176600;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176602;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176603;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176604;
-- [spelling-fix] was: "Explorer's League"
UPDATE `creature_template` SET `subname` = 'Explorers'' League' WHERE `entry` = 176871;
-- [spelling-fix] was: 'Principle Architect'
UPDATE `creature_template` SET `subname` = 'Principal Architect' WHERE `entry` = 180967;
-- [spelling-fix] was: 'Principle Architect'
UPDATE `creature_template` SET `subname` = 'Principal Architect' WHERE `entry` = 182064;
-- [spelling-fix] was: 'Principle Architect'
UPDATE `creature_template` SET `subname` = 'Principal Architect' WHERE `entry` = 182065;
-- [spelling-fix] was: 'Principle Architect'
UPDATE `creature_template` SET `subname` = 'Principal Architect' WHERE `entry` = 182066;
-- [remove-dev-tag] was: '(T0) SWARMER'
UPDATE `creature_template` SET `subname` = 'The Scourge' WHERE `entry` = 182783;
-- [remove-dev-tag] was: 'Primalist <MAKE ME A TARASEK>'
UPDATE `creature_template` SET `subname` = 'Primalist' WHERE `entry` = 184401;
-- [spelling-fix] was: 'Principle Architect'
UPDATE `creature_template` SET `subname` = 'Principal Architect' WHERE `entry` = 186178;
-- [grammar-fix] was: "Decimus' Oculus"
UPDATE `creature_template` SET `subname` = 'Decimus''s Oculus' WHERE `entry` = 238287;
-- [grammar-fix] was: "Decimus' Oculus"
UPDATE `creature_template` SET `subname` = 'Decimus''s Oculus' WHERE `entry` = 238289;
-- [grammar-fix] was: "Decimus' Oculus"
UPDATE `creature_template` SET `subname` = 'Decimus''s Oculus' WHERE `entry` = 243196;
-- [grammar-fix] was: "Vidious' Ledgerkeep"
UPDATE `creature_template` SET `subname` = 'Vidious''s Ledgerkeep' WHERE `entry` = 257501;
-- [typo-fix] was: "Ziadan's' Assistant"
UPDATE `creature_template` SET `subname` = 'Ziadan''s Assistant' WHERE `entry` = 257502;

-- ============================================================
-- SECTION 2: Missing subtitles (421 entries)
-- These NPCs have a subtitle on Wowhead but NULL/empty in DB
-- ============================================================

UPDATE `creature_template` SET `subname` = 'Keeper of the Great Forge' WHERE `entry` = 11145 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Darkmoon Faire Cannoneer' WHERE `entry` = 15303 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Assurance' WHERE `entry` = 29296 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Assurance' WHERE `entry` = 29299 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Gone Fishin''' WHERE `entry` = 57850 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Traveling Trader' WHERE `entry` = 62822 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Firebird''s Challenge' WHERE `entry` = 85546 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Rivermane Chieftain' WHERE `entry` = 94255 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Rivermane Tribe' WHERE `entry` = 94286 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Dockmaster' WHERE `entry` = 142641 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Cenarion Circle' WHERE `entry` = 156344 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lord of the Blood Elves' WHERE `entry` = 161428 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hand of Doubt' WHERE `entry` = 168299 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Condemned Demon' WHERE `entry` = 169428 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hand of the Arbiter' WHERE `entry` = 174564 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Might of Earth' WHERE `entry` = 178425 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Defiant Dragonscale' WHERE `entry` = 184286 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Rugged Dragonscale' WHERE `entry` = 184288 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alliance Escort' WHERE `entry` = 184450 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Horde Escort' WHERE `entry` = 184451 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Horde Escort' WHERE `entry` = 184452 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Wielder of the Hydrosphere' WHERE `entry` = 185489 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Last Eggtender' WHERE `entry` = 185904 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacktalon Spymaster' WHERE `entry` = 186331 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Madam Goya Operative' WHERE `entry` = 186429 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacksmith' WHERE `entry` = 186430 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Rogue Trainer' WHERE `entry` = 186447 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Food and Provisions' WHERE `entry` = 186454 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Leader of the Shrineguard' WHERE `entry` = 186584 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hard Stuff' WHERE `entry` = 186752 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Leader of the Shrineguard' WHERE `entry` = 187119 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Head Egg-Tender' WHERE `entry` = 187130 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Endbringer Herald' WHERE `entry` = 187275 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ferry' WHERE `entry` = 187292 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Uktulut Chieftain' WHERE `entry` = 187323 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Traveling Dragonbrew Vendor' WHERE `entry` = 187444 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Sabellian''s Servant' WHERE `entry` = 187533 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Earthen Ring' WHERE `entry` = 187609 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Expedition Supplies' WHERE `entry` = 187700 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacksmithing & Engineering Supplies' WHERE `entry` = 187955 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Explorer-in-Training' WHERE `entry` = 188103 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stable Master' WHERE `entry` = 188107 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Cooking Supplies' WHERE `entry` = 188199 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Historian and Research Supplier' WHERE `entry` = 188265 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stable Master' WHERE `entry` = 188445 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Earthen Ring' WHERE `entry` = 188735 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'King of the Soggymaw' WHERE `entry` = 188859 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'King of the Windyfin' WHERE `entry` = 188860 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Earthen Ring' WHERE `entry` = 188908 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Orgrimmar Artisans Guild' WHERE `entry` = 189065 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Keeper of Renown' WHERE `entry` = 189226 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Enchanting Supplies' WHERE `entry` = 189448 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacksmithing Supplies' WHERE `entry` = 189732 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stable Master' WHERE `entry` = 189738 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Black Prince' WHERE `entry` = 189942 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Artisan''s Consortium' WHERE `entry` = 190042 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Orgrimmar Dock Master' WHERE `entry` = 190516 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Fishing Supplies' WHERE `entry` = 190526 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Transformation Trainer' WHERE `entry` = 190839 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacktalon Operative' WHERE `entry` = 191014 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Fishing Supplies' WHERE `entry` = 191161 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Nursery Director' WHERE `entry` = 191174 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trade Goods' WHERE `entry` = 191954 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trade Supplies' WHERE `entry` = 192026 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Engineering Supplies' WHERE `entry` = 192177 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trade Goods' WHERE `entry` = 192207 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trade Goods' WHERE `entry` = 192210 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Expedition Instructor' WHERE `entry` = 192298 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ambassador of the Artisan''s Consortium' WHERE `entry` = 192438 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flight Master' WHERE `entry` = 192472 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flight Master' WHERE `entry` = 192484 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flight Master' WHERE `entry` = 192491 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flight Master' WHERE `entry` = 192493 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ambassador of the Artisan''s Consortium' WHERE `entry` = 192498 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Artisan''s Consortium' WHERE `entry` = 192539 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Artisan''s Consortium' WHERE `entry` = 192574 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Profession Equipment Specialist' WHERE `entry` = 192674 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'General Goods & Repairs' WHERE `entry` = 193013 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stable Master' WHERE `entry` = 193021 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Cooking Trainer' WHERE `entry` = 193121 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Food and Drink' WHERE `entry` = 193310 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Dragonriding Trainer' WHERE `entry` = 193364 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Dragonriding Trainer' WHERE `entry` = 193411 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Explorer-In-Training' WHERE `entry` = 193506 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Explorer-In-Training' WHERE `entry` = 193513 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Explorer-In-Training' WHERE `entry` = 193516 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Earth Shatterer' WHERE `entry` = 193783 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Archaeologist' WHERE `entry` = 193915 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ferry' WHERE `entry` = 194099 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Rugged Dragonscale' WHERE `entry` = 194369 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Rugged Dragonscale' WHERE `entry` = 194378 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Rugged Dragonscale' WHERE `entry` = 194393 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Rugged Dragonscale' WHERE `entry` = 194437 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Provisioner' WHERE `entry` = 194470 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Cook' WHERE `entry` = 194471 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Obsidian Outcasts' WHERE `entry` = 194511 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stable Master' WHERE `entry` = 194605 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Repairs' WHERE `entry` = 194606 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Cartographer' WHERE `entry` = 194671 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master Alchemist' WHERE `entry` = 194829 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master Blacksmith' WHERE `entry` = 194836 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Obsidian Outcasts' WHERE `entry` = 194922 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ambassador of the Artisan''s Consortium' WHERE `entry` = 195386 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Expert Pet Trainer' WHERE `entry` = 196264 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Apprentice' WHERE `entry` = 196376 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Bicep of Neltharion' WHERE `entry` = 196680 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Bartender' WHERE `entry` = 196753 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Jaw of Neltharion' WHERE `entry` = 196767 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Explorer-In-Training' WHERE `entry` = 197272 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Explorer-In-Training' WHERE `entry` = 197275 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trade Supplies' WHERE `entry` = 197854 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Dragon Isles Dock Master' WHERE `entry` = 198079 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stormwind Dock Master' WHERE `entry` = 198146 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Earthen Ring' WHERE `entry` = 198157 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Earthen Ring' WHERE `entry` = 198160 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hunting Supplies' WHERE `entry` = 198186 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Expedition Mouser' WHERE `entry` = 198364 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Obsidian Warders' WHERE `entry` = 198748 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Uktulut Dock Master' WHERE `entry` = 198835 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Zen''shiri Trading Post' WHERE `entry` = 199164 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ferry' WHERE `entry` = 199684 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stable Master' WHERE `entry` = 199701 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Elwynn Homeowner''s Association' WHERE `entry` = 220609 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227801 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227878 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227894 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227896 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227899 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227900 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227901 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227902 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 227952 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Tushui' WHERE `entry` = 229260 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Huojin' WHERE `entry` = 229266 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Cart Driver' WHERE `entry` = 229268 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Huojin' WHERE `entry` = 229348 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Tushui' WHERE `entry` = 229349 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Tushui' WHERE `entry` = 229380 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Huojin' WHERE `entry` = 229381 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Wind' WHERE `entry` = 229407 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Water' WHERE `entry` = 229410 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Wind' WHERE `entry` = 229459 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Earth' WHERE `entry` = 229460 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Fire' WHERE `entry` = 229461 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Tushui' WHERE `entry` = 229477 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Huojin' WHERE `entry` = 229478 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Fireworks Master' WHERE `entry` = 229528 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hozen Chieftain' WHERE `entry` = 229529 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hozen Chieftain' WHERE `entry` = 229553 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Fire' WHERE `entry` = 229567 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 229757 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'ABILITIES' WHERE `entry` = 229914 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stromgarde Militia' WHERE `entry` = 229915 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stromgarde Militia' WHERE `entry` = 229941 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'ABILITIES' WHERE `entry` = 229942 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Highlands Mill' WHERE `entry` = 229954 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Engineering Supplies' WHERE `entry` = 229959 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Engineering Trainer' WHERE `entry` = 229960 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alchemy Supplies' WHERE `entry` = 229961 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alchemy Trainer' WHERE `entry` = 229962 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Tailoring Trainer' WHERE `entry` = 229963 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Tailoring Supplies' WHERE `entry` = 229964 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacksmithing Trainer' WHERE `entry` = 229966 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacksmithing Supplies' WHERE `entry` = 229967 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Enchanting Trainer' WHERE `entry` = 229968 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Food & Drink' WHERE `entry` = 229969 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Innkeeper' WHERE `entry` = 229972 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Portal Trainer' WHERE `entry` = 229975 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Apprentice Armorer' WHERE `entry` = 229976 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Armorer' WHERE `entry` = 229977 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Gryphon Master' WHERE `entry` = 229981 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Tushui' WHERE `entry` = 230135 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Sparring' WHERE `entry` = 230213 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'General of Ny''alotha' WHERE `entry` = 230321 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'SI:7 Operative' WHERE `entry` = 230396 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Tushui' WHERE `entry` = 230417 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lord of Heritage Forces' WHERE `entry` = 230659 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Newstead Farm' WHERE `entry` = 230703 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Highlands Cow of the Year' WHERE `entry` = 230705 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Highlands Mill' WHERE `entry` = 230712 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Huojin' WHERE `entry` = 230864 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Memorial Flowers' WHERE `entry` = 230902 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Harbinger' WHERE `entry` = 230937 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alchemy Supplies' WHERE `entry` = 231112 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Tushui' WHERE `entry` = 231114 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Twilight''s Blade' WHERE `entry` = 231131 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Twilight''s Blade' WHERE `entry` = 231132 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stromgarde Militia' WHERE `entry` = 231173 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alchemy Trainer' WHERE `entry` = 231184 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacksmithing Trainer' WHERE `entry` = 231185 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Innkeeper' WHERE `entry` = 231187 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Tailoring Trainer' WHERE `entry` = 231188 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alchemy Supplies' WHERE `entry` = 231189 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Food & Drink' WHERE `entry` = 231190 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Engineering Trainer' WHERE `entry` = 231191 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Water' WHERE `entry` = 231205 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Earth' WHERE `entry` = 231206 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Water' WHERE `entry` = 231214 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alchemy Trainer' WHERE `entry` = 231256 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Preacher of the Light' WHERE `entry` = 231275 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Wind' WHERE `entry` = 231286 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Armorer & Shieldcrafter' WHERE `entry` = 231299 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Apprentice Armorer' WHERE `entry` = 231300 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Blacksmith' WHERE `entry` = 231301 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Apprentice Blacksmith' WHERE `entry` = 231305 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Engineer' WHERE `entry` = 231306 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Water' WHERE `entry` = 231386 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Fire' WHERE `entry` = 231387 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Earth' WHERE `entry` = 231388 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Wind' WHERE `entry` = 231389 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231398 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Awakened Crest Exchange' WHERE `entry` = 231596 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Item Upgrades' WHERE `entry` = 231599 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231703 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231704 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Eternal Traveler' WHERE `entry` = 231710 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231742 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231743 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231750 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231751 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Highlands Mill' WHERE `entry` = 231761 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231817 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 231818 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Newstead Groundskeeper' WHERE `entry` = 231823 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hozen Chieftain' WHERE `entry` = 231873 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Huojin' WHERE `entry` = 231914 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Tushui' WHERE `entry` = 231915 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stromgarde Militia' WHERE `entry` = 231961 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stromgarde Militia' WHERE `entry` = 231963 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Innkeeper' WHERE `entry` = 232027 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stable Master' WHERE `entry` = 232030 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Superior Butcher' WHERE `entry` = 232031 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'General Goods' WHERE `entry` = 232032 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Tailoring Supplies' WHERE `entry` = 232033 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trade Goods' WHERE `entry` = 232035 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Leatherworking Supplies' WHERE `entry` = 232036 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Bowyer' WHERE `entry` = 232037 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Butcher' WHERE `entry` = 232038 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stromgarde Militia' WHERE `entry` = 232084 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Champion of the Banshee Queen' WHERE `entry` = 232191 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Tyrande''s Companion' WHERE `entry` = 232192 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Highlands Mill' WHERE `entry` = 232680 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stromgarde Militia' WHERE `entry` = 232695 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Leatherworking Supplies' WHERE `entry` = 232697 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Superior Butcher' WHERE `entry` = 232700 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Tailoring Supplies' WHERE `entry` = 232701 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Butcher' WHERE `entry` = 232707 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stable Master' WHERE `entry` = 232748 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Innkeeper' WHERE `entry` = 232764 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'General Goods' WHERE `entry` = 232766 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trade Goods' WHERE `entry` = 232774 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Bowyer' WHERE `entry` = 232776 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Captain of the Guard' WHERE `entry` = 232865 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Steward' WHERE `entry` = 233063 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ranger-General of the Silver Covenant' WHERE `entry` = 233252 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ranger-General of the Silver Covenant' WHERE `entry` = 233314 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Founder of the Trust' WHERE `entry` = 233566 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ranger-General of the Silver Covenant' WHERE `entry` = 233569 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Steward' WHERE `entry` = 233708 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Commander of the Paladin Order' WHERE `entry` = 233752 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ambassador of Ironforge' WHERE `entry` = 233753 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Commander of the Paladin Order' WHERE `entry` = 233757 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'King of Lordaeron' WHERE `entry` = 233804 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 234309 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 234310 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 235147 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 235149 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stromgarde Workshop' WHERE `entry` = 235184 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alchemy Trainer' WHERE `entry` = 235709 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Alliance Ambassador' WHERE `entry` = 235852 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'King of Lordaeron' WHERE `entry` = 236001 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 236111 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 236112 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 236113 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 236115 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 236116 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 236117 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 236118 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Flightmaster' WHERE `entry` = 236119 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Innkeeper' WHERE `entry` = 236814 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Amateur Historian' WHERE `entry` = 236850 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ancient Spirit of Wind' WHERE `entry` = 236989 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Master of Huojin' WHERE `entry` = 237294 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Head of the Dai-Lo Farmstead' WHERE `entry` = 237807 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 238920 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 238921 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 238940 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 238945 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trader' WHERE `entry` = 239431 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Earthen Ring' WHERE `entry` = 239442 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Reliquary' WHERE `entry` = 239482 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Reliquary' WHERE `entry` = 239485 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Reliquary' WHERE `entry` = 239498 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Cenarion Circle' WHERE `entry` = 239524 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Ranger-General of the Silver Covenant' WHERE `entry` = 239798 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Timewalking Vendor' WHERE `entry` = 239840 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Bait Seller' WHERE `entry` = 240210 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Fishmonger' WHERE `entry` = 240345 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Cursed Goods' WHERE `entry` = 240465 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'CLIENT SCENE ACTOR' WHERE `entry` = 240513 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'CLIENT SCENE ACTOR' WHERE `entry` = 240514 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Mage Trainer' WHERE `entry` = 241145 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lamplighter' WHERE `entry` = 242315 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Juggernaught' WHERE `entry` = 243266 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'High General of the Kaldorei' WHERE `entry` = 244647 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'General Supplies' WHERE `entry` = 244681 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'General Supplies' WHERE `entry` = 244932 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Experience Eliminator' WHERE `entry` = 245016 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Food Vendor' WHERE `entry` = 245026 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hellhunter' WHERE `entry` = 245379 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hellhunter' WHERE `entry` = 245380 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Storm-Eater' WHERE `entry` = 245754 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Expedition Escort' WHERE `entry` = 245774 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 245948 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 245949 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 246041 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 246104 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Hull''n''Home Outlet Merchant' WHERE `entry` = 246721 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Ebon Scales' WHERE `entry` = 247407 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Ebon Scales' WHERE `entry` = 247448 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Endeavor Trader' WHERE `entry` = 248525 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Loa of Pestilence' WHERE `entry` = 248597 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'The Blazing' WHERE `entry` = 248878 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Woodland Supplies' WHERE `entry` = 248978 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Fungal Finds' WHERE `entry` = 249303 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Steward' WHERE `entry` = 249848 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Steward' WHERE `entry` = 249850 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Housing Wayfinder' WHERE `entry` = 250203 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Minion of the Greench' WHERE `entry` = 251082 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Minion of the Greench' WHERE `entry` = 251195 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Used Decor Salesman' WHERE `entry` = 251911 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 251921 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 252043 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Transmogrifier' WHERE `entry` = 252230 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Vendor' WHERE `entry` = 252232 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Frederick''s Fabulous Furniture' WHERE `entry` = 252312 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Traveling Decor Specialist' WHERE `entry` = 252313 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Chandelier Maker' WHERE `entry` = 252316 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 252345 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 252498 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Displaced Carpenter' WHERE `entry` = 252520 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Minion of the Greench' WHERE `entry` = 252652 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Timewalking Vendor' WHERE `entry` = 252687 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 252887 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Freywold Furniture' WHERE `entry` = 252901 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 252910 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 252969 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 253067 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Treasure Hunter' WHERE `entry` = 253086 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Minion of the Greench' WHERE `entry` = 253103 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 253221 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lorewalker Cho''s Companion' WHERE `entry` = 253223 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 253227 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Library Display Enthusiast' WHERE `entry` = 253232 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 253235 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Specialist' WHERE `entry` = 253387 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Irongrove Goods' WHERE `entry` = 253434 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Minion of the Greench' WHERE `entry` = 253582 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Neighborhood Festivities' WHERE `entry` = 253772 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Neighborhood Festivities' WHERE `entry` = 254507 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Battleground Decor Specialist' WHERE `entry` = 254603 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Battleground Decor Specialist' WHERE `entry` = 254606 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'General Contractor' WHERE `entry` = 254687 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Steward' WHERE `entry` = 254688 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Stone Skimmer' WHERE `entry` = 255101 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'General Contractor' WHERE `entry` = 255104 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Dye Crafter' WHERE `entry` = 255125 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Dye Crafter' WHERE `entry` = 255126 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255203 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255213 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255216 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255218 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255221 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255222 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255228 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255230 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255278 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255297 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255298 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255299 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255301 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255319 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255325 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Decor Vendor' WHERE `entry` = 255326 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trader' WHERE `entry` = 255477 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lumberjack' WHERE `entry` = 255519 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Lumberjack' WHERE `entry` = 255520 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Fruit Seller' WHERE `entry` = 255630 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Sushi Chef' WHERE `entry` = 255631 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Mussel Seller' WHERE `entry` = 255632 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Shell Trader' WHERE `entry` = 255643 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Trader' WHERE `entry` = 255644 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Farm Trader' WHERE `entry` = 255647 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Pretzels and Beer' WHERE `entry` = 255651 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Tailor' WHERE `entry` = 255654 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Meat Vendor' WHERE `entry` = 255684 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Florist' WHERE `entry` = 255694 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Baker' WHERE `entry` = 255695 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Meat Vendor' WHERE `entry` = 255713 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Forager' WHERE `entry` = 255714 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Subject 16' WHERE `entry` = 255908 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Produce Vendor' WHERE `entry` = 255941 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Donut Vendor' WHERE `entry` = 255942 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Clothing Vendor' WHERE `entry` = 255943 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Bread Vendor' WHERE `entry` = 255945 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Plant Vendor' WHERE `entry` = 255946 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Fruit Vendor' WHERE `entry` = 255947 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Bartender' WHERE `entry` = 255948 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Mixologist' WHERE `entry` = 255949 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Towelkeeper' WHERE `entry` = 255950 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Traveling Book Shop' WHERE `entry` = 256071 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Steward' WHERE `entry` = 256078 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Traveling Book Shop' WHERE `entry` = 256119 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Public Transportation' WHERE `entry` = 256327 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Chauffeur' WHERE `entry` = 256632 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Toy Vendor' WHERE `entry` = 256636 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Driver' WHERE `entry` = 256641 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Driver' WHERE `entry` = 256644 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Preowned Parts' WHERE `entry` = 256750 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'Housing Dye Tester' WHERE `entry` = 256784 AND (`subname` IS NULL OR `subname` = '');
UPDATE `creature_template` SET `subname` = 'High Chieftain' WHERE `entry` = 257276 AND (`subname` IS NULL OR `subname` = '');

-- End of file
