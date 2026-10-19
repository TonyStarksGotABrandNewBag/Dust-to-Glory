#include "Sim/GameState.h"

#include <cstdio>

namespace dtg::sim
{
	namespace detail_state
	{
		/** A stable fictional phone number for an NPC (555-01xx is reserved for fiction). */
		std::string PhoneFor(const std::string& Id)
		{
			char Number[32];
			std::snprintf(Number, sizeof(Number), "(843) 555-01%02u", static_cast<unsigned>(HashString(Id) % 100u));
			return Number;
		}

		/** A fictional email address on a reserved example domain. */
		std::string EmailFor(const std::string& Name)
		{
			std::string Local = Slugify(Name);
			for (char& Letter : Local)
			{
				if (Letter == '-') Letter = '.';
			}
			return Local + "@mail.example";
		}

		std::uint32_t ParseColor(const std::string& Text, std::uint32_t Fallback)
		{
			if (Text.size() != 7 || Text[0] != '#')
			{
				return Fallback;
			}
			std::uint32_t Value = 0;
			for (std::size_t Index = 1; Index < 7; ++Index)
			{
				const char Character = Text[Index];
				Value <<= 4u;
				if (Character >= '0' && Character <= '9') Value |= static_cast<std::uint32_t>(Character - '0');
				else if (Character >= 'a' && Character <= 'f') Value |= static_cast<std::uint32_t>(Character - 'a' + 10);
				else if (Character >= 'A' && Character <= 'F') Value |= static_cast<std::uint32_t>(Character - 'A' + 10);
				else return Fallback;
			}
			return Value;
		}

		std::vector<std::string> ReadStrings(const JsonValue& Array)
		{
			std::vector<std::string> Values;
			for (const JsonValue& Item : Array.Items())
			{
				Values.push_back(Item.AsString());
			}
			return Values;
		}

		struct DoubleField
		{
			const char* Key;
			double PartEffects::*Member;
		};

		const DoubleField EffectDoubles[] = {
			{"powerMul", &PartEffects::PowerMul},
			{"torqueMul", &PartEffects::TorqueMul},
			{"redlineAdd", &PartEffects::RedlineAdd},
			{"rpmShift", &PartEffects::RpmShift},
			{"camLope", &PartEffects::CamLope},
			{"inertiaMul", &PartEffects::InertiaMul},
			{"boostBar", &PartEffects::BoostBar},
			{"spoolSeconds", &PartEffects::SpoolSeconds},
			{"boostThresholdRpm", &PartEffects::BoostThresholdRpm},
			{"swapLitres", &PartEffects::SwapLitres},
			{"swapPowerKw", &PartEffects::SwapPowerKw},
			{"swapTorqueNm", &PartEffects::SwapTorqueNm},
			{"swapPowerRpm", &PartEffects::SwapPowerRpm},
			{"swapRedline", &PartEffects::SwapRedline},
			{"swapMassKg", &PartEffects::SwapMassKg},
			{"shiftTimeMul", &PartEffects::ShiftTimeMul},
			{"clutchCapacityMul", &PartEffects::ClutchCapacityMul},
			{"massKg", &PartEffects::MassKg},
			{"massMul", &PartEffects::MassMul},
			{"springMul", &PartEffects::SpringMul},
			{"rideHeightAddM", &PartEffects::RideHeightAddM},
			{"brakeTorqueMul", &PartEffects::BrakeTorqueMul},
			{"tireMu", &PartEffects::TireMu},
			{"tireStiffnessMul", &PartEffects::TireStiffnessMul},
			{"wheelRadiusAddM", &PartEffects::WheelRadiusAddM},
			{"wheelInertiaMul", &PartEffects::WheelInertiaMul},
			{"trackAddM", &PartEffects::TrackAddM},
			{"chassisGripMul", &PartEffects::ChassisGripMul},
			{"steerLockAddDeg", &PartEffects::SteerLockAddDeg},
			{"dragMul", &PartEffects::DragMul},
			{"downforceFront", &PartEffects::DownforceFront},
			{"downforceRear", &PartEffects::DownforceRear},
			{"durabilityMul", &PartEffects::DurabilityMul},
			{"nitroFraction", &PartEffects::NitroFraction},
			{"nitroSeconds", &PartEffects::NitroSeconds},
			{"exhaustLoudness", &PartEffects::ExhaustLoudness},
			{"intakeLoudness", &PartEffects::IntakeLoudness},
			{"pops", &PartEffects::Pops},
		};

		struct BoolField
		{
			const char* Key;
			bool PartEffects::*Member;
		};

		const BoolField EffectBools[] = {
			{"addsTurbo", &PartEffects::bAddsTurbo},
			{"addsSupercharger", &PartEffects::bAddsSupercharger},
			{"antiLag", &PartEffects::bAntiLag},
			{"customRatios", &PartEffects::bCustomRatios},
			{"adjustableFinal", &PartEffects::bAdjustableFinal},
			{"straightCut", &PartEffects::bStraightCut},
			{"adjustableDiff", &PartEffects::bAdjustableDiff},
			{"adjustableSuspension", &PartEffects::bAdjustableSuspension},
			{"adjustableBrakeBias", &PartEffects::bAdjustableBrakeBias},
			{"adjustableAero", &PartEffects::bAdjustableAero},
		};

		const std::vector<std::string>& KnownEffectKeys()
		{
			static std::vector<std::string> Keys;
			if (Keys.empty())
			{
				for (const auto& Field : EffectDoubles) Keys.push_back(Field.Key);
				for (const auto& Field : EffectBools) Keys.push_back(Field.Key);
				Keys.push_back("swapLayout");
				Keys.push_back("swapAspiration");
				Keys.push_back("gearsOverride");
				Keys.push_back("diff");
				Keys.push_back("exhaustTone");
			}
			return Keys;
		}

		std::string SectorForRole(LotRole Role)
		{
			switch (Role)
			{
			case LotRole::Diner: return "Restaurant";
			case LotRole::Office: return "Finance";
			case LotRole::TechCampus: return "Tech";
			case LotRole::Studio: return "Media";
			case LotRole::Dealership:
			case LotRole::ModShop: return "Auto";
			case LotRole::Church: return "Faith";
			case LotRole::Bank: return "Finance";
			case LotRole::Hotel: return "Hospitality";
			default: return "Retail";
			}
		}

		double MonthlyRevenueForRole(LotRole Role)
		{
			switch (Role)
			{
			case LotRole::Shop: return 25000.0;
			case LotRole::Diner: return 40000.0;
			case LotRole::Office: return 150000.0;
			case LotRole::Hotel: return 120000.0;
			case LotRole::FurnitureStore: return 80000.0;
			case LotRole::ModShop: return 60000.0;
			case LotRole::ElectronicsStore: return 70000.0;
			case LotRole::Gym: return 30000.0;
			case LotRole::Studio: return 35000.0;
			case LotRole::Dealership: return 400000.0;
			case LotRole::GardenCenter: return 30000.0;
			case LotRole::Church: return 20000.0;
			case LotRole::Museum: return 30000.0;
			case LotRole::RealEstate: return 45000.0;
			default: return 0.0;
			}
		}

	}

	// ==================================================================================== Loaders

	bool LoadEconomyJson(const JsonValue& Root, EconomyIndex& Out, std::vector<std::string>& Errors)
	{
		if (!Root.IsObject())
		{
			Errors.push_back("economy.json: root must be an object");
			return false;
		}
		Out.Era = Root.GetString("era", Out.Era);
		Out.CalendarYear = Root.GetInt("calendarYear", Out.CalendarYear);
		Out.Vehicles = Root.GetNumber("vehicles", Out.Vehicles);
		Out.Housing = Root.GetNumber("housing", Out.Housing);
		Out.Goods = Root.GetNumber("goods", Out.Goods);
		Out.Wages = Root.GetNumber("wages", Out.Wages);
		return true;
	}

	bool LoadVehiclesJson(const JsonValue& Root, VehicleCatalog& Out, std::vector<std::string>& Errors)
	{
		const JsonValue& Brands = Root.Get("brands");
		if (!Brands.IsArray() || Brands.Size() == 0)
		{
			Errors.push_back("vehicles.json: 'brands' must be a non-empty array");
			return false;
		}
		for (const JsonValue& BrandJson : Brands.Items())
		{
			Brand Maker;
			Maker.Id = BrandJson.GetString("id");
			Maker.Name = BrandJson.GetString("name");
			if (Maker.Id.empty() || Maker.Name.empty())
			{
				Errors.push_back("vehicles.json: every brand needs an id and a name");
				continue;
			}
			Maker.Country = BrandJson.GetString("country");
			Maker.Tagline = BrandJson.GetString("tagline");
			Maker.Founded = BrandJson.GetInt("founded", 1950);
			Maker.Defunct = BrandJson.GetInt("defunct", 0);
			Maker.Prestige = BrandJson.GetNumber("prestige", 0.5);
			Maker.PerformanceBias = BrandJson.GetNumber("performance", 0.0);
			Maker.ColorRgb = detail_state::ParseColor(BrandJson.GetString("color"), 0x8090A0);
			Maker.SignatureEngine = BrandJson.GetString("signatureEngine");
			Maker.DealerGroup = BrandJson.GetString("dealerGroup");
			Maker.bDealer = BrandJson.GetBool("dealer", true);
			if (!Maker.SignatureEngine.empty())
			{
				EngineLayout Ignored = EngineLayout::I4;
				if (!ParseEngineLayout(Maker.SignatureEngine, Ignored))
				{
					Errors.push_back("vehicles.json: brand " + Maker.Id + " has unknown signatureEngine " + Maker.SignatureEngine);
				}
			}
			for (const JsonValue& LineJson : BrandJson.Get("nameplates").Items())
			{
				Nameplate Line;
				Line.BrandId = Maker.Id;
				Line.Name = LineJson.GetString("name");
				if (!ParseBodyStyle(LineJson.GetString("body"), Line.Body))
				{
					Errors.push_back("vehicles.json: " + Maker.Id + " " + Line.Name + " has unknown body " + LineJson.GetString("body"));
				}
				if (!ParseDrivetrain(LineJson.GetString("drive", "RWD"), Line.Drive))
				{
					Errors.push_back("vehicles.json: " + Maker.Id + " " + Line.Name + " has unknown drive");
				}
				// "gens": launch years, or [start, end] pairs where production paused before the next generation.
				for (const JsonValue& Entry : LineJson.Get("gens").Items())
				{
					if (Entry.IsArray() && Entry.Size() >= 2)
					{
						Line.GenerationStarts.push_back(Entry.At(0).AsInt());
						Line.GenerationEnds.push_back(Entry.At(1).AsInt());
					}
					else
					{
						Line.GenerationStarts.push_back(Entry.AsInt());
						Line.GenerationEnds.push_back(0);
					}
				}
				Line.FirstYear = LineJson.GetInt("first", Line.GenerationStarts.empty() ? 1970 : Line.GenerationStarts.front());
				Line.LastYear = LineJson.GetInt("last", 0);
				Line.GenerationYears = LineJson.GetInt("genYears", 7);
				Line.Tier = LineJson.GetNumber("tier", 0.5);
				Line.ElectricFrom = LineJson.GetInt("electricFrom", 0);
				Line.bEventOnly = LineJson.GetBool("eventOnly", false);
				Line.EngineHint = LineJson.GetString("engine");
				Line.AspirationHint = LineJson.GetString("aspiration");
				Line.PositionHint = LineJson.GetString("position");
				if (Line.Name.empty())
				{
					Errors.push_back("vehicles.json: nameplate without a name in " + Maker.Id);
					continue;
				}
				Out.AddNameplate(Line);
			}
			Out.AddBrand(Maker);
		}
		return true;
	}

	bool LoadPartsJson(const JsonValue& Root, PartCatalog& Out, std::vector<std::string>& Errors)
	{
		const JsonValue& Parts = Root.Get("parts");
		if (!Parts.IsArray())
		{
			Errors.push_back("parts.json: 'parts' must be an array");
			return false;
		}
		for (const JsonValue& PartJson : Parts.Items())
		{
			PartDef Part;
			Part.Id = PartJson.GetString("id");
			Part.Name = PartJson.GetString("name");
			Part.Maker = PartJson.GetString("maker");
			Part.Description = PartJson.GetString("description");
			if (Part.Id.empty() || !ParsePartCategory(PartJson.GetString("category"), Part.Category))
			{
				Errors.push_back("parts.json: part '" + Part.Id + "' needs an id and a valid category");
				continue;
			}
			Part.Tier = PartJson.GetInt("tier", 1);
			Part.PriceBase = PartJson.GetNumber("price", 0.0);
			Part.PricePercentOfCar = PartJson.GetNumber("pricePct", 0.0);
			Part.bIceOnly = PartJson.GetBool("iceOnly", false);
			Part.bEvOnly = PartJson.GetBool("evOnly", false);
			Part.Visual = PartJson.GetString("visual");
			for (const JsonValue& BodyJson : PartJson.Get("bodies").Items())
			{
				BodyStyle Body = BodyStyle::Sedan;
				if (ParseBodyStyle(BodyJson.AsString(), Body))
				{
					Part.Bodies.push_back(Body);
				}
				else
				{
					Errors.push_back("parts.json: " + Part.Id + " lists unknown body " + BodyJson.AsString());
				}
			}
			const JsonValue& Fx = PartJson.Get("effects");
			for (const auto& Member : Fx.Members())
			{
				bool bKnown = false;
				for (const std::string& Key : detail_state::KnownEffectKeys())
				{
					bKnown = bKnown || Key == Member.first;
				}
				if (!bKnown)
				{
					Errors.push_back("parts.json: " + Part.Id + " has unknown effect '" + Member.first + "'");
				}
			}
			for (const auto& Field : detail_state::EffectDoubles)
			{
				if (const JsonValue* Value = Fx.Find(Field.Key))
				{
					Part.Effects.*(Field.Member) = Value->AsNumber();
				}
			}
			for (const auto& Field : detail_state::EffectBools)
			{
				if (const JsonValue* Value = Fx.Find(Field.Key))
				{
					Part.Effects.*(Field.Member) = Value->AsBool();
				}
			}
			Part.Effects.SwapLayout = Fx.GetString("swapLayout");
			Part.Effects.SwapAspiration = Fx.GetString("swapAspiration");
			Part.Effects.GearsOverride = Fx.GetInt("gearsOverride", 0);
			Part.Effects.ExhaustTone = Fx.GetInt("exhaustTone", -1);
			const std::string Diff = ToLowerAscii(Fx.GetString("diff"));
			if (!Diff.empty())
			{
				Part.Effects.bSetsDiff = true;
				if (Diff == "open") Part.Effects.Diff = DiffType::Open;
				else if (Diff == "lsd" || Diff == "limitedslip") Part.Effects.Diff = DiffType::LimitedSlip;
				else if (Diff == "locked" || Diff == "spool") Part.Effects.Diff = DiffType::Locked;
				else Errors.push_back("parts.json: " + Part.Id + " has unknown diff '" + Diff + "'");
			}
			Out.Add(Part);
		}
		return true;
	}

	bool LoadCareersJson(const JsonValue& Root, CareerBook& Out, std::vector<std::string>& Errors)
	{
		for (const JsonValue& EduJson : Root.Get("educations").Items())
		{
			EducationDef Edu;
			Edu.Id = EduJson.GetString("id");
			Edu.Name = EduJson.GetString("name");
			Edu.Description = EduJson.GetString("description");
			Edu.Tuition = ToMoney(EduJson.GetNumber("tuition", 0.0));
			Edu.StudyHours = EduJson.GetInt("hours", 20);
			Edu.Prerequisite = EduJson.GetString("prerequisite");
			if (!ParseSkill(EduJson.GetString("skill", "Science"), Edu.TrainsSkill))
			{
				Errors.push_back("careers.json: education " + Edu.Id + " has unknown skill");
			}
			if (Edu.Id.empty())
			{
				Errors.push_back("careers.json: education without an id");
				continue;
			}
			Out.AddEducation(Edu);
		}
		const JsonValue& Careers = Root.Get("careers");
		if (!Careers.IsArray() || Careers.Size() == 0)
		{
			Errors.push_back("careers.json: 'careers' must be a non-empty array");
			return false;
		}
		for (const JsonValue& CareerJson : Careers.Items())
		{
			CareerDef Def;
			Def.Id = CareerJson.GetString("id");
			Def.Name = CareerJson.GetString("name");
			Def.Category = CareerJson.GetString("category");
			Def.Description = CareerJson.GetString("description");
			Def.Workplace = CareerJson.GetString("workplace");
			Def.Education = CareerJson.GetString("education");
			Def.bCleanRecord = CareerJson.GetBool("cleanRecord", false);
			Def.RequiredSkillLevel = CareerJson.GetInt("requiredLevel", 0);
			Def.ShiftHours = CareerJson.GetInt("shiftHours", 8);
			if (!ParseSkill(CareerJson.GetString("skill", "People"), Def.PrimarySkill))
			{
				Errors.push_back("careers.json: career " + Def.Id + " has unknown skill");
			}
			if (!ParseMinigame(CareerJson.GetString("minigame", "SkillCheck"), Def.Minigame))
			{
				Errors.push_back("careers.json: career " + Def.Id + " has unknown minigame");
			}
			LotRole Workplace = LotRole::Office;
			if (!ParseLotRole(Def.Workplace, Workplace))
			{
				Errors.push_back("careers.json: career " + Def.Id + " has unknown workplace " + Def.Workplace);
			}
			Def.Tasks = detail_state::ReadStrings(CareerJson.Get("tasks"));
			for (const JsonValue& LevelJson : CareerJson.Get("levels").Items())
			{
				CareerLevel Level;
				Level.Title = LevelJson.GetString("title");
				Level.HourlyPay = ToMoney(LevelJson.GetNumber("pay", 10.0));
				Level.XpToNext = LevelJson.GetInt("xp", 200);
				Def.Levels.push_back(Level);
			}
			if (Def.Id.empty() || Def.Levels.empty())
			{
				Errors.push_back("careers.json: career '" + Def.Id + "' needs an id and at least one level");
				continue;
			}
			Out.AddCareer(Def);
		}
		for (const CareerDef& Def : Out.Careers())
		{
			if (!Def.Education.empty() && !Out.FindEducation(Def.Education))
			{
				Errors.push_back("careers.json: career " + Def.Id + " requires missing education " + Def.Education);
			}
		}
		return true;
	}

