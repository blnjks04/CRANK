#include "Character/WindupCharacter.h"
#include "Character/TorqueComponent.h"
#include "Character/WindInteractionComponent.h"
#include "Character/LaunchComponent.h"
#include "Character/GrabComponent.h"
#include "Character/ScatterReviveComponent.h"
#include "Character/WindupAnimInstance.h"
#include "Character/CrankInput.h"
#include "Gameplay/CarryableActor.h"
#include "Gameplay/BodyPartPickup.h"
#include "Gameplay/SurfaceZone.h"
#include "Boss/WaterTrail.h"
#include "Boss/BossDustEater.h"
#include "Game/RaidGameMode.h"
#include "CrankBalance.h"
#include "CrankAssets.h"
#include "FX/CrankFX.h"
#include "FX/CrankSound.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnhancedInputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "AnimationRuntime.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace
{
	float ServerNow(const UWorld* World)
	{
		if (!World)
		{
			return 0.f;
		}
		if (const AGameStateBase* GS = World->GetGameState())
		{
			return GS->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}

	FVector RefPoseAxisLocal(const USkeletalMeshComponent* Mesh, FName Bone, const FVector& ComponentDir)
	{
		const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!Asset)
		{
			return ComponentDir;
		}
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		const int32 Index = Ref.FindBoneIndex(Bone);
		if (Index == INDEX_NONE)
		{
			return ComponentDir;
		}
		const FTransform CS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Index);
		return CS.InverseTransformVectorNoScale(ComponentDir).GetSafeNormal();
	}
}

AWindupCharacter::AWindupCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(24.f, 50.f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCollisionResponseToChannel(ECC_BossBlocker, ECR_Ignore);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 600.f, 0.f);
	Move->JumpZVelocity = 480.f;	// ~117 uu: one drawer step (100) per jump
	Move->AirControl = 0.35f;
	Move->MaxWalkSpeed = CrankBalance::SpeedRun;
	Move->MaxWalkSpeedCrouched = CrankBalance::SpeedCrawl;
	Move->MaxAcceleration = 1500.f;
	Move->BrakingDecelerationWalking = 1400.f;
	Move->GroundFriction = 8.f;
	Move->NavAgentProps.bCanCrouch = true;
	Move->SetCrouchedHalfHeight(26.f);
	Move->bCanWalkOffLedgesWhenCrouching = true;
	Move->MaxStepHeight = 28.f;
	Move->SetWalkableFloorAngle(48.f);
	Move->bEnablePhysicsInteraction = true;
	Move->PushForceFactor = 300000.f;

	USkeletalMeshComponent* SkelMesh = GetMesh();
	SkelMesh->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
	SkelMesh->SetCollisionObjectType(ECC_PhysicsBody);
	SkelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SkelMesh->SetCollisionResponseToAllChannels(ECR_Block);
	SkelMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SkelMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SkelMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	SkelMesh->SetCollisionResponseToChannel(ECC_BossBlocker, ECR_Ignore);
	SkelMesh->SetGenerateOverlapEvents(false);
	SkelMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SkelMesh->SetAnimInstanceClass(UWindupAnimInstance::StaticClass());

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	CameraBoom->TargetArmLength = 380.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 30.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->ProbeSize = 10.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(85.f);

	Torque = CreateDefaultSubobject<UTorqueComponent>(TEXT("Torque"));
	Wind = CreateDefaultSubobject<UWindInteractionComponent>(TEXT("Wind"));
	LaunchComp = CreateDefaultSubobject<ULaunchComponent>(TEXT("Launch"));
	Grab = CreateDefaultSubobject<UGrabComponent>(TEXT("Grab"));
	Scatter = CreateDefaultSubobject<UScatterReviveComponent>(TEXT("Scatter"));
	PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicalAnimation"));

	KeyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyMesh"));
	KeyMesh->SetupAttachment(SkelMesh);
	KeyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	KeyMesh->SetGenerateOverlapEvents(false);
	KeyMesh->SetRelativeLocation(FVector(-18.f, 0.f, 58.f));

	SteamFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("SteamFX"));
	SteamFX->SetupAttachment(SkelMesh);
	SteamFX->SetRelativeLocation(FVector(0.f, 0.f, 72.f));
	SteamFX->SetRelativeRotation(FRotator(80.f, 0.f, 0.f));
	SteamFX->Effect = ECrankFX::Steam;
	SteamFX->Rate = 10.f;
	SteamFX->SpawnExtent = FVector(6.f, 14.f, 4.f);

	DustFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("DustFX"));
	DustFX->SetupAttachment(SkelMesh);
	DustFX->SetRelativeLocation(FVector(0.f, 0.f, 6.f));
	DustFX->Effect = ECrankFX::DustTrail;
	DustFX->Rate = 7.f;

	StarsFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("StarsFX"));
	StarsFX->SetupAttachment(SkelMesh);
	StarsFX->SetRelativeLocation(FVector(0.f, 0.f, 112.f));
	StarsFX->Effect = ECrankFX::StunStars;
	StarsFX->Rate = 1.f;

	FrostFX = CreateDefaultSubobject<UCrankFXEmitterComponent>(TEXT("FrostFX"));
	FrostFX->SetupAttachment(SkelMesh);
	FrostFX->SetRelativeLocation(FVector(14.f, 0.f, 86.f));
	FrostFX->Effect = ECrankFX::Steam;
	FrostFX->Rate = 1.2f;
	FrostFX->EffectScale = 0.5f;

	SemiRagdollBones = { TEXT("upperarm_l"), TEXT("upperarm_r") };

	bReplicates = true;
	SetNetUpdateFrequency(100.f);
}

void AWindupCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWindupCharacter, BodyRep);
	DOREPLIFETIME(AWindupCharacter, PlayerColorIndex);
	DOREPLIFETIME(AWindupCharacter, EmoteRep);
	DOREPLIFETIME(AWindupCharacter, CarriedBy);
	DOREPLIFETIME(AWindupCharacter, KeyTurns);
	DOREPLIFETIME(AWindupCharacter, DustyUntil);
	DOREPLIFETIME(AWindupCharacter, HelplessUntil);
	DOREPLIFETIME(AWindupCharacter, InvertedUntil);
}

void AWindupCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPaint();
}

void AWindupCharacter::BeginPlay()
{
	Super::BeginPlay();

	MeshDefaultRelative = GetMesh()->GetRelativeTransform();
	DefaultArmLength = CameraBoom->TargetArmLength;
	PhysicalAnimation->SetSkeletalMeshComponent(GetMesh());
	BodyStateLocalTime = GetWorld()->GetTimeSeconds();

	// Put the key on its socket, or ride the spine bone so it follows the ragdoll.
	if (GetMesh()->DoesSocketExist(KeySocket))
	{
		KeyMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, KeySocket);
	}
	else if (const USkeletalMesh* Asset = GetMesh()->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		const int32 SpineIndex = Ref.FindBoneIndex(TEXT("spine"));
		if (SpineIndex != INDEX_NONE)
		{
			const FTransform SpineCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, SpineIndex);
			const FVector KeyCS = KeyMesh->GetRelativeLocation();
			KeyMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("spine"));
			KeyMesh->SetRelativeLocation(SpineCS.InverseTransformPosition(KeyCS));
		}
	}
	KeyBackAxisLocal = RefPoseAxisLocal(GetMesh(), TEXT("spine"), FVector(-1.f, 0.f, 0.f));
	bKeyAxisCached = true;

	ApplyPaint();
	RefreshMovementSpeed();	// semi-ragdoll is switched on from Tick once the mesh has a pose
	SteamFX->SetEmitting(false);
	DustFX->SetEmitting(false);
	StarsFX->SetEmitting(false);
	FrostFX->SetEmitting(false);
}

void AWindupCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshMovementSpeed();
}

void AWindupCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
}

// ------------------------------------------------------------------------------------------
// Input

void AWindupCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}
	const FCrankInputActions& A = CrankInput::Get();
	Input->BindAction(A.Move, ETriggerEvent::Triggered, this, &AWindupCharacter::Input_Move);
	Input->BindAction(A.Move, ETriggerEvent::Completed, this, &AWindupCharacter::Input_MoveCompleted);
	Input->BindAction(A.LookMouse, ETriggerEvent::Triggered, this, &AWindupCharacter::Input_LookMouse);
	Input->BindAction(A.LookStick, ETriggerEvent::Triggered, this, &AWindupCharacter::Input_LookStick);
	Input->BindAction(A.LookStick, ETriggerEvent::Completed, this, &AWindupCharacter::Input_LookStickCompleted);
	Input->BindAction(A.Jump, ETriggerEvent::Started, this, &AWindupCharacter::Input_JumpStarted);
	Input->BindAction(A.Jump, ETriggerEvent::Completed, this, &AWindupCharacter::Input_JumpCompleted);
	Input->BindAction(A.GrabLeft, ETriggerEvent::Started, this, &AWindupCharacter::Input_GrabLeftStarted);
	Input->BindAction(A.GrabLeft, ETriggerEvent::Completed, this, &AWindupCharacter::Input_GrabLeftCompleted);
	Input->BindAction(A.GrabRight, ETriggerEvent::Started, this, &AWindupCharacter::Input_GrabRightStarted);
	Input->BindAction(A.GrabRight, ETriggerEvent::Completed, this, &AWindupCharacter::Input_GrabRightCompleted);
	Input->BindAction(A.HoldWind, ETriggerEvent::Started, this, &AWindupCharacter::Input_HoldWindStarted);
	Input->BindAction(A.HoldWind, ETriggerEvent::Completed, this, &AWindupCharacter::Input_HoldWindCompleted);
	Input->BindAction(A.SelfLaunch, ETriggerEvent::Started, this, &AWindupCharacter::Input_SelfLaunch);
	Input->BindAction(A.Emote[0], ETriggerEvent::Started, this, &AWindupCharacter::Input_Emote1);
	Input->BindAction(A.Emote[1], ETriggerEvent::Started, this, &AWindupCharacter::Input_Emote2);
	Input->BindAction(A.Emote[2], ETriggerEvent::Started, this, &AWindupCharacter::Input_Emote3);
	Input->BindAction(A.Emote[3], ETriggerEvent::Started, this, &AWindupCharacter::Input_Emote4);
}

void AWindupCharacter::Input_Move(const FInputActionValue& Value)
{
	FVector2D V = Value.Get<FVector2D>();
	MoveInput = V;
	const float Dt = GetWorld()->GetDeltaSeconds();

	if (Wind->IsWinding())
	{
		Wind->SetPullInput(V.Y < -0.5f);
		return;
	}
	if (Wind->IsBeingWound())
	{
		Wind->SetBreakFreeInput(V.Size(), Dt);
		return;
	}
	if (CarriedBy)
	{
		StruggleTimer = V.Size() > 0.9f ? StruggleTimer + Dt : FMath::Max(0.f, StruggleTimer - Dt);
		if (StruggleTimer > 0.7f)
		{
			StruggleTimer = 0.f;
			ServerStruggle();
		}
		return;
	}
	if (!CanMoveByInput() || !Controller)
	{
		return;
	}

	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), V.X);
}

void AWindupCharacter::Input_MoveCompleted(const FInputActionValue& Value)
{
	MoveInput = FVector2D::ZeroVector;
	if (Wind->IsWinding())
	{
		Wind->SetPullInput(false);
	}
	Wind->SetBreakFreeInput(0.f, GetWorld()->GetDeltaSeconds());
}

void AWindupCharacter::Input_LookMouse(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (Wind->ConsumesLookInput())
	{
		Wind->AddMouseDelta(V);
		return;
	}
	AddControllerYawInput(V.X);
	AddControllerPitchInput(-V.Y);
}

void AWindupCharacter::Input_LookStick(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (Wind->ConsumesLookInput())
	{
		Wind->AddStickInput(V);
		return;
	}
	const float Dt = GetWorld()->GetDeltaSeconds();
	AddControllerYawInput(V.X * 75.f * Dt);
	AddControllerPitchInput(-V.Y * 55.f * Dt);
}

void AWindupCharacter::Input_LookStickCompleted(const FInputActionValue& Value)
{
	Wind->AddStickInput(FVector2D::ZeroVector);
}

void AWindupCharacter::Input_JumpStarted(const FInputActionValue& Value)
{
	if (CanMoveByInput())
	{
		Jump();
	}
}

void AWindupCharacter::Input_JumpCompleted(const FInputActionValue& Value)
{
	StopJumping();
}

void AWindupCharacter::Input_GrabLeftStarted(const FInputActionValue& Value)		{ Grab->SetGrabInput(ECrankHand::Left, true); }
void AWindupCharacter::Input_GrabLeftCompleted(const FInputActionValue& Value)		{ Grab->SetGrabInput(ECrankHand::Left, false); }
void AWindupCharacter::Input_GrabRightStarted(const FInputActionValue& Value)		{ Grab->SetGrabInput(ECrankHand::Right, true); }
void AWindupCharacter::Input_GrabRightCompleted(const FInputActionValue& Value)	{ Grab->SetGrabInput(ECrankHand::Right, false); }
void AWindupCharacter::Input_HoldWindStarted(const FInputActionValue& Value)		{ Wind->SetHoldWindInput(true); }
void AWindupCharacter::Input_HoldWindCompleted(const FInputActionValue& Value)		{ Wind->SetHoldWindInput(false); }
void AWindupCharacter::Input_SelfLaunch(const FInputActionValue& Value)			{ LaunchComp->RequestSelfLaunch(); }

void AWindupCharacter::ServerStruggle_Implementation()
{
	if (AWindupCharacter* Carrier = CarriedBy)
	{
		EndCarried(FVector::ZeroVector);
		Carrier->GetGrab()->ReleaseActor(this);
	}
	else if (AWindupCharacter* Winder = Wind->GetWoundBy())
	{
		Winder->GetWind()->StopWinding();
	}
}

// ------------------------------------------------------------------------------------------
// Emotes

void AWindupCharacter::RequestEmote(uint8 EmoteId)
{
	if (GetBodyState() != ECrankBodyState::Normal || GetVelocity().Size2D() > 60.f)
	{
		return;
	}
	if (HasAuthority())
	{
		ServerEmote_Implementation(EmoteId);
	}
	else
	{
		ServerEmote(EmoteId);
	}
}

void AWindupCharacter::ServerEmote_Implementation(uint8 EmoteId)
{
	if (GetBodyState() != ECrankBodyState::Normal || EmoteId < 1 || EmoteId > 4)
	{
		return;
	}
	EmoteRep.EmoteId = EmoteId;
	EmoteRep.Seq++;
	EmoteLocalStart = GetWorld()->GetTimeSeconds();
	LastEmoteSeq = EmoteRep.Seq;
}

uint8 AWindupCharacter::GetActiveEmote() const
{
	return GetBodyState() == ECrankBodyState::Normal ? EmoteRep.EmoteId : 0;
}

float AWindupCharacter::GetEmoteTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - EmoteLocalStart : 0.f;
}

// ------------------------------------------------------------------------------------------
// State queries

float AWindupCharacter::GetBodyStateTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - BodyStateLocalTime : 0.f;
}

bool AWindupCharacter::IsCrawling() const
{
	return GetBodyState() == ECrankBodyState::Normal && Torque && Torque->IsDischarged();
}

bool AWindupCharacter::IsHelpless() const
{
	return ServerNow(GetWorld()) < HelplessUntil;
}

bool AWindupCharacter::IsDusty() const
{
	return ServerNow(GetWorld()) < DustyUntil;
}

bool AWindupCharacter::IsViewInverted() const
{
	return ServerNow(GetWorld()) < InvertedUntil;
}

bool AWindupCharacter::CanUseArms() const
{
	return GetBodyState() == ECrankBodyState::Normal && !IsHelpless() && !CarriedBy && !(Wind && Wind->IsBeingWound());
}

bool AWindupCharacter::CanMoveByInput() const
{
	return GetBodyState() == ECrankBodyState::Normal && !IsHelpless() && !CarriedBy
		&& !(Wind && (Wind->IsWinding() || Wind->IsBeingWound()));
}

bool AWindupCharacter::CanSelfLaunch() const
{
	return GetBodyState() == ECrankBodyState::Normal && !IsHelpless() && !CarriedBy
		&& Torque && Torque->CanLaunch() && !(Wind && Wind->IsWinding());
}

bool AWindupCharacter::CanBeWound() const
{
	return GetBodyState() == ECrankBodyState::Normal && !CarriedBy && Wind && !Wind->IsBeingWound() && !Wind->IsWinding();
}

