/*
 * Copyright 2025 MoDCore <https://moon-of-dragon.com/>
 * 	// Project Zero
 */
 
// Info: Call all MoDCore script here.

// #################################################### //
// Add them to Add_MoDCore_NameScripts(){ .. } without void. //
// #################################################### //

/* ######### Continents ######## > */
void Add_MoDCore_DragonflightScripts();
void Add_MoDCore_ShadowlandsScripts();
void Add_MoDCore_EasternKingdomsScripts();
/* < ######### Continents ######### */

/* ######### CUSTOM ######## > */
void AddSC_MoDCore_SolocraftScripts();
void AddSC_MoDCore_ScheduledShutdownScripts();
void AddSC_MoDCore_global_chat();
void AddSC_MoDCore_buff_commandscript();
/* < ######### CUSTOM ######### */

void AddMoDCoreScripts()
{
    /* ######### Continents ######## > */
	Add_MoDCore_DragonflightScripts();
	Add_MoDCore_ShadowlandsScripts();
	Add_MoDCore_EasternKingdomsScripts();
    /* < ######### Continents ######### */

    /* ######### CUSTOM ######## > */
    AddSC_MoDCore_SolocraftScripts();
    AddSC_MoDCore_ScheduledShutdownScripts();
	AddSC_MoDCore_global_chat();
    AddSC_MoDCore_buff_commandscript();
    /* < ######### CUSTOM ######### */
}

