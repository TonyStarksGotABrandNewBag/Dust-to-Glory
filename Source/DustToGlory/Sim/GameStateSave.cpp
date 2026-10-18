// Save and load for GameState. Content (catalogs, city, laws) is rebuilt from data files; only progress is saved.
#include "Sim/GameState.h"

#include <cstdlib>

namespace dtg::sim
{
	namespace detail_save
	{
		JsonValue Num(double Value) { return JsonValue::MakeNumber(Value); }
		JsonValue Str(const std::string& Value) { return JsonValue::MakeString(Value); }
		JsonValue Bool(bool Value) { return JsonValue::MakeBool(Value); }

		JsonValue Strings(const std::vector<std::string>& Values)
		{
			JsonValue Array = JsonValue::MakeArray();
			for (const std::string& Value : Values) Array.Push(Str(Value));
			return Array;
		}

		std::vector<std::string> ReadStrings(const JsonValue& Array)
		{
			std::vector<std::string> Values;
			for (const JsonValue& Item : Array.Items()) Values.push_back(Item.AsString());
			return Values;
		}

		// Money as a JSON number is exact for any realistic amount (doubles hold integers to 2^53 cents).
		JsonValue Cash(Money Value) { return Num(static_cast<double>(Value)); }
		Money ReadCash(const JsonValue& Object, const std::string& Key) { return Object.GetInt64(Key, 0); }

		JsonValue Rep(const Reputation& Value)
		{
			JsonValue Out = JsonValue::MakeObject();
			Out.Set("rating", Num(Value.Rating));
			Out.Set("reviews", Num(Value.ReviewCount));
			Out.Set("sentiment", Num(Value.Sentiment));
			Out.Set("followers", Num(Value.Followers));
			Out.Set("trust", Num(Value.Trust));
			return Out;
		}

		void ReadRep(const JsonValue& In, Reputation& Value)
		{
			Value.Rating = In.GetNumber("rating", Value.Rating);
			Value.ReviewCount = In.GetInt("reviews", Value.ReviewCount);
			Value.Sentiment = In.GetNumber("sentiment", Value.Sentiment);
			Value.Followers = In.GetInt("followers", Value.Followers);
			Value.Trust = In.GetNumber("trust", Value.Trust);
		}

		JsonValue Tuning(const TuningSheet& Sheet)
		{
			JsonValue Out = JsonValue::MakeObject();
			Out.Set("rideF", Num(Sheet.RideHeightFrontMm));
			Out.Set("rideR", Num(Sheet.RideHeightRearMm));
			Out.Set("springF", Num(Sheet.SpringFrontHz));
			Out.Set("springR", Num(Sheet.SpringRearHz));
			Out.Set("bumpF", Num(Sheet.DamperBumpFront));
			Out.Set("bumpR", Num(Sheet.DamperBumpRear));
			Out.Set("rebF", Num(Sheet.DamperReboundFront));
			Out.Set("rebR", Num(Sheet.DamperReboundRear));
			Out.Set("arbF", Num(Sheet.AntiRollFront));
			Out.Set("arbR", Num(Sheet.AntiRollRear));
			Out.Set("camberF", Num(Sheet.CamberFrontDeg));
			Out.Set("camberR", Num(Sheet.CamberRearDeg));
			Out.Set("toeF", Num(Sheet.ToeFrontDeg));
			Out.Set("toeR", Num(Sheet.ToeRearDeg));
			Out.Set("dfF", Num(Sheet.DownforceFront));
			Out.Set("dfR", Num(Sheet.DownforceRear));
			Out.Set("bias", Num(Sheet.BrakeBias));
			Out.Set("lsdI", Num(Sheet.LsdInitial));
			Out.Set("lsdA", Num(Sheet.LsdAccel));
			Out.Set("lsdD", Num(Sheet.LsdDecel));
			Out.Set("awd", Num(Sheet.AwdFrontSplit));
			JsonValue Ratios = JsonValue::MakeArray();
			for (const double Ratio : Sheet.GearRatios) Ratios.Push(Num(Ratio));
			Out.Set("gears", Ratios);
			Out.Set("final", Num(Sheet.FinalDrive));
			Out.Set("ecu", Num(Sheet.EcuOutputPercent));
			Out.Set("ballast", Num(Sheet.BallastKg));
			Out.Set("ballastPos", Num(Sheet.BallastPosition));
			Out.Set("tc", Num(Sheet.TractionControl));
			Out.Set("abs", Bool(Sheet.bAbs));
			return Out;
		}