bool AWindupCharacter::CanContinueBeingWound() const
{
	return GetBodyState() == ECrankBodyState::Normal && !CarriedBy;
}

FLinearColor AWindupCharacter::GetPlayerColor() const
{
	return CrankUtil::PlayerColor(PlayerColorIndex);
}

void AWindupCharacter::SetPlayerColorIndex(int32 Index)
{
	PlayerColorIndex = Index;
	ApplyPaint();
}

void AWindupCharacter::OnRep_PlayerColor()
{
	ApplyPaint();
}

void AWindupCharacter::ApplyPaint()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh || SkelMesh->GetNumMaterials() <= PaintMaterialIndex)
	{
		return;
	}
	if (!PaintMID || SkelMesh->GetMaterial(PaintMaterialIndex) != PaintMID)
	{
		PaintMID = SkelMesh->CreateDynamicMaterialInstance(PaintMaterialIndex);
	}
	if (PaintMID)
	{
		PaintMID->SetVectorParameterValue(TEXT("PaintColor"), GetPlayerColor());
	}
}

FVector AWindupCharacter::GetKeyWorldLocation() const
{
	if (KeyMesh && KeyMesh->IsVisible())
	{
		return KeyMesh->GetComponentLocation();
	}
	return GetActorLocation() - GetActorForwardVector() * 20.f + FVector(0.f, 0.f, 8.f);
}

// ------------------------------------------------------------------------------------------
// Torque coupling

float AWindupCharacter::GetDrainMultiplier() const
{
	float Multiplier = 1.f;
	if (const ACarryableActor* Item = Cast<ACarryableActor>(Grab->GetCarriedActor()))
	{
		Multiplier *= Item->GetDrainMultiplierFor(this);
	}
	if (GetCarriedFriend())
	{
		Multiplier *= CrankBalance::MulCarryFriend;
	}
	Multiplier *= ColdMultiplier;
	return Multiplier;
}

void AWindupCharacter::RefreshMovementSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move || !Torque)
	{
		return;
	}

	float Speed = Torque->GetBandSpeed();
	float Cap = 0.f;
	if (const ACarryableActor* Item = Cast<ACarryableActor>(Grab ? Grab->GetCarriedActor() : nullptr))
	{
		Cap = Item->GetSpeedCapFor(this);
	}
	if (GetCarriedFriend())
	{
		Cap = Cap > 0.f ? FMath::Min(Cap, CrankBalance::SpeedRun) : CrankBalance::SpeedRun;
	}
	if (Cap > 0.f)
	{
		Speed = FMath::Min(Speed, Cap);
	}
	Move->MaxWalkSpeed = Speed * SurfaceSpeedBoost;
	Move->MaxWalkSpeedCrouched = CrankBalance::SpeedCrawl * SurfaceSpeedBoost;
}

void AWindupCharacter::OnTorqueUpdated(float OldTorque, float NewTorque)
{
	if (NewTorque <= 0.f && OldTorque > 0.f)
	{
		CrankSound::PlayAt(this, TEXT("SFX_WindDown"), GetActorLocation(), 0.8f);
	}
	if (CrankUtil::BandForTorque(OldTorque) != CrankUtil::BandForTorque(NewTorque))
	{
		ApplySemiRagdoll();
	}
}

// ------------------------------------------------------------------------------------------
// Body state machine

void AWindupCharacter::SetBodyState(ECrankBodyState NewState, const FVector& Param)
{
	if (!HasAuthority())
	{
		return;
	}
	const ECrankBodyState OldState = BodyRep.State;
	BodyRep.State = NewState;
	BodyRep.Param = Param;
	BodyRep.Seq++;
	BodyStateLocalTime = GetWorld()->GetTimeSeconds();
	ApplyBodyState(OldState, NewState);
	ForceNetUpdate();
}

void AWindupCharacter::OnRep_BodyRep(const FBodyStateRep& OldRep)
{
	BodyStateLocalTime = GetWorld()->GetTimeSeconds();
	ApplyBodyState(OldRep.State, BodyRep.State);
}

void AWindupCharacter::ApplyBodyState(ECrankBodyState OldState, ECrankBodyState NewState)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();

	// Leave the old state.
	if (OldState == ECrankBodyState::Ragdoll && NewState != ECrankBodyState::Ragdoll)
	{
		ExitRagdollLocal(NewState == ECrankBodyState::GettingUp || NewState == ECrankBodyState::Normal);
	}
	if (OldState == ECrankBodyState::Splat)
	{
		Move->GravityScale = 1.f;
		GetMesh()->SetRelativeScale3D(MeshDefaultRelative.GetScale3D());
	}
	if (OldState == ECrankBodyState::Scattered || OldState == ECrankBodyState::Trapped)
	{
		SetActorHiddenInGame(false);
		KeyMesh->SetVisibility(true);
	}

	switch (NewState)
	{
	case ECrankBodyState::Normal:
	case ECrankBodyState::GettingUp:
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		SetActorHiddenInGame(false);
		if (Move->MovementMode == MOVE_None || Move->MovementMode == MOVE_Flying)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
		if (NewState == ECrankBodyState::GettingUp)
		{
			CrankSound::PlayAt(this, TEXT("SFX_GetUp"), GetActorLocation(), 0.7f, FMath::FRandRange(0.9f, 1.15f));
		}
		ApplySemiRagdoll();
		break;

	case ECrankBodyState::Ragdoll:
		EnterRagdollLocal(BodyRep.Param);
		CrankSound::PlayAt(this, TEXT("SFX_Thud"), GetActorLocation(), 0.8f, FMath::FRandRange(0.85f, 1.15f));
		break;

	case ECrankBodyState::Flying:
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Move->SetMovementMode(MOVE_Falling);
		if (IsLocallyControlled() && !HasAuthority())
		{
			LaunchCharacter(BodyRep.Param, true, true);
		}
		CrankSound::PlayAt(this, TEXT("SFX_Launch"), GetActorLocation(), 1.f, FMath::FRandRange(0.95f, 1.05f));
		UCrankFXSubsystem::Spawn(this, ECrankFX::Puff, GetActorLocation() - FVector(0, 0, 40.f), FRotator::ZeroRotator, 1.4f);
		break;

	case ECrankBodyState::Splat:
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);
		if (!HasAuthority())
		{
			SetActorRotation(FRotator(0.f, (-FVector(BodyRep.Param)).Rotation().Yaw, 0.f));
		}
		CrankSound::PlayAt(this, TEXT("SFX_Splat"), GetActorLocation(), 1.f);
		break;

	case ECrankBodyState::Carried:
		Move->DisableMovement();
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		break;

	case ECrankBodyState::Scattered:
		Move->DisableMovement();
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorHiddenInGame(true);
		CrankSound::PlayAt(this, TEXT("SFX_Pop"), GetActorLocation(), 1.f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Pop, GetActorLocation());
		break;

	case ECrankBodyState::Trapped:
		Move->DisableMovement();
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorHiddenInGame(true);
		CrankSound::PlayAt(this, TEXT("SFX_Suck"), GetActorLocation(), 1.f);
		break;

	case ECrankBodyState::Frozen:
		Move->DisableMovement();
		break;
	}

	if (IsLocallyControlled() || HasAuthority())
	{
		UpdateCrawlCrouch();
	}
}

void AWindupCharacter::StartRagdoll(const FVector& Impulse, float MinTime)
{
	if (!HasAuthority())
	{
		return;
	}
	const ECrankBodyState State = GetBodyState();
	if (State == ECrankBodyState::Scattered || State == ECrankBodyState::Trapped || State == ECrankBodyState::Frozen)
	{
		return;
	}
	if (CarriedBy)
	{
		AWindupCharacter* Carrier = CarriedBy;
		EndCarried(FVector::ZeroVector);
		Carrier->GetGrab()->ReleaseActor(this);
	}
	EndAllInteractions();
	RagdollMinTime = MinTime;
	SetBodyState(ECrankBodyState::Ragdoll, Impulse);
}

