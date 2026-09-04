// NewEraBallEditor - Stadium setup commandlet
// Run unattended with:
//   UnrealEditor-Cmd.exe "<Project>.uproject" -run=NewEraBallStadiumSetup -unattended -nosplash -nopause -log
// Creates the pitch/stand/light-rig materials under Content/NewEraBall/Materials, then populates
// NewEraBall_Main.umap with an ANewEraBallStadium, an ANewEraBallCamera and stadium lighting
// (a directional "floodlight" sun, a SkyAtmosphere, and a real-time-captured SkyLight fill),
// saving both the materials and the level for real.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "NewEraBallStadiumSetupCommandlet.generated.h"

class UMaterial;
struct FLinearColor;

UCLASS()
class UNewEraBallStadiumSetupCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	virtual int32 Main(const FString& Params) override;

private:
	/** Build and save a minimal opaque material: a flat base colour, roughness and metallic, with an optional emissive tint. */
	UMaterial* CreateSimpleMaterial(const FString& PackagePath, const FString& AssetName, const FLinearColor& BaseColor, float Roughness, float Metallic, const FLinearColor* EmissiveColor = nullptr);

	/** Save a package to its on-disk location under Content/. */
	bool SavePackageToDisk(UPackage* Package, UObject* Asset, const FString& LongPackageName);
};
