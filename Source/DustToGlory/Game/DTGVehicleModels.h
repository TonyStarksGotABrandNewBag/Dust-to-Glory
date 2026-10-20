// Real bodies for the simulation's cars: Epic's City Sample vehicles, when that pack has been added to
// the project through the editor's Fab window (docs/HANDOFF.md says how; it is gigabytes and not
// committed). The simulation still decides every car's size, weight and handling; this only picks which
// of the pack's bodies dresses a body style and hands over its pieces. Without the pack, cars stay the
// generated boxes.
//
// The pack's skeletal meshes are empty armatures for Chaos; the art is Nanite static meshes, all
// authored in one space: the body without wheels, the glass, and each wheel in place with its pivot at
// the car's origin. So a kit is a set of static meshes plus where each wheel's centre is.
#pragma once

#include "CoreMinimal.h"

#include "Sim/Vehicles.h"

class UMaterialInterface;
class UStaticMesh;

namespace DTGVehicleModels
{
	/** One wheel of a kit, authored in place: its centre and radius in the model's space, in centimetres. */
	struct FWheel
	{
		UStaticMesh* Mesh = nullptr;
		bool bFront = false;
		bool bLeft = false;
		/** An axle behind the rear one on a truck or bus; it rides along with the rear wheels. */
		bool bExtra = false;
		FVector Centre = FVector::ZeroVector;
		double Radius = 0.0;
	};

	/** A City Sample body: the shell, its glass (may be null), and its wheels, at least one front and one rear. */
	struct FKit
	{
		FString Name;
		UStaticMesh* Body = nullptr;
		UStaticMesh* Glass = nullptr;
		/** A paint the game can colour (Tools/Art/build_vehicle_paint.py), or null to use the body's own. */
		UMaterialInterface* Paint = nullptr;
		TArray<FWheel> Wheels;
	};

	/** The kit that dresses this style -- the same one for the same hash -- or false when none is imported. */
	bool For(dtg::sim::BodyStyle Style, uint32 Hash, FKit& Out);
}
