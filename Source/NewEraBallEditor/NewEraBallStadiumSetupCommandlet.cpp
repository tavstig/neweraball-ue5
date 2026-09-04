// NewEraBallEditor - Stadium setup commandlet implementation

#include "NewEraBallStadiumSetupCommandlet.h"
#include "NewEraBallEditor.h"
#include "NewEraBallStadium.h"
#include "NewEraBallCamera.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

UNewEraBallStadiumSetupCommandlet::UNewEraBallStadiumSetupCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
	HelpDescription = TEXT("Creates the NewEraBall stadium materials and populates NewEraBall_Main.umap with the stadium, camera and lighting.");
}

UMaterial* UNewEraBallStadiumSetupCommandlet::CreateSimpleMaterial(const FString& PackagePath, const FString& AssetName, const FLinearColor& BaseColor, float Roughness, float Metallic, const FLinearColor* EmissiveColor)
{
	const FString LongPackageName = PackagePath / AssetName;

	UPackage* Package = CreatePackage(*LongPackageName);
	if (!Package)
	{
		UE_LOG(LogNewEraBallEditor, Error, TEXT("Could not create package %s"), *LongPackageName);
		return nullptr;
	}
	Package->FullyLoad();

	UMaterial* Material = NewObject<UMaterial>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();

	UMaterialExpressionConstant3Vector* BaseColorExpr = NewObject<UMaterialExpressionConstant3Vector>(Material);
	BaseColorExpr->Constant = BaseColor;
	Material->GetExpressionCollection().AddExpression(BaseColorExpr);
	EditorOnly->BaseColor.Connect(0, BaseColorExpr);

	UMaterialExpressionConstant* RoughnessExpr = NewObject<UMaterialExpressionConstant>(Material);
	RoughnessExpr->R = Roughness;
	Material->GetExpressionCollection().AddExpression(RoughnessExpr);
	EditorOnly->Roughness.Connect(0, RoughnessExpr);

	UMaterialExpressionConstant* MetallicExpr = NewObject<UMaterialExpressionConstant>(Material);
	MetallicExpr->R = Metallic;
	Material->GetExpressionCollection().AddExpression(MetallicExpr);
	EditorOnly->Metallic.Connect(0, MetallicExpr);

	if (EmissiveColor)
	{
		UMaterialExpressionConstant3Vector* EmissiveExpr = NewObject<UMaterialExpressionConstant3Vector>(Material);
		EmissiveExpr->Constant = *EmissiveColor;
		Material->GetExpressionCollection().AddExpression(EmissiveExpr);
		EditorOnly->EmissiveColor.Connect(0, EmissiveExpr);
	}

	Material->PreEditChange(nullptr);
	Material->PostEditChange();

	FAssetRegistryModule::AssetCreated(Material);
	Package->MarkPackageDirty();

	if (!SavePackageToDisk(Package, Material, LongPackageName))
	{
		UE_LOG(LogNewEraBallEditor, Error, TEXT("Failed to save material package %s"), *LongPackageName);
		return nullptr;
	}

	UE_LOG(LogNewEraBallEditor, Display, TEXT("Created material %s"), *LongPackageName);
	return Material;
}

bool UNewEraBallStadiumSetupCommandlet::SavePackageToDisk(UPackage* Package, UObject* Asset, const FString& LongPackageName)
{
	const bool bIsMap = Asset && Asset->IsA<UWorld>();
	const FString Extension = bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
	const FString Filename = FPackageName::LongPackageNameToFilename(LongPackageName, Extension);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.bSlowTask = false;

	return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
}

