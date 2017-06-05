// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MirrorPlayerState.generated.h"

/**
 * Holds all gameplay data about player character 
 */
UCLASS()
class MIRRORESCAPE_API AMirrorPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite)
	bool bMirrorModeActive = false;
};
