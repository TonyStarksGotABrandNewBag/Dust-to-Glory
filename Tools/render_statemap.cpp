// A top-down picture of a state: terrain shaded by height, water, highways and towns.
#include "Sim/GameState.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
using namespace dtg::sim;
int main(int argc, char** argv)
{
	GameState S; ContentReport R; ContentFiles F;
	auto P = [](const char* N) { std::ifstream In(std::string((std::getenv("DTG_DATA") ? std::getenv("DTG_DATA") : "Content/Data/")) + N); std::ostringstream O; O << In.rdbuf(); return O.str(); };
	F.Economy=P("economy.json"); F.Vehicles=P("vehicles.json"); F.Parts=P("parts.json"); F.Careers=P("careers.json");
	F.Powers=P("powers.json"); F.Furniture=P("furniture.json"); F.Companies=P("companies.json"); F.Laws=P("laws.json");
	F.State=P(argc > 1 ? argv[1] : "state-northcarolina.json");
	if (!S.LoadContent(F, R)) { for (auto& E : R.Errors) std::printf("ERR %s\n", E.c_str()); return 1; }
	const StatePlan& M = S.Map;
	const int W = 1400, H = 700;
	const Rect B = M.Bounds;
	const double Pad = 0.03 * (B.MaxX - B.MinX);
	const double MinX = B.MinX - Pad, MaxX = B.MaxX + Pad, MinY = B.MinY - Pad, MaxY = B.MaxY + Pad;
	std::vector<unsigned char> Px(static_cast<std::size_t>(W * H * 3), 0);
	auto Put = [&](int X, int Y, int Rr, int Gg, int Bb) {
		if (X < 0 || Y < 0 || X >= W || Y >= H) return;
		const std::size_t A = static_cast<std::size_t>((Y * W + X) * 3);
		Px[A] = static_cast<unsigned char>(Rr); Px[A+1] = static_cast<unsigned char>(Gg); Px[A+2] = static_cast<unsigned char>(Bb);
	};
	double Peak = 1.0;
	for (int Y = 0; Y < H; ++Y) for (int X = 0; X < W; ++X) {
		const double Wx = MinX + (MaxX - MinX) * (X + 0.5) / W;
		const double Wy = MinY + (MaxY - MinY) * (Y + 0.5) / H;
		Peak = std::max(Peak, M.SurfaceAt(Wx, Wy).GroundM);
	}
	for (int Y = 0; Y < H; ++Y) for (int X = 0; X < W; ++X) {
		const double Wx = MinX + (MaxX - MinX) * (X + 0.5) / W;
		const double Wy = MinY + (MaxY - MinY) * (Y + 0.5) / H;
		const SurfaceInfo I = M.SurfaceAt(Wx, Wy);
		if (I.bOcean) { Put(X, Y, 18, 42, 78); continue; }
		if (I.bWater) { Put(X, Y, 34, 70, 104); continue; }
		if (!M.InsideState(Wx, Wy)) { Put(X, Y, 28, 30, 32); continue; }
		const double T = std::min(1.0, std::max(0.0, I.GroundM / Peak));
		Put(X, Y, static_cast<int>(60 + 170 * T), static_cast<int>(110 + 90 * T), static_cast<int>(60 + 70 * T));
	}
	auto Draw = [&](double Wx, double Wy, int Rr, int Gg, int Bb, int Size) {
		const int X = static_cast<int>((Wx - MinX) / (MaxX - MinX) * W);
		const int Y = static_cast<int>((Wy - MinY) / (MaxY - MinY) * H);
		for (int Dy = -Size; Dy <= Size; ++Dy) for (int Dx = -Size; Dx <= Size; ++Dx) Put(X + Dx, Y + Dy, Rr, Gg, Bb);
	};
	for (const HighwayPlan& Road : M.Highways) {
		const double L = Road.LengthM();
		for (double D = 0.0; D <= L; D += 15.0) { const auto A = Road.At(D); Draw(A[0], A[1], Road.bInterstate ? 240 : 200, Road.bInterstate ? 180 : 200, 60, 1); }
	}
	for (const StateCity& C : M.Cities) Draw(C.X, C.Y, 255, 60, 60, C.bStart ? 5 : 3);
	std::string Out = std::string("/tmp/claude-0/-home-claude/ed94bd4a-cb95-560a-aaa1-80e6f38f363b/scratchpad/") + (argc > 2 ? argv[2] : "map") + ".ppm";
	FILE* File = std::fopen(Out.c_str(), "wb");
	std::fprintf(File, "P6\n%d %d\n255\n", W, H);
	std::fwrite(Px.data(), 1, Px.size(), File);
	std::fclose(File);
	std::printf("%s: %zu cities, %zu highways, peak %.0f m, bounds %.0f x %.0f km\n", argv[1] ? argv[1] : "state",
		M.Cities.size(), M.Highways.size(), Peak, (B.MaxX - B.MinX) / 1000.0, (B.MaxY - B.MinY) / 1000.0);
	return 0;
}