void AWindupCharacter::EnterRagdollLocal(const FVector& Impulse)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	// A landing / wall hit still carries the speed into that surface: thin floors get tunneled by the ragdoll.
	const FVector InheritVelocity = RemoveVelocityIntoSurfaces(GetVelocity());

	Move->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	if (!SkelMesh->GetPhysicsAsset())
	{
		return;
	}
	PhysicalAnimation->SetStrengthMultiplyer(0.f);
	SkelMesh->SetAllUseCCD(true);	// fast bodies must not pass through floors / walls
	SkelMesh->SetAllBodiesSimulatePhysics(true);
	SkelMesh->SetSimulatePhysics(true);
	SkelMesh->SetAllBodiesPhysicsBlendWeight(1.f);
	SkelMesh->WakeAllRigidBodies();
	SkelMesh->SetAllPhysicsLinearVelocity(InheritVelocity);
	if (!Impulse.IsNearlyZero())
	{
		SkelMesh->AddImpulseToAllBodiesBelow(Impulse, PelvisBone, true, true);
	}
}

void AWindupCharacter::ExitRagdollLocal(bool bSnapshot)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	const bool bWasSimulating = SkelMesh->IsSimulatingPhysics(PelvisBone);

	FTransform PelvisWorld = FTransform::Identity;
	if (bSkipGetUpBlend || (bWasSimulating && FVector::Dist(SkelMesh->GetBoneLocation(PelvisBone), GetActorLocation()) > 300.f))
	{
		bSnapshot = false;	// recovered / teleported away from where the ragdoll lies: just stand up
		bSkipGetUpBlend = false;
	}
	if (bSnapshot && bWasSimulating)
	{
		PelvisWorld = SkelMesh->GetSocketTransform(PelvisBone, RTS_World);
		SkelMesh->SnapshotPose(GetUpSnapshot);
	}
	else
	{
		GetUpSnapshot.Reset();
	}

	SkelMesh->SetSimulatePhysics(false);
	SkelMesh->SetAllBodiesSimulatePhysics(false);
	SkelMesh->SetAllUseCCD(false);
	SkelMesh->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	// Base offsets follow crouching: a crawling (crouched) doll keeps its mesh on the floor instead of sinking 24 cm.
	SkelMesh->SetRelativeLocationAndRotation(GetBaseTranslationOffset(), GetBaseRotationOffset());
	SkelMesh->SetRelativeScale3D(MeshDefaultRelative.GetScale3D());
	PhysicalAnimation->SetStrengthMultiplyer(1.f);

	if (GetUpSnapshot.bIsValid)
	{
		// Re-express the pelvis relative to the re-attached component so the blend starts where the ragdoll lay.
		const int32 PelvisIndex = GetUpSnapshot.BoneNames.IndexOfByKey(PelvisBone);
		if (PelvisIndex != INDEX_NONE)
		{
			const FTransform CompWorld = SkelMesh->GetComponentTransform();
			FTransform ParentWorld = CompWorld;
			const USkeletalMesh* Asset = SkelMesh->GetSkeletalMeshAsset();
			if (Asset)
			{
				const int32 ParentIndex = Asset->GetRefSkeleton().GetParentIndex(Asset->GetRefSkeleton().FindBoneIndex(PelvisBone));
				if (ParentIndex != INDEX_NONE && GetUpSnapshot.LocalTransforms.IsValidIndex(ParentIndex))
				{
					ParentWorld = GetUpSnapshot.LocalTransforms[ParentIndex] * CompWorld;
				}
			}
			GetUpSnapshot.LocalTransforms[PelvisIndex] = PelvisWorld.GetRelativeTransform(ParentWorld);
		}
		GetUpStartTime = GetWorld()->GetTimeSeconds();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

float AWindupCharacter::GetGetUpAlpha() const
{
	if (!GetUpSnapshot.bIsValid || !GetWorld())
	{
		return 1.f;
	}
	const float Alpha = (GetWorld()->GetTimeSeconds() - GetUpStartTime) / 0.55f;
	return FMath::Clamp(Alpha, 0.f, 1.f);
}

float AWindupCharacter::GetSquashAlpha() const
{
	if (GetBodyState() != ECrankBodyState::Splat)
	{
		return 0.f;
	}
	return FMath::Clamp(1.f - GetBodyStateTime() / 2.5f, 0.f, 1.f);
}

FVector AWindupCharacter::ComputeStandLocationFromPelvis(FRotator& OutRotation) const
{
	const USkeletalMeshComponent* SkelMesh = GetMesh();
	const FTransform PelvisTM = SkelMesh->GetSocketTransform(PelvisBone, RTS_World);
	const FVector UpLocal = RefPoseAxisLocal(SkelMesh, PelvisBone, FVector::UpVector);
	FVector BodyAxis = PelvisTM.GetRotation().RotateVector(UpLocal);
	FVector Facing = BodyAxis.GetSafeNormal2D();
	if (Facing.IsNearlyZero())
	{
		Facing = GetActorForwardVector();
	}
	OutRotation = FRotator(0.f, Facing.Rotation().Yaw, 0.f);

	FVector Loc = PelvisTM.GetLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrankStand), false, this);
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (GetWorld()->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 40.f), Loc - FVector(0, 0, 250.f), ECC_Visibility, Params))
	{
		Loc.Z = Hit.Location.Z + HalfHeight + 2.f;
	}
	else
	{
		Loc.Z += HalfHeight * 0.5f;
	}
	return Loc;
}

void AWindupCharacter::UpdateRagdollServer(float DeltaSeconds)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh->IsSimulatingPhysics(PelvisBone))
	{
		// No physics asset: fake a short fall.
		if (GetBodyStateTime() > RagdollMinTime)
		{
			SetBodyState(Torque->IsDischarged() ? ECrankBodyState::Normal : ECrankBodyState::GettingUp);
		}
		return;
	}

	// Tunneled through a floor despite CCD: nothing below the pelvis any more -> back to the last floor.
	const FVector PelvisNow = SkelMesh->GetBoneLocation(PelvisBone);
	if (bHasSafeFloor && PelvisNow.Z < LastSafeFloor.Z - 120.f && !HasFloorBelow(PelvisNow, 8000.f))
	{
		RecoverToSafeGround();
		return;
	}

	// Root sync: the capsule follows the pelvis (replicated to everyone).
	FRotator StandRot;
	const FVector StandLoc = ComputeStandLocationFromPelvis(StandRot);
	SetActorLocation(StandLoc, false, nullptr, ETeleportType::None);

	const float PelvisSpeed = SkelMesh->GetPhysicsLinearVelocity(PelvisBone).Size();
	const float T = GetBodyStateTime();
	if (T >= RagdollMinTime && (PelvisSpeed < 70.f || T > RagdollMinTime + 2.5f))
	{
		SetActorLocationAndRotation(StandLoc, StandRot, false, nullptr, ETeleportType::None);
		if (Torque->IsDischarged())
		{
			SetBodyState(ECrankBodyState::Normal);	// crawl away
		}
		else
		{
			Torque->AddTorque(-FMath::Min(CrankBalance::GetUpCost, Torque->GetTorque()));
			SetBodyState(ECrankBodyState::GettingUp);
		}
	}
}

FVector AWindupCharacter::RemoveVelocityIntoSurfaces(const FVector& Velocity) const
{
	FVector V = Velocity;
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrankRagdollStart), false, this);
	const FVector Start = GetActorLocation();
	FHitResult Hit;
	// The floor right under the feet (landing): no downward speed.
	if (V.Z < 0.f && GetWorld()->SweepSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 60.f), FQuat::Identity, ECC_Visibility, Shape, Params))
	{
		V.Z = 0.f;
	}
	// Whatever lies just ahead (wall / slope): keep only the part sliding along it.
	const FVector Ahead = V * 0.1f;
	if (!Ahead.IsNearlyZero(1.f) && GetWorld()->SweepSingleByChannel(Hit, Start, Start + Ahead, FQuat::Identity, ECC_Visibility, Shape, Params))
	{
		const FVector Normal = Hit.bStartPenetrating ? Hit.Normal : Hit.ImpactNormal;
		const float Into = FVector::DotProduct(V, Normal);
		if (Into < 0.f)
		{
			V -= Into * Normal;
		}
	}
	return V;
}

bool AWindupCharacter::HasFloorBelow(const FVector& Point, float Depth) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrankFloorBelow), true, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, Point, Point - FVector(0.f, 0.f, Depth), ECC_Visibility, Params);
}

