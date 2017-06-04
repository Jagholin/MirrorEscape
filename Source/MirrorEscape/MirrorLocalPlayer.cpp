// Fill out your copyright notice in the Description page of Project Settings.

#include "MirrorLocalPlayer.h"
#include "SceneView.h"
#include "Stats.h"
#include "Camera/CameraTypes.h"
#include "Engine/Player.h"
#include "AssertionMacros.h"
#include "SceneViewExtension.h"

DEFINE_LOG_CATEGORY(MirrorLocalPlayer);

/*void UMirrorLocalPlayer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UE_LOG(MirrorLocalPlayer, Log, TEXT("AddReferencedObjects called"));
	UMirrorLocalPlayer* This = CastChecked<UMirrorLocalPlayer>(InThis);

	auto *Ref = This->mViewState.GetReference();
	if (Ref)
	{
		Ref->AddReferencedObjects(Collector);
	}

	ULocalPlayer::AddReferencedObjects(This, Collector);
}

bool UMirrorLocalPlayer::CalcSceneViewInitOptions(
	struct FSceneViewInitOptions& ViewInitOptions,
	FViewport* Viewport,
	class FViewElementDrawer* ViewDrawer,
	EStereoscopicPass StereoPass)
{
	UE_LOG(MirrorLocalPlayer, Log, TEXT("In CalcSceneViewInitOptions"));
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_CalcSceneViewInitOptions);
	if ((PlayerController == NULL) || (Size.X <= 0.f) || (Size.Y <= 0.f) || (Viewport == NULL))
	{
		return false;
	}
	// get the projection data
	if (GetProjectionData(Viewport, StereoPass,  ViewInitOptions) == false)
	{
		// Return NULL if this we didn't get back the info we needed
		return false;
	}

	// return if we have an invalid view rect
	if (!ViewInitOptions.IsValidViewRectangle())
	{
		return false;
	}

	if (PlayerController->PlayerCameraManager != NULL)
	{
		// Apply screen fade effect to screen.
		if (PlayerController->PlayerCameraManager->bEnableFading)
		{
			ViewInitOptions.OverlayColor = PlayerController->PlayerCameraManager->FadeColor;
			ViewInitOptions.OverlayColor.A = FMath::Clamp(PlayerController->PlayerCameraManager->FadeAmount, 0.0f, 1.0f);
		}

		// Do color scaling if desired.
		if (PlayerController->PlayerCameraManager->bEnableColorScaling)
		{
			ViewInitOptions.ColorScale = FLinearColor(
				PlayerController->PlayerCameraManager->ColorScale.X,
				PlayerController->PlayerCameraManager->ColorScale.Y,
				PlayerController->PlayerCameraManager->ColorScale.Z
			);
		}

		// Was there a camera cut this frame?
		ViewInitOptions.bInCameraCut = PlayerController->PlayerCameraManager->bGameCameraCutThisFrame;
	}

	check(PlayerController && PlayerController->GetWorld());
	switch (StereoPass)
	{
	case eSSP_FULL:
	case eSSP_LEFT_EYE:
		ViewInitOptions.SceneViewStateInterface = mViewState.GetReference();
		break;

	case eSSP_RIGHT_EYE:
	case eSSP_MONOSCOPIC_EYE:
		throw std::exception("These options are not implemented");
		break;
	}
	ViewInitOptions.ViewActor = PlayerController->GetViewTarget();
	ViewInitOptions.PlayerIndex = GetControllerId();
	ViewInitOptions.ViewElementDrawer = ViewDrawer;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.LODDistanceFactor = PlayerController->LocalPlayerCachedLODDistanceFactor;
	ViewInitOptions.StereoPass = StereoPass;
	ViewInitOptions.WorldToMetersScale = PlayerController->GetWorldSettings()->WorldToMeters;
	ViewInitOptions.CursorPos = Viewport->HasMouseCapture() ? FIntPoint(-1, -1) : FIntPoint(Viewport->GetMouseX(), Viewport->GetMouseY());
	ViewInitOptions.OriginOffsetThisFrame = PlayerController->GetWorld()->OriginOffsetThisFrame;

	return true;
}

static void SetupMonoParameters(FSceneViewFamily& ViewFamily, const FSceneView& MonoView)
{
	// Compute the NDC depths for the far field clip plane. This assumes symmetric projection.
	const FMatrix& LeftEyeProjection = ViewFamily.Views[0]->ViewMatrices.GetProjectionMatrix();

	// Start with a point on the far field clip plane in eye space. The mono view uses a point slightly biased towards the camera to ensure there's overlap.
	const FVector4 StereoDepthCullingPointEyeSpace(0.0f, 0.0f, ViewFamily.MonoParameters.CullingDistance, 1.0f);
	const FVector4 FarFieldDepthCullingPointEyeSpace(0.0f, 0.0f, ViewFamily.MonoParameters.CullingDistance - ViewFamily.MonoParameters.OverlapDistance, 1.0f);

	// Project into clip space
	const FVector4 ProjectedStereoDepthCullingPointClipSpace = LeftEyeProjection.TransformFVector4(StereoDepthCullingPointEyeSpace);
	const FVector4 ProjectedFarFieldDepthCullingPointClipSpace = LeftEyeProjection.TransformFVector4(FarFieldDepthCullingPointEyeSpace);

	// Perspective divide for NDC space
	ViewFamily.MonoParameters.StereoDepthClip = ProjectedStereoDepthCullingPointClipSpace.Z / ProjectedStereoDepthCullingPointClipSpace.W;
	ViewFamily.MonoParameters.MonoDepthClip = ProjectedFarFieldDepthCullingPointClipSpace.Z / ProjectedFarFieldDepthCullingPointClipSpace.W;

	// We need to determine the stereo disparity difference between the center mono view and an offset stereo view so we can account for it when compositing.
	// We take a point on a stereo view far field clip plane, unproject it, then reproject it using the mono view. The stereo disparity offset is then
	// the difference between the original test point and the reprojected point.
	const FVector4 ProjectedPointAtLimit(0.0f, 0.0f, ViewFamily.MonoParameters.MonoDepthClip, 1.0f);
	const FVector4 WorldProjectedPoint = ViewFamily.Views[0]->ViewMatrices.GetInvViewProjectionMatrix().TransformFVector4(ProjectedPointAtLimit);
	FVector4 MonoProjectedPoint = MonoView.ViewMatrices.GetViewProjectionMatrix().TransformFVector4(WorldProjectedPoint / WorldProjectedPoint.W);
	MonoProjectedPoint = MonoProjectedPoint / MonoProjectedPoint.W;
	ViewFamily.MonoParameters.LateralOffset = (MonoProjectedPoint.X - ProjectedPointAtLimit.X) / 2.0f;
}

FSceneView* UMirrorLocalPlayer::CalcSceneView(class FSceneViewFamily* ViewFamily, FVector& OutViewLocation, FRotator& OutViewRotation, FViewport* Viewport, class FViewElementDrawer* ViewDrawer , EStereoscopicPass StereoPass )
{
	//SCOPE_CYCLE_COUNTER(STAT_CalcSceneView);

	FSceneViewInitOptions ViewInitOptions;

	if (!CalcSceneViewInitOptions(ViewInitOptions, Viewport, ViewDrawer, StereoPass))
	{
		return nullptr;
	}

	// Get the viewpoint...technically doing this twice
	// but it makes GetProjectionData better
	FMinimalViewInfo ViewInfo;
	GetViewPoint(ViewInfo, StereoPass);
	OutViewLocation = ViewInfo.Location;
	OutViewRotation = ViewInfo.Rotation;
	ViewInitOptions.bUseFieldOfViewForLOD = ViewInfo.bUseFieldOfViewForLOD;

	// Fill out the rest of the view init options
	ViewInitOptions.ViewFamily = ViewFamily;

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_BuildHiddenComponentList);
		PlayerController->BuildHiddenComponentList(OutViewLocation,  ViewInitOptions.HiddenPrimitives);
	}

	//@TODO: SPLITSCREEN: This call will have an issue with splitscreen, as the show flags are shared across the view family
	EngineShowFlagOrthographicOverride(ViewInitOptions.IsPerspectiveProjection(), ViewFamily->EngineShowFlags);

	FSceneView* const View = new FSceneView(ViewInitOptions);

	View->ViewLocation = OutViewLocation;
	View->ViewRotation = OutViewRotation;

	ViewFamily->Views.Add(View);

	{
		View->StartFinalPostprocessSettings(OutViewLocation);

		// CameraAnim override
		if (PlayerController->PlayerCameraManager)
		{
			TArray<FPostProcessSettings> const* CameraAnimPPSettings;
			TArray<float> const* CameraAnimPPBlendWeights;
			PlayerController->PlayerCameraManager->GetCachedPostProcessBlends(CameraAnimPPSettings, CameraAnimPPBlendWeights);

			for (int32 PPIdx = 0; PPIdx < CameraAnimPPBlendWeights->Num(); ++PPIdx)
			{
				View->OverridePostProcessSettings((*CameraAnimPPSettings)[PPIdx], (*CameraAnimPPBlendWeights)[PPIdx]);
			}
		}

		//	CAMERA OVERRIDE
		//	NOTE: Matinee works through this channel
		View->OverridePostProcessSettings(ViewInfo.PostProcessSettings, ViewInfo.PostProcessBlendWeight);

		View->EndFinalPostprocessSettings(ViewInitOptions);
	}

	for (int ViewExt = 0; ViewExt < ViewFamily->ViewExtensions.Num(); ViewExt++)
	{
		ViewFamily->ViewExtensions[ViewExt]->SetupView(*ViewFamily, *View);
	}

	// Monoscopic far field setup
	if (ViewFamily->IsMonoscopicFarFieldEnabled() && StereoPass == eSSP_MONOSCOPIC_EYE)
	{
		SetupMonoParameters(*ViewFamily, *View);
	}

	return View;
}

void UMirrorLocalPlayer::PostInitProperties()
{
	Super::PostInitProperties();

	if (!IsTemplate())
	{
		mViewState.Allocate();
	}
}

void UMirrorLocalPlayer::FinishDestroy()
{
	Super::FinishDestroy();

	if (!IsTemplate())
	{
		mViewState.Destroy();
	}
}*/

bool UMirrorLocalPlayer::GetProjectionData(FViewport* Viewport, EStereoscopicPass StereoPass, FSceneViewProjectionData& ProjectionData) const
{
	bool result = Super::GetProjectionData(Viewport, StereoPass, ProjectionData);

	if (result)
	{
		for (unsigned int i = 0; i < 4; ++i)
			ProjectionData.ViewRotationMatrix.M[i][0] *= -1;
		UE_LOG(MirrorLocalPlayer, Log, TEXT("GetProjectionData executed OK"));
	}

	return result;
}
