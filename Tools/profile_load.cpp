// Where the time goes between pressing play and driving.
#include "Sim/GameState.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace dtg::sim;
using Clock = std::chrono::steady_clock;
static double Ms(Clock::time_point A, Clock::time_point B)
{ return std::chrono::duration<double, std::milli>(B - A).count(); }

int main(int argc, char** argv)
{
	const char* StateFile = argc > 1 ? argv[1] : "state-northcarolina.json";
	auto Read = [](const char* N) { std::ifstream In(std::string((std::getenv("DTG_DATA") ? std::getenv("DTG_DATA") : "Content/Data/")) + N); std::ostringstream O; O << In.rdbuf(); return O.str(); };
	const auto T0 = Clock::now();
	ContentFiles F;
	F.Economy=Read("economy.json"); F.Vehicles=Read("vehicles.json"); F.Parts=Read("parts.json");
	F.Careers=Read("careers.json"); F.Powers=Read("powers.json"); F.Furniture=Read("furniture.json");
	F.Companies=Read("companies.json"); F.Laws=Read("laws.json"); F.State=Read(StateFile);
	const auto T1 = Clock::now();
	GameState S; ContentReport R;
	if (!S.LoadContent(F, R)) { for (auto& E : R.Errors) std::printf("ERR %s\n", E.c_str()); return 1; }
	const auto T2 = Clock::now();
	S.NewGame(7, "Tester");
	const auto T3 = Clock::now();
	std::size_t Buildings = S.City.Buildings.size(), Roads = S.City.Roads.size(), Nodes = S.City.Intersections.size();
	// A frame's worth of terrain sampling: what streaming costs per chunk.
	const int Cells = 64;
	const double Step = 192.0 / Cells;
	const auto T4 = Clock::now();
	double Sink = 0.0;
	for (int J = 0; J <= Cells + 2; ++J) for (int I = 0; I <= Cells + 2; ++I)
		Sink += S.Map.SurfaceAt(S.City.SpawnX + I * Step, S.City.SpawnY + J * Step).GroundM;
	const auto T5 = Clock::now();
	// And a save/load round trip.
	const std::string Saved = S.SaveToJson();
	const auto T6 = Clock::now();
	GameState Back; ContentReport R2; Back.LoadContent(F, R2);
	std::string Err;
	Back.LoadFromJson(Saved, &Err);
	const auto T7 = Clock::now();

	std::printf("%s\n", StateFile);
	std::printf("  read 9 data files      %8.1f ms\n", Ms(T0, T1));
	std::printf("  LoadContent            %8.1f ms   <- parses + generates the whole state\n", Ms(T1, T2));
	std::printf("  NewGame                %8.1f ms\n", Ms(T2, T3));
	std::printf("  one terrain chunk      %8.2f ms   (%d x %d samples)\n", Ms(T4, T5), Cells + 3, Cells + 3);
	std::printf("  SaveToJson             %8.1f ms   (%.1f MB)\n", Ms(T5, T6), Saved.size() / 1048576.0);
	std::printf("  LoadFromJson + content %8.1f ms\n", Ms(T6, T7));
	std::printf("  TOTAL to playable      %8.1f ms\n", Ms(T0, T3));
	std::printf("  world: %zu buildings, %zu roads, %zu junctions (sink %.1f)\n", Buildings, Roads, Nodes, Sink);
	return 0;
}
