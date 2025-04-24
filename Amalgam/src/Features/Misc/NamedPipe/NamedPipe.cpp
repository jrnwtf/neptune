//
//
//
// TODO: REFACTOR THIS DOGSHIT
// TODO: REFACTOR THIS DOGSHIT
// TODO: REFACTOR THIS DOGSHIT
//
//
//
#include "NamedPipe.h"
#include "../../../SDK/SDK.h"
#include "../../Players/PlayerUtils.h"
#include "../../Configs/Configs.h"

#include <windows.h>
#include <thread>
#include <atomic>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unordered_map>
#include <filesystem>
#include <ShlObj.h>

namespace F::NamedPipe
{
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    std::atomic<bool> shouldRun(true);
    std::thread pipeThread;
    std::ofstream logFile("C:\\pipe_log.txt", std::ios::app);
    int botId = -1;
    

    std::unordered_map<uint32_t, bool> localBots;
    

    void ConnectAndMaintainPipe();

    void Log(const std::string& message)
    {
        if (!logFile.is_open())
        {
            std::cerr << "Failed to open log file" << std::endl;
            return;
        }
        logFile << message << std::endl;
        logFile.flush();
        OutputDebugStringA(("NamedPipe: " + message + "\n").c_str());
    }

    const char* PIPE_NAME = "\\\\.\\pipe\\AwootismBotPipe";

    std::string GetErrorMessage(DWORD error)
    {
        char* messageBuffer = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        return message;
    }

    std::string GetTF2Folder()
    {
        // Try common Steam install paths first
        std::vector<std::string> commonPaths = {
            "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2",
            "C:\\Program Files\\Steam\\steamapps\\common\\Team Fortress 2",
            "D:\\Steam\\steamapps\\common\\Team Fortress 2",
            "E:\\Steam\\steamapps\\common\\Team Fortress 2"
        };

        for (const auto& path : commonPaths) {
            if (std::filesystem::exists(path)) {
                Log("Found TF2 folder at common path: " + path);
                return path;
            }
        }

        // Try to get from the registry
        char steamPath[MAX_PATH] = {0};
        DWORD pathSize = sizeof(steamPath);
        HKEY hKey;
        
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            if (RegQueryValueExA(hKey, "SteamPath", NULL, NULL, (LPBYTE)steamPath, &pathSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                std::string possiblePath = std::string(steamPath) + "\\steamapps\\common\\Team Fortress 2";
                if (std::filesystem::exists(possiblePath)) {
                    Log("Found TF2 folder from registry: " + possiblePath);
                    return possiblePath;
                }
            }
            RegCloseKey(hKey);
        }
        
        // Try config path as fallback
        std::string configPath = F::Configs.m_sConfigPath;
        Log("Using config path as fallback: " + configPath);
        size_t pos = configPath.find_last_of("\\");
        if (pos != std::string::npos) {
            std::string baseFolder = configPath.substr(0, pos);
            Log("Derived base folder: " + baseFolder);
            return baseFolder;
        }
            
        // Last resort - get exe path
        char modulePath[MAX_PATH];
        GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        
        std::string path = modulePath;
        Log("Module path: " + path);
        pos = path.find_last_of("\\");
        if (pos != std::string::npos)
            path = path.substr(0, pos);
            
        Log("Final TF2 folder determination: " + path);
        return path;
    }

