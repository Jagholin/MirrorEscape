// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "MirrorLocalPlayer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(MirrorLocalPlayer, Log, All);

/**
 * Local player that implements special modifications to the
 * projection matrix of the 3d viewport to achieve
 * mirroring effect
 */
UCLASS()
class MIRRORESCAPE_API UMirrorLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

private:
	FSceneViewStateReference mViewState;
	
public:
	//static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	//bool CalcSceneViewInitOptions(struct FSceneViewInitOptions& ViewInitOptions, FViewport* Viewport, class FViewElementDrawer* ViewDrawer, EStereoscopicPass StereoPass);
	//virtual FSceneView* CalcSceneView(class FSceneViewFamily* ViewFamily, FVector& OutViewLocation, FRotator& OutViewRotation, FViewport* Viewport, class FViewElementDrawer* ViewDrawer = NULL, EStereoscopicPass StereoPass = eSSP_FULL) override;

	//virtual void PostInitProperties() override;
	//virtual void FinishDestroy() override;

	virtual bool GetProjectionData(FViewport* Viewport, EStereoscopicPass StereoPass, FSceneViewProjectionData& ProjectionData) const override;

};