bool AWindupCharacter::IsBlockedAboveStep(const FVector& Dir) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FVector Loc = GetActorLocation();
	const float FeetZ = Loc.Z - Capsule->GetScaledCapsuleHalfHeight();
	const FVector Start(Loc.X, Loc.Y, FeetZ + GetCharacterMovement()->MaxStepHeight + 4.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrankTripCheck), false, this);
	FCollisionResponseParams Response;
	Capsule->InitSweepCollisionParams(Params, Response);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * (Capsule->GetScaledCapsuleRadius() + 20.f),
		Capsule->GetCollisionObjectType(), Params, Response);
}

void AWindupCharacter::UpdateSafeLocation(float DeltaSeconds)
{
	SafeFloorTimer -= DeltaSeconds;
	if (SafeFloorTimer > 0.f)
	{
		return;
	}
	SafeFloorTimer = 0.25f;
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (GetBodyState() != ECrankBodyState::Normal || CarriedBy || !Move || !Move->IsMovingOnGround() || !Move->CurrentFloor.IsWalkableFloor())
	{
		return;
	}
	LastSafeFloor = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	bHasSafeFloor = true;
}

void AWindupCharacter::RecoverToSafeGround()
{
	if (!HasAuthority())
	{
		return;
	}
	if (!bHasSafeFloor)
	{
		if (ARaidGameMode* GM = GetWorld()->GetAuthGameMode<ARaidGameMode>())
		{
			GM->RespawnDoll(this);
		}
		return;
	}
	UE_LOG(LogCrank, Warning, TEXT("%s fell through the world at %s: back to %s"), *GetName(), *GetActorLocation().ToString(), *LastSafeFloor.ToString());
	EndAllInteractions();
	bSkipGetUpBlend = true;	// do not blend from the pose under the floor
	if (GetBodyState() != ECrankBodyState::Normal)
	{
		SetBodyState(ECrankBodyState::Normal);
	}
	bSkipGetUpBlend = false;
	const FVector Target = LastSafeFloor + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f);
	TeleportTo(Target, FRotator(0.f, GetActorRotation().Yaw, 0.f));
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	MulticastImpactFX(Target, FVector::UpVector, 300.f);
}

void AWindupCharacter::UpdateRagdollClient(float DeltaSeconds)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh->IsSimulatingPhysics(PelvisBone))
	{
		return;
	}
	// Gently pull the local ragdoll toward the server root.
	const FVector Pelvis = SkelMesh->GetBoneLocation(PelvisBone);
	FVector Error = GetActorLocation() - Pelvis;
	Error.Z = 0.f;
	const float ErrorSize = Error.Size();
	if (ErrorSize > 30.f)
	{
		SkelMesh->AddForce(Error * 40.f, PelvisBone, true);
	}
}

void AWindupCharacter::TickSplat(float DeltaSeconds)
{
	if (GetBodyState() != ECrankBodyState::Splat)
	{
		return;
	}
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (GetBodyStateTime() > LaunchComp->SplatStickTime && Move->MovementMode == MOVE_Flying)
	{
		Move->SetMovementMode(MOVE_Falling);
		Move->GravityScale = 0.12f;
		Move->Velocity = FVector(0.f, 0.f, -20.f);
	}
	if (HasAuthority() && GetBodyStateTime() > 6.f)
	{
		StartRagdoll(GetSplatNormal() * 60.f, 0.6f);
	}
}

void AWindupCharacter::ReviveAt(const FVector& Location, bool bHeadInverted)
{
	if (!HasAuthority())
	{
		return;
	}
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	FVector Target = Location + FVector(0.f, 0.f, HalfHeight + 10.f);
	TeleportTo(Target, FRotator(0.f, GetActorRotation().Yaw, 0.f));
	Torque->SetTorque(CrankBalance::ReviveTorque);
	SetBodyState(ECrankBodyState::Normal);
	if (bHeadInverted)
	{
		InvertedUntil = ServerNow(GetWorld()) + CrankBalance::InvertedViewTime;
	}
	CrankSound::PlayAt(this, TEXT("SFX_Revive"), GetActorLocation(), 1.f);
	UCrankFXSubsystem::Spawn(this, ECrankFX::Confetti, GetActorLocation(), FRotator::ZeroRotator, 0.6f);
}

void AWindupCharacter::Freeze()
{
	if (!HasAuthority())
	{
		return;
	}
	EndAllInteractions();
	if (GetBodyState() != ECrankBodyState::Scattered && GetBodyState() != ECrankBodyState::Trapped)
	{
		SetBodyState(ECrankBodyState::Frozen);
	}
}

// ------------------------------------------------------------------------------------------
// Tick

void AWindupCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ECrankBodyState State = GetBodyState();
	if (HasAuthority())
	{
		if (DebugRunUntil > GetWorld()->GetTimeSeconds() && CanMoveByInput())
		{
			AddMovementInput(GetActorForwardVector(), 1.f);
		}
		UpdateStatusServer(DeltaSeconds);
		UpdateSafeLocation(DeltaSeconds);
		if (PendingTripTime >= 0.f && GetWorld()->GetTimeSeconds() - PendingTripTime > 0.04f)
		{
			// Still standing and really stopped by the obstacle (a step-up or a slide keeps most of the speed).
			if (GetBodyState() == ECrankBodyState::Normal && GetVelocity().Size2D() < PendingTripSpeed * 0.45f)
			{
				LastTripTime = GetWorld()->GetTimeSeconds();
				MulticastImpactFX(PendingTripPoint, PendingTripNormal, PendingTripSpeed);
				StartRagdoll(PendingTripNormal * 220.f + FVector(0, 0, 200.f), 1.f);
			}
			PendingTripTime = -1.f;
		}
		PreMoveVelocity = GetVelocity();
		if (State == ECrankBodyState::Ragdoll)
		{
			UpdateRagdollServer(DeltaSeconds);
		}
	}
	else if (State == ECrankBodyState::Ragdoll)
	{
		UpdateRagdollClient(DeltaSeconds);
	}

	if (HasAuthority() || IsLocallyControlled())
	{
		TickSplat(DeltaSeconds);
		UpdateSurface();
	}
	if (IsLocallyControlled())
	{
		UpdateCrawlCrouch();
		UpdateCameraFeedback(DeltaSeconds);
		ReconcileHeldInputs();
	}

	if (EmoteRep.Seq != LastEmoteSeq)
	{
		LastEmoteSeq = EmoteRep.Seq;
		EmoteLocalStart = GetWorld()->GetTimeSeconds();
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateCosmetics(DeltaSeconds);
	}
}

void AWindupCharacter::ReconcileHeldInputs()
{
	// Safety net: a lost release event (focus change, input mode switch...) must never leave a hand closed.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerInput)
	{
		return;
	}
	const bool bLeft = PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_LeftTrigger);
	const bool bRight = PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_RightTrigger);
	const bool bHold = PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Left);
	Grab->ReleaseStaleInputs(bLeft, bRight);
	if (!bHold)
	{
		Wind->SetHoldWindInput(false);
	}
}

void AWindupCharacter::UpdateStatusServer(float DeltaSeconds)
{
	const ECrankBodyState State = GetBodyState();
	if (State == ECrankBodyState::GettingUp && GetBodyStateTime() >= CrankBalance::GetUpTime)
	{
		SetBodyState(ECrankBodyState::Normal);
	}
	if (State == ECrankBodyState::Carried && !IsValid(CarriedBy))
	{
		EndCarried(FVector::ZeroVector);
	}
	if (EmoteRep.EmoteId != 0 && (State != ECrankBodyState::Normal || GetVelocity().Size2D() > 60.f || GetWorld()->GetTimeSeconds() - EmoteLocalStart > 3.f))
	{
		EmoteRep.EmoteId = 0;
	}
	TugCooldown = FMath::Max(0.f, TugCooldown - DeltaSeconds);
}

void AWindupCharacter::UpdateCrawlCrouch()
{
	const bool bShouldCrawl = IsCrawling();
	if (bShouldCrawl && !bIsCrouched)
	{
		Crouch();
	}
	else if (!bShouldCrawl && bIsCrouched && GetBodyState() == ECrankBodyState::Normal)
	{
		UnCrouch();
	}
}

