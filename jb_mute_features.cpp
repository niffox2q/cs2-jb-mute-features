#include "jb_mute_features.h"
#include <random>
#include <cstdio>
#include <algorithm>

#define MAX_PLAYERS 64

#define CS_TEAM_NONE 0
#define CS_TEAM_SPECTATOR 1
#define CS_TEAM_T 2
#define CS_TEAM_CT 3

jb_mute_features g_jb_mute_features;
PLUGIN_EXPOSE(jb_mute_features, g_jb_mute_features);

// SYSTEM API`s
IVEngineServer2* engine = nullptr;
CGlobalVars* gpGlobals = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

// API
IUtilsApi* utils;
IJailbreakApi* jailbreak_api;
IAdminApi* admin_api;
IVIPApi* vip_api;

// =========================================
// CONFIG VARS
// =========================================

std::string sAdminPermission = "@admin/root";
std::vector<uint64_t> vSteamIDs;

bool g_bImmunityGaved[MAX_PLAYERS+1] = {false};

//==========================================
// HELPERS
//==========================================

void PlaySlotSound(int iSlot, const char* path){
    engine->ClientCommand(iSlot,"play %s",path);
}

void PlaySoundAll(const char* path){
    for(int i = 0; i < MAX_PLAYERS;i++){
        engine->ClientCommand(i,"play %s",path);
    }
}

std::vector<uint64_t> ParseUInt64List(const char* str) {
    std::vector<uint64_t> result;
    if (!str || str[0] == '\0') return result;

    const char* current = str;
    char* end;

    while (*current != '\0') {
        uint64_t value = std::strtoull(current, &end, 10);
        
        if (current != end) {
            result.push_back(value);
        } else {

            end++; 
        }

        current = end;
        while (*current == ',' || *current == ' ') {
            current++;
        }
    }

    return result;
}

// =========================================
// CONFIGS 
// =========================================
void LoadConfig() {
    KeyValues* config = new KeyValues("Config");
    const char* path = "addons/configs/Jailbreak/mute_features.ini";
    if (!config->LoadFromFile(g_pFullFileSystem, path)) {
        utils->ErrorLog("%s Failed to load: %s",g_PLAPI->GetLogTag(), path);
        delete config;
        return;
    }

    sAdminPermission = config->GetString("AdminPermission","@admin/root");
    vSteamIDs = ParseUInt64List(config->GetString("SteamIDs",""));

    delete config;
}

bool IsSteamIDSame(int iSlot){
    auto pController = CCSPlayerController::FromSlot(iSlot);
    uint64_t iSteamID = 0;
    if (pController) iSteamID = pController->m_steamID.Get();
    for (auto& allowedSteamID : vSteamIDs) {
        if (allowedSteamID == iSteamID) return true;
    } return false;
}


// =========================================
// OTHER
// =========================================


CGameEntitySystem* GameEntitySystem() {
    return utils ? utils->GetCGameEntitySystem() : nullptr;
}



void StartupServer() {
    g_pGameEntitySystem = GameEntitySystem();
    g_pEntitySystem = utils->GetCEntitySystem();
    gpGlobals = utils->GetCGlobalVars();
}

bool jb_mute_features::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late) {
    PLUGIN_SAVEVARS();

    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkSystem, INetworkSystem, NETWORKSYSTEM_INTERFACE_VERSION);

    ConVar_Register(FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);
    g_SMAPI->AddListener(this, this);

    return true;
}



void jb_mute_features::AllPluginsLoaded() {
    int ret;
    utils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing UTILS plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    jailbreak_api =(IJailbreakApi*)g_SMAPI->MetaFactory(JAILBREAK_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing Jailbreak Core plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    admin_api = (IAdminApi*)g_SMAPI->MetaFactory(Admin_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing Admin System plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    vip_api = (IVIPApi*)g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing Vip Core plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    LoadConfig();

    vip_api->VIP_RegisterFeature("jb_mute_immunity",VIP_BOOL,HIDE);


    utils->HookEvent(g_PLID, "round_prestart", [](const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_bImmunityGaved[i]) continue;
        
        auto pController = CCSPlayerController::FromSlot(i);
        if (!pController) continue; 
        
        uint64_t currentSteamID = pController->m_steamID.Get();

        bool bIsAdmin = (admin_api->IsAdmin(i) && admin_api->HasPermission(i, sAdminPermission.c_str()));
        bool bIsSteamSame = IsSteamIDSame(i);
        bool bIsVip = (vip_api->VIP_IsClientVIP(i) && vip_api->VIP_GetClientFeatureBool(i, "jb_mute_immunity"));


        if (bIsAdmin || bIsSteamSame || bIsVip) {
            jailbreak_api->GiveMuteImmunity(i);
            g_bImmunityGaved[i] = true;
        }
    }
});

    utils->HookEvent(g_PLID,"player_disconnect",[](const char* szName, IGameEvent* pEvent, bool bDontBroadcast){
        int iSlot = pEvent->GetInt("userid");
        if (g_bImmunityGaved[iSlot]) {
            jailbreak_api->RemoveMuteImmunity(iSlot);
            g_bImmunityGaved[iSlot] = false;
        }
    });

    utils->StartupServer(g_PLID, StartupServer);

}

bool jb_mute_features::Unload(char* error, size_t maxlen) {
    jailbreak_api->ClearAllPluginHooks(g_PLID);
    utils->ClearAllHooks(g_PLID);
    ConVar_Unregister();

   
    return true;
}

const char* jb_mute_features::GetAuthor() { return "niffox"; }
const char* jb_mute_features::GetDate() { return __DATE__; }
const char* jb_mute_features::GetDescription() { return "[JB] Mute Features"; }
const char* jb_mute_features::GetLicense() { return "Private"; }
const char* jb_mute_features::GetLogTag() { return "[JB] Mute Features"; }
const char* jb_mute_features::GetName() { return "[JB] Mute Features"; }
const char* jb_mute_features::GetURL() { return "https://t.me/niffox_2q"; }
const char* jb_mute_features::GetVersion() { return "1.0.1"; }