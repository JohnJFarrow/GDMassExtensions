// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerExtensions.h"

#if WITH_GAMEPLAY_DEBUGGER && WITH_MASSGAMEPLAY_DEBUG
#include "CanvasItem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "GameplayDebuggerConfig.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassDebuggerSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassGameplayDebugTypes.h"
#include "MassLODSubsystem.h"
#include "MassLookAtFragments.h"
#include "MassNavigationFragments.h"
#include "MassRepresentationFragments.h"
#include "MassSignalSubsystem.h"
#include "MassSimulationLOD.h"
#include "MassSmartObjectFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeFragments.h"
#include "MassZoneGraphNavigationFragments.h"
#include "SmartObjectSubsystem.h"
#include "Steering/MassSteeringFragments.h"
#include "Util/ColorConstants.h"

/*
namespace UE::Mass::Debug
{
FMassEntityHandle GetEntityFromActor( const AActor& Actor, const UMassAgentComponent *& OutMassAgentComponent )
{
	FMassEntityHandle EntityHandle;
	if ( const UMassAgentComponent *AgentComp = Actor.FindComponentByClass<UMassAgentComponent>() )
	{
		EntityHandle = AgentComp->GetEntityHandle();
		OutMassAgentComponent = AgentComp;
	}
	else if ( UMassActorSubsystem *ActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>( Actor.GetWorld() ) )
	{
		EntityHandle = ActorSubsystem->GetEntityHandleFromActor( &Actor );
	}
	return EntityHandle;
};

FMassEntityHandle GetBestEntity( const FVector ViewLocation, const FVector ViewDirection, const TConstArrayView<FMassEntityHandle> Entities,
								 const TConstArrayView<FVector> Locations, const bool bLimitAngle, const FVector::FReal MaxScanDistance )
{
	constexpr FVector::FReal MinViewDirDot = 0.707; // 45 degrees
	const FVector::FReal MaxScanDistanceSq = MaxScanDistance * MaxScanDistance;

	checkf( Entities.Num() == Locations.Num(), TEXT( "Both Entities and Locations lists are expected to be of the same size: %d vs %d" ), Entities.Num(), Locations.Num() );

	FVector::FReal BestScore = bLimitAngle ? MinViewDirDot : ( -1. - KINDA_SMALL_NUMBER );
	FMassEntityHandle BestEntity;

	for ( int i = 0; i < Entities.Num(); ++i )
	{
		if ( Entities[i].IsSet() == false )
		{
			continue;
		}

		const FVector DirToEntity = ( Locations[i] - ViewLocation );
		const FVector::FReal DistToEntitySq = DirToEntity.SizeSquared();
		if ( DistToEntitySq > MaxScanDistanceSq )
		{
			continue;
		}

		const FVector::FReal Distance = FMath::Sqrt( DistToEntitySq );
		const FVector DirToEntityNormal = ( FMath::IsNearlyZero( DistToEntitySq ) ) ? ViewDirection : ( DirToEntity / Distance );
		const FVector::FReal ViewDot = FVector::DotProduct( ViewDirection, DirToEntityNormal );
		const FVector::FReal Score = ViewDot * 0.1 * ( 1. - Distance / MaxScanDistance );
		if ( ViewDot > BestScore )
		{
			BestScore = ViewDot;
			BestEntity = Entities[i];
		}
	}

	return BestEntity;
}
} // namespace UE::Mass::Debug
*/

FGameplayDebuggerCategory_MassExtensions::FGameplayDebuggerCategory_MassExtensions()
{
}

FGameplayDebuggerCategory_MassExtensions::~FGameplayDebuggerCategory_MassExtensions()
{
}