	bool LoadPowersJson(const JsonValue& Root, PowerRoster& Out, std::vector<std::string>& Errors)
	{
		for (const JsonValue& MutationJson : Root.Get("mutations").Items())
		{
			MutationDef Mutation;
			Mutation.Id = MutationJson.GetString("id");
			Mutation.Name = MutationJson.GetString("name");
			Mutation.Description = MutationJson.GetString("description");
			Mutation.Weight = MutationJson.GetNumber("weight", 1.0);
			Mutation.bDrawback = MutationJson.GetBool("drawback", false);
			const JsonValue& Mods = MutationJson.Get("mods");
			Mutation.Mods.Potency = Mods.GetNumber("potency", 1.0);
			Mutation.Mods.Range = Mods.GetNumber("range", 1.0);
			Mutation.Mods.Cooldown = Mods.GetNumber("cooldown", 1.0);
			Mutation.Mods.EnergyCost = Mods.GetNumber("energy", 1.0);
			Mutation.Mods.Duration = Mods.GetNumber("duration", 1.0);
			Out.AddMutation(Mutation);
		}
		const JsonValue& Powers = Root.Get("powers");
		if (!Powers.IsArray() || Powers.Size() < 3)
		{
			Errors.push_back("powers.json: at least three powers are required");
			return false;
		}
		for (const JsonValue& PowerJson : Powers.Items())
		{
			PowerDef Def;
			Def.Id = PowerJson.GetString("id");
			Def.Name = PowerJson.GetString("name");
			Def.Archetype = PowerJson.GetString("archetype");
			Def.Description = PowerJson.GetString("description");
			if (!ParsePowerEffect(PowerJson.GetString("effect"), Def.Effect))
			{
				Errors.push_back("powers.json: power " + Def.Id + " has unknown effect " + PowerJson.GetString("effect"));
			}
			Def.BaseCooldownS = PowerJson.GetNumber("cooldown", 5.0);
			Def.BaseEnergy = PowerJson.GetNumber("energy", 20.0);
			Def.BaseRangeM = PowerJson.GetNumber("range", 20.0);
			Def.BaseDurationS = PowerJson.GetNumber("duration", 0.0);
			Def.BasePotency = PowerJson.GetNumber("potency", 1.0);
			Def.Mutations = detail_state::ReadStrings(PowerJson.Get("mutations"));
			Def.EvolvesTo = PowerJson.GetString("evolvesTo");
			Def.bStarter = PowerJson.GetBool("starter", true);
			Out.AddPower(Def);
		}
		for (const PowerDef& Def : Out.Powers())
		{
			for (const std::string& MutationId : Def.Mutations)
			{
				if (!Out.FindMutation(MutationId))
				{
					Errors.push_back("powers.json: power " + Def.Id + " references missing mutation " + MutationId);
				}
			}
			if (!Def.EvolvesTo.empty() && !Out.FindPower(Def.EvolvesTo))
			{
				Errors.push_back("powers.json: power " + Def.Id + " evolves into missing power " + Def.EvolvesTo);
			}
		}
		return true;
	}

	bool LoadFurnitureJson(const JsonValue& Root, HomeAndFurniture& Out, std::vector<std::string>& Errors)
	{
		const JsonValue& Items = Root.Get("items");
		if (!Items.IsArray() || Items.Size() == 0)
		{
			Errors.push_back("furniture.json: 'items' must be a non-empty array");
			return false;
		}
		for (const JsonValue& ItemJson : Items.Items())
		{
			FurnitureItem Item;
			Item.Id = ItemJson.GetString("id");
			Item.Name = ItemJson.GetString("name");
			Item.Store = ItemJson.GetString("store");
			Item.Category = ItemJson.GetString("category");
			Item.Price = ToMoney(ItemJson.GetNumber("price", 100.0));
			Item.WidthM = ItemJson.GetNumber("w", 1.0);
			Item.DepthM = ItemJson.GetNumber("d", 1.0);
			Item.HeightM = ItemJson.GetNumber("h", 1.0);
			Item.ColorRgb = detail_state::ParseColor(ItemJson.GetString("color"), 0x8B6F4E);
			if (Item.Id.empty() || Item.Store.empty())
			{
				Errors.push_back("furniture.json: item '" + Item.Id + "' needs an id and a store");
				continue;
			}
			Out.AddItem(Item);
		}
		return true;
	}

	bool LoadLawsJson(const JsonValue& Root, std::vector<LawDef>& Out, std::vector<std::string>& Errors)
	{
		const JsonValue& Laws = Root.Get("laws");
		if (!Laws.IsArray())
		{
			Errors.push_back("laws.json: 'laws' must be an array");
			return false;
		}
		struct OfficeName
		{
			const char* Key;
			Office Value;
		};
		const OfficeName Offices[] = {
			{"citycouncil", Office::CityCouncil}, {"mayor", Office::Mayor}, {"staterepresentative", Office::StateRepresentative},
			{"governor", Office::Governor}, {"senator", Office::Senator}, {"president", Office::President}};
		for (const JsonValue& LawJson : Laws.Items())
		{
			LawDef Law;
			Law.Id = LawJson.GetString("id");
			Law.Name = LawJson.GetString("name");
			Law.Description = LawJson.GetString("description");
			Law.Popularity = LawJson.GetNumber("popularity", 0.0);
			const std::string OfficeKey = ToLowerAscii(LawJson.GetString("minimumOffice", "CityCouncil"));
			bool bFound = false;
			for (const OfficeName& Entry : Offices)
			{
				if (OfficeKey == Entry.Key)
				{
					Law.MinimumOffice = Entry.Value;
					bFound = true;
				}
			}
			if (!bFound)
			{
				Errors.push_back("laws.json: law " + Law.Id + " has unknown minimumOffice");
			}
			const JsonValue& Fx = LawJson.Get("effects");
			Law.Effects.SpeedingHeatMul = Fx.GetNumber("speedingHeatMul", 1.0);
			Law.Effects.PoliceUnitsMul = Fx.GetNumber("policeUnitsMul", 1.0);
			Law.Effects.BusinessHealthBonus = Fx.GetNumber("businessHealthBonus", 0.0);
			Law.Effects.WageMul = Fx.GetNumber("wageMul", 1.0);
			Law.Effects.TrafficDensityMul = Fx.GetNumber("trafficDensityMul", 1.0);
			Law.Effects.HousingPriceMul = Fx.GetNumber("housingPriceMul", 1.0);
			Law.Effects.VehiclePriceMul = Fx.GetNumber("vehiclePriceMul", 1.0);
			Law.Effects.HackingHeatMul = Fx.GetNumber("hackingHeatMul", 1.0);
			Law.Effects.TuitionMul = Fx.GetNumber("tuitionMul", 1.0);
			Law.Effects.bStreetRacingLegal = Fx.GetBool("streetRacingLegal", false);
			if (Law.Id.empty() || Law.Name.empty())
			{
				Errors.push_back("laws.json: every law needs an id and a name");
				continue;
			}
			Out.push_back(Law);
		}
		return true;
	}

	// ==================================================================================== Content

	bool GameState::LoadContent(const ContentFiles& Files, ContentReport& Report)
	{
		struct Source
		{
			const std::string* Text;
			const char* Name;
			JsonValue Parsed;
		};
		Source Sources[] = {
			{&Files.Economy, "economy.json", {}},
			{&Files.Vehicles, "vehicles.json", {}},
			{&Files.Parts, "parts.json", {}},
			{&Files.Careers, "careers.json", {}},
			{&Files.Powers, "powers.json", {}},
			{&Files.Furniture, "furniture.json", {}},
			{&Files.Companies, "companies.json", {}},
			{&Files.Laws, "laws.json", {}},
			{&Files.State, "state.json", {}},
		};
		for (Source& Entry : Sources)
		{
			if (Entry.Text == &Files.State && Files.State.empty())
			{
				continue; // optional: no state map means a single city
			}
			JsonParseResult Result = ParseJson(*Entry.Text);
			if (!Result.bOk)
			{
				char Buffer[256];
				std::snprintf(Buffer, sizeof(Buffer), "%s: %s at line %zu, column %zu", Entry.Name, Result.Error.c_str(), Result.Line, Result.Column);
				Report.Errors.push_back(Buffer);
			}
			Entry.Parsed = std::move(Result.Value);
		}
		if (!Report.Ok())
		{
			return false;
		}

		Economy = EconomyIndex();
		Vehicles = VehicleCatalog();
		Parts = PartCatalog();
		Careers = CareerBook();
		Powers = PowerRoster();
		Homes = HomeAndFurniture();

		LoadEconomyJson(Sources[0].Parsed, Economy, Report.Errors);
		LoadVehiclesJson(Sources[1].Parsed, Vehicles, Report.Errors);
		LoadPartsJson(Sources[2].Parsed, Parts, Report.Errors);
		LoadCareersJson(Sources[3].Parsed, Careers, Report.Errors);
		LoadPowersJson(Sources[4].Parsed, Powers, Report.Errors);
		LoadFurnitureJson(Sources[5].Parsed, Homes, Report.Errors);
		CompaniesData = Sources[6].Parsed;
		LawBook.clear();
		LoadLawsJson(Sources[7].Parsed, LawBook, Report.Errors);

		const int CatalogYear = Sources[0].Parsed.GetInt("catalogYear", 2026);
		Vehicles.Build(CatalogYear, Economy.Vehicles);

		CityConfig Config;
		Config.Seed = WorldSeed;
		// One showroom per dealer group (sister brands share a building); defunct brands have none.
		for (const Brand& Maker : Vehicles.Brands())
		{
			if (!Maker.HasShowroom(Vehicles.CurrentYear()))
			{
				continue;
			}
			bool bMerged = false;
			for (auto& Entry : Config.Brands)
			{
				if (Entry.first == Maker.ShowroomId())
				{
					Entry.second += " & " + Maker.Name;
					bMerged = true;
				}
			}
			if (!bMerged)
			{
				Config.Brands.emplace_back(Maker.ShowroomId(), Maker.Name);
			}
		}
		Config.FurnitureStores = Homes.Stores();
		for (const CareerDef& Def : Careers.Careers())
		{
			Config.RequiredWorkplaces.push_back(Def.Workplace);
		}
		Map = StatePlan();
		bStateMap = !Files.State.empty();
		if (bStateMap)
		{
			std::vector<std::string> MapErrors;
			Map.Load(Sources[8].Parsed, MapErrors);
			for (const std::string& Error : MapErrors)
			{
				Report.Errors.push_back(Error);
			}
			City = Map.BuildWorld(Config);
		}
		else
		{
			City = GenerateCity(Config);
		}
		for (const BuildingPlan& Building : City.Buildings)
		{
			if (IsResidential(Building.Role))
			{
				Property Home;
				Home.Id = PropertyIdFor(Building.Id);
				Home.Name = Building.Name;
				Home.Kind = Building.Role == LotRole::Home ? "House" : "Apartment";
				Home.Price = static_cast<Money>(std::llround(static_cast<double>(Building.Price) * Economy.Housing));
				Home.Bedrooms = Building.Role == LotRole::Home ? 3 : 2;
				Home.InteriorWidthM = Building.Footprint.Width() - 1.0;
				Home.InteriorDepthM = Building.Footprint.Depth() - 1.0;
				Homes.AddProperty(Home);
			}
		}

		Markets = Market();
		BuildBusinesses(CompaniesData, Report);

		Report.Brands = static_cast<int>(Vehicles.Brands().size());
		Report.Nameplates = static_cast<int>(Vehicles.Nameplates().size());
		Report.Models = static_cast<int>(Vehicles.Models().size());
		Report.Parts = static_cast<int>(Parts.All().size());
		Report.Careers = static_cast<int>(Careers.Careers().size());
		Report.Educations = static_cast<int>(Careers.Educations().size());
		Report.Powers = static_cast<int>(Powers.Powers().size());
		Report.Mutations = static_cast<int>(Powers.Mutations().size());
		Report.FurnitureItems = static_cast<int>(Homes.Items().size());
		Report.Businesses = static_cast<int>(Markets.Businesses().size());
		Report.Buildings = static_cast<int>(City.Buildings.size());
		Report.Laws = static_cast<int>(LawBook.size());
		return Report.Ok();
	}

	void GameState::BuildBusinesses(const JsonValue& Companies, ContentReport& Report)
	{
		Rng Local(WorldSeed ^ 0xB0B0B0ULL);
		// Public companies first; an office tower named after one becomes its headquarters.
		for (const JsonValue& CompanyJson : Companies.Get("public").Items())
		{
			Business Value;
			Value.Id = CompanyJson.GetString("id");
			Value.Name = CompanyJson.GetString("name");
			Value.Sector = CompanyJson.GetString("sector", "Finance");
			Value.BaseRevenue = ToMoney(CompanyJson.GetNumber("revenue", 5000000.0));
			Value.BaseCosts = ToMoney(CompanyJson.GetNumber("costs", 4500000.0));
			Value.Cash = Value.BaseRevenue;
			Value.Integrity = CompanyJson.GetNumber("integrity", 0.8);
			Value.Health = CompanyJson.GetNumber("health", 0.7);
			Value.Rep.Rating = CompanyJson.GetNumber("rating", 3.8);
			Value.Rep.Followers = CompanyJson.GetInt("followers", 50000);
			if (Value.Id.empty() || Value.Name.empty())
			{
				Report.Errors.push_back("companies.json: public company needs an id and a name");
				continue;
			}
			for (const BuildingPlan& Building : City.Buildings)
			{
				if (Building.Role == LotRole::Office && Building.Name.rfind(Value.Name, 0) == 0 && !Markets.FindByBuilding(Building.Id))
				{
					Value.BuildingId = Building.Id;
					break;
				}
			}
			Markets.AddBusiness(Value);
			Markets.ListStock(Value.Id, CompanyJson.GetString("ticker"), CompanyJson.GetNumber("price", 20.0),
				CompanyJson.GetInt64("shares", 10000000), CompanyJson.GetNumber("volatility", 0.02));
		}
		// Rival ministries across the region.
		for (const JsonValue& MinistryJson : Companies.Get("ministries").Items())
		{
			Business Value;
			Value.Id = MinistryJson.GetString("id");
			Value.Name = MinistryJson.GetString("name");
			Value.Sector = "Faith";
			Value.BaseRevenue = ToMoney(MinistryJson.GetNumber("giving", 20000.0));
			Value.BaseCosts = static_cast<Money>(static_cast<double>(Value.BaseRevenue) * 0.9);
			Value.Integrity = MinistryJson.GetNumber("integrity", 0.8);
			Value.Doctrine = MinistryJson.GetNumber("doctrine", 0.9);
			Value.Rep.Followers = MinistryJson.GetInt("followers", 400);
			Value.Rep.Trust = MinistryJson.GetNumber("trust", 0.75);
			const std::string Building = MinistryJson.GetString("building");
			if (Building == "Church")
			{
				if (const BuildingPlan* Church = City.FirstWithRole(LotRole::Church))
				{
					Value.BuildingId = Church->Id;
				}
			}
			Markets.AddBusiness(Value);
		}
		// Every commercial building in the city is a private business.
		for (const BuildingPlan& Building : City.Buildings)
		{
			const double Revenue = detail_state::MonthlyRevenueForRole(Building.Role);
			if (Revenue <= 0.0 || Markets.FindByBuilding(Building.Id))
			{
				continue;
			}
			Business Value;
			Value.Id = "biz-" + std::to_string(Building.Id);
			Value.Name = Building.Name;
			Value.Sector = detail_state::SectorForRole(Building.Role);
			Value.BuildingId = Building.Id;
			const double Size = Clamp(0.6 + 0.1 * Building.Floors, 0.6, 2.5);
			Value.BaseRevenue = ToMoney(std::round(Revenue * Size * Local.Range(0.8, 1.2)));
			Value.BaseCosts = static_cast<Money>(static_cast<double>(Value.BaseRevenue) * Local.Range(0.82, 0.97));
			Value.Cash = Value.BaseRevenue / 2;
			Value.Integrity = Local.Range(0.4, 1.0);
			Value.Rep.Rating = Local.Range(3.2, 4.6);
			Value.Rep.ReviewCount = Local.RangeInt(5, 200);
			Value.Rep.Followers = Local.RangeInt(50, 3000);
			const double Roll = Local.NextDouble();
			int ProblemCount = 0;
			if (Roll < 0.08)
			{
				ProblemCount = Local.RangeInt(2, 3);
				Value.Health = 0.2;
			}
			else if (Roll < 0.33)
			{
				ProblemCount = Local.RangeInt(1, 2);
				Value.Health = 0.5;
			}
			for (int Index = 0; Index < ProblemCount; ++Index)
			{
				const ProblemKind Kind = static_cast<ProblemKind>(Local.RangeInt(0, static_cast<int>(ProblemKind::Count) - 1));
				bool bHas = false;
				for (const ProblemKind Existing : Value.Problems) bHas = bHas || Existing == Kind;
				if (!bHas) Value.Problems.push_back(Kind);
			}
			Markets.AddBusiness(Value);
		}
	}

	void GameState::NewGame(std::uint64_t Seed, const std::string& Name)
	{
		Random.Reseed(Seed);
		PlayerName = Name.empty() ? std::string("Player") : Name;
		Funds = Wallet(Dollars(2500));
		Clock = GameClock();
		Weather = WeatherSystem();
		Player = Profile();
		Jobs.clear();
		PlayerPowers = PowerSet();
		Homes.Load(HomeAndFurniture::Snapshot());
		Garage.clear();
		ActiveVehicleUid.clear();
		HackTools = HackToolkit();
		Wanted = WantedSystem();
		PlayerMinistry = Ministry();
		Ip = IpPortfolio();
		Npcs = NpcPopulation();
		UsedCars = UsedCarMarket();
		Politics = PoliticalCareer();
		NextVehicleNumber = 1;
		EventsWon = 0;
		PendingNotifications.clear();
		PrivateCars.clear();
		PendingOffers.clear();
		Justice = JusticeSystem();
		OpenCharges.clear();
		Outbreak = OutbreakSystem();
		Scheme = ConspiracySystem();
		Fights = CombatSystem();
		Phone = Inbox();
		NextOfferId = 1;
		bManualTransmission = false;

		// Everyone starts with a modest apartment and a cheap, old car.
		if (City.StarterHomeId >= 0)
		{
			Homes.GrantProperty(PropertyIdFor(City.StarterHomeId));
		}
		const VehicleModel* Starter = nullptr;
		for (const VehicleModel& Model : Vehicles.Models())
		{
			// A cheap, tired everyday car (not a collectible).
			if (!Model.bEventOnly && !Model.IsClassic() && Model.AgeYears >= 8 && (Model.Body == BodyStyle::Hatchback || Model.Body == BodyStyle::Sedan) &&
				(!Starter || Model.MarketPrice < Starter->MarketPrice))
			{
				Starter = &Model;
			}
		}
		if (Starter)
		{
			ActiveVehicleUid = GrantVehicle(Starter->Id);
		}
		RefreshHackBoard();
		{
			char Number[32];
			std::snprintf(Number, sizeof(Number), "(843) 555-01%02d", Random.RangeInt(0, 99));
			PhoneNumber = Number; // 555-0100..0199: reserved for fiction
		}
		RefreshUsedCars();
		RefreshDealerStock();
		const CityInfo* Home = City.CityAt(City.SpawnX, City.SpawnY);
		const std::string Town = Home ? Home->Name + (bStateMap && !Map.Abbreviation.empty() ? ", " + Map.Abbreviation : std::string()) : std::string("town");
		Notify("Welcome to " + Town + ", " + PlayerName + ". Choose your powers, find a job, and make something of yourself.");
	}

