#pragma once

#include "../../Utils/Feature/Feature.h"
#include <string>
#include <unordered_map>
#include <stdint.h>

class CNamedPipe
{
public:
    CNamedPipe() = default;
    ~CNamedPipe() = default;
};

ADD_FEATURE(CNamedPipe, NamedPipe)

namespace F::NPipe
{
    void Initialize();
    void Shutdown();
    
    void SendStatusUpdate(const std::string& status);
    void ExecuteCommand(const std::string& command);
    void SendHealthUpdate(int health);
    int GetCurrentPlayerClass();
    void SendPlayerClassUpdate(int playerClass);
    std::string GetCurrentLevelName();
    void SendMapUpdate();
    void SendServerInfo();
    void UpdateBotInfo();
    void BroadcastLocalBotId();
    void ProcessLocalBotMessage(const std::string& message);
    bool IsLocalBot(uint32_t friendsID);
    void UpdateLocalBotIgnoreStatus();
    void ClearLocalBots();
    bool SetStatus(bool bEnabled);
    bool SendStatusUpdate();
    bool ExecutePipedCommand(const std::string& command, std::string* outResponse);
    
    extern std::unordered_map<uint32_t, bool> localBots;
    extern bool bQueuedStatus;
    extern bool bStatusUpdateQueued;
} 