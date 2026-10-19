#include "Sim/GameState.h"
#include "Sim/RoadMarkings.h"
#include "Sim/Vegetation.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
using namespace dtg::sim;
using Clock=std::chrono::steady_clock;
static double Ms(Clock::time_point A,Clock::time_point B){return std::chrono::duration<double,std::milli>(B-A).count();}
int main(int argc,char** argv){
	auto Read=[](const char* N){ std::ifstream I(std::string((std::getenv("DTG_DATA") ? std::getenv("DTG_DATA") : "Content/Data/"))+N); std::ostringstream O; O<<I.rdbuf(); return O.str(); };
	ContentFiles F; F.Economy=Read("economy.json"); F.Vehicles=Read("vehicles.json"); F.Parts=Read("parts.json");
	F.Careers=Read("careers.json"); F.Powers=Read("powers.json"); F.Furniture=Read("furniture.json");
	F.Companies=Read("companies.json"); F.Laws=Read("laws.json"); F.State=Read(argc>1?argv[1]:"state.json");
	GameState S; ContentReport R; S.LoadContent(F,R);
	// Average over a ring of chunks around the spawn, as the streamer would ask for them.
	double Plan=0, Build=0; size_t Plants=0, Tris=0; int N=0;
	std::vector<PlantTri> T;
	for(int j=-3;j<=3;++j)for(int i=-3;i<=3;++i){
		const double Ox=S.City.SpawnX+i*192.0, Oy=S.City.SpawnY+j*192.0;
		Rect A{Ox,Oy,Ox+192.0,Oy+192.0};
		const auto P0=Clock::now(); auto P=PlantsIn(S.Map,S.City,A); const auto P1=Clock::now();
		for(const Plant& Q:P){ T.clear(); BuildPlant(Q,T); Tris+=T.size(); }
		const auto P2=Clock::now();
		Plan+=Ms(P0,P1); Build+=Ms(P1,P2); Plants+=P.size(); ++N;
	}
	std::printf("%s  %d chunks around spawn\n", argc>1?argv[1]:"state.json", N);
	std::printf("  plants per chunk        %6.0f\n", static_cast<double>(Plants)/N);
	std::printf("  triangles per chunk     %6.0f\n", static_cast<double>(Tris)/N);
	std::printf("  choosing them           %6.2f ms/chunk\n", Plan/N);
	std::printf("  building geometry       %6.2f ms/chunk\n", Build/N);
	const auto M0=Clock::now(); size_t Marks=0;
	for(const HighwayPlan& H:S.Map.Highways) Marks+=MarkHighway(H).size();
	const auto M1=Clock::now();
	std::printf("  all road paint, once    %6.2f ms   (%zu stripes, startup only)\n", Ms(M0,M1), Marks);
	return 0;
}