	// ==================================================================================== Offers on parked cars

	PrivateCar& GameState::PrivateCarAt(const std::string& CarId)
	{
		for (PrivateCar& Car : PrivateCars)
		{
			if (Car.Id == CarId)
			{
				return Car;
			}
		}
		PrivateCars.push_back(MakePrivateCar(Vehicles, CarId, WorldSeed));
		return PrivateCars.back();
	}

	bool GameState::IsOwnerHomeNow(const std::string& CarId)
	{
		return IsOwnerHome(PrivateCarAt(CarId), Clock.DayOfWeek(), Clock.Hour());
	}

	ActionResult GameState::BuyPrivateCar(PrivateCar& Car, Money Price)
	{
		const VehicleModel* Model = Vehicles.FindModel(Car.ModelId);
		if (!Model || Car.bSold)
		{
			return ActionResult::Failure("That car isn't available anymore.");
		}
		if (!Funds.TrySpend(Price, MoneyCategory::Vehicle, "Bought " + Model->DisplayName + " from its owner", Clock.TotalMinutes()))
		{
			return ActionResult::Failure("You need " + FormatMoney(Price) + " in the bank to close the deal.");
		}
		OwnedVehicle Owned;
		Owned.Uid = "car-" + std::to_string(NextVehicleNumber++);
		Owned.ModelId = Car.ModelId;
		Owned.Condition = Car.Condition;
		Owned.MileageKm = Car.MileageKm;
		Owned.Issues = Car.Issues;
		Owned.Build.PaintRgb = Car.PaintRgb;
		Garage.push_back(Owned);
		Car.bSold = true;
		NpcMind& Owner = Npcs.GetOrCreate(Car.OwnerId);
		Owner.Affinity = Clamp(Owner.Affinity + 0.1, -1.0, 1.0);
		RecordOpponentMove(Owner.Deal, true);
		return ActionResult::Success("Signed the title over: the " + Model->DisplayName + " is yours. It's waiting where it's parked.");
	}

	OfferResponse GameState::OfferInPerson(const std::string& CarId, Money Amount)
	{
		PrivateCar& Car = PrivateCarAt(CarId);
		OfferResponse Response;
		const VehicleModel* Model = Vehicles.FindModel(Car.ModelId);
		if (!Model || Car.bSold)
		{
			Response.Reply = "Nobody's selling that.";
			return Response;
		}
		if (!IsOwnerHome(Car, Clock.DayOfWeek(), Clock.Hour()))
		{
			Response.Reply = "Nobody answers the door. You could leave a note.";
			return Response;
		}
		if (Amount > Funds.Balance())
		{
			Response.Reply = "You can't offer money you don't have.";
			return Response;
		}
		NpcMind& Owner = Npcs.GetOrCreate(Car.OwnerId);
		Npcs.Observe(Owner, Stimulus::TalkedWith, 0.4, Clock.TotalMinutes(), Random, "Offered to buy their car");
		Response = EvaluateOffer(*Model, Car, Amount, Player.SkillLevel(Skill::People), Owner.Affinity, BargainStyle::FaceToFace, Random);
		Player.AddSkillXp(Skill::People, 5);
		if (Response.Outcome == OfferOutcome::Accepted)
		{
			const ActionResult Bought = BuyPrivateCar(Car, Response.Price);
			Response.Reply += " " + Bought.Message;
			Car.Rounds = 0;
		}
		else if (Response.Outcome != OfferOutcome::Countered)
		{
			Car.Rounds = 0;
			if (Response.Outcome == OfferOutcome::Declined && ToDollars(Amount) < 0.5 * OwnerReservation(*Model, Car))
			{
				Owner.Affinity = Clamp(Owner.Affinity - 0.05, -1.0, 1.0); // lowballs are remembered
			}
		}
		return Response;
	}

	ActionResult GameState::LeaveNote(const std::string& CarId, Money Amount)
	{
		PrivateCar& Car = PrivateCarAt(CarId);
		if (Car.bSold || !Vehicles.FindModel(Car.ModelId))
		{
			return ActionResult::Failure("Nobody's selling that.");
		}
		for (const PendingOffer& Existing : PendingOffers)
		{
			if (Existing.CarId == CarId)
			{
				return ActionResult::Failure("You already reached out about this one. Give them time to answer.");
			}
		}
		PendingOffer Offer;
		Offer.Id = NextOfferId++;
		Offer.CarId = CarId;
		Offer.Amount = Amount;
		Offer.PlacedMinute = Clock.TotalMinutes();
		Offer.ResponseMinute = NextHomeMinute(Car, Clock.TotalMinutes()) + Random.RangeInt(30, 240);
		Offer.Style = BargainStyle::Note;
		PendingOffers.push_back(Offer);
		return ActionResult::Success("You tuck a note under the wiper: \"I'd buy your car for " + FormatMoney(Amount) + ". Call or text " + PhoneNumber + ".\"");
	}

	ActionResult GameState::TextOffer(const std::string& CarId, Money Amount)
	{
		PrivateCar& Car = PrivateCarAt(CarId);
		const VehicleModel* Model = Vehicles.FindModel(Car.ModelId);
		if (Car.bSold || !Model)
		{
			return ActionResult::Failure("Nobody's selling that.");
		}
		const std::string Thread = "car:" + CarId;
		if (Phone.Thread(Thread).empty())
		{
			return ActionResult::Failure("You don't have their number yet. Leave a note first.");
		}
		for (const PendingOffer& Existing : PendingOffers)
		{
			if (Existing.CarId == CarId)
			{
				return ActionResult::Failure("Wait for them to answer your last message.");
			}
		}
		NpcMind& Owner = Npcs.GetOrCreate(Car.OwnerId);
		Message Sent;
		Sent.Via = Channel::Text;
		Sent.Topic = MessageTopic::CarOffer;
		Sent.FromName = "You";
		Sent.bFromPlayer = true;
		Sent.ThreadId = Thread;
		Sent.CarId = CarId;
		Sent.Price = Amount;
		Sent.Minute = Clock.TotalMinutes();
		Sent.Text = "Would you take " + FormatMoney(Amount) + " for the " + Model->Nameplate + "?";
		Phone.Post(Sent);
		if (Message* Last = Phone.LatestIncoming(Thread))
		{
			Last->bAwaitingReply = false; // a new offer replaces their last counter
		}
		PendingOffer Offer;
		Offer.Id = NextOfferId++;
		Offer.CarId = CarId;
		Offer.Amount = Amount;
		Offer.PlacedMinute = Clock.TotalMinutes();
		// Impatient owners answer fast; everyone answers faster when they like you.
		const double Patience = OwnerPatience(Car);
		Offer.ResponseMinute = Clock.TotalMinutes() + static_cast<std::int64_t>(Random.Range(5.0, 120.0) * Patience * (1.0 - 0.3 * Owner.Affinity));
		Offer.Style = BargainStyle::Text;
		PendingOffers.push_back(Offer);
		return ActionResult::Success("Sent.");
	}

	void GameState::Deliver(Message Value, TickReport* Report)
	{
		switch (Value.Via)
		{
		case Channel::Call: Notify("Call from " + Value.FromName + ": " + Value.Text); break;
		case Channel::Text: Notify("Text from " + Value.FromName + ": " + Value.Text); break;
		case Channel::Email: Notify("Email from " + Value.FromName + ": " + Value.Subject); break;
		case Channel::Count: break;
		}
		const int Id = Phone.Post(Value);
		if (Report)
		{
			if (const Message* Posted = Phone.Find(Id))
			{
				Report->NewMessages.push_back(*Posted);
			}
		}
	}

	int GameState::SendEmail(MessageTopic Topic, const std::string& FromName, const std::string& FromAddress, const std::string& Subject,
		const std::string& Body)
	{
		Message Mail;
		Mail.Via = Channel::Email;
		Mail.Topic = Topic;
		Mail.FromName = FromName;
		Mail.FromAddress = FromAddress;
		Mail.Subject = Subject;
		Mail.Text = Body;
		Mail.ThreadId = "mail:" + Subject;
		Mail.Minute = Clock.TotalMinutes();
		Deliver(Mail, nullptr);
		return Phone.All().empty() ? 0 : Phone.All().back().Id;
	}

	ActionResult GameState::RespondToMessage(int MessageId, bool bAccept)
	{
		Message* Value = Phone.Find(MessageId);
		if (!Value || !Value->bAwaitingReply)
		{
			return ActionResult::Failure("There's nothing to answer.");
		}
		Value->bAwaitingReply = false;
		Value->bRead = true;
		if (Value->Topic != MessageTopic::CarOffer)
		{
			return ActionResult::Success(bAccept ? "Accepted." : "Declined.");
		}
		PrivateCar& Car = PrivateCarAt(Value->CarId);
		NpcMind& Owner = Npcs.GetOrCreate(Car.OwnerId);
		const std::string Name = Value->FromName;
		const Money Price = Value->Price;
		const OfferOutcome Outcome = Value->Outcome;
		if (!bAccept)
		{
			if (Outcome == OfferOutcome::Accepted)
			{
				// They said yes to your own offer and you backed out: that's remembered (repeated game).
				Owner.Affinity = Clamp(Owner.Affinity - 0.15, -1.0, 1.0);
				RecordOpponentMove(Owner.Deal, false);
				return ActionResult::Success(Name + " is disappointed: \"You made the offer...\"");
			}
			return ActionResult::Success("You thank " + Name + " and pass.");
		}
		return BuyPrivateCar(Car, Price);
	}

	// ==================================================================================== Time

	TickReport GameState::Advance(double RealSeconds)
	{
		TickReport Report;
		if (!Wanted.IsWanted() && !OpenCharges.empty())
		{
			OpenCharges.clear(); // lost the police: nobody to charge
		}
		const std::int64_t Before = Clock.TotalMinutes();
		Report.MinutesElapsed = Clock.Advance(RealSeconds);
		for (std::int64_t Minute = Before + 1; Minute <= Before + Report.MinutesElapsed; ++Minute)
		{
			RunMinute(Minute, Report);
		}
		return Report;
	}

	TickReport GameState::Skip(std::int64_t Minutes)
	{
		TickReport Report;
		const std::int64_t Before = Clock.TotalMinutes();
		Clock.Skip(Minutes);
		Report.MinutesElapsed = Clock.TotalMinutes() - Before;
		for (std::int64_t Minute = Before + 1; Minute <= Clock.TotalMinutes(); ++Minute)
		{
			RunMinute(Minute, Report);
		}
		return Report;
	}

	void GameState::RunMinute(std::int64_t Minute, TickReport& Report)
	{
		if (Weather.Update(Minute, Random))
		{
			Report.bWeatherChanged = true;
		}
		Fights.Recover(1, Player.SkillLevel(Skill::Fitness));
		for (const DeliveryEvent& Event : Homes.Update(Minute))
		{
			Report.Deliveries.push_back(Event);
			const std::string Order = "#" + std::to_string(Event.OrderId);
			if (Event.NewStatus == OrderStatus::OutForDelivery)
			{
				SendEmail(MessageTopic::Delivery, "Furniture Delivery", "deliveries@furniture.example", "Order " + Order + " is out for delivery",
					"Good news! Your order " + Order + " is on the truck and will arrive today. Someone will need to be home.");
			}
			else if (Event.NewStatus == OrderStatus::Delivered)
			{
				SendEmail(MessageTopic::Delivery, "Furniture Delivery", "deliveries@furniture.example", "Order " + Order + " delivered",
					"Your order " + Order + " has been delivered. Arrange it however you like. Enjoy your new furniture!");
			}
		}
		const std::int64_t TimeOfDay = Minute % GameClock::MinutesPerDay;
		const int DayOfWeek = static_cast<int>((Minute / GameClock::MinutesPerDay) % 7);
		if (TimeOfDay == 6 * 60)
		{
			Report.bNewDay = true;
			RefreshHackBoard();
			RefreshUsedCars();
		}
		for (std::size_t Index = 0; Index < PendingOffers.size();)
		{
			if (PendingOffers[Index].ResponseMinute > Minute)
			{
				++Index;
				continue;
			}
			const PendingOffer Offer = PendingOffers[Index];
			PendingOffers.erase(PendingOffers.begin() + static_cast<std::ptrdiff_t>(Index));
			PrivateCar& Car = PrivateCarAt(Offer.CarId);
			const VehicleModel* Model = Vehicles.FindModel(Car.ModelId);
			if (!Model || Car.bSold)
			{
				continue;
			}
			NpcMind& Owner = Npcs.GetOrCreate(Car.OwnerId);
			const OfferResponse Response = EvaluateOffer(*Model, Car, Offer.Amount, Player.SkillLevel(Skill::People), Owner.Affinity, Offer.Style, Random);
			Message Reply;
			Reply.Via = Offer.Style == BargainStyle::Text ? Channel::Text : Car.Prefers;
			Reply.Topic = MessageTopic::CarOffer;
			Reply.FromId = Owner.Id;
			Reply.FromName = Owner.Name;
			Reply.FromAddress = Reply.Via == Channel::Email ? detail_state::EmailFor(Owner.Name) : detail_state::PhoneFor(Owner.Id);
			Reply.ThreadId = "car:" + Car.Id;
			Reply.CarId = Car.Id;
			Reply.Outcome = Response.Outcome;
			Reply.Price = Response.Outcome == OfferOutcome::Accepted ? Offer.Amount : Response.Price;
			Reply.Minute = Minute;
			Reply.bAwaitingReply = Response.Outcome == OfferOutcome::Accepted || Response.Outcome == OfferOutcome::Countered;
			if (Offer.Style == BargainStyle::Text)
			{
				Reply.Text = Response.Reply;
			}
			else
			{
				Reply.Subject = "Your offer on my " + Model->Nameplate;
				Reply.Text = (Reply.Via == Channel::Text ? "Hey, it's " : "Hi, this is ") + Owner.Name + ". I found your note on my " + Model->Nameplate +
					". " + Response.Reply;
			}
			if (Response.Outcome == OfferOutcome::Accepted)
			{
				Reply.Text += " Still want it?";
			}
			Owner.Conversations += 1;
			Deliver(Reply, &Report);
		}

		const ElectionResult Election = Politics.Update(Minute, Random);
		if (Election.bHeld)
		{
			Report.Election = Election;
			const int Share = static_cast<int>(std::lround(Election.PlayerShare * 100.0));
			Notify(Election.bWon ? "Election night: you won with " + std::to_string(Share) + "%! You are now " + OfficeTitle(Election.Target) + "."
				: "Election night: " + Election.Opponent + " beat you (" + std::to_string(Share) + "% for you).");
			SendEmail(MessageTopic::Politics, "Board of Elections", "results@elections.example",
				std::string("Official results: ") + OfficeTitle(Election.Target),
				"Certified results: you received " + std::to_string(Share) + "% of the vote. " +
				(Election.bWon ? std::string("Congratulations, and welcome to office.") : Election.Opponent + " won the race."));
		}
		if (TimeOfDay == 16 * 60)
		{
			RunDay(Minute, Report);
		}
		if (DayOfWeek == 6 && TimeOfDay == 11 * 60)
		{
			RunWeek(Minute, Report);
		}
	}

	void GameState::RunDay(std::int64_t Minute, TickReport& Report)
	{
		SyncMinistryBusiness();
		// Overnight the town talks: what people heard yesterday fades a little and the parts worth
		// repeating reach somebody new. Patience with the player's questions comes back too.
		Npcs.TownTalks(Minute, Random);
		if (Sundown.IsActive())
		{
			// A day in Sundown is its own thing: eat, drink, let the Alpha forget a little of what
			// it learned, and let any nest that was cleared long enough ago fill back up.
			Sundown.NewDay(Random);
		}
		const int OutbreakChange = Outbreak.DailyUpdate(Random);
		if (OutbreakChange > 0)
		{
			const std::string Name = Outbreak.Strain().Name;
			Markets.PostNews("Health officials confirm an outbreak of " + Name, "", -0.2, Minute);
			SendEmail(MessageTopic::General, "State Health Department", "alerts@health.example", "Health alert: " + Name,
				"Cases of " + Name + " are rising across the state. Wash your hands and stay home if you feel sick. Researchers with a "
				"promising treatment can apply for fast-track trials.");
		}
		else if (OutbreakChange < 0)
		{
			Markets.PostNews("The " + Outbreak.Strain().Name + " outbreak is over", "", 0.2, Minute);
		}
		// On the run: the manhunt cools while the player keeps out of sight and heats up when they don't.
		if (Justice.IsFugitive())
		{
			const bool bLayingLow = !Wanted.IsWanted() && Jobs.empty();
			if (Justice.AdvanceManhunt(bLayingLow, Random))
			{
				Notify("Deputies ran your plate at a gas station. They know roughly where you are.");
				Wanted.AddStars(3);
				Report.Messages.push_back("The manhunt found your trail");
			}
		}
		// The scheme: investigators build their case, and insiders decide whether to talk.
		if (Scheme.IsLive())
		{
			const double Anger = Clamp(1.0 - Markets.PlayerReputation().Trust, 0.0, 1.0) * (Outbreak.IsActive() ? 1.0 : 0.4);
			const SchemeDay Day = Scheme.DailyUpdate(Outbreak.IsActive(), Anger, Random);
			if (Day.bFlipped)
			{
				Notify(Day.FlippedName + " has been seen going into the sheriff's office with a lawyer.");
			}
			if (!Day.Headline.empty())
			{
				Markets.PostNews(Day.Headline, "", -0.5, Minute);
				Report.Messages.push_back(Day.Headline);
			}
			if (Day.bExposed)
			{
				SchemeExposed(Minute);
			}
			else if (Scheme.Stage() == SchemeStage::GotAway)
			{
				Notify("The file on the outbreak's origin is closed. You got away with it.");
			}
		}
		// A health official on the payroll keeps the state's response slow.
		Markets.DemandDrag = Outbreak.EconomicDrag();
		Markets.PolicyHealthBonus = ActiveLaws().BusinessHealthBonus;
		const Money Income = Markets.DailyUpdate(Minute, Random);
		if (Income > 0)
		{
			Funds.Deposit(Income, MoneyCategory::Reward, "Business owner income", Minute);
			Report.OwnerIncome += Income;
		}
		// Reputation attacks on the player's ministry come back through the mirror business.
		if (const Business* Mirror = Markets.FindBusiness(PlayerMinistryId))
		{
			if (Mirror->bDissolved && PlayerMinistry.IsFounded())
			{
				PlayerMinistry.Dissolve();
				Notify("Your ministry collapsed after losing the community's trust.");
				Report.Messages.push_back("Ministry dissolved");
			}
		}
		// A visible ministry draws critics; the less integrity, the more likely something sticks.
		if (PlayerMinistry.IsFounded() && PlayerMinistry.Members() > 80 && Random.Chance(0.01 + 0.03 * (1.0 - PlayerMinistry.Integrity())))
		{
			const AttackKind Kind = PlayerMinistry.Doctrine() < 0.5 ? AttackKind::DoctrineExpose : (Random.Chance(0.5) ? AttackKind::SmearCampaign : AttackKind::Allegation);
			const AttackResult Result = Attack(PlayerMinistryId, Kind);
			Report.Messages.push_back(Result.Headline);
		}
		// Rare cosmic events mutate powers.
		if (PlayerPowers.HasPowers() && Random.Chance(0.05))
		{
			MutationEvent Event;
			if (PlayerPowers.Surge(Powers, Random, Event))
			{
				Report.bPowerSurge = true;
				Report.Surge = Event;
				Notify("A meteor shower lit up the sky. Your powers feel different...");
			}
		}
	}

