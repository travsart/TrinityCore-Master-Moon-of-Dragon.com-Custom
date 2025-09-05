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
        shutdownHour = sConfigMgr->GetIntDefault("ScheduledShutdown.Hour", 4);
        shutdownMinute = sConfigMgr->GetIntDefault("ScheduledShutdown.Minute", 0);
        announcementSeconds = sConfigMgr->GetIntDefault("ScheduledShutdown.AnnouncementSeconds", 30);

        if (announcementSeconds < 1)
            announcementSeconds = 1;

        alreadyExecuted = false;
    }

    void OnUpdate(uint32 /*diff*/) override
    {
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
                std::string msg = "Automatically Server-Restart in " + std::to_string(announcementSeconds) + " Seconds!";
                sWorld->SendWorldText(LANG_SYSTEMMESSAGE, msg.c_str());

                // -> Passend zu deiner Signatur
                sWorld->ShutdownServ(
                    announcementSeconds, // Delay
                    0,                   // Options (0 = normal)
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
    int32 shutdownHour = 6;
    int32 shutdownMinute = 0;
    int32 announcementSeconds = 30;
    bool alreadyExecuted = false;
};

void Add_MoDCore_ScheduledShutdownScripts()
{
    new ScheduledShutdown();
}
