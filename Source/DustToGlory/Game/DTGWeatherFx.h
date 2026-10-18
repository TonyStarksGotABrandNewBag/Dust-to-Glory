// Rain you can see.
//
// There is no particle asset in this repository, so rain is geometry like everything else: three boxes of
// streaks built once at startup, kept around the camera, and scrolled downward so they wrap out of phase
// with each other. Rain is statistically the same everywhere, which is what makes the wrap invisible -- and
// what makes three layers at different speeds and sizes read as depth rather than as three boxes.
//
// It decides nothing. The simulation's weather says whether it is raining and how hard, the same weather
// that is already taking grip away from the tires, so what the player sees and what the car does agree.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DTGWeatherFx.generated.h"

class ADTGSky;
class UDTGGameSubsystem;
class UDTGWeatherAudio;
class UProceduralMeshComponent;

UCLASS()
class DUSTTOGLORY_API ADTGWeatherFx : public AActor
{
	GENERATED_BODY()

public:
	ADTGWeatherFx();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 0 in the dry, 1 in a storm. What the rain is actually drawn at, after it has faded in. */
	double Intensity() const { return Fall; }

	/** How many streaks each layer holds. More is denser rain and more triangles. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Weather")
	int32 StreaksPerLayer = 700;

	/** The box each layer fills, in metres. The camera sits in the middle of it. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Weather")
	float CellWidthM = 44.0f;

	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Weather")
	float CellHeightM = 26.0f;

private:
	static constexpr int32 LayerCount = 3;

	/** Builds one layer's streaks into its component. Scale fattens and lengthens them for the near layer. */
	void BuildLayer(int32 Index, double Scale, const FLinearColor& Colour);
	/** What the current weather asks for: how hard it falls and how hard the wind blows. */
	void ReadWeather(float DeltaSeconds);
	/** Keeps the sound in step with what is being drawn, and claps when the sky flashes. */
	void UpdateSound();

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> Layers;

	UPROPERTY()
	TObjectPtr<UDTGGameSubsystem> Game;

	UPROPERTY()
	TObjectPtr<UDTGWeatherAudio> Sound;

	UPROPERTY()
	TObjectPtr<ADTGSky> Sky;

	/** How far down each layer has scrolled, in Unreal units, wrapping at the cell height. */
	TArray<double> Scroll;
	TArray<double> Speeds;

	FVector LastEye = FVector::ZeroVector;
	double Fall = 0.0;        // 0..1, eased toward what the weather asks for
	double Wind = 0.0;        // crosswind, metres per second
	double LastFlash = 0.0;   // so one flash makes one clap
	bool bBuilt = false;
	bool bHasLastEye = false;
	bool bIndoors = false;
};
