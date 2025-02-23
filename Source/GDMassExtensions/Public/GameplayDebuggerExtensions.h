// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Required first for WITH_MASSGAMEPLAY_DEBUG
#include "MassCommonTypes.h"

#if WITH_GAMEPLAY_DEBUGGER && WITH_MASSGAMEPLAY_DEBUG

#include "GameplayDebuggerCategory.h"
#include "HAL/IConsoleManager.h"

struct FMassEntityManager;
class UMassDebuggerSubsystem;
class APlayerController;
class AActor;

class FGameplayDebuggerCategory_MassExtensions : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

  public:
	FGameplayDebuggerCategory_MassExtensions();
	virtual ~FGameplayDebuggerCategory_MassExtensions();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

  protected:
	virtual void CollectData( APlayerController* OwnerPlayerController, AActor* DebugActor ) override;
	virtual void DrawData( APlayerController* OwnerPlayerController, FGameplayDebuggerCanvasContext& CanvasContext ) override;

  protected:
	struct FEntityDescription
	{
		FEntityDescription() = default;
		FEntityDescription( const float InScore, const FVector& InLocation, const FString& InDescription ) : Score( InScore ), Location( InLocation ), Description( InDescription )
		{
		}

		float Score = 0.0f;
		FVector Location = FVector::ZeroVector;
		FString Description;
	};

	TArray<FEntityDescription> NearEntityDescriptions;
};

#endif // WITH_GAMEPLAY_DEBUGGER && WITH_MASSGAMEPLAY_DEBUG
