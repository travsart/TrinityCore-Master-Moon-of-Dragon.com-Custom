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
void Add_MoDCore_SolocraftScripts();
void Add_MoDCore_ScheduledShutdownScripts();
/* < ######### CUSTOM ######### */

void AddMoDCoreScripts()
{
    /* ######### Continents ######## > */
	Add_MoDCore_DragonflightScripts();
	Add_MoDCore_ShadowlandsScripts();
	Add_MoDCore_EasternKingdomsScripts();
    /* < ######### Continents ######### */

    /* ######### CUSTOM ######## > */
    Add_MoDCore_SolocraftScripts();
    Add_MoDCore_ScheduledShutdownScripts();
    /* < ######### CUSTOM ######### */
}