int32 UNewEraBallStadiumSetupCommandlet::Main(const FString& Params)
{
	UE_LOG(LogNewEraBallEditor, Display, TEXT("NewEraBallStadiumSetup: starting."));

	const FString MaterialsPath = TEXT("/Game/NewEraBall/Materials");

	UMaterial* GrassMat = CreateSimpleMaterial(MaterialsPath, TEXT("M_PitchGrass"), FLinearColor(0.035f, 0.16f, 0.03f), 0.85f, 0.0f);
	UMaterial* LineMat = CreateSimpleMaterial(MaterialsPath, TEXT("M_PitchLineMarking"), FLinearColor(0.9f, 0.9f, 0.9f), 0.4f, 0.0f);
	UMaterial* GoalFrameMat = CreateSimpleMaterial(MaterialsPath, TEXT("M_GoalFrame"), FLinearColor(0.95f, 0.95f, 0.95f), 0.3f, 0.1f);
	UMaterial* StandMat = CreateSimpleMaterial(MaterialsPath, TEXT("M_StadiumStand"), FLinearColor(0.06f, 0.06f, 0.07f), 0.75f, 0.0f);

	const FLinearColor FasciaAccent(0.75f, 0.05f, 0.08f);
	UMaterial* FasciaMat = CreateSimpleMaterial(MaterialsPath, TEXT("M_StadiumFascia"), FasciaAccent, 0.5f, 0.0f, &FasciaAccent);

	const FLinearColor LightRigGlow(3.0f, 2.9f, 2.6f);
	UMaterial* LightRigMat = CreateSimpleMaterial(MaterialsPath, TEXT("M_StadiumLightRig"), FLinearColor(0.02f, 0.02f, 0.02f), 0.4f, 0.2f, &LightRigGlow);

	if (!GrassMat || !LineMat || !GoalFrameMat || !StandMat || !FasciaMat || !LightRigMat)
	{
		UE_LOG(LogNewEraBallEditor, Error, TEXT("NewEraBallStadiumSetup: one or more materials failed to create. Aborting."));
		return 1;
	}

	const FString MapPackageName = TEXT("/Game/NewEraBall_Main");
	const FString MapFilename = FPackageName::LongPackageNameToFilename(MapPackageName, FPackageName::GetMapPackageExtension());

	if (!FEditorFileUtils::LoadMap(MapFilename, false, true))
	{
		UE_LOG(LogNewEraBallEditor, Error, TEXT("NewEraBallStadiumSetup: failed to load %s"), *MapFilename);
		return 1;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogNewEraBallEditor, Error, TEXT("NewEraBallStadiumSetup: no editor world available after loading %s"), *MapFilename);
		return 1;
	}

	// Idempotent re-run: replace any stadium/camera/lighting actors a previous run of this
	// commandlet already placed, rather than piling up duplicates.
	{
		TArray<AActor*> ToRemove;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && (Actor->IsA<ANewEraBallStadium>() || Actor->IsA<ANewEraBallCamera>()
				|| Actor->IsA<ADirectionalLight>() || Actor->IsA<ASkyLight>() || Actor->IsA<ASkyAtmosphere>()))
			{
				ToRemove.Add(Actor);
			}
		}
		for (AActor* Actor : ToRemove)
		{
			World->DestroyActor(Actor);
		}
	}

	// Stadium shell and pitch.
	ANewEraBallStadium* Stadium = World->SpawnActor<ANewEraBallStadium>(ANewEraBallStadium::StaticClass(), FTransform::Identity);
	if (Stadium)
	{
		Stadium->GrassMaterial = GrassMat;
		Stadium->LineMaterial = LineMat;
		Stadium->GoalFrameMaterial = GoalFrameMat;
		Stadium->StandMaterial = StandMat;
		Stadium->StandFasciaMaterial = FasciaMat;
		Stadium->LightRigMaterial = LightRigMat;
		Stadium->RebuildStadium();
	}
	else
	{
		UE_LOG(LogNewEraBallEditor, Error, TEXT("NewEraBallStadiumSetup: failed to spawn ANewEraBallStadium."));
	}

	// Fixed broadcast camera: elevated, behind and above one touchline, framing the full pitch.
	ANewEraBallCamera* Camera = World->SpawnActor<ANewEraBallCamera>(ANewEraBallCamera::StaticClass(), FTransform::Identity);
	if (Camera)
	{
		Camera->SidelineDistance = 3800.0f;
		Camera->MountHeight = 2400.0f;
		Camera->PlaceFromPitch();
		Camera->SnapToTarget();
	}
	else
	{
		UE_LOG(LogNewEraBallEditor, Error, TEXT("NewEraBallStadiumSetup: failed to spawn ANewEraBallCamera."));
	}

	// Lighting: a bright, warm directional light stands in for the stadium floodlights and drives the
	// sky atmosphere's sun; a low elevation gives the evening/night broadcast look the brief asks for.
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-8.0f, 45.0f, 0.0f), FVector(0.0f, 0.0f, 3000.0f)));
	if (Sun)
	{
		if (UDirectionalLightComponent* Comp = Sun->GetComponent())
		{
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->Intensity = 9.0f;
			Comp->SetLightColor(FLinearColor(1.0f, 0.92f, 0.78f));
			Comp->bAtmosphereSunLight = true;
		}
	}

	// Outdoor stadium sky; ties its scattering to the directional light marked as the atmosphere sun above.
	World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity);

	// Ambient fill so pitch shadows never go fully black; real-time captured so no lighting build is needed.
	ASkyLight* Fill = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform(FVector(0.0f, 0.0f, 2000.0f)));
	if (Fill)
	{
		if (USkyLightComponent* Comp = Fill->GetLightComponent())
		{
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->bRealTimeCapture = true;
			Comp->Intensity = 0.6f;
			Comp->RecaptureSky();
		}
	}

	World->MarkPackageDirty();
	if (!SavePackageToDisk(World->GetOutermost(), World, MapPackageName))
	{
		UE_LOG(LogNewEraBallEditor, Error, TEXT("NewEraBallStadiumSetup: failed to save %s"), *MapPackageName);
		return 1;
	}

	UE_LOG(LogNewEraBallEditor, Display, TEXT("NewEraBallStadiumSetup: complete. Saved 6 materials under %s and updated %s."), *MaterialsPath, *MapPackageName);
	return 0;
}
