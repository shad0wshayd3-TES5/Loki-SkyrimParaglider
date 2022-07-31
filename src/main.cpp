#include "LokiParaglider.h"

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []()
{
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Paraglider");
	v.AuthorName("LokiWasHere");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor())
	{
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39)
	{
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif

namespace
{
	void InitializeLog()
	{
		auto path = logger::log_directory();
		if (!path)
		{
			stl::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

		logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type)
		{
			case SKSE::MessagingInterface::kDataLoaded:
				if (auto TESDataHandler = RE::TESDataHandler::GetSingleton())
				{
					// load settings
					{
						CSimpleIniA ini;
						ini.SetUnicode();
						auto filename = L"Data/SKSE/Plugins/Paraglider.ini";
						[[maybe_unused]] SI_Error rc = ini.LoadFile(filename);

						LokiParaglider::fFallSpeed = (float)ini.GetDoubleValue("SETTINGS", "fFallSpeed", 0.0f);
						LokiParaglider::fGaleSpeed = (float)ini.GetDoubleValue("SETTINGS", "fGaleSpeed", 0.0f);
					}
					
					// load form
					{
						LokiParaglider::NotRevalisGale = TESDataHandler->LookupForm<RE::EffectSetting>(0x10C68, "Paragliding.esp"sv);
						if (!LokiParaglider::NotRevalisGale)
						{
							LokiParaglider::NotRevalisGale = RE::TESForm::LookupByEditorID<RE::EffectSetting>("NotRevalisGaleMGEF"sv);
							if (!LokiParaglider::NotRevalisGale)
							{
								logger::error("Failed to find NotRevalisGaleMGEF."sv);
								RE::DebugMessageBox("Warning: Paragliding.esp is either not loaded, or has been modified.\nTarhiel's Gale will not work until you fix this.\n\nReinstall the mod and ensure it's enabled in your mod manager."sv);
							}
						}
					}
				}
				break;

			default:
				break;
		}
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	logger::info("Paraglider loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(128);

	const auto messaging = SKSE::GetMessagingInterface();
	if (messaging)
	{
		messaging->RegisterListener("SKSE", MessageHandler);
	}

	LokiParaglider::InstallHooks();

	return true;
}