void FGameplayDebuggerCategory_MassExtensions::CollectData( APlayerController* OwnerPlayerController, AActor* DebugActor )
{
	UWorld* World = GetDataWorld( OwnerPlayerController, DebugActor );
	check( World );

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>( World );
	if ( EntitySubsystem == nullptr )
	{
		AddTextLine( FString::Printf( TEXT( "{Red}EntitySubsystem instance is missing" ) ) );
		return;
	}
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;

	NearEntityDescriptions.Reset();

	if ( OwnerPlayerController )
	{
		FMassEntityQuery EntityQuery;
		// EntityQuery.AddRequirement<FMassStateTreeInstanceFragment>( EMassFragmentAccess::ReadOnly );
		// EntityQuery.AddConstSharedRequirement<FMassStateTreeSharedFragment>();
		EntityQuery.AddRequirement<FTransformFragment>( EMassFragmentAccess::ReadOnly );
		// EntityQuery.AddRequirement<FMassStandingSteeringFragment>( EMassFragmentAccess::ReadOnly );
		// EntityQuery.AddRequirement<FMassGhostLocationFragment>( EMassFragmentAccess::ReadOnly );

		EntityQuery.AddRequirement<FMassLookAtFragment>( EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional );
		EntityQuery.AddRequirement<FAgentRadiusFragment>( EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional );
		EntityQuery.AddRequirement<FMassVelocityFragment>( EMassFragmentAccess::ReadOnly );
		EntityQuery.AddRequirement<FMassSteeringFragment>( EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional );
		EntityQuery.AddRequirement<FMassMoveTargetFragment>( EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional );
		EntityQuery.AddRequirement<FMassForceFragment>( EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional );
		// EntityQuery.AddRequirement<FMassZoneGraphShortPathFragment>( EMassFragmentAccess::ReadOnly );
		// EntityQuery.AddRequirement<FMassSmartObjectUserFragment>( EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional );

		// multiple potential lods
		EntityQuery.AddRequirement<FMassSimulationLODFragment>( EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional );
		EntityQuery.AddRequirement<FMassRepresentationLODFragment>( EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional );

		const double CurrentTime = World->GetTimeSeconds();

		UMassStateTreeSubsystem* MassStateTreeSubsystem = World->GetSubsystem<UMassStateTreeSubsystem>();
		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		USmartObjectSubsystem* SmartObjectSubsystem = World->GetSubsystem<USmartObjectSubsystem>();

		if ( MassStateTreeSubsystem && SignalSubsystem && SmartObjectSubsystem )
		{
			FMassExecutionContext Context( EntityManager, 0.0f );

			EntityQuery.ForEachEntityChunk( EntityManager,
											Context,
											[this, MassStateTreeSubsystem, SignalSubsystem, SmartObjectSubsystem, OwnerPlayerController, ViewLocation, ViewDirection, CurrentTime](
												FMassExecutionContext& Context )
			{
				FMassEntityManager& EntityManager = Context.GetEntityManagerChecked();

				const int32 NumEntities = Context.GetNumEntities();

				// FTransformFragment is only mandatory one, to test if entity is near player for drawing or ignoring
				const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

				const TConstArrayView<FMassLookAtFragment> LookAtList = Context.GetFragmentView<FMassLookAtFragment>();
				const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
				const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();
				const TConstArrayView<FMassSteeringFragment> SteeringList = Context.GetFragmentView<FMassSteeringFragment>();
				const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
				const TConstArrayView<FMassForceFragment> ForceList = Context.GetFragmentView<FMassForceFragment>();

				const TConstArrayView<FMassSimulationLODFragment> SimLODList = Context.GetFragmentView<FMassSimulationLODFragment>();
				const TConstArrayView<FMassRepresentationLODFragment> RepLODList = Context.GetFragmentView<FMassRepresentationLODFragment>();

				/*
				const TConstArrayView<FMassStateTreeInstanceFragment> StateTreeInstanceList = Context.GetFragmentView<FMassStateTreeInstanceFragment>();
				const TConstArrayView<FMassStandingSteeringFragment> StandingSteeringList = Context.GetFragmentView<FMassStandingSteeringFragment>();
				const TConstArrayView<FMassGhostLocationFragment> GhostList = Context.GetFragmentView<FMassGhostLocationFragment>();
				const TConstArrayView<FMassZoneGraphShortPathFragment> ShortPathList = Context.GetFragmentView<FMassZoneGraphShortPathFragment>();
				const TConstArrayView<FMassSmartObjectUserFragment> SOUserList = Context.GetFragmentView<FMassSmartObjectUserFragment>();
				const FMassStateTreeSharedFragment& SharedStateTree = Context.GetConstSharedFragment<FMassStateTreeSharedFragment>();

				const bool bHasSOUser = ( SOUserList.Num() > 0 );
				const UStateTree *StateTree = SharedStateTree.StateTree;
				*/

				const bool bHasLookAt = ( LookAtList.Num() > 0 );

				const UGameplayDebuggerUserSettings* Settings = GetDefault<UGameplayDebuggerUserSettings>();
				const FVector::FReal MaxViewDistance = Settings->MaxViewDistance;

				// Settings->MaxViewAngle is 45 degrees if they get too close its too narrow a field of view
				const FVector::FReal MinViewDirDot = FMath::Cos( FMath::DegreesToRadians( Settings->MaxViewAngle * 2.0f ) );
				const FVector::FReal MaxViewDistanceSquared = FMath::Square( MaxViewDistance );

				for ( int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex )
				{
					const FTransformFragment& Transform = TransformList[EntityIndex];

					const FVector EntityLocation = Transform.GetTransform().GetLocation();
					// only want entities ones close to player
					const FVector DirToEntity = EntityLocation - ViewLocation;
					const FVector::FReal DistanceToEntitySq = DirToEntity.SquaredLength();
					if ( DistanceToEntitySq > MaxViewDistanceSquared )
					{
						continue;
					}
					const FVector::FReal ViewDot = FVector::DotProduct( DirToEntity.GetSafeNormal(), ViewDirection );
					if ( ViewDot < MinViewDirDot )
					{
						if ( EntityIndex == 0 )
						{
							UE_LOG( LogTemp,
									Log,
									TEXT( "DirToEntity %s ViewDirection %s ViewDot %f MinViewDirDot %f" ),
									*DirToEntity.ToString(),
									*ViewDirection.ToString(),
									ViewDot,
									MinViewDirDot );
						}
						continue;
					}

					const float Radius = RadiusList.Num() > 0 ? RadiusList[EntityIndex].Radius : 40.0f;
					const FVector Velocity = VelocityList.Num() > 0 ? VelocityList[EntityIndex].Value : FVector::ZeroVector;
					const FVector SteeringDesiredVelocity = SteeringList.Num() > 0 ? SteeringList[EntityIndex].DesiredVelocity : FVector::ZeroVector;
					const FMassMoveTargetFragment& MoveTarget = MoveTargetList.Num() > 0 ? MoveTargetList[EntityIndex] : FMassMoveTargetFragment();
					const FVector Force = ForceList.Num() > 0 ? ForceList[EntityIndex].Value : FVector::ZeroVector;

					// multiple possible LOD sources
					// Max = undefined range is (high,med,low,off)
					EMassLOD::Type LOD = EMassLOD::Max;

					if ( SimLODList.Num() > 0 )
					{
						LOD = SimLODList[EntityIndex].LOD;
					}
					else if ( RepLODList.Num() > 0 )
					{
						LOD = RepLODList[EntityIndex].LOD;
					}

					/*
					const FMassStandingSteeringFragment& StandingSteering = StandingSteeringList[EntityIndex];
					const FMassGhostLocationFragment& Ghost = GhostList[EntityIndex];
					const FMassZoneGraphShortPathFragment& ShortPath = ShortPathList[EntityIndex];
					const FMassStateTreeInstanceFragment& StateTreeInstance = StateTreeInstanceList[EntityIndex];
					*/
					const FVector EntityForward = Transform.GetTransform().GetRotation().GetForwardVector();

					constexpr float EyeHeight = 160.0f; // @todo: add eye height to agent.

					// Draw entity position and orientation.
					FVector BasePos = EntityLocation + FVector( 0.0f, 0.0f, 25.0f );

					AddShape( FGameplayDebuggerShape::MakeCircle( BasePos, FVector::UpVector, Radius, FColor::White ) );
					AddShape( FGameplayDebuggerShape::MakeSegment( BasePos, BasePos + EntityForward * Radius * 1.25f, FColor::White ) );

					// Velocity and steering target
					BasePos += FVector( 0.0f, 0.0f, 5.0f );
					AddShape( FGameplayDebuggerShape::MakeArrow( BasePos, BasePos + Velocity, 10.0f, 2.0f, FColor::Yellow ) );
					BasePos += FVector( 0.0f, 0.0f, 5.0f );
					AddShape( FGameplayDebuggerShape::MakeArrow( BasePos, BasePos + SteeringDesiredVelocity, 10.0f, 1.0f, FColorList::Pink ) );

					// Move target
					const FVector MoveBasePos = MoveTarget.Center + FVector( 0, 0, 5 );
					// dont draw red arrow past target
					// AddShape( FGameplayDebuggerShape::MakeArrow( MoveBasePos - MoveTarget.Forward * Radius, MoveBasePos + MoveTarget.Forward * Radius, 10.0f, 2.0f,
					// FColorList::MediumVioletRed ) );

					// Look at
					constexpr FVector::FReal LookArrowLength = 100.;
					BasePos = EntityLocation + FVector( 0, 0, EyeHeight );

					if ( bHasLookAt )
					{
						const FMassLookAtFragment& LookAt = LookAtList[EntityIndex];
						const FVector WorldLookDirection = Transform.GetTransform().TransformVector( LookAt.Direction );
						bool bLookArrowDrawn = false;
						if ( LookAt.LookAtMode == EMassLookAtMode::LookAtEntity && EntityManager.IsEntityValid( LookAt.TrackedEntity ) )
						{
							if ( const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>( LookAt.TrackedEntity ) )
							{
								FVector TargetPosition = TargetTransform->GetTransform().GetLocation();
								TargetPosition.Z = BasePos.Z;

								// don't draw red arrow past target
								// AddShape( FGameplayDebuggerShape::MakeCircle( TargetPosition, FVector::UpVector, Radius, FColor::Red ) );

								const FVector::FReal TargetDistance = FMath::Max( LookArrowLength, FVector::DotProduct( WorldLookDirection, TargetPosition - BasePos ) );
								AddShape( FGameplayDebuggerShape::MakeSegment( BasePos, BasePos + WorldLookDirection * TargetDistance, FColorList::LightGrey ) );
								bLookArrowDrawn = true;
							}
						}

						if ( LookAt.bRandomGazeEntities && EntityManager.IsEntityValid( LookAt.GazeTrackedEntity ) )
						{
							if ( const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>( LookAt.GazeTrackedEntity ) )
							{
								FVector TargetPosition = TargetTransform->GetTransform().GetLocation();
								TargetPosition.Z = BasePos.Z;
								AddShape( FGameplayDebuggerShape::MakeCircle( TargetPosition, FVector::UpVector, Radius, FColor::Turquoise ) );
							}
						}

						if ( !bLookArrowDrawn )
						{
							AddShape( FGameplayDebuggerShape::MakeArrow( BasePos, BasePos + WorldLookDirection * LookArrowLength, 10.0f, 1.0f, FColor::Turquoise ) );
						}
					}

					// SmartObject
					/*
					if ( bHasSOUser )
					{
						const FMassSmartObjectUserFragment& SOUser = SOUserList[EntityIndex];
						if ( SOUser.InteractionHandle.IsValid() )
						{
							const FVector ZOffset = FVector( 0.0f, 0.0f, 25.0f );
							const FTransform SlotTransform = SmartObjectSubsystem->GetSlotTransform( SOUser.InteractionHandle ).Get( FTransform::Identity );
							const FVector SlotLocation = SlotTransform.GetLocation();
							AddShape( FGameplayDebuggerShape::MakeSegment( EntityLocation + ZOffset, SlotLocation + ZOffset, 3.0f, FColorList::Orange ) );
						}
					}
					*/

					// Path
					/*
					if ( bShowNearEntityPath )
					{
						const FVector ZOffset = FVector( 0.0f, 0.0f, 25.0f );
						for ( uint8 PointIndex = 0; PointIndex < ShortPath.NumPoints - 1; PointIndex++ )
						{
							const FMassZoneGraphPathPoint& CurrPoint = ShortPath.Points[PointIndex];
							const FMassZoneGraphPathPoint& NextPoint = ShortPath.Points[PointIndex + 1];
							AddShape( FGameplayDebuggerShape::MakeSegment( CurrPoint.Position + ZOffset, NextPoint.Position + ZOffset, 3.0f, FColorList::Grey ) );
						}

						for ( uint8 PointIndex = 0; PointIndex < ShortPath.NumPoints; PointIndex++ )
						{
							const FMassZoneGraphPathPoint& CurrPoint = ShortPath.Points[PointIndex];
							const FVector CurrBase = CurrPoint.Position + ZOffset;
							// Lane tangents
							AddShape( FGameplayDebuggerShape::MakeSegment( CurrBase, CurrBase + CurrPoint.Tangent.GetVector() * 50.0f, 1.0f, FColorList::LightGrey ) );
						}
					}

					if ( bShowNearEntityAvoidance )
					{
						// Standing avoidance.
						if ( Ghost.IsValid( MoveTarget.GetCurrentActionID() ) )
						{
							FVector GhostBasePos = Ghost.Location + FVector( 0.0f, 0.0f, 25.0f );
							AddShape( FGameplayDebuggerShape::MakeCircle( GhostBasePos, FVector::UpVector, Radius.Radius, FColorList::LightGrey ) );
							GhostBasePos += FVector( 0, 0, 5 );
							AddShape( FGameplayDebuggerShape::MakeArrow( GhostBasePos, GhostBasePos + Ghost.Velocity, 10.0f, 2.0f, FColorList::LightGrey ) );

							const FVector GhostTargetBasePos = StandingSteering.TargetLocation + FVector( 0.0f, 0.0f, 25.0f );
							AddShape( FGameplayDebuggerShape::MakeCircle( GhostTargetBasePos, FVector::UpVector, Radius.Radius * 0.75f, FColorList::Orange ) );
						}
					}
					*/

					// Status
					if ( DistanceToEntitySq < FMath::Square( MaxViewDistance * 0.5f ) )
					{
						FString Status;

						// Entity name
						FMassEntityHandle Entity = Context.GetEntity( EntityIndex );
						Status += TEXT( "{orange}" );
						Status += Entity.DebugGetDescription();
						Status += TEXT( " {white}LOD " );
						switch ( LOD )
						{
						case EMassLOD::High:
							Status += TEXT( "High" );
							break;
						case EMassLOD::Medium:
							Status += TEXT( "Med" );
							break;
						case EMassLOD::Low:
							Status += TEXT( "Low" );
							break;
						case EMassLOD::Off:
							Status += TEXT( "Off" );
							break;
						default:
							Status += TEXT( "?" );
							break;
						}
						Status += TEXT( "\n" );

						// Current StateTree task
						/*
						if ( StateTree != nullptr )
						{
							if ( FStateTreeInstanceData* InstanceData = MassStateTreeSubsystem->GetInstanceData( StateTreeInstance.InstanceHandle ) )
							{
								FMassStateTreeExecutionContext StateTreeContext( *OwnerPlayerController, *StateTree, *InstanceData, EntityManager, *SignalSubsystem, Context );
								StateTreeContext.SetEntity( Entity );

								Status += StateTreeContext.GetActiveStateName();
								Status += FString::Printf( TEXT( "  {yellow}%d{white}\n" ), StateTreeContext.GetStateChangeCount() );
							}
							else
							{
								Status += TEXT( "{red}<No StateTree instance>{white}\n" );
							}
						}
						*/

						// Movement info
						Status += FString::Printf( TEXT( "{yellow}%s/%03d {lightgrey}Speed:{white}%.1f {lightgrey}Force:{white}%.1f\n" ),
												   *UEnum::GetDisplayValueAsText( MoveTarget.GetCurrentAction() ).ToString(),
												   MoveTarget.GetCurrentActionID(),
												   Velocity.Length(),
												   Force.Length() );
						Status += FString::Printf(
							TEXT( "{pink}-> %s {white}Dist: %.1f\n" ), *UEnum::GetDisplayValueAsText( MoveTarget.IntentAtGoal ).ToString(), MoveTarget.DistanceToGoal );

						// Look
						if ( bHasLookAt )
						{
							const FMassLookAtFragment& LookAt = LookAtList[EntityIndex];
							const double RemainingTime = LookAt.GazeDuration - ( CurrentTime - LookAt.GazeStartTime );
							Status += FString::Printf( TEXT( "{turquoise}%s/%s {lightgrey}%.1f\n" ),
													   *UEnum::GetDisplayValueAsText( LookAt.LookAtMode ).ToString(),
													   *UEnum::GetDisplayValueAsText( LookAt.RandomGazeMode ).ToString(),
													   RemainingTime );
						}

						if ( !Status.IsEmpty() )
						{
							BasePos += FVector( 0, 0, 50 );
							constexpr FVector::FReal ViewWeight = 0.6f;						   // Higher the number the more the view angle affects the score.
							const FVector::FReal ViewScale = 1. - ( ViewDot / MinViewDirDot ); // Zero at center of screen
							NearEntityDescriptions.Emplace( static_cast<float>( DistanceToEntitySq * ( ( 1. - ViewWeight ) + ViewScale * ViewWeight ) ), BasePos, Status );
						}
					}
				}
			} );
		}

		/*
		if ( bShowNearEntityAvoidance )
		{
			FMassEntityQuery EntityColliderQuery;
			EntityColliderQuery.AddRequirement<FMassAvoidanceColliderFragment>( EMassFragmentAccess::ReadOnly );
			EntityColliderQuery.AddRequirement<FTransformFragment>( EMassFragmentAccess::ReadOnly );
			FMassExecutionContext Context( EntityManager, 0.f );
			EntityColliderQuery.ForEachEntityChunk( EntityManager, Context, [this, ViewLocation, ViewDirection]( const FMassExecutionContext& Context )
			{
				const int32 NumEntities = Context.GetNumEntities();
				const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
				const TConstArrayView<FMassAvoidanceColliderFragment> CollidersList = Context.GetFragmentView<FMassAvoidanceColliderFragment>();

				for ( int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex )
				{
					const FTransformFragment& Transform = TransformList[EntityIndex];
					const FVector EntityLocation = Transform.GetTransform().GetLocation();
					const FVector EntityForward = Transform.GetTransform().GetRotation().GetForwardVector();

					FVector BasePos = EntityLocation + FVector( 0.0f, 0.0f, 25.0f );

					// Cull entities
					if ( !IsLocationInViewCone( ViewLocation, ViewDirection, EntityLocation ) )
					{
						continue;
					}

					// Display colliders
					const FMassAvoidanceColliderFragment& Collider = CollidersList[EntityIndex];
					if ( Collider.Type == EMassColliderType::Circle )
					{
						AddShape( FGameplayDebuggerShape::MakeCircle( BasePos, FVector::UpVector, Collider.GetCircleCollider().Radius, FColor::Blue ) );
					}
					else if ( Collider.Type == EMassColliderType::Pill )
					{
						const FMassPillCollider& Pill = Collider.GetPillCollider();
						AddShape( FGameplayDebuggerShape::MakeCircle( BasePos + Pill.HalfLength * EntityForward, FVector::UpVector, Pill.Radius, FColor::Blue ) );
						AddShape( FGameplayDebuggerShape::MakeCircle( BasePos - Pill.HalfLength * EntityForward, FVector::UpVector, Pill.Radius, FColor::Blue ) );
					}
				}
			} );
		}

		// Cap labels to closest ones.
		NearEntityDescriptions.Sort( []( const FEntityDescription& LHS, const FEntityDescription& RHS ) { return LHS.Score < RHS.Score; } );
		constexpr int32 MaxLabels = 15;
		if ( NearEntityDescriptions.Num() > MaxLabels )
		{
			NearEntityDescriptions.RemoveAt( MaxLabels, NearEntityDescriptions.Num() - MaxLabels );
		}
		*/
	}
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_MassExtensions::MakeInstance()
{
	return MakeShareable( new FGameplayDebuggerCategory_MassExtensions() );
}

void FGameplayDebuggerCategory_MassExtensions::DrawData( APlayerController* OwnerPlayerController, FGameplayDebuggerCanvasContext& CanvasContext )
{
	struct FEntityLayoutRect
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		int32 Index = INDEX_NONE;
		float Alpha = 1.0f;
	};

	constexpr int32 MaxDesc = 20;

	TArray<FEntityLayoutRect> Layout;
	Layout.Reserve( MaxDesc );

	// The loop below is O(N^2), make sure to keep the N small.
	const int32 NumDescs = FMath::Min( NearEntityDescriptions.Num(), MaxDesc );

	// The labels are assumed to have been ordered in order of importance (i.e. front to back).
	for ( int32 Index = 0; Index < NumDescs; Index++ )
	{
		const FEntityDescription& Desc = NearEntityDescriptions[Index];
		if ( Desc.Description.Len() && CanvasContext.IsLocationVisible( Desc.Location ) )
		{
			float SizeX = 0, SizeY = 0;
			const FVector2D ScreenLocation = CanvasContext.ProjectLocation( Desc.Location );
			CanvasContext.MeasureString( Desc.Description, SizeX, SizeY );

			FEntityLayoutRect Rect;
			Rect.Min = ScreenLocation + FVector2D( 0, -SizeY * 0.5f );
			Rect.Max = Rect.Min + FVector2D( SizeX, SizeY );
			Rect.Index = Index;
			Rect.Alpha = 0.0f;

			// Calculate transparency based on how much more important rects are overlapping the new rect.
			const FVector::FReal Area = FMath::Max( 0.0, Rect.Max.X - Rect.Min.X ) * FMath::Max( 0.0, Rect.Max.Y - Rect.Min.Y );
			const FVector::FReal InvArea = Area > KINDA_SMALL_NUMBER ? 1.0 / Area : 0.0;
			FVector::FReal Coverage = 0.0;

			for ( const FEntityLayoutRect& Other : Layout )
			{
				// Calculate rect intersection
				const FVector::FReal MinX = FMath::Max( Rect.Min.X, Other.Min.X );
				const FVector::FReal MinY = FMath::Max( Rect.Min.Y, Other.Min.Y );
				const FVector::FReal MaxX = FMath::Min( Rect.Max.X, Other.Max.X );
				const FVector::FReal MaxY = FMath::Min( Rect.Max.Y, Other.Max.Y );

				// return zero area if not overlapping
				const FVector::FReal IntersectingArea = FMath::Max( 0.0, MaxX - MinX ) * FMath::Max( 0.0, MaxY - MinY );
				Coverage += ( IntersectingArea * InvArea ) * Other.Alpha;
			}

			Rect.Alpha = FloatCastChecked<float>( FMath::Square( 1.0 - FMath::Min( Coverage, 1.0 ) ), UE::LWC::DefaultFloatPrecision );

			if ( Rect.Alpha > KINDA_SMALL_NUMBER )
			{
				Layout.Add( Rect );
			}
		}
	}

	// Render back to front so that the most important item renders at top.
	const FVector2D Padding( 5, 5 );
	for ( int32 Index = Layout.Num() - 1; Index >= 0; Index-- )
	{
		const FEntityLayoutRect& Rect = Layout[Index];
		const FEntityDescription& Desc = NearEntityDescriptions[Rect.Index];

		const FVector2D BackgroundPosition( Rect.Min - Padding );
		FCanvasTileItem Background( Rect.Min - Padding, Rect.Max - Rect.Min + Padding * 2.0f, FLinearColor( 0.0f, 0.0f, 0.0f, 0.35f * Rect.Alpha ) );
		Background.BlendMode = SE_BLEND_TranslucentAlphaOnly;
		CanvasContext.DrawItem( Background,
								FloatCastChecked<float>( BackgroundPosition.X, UE::LWC::DefaultFloatPrecision ),
								FloatCastChecked<float>( BackgroundPosition.Y, UE::LWC::DefaultFloatPrecision ) );

		CanvasContext.PrintAt( FloatCastChecked<float>( Rect.Min.X, UE::LWC::DefaultFloatPrecision ),
							   FloatCastChecked<float>( Rect.Min.Y, UE::LWC::DefaultFloatPrecision ),
							   FColor::White,
							   Rect.Alpha,
							   Desc.Description );
	}

	FGameplayDebuggerCategory::DrawData( OwnerPlayerController, CanvasContext );
}

#endif // WITH_GAMEPLAY_DEBUGGER && WITH_MASSGAMEPLAY_DEBUG
