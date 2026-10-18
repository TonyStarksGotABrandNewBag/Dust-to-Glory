#include "Sim/PrivateSales.h"

#include "Sim/GameTheory.h"

#include <algorithm>
#include <cmath>

namespace dtg::sim
{
	namespace detail_private
	{
		constexpr std::int64_t MinutesPerDay = 24 * 60;

		const char* const ProudLines[] = {
			"Not for sale. My dad bought it new and I learned to drive in it.",
			"Sorry, this one's family. My kids are going to fight over it someday.",
			"I've turned down better offers than that. It stays in the driveway.",
		};
	}

	PrivateCar MakePrivateCar(const VehicleCatalog& Catalog, const std::string& CarId, std::uint64_t WorldSeed)
	{
		Rng Random(HashString(CarId) ^ (WorldSeed * 0x9E3779B97F4A7C15ULL));
		PrivateCar Car;
		Car.Id = CarId;
		Car.OwnerId = "owner-" + CarId;

		// Everyday cars from the last few decades, with the odd treasured classic in a garage.
		const bool bClassic = Random.Chance(0.06);
		std::vector<const VehicleModel*> Pool;
		for (const VehicleModel& Model : Catalog.Models())
		{
			if (Model.bEventOnly || Model.Body == BodyStyle::Bus || Model.Body == BodyStyle::Truck)
			{
				continue;
			}
			const bool bFits = bClassic ? Model.IsClassic() : (Model.AgeYears <= 22 && Model.Body != BodyStyle::Supercar);
			if (bFits)
			{
				Pool.push_back(&Model);
			}
		}
		if (Pool.empty())
		{
			return Car;
		}
		const VehicleModel* Model = Pool[Random.Index(Pool.size())];
		Car.ModelId = Model->Id;
		const int Age = Model->AgeYears + Random.RangeInt(0, std::max(1, Model->YearEnd - Model->YearStart));
		Car.MileageKm = std::round(std::max(3000.0, Age * 14000.0 * Random.Range(0.4, 1.3)) / 100.0) * 100.0;
		Car.Condition = bClassic ? Random.Range(0.55, 0.95) : Clamp(0.95 - 0.02 * Age + Random.Range(-0.15, 0.08), 0.25, 0.98);
		if (Car.Condition < 0.6)
		{
			const CarIssue Common[] = {CarIssue::WornBrakes, CarIssue::BaldTires, CarIssue::FadedPaint, CarIssue::WornSuspension};
			Car.Issues.push_back(Common[Random.Index(4)]);
		}
		const std::uint32_t Paints[] = {0xF2F2F2, 0x1C1C1C, 0x9A9A9A, 0xB71C1C, 0x0D47A1, 0x2E7D32, 0xC9A227, 0x5D4037, 0x37474F, 0x6A1B9A};
		Car.PaintRgb = Paints[Random.Index(10)];
		Car.Attachment = bClassic ? Random.Range(0.5, 1.0) : Random.Range(0.0, 0.8);
		Car.CashNeed = Random.Range(0.0, 0.7);
		const double Roll = Random.NextDouble();
		Car.Schedule = Roll < 0.65 ? OwnerSchedule::DayWorker : (Roll < 0.8 ? OwnerSchedule::NightShift : OwnerSchedule::HomeBody);
		const double Contact = Random.NextDouble();
		if (Car.Schedule == OwnerSchedule::HomeBody)
		{
			Car.Prefers = Contact < 0.6 ? Channel::Call : Channel::Email;
		}
		else
		{
			Car.Prefers = Contact < 0.6 ? Channel::Text : (Contact < 0.9 ? Channel::Call : Channel::Email);
		}
		return Car;
	}

	bool IsOwnerHome(const PrivateCar& Car, int DayOfWeek, int Hour)
	{
		const bool bWeekend = DayOfWeek >= 5;
		switch (Car.Schedule)
		{
		case OwnerSchedule::DayWorker:
			// Home evenings, nights and weekends (but asleep and not answering the door 23:00-7:00).
			if (Hour >= 23 || Hour < 7) return false;
			return bWeekend || Hour >= 18 || Hour < 8;
		case OwnerSchedule::NightShift:
			// Out working nights; sleeps until early afternoon.
			return Hour >= 14 && Hour < 19;
		case OwnerSchedule::HomeBody:
			return Hour >= 8 && Hour < 21;
		case OwnerSchedule::Count:
			break;
		}
		return false;
	}

	std::int64_t NextHomeMinute(const PrivateCar& Car, std::int64_t NowMinute)
	{
		using detail_private::MinutesPerDay;
		// Scan forward hour by hour (a week at most).
		std::int64_t Minute = (NowMinute / 60 + 1) * 60;
		for (int Step = 0; Step < 24 * 7; ++Step, Minute += 60)
		{
			const int Day = static_cast<int>((Minute / MinutesPerDay) % 7);
			const int Hour = static_cast<int>((Minute % MinutesPerDay) / 60);
			if (IsOwnerHome(Car, Day, Hour))
			{
				return Minute;
			}
		}
		return NowMinute + MinutesPerDay;
	}

