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

static RE::EffectSetting* NotRevalisGale{ nullptr };

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
					NotRevalisGale = TESDataHandler->LookupForm<RE::EffectSetting>(0x10C68, "Paragliding.esp");
				}
				break;

			default:
				break;
		}
	}
}

static bool bIsActivate{ false };
static bool bIsParagliding{ false };
static float fStart{ 0.0f };
static float fProgression{ 0.0f };

class MagicEffectApplyEventHandler :
	public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
{
public:
	static MagicEffectApplyEventHandler* GetSingleton()
	{
		static MagicEffectApplyEventHandler singleton;
		return &singleton;
	}

	auto ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*)
		-> RE::BSEventNotifyControl override
	{
		if (a_event)
		{
			if (NotRevalisGale)
			{
				if (a_event->magicEffect == NotRevalisGale->formID)
				{
					fStart = 0.00f;
					fProgression = 0.00f;
				}
			}
			else
			{
				logger::error("RevalisGale Form not found, is Paraglider.esp loaded?");
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

protected:
	MagicEffectApplyEventHandler() = default;
	MagicEffectApplyEventHandler(const MagicEffectApplyEventHandler&) = delete;
	MagicEffectApplyEventHandler(MagicEffectApplyEventHandler&&) = delete;
	virtual ~MagicEffectApplyEventHandler() = default;

	auto operator=(const MagicEffectApplyEventHandler&) -> MagicEffectApplyEventHandler& = delete;
	auto operator=(MagicEffectApplyEventHandler&&) -> MagicEffectApplyEventHandler& = delete;
};

class Loki_Paraglider
{
public:
	float FallSpeed, GaleSpeed;

	Loki_Paraglider()
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		auto filename = L"Data/SKSE/Plugins/Paraglider.ini";
		[[maybe_unused]] SI_Error rc = ini.LoadFile(filename);

		this->FallSpeed = (float)ini.GetDoubleValue("SETTINGS", "fFallSpeed", 0.00f);
		this->GaleSpeed = (float)ini.GetDoubleValue("SETTINGS", "fGaleSpeed", 0.00f);
	}

	static void* CodeAllocation(Xbyak::CodeGenerator& a_code, SKSE::Trampoline* t_ptr)
	{
		auto result = t_ptr->allocate(a_code.getSize());
		std::memcpy(result, a_code.getCode(), a_code.getSize());
		return result;
	}

	static float lerp(float a, float b, float f)
	{
		return a + f * (b - a);
	}

	static void InstallActivateTrue()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(41346, 42420), OFFSET(0x3C, 0x3C) };
		REL::Relocation<std::uintptr_t> addr{ RELOCATION_ID(41346, 42420), OFFSET(0x140, 0x142) };

		struct Patch :
			Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_var, [[maybe_unused]] std::uintptr_t a_target)
			{
				Xbyak::Label ourJmp;
				Xbyak::Label ActivateIsTrue;

				mov(byte[rcx + 0x18], 0x1);
				push(rax);
				mov(rax, (uintptr_t)&bIsActivate);
				cmp(byte[rax], 0x1);
				je(ActivateIsTrue);
				mov(byte[rax], 0x1);
				pop(rax);
				jmp(ptr[rip + ourJmp]);

				L(ActivateIsTrue);
				mov(byte[rax], 0x0);
				pop(rax);
				jmp(ptr[rip + ourJmp]);

				L(ourJmp);
				dq(a_var);
			};
		};

		Patch patch(addr.address(), target.address());
		patch.ready();

		auto& trampoline = SKSE::GetTrampoline();
		trampoline.write_branch<6>(target.address(), Loki_Paraglider::CodeAllocation(patch, &trampoline));
	};

	static void InstallParagliderWatcher()
	{
		REL::Relocation<std::uintptr_t> ActorUpdate{ RELOCATION_ID(39375, 40447), OFFSET(0x8B4, 0xC1B) };  // 69E580, 6C61B0

		auto& trampoline = SKSE::GetTrampoline();
		_Paraglider = trampoline.write_call<5>(ActorUpdate.address(), Paraglider);
	};

	static void AddMGEFApplyEventSink()
	{
		if (auto ScriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton())
		{
			ScriptEventSourceHolder->AddEventSink(MagicEffectApplyEventHandler::GetSingleton());
		}
	}

private:
	static void Paraglider(RE::Actor* a_this)
	{
		_Paraglider(a_this);

		static Loki_Paraglider* lp = NULL;
		if (!lp)
		{
			lp = new Loki_Paraglider();
		}

		if (!bIsActivate)
		{
			bIsParagliding = false;
			const RE::BSFixedString endPara = "EndPara";
			if (a_this->NotifyAnimationGraph(endPara))
			{
				RE::hkVector4 hkv;
				a_this->GetCharController()->GetPositionImpl(hkv, false);
				hkv.quad.m128_f32[2] /= 0.0142875f;
				a_this->GetCharController()->fallStartHeight = hkv.quad.m128_f32[2];
				a_this->GetCharController()->fallTime = 0.00f;
			}

			fStart = 0.00f;
			fProgression = 0.00f;
			return;
		}
		else
		{
			const RE::BSFixedString startPara = "StartPara";
			int hasIt;
			a_this->GetGraphVariableInt("hasparaglider", hasIt);
			
			if (hasIt)
			{
				if (a_this->NotifyAnimationGraph(startPara))
				{
					bIsParagliding = true;
				}

				if (bIsParagliding)
				{
					RE::hkVector4 hkv;
					a_this->GetCharController()->GetLinearVelocityImpl(hkv);
					if (fStart == 0.0f)
					{
						fStart = hkv.quad.m128_f32[2];
					}

					float dest = lp->FallSpeed;
					if (a_this->HasMagicEffect(NotRevalisGale))
					{
						dest = lp->GaleSpeed;
					}

					auto a_result = Loki_Paraglider::lerp(fStart, dest, fProgression);
					if (fProgression < 1.00f)
					{
						(a_this->HasMagicEffect(NotRevalisGale)) ? fProgression += 0.01f : fProgression += 0.025f;
					}

					hkv.quad.m128_f32[2] = a_result;
					a_this->GetCharController()->SetLinearVelocityImpl(hkv);
				}

				if (a_this->GetCharController()->context.currentState == RE::hkpCharacterStateType::kOnGround)
				{
					bIsActivate = false;
					bIsParagliding = false;
				}
			}
		}

		return;
	};

	static inline REL::Relocation<decltype(Paraglider)> _Paraglider;
};

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

	Loki_Paraglider::InstallActivateTrue();
	Loki_Paraglider::InstallParagliderWatcher();
	Loki_Paraglider::AddMGEFApplyEventSink();

	return true;
}