	void GameState::RunWeek(std::int64_t Minute, TickReport& Report)
	{
		Report.bNewWeek = true;
		RefreshDealerStock();
		for (PrivateCar& Car : PrivateCars)
		{
			Car.CashNeed = Clamp(Car.CashNeed + Random.Range(-0.1, 0.15), 0.0, 1.0);
			const VehicleModel* Model = Vehicles.FindModel(Car.ModelId);
			if (Car.bSold || Car.OffersMade == 0 || !Model || Car.CashNeed < 0.6 || !Random.Chance(0.15))
			{
				continue;
			}
			// Circumstances change: an owner who turned the player down calls back.
			NpcMind& Owner = Npcs.GetOrCreate(Car.OwnerId);
			Message Call;
			Call.Via = Car.Prefers;
			Call.Topic = MessageTopic::CarOffer;
			Call.FromId = Owner.Id;
			Call.FromName = Owner.Name;
			Call.FromAddress = Call.Via == Channel::Email ? detail_state::EmailFor(Owner.Name) : detail_state::PhoneFor(Owner.Id);
			Call.ThreadId = "car:" + Car.Id;
			Call.CarId = Car.Id;
			Call.Outcome = OfferOutcome::Countered;
			Call.Price = ToMoney(std::round(OwnerReservation(*Model, Car) / 50.0) * 50.0);
			Call.Minute = Minute;
			Call.bAwaitingReply = true;
			Call.Subject = "Still interested in my " + Model->Nameplate + "?";
			Call.Text = "Hi, it's " + Owner.Name + ". You asked about my " + Model->Nameplate + " a while back. Things have changed. Would you still do " +
				FormatMoney(Call.Price) + "?";
			Deliver(Call, &Report);
		}
		if (PlayerMinistry.IsFounded())
		{
			Report.Offering = PlayerMinistry.CollectWeeklyOffering(Random);
			Report.bOffering = true;
			Funds.Deposit(Report.Offering.Salary, MoneyCategory::Ministry, "Ministry salary", Minute);
			Notify("Sunday service: " + std::to_string(Report.Offering.Attendance) + " attended. Giving " +
				FormatMoney(Report.Offering.Total) + ", your salary " + FormatMoney(Report.Offering.Salary) + ".");
		}
		if (Politics.WeeklySalary() > 0)
		{
			Funds.Deposit(Politics.WeeklySalary(), MoneyCategory::Job, std::string("Salary: ") + OfficeTitle(Politics.Current()), Minute);
		}
		for (const WeeklyRoyalty& Payment : Ip.WeeklyRoyalties(Random))
		{
			Funds.Deposit(Payment.Amount, MoneyCategory::Reward, "Royalties", Minute);
			Report.Royalties += Payment.Amount;
		}
		if (Report.Royalties > 0)
		{
			Notify("Royalties this week: " + FormatMoney(Report.Royalties) + ".");
		}
	}

	void GameState::SyncMinistryBusiness()
	{
		if (!PlayerMinistry.IsFounded())
		{
			return;
		}
		Business* Mirror = Markets.FindBusiness(PlayerMinistryId);
		if (!Mirror)
		{
			Business Value;
			Value.Id = PlayerMinistryId;
			Value.Name = PlayerMinistry.Name();
			Value.Sector = "Faith";
			Value.PlayerEquity = 0.0; // a ministry isn't owned; the salary is paid separately
			Markets.AddBusiness(Value);
			Mirror = Markets.FindBusiness(PlayerMinistryId);
		}
		Mirror->bDissolved = false;
		Mirror->Name = PlayerMinistry.Name();
		Mirror->Integrity = PlayerMinistry.Integrity();
		Mirror->Doctrine = PlayerMinistry.Doctrine();
		Mirror->Rep.Followers = PlayerMinistry.Members();
		Mirror->Rep.Trust = Clamp(0.5 * Mirror->Rep.Trust + 0.5 * PlayerMinistry.Integrity(), 0.0, 1.0);
		Mirror->BaseRevenue = ToMoney(PlayerMinistry.Members() * 18.0 * 4.0);
		Mirror->BaseCosts = Mirror->BaseRevenue;
		Mirror->Health = 0.7;
	}

	// ==================================================================================== Vehicles

	std::string GameState::GrantVehicle(const std::string& ModelId)
	{
		OwnedVehicle Car;
		Car.Uid = "car-" + std::to_string(NextVehicleNumber++);
		Car.ModelId = ModelId;
		Garage.push_back(Car);
		return Car.Uid;
	}

	ActionResult GameState::BuyVehicle(const std::string& ModelId)
	{
		const VehicleModel* Model = Vehicles.FindModel(ModelId);
		if (!Model || Model->bEventOnly)
		{
			return ActionResult::Failure("That model isn't for sale.");
		}
		if (!Model->IsCurrent())
		{
			return ActionResult::Failure("That model is out of production. Ask the showroom to locate one, or try the used and classic dealers.");
		}
		const Money Price = NewCarPrice(*Model);
		if (!Funds.TrySpend(Price, MoneyCategory::Vehicle, "Bought " + Model->DisplayName, Clock.TotalMinutes()))
		{
			return ActionResult::Failure("You need " + FormatMoney(Price) + ".");
		}
		ActiveVehicleUid = GrantVehicle(ModelId);
		if (OwnedVehicle* Car = FindOwned(ActiveVehicleUid))
		{
			Car->WarrantyUntilMinute = Clock.TotalMinutes() + 3 * 365 * GameClock::MinutesPerDay;
		}
		return ActionResult::Success("Congratulations on your new " + Model->DisplayName + "! Three-year warranty included. It's parked out front.");
	}

	Money GameState::TradeInPrice(const std::string& Uid) const
	{
		for (const OwnedVehicle& Car : Garage)
		{
			if (Car.Uid != Uid)
			{
				continue;
			}
			const VehicleModel* Model = Vehicles.FindModel(Car.ModelId);
			Money Value = Model ? TradeInOffer(*Model, Car) : 0;
			// Installed parts add a little resale value.
			for (const std::string& PartId : Car.Build.Parts)
			{
				if (const PartDef* Part = Parts.Find(PartId))
				{
					if (Model)
					{
						Value += PartPrice(*Part, *Model) / 4;
					}
				}
			}
			return Value;
		}
		return 0;
	}

	ActionResult GameState::SellVehicle(const std::string& Uid)
	{
		for (auto It = Garage.begin(); It != Garage.end(); ++It)
		{
			if (It->Uid == Uid)
			{
				const Money Value = TradeInPrice(Uid);
				Funds.Deposit(Value, MoneyCategory::Vehicle, "Sold car", Clock.TotalMinutes());
				if (ActiveVehicleUid == Uid)
				{
					ActiveVehicleUid.clear();
				}
				Garage.erase(It);
				return ActionResult::Success("Sold for " + FormatMoney(Value) + ".");
			}
		}
		return ActionResult::Failure("You don't own that car.");
	}

	OwnedVehicle* GameState::FindOwned(const std::string& Uid)
	{
		for (OwnedVehicle& Car : Garage)
		{
			if (Car.Uid == Uid) return &Car;
		}
		return nullptr;
	}

	const OwnedVehicle* GameState::FindOwned(const std::string& Uid) const
	{
		for (const OwnedVehicle& Car : Garage)
		{
			if (Car.Uid == Uid) return &Car;
		}
		return nullptr;
	}

	const OwnedVehicle* GameState::ActiveVehicle() const
	{
		return FindOwned(ActiveVehicleUid);
	}

	ActionResult GameState::InstallPart(const std::string& Uid, const std::string& PartId)
	{
		OwnedVehicle* Car = FindOwned(Uid);
		const PartDef* Part = Parts.Find(PartId);
		const VehicleModel* Model = Car ? Vehicles.FindModel(Car->ModelId) : nullptr;
		if (!Car || !Part || !Model)
		{
			return ActionResult::Failure("Unknown car or part.");
		}
		if (!Parts.Fits(*Part, *Model))
		{
			return ActionResult::Failure(Part->Name + " doesn't fit this car.");
		}
		if (Car->Build.Has(PartId))
		{
			return ActionResult::Failure("Already installed.");
		}
		const Money Price = PartPrice(*Part, *Model);
		if (!Funds.TrySpend(Price, MoneyCategory::Vehicle, Part->Name, Clock.TotalMinutes()))
		{
			return ActionResult::Failure("You need " + FormatMoney(Price) + ".");
		}
		Car->Build.Install(Parts, *Part);
		return ActionResult::Success(Part->Name + " installed.");
	}

	ActionResult GameState::RemovePart(const std::string& Uid, PartCategory Category)
	{
		OwnedVehicle* Car = FindOwned(Uid);
		if (!Car)
		{
			return ActionResult::Failure("Unknown car.");
		}
		Car->Build.Remove(Parts, Category);
		return ActionResult::Success(std::string(PartCategoryName(Category)) + " returned to stock.");
	}

	ActionResult GameState::SetTuning(const std::string& Uid, const TuningSheet& Sheet)
	{
		OwnedVehicle* Car = FindOwned(Uid);
		if (!Car)
		{
			return ActionResult::Failure("Unknown car.");
		}
		Car->Build.Tuning = Sheet;
		return ActionResult::Success("Tune saved.");
	}

	Money GameState::PaintPrice(int Finish) const
	{
		return static_cast<Money>(std::llround(Dollars(800) * (1.0 + 0.5 * Clamp(Finish, 0, 4)) * Economy.Goods / 0.55));
	}

	ActionResult GameState::Paint(const std::string& Uid, std::uint32_t Rgb, int Finish)
	{
		OwnedVehicle* Car = FindOwned(Uid);
		if (!Car)
		{
			return ActionResult::Failure("Unknown car.");
		}
		const Money Price = PaintPrice(Finish);
		if (!Funds.TrySpend(Price, MoneyCategory::Vehicle, "Paint job", Clock.TotalMinutes()))
		{
			return ActionResult::Failure("You need " + FormatMoney(Price) + ".");
		}
		Car->Build.PaintRgb = Rgb;
		Car->Build.PaintFinish = Clamp(Finish, 0, 4);
		return ActionResult::Success("Fresh paint!");
	}

	ActionResult GameState::Repair(const std::string& Uid)
	{
		OwnedVehicle* Car = FindOwned(Uid);
		const VehicleModel* Model = Car ? Vehicles.FindModel(Car->ModelId) : nullptr;
		if (!Car || !Model)
		{
			return ActionResult::Failure("Unknown car.");
		}
		const Money Price = static_cast<Money>(std::llround(ToDollars(Model->MarketPrice) * 100.0 * 0.15 * (1.0 - Car->Condition)));
		if (!Funds.TrySpend(Price, MoneyCategory::Vehicle, "Repairs", Clock.TotalMinutes()))
		{
			return ActionResult::Failure("Repairs cost " + FormatMoney(Price) + ".");
		}
		Car->Condition = 1.0;
		return ActionResult::Success("Good as new.");
	}

	VehicleSetup GameState::SetupFor(const std::string& ModelId, const VehicleBuild* Build) const
	{
		const VehicleModel* Model = Vehicles.FindModel(ModelId);
		if (!Model)
		{
			return VehicleSetup();
		}
		static const VehicleBuild Stock;
		return BuildSetup(*Model, Build ? *Build : Stock, Parts);
	}

	// ==================================================================================== Careers

	CareerProgress* GameState::FindJob(const std::string& CareerId)
	{
		for (CareerProgress& Job : Jobs)
		{
			if (Job.CareerId == CareerId) return &Job;
		}
		return nullptr;
	}

	ActionResult GameState::ApplyForCareer(const std::string& CareerId)
	{
		if (FindJob(CareerId))
		{
			return ActionResult::Failure("You already work here.");
		}
		std::string Reason;
		if (!Careers.CanApply(Player, CareerId, &Reason))
		{
			return ActionResult::Failure(Reason);
		}
		const CareerDef* Applying = Careers.FindCareer(CareerId);
		if (Applying && Applying->bCleanRecord && Justice.HasFelony())
		{
			return ActionResult::Failure("This job requires a clean record. A lawyer can help expunge old convictions.");
		}
		if (InJail())
		{
			return ActionResult::Failure("Not from a jail cell.");
		}
		if (Justice.IsFugitive())
		{
			return ActionResult::Failure("Every application runs your name. You're a wanted escapee.");
		}
		CareerProgress Job;
		Job.CareerId = CareerId;
		Jobs.push_back(Job);
		const CareerDef* Def = Careers.FindCareer(CareerId);
		return ActionResult::Success("You're hired as " + (Def ? Def->Levels.front().Title : CareerId) + "!");
	}

	ShiftResult GameState::WorkShift(const std::string& CareerId, double Performance, TickReport* OutTick)
	{
		ShiftResult Result;
		CareerProgress* Job = FindJob(CareerId);
		const CareerDef* Def = Careers.FindCareer(CareerId);
		if (!Job || !Def || InJail())
		{
			return Result;
		}
		Result = Careers.ResolveShift(*Def, *Job, Player, Performance);
		const double SurgePay = Def->Workplace == "Hospital" ? Outbreak.ClinicianPayMul() : 1.0;
		Result.Pay = static_cast<Money>(std::llround(static_cast<double>(Result.Pay) * Economy.Wages * ActiveLaws().WageMul * SurgePay));
		Funds.Deposit(Result.Pay, MoneyCategory::Job, Def->Name + " shift", Clock.TotalMinutes());
		TickReport Tick = Skip(static_cast<std::int64_t>(Result.HoursWorked) * 60);
		if (OutTick)
		{
			*OutTick = Tick;
		}
		if (Result.bPromoted)
		{
			Notify("Promotion! You're now " + Result.Title + ".");
			SendEmail(MessageTopic::JobOffer, "Human Resources", "hr@" + Slugify(Def->Workplace) + ".example", "Congratulations on your promotion",
				"We're pleased to let you know you've been promoted to " + Result.Title + ". Your new pay rate starts with your next shift.");
		}
		return Result;
	}

	ActionResult GameState::Enroll(const std::string& EducationId)
	{
		std::string Reason;
		if (!Careers.CanEnroll(Player, EducationId, &Reason))
		{
			return ActionResult::Failure(Reason);
		}
		const EducationDef* Def = Careers.FindEducation(EducationId);
		const Money Tuition = static_cast<Money>(std::llround(static_cast<double>(Def->Tuition) * ActiveLaws().TuitionMul));
		if (!Funds.TrySpend(Tuition, MoneyCategory::Education, "Tuition: " + Def->Name, Clock.TotalMinutes()))
		{
			return ActionResult::Failure("Tuition is " + FormatMoney(Tuition) + ".");
		}
		Enrollment Study;
		Study.EducationId = EducationId;
		Player.SetEnrollment(Study);
		return ActionResult::Success("Enrolled in " + Def->Name + ". Attend classes at the university.");
	}

	ClassResult GameState::AttendClass(double Performance, int Hours)
	{
		ClassResult Result = Careers.AttendClass(Player, Performance, Hours);
		if (Result.bValid)
		{
			Skip(static_cast<std::int64_t>(Hours) * 60);
			if (Result.bGraduated)
			{
				Notify("You graduated: " + Result.EducationName + "!");
				// Ministry credentials advance ordination.
				if (Player.HasEducation("ministry-certificate"))
				{
					PlayerMinistry.SetOrdination(std::max(PlayerMinistry.Ordination(), 1));
				}
				if (Player.HasEducation("master-of-divinity"))
				{
					PlayerMinistry.SetOrdination(std::max(PlayerMinistry.Ordination(), 2));
				}
			}
		}
		return Result;
	}

	ActionResult GameState::ChoosePowers(const std::vector<std::string>& Ids)
	{
		std::string Error;
		if (!PlayerPowers.Choose(Powers, Ids, &Error))
		{
			return ActionResult::Failure(Error);
		}
		return ActionResult::Success("Your powers awaken. Use them wisely: they will change as you do.");
	}