void AWindupCharacter::UpdateSurface()
{
	float Friction = 8.f;
	float Boost = 1.f;
	float Cold = 1.f;
	for (int32 i = ActiveZones.Num() - 1; i >= 0; --i)
	{
		const ASurfaceZone* Zone = ActiveZones[i];
		if (!IsValid(Zone))
		{
			ActiveZones.RemoveAt(i);
			continue;
		}
		Cold = FMath::Max(Cold, Zone->DrainMultiplier);
		Friction = FMath::Min(Friction, Zone->GroundFriction);
		Boost = FMath::Max(Boost, Zone->SpeedBoost);
	}

	for (TActorIterator<AWaterTrail> It(GetWorld()); It; ++It)
	{
		if (It->IsWetAt(GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight())))
		{
			Friction = FMath::Min(Friction, CrankBalance::BossWaterFriction);
		}
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();

	// Phase 3 boss suction pulls dolls in front of the intake unless they hold on to something.
	if (GetBodyState() == ECrankBodyState::Normal && !Grab->IsAnchored() && Move->IsMovingOnGround())
	{
		for (TActorIterator<ABossDustEater> It(GetWorld()); It; ++It)
		{
			const FVector Accel = It->GetSuctionAccel(this);
			if (!Accel.IsNearlyZero())
			{
				const float Along = FVector::DotProduct(Move->Velocity, Accel.GetSafeNormal());
				if (Along < CrankBalance::BossSuctionSpeed)
				{
					Move->AddForce(Accel * Move->Mass);
				}
			}
		}
	}

	Move->GroundFriction = Friction;
	Move->BrakingDecelerationWalking = Friction < 1.f ? 60.f : 1400.f;
	ColdMultiplier = Cold;
	if (!FMath::IsNearlyEqual(Boost, SurfaceSpeedBoost))
	{
		SurfaceSpeedBoost = Boost;
		RefreshMovementSpeed();
	}
}

void AWindupCharacter::RegisterSurfaceZone(ASurfaceZone* Zone, bool bEnter)
{
	if (bEnter)
	{
		ActiveZones.AddUnique(Zone);
	}
	else
	{
		ActiveZones.Remove(Zone);
	}
}

void AWindupCharacter::UpdateCameraFeedback(float DeltaSeconds)
{
	float TargetArm = DefaultArmLength;
	if (Wind->IsPulling())
	{
		TargetArm = DefaultArmLength * 0.72f;
	}
	else if (IsCrawling())
	{
		TargetArm = DefaultArmLength * 0.85f;
	}
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaSeconds, 4.f);

	const float TargetRoll = IsViewInverted() ? 180.f : 0.f;
	CameraRoll = FMath::FInterpTo(CameraRoll, TargetRoll, DeltaSeconds, 3.f);
	FollowCamera->SetRelativeRotation(FRotator(0.f, 0.f, CameraRoll));
}

void AWindupCharacter::UpdateCosmetics(float DeltaSeconds)
{
	const ECrankBodyState State = GetBodyState();
	const bool bVisible = State != ECrankBodyState::Scattered && State != ECrankBodyState::Trapped;
	const float T = Torque->GetTorque();
	const float Speed2D = GetVelocity().Size2D();
	const UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();

	// Key: unwinds while walking, spins when wound, spins freely after a slip.
	if (State == ECrankBodyState::Normal && Speed2D > 20.f && T > 0.f)
	{
		const float Rate = T >= 70.f ? 540.f : (T >= 30.f ? 300.f : 120.f);
		KeyVisualTarget -= Rate * DeltaSeconds;
	}
	if (Now < KeyFreeSpinUntil)
	{
		KeyVisualTarget -= 1800.f * DeltaSeconds;
	}
	KeyVisualAngle = FMath::FInterpTo(KeyVisualAngle, KeyVisualTarget, DeltaSeconds, 14.f);
	if (KeyMesh->GetStaticMesh())
	{
		FVector Axis = -GetActorForwardVector();
		if (bKeyAxisCached && GetMesh()->GetBoneIndex(TEXT("spine")) != INDEX_NONE)
		{
			const FQuat SpineRot = GetMesh()->GetBoneQuaternion(TEXT("spine"), EBoneSpaces::WorldSpace);
			Axis = SpineRot.RotateVector(KeyBackAxisLocal);
		}
		const FQuat Base = FRotationMatrix::MakeFromXZ(Axis, FVector::UpVector).ToQuat();
		const FQuat Spin(FVector::ForwardVector, FMath::DegreesToRadians(KeyVisualAngle));
		KeyMesh->SetWorldRotation(Base * Spin);
	}

	// Overwound: steam + rising warning beeps.
	const bool bOverwound = bVisible && T > CrankBalance::TorqueMax;
	SteamFX->SetEmitting(bOverwound);
	SteamFX->Rate = 6.f + (T - 100.f) * 1.2f;
	if (bOverwound)
	{
		WarnTimer -= DeltaSeconds;
		if (WarnTimer <= 0.f)
		{
			const float Danger = (T - 100.f) / 20.f;
			WarnTimer = FMath::Lerp(0.7f, 0.15f, Danger);
			CrankSound::PlayAt(this, TEXT("SFX_Warn"), GetActorLocation(), 0.35f, 1.f + Danger * 0.8f);
		}
	}

	DustFX->SetEmitting(bVisible && IsDusty() && Speed2D > 30.f);
	StarsFX->SetEmitting(bVisible && IsHelpless());
	FrostFX->SetEmitting(bVisible && IsInColdZone());

	// Clockwork tick: faster when fully wound, silent when discharged.
	if (bVisible && T > 0.f && State != ECrankBodyState::Frozen)
	{
		TickSoundTimer -= DeltaSeconds;
		if (TickSoundTimer <= 0.f)
		{
			const ETorqueBand Band = Torque->GetBand();
			TickSoundTimer = Band >= ETorqueBand::Sprint ? 0.16f : (Band == ETorqueBand::Run ? 0.3f : 0.55f);
			const float Pitch = Band >= ETorqueBand::Sprint ? 1.15f : (Band == ETorqueBand::Run ? 1.f : 0.82f);
			CrankSound::PlayAt(this, TEXT("SFX_Tick"), GetActorLocation(), IsLocallyControlled() ? 0.22f : 0.14f, Pitch * FMath::FRandRange(0.97f, 1.03f));
		}
	}

	// Crawling: arms scrape with an occasional creak.
	if (IsCrawling() && Speed2D > 10.f)
	{
		CreakTimer -= DeltaSeconds;
		if (CreakTimer <= 0.f)
		{
			CreakTimer = FMath::FRandRange(1.4f, 2.6f);
			CrankSound::PlayAt(this, TEXT("SFX_Creak"), GetActorLocation(), 0.45f, FMath::FRandRange(0.8f, 1.1f));
		}
	}

	// Tin footsteps.
	if (State == ECrankBodyState::Normal && GetCharacterMovement()->IsMovingOnGround() && !IsCrawling())
	{
		FootstepDistance += Speed2D * DeltaSeconds;
		const float Stride = Torque->GetBand() >= ETorqueBand::Sprint ? 70.f : 45.f;
		if (FootstepDistance > Stride)
		{
			FootstepDistance = 0.f;
			CrankSound::PlayAt(this, TEXT("SFX_Step"), GetActorLocation() - FVector(0, 0, 48.f), 0.25f, FMath::FRandRange(0.9f, 1.2f));
			if (IsDusty() || IsInColdZone())
			{
				UCrankFXSubsystem::Spawn(this, ECrankFX::Footstep, GetActorLocation() - FVector(0, 0, 48.f));
			}
		}
	}

	// Splat squash: flattened against the wall, slowly recovering.
	if (State == ECrankBodyState::Splat)
	{
		const float Squash = GetSquashAlpha();
		const FVector Base = MeshDefaultRelative.GetScale3D();
		GetMesh()->SetRelativeScale3D(Base * FVector(FMath::Lerp(1.f, 0.35f, Squash), FMath::Lerp(1.f, 1.25f, Squash), FMath::Lerp(1.f, 1.15f, Squash)));
	}

	// Semi-ragdoll on/off as the arms get busy.
	ApplySemiRagdoll();
}

