// Buying any car you see: every car parked at a house or on the street belongs to a persistent NPC owner.
// Knock on the door when they're home and haggle, or leave a note with your phone number and an offer;
// they call back with an answer. Owners are generated deterministically from the car's world id, so the
// same car always has the same owner, and only cars the player has dealt with need saving.
#pragma once

#include "Sim/Garage.h"
#include "Sim/Messages.h"
#include "Sim/SimCommon.h"
#include "Sim/Vehicles.h"

#include <string>
#include <vector>

namespace dtg::sim
{
	/** When the owner is usually home. */
	enum class OwnerSchedule : int
	{
		DayWorker,   // out 8:00-18:00 on weekdays
		NightShift,  // sleeps days, out 19:00-7:00
		HomeBody,    // retired or works from home: usually in
		Count
	};

	struct PrivateCar
	{
		std::string Id;            // world id of the parking spot, e.g. "home-42-0"
		std::string OwnerId;       // NPC mind id
		std::string ModelId;
		double Condition = 0.7;
		double MileageKm = 90000.0;
		std::vector<CarIssue> Issues;
		std::uint32_t PaintRgb = 0x808080;
		double Attachment = 0.3;   // 0..1: how much they love it
		double CashNeed = 0.2;     // 0..1: how much they could use the money right now
		OwnerSchedule Schedule = OwnerSchedule::DayWorker;
		Channel Prefers = Channel::Text; // how they get back to people
		bool bSold = false;        // the player bought it; the spot is empty now
		int OffersMade = 0;
		int Rounds = 0;            // face-to-face haggling rounds in the current conversation
	};

	/** The car (and owner traits) that lives in a parking spot. Deterministic for a world seed. */
	PrivateCar MakePrivateCar(const VehicleCatalog& Catalog, const std::string& CarId, std::uint64_t WorldSeed);
	bool IsOwnerHome(const PrivateCar& Car, int DayOfWeek, int Hour);
	/** The next game minute (after Now) when the owner is home and awake enough to read a note. */
	std::int64_t NextHomeMinute(const PrivateCar& Car, std::int64_t NowMinute);

	struct OfferResponse
	{
		OfferOutcome Outcome = OfferOutcome::Declined;
		Money Price = 0;           // accepted price, or their counter
		std::string Reply;
	};

	/** Face to face and notes use Nash bargaining; text threads use Rubinstein alternating offers. */
	enum class BargainStyle : int
	{
		FaceToFace,
		Note,
		Text
	};

	/**
	 * The owner's answer. Their reservation price comes from the car's value, how attached they are and how much
	 * they need cash. Face to face (and in reply to a note) the surplus up to what a keen buyer would pay is split
	 * by Nash bargaining, with People skill and the owner's liking for the player as leverage. Over text each round
	 * of delay costs both sides, so the split follows Rubinstein's alternating-offers equilibrium: an owner who needs
	 * the money soon (impatient) concedes more, and accepts any offer worth more than waiting another round.
	 */
	OfferResponse EvaluateOffer(const VehicleModel& Model, PrivateCar& Car, Money Offer, int PeopleSkill, double Affinity, BargainStyle Style,
		Rng& Random);
	/** Per-round patience (discount factor) of the owner and of the player in text haggling. */
	double OwnerPatience(const PrivateCar& Car);
	double PlayerPatience(int PeopleSkill);
	/** The owner's private reservation price (exposed for tests). */
	double OwnerReservation(const VehicleModel& Model, const PrivateCar& Car);

	struct PendingOffer
	{
		int Id = 0;
		std::string CarId;
		Money Amount = 0;
		std::int64_t PlacedMinute = 0;
		std::int64_t ResponseMinute = 0;
		BargainStyle Style = BargainStyle::Note; // a note, or a counter sent by text
	};
}
