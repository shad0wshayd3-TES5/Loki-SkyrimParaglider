#include "LokiParaglider.h"

namespace
{
	void MessageCallback(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
		{
			LokiParaglider::InstallHooks();
			break;
		}
		case SKSE::MessagingInterface::kDataLoaded:
			// load settings
			//			{
			//				CSimpleIniA ini;
			//				ini.SetUnicode();
			//				auto filename = L"Data/SKSE/Plugins/Paraglider.ini";
			//				[[maybe_unused]] SI_Error rc = ini.LoadFile(filename);
			//
			//				LokiParaglider::fFallSpeed = (float)ini.GetDoubleValue("SETTINGS", "fFallSpeed", 0.0f);
			//				LokiParaglider::fGaleSpeed = (float)ini.GetDoubleValue("SETTINGS", "fGaleSpeed", 0.0f);
			//			}

			// load forms
			if (auto TESDataHandler = RE::TESDataHandler::GetSingleton())
			{
				LokiParaglider::NotRevalisGale = TESDataHandler->LookupForm<RE::EffectSetting>(0x10C68, "Paragliding.esp"sv);
				if (!LokiParaglider::NotRevalisGale)
				{
					LokiParaglider::NotRevalisGale = RE::TESForm::LookupByEditorID<RE::EffectSetting>("NotRevalisGaleMGEF"sv);
					if (!LokiParaglider::NotRevalisGale)
					{
						SKSE::log::error("Failed to find NotRevalisGaleMGEF.");
						RE::DebugMessageBox("Warning: Paragliding.esp is either not loaded, or has been modified.\nTarhiel's Gale will not work until you fix this.\n\nReinstall the mod and ensure it's enabled in your mod manager.");
					}
				}

				LokiParaglider::ParagliderForm = TESDataHandler->LookupForm<RE::TESObjectMISC>(0x00802, "Paragliding.esp"sv);
				if (!LokiParaglider::ParagliderForm)
				{
					LokiParaglider::ParagliderForm = RE::TESForm::LookupByEditorID<RE::TESObjectMISC>("Paraglider_gnd"sv);
					if (!LokiParaglider::ParagliderForm)
					{
						SKSE::log::warn("Failed to find ParagliderForm.");
					}
				}
			}
			break;

		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	SKSE::AllocTrampoline(128);
	SKSE::GetMessagingInterface()->RegisterListener(MessageCallback);

	return true;
}