	GameState::PowerUse GameState::UsePower(int Slot, const std::vector<std::string>& Witnesses, bool bInPublic)
	{
		PowerUse Result;
		const PowerInstance* Instance = PlayerPowers.Slot(Slot);
		if (!Instance)
		{
			Result.Message = "You haven't awakened that power.";
			return Result;
		}
		Result.Power = Powers.Evaluate(*Instance);
		Result.bOk = Result.Power.Def != nullptr;
		if (!Result.bOk)
		{
			Result.Message = "That power won't answer.";
			return Result;
		}
		bool bLevelUp = false;
		MutationEvent Event;
		Result.bMutated = PlayerPowers.RecordUse(Slot, Powers, Random, Event, bLevelUp);
		Result.bLeveledUp = bLevelUp;
		Result.Mutation = Event;
		Result.Message = Result.Power.Def->Name;
		if (bLevelUp)
		{
			const PowerInstance* Now = PlayerPowers.Slot(Slot);
			Notify(std::string(Result.Power.Def->Name) + " is getting stronger (level " +
				std::to_string(Now ? Now->Level : 0) + ").");
		}
		if (Result.bMutated)
		{
			if (Event.bEvolved)
			{
				const PowerDef* After = Powers.FindPower(Event.PowerAfter);
				Notify("Something changed. " + std::string(Result.Power.Def->Name) + " became " +
					(After ? After->Name : Event.PowerAfter) + ".");
			}
			else if (const MutationDef* Mutation = Powers.FindMutation(Event.MutationId))
			{
				Notify(std::string(Result.Power.Def->Name) + " mutated: " + Mutation->Name + ".");
			}
		}
		// People who see it remember it, and that is what starts waking them up.
		const bool bGlitch = Result.Power.Def->Effect == PowerEffect::Teleport || Result.Power.Def->Effect == PowerEffect::Phasing ||
			Result.Power.Def->Effect == PowerEffect::TimeSlow;
		for (const std::string& WitnessId : Witnesses)
		{
			NpcMind& Mind = Npcs.GetOrCreate(WitnessId);
			Npcs.Observe(Mind, bGlitch ? Stimulus::SawGlitch : Stimulus::SawPower, 0.6, Clock.TotalMinutes(), Random,
				Result.Power.Def->Name, &Powers);
		}
		if (bInPublic && !Witnesses.empty())
		{
			// A public display gets talked about: followers first, then the authorities' interest.
			Markets.PlayerReputation().Followers += static_cast<int>(Witnesses.size()) * 5;
			if (Justice.IsFugitive())
			{
				Wanted.AddStars(1); // using powers while wanted tells everyone exactly where you are
			}
		}
		return Result;
	}

	// ==================================================================================== Talking

	NpcMind& GameState::Meet(const std::string& NpcId, double X, double Y)
	{
		NpcMind& Mind = Npcs.GetOrCreate(NpcId);
		if (Mind.Hometown.empty())
		{
			// Whoever you are standing next to is from the nearest town, which gives them a place to
			// have an opinion about.
			const StateCity* Closest = nullptr;
			double Best = 0.0;
			for (const StateCity& Town : Map.Cities)
			{
				const double Dx = Town.X - X;
				const double Dy = Town.Y - Y;
				const double Distance = Dx * Dx + Dy * Dy;
				if (!Closest || Distance < Best)
				{
					Best = Distance;
					Closest = &Town;
				}
			}
			Npcs.Place(Mind, Closest ? Closest->Name : Map.Name);
		}
		if (Mind.Knows.empty())
		{
			Rng Local(HashString("circle:" + NpcId));
			Npcs.Mingle(Mind, Local, 3);
		}
		// Mingle can move the vector out from under an old reference, so look it up again.
		return Npcs.GetOrCreate(NpcId);
	}

	std::vector<DialogueOption> GameState::TalkOptions(const std::string& NpcId) const
	{
		if (const NpcMind* Mind = Npcs.Find(NpcId))
		{
			return Npcs.Options(*Mind);
		}
		return std::vector<DialogueOption>();
	}

	TalkTurn GameState::Talk(const std::string& NpcId, const std::string& OptionId)
	{
		TalkTurn Turn;
		NpcMind* Mind = Npcs.Find(NpcId);
		if (!Mind)
		{
			Turn.Reply = "There's nobody there.";
			Turn.bEnded = true;
			return Turn;
		}
		const std::int64_t Minute = Clock.TotalMinutes();
		// Say is told what is in the wallet and declines anything the player cannot pay for, so by the
		// time it reports a cost the money is known to be there and TrySpend cannot fail.
		Turn = Npcs.Say(*Mind, OptionId, Minute, Random, Careers, Funds.Balance());
		if (Turn.Cost > 0)
		{
			const std::string Note = OptionId == "help" ? "Helped " + Mind->Name : "Paid " + Mind->Name + " for information";
			Funds.TrySpend(Turn.Cost, MoneyCategory::Other, Note, Minute);
		}
		// Word travels on its own. The name is the player's, so the town's opinion is about them.
		if (Turn.bNewRumor && !Turn.Rumor.empty())
		{
			const int Heard = Npcs.Spread(NpcId, Turn.Rumor, 0.9, Minute, Random);
			if (Heard > 0)
			{
				const bool bGood = OptionId == "help" || OptionId.rfind("teach:", 0) == 0;
				Markets.PlayerReputation().Followers += Heard * (bGood ? 3 : -2);
				Markets.PlayerReputation().Sentiment =
					Clamp(Markets.PlayerReputation().Sentiment + (bGood ? 0.01 : -0.02) * static_cast<double>(Heard), -1.0, 1.0);
			}
		}
		return Turn;
	}


	// ==================================================================================== Amenities

	GameState::AmenityOffer GameState::QuoteAmenity(Amenity Kind) const
	{
		AmenityOffer Offer;
		Offer.bAvailable = true;
		// Prices are in 1990s money like everything else the player buys, and scale with the economy.
		// These are already written in era dollars, so they take the same correction the rest of the
		// era-priced things take rather than the modern-to-era discount, which would halve them twice.
		const double Index = Economy.Goods / 0.55;
		auto Price = [Index](int EraDollars)
		{
			return static_cast<Money>(std::llround(static_cast<double>(Dollars(EraDollars)) * Index));
		};
		switch (Kind)
		{
		case Amenity::Meal:
			Offer.Cost = Price(7);
			Offer.Minutes = 45;
			break;
		case Amenity::Lodging:
			Offer.Cost = Price(45);
			Offer.Minutes = 8 * 60;
			break;
		case Amenity::Treatment:
			Offer.Cost = Price(120);
			Offer.Minutes = 2 * 60;
			if (Fights.PlayerHealth() >= CombatSystem::PlayerMaxHealth)
			{
				Offer.bAvailable = false;
				Offer.Reason = "There's nothing wrong with you.";
			}
			break;
		case Amenity::Fines:
			Offer.Cost = Justice.Case().Fine;
			Offer.Minutes = 30;
			if (Justice.Case().Fine <= 0)
			{
				Offer.bAvailable = false;
				Offer.Reason = "You don't owe anything.";
			}
			break;
		case Amenity::Gym:
			Offer.Cost = Price(5);
			Offer.Minutes = 60;
			Offer.Trains = Skill::Fitness;
			Offer.bTrains = true;
			break;
		case Amenity::Track:
			Offer.Cost = 0;
			Offer.Minutes = 60;
			Offer.Trains = Skill::Athletics;
			Offer.bTrains = true;
			break;
		case Amenity::Library:
			Offer.Cost = 0;
			Offer.Minutes = 60;
			Offer.Trains = Skill::Science;
			Offer.bTrains = true;
			break;
		case Amenity::Service:
			Offer.Cost = 0;
			Offer.Minutes = 60;
			Offer.Trains = Skill::Faith;
			Offer.bTrains = true;
			break;
		case Amenity::Leisure:
			Offer.Cost = Price(6);
			Offer.Minutes = 90;
			Offer.Trains = Skill::People;
			Offer.bTrains = true;
			break;
		case Amenity::Count:
			Offer.bAvailable = false;
			break;
		}
		if (Offer.bAvailable && Offer.Cost > 0 && !Funds.CanAfford(Offer.Cost))
		{
			Offer.bAvailable = false;
			Offer.Reason = "You can't afford it.";
		}
		if (Offer.bAvailable && Justice.Jail().bInJail)
		{
			Offer.bAvailable = false;
			Offer.Reason = "Not from in here.";
		}
		return Offer;
	}

	ActionResult GameState::UseAmenity(Amenity Kind)
	{
		const AmenityOffer Offer = QuoteAmenity(Kind);
		if (!Offer.bAvailable)
		{
			return ActionResult::Failure(Offer.Reason.empty() ? std::string("Not now.") : Offer.Reason);
		}
		// Booked where it belongs, so the ledger says what the money went on.
		MoneyCategory Ledger = MoneyCategory::Other;
		std::string Note = "Spent in town";
		switch (Kind)
		{
		case Amenity::Treatment: Ledger = MoneyCategory::Medical; Note = "Treatment"; break;
		case Amenity::Fines: Ledger = MoneyCategory::Fine; Note = "Paid a fine"; break;
		case Amenity::Lodging: Note = "A room for the night"; break;
		case Amenity::Meal: Note = "A meal"; break;
		case Amenity::Gym: Note = "The gym"; break;
		default: break;
		}
		if (Offer.Cost > 0 && !Funds.TrySpend(Offer.Cost, Ledger, Note, Clock.TotalMinutes()))
		{
			return ActionResult::Failure("You can't afford it.");
		}
		std::string Said;
		switch (Kind)
		{
		case Amenity::Meal:
			Said = "A plate of something hot and a cup of coffee.";
			break;
		case Amenity::Lodging:
			// The time skip below recovers the hours; doing it here as well would heal them twice.
			Said = "A room for the night. You sleep properly for once.";
			break;
		case Amenity::Treatment:
			Fights.ReviveAtHospital();
			Said = "Patched up and sent home.";
			break;
		case Amenity::Fines:
			Said = "Paid in full. The clerk stamps it and that's that.";
			Justice.PayFine();
			break;
		case Amenity::Gym:
		case Amenity::Track:
		case Amenity::Library:
		case Amenity::Service:
		case Amenity::Leisure:
		case Amenity::Count:
			break;
		}
		if (Offer.bTrains)
		{
			// An hour of honest practice. What it is worth falls off as the skill gets high, which is
			// what makes a career or a course the way up from here.
			const int Level = Player.SkillLevel(Offer.Trains);
			// Less than an hour of a class is worth, so a course or a career stays the way up from here.
			const int Xp = static_cast<int>(std::lround(4.0 / (1.0 + 0.35 * Level))) + 1;
			const int Gained = Player.AddSkillXp(Offer.Trains, Xp);
			Said = std::string("An hour of ") + SkillName(Offer.Trains) + ".";
			if (Gained > 0)
			{
				Said += " Something clicked: level " + std::to_string(Player.SkillLevel(Offer.Trains)) + ".";
			}
		}
		Skip(Offer.Minutes);
		return ActionResult::Success(Said);
	}

	// ==================================================================================== Homes

	ActionResult GameState::BuyHome(int BuildingId)
	{
		std::string Error;
		if (!Homes.BuyProperty(PropertyIdFor(BuildingId), Funds, Clock.TotalMinutes(), &Error, ActiveLaws().HousingPriceMul))
		{
			return ActionResult::Failure(Error);
		}
		return ActionResult::Success("The keys are yours. Welcome home!");
	}

	ActionResult GameState::OrderFurniture(const std::vector<std::string>& ItemIds, int HomeBuildingId, DeliverySpeed Speed)
	{
		std::string Error;
		int OrderId = 0;
		if (!Homes.PlaceOrder(ItemIds, PropertyIdFor(HomeBuildingId), Speed, Funds, Clock.TotalMinutes(), OrderId, &Error))
		{
			return ActionResult::Failure(Error);
		}
		const FurnitureOrder* Order = Homes.FindOrder(OrderId);
		GameClock Arrival;
		Arrival.SetTotalMinutes(static_cast<double>(Order ? Order->DeliverMinute : 0));
		return ActionResult::Success("Order #" + std::to_string(OrderId) + " placed. Delivery " + Arrival.Format() + ".");
	}

	// ==================================================================================== Hacking

	void GameState::RefreshHackBoard()
	{
		HackBoard = GenerateHackContracts(5, Player.SkillLevel(Skill::Tech), Random);
	}

	ActionResult GameState::BuyHackTool(int Which)
	{
		int* Level = nullptr;
		const char* Name = "";
		switch (Which)
		{
		case 0: Level = &HackTools.ProbeKit; Name = "Probe Kit"; break;
		case 1: Level = &HackTools.GhostRelay; Name = "Ghost Relay"; break;
		case 2: Level = &HackTools.PatternEngine; Name = "Pattern Engine"; break;
		default: return ActionResult::Failure("Unknown tool.");
		}
		const Money Price = HackToolPrice(*Level);
		if (Price <= 0)
		{
			return ActionResult::Failure(std::string(Name) + " is fully upgraded.");
		}
		if (!Funds.TrySpend(Price, MoneyCategory::Tools, Name, Clock.TotalMinutes()))
		{
			return ActionResult::Failure("You need " + FormatMoney(Price) + ".");
		}
		*Level += 1;
		return ActionResult::Success(std::string(Name) + " upgraded to level " + std::to_string(*Level) + ".");
	}

	HackConfig GameState::ConfigFor(const HackContract& Contract) const
	{
		return MakeHackConfig(Contract.Tier, Player.SkillLevel(Skill::Tech), HackTools);
	}

	ActionResult GameState::CompleteHack(const std::string& ContractId, bool bSuccess)
	{
		for (auto It = HackBoard.begin(); It != HackBoard.end(); ++It)
		{
			if (It->Id != ContractId)
			{
				continue;
			}
			const HackContract Contract = *It;
			HackBoard.erase(It);
			Player.AddSkillXp(Skill::Tech, bSuccess ? 40 * Contract.Tier : 10);
			if (!bSuccess)
			{
				if (Contract.Legality == HackLegality::Unauthorized)
				{
					Wanted.AddStars(std::max(1, static_cast<int>(std::lround(ActiveLaws().HackingHeatMul))));
					return ActionResult::Failure("Traced! The police are on their way.");
				}
				return ActionResult::Failure("The client's defenses held. Better luck next time.");
			}
			Funds.Deposit(Contract.Payout, MoneyCategory::Hacking, Contract.Title, Clock.TotalMinutes());
			if (Contract.Legality == HackLegality::Unauthorized)
			{
				Wanted.AddStars(std::max(1, static_cast<int>(std::lround(Contract.HeatStars * ActiveLaws().HackingHeatMul))));
				return ActionResult::Success("Paid " + FormatMoney(Contract.Payout) + ". Someone noticed. Lay low.");
			}
			return ActionResult::Success("Report delivered. " + Contract.Client + " paid " + FormatMoney(Contract.Payout) + ".");
		}
		return ActionResult::Failure("That contract is gone.");
	}

	// ==================================================================================== Ministry

	ActionResult GameState::FoundMinistry(const std::string& Name)
	{
		if (!PlayerMinistry.Found(Name))
		{
			return ActionResult::Failure(PlayerMinistry.IsFounded() ? "You already lead a ministry." : "Give your ministry a name.");
		}
		SyncMinistryBusiness();
		return ActionResult::Success(Name + " is founded. Preach on Sundays, serve the city, and lead with integrity.");
	}

	SermonResult GameState::Preach(SermonTheme Theme, double Performance)
	{
		const SermonResult Result = PlayerMinistry.Preach(Theme, Performance, Random);
		if (Result.bValid)
		{
			Player.AddSkillXp(Skill::Faith, static_cast<int>(std::lround(30.0 * (0.5 + Performance))));
			Player.AddSkillXp(Skill::People, 10);
		}
		return Result;
	}

	ActionResult GameState::RunOutreach(OutreachKind Kind)
	{
		OutreachResult Result;
		std::string Error;
		if (!PlayerMinistry.RunOutreach(Kind, Random, Result, &Error))
		{
			return ActionResult::Failure(Error);
		}
		Skip(static_cast<std::int64_t>(GetOutreachInfo(Kind).Hours) * 60);
		Markets.PostNews(PlayerMinistry.Name() + " hosts a " + ToLowerAscii(GetOutreachInfo(Kind).Name), PlayerMinistryId, 0.2, Clock.TotalMinutes());
		return ActionResult::Success(std::to_string(Result.Visitors) + " people came. " + std::to_string(Result.NewMembers) + " joined.");
	}

	ActionResult GameState::UpgradeMinistry(Facility Value)
	{
		std::string Error;
		if (!PlayerMinistry.UpgradeFacility(Value, &Error))
		{
			return ActionResult::Failure(Error);
		}
		return ActionResult::Success(std::string(FacilityName(Value)) + " upgraded.");
	}

	ActionResult GameState::Telethon()
	{
		const Money Raised = PlayerMinistry.RunTelethon(Random);
		if (Raised <= 0)
		{
			return ActionResult::Failure("Found a ministry first.");
		}
		return ActionResult::Success("The telethon raised " + FormatMoney(Raised) + ". Some members found it uncomfortable.");
	}

	ActionResult GameState::Accountability()
	{
		std::string Error;
		if (!PlayerMinistry.CommitToAccountability(&Error))
		{
			return ActionResult::Failure(Error);
		}
		return ActionResult::Success("Open books, an elder board and a solid teaching series. Trust is rebuilding.");
	}

	// ==================================================================================== Market

	ActionResult GameState::BuyShares(const std::string& Ticker, std::int64_t Shares)
	{
		std::string Error;
		if (!Markets.BuyShares(Ticker, Shares, Funds, Clock.TotalMinutes(), &Error))
		{
			return ActionResult::Failure(Error);
		}
		return ActionResult::Success("Bought " + std::to_string(Shares) + " shares of " + Ticker + ".");
	}

	ActionResult GameState::SellShares(const std::string& Ticker, std::int64_t Shares)
	{
		std::string Error;
		if (!Markets.SellShares(Ticker, Shares, Funds, Clock.TotalMinutes(), &Error))
		{
			return ActionResult::Failure(Error);
		}
		return ActionResult::Success("Sold " + std::to_string(Shares) + " shares of " + Ticker + ".");
	}

	ActionResult GameState::BuyBusiness(const std::string& BusinessId)
	{
		std::string Error;
		if (!Markets.BuyBusiness(BusinessId, Funds, Clock.TotalMinutes(), &Error))
		{
			return ActionResult::Failure(Error);
		}
		const Business* Value = Markets.FindBusiness(BusinessId);
		return ActionResult::Success("You now own " + (Value ? Value->Name : BusinessId) + ".");
	}

	AttackResult GameState::Attack(const std::string& BusinessId, AttackKind Kind)
	{
		const bool bOwnMinistry = BusinessId == PlayerMinistryId;
		if (bOwnMinistry)
		{
			SyncMinistryBusiness();
		}
		const AttackResult Result = Markets.LaunchAttack(BusinessId, Kind, !bOwnMinistry, Clock.TotalMinutes(), Random);
		if (bOwnMinistry && PlayerMinistry.IsFounded())
		{
			if (Result.TrustDelta < 0.0)
			{
				PlayerMinistry.ApplyScandal(-Result.TrustDelta);
			}
			if (Result.bCollapsed)
			{
				PlayerMinistry.Dissolve();
			}
		}
		if (!Result.Headline.empty())
		{
			Notify(Result.Headline);
		}
		return Result;
	}

