// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDMassExtensions.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebuggerExtensions.h"
#endif

#define LOCTEXT_NAMESPACE "FGDMassExtensionsModule"

void FGDMassExtensionsModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory( "MassExtensions",
											 IGameplayDebugger::FOnGetCategory::CreateStatic( &FGameplayDebuggerCategory_MassExtensions::MakeInstance ),
											 EGameplayDebuggerCategoryState::EnabledInGameAndSimulate,
											 9 );
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FGDMassExtensionsModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if ( IGameplayDebugger::IsAvailable() )
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory( "MassExtensions" );
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE( FGDMassExtensionsModule, GDMassExtensions )