void AWindupCharacter::ApplySemiRagdoll()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh || !SkelMesh->GetPhysicsAsset() || !PhysicalAnimation || GetBodyState() == ECrankBodyState::Ragdoll)
	{
		return;
	}

	const bool bArmsBusy = (Grab && Grab->IsHoldingAnything()) || (Grab && (Grab->WantsGrab(ECrankHand::Left) || Grab->WantsGrab(ECrankHand::Right)))
		|| (Wind && (Wind->IsWinding() || Wind->IsBeingWound()));
	const bool bEnable = bEnableSemiRagdoll && !bArmsBusy && GetBodyState() == ECrankBodyState::Normal && GetActiveEmote() == 0;

	float Strength = 2200.f;
	switch (Torque ? Torque->GetBand() : ETorqueBand::Run)
	{
	case ETorqueBand::Discharged:	Strength = 1200.f; break;
	case ETorqueBand::Toddle:		Strength = 2600.f; break;
	case ETorqueBand::Run:			Strength = 2000.f; break;
	case ETorqueBand::Sprint:		Strength = 900.f; break;
	case ETorqueBand::Overwound:	Strength = 600.f; break;
	}

	// Only touch physics when the configuration actually changes.
	const int32 Signature = bEnable ? FMath::RoundToInt(Strength) : 0;
	if (SemiRagdollSignature == Signature)
	{
		return;
	}
	SemiRagdollSignature = Signature;

	for (const FName& Bone : SemiRagdollBones)
	{
		if (SkelMesh->GetBoneIndex(Bone) == INDEX_NONE)
		{
			continue;
		}
		if (bEnable)
		{
			// World-space targets: the engine guards that path against the pose buffers being swapped out
			// during parallel animation evaluation (local-space targets read bone-space transforms unguarded).
			FPhysicalAnimationData Data;
			Data.bIsLocalSimulation = false;
			Data.OrientationStrength = Strength;
			Data.AngularVelocityStrength = Strength * 0.1f;
			Data.PositionStrength = 0.f;
			Data.VelocityStrength = 0.f;
			PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(Bone, Data, true);
			SkelMesh->SetAllBodiesBelowSimulatePhysics(Bone, true, true);
			SkelMesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, 1.f, false, true);
		}
		else
		{
			SkelMesh->SetAllBodiesBelowSimulatePhysics(Bone, false, true);
			SkelMesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, 0.f, false, true);
		}
	}
}

// ------------------------------------------------------------------------------------------
// Movement hooks

bool AWindupCharacter::CanJumpInternal_Implementation() const
{
	if (GetBodyState() != ECrankBodyState::Normal || IsHelpless() || CarriedBy)
	{
		return false;
	}
	if (!Torque || Torque->GetTorque() < CrankBalance::JumpMinTorque)
	{
		return false;
	}
	if (Wind && (Wind->IsWinding() || Wind->IsBeingWound()))
	{
		return false;
	}
	if (const ACarryableActor* Item = Cast<ACarryableActor>(Grab ? Grab->GetCarriedActor() : nullptr))
	{
		if (Item->GetWeight() == ECarryWeight::TwoPerson || Item->GetWeight() == ECarryWeight::Cloth)
		{
			return false;
		}
	}
	return Super::CanJumpInternal_Implementation();
}

void AWindupCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	if (HasAuthority())
	{
		Torque->AddTorque(-CrankBalance::JumpCost);
	}
	CrankSound::PlayAt(this, TEXT("SFX_Jump"), GetActorLocation(), 0.5f, FMath::FRandRange(0.95f, 1.1f));
}

void AWindupCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	const ECrankBodyState State = GetBodyState();
	if (HasAuthority() && (State == ECrankBodyState::Flying || State == ECrankBodyState::Splat))
	{
		LaunchComp->HandleFlightLanded(Hit);
		return;
	}
	if (State == ECrankBodyState::Normal)
	{
		CrankSound::PlayAt(this, TEXT("SFX_Step"), GetActorLocation(), 0.45f, 0.8f);
		UCrankFXSubsystem::Spawn(this, ECrankFX::Land, Hit.ImpactPoint, FRotator::ZeroRotator, 0.6f);
	}
}

void AWindupCharacter::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	if (!HasAuthority())
	{
		return;
	}

	const ECrankBodyState State = GetBodyState();
	if (State == ECrankBodyState::Flying)
	{
		LaunchComp->HandleFlightHit(Other, OtherComp, Hit);
		return;
	}

	// Sprinting dolls trip (9.4) - but only when they slam head-on into something they cannot step over.
	// Edges below the step height (tile bevels, rug rims, prop bases) are climbed by the movement component anyway:
	// the sweep reports them as hits before stepping up, so they must never trip.
	if (State == ECrankBodyState::Normal && GetCharacterMovement()->IsMovingOnGround() && Torque->GetBand() >= ETorqueBand::Sprint
		&& FMath::Abs(HitNormal.Z) < 0.3f && !Wind->IsWinding() && !CarriedBy && !Cast<AWindupCharacter>(Other)
		&& !(OtherComp && OtherComp->IsSimulatingPhysics()) && PendingTripTime < 0.f)
	{
		// (The impact point of a capsule against a flat wall is the bottom of its straight part, 24 cm up: no use here.)
		// Hits are dispatched after the move: the velocity is already cut by the obstacle, it was hit at the last move's.
		const FVector Vel = PreMoveVelocity.SizeSquared2D() > GetVelocity().SizeSquared2D() ? PreMoveVelocity : GetVelocity();
		const float Speed = Vel.Size2D();
		const float HeadOn = Speed > 1.f ? FVector::DotProduct(Vel.GetSafeNormal2D(), -HitNormal.GetSafeNormal2D()) : 0.f;
		const float Now = GetWorld()->GetTimeSeconds();
		if (Speed > SprintTripSpeed && HeadOn > 0.82f && Now - LastTripTime > 4.f && IsBlockedAboveStep(-HitNormal.GetSafeNormal2D()))
		{
			PendingTripTime = Now;
			PendingTripSpeed = Speed;
			PendingTripNormal = HitNormal;
			PendingTripPoint = Hit.ImpactPoint;
		}
	}
}

void AWindupCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (!HasAuthority())
	{
		return;
	}
	RecoverToSafeGround();
}

void AWindupCharacter::MulticastImpactFX_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal, float Speed)
{
	const float Strength = FMath::Clamp(Speed / 1500.f, 0.3f, 1.5f);
	UCrankFXSubsystem::Spawn(this, ECrankFX::Sparks, Location, FVector(Normal).Rotation(), Strength);
	UCrankFXSubsystem::Spawn(this, ECrankFX::Puff, Location, FRotator::ZeroRotator, Strength);
	CrankSound::PlayAt(this, TEXT("SFX_Bonk"), Location, FMath::Clamp(Strength, 0.4f, 1.f), FMath::FRandRange(0.9f, 1.1f));
}

void AWindupCharacter::MulticastKeyFreeSpin_Implementation()
{
	KeyFreeSpinUntil = GetWorld()->GetTimeSeconds() + 1.1f;
}

// ------------------------------------------------------------------------------------------
// Winding hooks

void AWindupCharacter::OnStartBeingWound(AWindupCharacter* Winder)
{
	if (HasAuthority())
	{
		Grab->ReleaseAll(false);
		EmoteRep.EmoteId = 0;
	}
	ApplySemiRagdoll();
}

void AWindupCharacter::OnStopBeingWound()
{
	ApplySemiRagdoll();
}

void AWindupCharacter::OnStartWinding(AWindupCharacter* Target)
{
	if (HasAuthority())
	{
		EmoteRep.EmoteId = 0;
	}
	ApplySemiRagdoll();
}

void AWindupCharacter::OnStopWinding()
{
	ApplySemiRagdoll();
}

void AWindupCharacter::NotifyKeyTurned()
{
	KeyTurns++;
	OnRep_KeyTurns();
}

void AWindupCharacter::OnRep_KeyTurns()
{
	// The local winder already spins the key from its own input.
	const AWindupCharacter* Winder = Wind ? Wind->GetWoundBy() : nullptr;
	if (Winder && Winder->IsLocallyControlled())
	{
		return;
	}
	if (!Wind || !Wind->GetWoundBy() || !Wind->GetWoundBy()->GetWind()->IsHoldWinding())
	{
		KeyVisualTarget += 360.f;
	}
}

// ------------------------------------------------------------------------------------------
// Partner grabbing / piggyback