	CreativeWork& GameState::CreateWork(WorkKind Kind, const std::string& Title, int Hours, double Performance)
	{
		CreativeWork& Work = Ip.Create(Kind, Title, Player, Hours, Performance, Markets.PlayerReputation().Score(), Random);
		Skip(static_cast<std::int64_t>(std::max(1, Hours)) * 60);
		return Work;
	}

	ActionResult GameState::DevelopMedicine(const std::string& Title, int Hours, double Performance, bool bTargetOutbreak)
	{
		if (Player.SkillLevel(Skill::Science) < 2 && Player.SkillLevel(Skill::Medicine) < 2)
		{
			return ActionResult::Failure("You need a background in science or medicine to run a research program.");
		}
		CreativeWork& Work = Ip.Create(WorkKind::Medicine, Title, Player, Hours, Performance, Markets.PlayerReputation().Score(), Random);
		if (bTargetOutbreak && Outbreak.IsActive())
		{
			Work.Treats = Outbreak.Strain().Id;
		}
		const std::string Name = Work.Title;
		const double Quality = Work.Quality;
		Skip(static_cast<std::int64_t>(std::max(1, Hours)) * 60);
		return ActionResult::Success("Discovery complete: " + Name + " (promise " + std::to_string(static_cast<int>(Quality * 100.0)) +
			"%). Next, clinical trials.");
	}

	TrialResult GameState::RunTrial(const std::string& WorkId)
	{
		const CreativeWork* Work = Ip.Find(WorkId);
		const bool bFastTrack = Work && !Work->Treats.empty() && Outbreak.IsActive() && Work->Treats == Outbreak.Strain().Id;
		const TrialResult Result = Ip.AdvanceTrial(WorkId, bFastTrack, Funds, Clock.TotalMinutes(), Random);
		if (!Result.Message.empty())
		{
			Notify(Result.Message);
		}
		return Result;
	}

	TreatmentQuote GameState::QuoteTreatment(const std::string& WorkId) const
	{
		const CreativeWork* Work = nullptr;
		for (const CreativeWork& Entry : Ip.Works())
		{
			if (Entry.Id == WorkId)
			{
				Work = &Entry;
			}
		}
		TreatmentQuote Quote;
		if (!Work || Work->Kind != WorkKind::Medicine || !Work->bApproved)
		{
			Quote.Reason = "You need an approved medicine.";
			return Quote;
		}
		if (!Outbreak.IsActive() || Work->Treats != Outbreak.Strain().Id)
		{
			Quote.Reason = "This medicine doesn't treat the current outbreak.";
			return Quote;
		}
		return QuoteTreatmentContract(Outbreak, Work->Quality, Player.SkillLevel(Skill::Business), Markets.PlayerReputation().Trust);
	}

	ActionResult GameState::SellTreatment(const std::string& WorkId, TreatmentPricing Pricing)
	{
		const TreatmentQuote Quote = QuoteTreatment(WorkId);
		if (!Quote.bValid)
		{
			return ActionResult::Failure(Quote.Reason);
		}
		const Money Price = Quote.Prices[static_cast<int>(Pricing)];
		const Money Revenue = Price * static_cast<Money>(Quote.Doses);
		const Money Production = Quote.CostPerDose * static_cast<Money>(Quote.Doses);
		Funds.Deposit(Revenue - Production, MoneyCategory::Business, "Treatment contract", Clock.TotalMinutes());
		Outbreak.DeployTreatment(0.6);
		if (Scheme.IsLive())
		{
			// The crew bargained for a share of exactly this money, and they're watching the news too.
			Scheme.SharePayout(Revenue - Production, false);
			Notify("Your crew saw the contract announcement. They want their cuts.");
		}
		Reputation& Rep = Markets.PlayerReputation();
		const std::int64_t Minute = Clock.TotalMinutes();
		const std::string Count = std::to_string(Quote.Doses);
		switch (Pricing)
		{
		case TreatmentPricing::AtCost:
			Rep.Trust = Clamp(Rep.Trust + 0.15, 0.0, 1.0);
			Rep.Sentiment = Clamp(Rep.Sentiment + 0.5, -1.0, 1.0);
			Rep.Followers += 3000;
			Markets.PostNews(PlayerName + " supplies " + Count + " doses at cost: \"People first.\"", "", 0.5, Minute);
			break;
		case TreatmentPricing::Fair:
			Rep.Trust = Clamp(Rep.Trust + 0.05, 0.0, 1.0);
			Rep.Followers += 1000;
			Markets.PostNews("State signs a fair deal for " + Count + " doses of " + PlayerName + "'s treatment", "", 0.2, Minute);
			break;
		case TreatmentPricing::Market:
			Markets.PostNews("State buys " + Count + " doses of a new treatment", "", 0.0, Minute);
			break;
		case TreatmentPricing::Gouge:
			Rep.Trust = Clamp(Rep.Trust - 0.3, 0.0, 1.0);
			Rep.Sentiment = Clamp(Rep.Sentiment - 0.6, -1.0, 1.0);
			Markets.PostNews("Outrage as " + PlayerName + " charges the state top dollar for outbreak treatment", "", -0.6, Minute);
			if (Politics.Current() != Office::None)
			{
				Politics.Scandal(0.5);
			}
			if (PlayerMinistry.IsFounded())
			{
				PlayerMinistry.ApplyScandal(0.15);
			}
			break;
		case TreatmentPricing::Count:
			break;
		}
		return ActionResult::Success("Contract signed: " + Count + " doses at " + FormatMoney(Price) + " each. Clinics start treating patients today.");
	}

	// ==================================================================================== Breaking out

	namespace
	{
		/** The strongest power the player has, as a rough 0..3 potency, for escape odds. */
		double EscapePotency(const PowerSet& Powers, const PowerRoster& Roster)
		{
			double Best = 0.0;
			for (const PowerInstance& Slot : Powers.Slots())
			{
				const EffectivePower Power = Roster.Evaluate(Slot);
				if (!Power.Def)
				{
					continue;
				}
				// Powers that move you or move walls are what matter behind a locked door.
				double Fit = 0.3;
				switch (Power.Def->Effect)
				{
				case PowerEffect::Teleport:
				case PowerEffect::Phasing: Fit = 1.0; break;
				case PowerEffect::SuperStrength:
				case PowerEffect::Invisibility: Fit = 0.8; break;
				case PowerEffect::Flight:
				case PowerEffect::Shapeshift:
				case PowerEffect::TimeSlow: Fit = 0.7; break;
				case PowerEffect::Telekinesis:
				case PowerEffect::SuperSpeed:
				case PowerEffect::Grapple: Fit = 0.6; break;
				default: Fit = 0.3; break;
				}
				Best = std::max(Best, Fit * (0.5 + 0.1 * Slot.Level) * std::max(0.5, Power.Potency));
			}
			return Best;
		}
	}

	EscapeOdds GameState::QuoteEscape(EscapePlan Plan) const
	{
		return Justice.QuoteEscape(Plan, Player, EscapePotency(PlayerPowers, Powers), Funds.Balance());
	}

	ActionResult GameState::PlanEscape(EscapePlan Plan)
	{
		const EscapeOdds Odds = QuoteEscape(Plan);
		if (!Odds.bAvailable)
		{
			return ActionResult::Failure(Odds.Reason.empty() ? "You can't work that angle." : Odds.Reason);
		}
		bool bPaid = true;
		if (Odds.Cost > 0)
		{
			const char* What = Plan == EscapePlan::BribeGuard ? "Payment to a correctional officer" : "Payment to an outside crew";
			if (!Funds.TrySpend(Odds.Cost, MoneyCategory::Other, What, Clock.TotalMinutes()))
			{
				return ActionResult::Failure("That costs " + FormatMoney(Odds.Cost) + ".");
			}
			if (Plan == EscapePlan::BribeGuard)
			{
				// Handing money to an officer is its own felony, whether or not the escape ever happens.
				OpenCharges.push_back(Crime::Bribery);
			}
		}
		Justice.StartEscape(Plan, bPaid);
		return ActionResult::Success(std::string(EscapePlanName(Plan)) + ": " + std::to_string(Odds.PrepDays) +
			" night(s) of groundwork, then roughly " + std::to_string(static_cast<int>(std::lround(Odds.Success * 100.0))) + "% odds.");
	}

	ActionResult GameState::AbandonEscape()
	{
		if (Justice.Escape().Plan == EscapePlan::None)
		{
			return ActionResult::Failure("You have nothing going.");
		}
		Justice.AbandonEscape();
		return ActionResult::Success("You let it go. Money spent stays spent.");
	}

	EscapeResult GameState::PrepareEscape()
	{
		const EscapeResult Result = Justice.PrepareEscape(Player, EscapePotency(PlayerPowers, Powers), Funds.Balance(), Random);
		if (!Result.Summary.empty())
		{
			Notify(Result.Summary);
		}
		Player.AddSkillXp(Skill::Trades, 8);
		Skip(GameClock::MinutesPerDay);
		return Result;
	}

	EscapeResult GameState::AttemptEscape()
	{
		const bool bPowerBreak = Justice.Escape().Plan == EscapePlan::PowerBreak;
		const EscapeResult Result = Justice.AttemptEscape(Player, EscapePotency(PlayerPowers, Powers), Funds.Balance(), Random);
		if (!Result.Summary.empty())
		{
			Notify(Result.Summary);
		}
		if (Result.bEscaped)
		{
			// Out, wanted, and the whole state hears about it.
			Wanted.AddStars(bPowerBreak ? 5 : 4);
			Markets.PlayerReputation().Trust = Clamp(Markets.PlayerReputation().Trust - 0.2, 0.0, 1.0);
			Markets.PostNews("Manhunt underway after escape from the county detention center", "", -0.4, Clock.TotalMinutes());
			if (!Jobs.empty())
			{
				Jobs.clear(); // no employer keeps a fugitive on the books
				Notify("Your employer let you go the moment your name was on the news.");
			}
			if (Politics.Current() != Office::None)
			{
				Politics.Scandal(1.0);
				Politics.Leave();
				Notify("You were removed from office the same morning.");
			}
			SendEmail(MessageTopic::General, "Horry County Sheriff's Office", "tips@sheriff.example", "Wanted: you",
				"Your face is on the evening news. Deputies are checking the places you're known to go. Lie low, leave the county, or walk "
				"into the courthouse and hand yourself in before they find you.");
		}
		else
		{
			Skip(GameClock::MinutesPerDay);
		}
		return Result;
	}

	ActionResult GameState::Surrender()
	{
		if (!Justice.IsFugitive())
		{
			return ActionResult::Failure("There's nothing to turn yourself in for.");
		}
		Justice.Surrender(Clock.TotalMinutes());
		Wanted.Clear();
		Markets.PlayerReputation().Trust = Clamp(Markets.PlayerReputation().Trust + 0.05, 0.0, 1.0);
		return ActionResult::Success("You walked into the courthouse with your hands visible. The judge noted it: " +
			std::to_string(Justice.Jail().SentenceDays) + " days.");
	}

	// ==================================================================================== The scheme

	RecruitQuote GameState::QuoteInsider(const std::string& NpcId, InsiderRole Role) const
	{
		const NpcMind* Mind = Npcs.Find(NpcId);
		if (!Mind)
		{
			RecruitQuote Quote;
			Quote.Reason = "You don't know anyone like that.";
			return Quote;
		}
		const CreativeWork* Best = nullptr;
		for (const CreativeWork& Work : Ip.Works())
		{
			if (Work.Kind == WorkKind::Medicine && (!Best || Work.Quality > Best->Quality))
			{
				Best = &Work;
			}
		}
		return Scheme.QuoteInsider(Role, Mind->Name, Mind->Affinity, Mind->Awareness, Player.SkillLevel(Skill::Business),
			Best ? Best->Quality : 0.3, Funds.Balance());
	}

	ActionResult GameState::RecruitInsider(const std::string& NpcId, InsiderRole Role)
	{
		if (Scheme.Stage() == SchemeStage::Exposed)
		{
			return ActionResult::Failure("Everyone who knows you is talking to investigators already.");
		}
		const RecruitQuote Terms = QuoteInsider(NpcId, Role);
		if (!Terms.bAvailable)
		{
			return ActionResult::Failure(Terms.Reason);
		}
		NpcMind& Mind = Npcs.GetOrCreate(NpcId);
		if (Terms.Retainer > 0 && !Funds.TrySpend(Terms.Retainer, MoneyCategory::Other, "Retainer: " + Mind.Name, Clock.TotalMinutes()))
		{
			return ActionResult::Failure("They want " + FormatMoney(Terms.Retainer) + " up front.");
		}
		Scheme.Recruit(NpcId, Mind.Name, Role, Terms);
		OpenCharges.push_back(Crime::Conspiracy);
		return ActionResult::Success(Mind.Name + " is in as your " + ToLowerAscii(InsiderRoleName(Role)) + ", for " +
			std::to_string(static_cast<int>(std::lround(Terms.Cut * 100.0))) + "% of the take.");
	}

	MatrixGame GameState::InsiderDilemma(const std::string& NpcId) const
	{
		const CreativeWork* Best = nullptr;
		for (const CreativeWork& Work : Ip.Works())
		{
			if (Work.Kind == WorkKind::Medicine && (!Best || Work.Quality > Best->Quality))
			{
				Best = &Work;
			}
		}
		for (const Insider& Member : Scheme.Insiders())
		{
			if (Member.NpcId == NpcId)
			{
				return Scheme.InsiderDilemma(Member, Best ? Best->Quality : 0.3, Funds.Balance());
			}
		}
		return MatrixGame();
	}

	ActionResult GameState::ReleaseStrain()
	{
		std::string Reason;
		if (!Scheme.CanRelease(&Reason))
		{
			return ActionResult::Failure(Reason);
		}
		if (Outbreak.IsActive())
		{
			return ActionResult::Failure("One is already going around. Sell into that one instead.");
		}
		Outbreak.Begin(Random);
		// A deliberate release spreads faster and hits harder than a seasonal bug.
		Scheme.Release();
		const std::string Name = Outbreak.Strain().Name;
		Markets.PostNews("Health officials confirm an outbreak of " + Name, "", -0.3, Clock.TotalMinutes());
		SendEmail(MessageTopic::General, "State Health Department", "alerts@health.example", "Health alert: " + Name,
			"Cases of " + Name + " are climbing unusually fast. Researchers with a promising treatment can apply for fast-track trials.");
		Notify("It's out. Nobody but your crew knows where it came from -- for now.");
		return ActionResult::Success("The strain is loose. Your treatment had better be ready.");
	}

	ActionResult GameState::PayTheCrew(bool bPay)
	{
		if (Scheme.Insiders().empty())
		{
			return ActionResult::Failure("There's nobody to pay.");
		}
		Money Owed = 0;
		for (const Insider& Member : Scheme.Insiders())
		{
			if (!Member.bFlipped)
			{
				Owed += ToMoney(ToDollars(Scheme.Take()) * Member.Cut);
			}
		}
		if (!bPay)
		{
			Scheme.SharePayout(0, false);
			return ActionResult::Success("You kept all of it. They'll notice.");
		}
		if (!Funds.TrySpend(Owed, MoneyCategory::Other, "Crew payout", Clock.TotalMinutes()))
		{
			Scheme.SharePayout(0, false);
			return ActionResult::Failure("Their cuts come to " + FormatMoney(Owed) + " and you don't have it. They've been told.");
		}
		Scheme.SharePayout(0, true);
		return ActionResult::Success("Paid in full: " + FormatMoney(Owed) + ". Everyone has a reason to stay quiet.");
	}

	ActionResult GameState::CallOffScheme()
	{
		if (!Scheme.IsActive())
		{
			return ActionResult::Failure("There's nothing to call off.");
		}
		if (Scheme.IsLive())
		{
			return ActionResult::Failure("It's already out. You can't put it back.");
		}
		Scheme.Close();
		OpenCharges.erase(std::remove(OpenCharges.begin(), OpenCharges.end(), Crime::Conspiracy), OpenCharges.end());
		return ActionResult::Success("You walked it back before anything happened. The retainers are gone.");
	}

	void GameState::SchemeExposed(std::int64_t Minute)
	{
		Scheme.MarkExposed();
		Markets.PostNews("Investigators: the " + Outbreak.Strain().Name + " outbreak was deliberate", "", -1.0, Minute);
		Reputation& Rep = Markets.PlayerReputation();
		Rep.Trust = Clamp(Rep.Trust - 0.6, 0.0, 1.0);
		Rep.Sentiment = Clamp(Rep.Sentiment - 1.0, -1.0, 1.0);
		Rep.Followers = std::max(0, Rep.Followers / 4);
		if (Politics.Current() != Office::None)
		{
			Politics.Scandal(1.0);
			Politics.Leave();
		}
		if (PlayerMinistry.IsFounded())
		{
			PlayerMinistry.ApplyScandal(0.6);
		}
		// Every treatment contract is void, and the state wants its money back.
		const Money Clawback = std::min(Funds.Balance(), Scheme.Take());
		if (Clawback > 0)
		{
			Funds.Charge(Clawback, MoneyCategory::Fine, "Voided treatment contract", Minute);
		}
		for (const Crime Charge : Scheme.Charges())
		{
			OpenCharges.push_back(Charge);
		}
		Wanted.AddStars(5);
		SendEmail(MessageTopic::General, "State Attorney General", "cases@ag.example", "You are under indictment",
			"A grand jury has returned charges connected to the origin of the outbreak. Surrender yourself and retain counsel.");
		Notify("It's over. The state knows what you did, and they're coming with everything.");
	}

	// ==================================================================================== Events

	Money GameState::EventEntryFee(EventKind Kind, int Tier) const
	{
		const int T = Clamp(Tier, 1, 5);
		switch (Kind)
		{
		case EventKind::DragRace: return Dollars(150) * T;
		case EventKind::DemolitionDerby: return Dollars(100) * T;
		case EventKind::MonsterJam: return Dollars(250) * T;
		case EventKind::Formula: return Dollars(1000) * T;
		case EventKind::Count: break;
		}
		return 0;
	}

	ActionResult GameState::EnterEvent(EventKind Kind, int Tier)
	{
		const Money Fee = EventEntryFee(Kind, Tier);
		if (!Funds.TrySpend(Fee, MoneyCategory::Event, std::string("Entry: ") + EventKindName(Kind), Clock.TotalMinutes()))
		{
			return ActionResult::Failure("Entry costs " + FormatMoney(Fee) + ".");
		}
		return ActionResult::Success("You're in. Good luck!");
	}

