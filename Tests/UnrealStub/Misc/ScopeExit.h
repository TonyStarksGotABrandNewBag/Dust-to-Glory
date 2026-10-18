#pragma once
#include "CoreMinimal.h"

namespace dtg_stub
{
	template <typename Fn>
	struct ScopeGuard
	{
		Fn Action;
		explicit ScopeGuard(Fn InAction) : Action(InAction) {}
		~ScopeGuard() { Action(); }
	};
	struct ScopeGuardMaker
	{
		template <typename Fn>
		ScopeGuard<Fn> operator+(Fn InAction) const { return ScopeGuard<Fn>(InAction); }
	};
}

#define DTG_STUB_CAT_INNER(a, b) a##b
#define DTG_STUB_CAT(a, b) DTG_STUB_CAT_INNER(a, b)
#define ON_SCOPE_EXIT const auto DTG_STUB_CAT(ScopeGuard_, __LINE__) = dtg_stub::ScopeGuardMaker() + [&]()
