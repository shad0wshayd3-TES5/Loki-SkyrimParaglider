#pragma once

class LokiParaglider :
	public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
{
public:
	static LokiParaglider* GetSingleton()
	{
		static LokiParaglider singleton;
		return &singleton;
	}

	RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
	{
		if (a_event && NotRevalisGale)
		{
			if (NotRevalisGale->formID == a_event->magicEffect)
			{
				fStart = 0.0f;
				fProgression = 0.0f;
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	static void InstallHooks()
	{
		InstallActivateTrue();
		InstallParagliderWatcher();
		InstallMGEFApplyEventSink();
	}

	inline static bool bIsActivate{ false };
	inline static bool bIsParagliding{ false };
	inline static float fFallSpeed{ 0.0f };
	inline static float fGaleSpeed{ 0.0f };
	inline static float fStart{ 0.0f };
	inline static float fProgression{ 0.0f };
	inline static RE::EffectSetting* NotRevalisGale{ nullptr };

private:
	static void InstallActivateTrue()
	{
		REL::Relocation<std::uintptr_t> targ{ RELOCATION_ID(41346, 42420), OFFSET(0x03C, 0x03C) };
		REL::Relocation<std::uintptr_t> addr{ RELOCATION_ID(41346, 42420), OFFSET(0x140, 0x142) };

		struct Patch : Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_var, std::uintptr_t)
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

		Patch patch(addr.address(), targ.address());
		patch.ready();

		auto& trampoline = SKSE::GetTrampoline();
		trampoline.write_branch<6>(targ.address(), CodeAllocation(patch, &trampoline));
	};

	static void InstallParagliderWatcher()
	{
		REL::Relocation<std::uintptr_t> ActorUpdate{ RELOCATION_ID(39375, 40447), OFFSET(0x8B4, 0xC1B) };  // 69E580, 6C61B0

		auto& trampoline = SKSE::GetTrampoline();
		_Paraglider = trampoline.write_call<5>(ActorUpdate.address(), Paraglider);
	};

	static void InstallMGEFApplyEventSink()
	{
		if (auto ScriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton())
		{
			ScriptEventSourceHolder->AddEventSink(LokiParaglider::GetSingleton());
		}
	}

	static void Paraglider(RE::Actor* a_this)
	{
		_Paraglider(a_this);

		if (!bIsActivate)
		{
			bIsParagliding = false;
			if (a_this->NotifyAnimationGraph("EndPara"sv))
			{
				RE::hkVector4 hkv;
				a_this->GetCharController()->GetPositionImpl(hkv, false);
				hkv.quad.m128_f32[2] /= 0.0142875f;
				a_this->GetCharController()->fallStartHeight = hkv.quad.m128_f32[2];
				a_this->GetCharController()->fallTime = 0.0f;
			}

			fStart = 0.0f;
			fProgression = 0.0f;
			return;
		}
		else
		{
			std::int32_t HasParaglider{ 0 };
			a_this->GetGraphVariableInt("hasparaglider", HasParaglider);

			if (HasParaglider)
			{
				if (a_this->NotifyAnimationGraph("StartPara"sv))
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

					bool bHasEffect = a_this->HasMagicEffect(NotRevalisGale);

					float fSpeed = bHasEffect ? fGaleSpeed : fFallSpeed;
					float fValue = lerp(fStart, fSpeed, fProgression);
					if (fProgression < 1.00f)
					{
						fProgression += bHasEffect ? 0.01f : 0.025f;
					}

					hkv.quad.m128_f32[2] = fValue;
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

	static inline REL::Relocation<decltype(Paraglider)> _Paraglider;

protected:
	LokiParaglider() = default;
	LokiParaglider(const LokiParaglider&) = delete;
	LokiParaglider(LokiParaglider&&) = delete;
	virtual ~LokiParaglider() = default;

	auto operator=(const LokiParaglider&) -> LokiParaglider& = delete;
	auto operator=(LokiParaglider&&) -> LokiParaglider& = delete;
};
