#include "Commands.h"

#include "../../Core/Core.h"
#include "../ImGui/Menu/Menu.h"
#include "../NavBot/NavEngine/NavEngine.h"
#include "../Configs/Configs.h"
#include "../Players/PlayerUtils.h"
#include <utility>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>

bool CCommands::Run(const std::string& cmd, std::deque<std::string>& args)
{
	auto uHash = FNV1A::Hash32(cmd.c_str());
	if (!CommandMap.contains(uHash))
		return false;

	CommandMap[uHash](args);
	return true;
}

void CCommands::Register(const std::string& name, CommandCallback callback)
{
	CommandMap[FNV1A::Hash32(name.c_str())] = std::move(callback);
}

void CCommands::Initialize()
{
	Register("cat_queue", [](const std::deque<std::string>& args)
		{
			if (!I::TFPartyClient)
				return;
			if (I::TFPartyClient->BInQueueForMatchGroup(k_eTFMatchGroup_Casual_Default))
				return;
			if (I::EngineClient->IsDrawingLoadingImage())
				return;

			static bool bHasLoaded = false;
			if (!bHasLoaded)
			{
				I::TFPartyClient->LoadSavedCasualCriteria();
				bHasLoaded = true;
			}
			I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_Casual_Default);
		});

	Register("cat_load", [](const std::deque<std::string>& args)
		{
			if (args.empty())
			{
				SDK::Output("Usage:\n\tcat_load <config_name>");
				return;
			}

			F::Configs.LoadConfig(args[0], true);
		});

	Register("cat_setcvar", [](const std::deque<std::string>& args)
		{
			if (args.size() < 2)
			{
				SDK::Output("Usage:\n\tcat_setcvar <cvar> <value>");
				return;
			}

			const auto foundCVar = I::CVar->FindVar(args[0].c_str());
			const std::string cvarName = args[0];
			if (!foundCVar)
			{
				SDK::Output(std::format("Could not find {}", cvarName).c_str());
				return;
			}

			auto vArgs = args; vArgs.pop_front();
			std::string newValue = boost::algorithm::join(vArgs, " ");
			boost::replace_all(newValue, "\"", "");
			foundCVar->SetValue(newValue.c_str());
			SDK::Output(std::format("Set {} to {}", cvarName, newValue).c_str());
		});

	Register("cat_getcvar", [](const std::deque<std::string>& args)
		{
			if (args.size() != 1)
			{
				SDK::Output("Usage:\n\tcat_getcvar <cvar>");
				return;
			}

			const auto foundCVar = I::CVar->FindVar(args[0].c_str());
			const std::string cvarName = args[0];
			if (!foundCVar)
			{
				SDK::Output(std::format("Could not find {}", cvarName).c_str());
				return;
			}

			SDK::Output(std::format("Value of {} is {}", cvarName, foundCVar->GetString()).c_str());
		});

	Register("cat_menu", [](const std::deque<std::string>& args)
		{
			I::MatSystemSurface->SetCursorAlwaysVisible(F::Menu.m_bIsOpen = !F::Menu.m_bIsOpen);
		});

	Register("cat_path_to", [](std::deque<std::string> args)
		{
			// Check if the user provided at least 3 args
			if (args.size() < 3)
			{
				I::CVar->ConsoleColorPrintf({ 255, 255, 255, 255 }, "Usage: cat_path_to <x> <y> <z>\n");
				return;
			}

			// Get the Vec3
			const auto Vec = Vec3( atoi( args[ 0 ].c_str( ) ), atoi( args[ 1 ].c_str( ) ), atoi( args[ 2 ].c_str( ) ) );

			F::NavEngine.navTo( Vec );
		});

	Register("cat_nav_search_spawnrooms", [](std::deque<std::string> args)
		{
			if ( F::NavEngine.map && F::NavEngine.map->state == CNavParser::NavState::Active )
				F::NavEngine.map->UpdateRespawnRooms( );
		});

	Register("cat_save_nav_mesh", [](std::deque<std::string> args)
		{
			if ( auto pNavFile = F::NavEngine.getNavFile( ) )
				pNavFile->Write( );
		});

	Register("cat_detach", [](const std::deque<std::string>& args)
		{
			if (F::Menu.m_bIsOpen)
				I::MatSystemSurface->SetCursorAlwaysVisible(F::Menu.m_bIsOpen = false);
			U::Core.m_bUnload = true;
		});

	Register("cat_ignore", [](const std::deque<std::string>& args)
		{
			if (args.size() < 2)
			{
				SDK::Output("Usage:\n\tcat_ignore <id32> <tag>");
				return;
			}

			uint32_t uFriendsID = 0;
			try
			{
				uFriendsID = std::stoul(args[0]);
			}
			catch (...)
			{
				SDK::Output("Invalid ID32 format");
				return;
			}

			if (!uFriendsID)
			{
				SDK::Output("Invalid ID32");
				return;
			}

			const std::string& sTag = args[1];
			int iTagID = F::PlayerUtils.GetTag(sTag);
			if (iTagID == -1)
			{
				SDK::Output(std::format("Invalid tag: {}", sTag).c_str());
				return;
			}

			auto pTag = F::PlayerUtils.GetTag(iTagID);
			if (!pTag || !pTag->Assignable)
			{
				SDK::Output(std::format("Tag {} is not assignable", sTag).c_str());
				return;
			}

			if (F::PlayerUtils.HasTag(uFriendsID, iTagID))
			{
				F::PlayerUtils.RemoveTag(uFriendsID, iTagID, true);
				SDK::Output(std::format("Removed tag {} from ID32 {}", sTag, uFriendsID).c_str());
			}
			else
			{
				F::PlayerUtils.AddTag(uFriendsID, iTagID, true);
				SDK::Output(std::format("Added tag {} to ID32 {}", sTag, uFriendsID).c_str());
			}
		});
}