    int ReadBotIdFromFile()
    {
        // Get paths to check
        std::string tf2Folder = GetTF2Folder();
        std::string amalgamFolder = F::Configs.m_sConfigPath;
        
        Log("Starting bot ID file search...");
        Log("TF2 folder: " + tf2Folder);
        Log("Amalgam folder: " + amalgamFolder);
        
        std::regex botFileRegex("bot(\\d+)\\.txt");
        
        // List of folders to check, in order of preference
        std::vector<std::pair<std::string, std::string>> foldersToCheck = {
            {tf2Folder, "TF2"},
            {amalgamFolder, "Amalgam"},
            {std::filesystem::current_path().string(), "Current Working Directory"}
        };
        
        // Try to find in each folder
        for (const auto& [folder, folderName] : foldersToCheck) {
            Log("Searching in " + folderName + " folder: " + folder);
            
            if (!std::filesystem::exists(folder)) {
                Log(folderName + " folder doesn't exist: " + folder);
                continue;
            }
            
            try {
                for (const auto& entry : std::filesystem::directory_iterator(folder)) {
                    if (!entry.is_regular_file()) continue;
                    
                    std::string filename = entry.path().filename().string();
                    Log("Found file: " + filename);
                    
                    std::smatch match;
                    if (std::regex_match(filename, match, botFileRegex)) {
                        int botId = std::stoi(match[1]);
                        Log("Found bot ID " + std::to_string(botId) + " in " + folderName + " folder");
                        return botId;
                    }
                }
            } catch (const std::exception& e) {
                Log("Error searching " + folderName + " folder: " + std::string(e.what()));
                
                // Try old-style FindFirstFile as backup
                WIN32_FIND_DATA findFileData;
                HANDLE hFind = FindFirstFile((folder + "\\*.txt").c_str(), &findFileData);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        std::string filename(findFileData.cFileName);
                        Log("Found file (FindFirstFile): " + filename);
                        std::smatch match;
                        if (std::regex_match(filename, match, botFileRegex)) {
                            int botId = std::stoi(match[1]);
                            Log("Found bot ID " + std::to_string(botId) + " in " + folderName + " folder using FindFirstFile");
                            FindClose(hFind);
                            return botId;
                        }
                    } while (FindNextFile(hFind, &findFileData) != 0);
                    FindClose(hFind);
                } else {
                    DWORD error = GetLastError();
                    Log("FindFirstFile failed in " + folderName + " folder: " + GetErrorMessage(error));
                }
            }
        }
        
        // If no bot ID file was found, create one in the Amalgam folder
        Log("No bot ID file found, attempting to create one");
        
        // Try folders in reverse (write to TF2 folder last as it might require admin permissions)
        for (auto it = foldersToCheck.rbegin(); it != foldersToCheck.rend(); ++it) {
            const auto& [folder, folderName] = *it;
            
            if (!std::filesystem::exists(folder)) {
                Log("Can't create file in " + folderName + " folder - folder doesn't exist");
                continue;
            }
            
            try {
                std::string filepath = folder + "\\bot1.txt";
                Log("Attempting to create " + filepath);
                
                std::ofstream botFile(filepath);
                if (botFile.is_open()) {
                    botFile << "This file is used to identify the bot instance. ID: 1";
                    botFile.close();
                    
                    if (std::filesystem::exists(filepath)) {
                        Log("Successfully created bot ID file in " + folderName + " folder");
                        return 1;
                    } else {
                        Log("File creation reported success but file doesn't exist in " + folderName + " folder");
                    }
                } else {
                    Log("Failed to open file for writing in " + folderName + " folder");
                }
            } catch (const std::exception& e) {
                Log("Exception creating file in " + folderName + " folder: " + std::string(e.what()));
            }
        }
        
        Log("Failed to find or create any bot ID file");
        return -1;
    }

    void Initialize()
    {
        Log("NamedPipe::Initialize() called");
        botId = ReadBotIdFromFile();
        
        std::stringstream ss;
        if (botId == -1)
        {
            ss << "Failed to read bot ID from file";
            Log(ss.str());
        }
        else
        {
            ss << "Bot ID read from file: " << botId;
            Log(ss.str());
        }

        localBots.clear();
        Log("Cleared local bots list on startup");

        pipeThread = std::thread(ConnectAndMaintainPipe);
        Log("Pipe thread started");
    }

    void Shutdown()
    {
        shouldRun = false;
        if (pipeThread.joinable())
        {
            pipeThread.join();
        }
    }

    void SendStatusUpdate(const std::string& status)
    {
        if (hPipe == INVALID_HANDLE_VALUE || botId == -1)
        {
            return;
        }

        std::string message = std::to_string(botId) + ":" + status;
        DWORD bytesWritten;
        WriteFile(hPipe, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
    }

    void ExecuteCommand(const std::string& command)
    {
        Log("ExecuteCommand called with: " + command);
        
        if (command == "debug navbot")
        {
            Log("Debug navbot command received, sending 'kill' to TF2 console");
            if (I::EngineClient)
            {
                I::EngineClient->ClientCmd_Unrestricted("kill");
                Log("'kill' command sent to TF2 console");
                SendStatusUpdate("CommandExecuted:kill");
            }
            else
            {
                Log("Error: EngineClient is not available. 'kill' command not executed");
                SendStatusUpdate("CommandFailed:kill");
            }
            return;
        }
        
        if (command.substr(0, 11) == "loadconfig ")
        {
            std::string configName = command.substr(11);
            Log("Loading config: " + configName);
            
            if (F::Configs.LoadConfig(configName, true))
            {
                Log("Config loaded successfully: " + configName);
                SendStatusUpdate("ConfigLoaded:" + configName);
            }
            else
            {
                Log("Failed to load config: " + configName);
                SendStatusUpdate("ConfigLoadFailed:" + configName);
            }
        }
        else if (I::EngineClient)
        {
            Log("EngineClient is available, sending command to TF2 console");
            I::EngineClient->ClientCmd_Unrestricted(command.c_str());
            Log("Command sent to TF2 console: " + command);
            SendStatusUpdate("CommandExecuted:" + command);
        }
        else
        {
            Log("Error: EngineClient is not available. Command not executed: " + command);
            SendStatusUpdate("CommandFailed:" + command);
        }
    }

    void SendHealthUpdate(int health)
    {
        if (hPipe == INVALID_HANDLE_VALUE || botId == -1)
        {
            return;
        }

        std::string message = std::to_string(botId) + ":Health:" + std::to_string(health);
        DWORD bytesWritten;
        WriteFile(hPipe, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
    }

    int GetCurrentPlayerClass()
    {
        Log("GetCurrentPlayerClass called");
        if (I::EngineClient && I::EngineClient->IsInGame())
        {
            Log("In game");
            auto pLocal = H::Entities.GetLocal();
            if (pLocal)
            {
                Log("Local player found");
                int playerClass = pLocal->As<CTFPlayer>()->m_iClass();
                Log("Player class: " + std::to_string(playerClass));
                return playerClass;
            }
            else
            {
                Log("Local player not found");
            }
        }
        else
        {
            Log("Not in game or EngineClient not available");
        }
        return -1;
    }

    void SendPlayerClassUpdate(int playerClass)
    {
        if (hPipe == INVALID_HANDLE_VALUE || botId == -1)
        {
            return;
        }

        std::string classString;
        switch (playerClass)
        {
            case 1: classString = "Scout"; break;
            case 2: classString = "Sniper"; break;
            case 3: classString = "Soldier"; break;
            case 4: classString = "Demoman"; break;
            case 5: classString = "Medic"; break;
            case 6: classString = "Heavy"; break;
            case 7: classString = "Pyro"; break;
            case 8: classString = "Spy"; break;
            case 9: classString = "Engineer"; break;
            default: classString = "Unknown";
        }

        std::string message = std::to_string(botId) + ":PlayerClass:" + classString + "\n";
        DWORD bytesWritten;
        WriteFile(hPipe, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
        Log("Sent player class update: " + classString);
    }

    std::string GetCurrentLevelName()
    {
        return SDK::GetLevelName();
    }

    void SendMapUpdate()
    {
        if (hPipe == INVALID_HANDLE_VALUE || botId == -1)
        {
            return;
        }

        std::string mapName = GetCurrentLevelName();
        std::string message = std::to_string(botId) + ":Map:" + mapName + "\n";
        DWORD bytesWritten;
        WriteFile(hPipe, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
        Log("Sent map update: " + mapName);
    }

    void SendServerInfo()
    {
        if (hPipe == INVALID_HANDLE_VALUE || botId == -1)
        {
            return;
        }

        std::string serverInfo = "0.0.0.0";
        std::string message = std::to_string(botId) + ":ServerInfo:" + serverInfo + "\n";
        DWORD bytesWritten;
        WriteFile(hPipe, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
        Log("Sent server info update: " + serverInfo);
    }

    void UpdateBotInfo()
    {
        SendPlayerClassUpdate(GetCurrentPlayerClass());
        SendMapUpdate();
        SendServerInfo();
    }

    void BroadcastLocalBotId()
    {
        if (hPipe == INVALID_HANDLE_VALUE)
        {
            return;
        }

        auto pLocal = H::Entities.GetLocal();
        if (!pLocal)
        {
            return;
        }

        PlayerInfo_t pi{};
        int localIndex = I::EngineClient->GetLocalPlayer();
        if (I::EngineClient->GetPlayerInfo(localIndex, &pi))
        {
            std::string message = std::to_string(botId != -1 ? botId : 0) + ":LocalBot:" + std::to_string(pi.friendsID) + "\n";
            DWORD bytesWritten;
            WriteFile(hPipe, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
            Log("Broadcasted local bot ID: " + std::to_string(pi.friendsID));
        }
    }

    void ProcessLocalBotMessage(const std::string& message)
    {
        std::istringstream iss(message);
        std::string botNumber, messageType, friendsIDstr;
        std::getline(iss, botNumber, ':');
        std::getline(iss, messageType, ':');
        std::getline(iss, friendsIDstr);
        
        if (messageType == "LocalBot" && !friendsIDstr.empty())
        {
            try
            {
                uint32_t friendsID = std::stoull(friendsIDstr);
                
                // Don't skip messages from our own bot ID - we might have multiple instances
                // with the same ID running
                
                localBots[friendsID] = true;
                Log("Added local bot with friendsID: " + friendsIDstr);
                
                // Try to find player information and add ignored tag
                bool tagAdded = false;
                PlayerInfo_t pi{};
                for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
                {
                    if (I::EngineClient->GetPlayerInfo(i, &pi) && pi.friendsID == friendsID)
                    {
                        // Add both IGNORED_TAG and FRIEND_TAG
                        F::PlayerUtils.AddTag(friendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG), true, pi.name);
                        F::PlayerUtils.AddTag(friendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG), true, pi.name);
                        Log("Marked local bot as ignored and friend: " + std::string(pi.name));
                        tagAdded = true;
                        break;
                    }
                }
                
                if (!tagAdded)
                {
                    Log("Could not find player info for friendsID: " + friendsIDstr + " to add tags");
                }
            }
            catch (const std::exception& e)
            {
                Log("Error processing LocalBot message: " + std::string(e.what()));
            }
        }
    }

    bool IsLocalBot(uint32_t friendsID)
    {
        if (friendsID == 0)
            return false;
            
        return localBots.find(friendsID) != localBots.end();
    }

    void UpdateLocalBotIgnoreStatus()
    {
        BroadcastLocalBotId();
        
        for (const auto& [friendsID, isLocal] : localBots)
        {
            bool needsUpdate = false;
            
            if (!F::PlayerUtils.HasTag(friendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG)))
            {
                needsUpdate = true;
            }
            
            if (!F::PlayerUtils.HasTag(friendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG)))
            {
                needsUpdate = true;
            }
            
            if (needsUpdate)
            {
                PlayerInfo_t pi{};
                for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
                {
                    if (I::EngineClient->GetPlayerInfo(i, &pi) && pi.friendsID == friendsID)
                    {
                        F::PlayerUtils.AddTag(friendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG), true, pi.name);
                        F::PlayerUtils.AddTag(friendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG), true, pi.name);
                        Log("Marked local bot as ignored and friend: " + std::string(pi.name));
                        break;
                    }
                }
            }
        }
    }

    void ClearLocalBots()
    {
        localBots.clear();
        Log("Cleared local bots list");
    }

    void ConnectAndMaintainPipe()
    {
        Log("ConnectAndMaintainPipe started");
        while (shouldRun)
        {
            if (hPipe == INVALID_HANDLE_VALUE)
            {
                Log("Attempting to connect to pipe");
                hPipe = CreateFile(
                    PIPE_NAME,
                    GENERIC_READ | GENERIC_WRITE,
                     0,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL);

                if (hPipe != INVALID_HANDLE_VALUE)
                {
                    Log("Connected to pipe");
                    SendStatusUpdate("Connected");

                    if (botId != -1)
                    {
                        Log("Using Bot ID: " + std::to_string(botId));
                    }
                    else
                    {
                        Log("Warning: Bot ID not set");
                    }
                    
                    ClearLocalBots();
                }
                else
                {
                    DWORD error = GetLastError();
                    Log("Failed to connect to pipe: " + std::to_string(error) + " - " + GetErrorMessage(error));
                }
            }

            if (hPipe != INVALID_HANDLE_VALUE)
            {
                SendStatusUpdate("Heartbeat");
                SendPlayerClassUpdate(GetCurrentPlayerClass());
                SendMapUpdate();
                SendServerInfo();
                
                if (I::EngineClient && I::EngineClient->IsInGame())
                {
                    UpdateLocalBotIgnoreStatus();
                }
                
                char buffer[1024];
                DWORD bytesRead;
                DWORD bytesAvail = 0;

                if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0)
                {
                    if (ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL))
                    {
                        std::string message(buffer, bytesRead);
                        Log("Received message: " + message);
                        std::istringstream iss(message);
                        std::string botNumber, messageType, content;
                        std::getline(iss, botNumber, ':');
                        std::getline(iss, messageType, ':');
                        std::getline(iss, content);

                        if (messageType == "Command")
                        {
                            Log("Executing command: " + content);
                            ExecuteCommand(content);
                        }
                        else if (messageType == "LocalBot")
                        {
                            ProcessLocalBotMessage(message);
                        }
                        else
                        {
                            Log("Received unknown message type: " + messageType);
                        }
                    }
                    else
                    {
                        Log("ReadFile failed: " + std::to_string(GetLastError()));
                        CloseHandle(hPipe);
                        hPipe = INVALID_HANDLE_VALUE;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (hPipe != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hPipe);
            hPipe = INVALID_HANDLE_VALUE;
        }
        Log("ConnectAndMaintainPipe ended");
    }
} 