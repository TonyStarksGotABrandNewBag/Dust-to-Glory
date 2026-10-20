#include "Game/DTGVehicleModels.h"

#include "DustToGlory.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

using namespace dtg::sim;

namespace detail_vehiclemodels
{
	/**
	 * The City Sample bodies by folder; the meshes are SM_<Kind>_<Id>_No_Wheel, SM_All_Trans_<Kind>_<Id>
	 * (or SM_<Kind>_<Id>_Trans) and SM_Wheel_<Axle>_<Kind>_<Id> in <Folder>/Mesh. The trailer is left out.
	 */
	struct FBody
	{
		const TCHAR* Folder;
		const TCHAR* Kind;
		const TCHAR* Id;
	};
	const FBody Bodies[] = {
		{TEXT("vehicle01_Van"), TEXT("vehVan"), TEXT("vehicle01")},     // 0  minivan
		{TEXT("vehicle02_Car"), TEXT("vehCar"), TEXT("vehicle02")},     // 1  sedan
		{TEXT("vehicle03_Car"), TEXT("vehCar"), TEXT("vehicle03")},     // 2  long sedan
		{TEXT("vehicle04_Truck"), TEXT("vehTruck"), TEXT("vehicle04")}, // 3  pickup
		{TEXT("vehicle05_Car"), TEXT("vehCar"), TEXT("vehicle05")},     // 4  compact
		{TEXT("vehicle06_Car"), TEXT("vehCar"), TEXT("vehicle06")},     // 5  low sports car
		{TEXT("vehicle07_Car"), TEXT("vehCar"), TEXT("vehicle07")},     // 6  coupe
		{TEXT("vehicle08_Truck"), TEXT("vehTruck"), TEXT("vehicle08")}, // 7  semi tractor
		{TEXT("vehicle09_Van"), TEXT("vehVan"), TEXT("vehicle09")},     // 8  delivery van
		{TEXT("vehicle10_Bus"), TEXT("vehBus"), TEXT("vehicle10")},     // 9  bus
		{TEXT("vehicle11_Truck"), TEXT("vehTruck"), TEXT("vehicle11")}, // 10 garbage truck
		{TEXT("vehicle12_Car"), TEXT("vehCar"), TEXT("vehicle12")},     // 11 tall sedan footprint: SUV / wagon
		{TEXT("vehicle13_Car"), TEXT("vehCar"), TEXT("vehicle13")},     // 12 sedan
	};
	constexpr int32 BodyCount = static_cast<int32>(sizeof(Bodies) / sizeof(Bodies[0]));

	/** The wheel meshes a body may have, by the middle of their names. */
	struct FWheelName
	{
		const TCHAR* Name;
		bool bFront;
		bool bLeft;
		bool bExtra;
	};
	const FWheelName WheelNames[] = {
		{TEXT("Front_L"), true, true, false},  {TEXT("Front_R"), true, false, false},
		{TEXT("Rear_L"), false, true, false},  {TEXT("Rear_R"), false, false, false},
		{TEXT("Axel1_L"), false, true, true},  {TEXT("Axel1_R"), false, false, true},
		{TEXT("Axel2_L"), false, true, true},  {TEXT("Axel2_R"), false, false, true},
		{TEXT("Axel3_L"), false, true, true},  {TEXT("Axel3_R"), false, false, true},
	};

	/**
	 * The bodies that may dress each style, by index into Bodies, from the pack's folders and the
	 * meshes' sizes (Saved/edits/inspect_vehicles.py prints them). A style with no body keeps the
	 * generated shell.
	 */
	struct FVariants
	{
		int32 Bodies[3];
		int32 Num;
	};
	FVariants VariantsFor(BodyStyle Style)
	{
		switch (Style)
		{
		case BodyStyle::Sedan: return {{1, 2, 12}, 3};
		case BodyStyle::Coupe: return {{6, 5, 0}, 2};
		case BodyStyle::Hatchback: return {{4, 0, 0}, 1};
		case BodyStyle::Wagon: return {{11, 0, 0}, 1};
		case BodyStyle::Convertible: return {{5, 0, 0}, 1};
		case BodyStyle::SUV: return {{11, 0, 0}, 2};
		case BodyStyle::Pickup: return {{3, 0, 0}, 1};
		case BodyStyle::Van: return {{8, 0, 0}, 2};
		case BodyStyle::Muscle: return {{6, 1, 0}, 2};
		case BodyStyle::Sports: return {{5, 0, 0}, 1};
		case BodyStyle::Supercar: return {{5, 0, 0}, 1};
		case BodyStyle::Luxury: return {{2, 0, 0}, 1};
		case BodyStyle::Truck: return {{10, 7, 0}, 2};
		default: break; // open-wheelers and monster trucks have nothing like them in the pack
		}
		return {{0, 0, 0}, 0};
	}

	UStaticMesh* LoadMesh(const FBody& Body, const FString& Leaf)
	{
		const FString Path = FString::Printf(TEXT("/Game/CitySampleVehicles/%s/Mesh/%s.%s"), Body.Folder, *Leaf, *Leaf);
		// Quietly: most bodies lack most of the wheel names probed for, and a missing pack is reported once below.
		return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}

