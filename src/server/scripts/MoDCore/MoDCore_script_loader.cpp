/*
 * Copyright 2025 GCCore
 * 	// Project Zero
 */
 
// Info: Call all gccore script here.

// #################################################### //
// Add them to Add_MoDCore_NameScripts(){ .. } without void. //
// #################################################### //

/* ######### Continents ######## > */
void Add_MoDCore_ShadowlandsScripts();
void Add_MoDCore_EasternKingdomsScripts();
/* < ######### Continents ######### */

/* ######### CUSTOM ######## > */
void Add_MoDCore_SolocraftScripts();
/* < ######### CUSTOM ######### */

void AddMoDCoreScripts()
{
    /* ######### Continents ######## > */
	Add_MoDCore_ShadowlandsScripts();
	Add_MoDCore_EasternKingdomsScripts();
    /* < ######### Continents ######### */

    /* ######### CUSTOM ######## > */
    Add_MoDCore_SolocraftScripts();
    /* < ######### CUSTOM ######### */
}