bool AWindupCharacter::CanBeGrabbed(const AWindupCharacter* By, ECrankHand Hand, const UPrimitiveComponent* Comp) const
{
	if (!By || By == this || CarriedBy || By->GetCarriedBy())
	{
		return false;
	}
	const ECrankBodyState State = GetBodyState();
	if (State != ECrankBodyState::Normal && State != ECrankBodyState::Ragdoll)
	{
		return false;
	}
	if (By->GetGrab()->GetCarriedActor())
	{
		return false;
	}
	return !(Wind && (Wind->IsWinding() || Wind->IsBeingWound()));
}

FVector AWindupCharacter::GetGrabPoint(const UPrimitiveComponent* Comp, const FVector& HandLocation) const
{
	if (GetBodyState() == ECrankBodyState::Ragdoll)
	{
		return GetMesh()->GetBoneLocation(PelvisBone);
	}
	return GetActorLocation();
}

FText AWindupCharacter::GetGrabLabel(const UPrimitiveComponent* Comp) const
{
	return NSLOCTEXT("Crank", "GrabFriend", "친구 잡기 (양손: 업기)");
}

void AWindupCharacter::OnReleased(AWindupCharacter* By, ECrankHand Hand, UPrimitiveComponent* Comp, const FVector& ThrowVelocity)
{
	if (CarriedBy == By)
	{
		EndCarried(ThrowVelocity);
	}
}

AWindupCharacter* AWindupCharacter::GetCarriedFriend() const
{
	if (!Grab)
	{
		return nullptr;
	}
	for (const ECrankHand Hand : { ECrankHand::Left, ECrankHand::Right })
	{
		const FCrankHandState& H = Grab->GetHand(Hand);
		if (H.Style == EGrabStyle::Friend)
		{
			AWindupCharacter* Friend = Cast<AWindupCharacter>(H.Actor);
			if (Friend && Friend->CarriedBy == this)
			{
				return Friend;
			}
		}
	}
	return nullptr;
}

void AWindupCharacter::UpdateFriendGrab()
{
	const FCrankHandState& L = Grab->GetHand(ECrankHand::Left);
	const FCrankHandState& R = Grab->GetHand(ECrankHand::Right);
	AWindupCharacter* LeftFriend = L.Style == EGrabStyle::Friend ? Cast<AWindupCharacter>(L.Actor) : nullptr;
	AWindupCharacter* RightFriend = R.Style == EGrabStyle::Friend ? Cast<AWindupCharacter>(R.Actor) : nullptr;

	if (LeftFriend && LeftFriend == RightFriend)
	{
		if (!LeftFriend->CarriedBy)
		{
			LeftFriend->BeginCarriedBy(this);
		}
		return;
	}

	// One hand: tug the partner along.
	AWindupCharacter* Friend = LeftFriend ? LeftFriend : RightFriend;
	if (Friend && !Friend->CarriedBy && TugCooldown <= 0.f)
	{
		const FVector ToMe = GetActorLocation() - Friend->GetActorLocation();
		if (ToMe.Size2D() > 110.f)
		{
			TugCooldown = 0.35f;
			if (Friend->GetBodyState() == ECrankBodyState::Normal)
			{
				Friend->LaunchCharacter(ToMe.GetSafeNormal2D() * 230.f + FVector(0, 0, 60.f), true, false);
			}
			else if (Friend->GetBodyState() == ECrankBodyState::Ragdoll)
			{
				Friend->GetMesh()->AddImpulseToAllBodiesBelow(ToMe.GetSafeNormal2D() * 150.f, Friend->PelvisBone, true, true);
			}
		}
	}
}

void AWindupCharacter::BeginCarriedBy(AWindupCharacter* Carrier)
{
	if (!HasAuthority() || CarriedBy || !Carrier)
	{
		return;
	}
	if (GetBodyState() == ECrankBodyState::Ragdoll)
	{
		// Pick up a fallen partner: skip the get-up.
		SetBodyState(ECrankBodyState::Normal);
	}
	Grab->ReleaseAll(false);
	if (Wind->IsWinding())
	{
		Wind->StopWinding();
	}
	EmoteRep.EmoteId = 0;

	CarriedBy = Carrier;
	SetBodyState(ECrankBodyState::Carried);
	AttachToComponent(Carrier->GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(-6.f, 0.f, 64.f));
	SetActorRelativeRotation(FRotator(0.f, 90.f, 0.f));
	Carrier->RefreshMovementSpeed();
	CrankSound::PlayAt(this, TEXT("SFX_Pickup"), GetActorLocation(), 0.9f, 0.8f);
}

void AWindupCharacter::OnRep_CarriedBy()
{
	if (AWindupCharacter* Carrier = CarriedBy)
	{
		Carrier->RefreshMovementSpeed();
	}
	for (TActorIterator<AWindupCharacter> It(GetWorld()); It; ++It)
	{
		It->RefreshMovementSpeed();
	}
}

void AWindupCharacter::EndCarried(const FVector& ThrowVelocity)
{
	if (!HasAuthority() || !CarriedBy)
	{
		return;
	}
	AWindupCharacter* Carrier = CarriedBy;
	CarriedBy = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Carrier->GetGrab()->ReleaseActor(this);

	const FVector Drop = Carrier->GetActorLocation() + Carrier->GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 25.f);
	TeleportTo(Drop, FRotator(0.f, Carrier->GetActorRotation().Yaw, 0.f));
	SetBodyState(ECrankBodyState::Normal);

	if (!ThrowVelocity.IsNearlyZero())
	{
		LaunchComp->Launch(ThrowVelocity * 1.15f, Carrier, false, false);
		CrankSound::PlayAt(this, TEXT("SFX_Throw"), GetActorLocation(), 1.f);
	}
	Carrier->RefreshMovementSpeed();
	RefreshMovementSpeed();
}

void AWindupCharacter::EndAllInteractions()
{
	if (!HasAuthority())
	{
		return;
	}
	if (AWindupCharacter* Friend = GetCarriedFriend())
	{
		Friend->EndCarried(FVector::ZeroVector);
	}
	Grab->ReleaseAll(false);
	if (Wind->IsWinding())
	{
		Wind->StopWinding();
	}
	if (AWindupCharacter* Winder = Wind->GetWoundBy())
	{
		Winder->GetWind()->StopWinding();
	}
	if (CarriedBy)
	{
		AWindupCharacter* Carrier = CarriedBy;
		EndCarried(FVector::ZeroVector);
		Carrier->GetGrab()->ReleaseActor(this);
	}
	EmoteRep.EmoteId = 0;
}

// ------------------------------------------------------------------------------------------
// Status effects / dust bin

void AWindupCharacter::ApplyDusty(float Duration)
{
	DustyUntil = ServerNow(GetWorld()) + Duration;
}

void AWindupCharacter::ApplyHelpless(float Duration)
{
	HelplessUntil = ServerNow(GetWorld()) + Duration;
}

void AWindupCharacter::EnterDustBin(AActor* Boss)
{
	if (!HasAuthority() || !Boss)
	{
		return;
	}
	EndAllInteractions();
	TrappedIn = Boss;
	Torque->bSuspendMovementDrain = true;
	SetBodyState(ECrankBodyState::Trapped);
	AttachToActor(Boss, FAttachmentTransformRules::KeepWorldTransform);
	SetActorRelativeLocation(FVector(-40.f, 0.f, 20.f));
}

void AWindupCharacter::ExitDustBin(const FVector& EjectLocation, const FVector& EjectVelocity, bool bAutoEjected)
{
	if (!HasAuthority() || GetBodyState() != ECrankBodyState::Trapped)
	{
		return;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	TrappedIn.Reset();
	Torque->bSuspendMovementDrain = false;
	TeleportTo(EjectLocation, FRotator(0.f, EjectVelocity.Rotation().Yaw, 0.f));
	SetBodyState(ECrankBodyState::Normal);

	UCrankFXSubsystem::Spawn(this, ECrankFX::DustBurst, EjectLocation);
	if (bAutoEjected)
	{
		ApplyDusty(CrankBalance::BossDustyTime);
		ApplyHelpless(CrankBalance::BossEjectHelplessTime);
		StartRagdoll(EjectVelocity * 0.6f, CrankBalance::BossEjectHelplessTime * 0.5f);
	}
	else
	{
		ApplyDusty(CrankBalance::BossDustyTime * 0.5f);
		LaunchComp->Launch(EjectVelocity, nullptr, false, false);
	}
}
