#include "Sim/GameState.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
using namespace dtg::sim;
using Clock = std::chrono::steady_clock;
static double Ms(Clock::time_point A, Clock::time_point B){ return std::chrono::duration<double,std::milli>(B-A).count(); }
int main(int argc, char** argv)
{
	auto Read=[](const char* N){ std::ifstream I(std::string((std::getenv("DTG_DATA") ? std::getenv("DTG_DATA") : "Content/Data/"))+N); std::ostringstream O; O<<I.rdbuf(); return O.str(); };
	ContentFiles F; F.Economy=Read("economy.json"); F.Vehicles=Read("vehicles.json"); F.Parts=Read("parts.json");
	F.Careers=Read("careers.json"); F.Powers=Read("powers.json"); F.Furniture=Read("furniture.json");
	F.Companies=Read("companies.json"); F.Laws=Read("laws.json"); F.State=Read(argc>1?argv[1]:"state-northcarolina.json");
	GameState S; ContentReport R; S.LoadContent(F, R);
	const bool bExact = argc > 2 && std::string(argv[2]) == "exact";
	if (bExact) S.Map.ForgetLookup();
	std::printf("%s\n", bExact ? "-- exhaustive (no lookup grid)" : "-- indexed");
	const StatePlan& M = S.Map;
	std::size_t Segments = 0;
	for (const auto& H : M.Highways) Segments += H.Points.size() > 0 ? H.Points.size() - 1 : 0;
	std::printf("highways=%zu segments=%zu cities=%zu outline=%zu lakes=%zu rivers=%zu venues=%zu\n",
		M.Highways.size(), Segments, M.Cities.size(), M.Outline.size(), M.Lakes.size(), M.Rivers.size(), M.Venues.size());
	const int N = 60;
	const double Step = 3.0;
	const double X0 = S.City.SpawnX, Y0 = S.City.SpawnY;
	double Sink = 0.0;
	auto Time = [&](const char* Name, auto&& Fn)
	{
		const auto A = Clock::now();
		for (int J = 0; J <= N; ++J) for (int I = 0; I <= N; ++I) Sink += Fn(X0 + I*Step, Y0 + J*Step);
		const auto B = Clock::now();
		std::printf("  %-22s %7.2f ms   %6.2f us/sample\n", Name, Ms(A,B), Ms(A,B)*1000.0/((N+1)*(N+1)));
	};
	Time("SurfaceAt (all of it)", [&](double X,double Y){ return M.SurfaceAt(X,Y).GroundM; });
	Time("NaturalHeight", [&](double X,double Y){ return M.NaturalHeight(X,Y); });
	Time("CoastDistance", [&](double X,double Y){ return M.CoastDistance(X,Y); });
	Time("IsOcean", [&](double X,double Y){ return M.IsOcean(X,Y) ? 1.0 : 0.0; });
	Time("InsideState", [&](double X,double Y){ return M.InsideState(X,Y) ? 1.0 : 0.0; });
	Time("RoadHeightAt", [&](double X,double Y){ return M.RoadHeightAt(X,Y); });
	std::printf("(sink %.1f)\n", Sink);
	return 0;
}
