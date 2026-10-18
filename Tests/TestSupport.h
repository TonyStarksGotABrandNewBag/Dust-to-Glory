#pragma once

#include "Sim/Dynamics.h"
#include "Sim/EngineAudio.h"
#include "Sim/GameState.h"

#include <cstdio>

#include <fstream>
#include <sstream>
#include <string>

namespace dtg_test
{
	inline std::string ReadDataFile(const std::string& Name)
	{
		std::ifstream File(std::string(DTG_DATA_DIR) + "/" + Name);
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		return Buffer.str();
	}

	inline dtg::sim::ContentFiles LoadAllFiles()
	{
		dtg::sim::ContentFiles Files;
		Files.Economy = ReadDataFile("economy.json");
		Files.Vehicles = ReadDataFile("vehicles.json");
		Files.Parts = ReadDataFile("parts.json");
		Files.Careers = ReadDataFile("careers.json");
		Files.Powers = ReadDataFile("powers.json");
		Files.Furniture = ReadDataFile("furniture.json");
		Files.Companies = ReadDataFile("companies.json");
		Files.Laws = ReadDataFile("laws.json");
		return Files;
	}

	/** A freshly loaded world; loading is cheap enough to do per test. */
	inline dtg::sim::GameState& LoadedState()
	{
		static dtg::sim::GameState State;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			dtg::sim::ContentReport Report;
			State.LoadContent(LoadAllFiles(), Report);
			for (const std::string& Error : Report.Errors)
			{
				std::printf("content error: %s\n", Error.c_str());
			}
			bLoaded = true;
		}
		return State;
	}

	inline dtg::sim::GameState FreshGame(std::uint64_t Seed = 42)
	{
		dtg::sim::GameState State = LoadedState();
		State.NewGame(Seed, "Tester");
		return State;
	}
}