	double OwnerReservation(const VehicleModel& Model, const PrivateCar& Car)
	{
		const double Value = ToDollars(MarketValue(Model, Car.Condition, Car.MileageKm, Car.Issues));
		// Attached owners want a premium; owners who need cash will take less than it's worth.
		return Value * (0.92 + 0.6 * Car.Attachment - 0.25 * Car.CashNeed);
	}

	double OwnerPatience(const PrivateCar& Car)
	{
		return Clamp(0.95 - 0.4 * Car.CashNeed, 0.5, 0.97);
	}

	double PlayerPatience(int PeopleSkill)
	{
		return Clamp(0.85 + 0.01 * PeopleSkill, 0.8, 0.95);
	}

	OfferResponse EvaluateOffer(const VehicleModel& Model, PrivateCar& Car, Money Offer, int PeopleSkill, double Affinity, BargainStyle Style,
		Rng& Random)
	{
		const bool bNote = Style != BargainStyle::FaceToFace;
		OfferResponse Response;
		const double Value = ToDollars(MarketValue(Model, Car.Condition, Car.MileageKm, Car.Issues));
		const double Reservation = OwnerReservation(Model, Car);
		const double Amount = ToDollars(Offer);
		Car.OffersMade += 1;
		if (!bNote)
		{
			Car.Rounds += 1;
		}
		if (Car.Attachment > 0.85 && Amount < Value * 2.0)
		{
			Response.Outcome = OfferOutcome::NotForSale;
			Response.Reply = detail_private::ProudLines[Random.Index(3)];
			return Response;
		}
		const double Ceiling = std::max(Reservation, Value * 1.25);
		if (Style == BargainStyle::Text)
		{
			// Rubinstein alternating offers: the owner, proposing next, would claim this share of the surplus.
			const double Pie = Ceiling - Reservation;
			const double OwnerDelta = OwnerPatience(Car);
			const double OwnerShare = RubinsteinShare(OwnerDelta, PlayerPatience(PeopleSkill) + 0.03 * Clamp(Affinity, -1.0, 1.0));
			const double NextCounter = Reservation + OwnerShare * Pie;
			// Accept anything worth more than waiting a round to get their own counter.
			if (Amount >= Reservation + OwnerDelta * OwnerShare * Pie)
			{
				Response.Outcome = OfferOutcome::Accepted;
				Response.Price = Offer;
				Response.Reply = "OK, deal. When can you pick it up?";
				return Response;
			}
			if (Amount < 0.7 * Reservation)
			{
				Response.Outcome = OfferOutcome::Declined;
				Response.Reply = "Sorry, that's way too low for me.";
				return Response;
			}
			Response.Outcome = OfferOutcome::Countered;
			Response.Price = ToMoney(std::round(std::max(NextCounter, Amount) / 50.0) * 50.0);
			Response.Reply = "Best I can do is " + FormatMoney(Response.Price) + ".";
			return Response;
		}
		// Nash bargaining: the surplus between the owner's floor and what they figure a keen buyer would pay is
		// split by bargaining power.
		const double Power = Clamp(0.3 + 0.04 * PeopleSkill + 0.15 * Clamp(Affinity, -1.0, 1.0), 0.1, 0.85);
		const Bargain Deal = NashBargain(Ceiling - Reservation, 0.0, 0.0, Power);
		const double NashPrice = Ceiling - Deal.PayoffA;
		if (Amount >= NashPrice * Random.Range(0.97, 1.0))
		{
			Response.Outcome = OfferOutcome::Accepted;
			Response.Price = Offer;
			Response.Reply = Car.CashNeed > 0.5 ? "Honestly? The timing couldn't be better. You've got a deal."
				: "You know what, that's fair. It's yours.";
			return Response;
		}
		const bool bInsulting = Amount < 0.7 * Reservation;
		if (bInsulting || (!bNote && Car.Rounds > 3))
		{
			Response.Outcome = OfferOutcome::Declined;
			Response.Reply = bInsulting ? "I appreciate the interest, but that's not even close."
				: "I think we're too far apart. Thanks, though.";
			return Response;
		}
		Response.Outcome = OfferOutcome::Countered;
		const double Counter = bNote || Car.Rounds > 1 ? NashPrice : 0.5 * (NashPrice + Ceiling);
		Response.Price = ToMoney(std::round(std::max(Counter, Amount) / 50.0) * 50.0);
		Response.Reply = "I could let it go for " + FormatMoney(Response.Price) + ".";
		return Response;
	}
}