	/** What has been loaded for a body. Weak: the meshes live as long as some car wears them. */
	struct FEntry
	{
		bool bTried = false;
		bool bUsable = false;
		TWeakObjectPtr<UStaticMesh> Body;
		TWeakObjectPtr<UStaticMesh> Glass;
		TWeakObjectPtr<UMaterialInterface> Paint;
		TArray<TWeakObjectPtr<UStaticMesh>> WheelMeshes;
		TArray<DTGVehicleModels::FWheel> Wheels;
	};

	void Load(int32 Index, FEntry& Entry)
	{
		Entry.bTried = true;
		Entry.bUsable = false;
		Entry.WheelMeshes.Reset();
		Entry.Wheels.Reset();
		const FBody& Body = Bodies[Index];
		const FString Stem = FString::Printf(TEXT("%s_%s"), Body.Kind, Body.Id);
		Entry.Body = LoadMesh(Body, FString::Printf(TEXT("SM_%s_No_Wheel"), *Stem));
		if (!Entry.Body.IsValid())
		{
			UE_LOG(LogDTG, Log, TEXT("No vehicle body in /Game/CitySampleVehicles/%s; the City Sample Vehicles pack is added through the editor's Fab window."), Body.Folder);
			return;
		}
		Entry.Glass = LoadMesh(Body, FString::Printf(TEXT("SM_All_Trans_%s"), *Stem));
		if (!Entry.Glass.IsValid())
		{
			Entry.Glass = LoadMesh(Body, FString::Printf(TEXT("SM_%s_Trans"), *Stem));
		}
		// The pack's paint takes its colour from a gradient by per-instance random, which an actor never
		// has; Tools/Art/build_vehicle_paint.py makes a child per body with that switched off.
		const FString PaintPath = FString::Printf(TEXT("/Game/CitySampleVehicles/DTG/MI_DTG_Paint_%s.MI_DTG_Paint_%s"), Body.Folder, Body.Folder);
		Entry.Paint = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *PaintPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
		if (!Entry.Paint.IsValid())
		{
			UE_LOG(LogDTG, Warning, TEXT("No paint instance at %s; cars in this body keep the pack's first gradient colour. Tools/Art/build_vehicle_paint.py makes it."), *PaintPath);
		}
		bool bFront = false;
		bool bRear = false;
		for (const FWheelName& Name : WheelNames)
		{
			UStaticMesh* Mesh = LoadMesh(Body, FString::Printf(TEXT("SM_Wheel_%s_%s"), Name.Name, *Stem));
			if (!Mesh)
			{
				continue;
			}
			const FBoxSphereBounds Bounds = Mesh->GetBounds();
			DTGVehicleModels::FWheel Wheel;
			Wheel.bFront = Name.bFront;
			Wheel.bLeft = Name.bLeft;
			Wheel.bExtra = Name.bExtra;
			Wheel.Centre = Bounds.Origin;
			Wheel.Radius = Bounds.BoxExtent.Z;
			Entry.WheelMeshes.Add(Mesh);
			Entry.Wheels.Add(Wheel);
			bFront = bFront || Name.bFront;
			bRear = bRear || (!Name.bFront && !Name.bExtra);
		}
		Entry.bUsable = bFront && bRear;
		if (!Entry.bUsable)
		{
			UE_LOG(LogDTG, Warning, TEXT("Vehicle body %s has no usable wheels; it is not drawn."), Body.Folder);
		}
	}

	bool Fetch(int32 Index, DTGVehicleModels::FKit& Out)
	{
		static FEntry Entries[BodyCount];
		FEntry& Entry = Entries[Index];
		// Loaded once; loaded again if the meshes were collected while no car wore them.
		if (!Entry.bTried || (Entry.bUsable && !Entry.Body.IsValid()))
		{
			Load(Index, Entry);
		}
		if (!Entry.bUsable)
		{
			return false;
		}
		Out.Name = Bodies[Index].Folder;
		Out.Body = Entry.Body.Get();
		Out.Glass = Entry.Glass.Get();
		Out.Paint = Entry.Paint.Get();
		Out.Wheels = Entry.Wheels;
		for (int32 Wheel = 0; Wheel < Out.Wheels.Num(); ++Wheel)
		{
			Out.Wheels[Wheel].Mesh = Entry.WheelMeshes[Wheel].Get();
		}
		return Out.Body != nullptr;
	}
}

namespace DTGVehicleModels
{
	bool For(BodyStyle Style, uint32 Hash, FKit& Out)
	{
		const detail_vehiclemodels::FVariants Options = detail_vehiclemodels::VariantsFor(Style);
		// Start at the hashed choice and take the first one that is there.
		for (int32 Step = 0; Step < Options.Num; ++Step)
		{
			const int32 Pick = Options.Bodies[(Hash + static_cast<uint32>(Step)) % static_cast<uint32>(Options.Num)];
			if (detail_vehiclemodels::Fetch(Pick, Out))
			{
				return true;
			}
		}
		return false;
	}
}