		TuningSheet ReadTuning(const JsonValue& In)
		{
			TuningSheet Sheet;
			Sheet.RideHeightFrontMm = In.GetNumber("rideF", Sheet.RideHeightFrontMm);
			Sheet.RideHeightRearMm = In.GetNumber("rideR", Sheet.RideHeightRearMm);
			Sheet.SpringFrontHz = In.GetNumber("springF", Sheet.SpringFrontHz);
			Sheet.SpringRearHz = In.GetNumber("springR", Sheet.SpringRearHz);
			Sheet.DamperBumpFront = In.GetInt("bumpF", Sheet.DamperBumpFront);
			Sheet.DamperBumpRear = In.GetInt("bumpR", Sheet.DamperBumpRear);
			Sheet.DamperReboundFront = In.GetInt("rebF", Sheet.DamperReboundFront);
			Sheet.DamperReboundRear = In.GetInt("rebR", Sheet.DamperReboundRear);
			Sheet.AntiRollFront = In.GetInt("arbF", Sheet.AntiRollFront);
			Sheet.AntiRollRear = In.GetInt("arbR", Sheet.AntiRollRear);
			Sheet.CamberFrontDeg = In.GetNumber("camberF", Sheet.CamberFrontDeg);
			Sheet.CamberRearDeg = In.GetNumber("camberR", Sheet.CamberRearDeg);
			Sheet.ToeFrontDeg = In.GetNumber("toeF", Sheet.ToeFrontDeg);
			Sheet.ToeRearDeg = In.GetNumber("toeR", Sheet.ToeRearDeg);
			Sheet.DownforceFront = In.GetInt("dfF", Sheet.DownforceFront);
			Sheet.DownforceRear = In.GetInt("dfR", Sheet.DownforceRear);
			Sheet.BrakeBias = In.GetNumber("bias", Sheet.BrakeBias);
			Sheet.LsdInitial = In.GetInt("lsdI", Sheet.LsdInitial);
			Sheet.LsdAccel = In.GetInt("lsdA", Sheet.LsdAccel);
			Sheet.LsdDecel = In.GetInt("lsdD", Sheet.LsdDecel);
			Sheet.AwdFrontSplit = In.GetNumber("awd", Sheet.AwdFrontSplit);
			for (const JsonValue& Ratio : In.Get("gears").Items()) Sheet.GearRatios.push_back(Ratio.AsNumber());
			Sheet.FinalDrive = In.GetNumber("final", Sheet.FinalDrive);
			Sheet.EcuOutputPercent = In.GetInt("ecu", Sheet.EcuOutputPercent);
			Sheet.BallastKg = In.GetNumber("ballast", Sheet.BallastKg);
			Sheet.BallastPosition = In.GetNumber("ballastPos", Sheet.BallastPosition);
			Sheet.TractionControl = In.GetInt("tc", Sheet.TractionControl);
			Sheet.bAbs = In.GetBool("abs", Sheet.bAbs);
			return Sheet;
		}

		JsonValue Power(const PowerInstance& Value)
		{
			JsonValue Out = JsonValue::MakeObject();
			Out.Set("id", Str(Value.PowerId));
			Out.Set("level", Num(Value.Level));
			Out.Set("xp", Num(Value.Xp));
			Out.Set("instability", Num(Value.Instability));
			Out.Set("mutations", Strings(Value.Mutations));
			return Out;
		}

		PowerInstance ReadPower(const JsonValue& In)
		{
			PowerInstance Value;
			Value.PowerId = In.GetString("id");
			Value.Level = In.GetInt("level", 1);
			Value.Xp = In.GetInt("xp");
			Value.Instability = In.GetNumber("instability");
			Value.Mutations = ReadStrings(In.Get("mutations"));
			return Value;
		}

		JsonValue Issues(const std::vector<CarIssue>& Values)
		{
			JsonValue Array = JsonValue::MakeArray();
			for (const CarIssue Issue : Values) Array.Push(Num(static_cast<int>(Issue)));
			return Array;
		}

		std::vector<CarIssue> ReadIssues(const JsonValue& Array)
		{
			std::vector<CarIssue> Values;
			for (const JsonValue& Item : Array.Items())
			{
				const int Index = Item.AsInt(-1);
				if (Index >= 0 && Index < static_cast<int>(CarIssue::Count)) Values.push_back(static_cast<CarIssue>(Index));
			}
			return Values;
		}

		template <typename T, std::size_t N>
		JsonValue IntArray(const std::array<T, N>& Values)
		{
			JsonValue Array = JsonValue::MakeArray();
			for (const T Value : Values) Array.Push(Num(static_cast<double>(Value)));
			return Array;
		}
	}

	std::string GameState::SaveToJson() const
	{
		using namespace detail_save;
		JsonValue Root = JsonValue::MakeObject();
		Root.Set("version", Num(SaveVersion));
		Root.Set("seed", Num(static_cast<double>(WorldSeed)));
		Root.Set("name", Str(PlayerName));
		Root.Set("minutes", Num(Clock.ExactMinutes()));
		Root.Set("timeScale", Num(Clock.TimeScale));
		Root.Set("weather", Num(static_cast<int>(Weather.Current())));
		Root.Set("weatherNext", Num(static_cast<double>(Weather.NextChangeMinute())));
		// The RNG state is two 64-bit words: stored as strings to keep every bit.
		Root.Set("rngState", Str(std::to_string(Random.GetState())));
		Root.Set("rngInc", Str(std::to_string(Random.GetIncrement())));
		Root.Set("nextVehicle", Num(NextVehicleNumber));
		Root.Set("eventsWon", Num(EventsWon));

		JsonValue WalletJson = JsonValue::MakeObject();
		WalletJson.Set("balance", Cash(Funds.Balance()));
		JsonValue Earned = JsonValue::MakeArray();
		for (const Money Value : Funds.EarnedTotals()) Earned.Push(Cash(Value));
		JsonValue Spent = JsonValue::MakeArray();
		for (const Money Value : Funds.SpentTotals()) Spent.Push(Cash(Value));
		WalletJson.Set("earned", Earned);
		WalletJson.Set("spent", Spent);
		JsonValue History = JsonValue::MakeArray();
		for (const Transaction& Entry : Funds.Recent())
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("minute", Num(static_cast<double>(Entry.GameMinute)));
			Item.Set("amount", Cash(Entry.Amount));
			Item.Set("category", Num(static_cast<int>(Entry.Category)));
			Item.Set("note", Str(Entry.Note));
			History.Push(Item);
		}
		WalletJson.Set("history", History);
		Root.Set("wallet", WalletJson);

