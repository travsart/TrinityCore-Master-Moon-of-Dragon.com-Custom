#include "ScriptMgr.h"
#include "World.h"
#include "Config.h"
#include <ctime>
#include <string>

class ScheduledShutdown : public WorldScript
{
public:
    ScheduledShutdown() : WorldScript("ScheduledShutdown") {}

    void OnStartup() override
    {
        shutdownEnabled = sConfigMgr->GetBoolDefault("ScheduledShutdown.Enable", true);
        shutdownHour = sConfigMgr->GetIntDefault("ScheduledShutdown.Hour", 4);
        shutdownMinute = sConfigMgr->GetIntDefault("ScheduledShutdown.Minute", 0);
        announcementSeconds = sConfigMgr->GetIntDefault("ScheduledShutdown.AnnouncementSeconds", 30);

        if (announcementSeconds < 1)
            announcementSeconds = 1;

        alreadyExecuted = false;

        if (shutdownEnabled)
        {
            TC_LOG_INFO("server.scripts", "ScheduledShutdown: Enabled (Daily at {:02d}:{:02d})", shutdownHour, shutdownMinute);
        }
        else
        {
            TC_LOG_INFO("server.scripts", "ScheduledShutdown: Disabled via config.");
        }
    }

    void OnUpdate(uint32 /*diff*/) override
    {
        if (!shutdownEnabled)
            return;

        time_t now = time(nullptr);
        tm localTime;
#ifdef _WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif

        if (localTime.tm_hour == shutdownHour && localTime.tm_min == shutdownMinute)
        {
            if (!alreadyExecuted)
            {
                std::string msg = "Automatic server restart in " + std::to_string(announcementSeconds) + " seconds!";
                sWorld->SendWorldText(LANG_SYSTEMMESSAGE, msg.c_str());

                sWorld->ShutdownServ(
                    announcementSeconds,
                    0,                   // Shutdown options (normal)
                    RESTART_EXIT_CODE,   // Restart
                    "Planned daily restart"
                );

                alreadyExecuted = true;
            }
        }
        else
        {
            alreadyExecuted = false; // Reset für den nächsten Tag
        }
    }

private:
    bool shutdownEnabled = true;
    int32 shutdownHour = 4;
    int32 shutdownMinute = 0;
    int32 announcementSeconds = 30;
    bool alreadyExecuted = false;
};

void Add_MoDCore_ScheduledShutdownScripts()
{
    new ScheduledShutdown();
}