	Money GameState::FinishEvent(EventKind Kind, int Tier, int Position, int FieldSize)
	{
		const Money Prize = EventPrize(Kind, Tier, Position, FieldSize);
		if (Prize > 0)
		{
			Funds.Deposit(Prize, MoneyCategory::Event, std::string(EventKindName(Kind)) + " prize", Clock.TotalMinutes());
		}
		if (Position == 1)
		{
			EventsWon += 1;
			Markets.PlayerReputation().Followers += 200 * Clamp(Tier, 1, 5);
			Player.AddSkillXp(Skill::Driving, 60);
		}
		else
		{
			Player.AddSkillXp(Skill::Driving, 25);
		}
		return Prize;
	}

	Money GameState::Busted(const std::string& CoDefendantId)
	{
		if (Politics.Current() != Office::None)
		{
			Politics.Scandal(0.4);
			Markets.PlayerReputation().Trust = Clamp(Markets.PlayerReputation().Trust - 0.1, 0.0, 1.0);
		}
		if (Justice.IsFugitive())
		{
			// Picked up on the run: the escape goes on the record and the rest of the sentence resumes,
			// on top of whatever they were caught doing today.
			Justice.Recapture(Clock.TotalMinutes());
			Wanted.Clear();
			OpenCharges.clear();
			Notify("They caught up with you. You're back inside, and the escape charge is on your record: " +
				std::to_string(Justice.Jail().SentenceDays) + " days.");
			return 0;
		}
		std::vector<Crime> Charges = OpenCharges;
		if (Charges.empty())
		{
			Charges.push_back(Crime::EvadingPolice);
		}
		// Evidence: more stars means more witnesses and footage.
		const double Evidence = 0.35 + 0.1 * Wanted.Stars();
		std::string CoName;
		if (!CoDefendantId.empty())
		{
			CoName = Npcs.GetOrCreate(CoDefendantId).Name;
		}
		Wanted.Clear();
		OpenCharges.clear();
		if (!Justice.Arrest(Charges, Evidence, CoDefendantId, CoName, Funds.Balance()))
		{
			const Money Owed = Justice.Case().Fine;
			const Money Taken = Funds.Charge(Owed, MoneyCategory::Fine, "Police fine", Clock.TotalMinutes());
			// What was taken is settled. What was not -- a player with less in their pocket than the
			// fine -- stays owed, to be paid at the station window; without this the whole fine stayed
			// on the record and could be collected a second time.
			Justice.ReduceFine(Taken);
			Notify(Taken >= Owed
				? "Busted. You paid " + FormatMoney(Taken) + " in fines."
				: "Busted. You paid " + FormatMoney(Taken) + "; " + FormatMoney(Owed - Taken) +
					  " is still owed at the station.");
			return Taken;
		}
		Notify("Busted. You're booked and arraigned: " + std::to_string(Charges.size()) + " charge(s). Talk to a lawyer before you decide how to plead.");
		return 0;
	}

	ActionResult GameState::HireLawyer()
	{
		if (!Justice.Case().bOpen || Justice.Case().bLawyer)
		{
			return ActionResult::Failure("You don't need a lawyer right now.");
		}
		const Money Fee = JusticeSystem::LawyerFee(Justice.Case().TrialDays);
		if (!Funds.TrySpend(Fee, MoneyCategory::Fine, "Defense attorney", Clock.TotalMinutes()))
		{
			return ActionResult::Failure("A good defense attorney costs " + FormatMoney(Fee) + ". The public defender will take your case.");
		}
		Justice.HireLawyer();
		return ActionResult::Success("Your attorney is reviewing the evidence. Your plea offer just got better.");
	}

	void GameState::ApplyVerdict(const Verdict& Result)
	{
		if (!Result.bDecided)
		{
			return;
		}
		Notify(Result.Summary);
		if (Result.bConvicted && Politics.Current() != Office::None && Justice.HasFelony())
		{
			Politics.Scandal(1.0);
			Politics.Leave();
			Notify("The conviction forced you out of office.");
		}
		if (Result.SentenceDays > 0)
		{
			Justice.BeginSentence(Result.SentenceDays, Clock.TotalMinutes());
		}
	}

	Verdict GameState::TakePlea()
	{
		const Verdict Result = Justice.TakePlea(Player.SkillLevel(Skill::People), Clock.TotalMinutes());
		ApplyVerdict(Result);
		return Result;
	}

	Verdict GameState::GoToTrial()
	{
		const Verdict Result = Justice.GoToTrial(Player.SkillLevel(Skill::People), Random, Clock.TotalMinutes());
		ApplyVerdict(Result);
		return Result;
	}

	Verdict GameState::CoDefendantChoice(bool bTestify)
	{
		const std::string OtherId = Justice.Case().CoDefendantId;
		bool bOtherTestifies = false;
		if (!OtherId.empty())
		{
			// The co-defendant plays the repeated game: how the player has treated them decides whether they keep quiet.
			NpcMind& Other = Npcs.GetOrCreate(OtherId);
			bOtherTestifies = !DecideCooperate(Other.Deal, Random);
			RecordOpponentMove(Other.Deal, !bTestify);
			Other.Affinity = Clamp(Other.Affinity + (bTestify ? -0.4 : 0.15), -1.0, 1.0);
		}
		const Verdict Result = Justice.CoDefendantVerdict(bTestify, bOtherTestifies, Clock.TotalMinutes());
		ApplyVerdict(Result);
		return Result;
	}

	JailDayResult GameState::ServeJailDay(JailActivity Activity)
	{
		const JailDayResult Result = Justice.ServeDay(Activity, Player, Clock.TotalMinutes());
		if (Result.Earned > 0)
		{
			Funds.Deposit(Result.Earned, MoneyCategory::Job, "Jail job", Clock.TotalMinutes());
		}
		Skip(GameClock::MinutesPerDay);
		if (Result.bReleased)
		{
			Notify("Released from county jail.");
		}
		return Result;
	}

	ActionResult GameState::RequestParole()
	{
		if (!Justice.CanRequestParole(Clock.TotalMinutes()))
		{
			return ActionResult::Failure("You're not eligible for a parole hearing yet.");
		}
		if (Justice.ParoleHearing(Clock.TotalMinutes(), Random))
		{
			return ActionResult::Success("Parole granted. Stay out of trouble.");
		}
		return ActionResult::Success("Parole denied. You can ask again in 30 days.");
	}

	ActionResult GameState::FileAppeal()
	{
		if (!InJail())
		{
			return ActionResult::Failure("There's nothing to appeal.");
		}
		const Money Cost = JusticeSystem::AppealCost(Justice.Jail().SentenceDays);
		if (!Funds.TrySpend(Cost, MoneyCategory::Fine, "Appeal", Clock.TotalMinutes()))
		{
			return ActionResult::Failure("An appeal costs " + FormatMoney(Cost) + ".");
		}
		if (Justice.Appeal(Random))
		{
			return ActionResult::Success("Conviction overturned on appeal! You're released and your record is cleared.");
		}
		return ActionResult::Success("The appeals court upheld your conviction.");
	}

	// ---------------------------------------------------------------------- Cheats

	ActionResult GameState::EnterCheat(const std::string& Typed)
	{
		const CheatDef* Def = FindCheat(Typed);
		if (Def == nullptr)
		{
			return ActionResult::Failure("That number rings out.");
		}
		const std::int64_t Now = Clock.TotalMinutes();
		auto Confirm = [&](const std::string& What)
		{
			// Every cheat answers. There is no state the player cannot see.
			Deliver(Message{}, nullptr);
			return ActionResult::Success(What);
		};

		switch (Def->Which)
		{
		case Cheat::Payday:
		{
			const Money Amount = Dollars(250000);
			Funds.Deposit(Amount, MoneyCategory::Other, "Payday", Now);
			Cheats.Fired(Def->Which);
			return Confirm(FormatMoney(Amount) + " in the account. Nobody asked where from.");
		}
		case Cheat::Landslide:
		{
			const int Next = static_cast<int>(Politics.Current()) + 1;
			if (Next >= static_cast<int>(Office::Count))
			{
				Cheats.Fired(Def->Which);
				return Confirm("There is no higher office to hand you.");
			}
			Politics.WinNow(static_cast<Office>(Next));
			Cheats.Fired(Def->Which);
			return Confirm(std::string("Sworn in. You are ") + OfficeTitle(Politics.Current()) + " now.");
		}
		case Cheat::Showroom:
		{
			OwnedVehicle* Car = nullptr;
			for (OwnedVehicle& Owned : Garage)
			{
				if (Owned.Uid == ActiveVehicleUid)
				{
					Car = &Owned;
					break;
				}
			}
			if (Car == nullptr && !Garage.empty())
			{
				Car = &Garage.front();
			}
			if (Car == nullptr)
			{
				return ActionResult::Failure("You do not have a car.");
			}
			Car->Condition = 1.0;
			Car->Issues.clear();
			Cheats.Fired(Def->Which);
			return Confirm("Straight, clean and running right again.");
		}
		case Cheat::BarnFind:
		{
			// Something genuinely old, so it arrives as a project rather than a prize.
			const VehicleModel* Pick = nullptr;
			int Oldest = 9999;
			for (const VehicleModel& Model : Vehicles.Models())
			{
				if (Model.bEventOnly || Model.YearStart <= 0)
				{
					continue;
				}
				const int Jitter = Random.RangeInt(0, 40);
				if (Model.YearStart + Jitter < Oldest)
				{
					Oldest = Model.YearStart + Jitter;
					Pick = &Model;
				}
			}
			if (Pick == nullptr)
			{
				return ActionResult::Failure("Nothing turned up.");
			}
			const std::string Uid = GrantVehicle(Pick->Id);
			for (OwnedVehicle& Owned : Garage)
			{
				if (Owned.Uid == Uid)
				{
					Owned.Condition = 0.18 + 0.12 * Random.NextDouble();
					Owned.MileageKm = 140000.0 + 90000.0 * Random.NextDouble();
				}
			}
			Cheats.Fired(Def->Which);
			return Confirm("There is a " + Pick->DisplayName + " under a tarp in your garage. It does not run.");
		}
		case Cheat::ColdCase:
			Wanted.Clear();
			OpenCharges.clear();
			Cheats.Fired(Def->Which);
			return Confirm("The radios went quiet. Nobody is looking for you.");
		case Cheat::MostWanted:
			Wanted.AddStars(WantedSystem::MaxStars);
			Cheats.Fired(Def->Which);
			return Confirm("Every unit in the county, and they all know the car.");
		case Cheat::TimeServed:
			if (!Justice.Jail().bInJail)
			{
				return ActionResult::Failure("You are not in jail.");
			}
			Justice.Release();
			Cheats.Fired(Def->Which);
			return Confirm("The gate opened. The conviction is still on your record.");
		case Cheat::CleanSlate:
			Justice.Pardon();
			Cheats.Fired(Def->Which);
			return Confirm("Sealed, all of it. On paper you have never been arrested.");
		case Cheat::AllThree:
		{
			if (PlayerPowers.HasPowers())
			{
				return ActionResult::Failure("You already have your three.");
			}
			std::vector<std::string> Picks;
			for (const PowerDef& Power : Powers.Powers())
			{
				if (Picks.size() < PowerSet::SlotCount)
				{
					Picks.push_back(Power.Id);
				}
			}
			std::string Error;
			if (Picks.size() < PowerSet::SlotCount || !PlayerPowers.Choose(Powers, Picks, &Error))
			{
				return ActionResult::Failure(Error.empty() ? "The powers did not take." : Error);
			}
			Cheats.Fired(Def->Which);
			return Confirm("All three, at once. That is not how it is supposed to work.");
		}
		case Cheat::FullCharge:
		{
			if (!PlayerPowers.HasPowers())
			{
				return ActionResult::Failure("You have no powers to charge.");
			}
			// Instability is the real price of using them, and this is what wipes it.
			std::vector<PowerInstance> Slots = PlayerPowers.Slots();
			for (PowerInstance& Slot : Slots)
			{
				Slot.Instability = 0.0;
			}
			PlayerPowers.Restore(std::move(Slots));
			Cheats.Fired(Def->Which);
			return Confirm("Steady again. Nothing is straining.");
		}
		case Cheat::Reroll:
		{
			if (!PlayerPowers.HasPowers())
			{
				return ActionResult::Failure("You have no powers to re-roll.");
			}
			MutationEvent Event;
			int Changed = 0;
			for (int Attempt = 0; Attempt < PowerSet::SlotCount; ++Attempt)
			{
				if (PlayerPowers.Surge(Powers, Random, Event))
				{
					++Changed;
				}
			}
			Cheats.Fired(Def->Which);
			return Confirm(Changed > 0
				? "Something shifted. They are not quite what they were."
				: "Nothing took this time.");
		}
		case Cheat::FourSeasons:
		{
			const int Next = (static_cast<int>(Weather.Current()) + 1) % static_cast<int>(dtg::sim::Weather::Count);
			Weather.Force(static_cast<dtg::sim::Weather>(Next), Now, 180);
			Cheats.Fired(Def->Which);
			return Confirm("The sky changed its mind.");
		}
		case Cheat::KeysToTheCity:
		{
			const bool bOn = Cheats.Toggle(Def->Which);
			if (bOn)
			{
				RefreshDealerStock();
			}
			return Confirm(bOn
				? "Every lot in the state just found the keys to everything."
				: "The lots went back to their usual stock.");
		}
		case Cheat::Sundown:
		{
			const bool bOn = Cheats.Toggle(Def->Which);
			if (bOn)
			{
				Sundown.Begin(City, &Outbreak, Clock.Day(), Random);
				return Confirm("The power went out an hour ago and it is not coming back on. "
					"Whatever you are going to do, do it before dark.");
			}
			Sundown.End();
			return Confirm("The lights are back. Everyone is where they were.");
		}
		// The rest are read by the engine layer rather than acted on here: physics, traffic density,
		// the clock, what a showroom will sell you. The simulation only has to remember which are on.
		case Cheat::BlueChip:
		case Cheat::GroundFloor:
		case Cheat::Titanium:
		case Cheat::StickyTires:
		case Cheat::HighNoon:
		case Cheat::FeatherFall:
		case Cheat::HalfSpeed:
		case Cheat::GhostTown:
		case Cheat::RushHour:
		{
			const bool bOn = Cheats.Toggle(Def->Which);
			return Confirm(std::string(Def->Title) + (bOn ? ": on." : ": off."));
		}
		case Cheat::Count:
			break;
		}
		return ActionResult::Failure("That number rings out.");
	}

	ActionResult GameState::ExpungeRecord()
	{
		const Money Fee = Dollars(1500);
		if (!Funds.CanAfford(Fee))
		{
			return ActionResult::Failure("An expungement petition costs " + FormatMoney(Fee) + ".");
		}
		if (!Justice.Expunge(Clock.TotalMinutes()))
		{
			return ActionResult::Failure("Nothing on your record is old enough to expunge (three years).");
		}
		Funds.TrySpend(Fee, MoneyCategory::Fine, "Expungement", Clock.TotalMinutes());
		return ActionResult::Success("The court granted your petition. Old convictions are sealed.");
	}

	// ==================================================================================== Fighting

	std::string GameState::EngageFighter(const std::string& Id, const std::string& Name, bool bSuperhuman, int Threat)
	{
		Fighter Who;
		Who.Id = Id;
		Who.Name = Name.empty() ? Npcs.GetOrCreate(Id).Name : Name;
		Who.bSuperhuman = bSuperhuman;
		Who.Threat = Clamp(Threat, 0, 5);
		if (bSuperhuman)
		{
			Who.MaxHealth = 220.0 + 90.0 * Who.Threat;
			Who.Skill = Clamp(0.4 + 0.1 * Who.Threat, 0.0, 0.95);
			Who.Bounty = Dollars(1500) * (Who.Threat + 1);
		}
		else
		{
			// An ordinary person fights like one, and what they can take depends on how fit they are.
			const NpcMind& Mind = Npcs.GetOrCreate(Id);
			Who.MaxHealth = 70.0 + 6.0 * Mind.SkillLevel(Skill::Fitness);
			Who.Skill = Clamp(0.1 + 0.06 * Mind.SkillLevel(Skill::Fitness), 0.0, 0.8);
		}
		Who.Health = Who.MaxHealth;
		return Fights.Engage(Who).Id;
	}

	std::string GameState::EngageSuperhuman(Alignment Side, int Threat)
	{
		const Superhuman Who = GenerateSuperhuman(Powers, Side, Threat, Random);
		const std::string Id = EngageFighter(Who.Id, Who.Codename, true, Who.Threat);
		Notify(Who.Codename + " is here. " + Who.Motive);
		return Id;
	}

	StrikeResult GameState::Strike(const std::string& Id, StrikeKind Kind, int PowerSlot)
	{
		double Potency = 0.0;
		if (Kind == StrikeKind::Power && PowerSlot >= 0)
		{
			// Fighting with a power is using it: it levels and mutates exactly as it would anywhere else.
			const PowerUse Used = UsePower(PowerSlot, std::vector<std::string>{Id}, true);
			if (!Used.bOk)
			{
				StrikeResult Blocked;
				Blocked.Summary = Used.Message;
				return Blocked;
			}
			Potency = Used.Power.Potency;
		}
		const StrikeResult Result = Fights.Strike(Id, Kind, Player.SkillLevel(Skill::Fitness), Potency, Random);
		if (!Result.bLanded)
		{
			return Result;
		}
		Player.AddSkillXp(Skill::Fitness, 6);
		if (Result.bWasInnocent)
		{
			// Hitting someone who isn't fighting back is assault, whoever started it.
			ReportCrime(Crime::Assault, false, true);
			NpcMind& Mind = Npcs.GetOrCreate(Id);
			Mind.Affinity = Clamp(Mind.Affinity - 0.5, -1.0, 1.0);
			Npcs.Observe(Mind, Stimulus::SawVillain, 0.7, Clock.TotalMinutes(), Random, "Attacked by " + PlayerName, &Powers);
		}
		if (Result.bKnockedOut)
		{
			Notify(Result.Summary);
			if (Result.Bounty > 0)
			{
				Funds.Deposit(Result.Bounty, MoneyCategory::Reward, "Bounty", Clock.TotalMinutes());
				Markets.PlayerReputation().Followers += 400;
				Markets.PlayerReputation().Trust = Clamp(Markets.PlayerReputation().Trust + 0.03, 0.0, 1.0);
				Notify("Bounty paid: " + FormatMoney(Result.Bounty) + ".");
			}
			Fights.Disengage(Id);
		}
		return Result;
	}

