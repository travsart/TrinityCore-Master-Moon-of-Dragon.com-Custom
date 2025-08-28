/*
 * Copyright DekkCore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

//Zones
void AddSC_zone_the_waking_shores();
void AddSC_zone_thaldraszus();

//BrackenHide Hollow
void AddSC_instance_brackenhide_hollow();
void AddSC_npc_rira_hackclaw();
void AddSC_boss_treemouth();
void AddSC_Boss_Gutshoot();

//dragonflight
void Add_MoDCore_DragonflightScripts()
{
    //Zones
	AddSC_zone_the_waking_shores();
	AddSC_zone_thaldraszus();
	
    //BRACKENHIDE HOLLOW
    AddSC_instance_brackenhide_hollow();
    AddSC_npc_rira_hackclaw();
    AddSC_boss_treemouth();
    AddSC_Boss_Gutshoot();
};