		JsonValue ProfileJson = JsonValue::MakeObject();
		ProfileJson.Set("xp", IntArray(Player.AllXp()));
		ProfileJson.Set("degrees", Strings(Player.Educations()));
		ProfileJson.Set("enrolledIn", Str(Player.CurrentStudy().EducationId));
		ProfileJson.Set("enrolledHours", Num(Player.CurrentStudy().HoursDone));
		Root.Set("profile", ProfileJson);

		JsonValue JobsJson = JsonValue::MakeArray();
		for (const CareerProgress& Job : Jobs)
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("career", Str(Job.CareerId));
			Item.Set("level", Num(Job.Level));
			Item.Set("xp", Num(Job.Xp));
			Item.Set("shifts", Num(Job.ShiftsWorked));
			Item.Set("earned", Cash(Job.TotalEarned));
			JobsJson.Push(Item);
		}
		Root.Set("jobs", JobsJson);

		JsonValue PowersJson = JsonValue::MakeArray();
		for (const PowerInstance& Value : PlayerPowers.Slots()) PowersJson.Push(Power(Value));
		Root.Set("powers", PowersJson);

		const HomeAndFurniture::Snapshot HomeData = Homes.Save();
		JsonValue HomesJson = JsonValue::MakeObject();
		HomesJson.Set("owned", Strings(HomeData.Owned));
		HomesJson.Set("nextOrder", Num(HomeData.NextOrderId));
		HomesJson.Set("nextUid", Num(HomeData.NextUid));
		JsonValue Orders = JsonValue::MakeArray();
		for (const FurnitureOrder& Order : HomeData.Orders)
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("id", Num(Order.Id));
			Item.Set("property", Str(Order.PropertyId));
			Item.Set("items", Strings(Order.Items));
			Item.Set("subtotal", Cash(Order.Subtotal));
			Item.Set("fee", Cash(Order.DeliveryFee));
			Item.Set("speed", Num(static_cast<int>(Order.Speed)));
			Item.Set("placed", Num(static_cast<double>(Order.PlacedMinute)));
			Item.Set("dispatch", Num(static_cast<double>(Order.DispatchMinute)));
			Item.Set("deliver", Num(static_cast<double>(Order.DeliverMinute)));
			Item.Set("status", Num(static_cast<int>(Order.Status)));
			Orders.Push(Item);
		}
		HomesJson.Set("orders", Orders);
		JsonValue Inventory = JsonValue::MakeArray();
		for (const InventoryEntry& Entry : HomeData.Inventory)
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("property", Str(Entry.PropertyId));
			Item.Set("item", Str(Entry.ItemId));
			Inventory.Push(Item);
		}
		HomesJson.Set("inventory", Inventory);
		JsonValue Placed = JsonValue::MakeArray();
		for (const PlacedFurniture& Piece : HomeData.Placed)
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("uid", Num(Piece.Uid));
			Item.Set("item", Str(Piece.ItemId));
			Item.Set("property", Str(Piece.PropertyId));
			Item.Set("x", Num(Piece.X));
			Item.Set("y", Num(Piece.Y));
			Item.Set("yaw", Num(Piece.YawDeg));
			Placed.Push(Item);
		}
		HomesJson.Set("placed", Placed);
		Root.Set("homes", HomesJson);

		JsonValue GarageJson = JsonValue::MakeArray();
		for (const OwnedVehicle& Car : Garage)
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("uid", Str(Car.Uid));
			Item.Set("model", Str(Car.ModelId));
			Item.Set("condition", Num(Car.Condition));
			Item.Set("km", Num(Car.MileageKm));
			Item.Set("issues", Issues(Car.Issues));
			Item.Set("parts", Strings(Car.Build.Parts));
			Item.Set("tuning", Tuning(Car.Build.Tuning));
			Item.Set("paint", Num(Car.Build.PaintRgb));
			Item.Set("finish", Num(Car.Build.PaintFinish));
			Item.Set("wheels", Num(Car.Build.WheelRgb));
			Item.Set("restoration", Num(static_cast<int>(Car.Restoration)));
			Item.Set("certified", Bool(Car.bCertified));
			Item.Set("warranty", Num(static_cast<double>(Car.WarrantyUntilMinute)));
			Item.Set("ready", Num(static_cast<double>(Car.ReadyMinute)));
			GarageJson.Push(Item);
		}
		Root.Set("garage", GarageJson);
		Root.Set("activeVehicle", Str(ActiveVehicleUid));
		Root.Set("manualTransmission", Bool(bManualTransmission));

		JsonValue LotJson = JsonValue::MakeArray();
		for (const UsedListing& Listing : UsedCars.Listings())
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("id", Str(Listing.Id));
			Item.Set("model", Str(Listing.ModelId));
			Item.Set("condition", Num(Listing.Condition));
			Item.Set("km", Num(Listing.MileageKm));
			Item.Set("issues", Issues(Listing.Issues));
			Item.Set("asking", Cash(Listing.AskingPrice));
			Item.Set("seller", Str(Listing.Seller));
			Item.Set("barnFind", Bool(Listing.bBarnFind));
			Item.Set("rounds", Num(Listing.Rounds));
			Item.Set("dealer", Num(static_cast<int>(Listing.Dealer)));
			Item.Set("showroom", Str(Listing.ShowroomId));
			Item.Set("restoration", Num(static_cast<int>(Listing.Restoration)));
			Item.Set("parts", Strings(Listing.Parts));
			Item.Set("noHaggle", Bool(Listing.bNoHaggle));
			LotJson.Push(Item);
		}
		Root.Set("usedCars", LotJson);

		JsonValue Tools = JsonValue::MakeObject();
		Tools.Set("probe", Num(HackTools.ProbeKit));
		Tools.Set("ghost", Num(HackTools.GhostRelay));
		Tools.Set("pattern", Num(HackTools.PatternEngine));
		Root.Set("hackTools", Tools);

		const Ministry::Snapshot MinistryData = PlayerMinistry.Save();
		JsonValue MinistryJson = JsonValue::MakeObject();
		MinistryJson.Set("name", Str(MinistryData.Name));
		MinistryJson.Set("members", Num(MinistryData.Members));
		MinistryJson.Set("engagement", Num(MinistryData.Engagement));
		MinistryJson.Set("integrity", Num(MinistryData.Integrity));
		MinistryJson.Set("reputation", Num(MinistryData.Reputation));
		MinistryJson.Set("salaryShare", Num(MinistryData.SalaryShare));
		MinistryJson.Set("fund", Cash(MinistryData.Fund));
		MinistryJson.Set("ordination", Num(MinistryData.Ordination));
		MinistryJson.Set("facilities", IntArray(MinistryData.Facilities));
		MinistryJson.Set("lastTheme", Num(MinistryData.LastTheme));
		MinistryJson.Set("pending", Num(MinistryData.PendingVisitors));
		MinistryJson.Set("weeks", Num(MinistryData.WeeksRun));
		MinistryJson.Set("doctrine", Num(MinistryData.Doctrine));
		Root.Set("ministry", MinistryJson);

		JsonValue MarketJson = JsonValue::MakeObject();
		JsonValue BusinessJson = JsonValue::MakeArray();
		for (const Business& Value : Markets.Businesses())
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("id", Str(Value.Id));
			Item.Set("name", Str(Value.Name));
			Item.Set("cash", Cash(Value.Cash));
			Item.Set("health", Num(Value.Health));
			JsonValue Problems = JsonValue::MakeArray();
			for (const ProblemKind Kind : Value.Problems) Problems.Push(Num(static_cast<int>(Kind)));
			Item.Set("problems", Problems);
			Item.Set("integrity", Num(Value.Integrity));
			Item.Set("doctrine", Num(Value.Doctrine));
			Item.Set("equity", Num(Value.PlayerEquity));
			Item.Set("price", Num(Value.PriceLevel));
			Item.Set("rival", Num(Value.RivalPrice));
			Item.Set("dissolved", Bool(Value.bDissolved));
			Item.Set("rep", Rep(Value.Rep));
			BusinessJson.Push(Item);
		}
		MarketJson.Set("businesses", BusinessJson);
		JsonValue StockJson = JsonValue::MakeArray();
		for (const Stock& Value : Markets.Stocks())
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("ticker", Str(Value.Ticker));
			Item.Set("price", Num(Value.Price));
			Item.Set("prev", Num(Value.PreviousClose));
			Item.Set("pending", Num(Value.PendingImpact));
			JsonValue Closes = JsonValue::MakeArray();
			for (const double Close : Value.History) Closes.Push(Num(Close));
			Item.Set("history", Closes);
			StockJson.Push(Item);
		}
		MarketJson.Set("stocks", StockJson);
		JsonValue HoldingJson = JsonValue::MakeArray();
		for (const Holding& Value : Markets.Portfolio())
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("ticker", Str(Value.Ticker));
			Item.Set("shares", Num(static_cast<double>(Value.Shares)));
			Item.Set("cost", Num(Value.AverageCost));
			HoldingJson.Push(Item);
		}
		MarketJson.Set("holdings", HoldingJson);
		JsonValue NewsJson = JsonValue::MakeArray();
		for (const NewsItem& Value : Markets.News())
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("headline", Str(Value.Headline));
			Item.Set("entity", Str(Value.EntityId));
			Item.Set("impact", Num(Value.Impact));
			Item.Set("minute", Num(static_cast<double>(Value.Minute)));
			NewsJson.Push(Item);
		}
		MarketJson.Set("news", NewsJson);
		MarketJson.Set("playerRep", Rep(Markets.PlayerReputation()));
		Root.Set("market", MarketJson);

		JsonValue WorksJson = JsonValue::MakeArray();
		for (const CreativeWork& Work : Ip.Works())
		{
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("id", Str(Work.Id));
			Item.Set("title", Str(Work.Title));
			Item.Set("kind", Num(static_cast<int>(Work.Kind)));
			Item.Set("quality", Num(Work.Quality));
			Item.Set("hype", Num(Work.Hype));
			Item.Set("weeks", Num(Work.WeeksOut));
			Item.Set("royalties", Cash(Work.LifetimeRoyalties));
			Item.Set("protected", Bool(Work.bProtected));
			Item.Set("licensee", Str(Work.Licensee));
			Item.Set("licenseeShare", Num(Work.LicenseeShare));
			WorksJson.Push(Item);
		}
		Root.Set("works", WorksJson);

		JsonValue MindsJson = JsonValue::MakeArray();
		for (const NpcMind& Mind : Npcs.All())
		{
			// Only minds that have experienced something are worth saving; the rest regenerate from their id.
			if (Mind.Awareness <= 0.0 && Mind.Conversations == 0)
			{
				continue;
			}
			JsonValue Item = JsonValue::MakeObject();
			Item.Set("id", Str(Mind.Id));
			Item.Set("awareness", Num(Mind.Awareness));
			Item.Set("stage", Num(static_cast<int>(Mind.Stage)));
			Item.Set("affinity", Num(Mind.Affinity));
			Item.Set("skills", IntArray(Mind.SkillXp));
			Item.Set("seen", IntArray(Mind.Seen));
			Item.Set("goal", Str(Mind.Goal));
			Item.Set("goalCareer", Str(Mind.GoalCareer));
			Item.Set("career", Str(Mind.CareerId));
			Item.Set("companion", Bool(Mind.bCompanion));
			Item.Set("power", Str(Mind.PowerId));
			Item.Set("side", Num(static_cast<int>(Mind.Side)));
			Item.Set("conversations", Num(Mind.Conversations));
			Item.Set("dealStrategy", Num(static_cast<int>(Mind.Deal.Strategy)));
			Item.Set("dealLast", Num(Mind.Deal.LastOpponentMove));
			Item.Set("dealTriggered", Bool(Mind.Deal.bTriggered));
			Item.Set("dealRounds", Num(Mind.Deal.Rounds));
			Item.Set("dealDefections", Num(Mind.Deal.OpponentDefections));
			MindsJson.Push(Item);
		}
		Root.Set("minds", MindsJson);

		const PoliticalCareer::Snapshot PoliticsData = Politics.Save();
		JsonValue PoliticsJson = JsonValue::MakeObject();
		PoliticsJson.Set("office", Num(static_cast<int>(PoliticsData.CurrentOffice)));
		PoliticsJson.Set("highest", Num(static_cast<int>(PoliticsData.HighestOffice)));
		PoliticsJson.Set("approval", Num(PoliticsData.Approval));
		PoliticsJson.Set("termEnds", Num(static_cast<double>(PoliticsData.TermEndMinute)));
		PoliticsJson.Set("laws", Strings(PoliticsData.Laws));
		PoliticsJson.Set("wins", Num(PoliticsData.Wins));
		PoliticsJson.Set("losses", Num(PoliticsData.Losses));
		JsonValue RunJson = JsonValue::MakeObject();
		RunJson.Set("active", Bool(PoliticsData.Run.bActive));
		RunJson.Set("target", Num(static_cast<int>(PoliticsData.Run.Target)));
		RunJson.Set("support", Num(PoliticsData.Run.Support));
		RunJson.Set("opponentSupport", Num(PoliticsData.Run.OpponentSupport));
		RunJson.Set("opponent", Str(PoliticsData.Run.Opponent));
		RunJson.Set("spent", Cash(PoliticsData.Run.Spent));
		RunJson.Set("rallies", Num(PoliticsData.Run.Rallies));
		RunJson.Set("debates", Num(PoliticsData.Run.Debates));
		RunJson.Set("endorsements", Num(PoliticsData.Run.Endorsements));
		RunJson.Set("election", Num(static_cast<double>(PoliticsData.Run.ElectionMinute)));
		PoliticsJson.Set("campaign", RunJson);
		Root.Set("politics", PoliticsJson);

		return Root.Dump(true);
	}

	bool GameState::LoadFromJson(const std::string& Text, std::string* OutError)
	{
		using namespace detail_save;
		const JsonParseResult Parsed = ParseJson(Text);
		if (!Parsed.bOk)
		{
			if (OutError) *OutError = "Save file is damaged: " + Parsed.Error;
			return false;
		}
		const JsonValue& Root = Parsed.Value;
		if (Root.GetInt("version", 0) > SaveVersion)
		{
			if (OutError) *OutError = "This save was made by a newer version of the game.";
			return false;
		}
		PlayerName = Root.GetString("name", "Player");
		Clock = GameClock();
		Clock.SetTotalMinutes(Root.GetNumber("minutes", 480.0));
		Clock.TimeScale = Root.GetNumber("timeScale", 0.5);
		const int WeatherIndex = Clamp(Root.GetInt("weather", 0), 0, static_cast<int>(Weather::Count) - 1);
		Weather.Restore(static_cast<dtg::sim::Weather>(WeatherIndex), Root.GetInt64("weatherNext", 0));
		NextVehicleNumber = Root.GetInt("nextVehicle", 1);
		EventsWon = Root.GetInt("eventsWon", 0);

		const JsonValue& WalletJson = Root.Get("wallet");
		std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)> Earned{};
		std::array<Money, static_cast<std::size_t>(MoneyCategory::Count)> Spent{};
		for (std::size_t Index = 0; Index < Earned.size(); ++Index)
		{
			Earned[Index] = WalletJson.Get("earned").At(Index).AsInt64();
			Spent[Index] = WalletJson.Get("spent").At(Index).AsInt64();
		}
		std::vector<Transaction> History;
		for (const JsonValue& Item : WalletJson.Get("history").Items())
		{
			Transaction Entry;
			Entry.GameMinute = Item.GetInt64("minute");
			Entry.Amount = Item.GetInt64("amount");
			Entry.Category = static_cast<MoneyCategory>(Clamp(Item.GetInt("category"), 0, static_cast<int>(MoneyCategory::Count) - 1));
			Entry.Note = Item.GetString("note");
			History.push_back(Entry);
		}
		Funds.RestoreState(ReadCash(WalletJson, "balance"), Earned, Spent, History);

		const JsonValue& ProfileJson = Root.Get("profile");
		Player = Profile();
		std::array<int, static_cast<std::size_t>(Skill::Count)> Xp{};
		for (std::size_t Index = 0; Index < Xp.size(); ++Index)
		{
			Xp[Index] = ProfileJson.Get("xp").At(Index).AsInt();
		}
		Player.RestoreXp(Xp);
		for (const std::string& Degree : ReadStrings(ProfileJson.Get("degrees")))
		{
			Player.GrantEducation(Degree);
		}
		if (!ProfileJson.GetString("enrolledIn").empty())
		{
			Enrollment Study;
			Study.EducationId = ProfileJson.GetString("enrolledIn");
			Study.HoursDone = ProfileJson.GetInt("enrolledHours");
			Player.SetEnrollment(Study);
		}

		Jobs.clear();
		for (const JsonValue& Item : Root.Get("jobs").Items())
		{
			CareerProgress Job;
			Job.CareerId = Item.GetString("career");
			Job.Level = Item.GetInt("level");
			Job.Xp = Item.GetInt("xp");
			Job.ShiftsWorked = Item.GetInt("shifts");
			Job.TotalEarned = ReadCash(Item, "earned");
			if (Careers.FindCareer(Job.CareerId))
			{
				Jobs.push_back(Job);
			}
		}

		std::vector<PowerInstance> Slots;
		for (const JsonValue& Item : Root.Get("powers").Items())
		{
			PowerInstance Value = ReadPower(Item);
			if (Powers.FindPower(Value.PowerId))
			{
				Slots.push_back(Value);
			}
		}
		PlayerPowers.Restore(Slots);

		const JsonValue& HomesJson = Root.Get("homes");
		HomeAndFurniture::Snapshot HomeData;
		HomeData.Owned = ReadStrings(HomesJson.Get("owned"));
		HomeData.NextOrderId = HomesJson.GetInt("nextOrder", 1);
		HomeData.NextUid = HomesJson.GetInt("nextUid", 1);
		for (const JsonValue& Item : HomesJson.Get("orders").Items())
		{
			FurnitureOrder Order;
			Order.Id = Item.GetInt("id");
			Order.PropertyId = Item.GetString("property");
			Order.Items = ReadStrings(Item.Get("items"));
			Order.Subtotal = ReadCash(Item, "subtotal");
			Order.DeliveryFee = ReadCash(Item, "fee");
			Order.Speed = static_cast<DeliverySpeed>(Clamp(Item.GetInt("speed"), 0, 1));
			Order.PlacedMinute = Item.GetInt64("placed");
			Order.DispatchMinute = Item.GetInt64("dispatch");
			Order.DeliverMinute = Item.GetInt64("deliver");
			Order.Status = static_cast<OrderStatus>(Clamp(Item.GetInt("status"), 0, 3));
			HomeData.Orders.push_back(Order);
		}
		for (const JsonValue& Item : HomesJson.Get("inventory").Items())
		{
			HomeData.Inventory.push_back(InventoryEntry{Item.GetString("property"), Item.GetString("item")});
		}
		for (const JsonValue& Item : HomesJson.Get("placed").Items())
		{
			PlacedFurniture Piece;
			Piece.Uid = Item.GetInt("uid");
			Piece.ItemId = Item.GetString("item");
			Piece.PropertyId = Item.GetString("property");
			Piece.X = Item.GetNumber("x");
			Piece.Y = Item.GetNumber("y");
			Piece.YawDeg = Item.GetNumber("yaw");
			HomeData.Placed.push_back(Piece);
		}
		Homes.Load(HomeData);

		Garage.clear();
		for (const JsonValue& Item : Root.Get("garage").Items())
		{
			OwnedVehicle Car;
			Car.Uid = Item.GetString("uid");
			Car.ModelId = Item.GetString("model");
			Car.Condition = Item.GetNumber("condition", 1.0);
			Car.MileageKm = Item.GetNumber("km", 0.0);
			Car.Issues = ReadIssues(Item.Get("issues"));
			Car.Build.Parts = ReadStrings(Item.Get("parts"));
			Car.Build.Tuning = ReadTuning(Item.Get("tuning"));
			Car.Build.PaintRgb = static_cast<std::uint32_t>(Item.GetInt64("paint", 0xB8B8B8));
			Car.Build.PaintFinish = Item.GetInt("finish", 0);
			Car.Build.WheelRgb = static_cast<std::uint32_t>(Item.GetInt64("wheels", 0x303030));
			Car.Restoration = static_cast<RestorationLevel>(Clamp(Item.GetInt("restoration"), 0, static_cast<int>(RestorationLevel::Count) - 1));
			Car.bCertified = Item.GetBool("certified");
			Car.WarrantyUntilMinute = Item.GetInt64("warranty");
			Car.ReadyMinute = Item.GetInt64("ready");
			if (Vehicles.FindModel(Car.ModelId))
			{
				Garage.push_back(Car);
			}
		}
		ActiveVehicleUid = Root.GetString("activeVehicle");
		bManualTransmission = Root.GetBool("manualTransmission");

		const JsonValue& Tools = Root.Get("hackTools");
		HackTools.ProbeKit = Tools.GetInt("probe");
		HackTools.GhostRelay = Tools.GetInt("ghost");
		HackTools.PatternEngine = Tools.GetInt("pattern");
		RefreshHackBoard();

		const JsonValue& MinistryJson = Root.Get("ministry");
		Ministry::Snapshot MinistryData;
		MinistryData.Name = MinistryJson.GetString("name");
		MinistryData.Members = MinistryJson.GetInt("members");
		MinistryData.Engagement = MinistryJson.GetNumber("engagement", 0.5);
		MinistryData.Integrity = MinistryJson.GetNumber("integrity", 1.0);
		MinistryData.Reputation = MinistryJson.GetNumber("reputation", 0.0);
		MinistryData.SalaryShare = MinistryJson.GetNumber("salaryShare", 0.25);
		MinistryData.Fund = ReadCash(MinistryJson, "fund");
		MinistryData.Ordination = MinistryJson.GetInt("ordination");
		for (std::size_t Index = 0; Index < MinistryData.Facilities.size(); ++Index)
		{
			MinistryData.Facilities[Index] = MinistryJson.Get("facilities").At(Index).AsInt();
		}
		MinistryData.LastTheme = MinistryJson.GetInt("lastTheme", -1);
		MinistryData.PendingVisitors = MinistryJson.GetInt("pending");
		MinistryData.WeeksRun = MinistryJson.GetInt("weeks");
		MinistryData.Doctrine = MinistryJson.GetNumber("doctrine", 1.0);
		PlayerMinistry.Load(MinistryData);

		// Businesses start from freshly generated content, then saved dynamic state is applied by id.
		const JsonValue& MarketJson = Root.Get("market");
		std::vector<Business> Businesses = Markets.Businesses();
		for (const JsonValue& Item : MarketJson.Get("businesses").Items())
		{
			const std::string Id = Item.GetString("id");
			Business* Target = nullptr;
			for (Business& Candidate : Businesses)
			{
				if (Candidate.Id == Id) Target = &Candidate;
			}
			if (!Target)
			{
				if (Id != PlayerMinistryId)
				{
					continue;
				}
				Business Mirror;
				Mirror.Id = Id;
				Mirror.Sector = "Faith";
				Businesses.push_back(Mirror);
				Target = &Businesses.back();
			}
			Target->Name = Item.GetString("name", Target->Name);
			Target->Cash = ReadCash(Item, "cash");
			Target->Health = Item.GetNumber("health", Target->Health);
			Target->Problems.clear();
			for (const JsonValue& Problem : Item.Get("problems").Items())
			{
				const int Index = Problem.AsInt(-1);
				if (Index >= 0 && Index < static_cast<int>(ProblemKind::Count)) Target->Problems.push_back(static_cast<ProblemKind>(Index));
			}
			Target->Integrity = Item.GetNumber("integrity", Target->Integrity);
			Target->Doctrine = Item.GetNumber("doctrine", Target->Doctrine);
			Target->PlayerEquity = Item.GetNumber("equity", 0.0);
			Target->PriceLevel = Item.GetNumber("price", 1.0);
			Target->RivalPrice = Item.GetNumber("rival", 1.0);
			Target->bDissolved = Item.GetBool("dissolved", false);
			ReadRep(Item.Get("rep"), Target->Rep);
		}
		std::vector<Stock> Stocks = Markets.Stocks();
		for (const JsonValue& Item : MarketJson.Get("stocks").Items())
		{
			for (Stock& Candidate : Stocks)
			{
				if (Candidate.Ticker == Item.GetString("ticker"))
				{
					Candidate.Price = Item.GetNumber("price", Candidate.Price);
					Candidate.PreviousClose = Item.GetNumber("prev", Candidate.PreviousClose);
					Candidate.PendingImpact = Item.GetNumber("pending", 0.0);
					Candidate.History.clear();
					for (const JsonValue& Close : Item.Get("history").Items()) Candidate.History.push_back(Close.AsNumber());
				}
			}
		}
		std::vector<Holding> Holdings;
		for (const JsonValue& Item : MarketJson.Get("holdings").Items())
		{
			Holdings.push_back(Holding{Item.GetString("ticker"), Item.GetInt64("shares"), Item.GetNumber("cost")});
		}
		std::vector<NewsItem> News;
		for (const JsonValue& Item : MarketJson.Get("news").Items())
		{
			NewsItem Value;
			Value.Headline = Item.GetString("headline");
			Value.EntityId = Item.GetString("entity");
			Value.Impact = Item.GetNumber("impact");
			Value.Minute = Item.GetInt64("minute");
			News.push_back(Value);
		}
		Reputation PlayerRep;
		ReadRep(MarketJson.Get("playerRep"), PlayerRep);
		Markets.Restore(Businesses, Stocks, Holdings, News, PlayerRep);

		std::vector<CreativeWork> Works;
		for (const JsonValue& Item : Root.Get("works").Items())
		{
			CreativeWork Work;
			Work.Id = Item.GetString("id");
			Work.Title = Item.GetString("title");
			Work.Kind = static_cast<WorkKind>(Clamp(Item.GetInt("kind"), 0, static_cast<int>(WorkKind::Count) - 1));
			Work.Quality = Item.GetNumber("quality", 0.5);
			Work.Hype = Item.GetNumber("hype", 1.0);
			Work.WeeksOut = Item.GetInt("weeks");
			Work.LifetimeRoyalties = ReadCash(Item, "royalties");
			Work.bProtected = Item.GetBool("protected");
			Work.Licensee = Item.GetString("licensee");
			Work.LicenseeShare = Item.GetNumber("licenseeShare");
			Works.push_back(Work);
		}
		Ip.Restore(Works);

		Npcs = NpcPopulation();
		for (const JsonValue& Item : Root.Get("minds").Items())
		{
			NpcMind& Mind = Npcs.GetOrCreate(Item.GetString("id"));
			Mind.Awareness = Item.GetNumber("awareness");
			Mind.Stage = static_cast<AwarenessStage>(Clamp(Item.GetInt("stage"), 0, static_cast<int>(AwarenessStage::Count) - 1));
			Mind.Affinity = Item.GetNumber("affinity");
			for (std::size_t Index = 0; Index < Mind.SkillXp.size(); ++Index) Mind.SkillXp[Index] = Item.Get("skills").At(Index).AsInt();
			for (std::size_t Index = 0; Index < Mind.Seen.size(); ++Index) Mind.Seen[Index] = Item.Get("seen").At(Index).AsInt();
			Mind.Goal = Item.GetString("goal");
			Mind.GoalCareer = Item.GetString("goalCareer");
			Mind.CareerId = Item.GetString("career");
			Mind.bCompanion = Item.GetBool("companion");
			Mind.PowerId = Item.GetString("power");
			Mind.Side = Item.GetInt("side") == 1 ? Alignment::Villain : Alignment::Hero;
			Mind.Conversations = Item.GetInt("conversations");
			Mind.Deal.Strategy = static_cast<RepeatedStrategy>(Clamp(Item.GetInt("dealStrategy"), 0, static_cast<int>(RepeatedStrategy::Count) - 1));
			Mind.Deal.LastOpponentMove = Item.GetInt("dealLast", -1);
			Mind.Deal.bTriggered = Item.GetBool("dealTriggered");
			Mind.Deal.Rounds = Item.GetInt("dealRounds");
			Mind.Deal.OpponentDefections = Item.GetInt("dealDefections");
		}

		const JsonValue& PoliticsJson = Root.Get("politics");
		PoliticalCareer::Snapshot PoliticsData;
		PoliticsData.CurrentOffice = static_cast<Office>(Clamp(PoliticsJson.GetInt("office"), 0, static_cast<int>(Office::Count) - 1));
		PoliticsData.HighestOffice = static_cast<Office>(Clamp(PoliticsJson.GetInt("highest"), 0, static_cast<int>(Office::Count) - 1));
		PoliticsData.Approval = PoliticsJson.GetNumber("approval", 0.5);
		PoliticsData.TermEndMinute = PoliticsJson.GetInt64("termEnds");
		PoliticsData.Laws = ReadStrings(PoliticsJson.Get("laws"));
		PoliticsData.Wins = PoliticsJson.GetInt("wins");
		PoliticsData.Losses = PoliticsJson.GetInt("losses");
		const JsonValue& RunJson = PoliticsJson.Get("campaign");
		PoliticsData.Run.bActive = RunJson.GetBool("active");
		PoliticsData.Run.Target = static_cast<Office>(Clamp(RunJson.GetInt("target"), 0, static_cast<int>(Office::Count) - 1));
		PoliticsData.Run.Support = RunJson.GetNumber("support", 0.3);
		PoliticsData.Run.OpponentSupport = RunJson.GetNumber("opponentSupport", 0.5);
		PoliticsData.Run.Opponent = RunJson.GetString("opponent");
		PoliticsData.Run.Spent = ReadCash(RunJson, "spent");
		PoliticsData.Run.Rallies = RunJson.GetInt("rallies");
		PoliticsData.Run.Debates = RunJson.GetInt("debates");
		PoliticsData.Run.Endorsements = RunJson.GetInt("endorsements");
		PoliticsData.Run.ElectionMinute = RunJson.GetInt64("election");
		Politics.Load(PoliticsData);

		UsedCars = UsedCarMarket();
		std::vector<UsedListing> Lot;
		for (const JsonValue& Item : Root.Get("usedCars").Items())
		{
			UsedListing Listing;
			Listing.Id = Item.GetString("id");
			Listing.ModelId = Item.GetString("model");
			if (!Vehicles.FindModel(Listing.ModelId))
			{
				continue; // the catalog changed since this save
			}
			Listing.Condition = Item.GetNumber("condition", 0.6);
			Listing.MileageKm = Item.GetNumber("km", 100000.0);
			Listing.Issues = ReadIssues(Item.Get("issues"));
			Listing.AskingPrice = ReadCash(Item, "asking");
			Listing.Seller = Item.GetString("seller");
			Listing.bBarnFind = Item.GetBool("barnFind");
			Listing.Rounds = Item.GetInt("rounds");
			Listing.Dealer = static_cast<DealerKind>(Clamp(Item.GetInt("dealer"), 0, static_cast<int>(DealerKind::Count) - 1));
			Listing.ShowroomId = Item.GetString("showroom");
			Listing.Restoration = static_cast<RestorationLevel>(Clamp(Item.GetInt("restoration"), 0, static_cast<int>(RestorationLevel::Count) - 1));
			Listing.Parts = ReadStrings(Item.Get("parts"));
			Listing.bNoHaggle = Item.GetBool("noHaggle");
			Lot.push_back(Listing);
		}
		if (Lot.empty())
		{
			RefreshUsedCars();
			RefreshDealerStock();
		}
		else
		{
			UsedCars.Restore(std::move(Lot));
		}
		Wanted = WantedSystem();
		PendingNotifications.clear();
		// Restore the random stream last so anything regenerated above does not disturb it.
		Random.Restore(std::strtoull(Root.GetString("rngState", "1").c_str(), nullptr, 10), std::strtoull(Root.GetString("rngInc", "1").c_str(), nullptr, 10));
		return true;
	}
}