	std::vector<std::string> GameState::FightBack()
	{
		std::vector<std::string> Messages;
		Fights.OpponentsAct(Player.SkillLevel(Skill::Fitness), Random, Messages);
		if (Fights.PlayerIsDown())
		{
			// Waking up at the hospital costs money and ends the fight.
			Hospitalized();
			Fights.ReviveAtHospital();
			Fights.Clear();
			Messages.push_back("You woke up at the hospital.");
		}
		return Messages;
	}

	Money GameState::Hospitalized()
	{
		const Money Bill = Dollars(500) + std::max<Money>(0, Funds.Balance() / 10);
		const Money Taken = Funds.Charge(Bill, MoneyCategory::Medical, "Hospital bill", Clock.TotalMinutes());
		Wanted.Clear();
		Notify("You woke up at Mercy General. The bill: " + FormatMoney(Taken) + ".");
		return Taken;
	}

	Money GameState::NetWorth() const
	{
		Money Total = Funds.Balance() + Markets.PortfolioValue();
		for (const OwnedVehicle& Car : Garage)
		{
			if (const VehicleModel* Model = Vehicles.FindModel(Car.ModelId))
			{
				Total += ResaleValue(*Model, Car);
			}
		}
		for (const std::string& Id : Homes.OwnedProperties())
		{
			if (const Property* Home = Homes.FindProperty(Id))
			{
				Total += Home->Price;
			}
		}
		for (const Business& Value : Markets.Businesses())
		{
			if (Value.PlayerEquity > 0.0)
			{
				Total += static_cast<Money>(std::llround(static_cast<double>(Markets.Valuation(Value)) * Value.PlayerEquity));
			}
		}
		return Total;
	}

	// ==================================================================================== Used cars

	void GameState::RefreshUsedCars()
	{
		UsedCars.Refresh(Vehicles, Random, 10);
	}

	HaggleResult GameState::HaggleUsed(const std::string& ListingId, Money Offer)
	{
		return UsedCars.Haggle(ListingId, Offer, Vehicles, Player, Random);
	}

	ActionResult GameState::BuyUsed(const std::string& ListingId, Money Price)
	{
		UsedListing* Listing = UsedCars.Find(ListingId);
		if (!Listing)
		{
			return ActionResult::Failure("That car already sold.");
		}
		if (!Funds.TrySpend(Price, MoneyCategory::Vehicle, "Used car", Clock.TotalMinutes()))
		{
			return ActionResult::Failure("You need " + FormatMoney(Price) + ".");
		}
		const DealerKind Kind = Listing->Dealer;
		OwnedVehicle Car;
		UsedCars.Take(ListingId, Car);
		Car.Uid = "car-" + std::to_string(NextVehicleNumber++);
		if (Kind == DealerKind::Certified)
		{
			Car.WarrantyUntilMinute = Clock.TotalMinutes() + 365 * GameClock::MinutesPerDay;
		}
		Garage.push_back(Car);
		ActiveVehicleUid = Car.Uid;
		switch (Kind)
		{
		case DealerKind::Certified: return ActionResult::Success("Certified pre-owned, with a one-year warranty. Enjoy it.");
		case DealerKind::Classic: return ActionResult::Success("A piece of history is yours. Treat it kindly.");
		case DealerKind::Located: return ActionResult::Success("The dealership delivered your car. It's parked out front.");
		default: return ActionResult::Success("It's yours. Check it over in the garage before you drive it hard.");
		}
	}

	RepairQuote GameState::Quote(const std::string& Uid, CarIssue Issue) const
	{
		const OwnedVehicle* Car = FindOwned(Uid);
		const VehicleModel* Model = Car ? Vehicles.FindModel(Car->ModelId) : nullptr;
		if (!Model)
		{
			return RepairQuote();
		}
		return QuoteRepair(*Model, Issue, Player);
	}

	RepairResult GameState::RepairDiy(const std::string& Uid, CarIssue Issue, double Performance)
	{
		RepairResult Result;
		OwnedVehicle* Car = FindOwned(Uid);
		if (!Car || !Car->Has(Issue))
		{
			return Result;
		}
		const RepairQuote QuoteValue = Quote(Uid, Issue);
		if (!Funds.CanAfford(QuoteValue.Parts))
		{
			return Result;
		}
		Result = DiyRepair(*Car, Issue, QuoteValue, Player, Performance, Random);
		Funds.TrySpend(Result.PartsSpent, MoneyCategory::Vehicle, std::string("Parts: ") + GetIssueInfo(Issue).Name, Clock.TotalMinutes());
		Skip(static_cast<std::int64_t>(Result.Hours) * 60);
		return Result;
	}

	ActionResult GameState::RepairAtShop(const std::string& Uid, CarIssue Issue)
	{
		OwnedVehicle* Car = FindOwned(Uid);
		if (!Car || !Car->Has(Issue))
		{
			return ActionResult::Failure("Nothing to fix.");
		}
		if (Car->UnderWarranty(Clock.TotalMinutes()) && !GetIssueInfo(Issue).bCosmetic)
		{
			ShopRepair(*Car, Issue);
			return ActionResult::Success(std::string(GetIssueInfo(Issue).Name) + " fixed under warranty. No charge.");
		}
		const RepairQuote QuoteValue = Quote(Uid, Issue);
		const Money Total = QuoteValue.Parts + QuoteValue.ShopLabor;
		if (!Funds.TrySpend(Total, MoneyCategory::Vehicle, std::string("Shop: ") + GetIssueInfo(Issue).Name, Clock.TotalMinutes()))
		{
			return ActionResult::Failure("The shop wants " + FormatMoney(Total) + ".");
		}
		ShopRepair(*Car, Issue);
		return ActionResult::Success(std::string(GetIssueInfo(Issue).Name) + " fixed by the shop.");
	}

	Money GameState::PrivateOffer(const std::string& Uid)
	{
		const OwnedVehicle* Car = FindOwned(Uid);
		const VehicleModel* Model = Car ? Vehicles.FindModel(Car->ModelId) : nullptr;
		return Model ? PrivateSaleOffer(*Model, *Car, Parts, Random) : 0;
	}

	ActionResult GameState::SellPrivately(const std::string& Uid, Money Price)
	{
		for (auto It = Garage.begin(); It != Garage.end(); ++It)
		{
			if (It->Uid == Uid)
			{
				Funds.Deposit(Price, MoneyCategory::Vehicle, "Private sale", Clock.TotalMinutes());
				if (ActiveVehicleUid == Uid)
				{
					ActiveVehicleUid.clear();
				}
				Garage.erase(It);
				Player.AddSkillXp(Skill::Business, 15);
				return ActionResult::Success("Sold for " + FormatMoney(Price) + ".");
			}
		}
		return ActionResult::Failure("You don't own that car.");
	}

	VehicleSetup GameState::SetupForOwned(const OwnedVehicle& Car) const
	{
		VehicleSetup Setup = SetupFor(Car.ModelId, &Car.Build);
		ApplyIssues(Setup, Car.Issues);
		return Setup;
	}

	// ==================================================================================== Politics

	Money GameState::NewCarPrice(const VehicleModel& Model) const
	{
		return static_cast<Money>(std::llround(static_cast<double>(Model.MarketPrice) * ActiveLaws().VehiclePriceMul));
	}

	void GameState::RefreshDealerStock()
	{
		UsedCars.ReplaceStock(DealerKind::Classic, std::string(), GenerateClassics(Vehicles, Random, 14));
		std::vector<std::string> Showrooms;
		for (const Brand& Maker : Vehicles.Brands())
		{
			if (Maker.HasShowroom(Vehicles.CurrentYear()) &&
				std::find(Showrooms.begin(), Showrooms.end(), Maker.ShowroomId()) == Showrooms.end())
			{
				Showrooms.push_back(Maker.ShowroomId());
			}
		}
		for (const std::string& Showroom : Showrooms)
		{
			std::vector<std::string> BrandIds;
			for (const Brand* Maker : BrandsAtShowroom(Showroom))
			{
				BrandIds.push_back(Maker->Id);
			}
			UsedCars.ReplaceStock(DealerKind::Certified, Showroom, GenerateCertified(Vehicles, BrandIds, Showroom, Random, 4));
		}
	}

	ActionResult GameState::LocateModel(const std::string& ShowroomId, const std::string& ModelId)
	{
		const VehicleModel* Model = Vehicles.FindModel(ModelId);
		if (!Model || Model->bEventOnly)
		{
			return ActionResult::Failure("Nobody can find one of those.");
		}
		bool bSold = false;
		for (const Brand* Maker : BrandsAtShowroom(ShowroomId))
		{
			bSold = bSold || Maker->Id == Model->BrandId;
		}
		if (!bSold)
		{
			return ActionResult::Failure("This showroom doesn't deal in that brand.");
		}
		if (Model->IsCurrent())
		{
			return ActionResult::Failure("That one's on sale new. No need to hunt for it.");
		}
		UsedListing Listing = LocateExample(*Model, ShowroomId, Random);
		const std::string Price = FormatMoney(Listing.AskingPrice);
		UsedCars.Add(Listing);
		return ActionResult::Success("Found one: " + Model->DisplayName + " for " + Price + ". It's waiting at the showroom.");
	}

	RestorationQuote GameState::QuoteRestore(const std::string& Uid, RestorationLevel Level, bool bCrateEngine) const
	{
		const OwnedVehicle* Car = FindOwned(Uid);
		const VehicleModel* Model = Car ? Vehicles.FindModel(Car->ModelId) : nullptr;
		if (!Car || !Model)
		{
			RestorationQuote Missing;
			Missing.Reason = "No such car.";
			return Missing;
		}
		return QuoteRestoration(*Model, *Car, Level, Player, Parts, bCrateEngine);
	}

	ActionResult GameState::RestoreAtShop(const std::string& Uid, RestorationLevel Level, bool bCrateEngine)
	{
		const RestorationQuote QuoteValue = QuoteRestore(Uid, Level, bCrateEngine);
		OwnedVehicle* Car = FindOwned(Uid);
		if (!Car || !QuoteValue.bAllowed)
		{
			return ActionResult::Failure(QuoteValue.Reason.empty() ? "Can't restore that." : QuoteValue.Reason);
		}
		if (Car->InShop(Clock.TotalMinutes()))
		{
			return ActionResult::Failure("It's already in the shop.");
		}
		if (!Funds.TrySpend(QuoteValue.ShopCost, MoneyCategory::Vehicle, RestorationName(Level), Clock.TotalMinutes()))
		{
			return ActionResult::Failure("The restoration shop wants " + FormatMoney(QuoteValue.ShopCost) + ".");
		}
		ApplyRestoration(*Car, Level, QuoteValue, Parts, 1.0);
		Car->ReadyMinute = Clock.TotalMinutes() + static_cast<std::int64_t>(QuoteValue.ShopDays) * GameClock::MinutesPerDay;
		return ActionResult::Success(std::string(RestorationName(Level)) + " booked. Ready in " + std::to_string(QuoteValue.ShopDays) + " days.");
	}

	ActionResult GameState::RestoreDiy(const std::string& Uid, RestorationLevel Level, bool bCrateEngine, double Performance)
	{
		const RestorationQuote QuoteValue = QuoteRestore(Uid, Level, bCrateEngine);
		OwnedVehicle* Car = FindOwned(Uid);
		if (!Car || !QuoteValue.bAllowed)
		{
			return ActionResult::Failure(QuoteValue.Reason.empty() ? "Can't restore that." : QuoteValue.Reason);
		}
		if (!Funds.TrySpend(QuoteValue.DiyParts, MoneyCategory::Vehicle, std::string("Parts: ") + RestorationName(Level), Clock.TotalMinutes()))
		{
			return ActionResult::Failure("Parts alone cost " + FormatMoney(QuoteValue.DiyParts) + ".");
		}
		const bool bClean = Random.Chance(QuoteValue.DiySuccessChance);
		const double Quality = Clamp(Performance, 0.0, 1.0) * (bClean ? 1.0 : 0.55);
		ApplyRestoration(*Car, Level, QuoteValue, Parts, Quality);
		if (!bClean)
		{
			Car->Condition = std::max(0.3, Car->Condition - 0.12);
		}
		Player.AddSkillXp(Skill::Trades, QuoteValue.DiyHours * 6);
		Skip(static_cast<std::int64_t>(QuoteValue.DiyHours) * 60);
		return bClean ? ActionResult::Success("Weekends well spent: the restoration came out beautifully.")
			: ActionResult::Success("It's done, but it shows a few amateur touches. Still better than before.");
	}

	ActionResult GameState::SetTransmission(bool bManual)
	{
		bManualTransmission = bManual;
		return ActionResult::Success(bManual ? "Manual: you shift (clutch is automatic)." : "Automatic: the gearbox shifts for you.");
	}

	std::vector<const Brand*> GameState::BrandsAtShowroom(const std::string& ShowroomId) const
	{
		std::vector<const Brand*> Result;
		for (const Brand& Maker : Vehicles.Brands())
		{
			if (Maker.ShowroomId() == ShowroomId && Maker.HasShowroom(Vehicles.CurrentYear()))
			{
				Result.push_back(&Maker);
			}
		}
		return Result;
	}

	const LawDef* GameState::FindLaw(const std::string& Id) const
	{
		for (const LawDef& Law : LawBook)
		{
			if (Law.Id == Id) return &Law;
		}
		return nullptr;
	}

	LawEffects GameState::ActiveLaws() const
	{
		return CombineLaws(LawBook, Politics.EnactedLaws());
	}

	ActionResult GameState::RunForOffice(Office Target)
	{
		std::string Error;
		if (Justice.HasFelony())
		{
			return ActionResult::Failure("A felony conviction keeps you off the ballot unless it's expunged or pardoned.");
		}
		if (Justice.IsFugitive())
		{
			return ActionResult::Failure("You're a wanted escapee. Filing papers would be filing your own location.");
		}
		const Reputation& Rep = Markets.PlayerReputation();
		if (!Politics.StartCampaign(Target, Rep.Followers, Rep.Trust, Rep.Score(), Funds, Clock.TotalMinutes(), Random, &Error))
		{
			return ActionResult::Failure(Error);
		}
		return ActionResult::Success(std::string("You're officially running for ") + OfficeTitle(Target) + " against " +
			Politics.ActiveCampaign().Opponent + ". Election in " + std::to_string(GetOfficeInfo(Target).CampaignDays) + " days.");
	}

	ActionResult GameState::CampaignRally(double Performance)
	{
		const double Gain = Politics.Rally(Performance);
		if (Gain <= 0.0)
		{
			return ActionResult::Failure("You aren't campaigning.");
		}
		Markets.PlayerReputation().Followers += static_cast<int>(400 * Performance) + 50;
		Skip(120);
		return ActionResult::Success("The crowd loved it. Support up " + std::to_string(static_cast<int>(std::lround(Gain * 1000.0)) / 10.0).substr(0, 4) + " points.");
	}

	ActionResult GameState::CampaignAds(Money Amount)
	{
		const double Gain = Politics.RunAds(Amount, Funds, Clock.TotalMinutes());
		if (Gain <= 0.0)
		{
			return ActionResult::Failure("The ad buy didn't happen.");
		}
		return ActionResult::Success("Ads are running across the state.");
	}

	ActionResult GameState::CampaignDebate(double Performance)
	{
		if (!Politics.ActiveCampaign().bActive)
		{
			return ActionResult::Failure("You aren't campaigning.");
		}
		const double Gain = Politics.Debate(Performance);
		Player.AddSkillXp(Skill::People, 30);
		return Gain >= 0.0 ? ActionResult::Success("You won the debate.") : ActionResult::Failure("The debate went badly.");
	}

	MatrixGame GameState::CampaignMatrix() const
	{
		return CampaignGame(Player.SkillLevel(Skill::People), Player.SkillLevel(Skill::Creativity), Markets.PlayerReputation().Trust);
	}

	CampaignRound GameState::CampaignStrategy(int Choice, double Performance)
	{
		const CampaignRound Round = Politics.PlayStrategy(CampaignMatrix(), Choice, Performance, Random);
		if (Round.bPlayed)
		{
			Skip(240);
		}
		return Round;
	}

	LawVote GameState::ProposeLaw(const std::string& LawId)
	{
		const LawDef* Law = FindLaw(LawId);
		if (!Law)
		{
			LawVote Vote;
			Vote.Message = "Unknown bill.";
			return Vote;
		}
		LawVote Vote = Politics.Propose(*Law, Random);
		Notify(Vote.Message);
		return Vote;
	}

	LawVote GameState::LobbyForLaw(const std::string& LawId, Money Amount)
	{
		const LawDef* Law = FindLaw(LawId);
		if (!Law)
		{
			LawVote Vote;
			Vote.Message = "Unknown bill.";
			return Vote;
		}
		LawVote Vote = Politics.Lobby(*Law, Amount, Funds, Clock.TotalMinutes(), Random);
		Notify(Vote.Message);
		return Vote;
	}

	void GameState::ReportCrime(Crime Value, bool bPoliceWitness, bool bCivilianWitness)
	{
		const LawEffects Laws = ActiveLaws();
		if (Value == Crime::Speeding && Laws.SpeedingHeatMul < 1.0 && !Random.Chance(Laws.SpeedingHeatMul))
		{
			return; // relaxed speed limits: most speeding goes unpunished
		}
		const double HeatBefore = Wanted.Heat();
		Wanted.ReportCrime(Value, bPoliceWitness, bCivilianWitness, Random);
		if (Wanted.Heat() > HeatBefore)
		{
			OpenCharges.push_back(Value);
		}
		if (Value == Crime::Speeding && Laws.SpeedingHeatMul > 1.0 && bPoliceWitness)
		{
			Wanted.ReportCrime(Value, true, false, Random); // stricter enforcement: counts twice
		}
	}

	double GameState::PolicePatrolChance(double CrimeGain) const
	{
		// Inspection game: fines scale with the player's means; patrols cost the city, crime costs residents.
		const double Fine = std::max(500.0, ToDollars(Funds.Balance()) * 0.05);
		const InspectionResult Result = InspectionGame(std::max(1.0, CrimeGain), Fine, 200.0, 1000.0);
		// An active manhunt puts far more cars on the road looking for one person.
		const double Hunt = Justice.IsFugitive() ? 1.0 + 1.5 * Justice.Escape().Manhunt : 1.0;
		return Clamp(Result.InspectProbability * ActiveLaws().PoliceUnitsMul * Hunt, 0.02, 0.95);
	}

	void GameState::Notify(const std::string& Text)
	{
		PendingNotifications.push_back(Text);
		if (PendingNotifications.size() > 32)
		{
			PendingNotifications.erase(PendingNotifications.begin());
		}
	}

	std::vector<std::string> GameState::TakeNotifications()
	{
		std::vector<std::string> Out;
		Out.swap(PendingNotifications);
		return Out;
	}

}
