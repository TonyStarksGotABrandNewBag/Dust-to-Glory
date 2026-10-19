// Renders what the player would see: a ground-level camera on a highway, terrain plus vegetation,
// depth-buffered with a simple sun. Not a beauty contest -- it is how I check that the planting
// reads as the Southeast and that no tree is standing in the road or floating.
#include "Sim/GameState.h"
#include "Sim/RoadMarkings.h"
#include "Sim/Sundown.h"
#include "Sim/Vegetation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace dtg::sim;

bool& gNoDepthRef();
struct Img {
	int W, H; std::vector<std::uint8_t> Px; std::vector<double> Zb;
	Img(int w,int h):W(w),H(h),Px(static_cast<size_t>(w*h*3)),Zb(static_cast<size_t>(w*h),1e18){}
	void Sky(double Sun,double Warm,double Night){ for(int y=0;y<H;++y){ const double T=static_cast<double>(y)/H;
		// a hazy Carolina sky, brighter at the horizon
		double R=0.14+0.30*T, G=0.26+0.34*T, B=0.55+0.30*T;
		R = R*(0.05+0.95*Sun) + Warm*0.42*(1.0-T)*(1.0-T);
		G = G*(0.05+0.95*Sun) + Warm*0.13*(1.0-T)*(1.0-T);
		B = B*(0.06+0.94*Sun) + Night*0.012;
		auto Srgb=[](double V){ V=std::clamp(V,0.0,1.0);
			return static_cast<std::uint8_t>(255.0*(V<=0.0031308?12.92*V:1.055*std::pow(V,1.0/2.4)-0.055)); };
		for(int x=0;x<W;++x){ const size_t I=static_cast<size_t>((y*W+x)*3);
			Px[I]=Srgb(R); Px[I+1]=Srgb(G); Px[I+2]=Srgb(B);} } }
	void Put(int x,int y,double z,double r,double g,double b){
		if(x<0||y<0||x>=W||y>=H) return; const size_t D=static_cast<size_t>(y*W+x);
		if(!gNoDepthRef() && z>=Zb[D]) return;
		if(!gNoDepthRef()) Zb[D]=z;
		const size_t I=D*3;
		auto Srgb=[](double V){ V=std::clamp(V,0.0,1.0);
			return static_cast<std::uint8_t>(255.0*(V<=0.0031308?12.92*V:1.055*std::pow(V,1.0/2.4)-0.055)); };
		Px[I]=Srgb(r); Px[I+1]=Srgb(g); Px[I+2]=Srgb(b);}
	void Save(const char* P){ std::ofstream O(P,std::ios::binary); O<<"P6\n"<<W<<" "<<H<<"\n255\n"; O.write(reinterpret_cast<const char*>(Px.data()),static_cast<std::streamsize>(Px.size())); }
};

struct Cam { double X,Y,Z,Yaw,Pitch,Fov; };
static bool gNoDepthFlag=false;
bool& gNoDepthRef(){ return gNoDepthFlag; }

// World -> screen. Returns false behind the camera.
static bool Proj(const Cam& C,const double P[3],int W,int H,double& Sx,double& Sy,double& Depth){
	const double Dx=P[0]-C.X, Dy=P[1]-C.Y, Dz=P[2]-C.Z;
	const double Cy=std::cos(-C.Yaw), Sy2=std::sin(-C.Yaw);
	const double Fx=Dx*Cy-Dy*Sy2, Fy=Dx*Sy2+Dy*Cy;
	const double Cp=std::cos(-C.Pitch), Sp=std::sin(-C.Pitch);
	const double Forward=Fx*Cp-Dz*Sp, Up=Fx*Sp+Dz*Cp;
	if(Forward<0.39) return false;
	const double F=(0.5*W)/std::tan(0.5*C.Fov);
	Sx=0.5*W+F*(-Fy)/Forward; Sy=0.5*H-F*Up/Forward; Depth=Forward; return true;
}

// How far in front of the camera a point is.
static double Fwd(const Cam& C,const double P[3]){
	const double Dx=P[0]-C.X,Dy=P[1]-C.Y,Dz=P[2]-C.Z;
	const double Cy=std::cos(-C.Yaw),Sy2=std::sin(-C.Yaw);
	const double Fx=Dx*Cy-Dy*Sy2;
	return Fx*std::cos(-C.Pitch)-Dz*std::sin(-C.Pitch);
}
static double gDepthBias=0.0; static double gSun=1.0;
static void RawTri(Img& Im,const Cam& C,const double A[3],const double B[3],const double D[3],double r,double g,double b);

// Clips a triangle against the near plane and draws the pieces. Without this, a stripe running from
// behind the camera to far ahead -- which is what a long solid line is -- disappears completely.
static void Tri(Img& Im,const Cam& C,const double A[3],const double B[3],const double D[3],double r,double g,double b){
	const double* V[3]={A,B,D};
	double F[3]={Fwd(C,A),Fwd(C,B),Fwd(C,D)};
	const double Near=0.40;
	int In=0; for(int k=0;k<3;++k) if(F[k]>Near) ++In;
	if(In==0) return;
	if(In==3){ RawTri(Im,C,A,B,D,r,g,b); return; }
	// Build the clipped polygon (3 or 4 corners) and fan it.
	double Poly[4][3]; int N=0;
	for(int k=0;k<3;++k){
		const int j=(k+1)%3;
		const bool bK=F[k]>Near, bJ=F[j]>Near;
		if(bK){ for(int c=0;c<3;++c) Poly[N][c]=V[k][c]; ++N; }
		if(bK!=bJ && N<4){
			const double T=(Near-F[k])/(F[j]-F[k]);
			for(int c=0;c<3;++c) Poly[N][c]=V[k][c]+(V[j][c]-V[k][c])*T;
			++N;
		}
	}
	for(int k=2;k<N;++k) RawTri(Im,C,Poly[0],Poly[k-1],Poly[k],r,g,b);
}

static void RawTri(Img& Im,const Cam& C,const double A[3],const double B[3],const double D[3],double r,double g,double b){
	double ax,ay,az,bx,by,bz,cx,cy,cz;
	if(!Proj(C,A,Im.W,Im.H,ax,ay,az)||!Proj(C,B,Im.W,Im.H,bx,by,bz)||!Proj(C,D,Im.W,Im.H,cx,cy,cz)) return;
	const int MinX=std::max(0,static_cast<int>(std::floor(std::min({ax,bx,cx}))));
	const int MaxX=std::min(Im.W-1,static_cast<int>(std::ceil(std::max({ax,bx,cx}))));
	const int MinY=std::max(0,static_cast<int>(std::floor(std::min({ay,by,cy}))));
	const int MaxY=std::min(Im.H-1,static_cast<int>(std::ceil(std::max({ay,by,cy}))));
	const double Area=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax);
	if(std::fabs(Area)<1e-9) return;
	for(int y=MinY;y<=MaxY;++y)for(int x=MinX;x<=MaxX;++x){
		const double Px=x+0.5, Py=y+0.5;
		const double W0=((bx-ax)*(Py-ay)-(by-ay)*(Px-ax))/Area;
		const double W1=((Px-ax)*(cy-ay)-(Py-ay)*(cx-ax))/Area;
		if(W0<0||W1<0||W0+W1>1) continue;
		const double Z=az+(bz-az)*W1+(cz-az)*W0-gDepthBias;
		// aerial haze, which is most of what sells distance
		const double Haze=std::clamp(Z/900.0,0.0,0.82);
		const double HS=0.035+0.965*gSun;
		Im.Put(x,y,Z,r*(1-Haze)+0.33*Haze*HS,g*(1-Haze)+0.44*Haze*HS,b*(1-Haze)+0.68*Haze*HS);}
}

int main(int argc,char** argv){
	auto Read=[](const char* N){ std::ifstream I(std::string((std::getenv("DTG_DATA") ? std::getenv("DTG_DATA") : "Content/Data/"))+N); std::ostringstream O; O<<I.rdbuf(); return O.str(); };
	ContentFiles F; F.Economy=Read("economy.json"); F.Vehicles=Read("vehicles.json"); F.Parts=Read("parts.json");
	F.Careers=Read("careers.json"); F.Powers=Read("powers.json"); F.Furniture=Read("furniture.json");
	F.Companies=Read("companies.json"); F.Laws=Read("laws.json");
	const char* StateFile=argc>1?argv[1]:"state.json"; F.State=Read(StateFile);
	GameState S; ContentReport R; S.LoadContent(F,R);
	const int Which=argc>2?std::atoi(argv[2]):0;
	const int MinuteOfDay = argc>3?std::atoi(argv[3]):720;
	S.EnterCheat("SUNDOWN");
	const double Sun = S.Sundown.SunAt(MinuteOfDay);
	const double Night = 1.0 - Sun;
	// Dusk reddens as it goes, then drains to almost nothing. Nothing here is tinted by hand: the
	// one number driving all of it is the same SunAt the gameplay rules read.
	const double Warm = std::clamp(1.0 - std::fabs(Sun - 0.35) / 0.35, 0.0, 1.0);
	std::printf("  minute %d  sun %.3f  light left %.0f min  exposure(open) %.2f  exposure(dark room) %.2f\n",
		MinuteOfDay, Sun, S.Sundown.MinutesOfLightLeft(MinuteOfDay),
		S.Sundown.Exposure(MinuteOfDay,false,false,0.0), S.Sundown.Exposure(MinuteOfDay,true,false,0.0));

	// Stand on a highway, looking along it.
	const HighwayPlan& Road=S.Map.Highways[static_cast<size_t>(Which)%S.Map.Highways.size()];
	const double Length=Road.LengthM();
	const double Along=Length*0.45;
	const std::array<double,3> Here=Road.At(Along);
	const std::array<double,3> Ahead=Road.At(std::min(Length,Along+30.0));
	// Sit where a driver sits: in a lane, not on the centreline or the median.
	const double Dirx=Ahead[0]-Here[0], Diry=Ahead[1]-Here[1];
	const double DirL=std::max(1e-9,std::hypot(Dirx,Diry));
	const double Rx=Diry/DirL, Ry=-Dirx/DirL;           // to the driver's right
	const double LaneOff=Road.bInterstate?0.30*Road.WidthM:0.25*Road.WidthM;
	Cam C; C.X=Here[0]+Rx*LaneOff; C.Y=Here[1]+Ry*LaneOff;
	C.Z=Here[2]+1.25; C.Yaw=std::atan2(Diry,Dirx); C.Pitch=-0.055; C.Fov=1.05;
	gSun=Sun;
	Img Im(1100,620); Im.Sky(Sun,Warm,Night);

	// Terrain, finer close in. Drawn as a radial fan around the camera so near ground gets detail.
	const double SunDir[3]={0.42,0.33,0.84};
	for(int Ring=0;Ring<150;++Ring){
		const double R0=1.0+std::pow(Ring/150.0,2.1)*820.0;
		const double R1=1.0+std::pow((Ring+1)/150.0,2.1)*820.0;
		const int Spokes=96;
		for(int Spoke=0;Spoke<Spokes;++Spoke){
			const double A0=C.Yaw-C.Fov+2.0*C.Fov*Spoke/Spokes;
			const double A1=C.Yaw-C.Fov+2.0*C.Fov*(Spoke+1)/Spokes;
			double Q[4][3];
			const double Rs[4]={R0,R1,R1,R0}; const double As[4]={A0,A0,A1,A1};
			for(int K=0;K<4;++K){ Q[K][0]=C.X+std::cos(As[K])*Rs[K]; Q[K][1]=C.Y+std::sin(As[K])*Rs[K];
				Q[K][2]=S.Map.SurfaceAt(Q[K][0],Q[K][1]).GroundM; }
			const SurfaceInfo Mid=S.Map.SurfaceAt((Q[0][0]+Q[2][0])*0.5,(Q[0][1]+Q[2][1])*0.5);
			double r,g,b;
			if(Mid.bWater){ r=0.10;g=0.20;b=0.26; for(int K=0;K<4;++K) Q[K][2]=Mid.WaterM; }
			// bRoad is exactly the carriageway plus its paved shoulder; RoadNearness fades on out
			// across the graded verge, which is grass, not tarmac.
			else if(Mid.bRoad){ r=0.055;g=0.053;b=0.058; }
			else if(Mid.RoadNearness>0.0){ const double V=0.5+0.5*S.Map.Noise(Q[0][0],Q[0][1],18.0);
				r=0.14+0.05*V; g=0.19+0.07*V; b=0.09+0.03*V; }   // mown verge
			else if(Mid.GroundM<1.2&&S.Map.CoastDistance((Q[0][0]+Q[2][0])*0.5,(Q[0][1]+Q[2][1])*0.5)<90.0){ r=0.62;g=0.56;b=0.42; }
			else { const double V=0.5+0.5*S.Map.Noise(Q[0][0],Q[0][1],28.0);
				r=0.13+0.05*V; g=0.22+0.09*V; b=0.10+0.03*V; }
			// flat shading from the quad's own slope
			const double Ux=Q[1][0]-Q[0][0],Uy=Q[1][1]-Q[0][1],Uz=Q[1][2]-Q[0][2];
			const double Vx=Q[3][0]-Q[0][0],Vy=Q[3][1]-Q[0][1],Vz=Q[3][2]-Q[0][2];
			double Nx=Uy*Vz-Uz*Vy, Ny=Uz*Vx-Ux*Vz, Nz=Ux*Vy-Uy*Vx;
			const double NL=std::max(1e-9,std::sqrt(Nx*Nx+Ny*Ny+Nz*Nz)); Nx/=NL;Ny/=NL;Nz/=NL;
			if(Nz<0){Nx=-Nx;Ny=-Ny;Nz=-Nz;}
			const double Lit=0.45+0.55*std::max(0.0,Nx*SunDir[0]+Ny*SunDir[1]+Nz*SunDir[2]);
			const double D=(0.035+0.965*Sun)*Lit;
			Tri(Im,C,Q[0],Q[1],Q[2],r*D,g*D,b*D); Tri(Im,C,Q[0],Q[2],Q[3],r*D,g*D,b*D);
		}
	}

	// Road paint, on every highway near the camera.
	size_t Painted=0;
	gDepthBias=0.0;
	struct Stripe { double Q[4][3]; double Rgb[3]; double Dist; };
	std::vector<Stripe> Stripes;
	for(const HighwayPlan& H:S.Map.Highways){
		// skip roads entirely out of view
		bool bNear=false;
		for(const auto& P:H.Points) if(std::hypot(P[0]-C.X,P[1]-C.Y)<900.0){bNear=true;break;}
		if(!bNear) continue;
		for(const RoadMark& M:MarkHighway(H)){
			const double Dx=M.Bx-M.Ax, Dy=M.By-M.Ay;
			const double L=std::max(1e-9,std::hypot(Dx,Dy));
			const double Nx=-Dy/L*M.WidthM*0.5, Ny=Dx/L*M.WidthM*0.5;
			Stripe St;
			const double Qs[4][3]={{M.Ax-Nx,M.Ay-Ny,M.Az},{M.Ax+Nx,M.Ay+Ny,M.Az},
				{M.Bx+Nx,M.By+Ny,M.Bz},{M.Bx-Nx,M.By-Ny,M.Bz}};
			for(int k=0;k<4;++k) for(int c=0;c<3;++c) St.Q[k][c]=Qs[k][c];
			for(int c=0;c<3;++c) St.Rgb[c]=M.Rgb[c];
			St.Dist=std::hypot(0.5*(M.Ax+M.Bx)-C.X,0.5*(M.Ay+M.By)-C.Y);
			Stripes.push_back(St);
			++Painted;
		}
	}

	std::sort(Stripes.begin(),Stripes.end(),[](const Stripe& A,const Stripe& B){return A.Dist>B.Dist;});
	gNoDepthRef()=true;
	for(const Stripe& St:Stripes){
		const double D=0.10+0.90*Sun;
		Tri(Im,C,St.Q[0],St.Q[1],St.Q[2],St.Rgb[0]*D,St.Rgb[1]*D,St.Rgb[2]*D);
		Tri(Im,C,St.Q[0],St.Q[2],St.Q[3],St.Rgb[0]*D,St.Rgb[1]*D,St.Rgb[2]*D);
	}
	gNoDepthRef()=false;

	// Vegetation out to the horizon of interest.
	Rect Area{C.X-620,C.Y-620,C.X+620,C.Y+620};
	PlantingRules Rules; Rules.Limit=20000;
	const std::vector<Plant> Grown=PlantsIn(S.Map,S.City,Area,Rules);
	std::vector<PlantTri> Tris;
	size_t Drawn=0;
	for(const Plant& P:Grown){
		const double Dist=std::hypot(P.X-C.X,P.Y-C.Y);
		if(Dist>620.0) continue;
		Tris.clear(); BuildPlant(P,Tris); ++Drawn;
		for(const PlantTri& T:Tris){
			// crude per-triangle shading from its own normal
			const double Ux=T.P[1][0]-T.P[0][0],Uy=T.P[1][1]-T.P[0][1],Uz=T.P[1][2]-T.P[0][2];
			const double Vx=T.P[2][0]-T.P[0][0],Vy=T.P[2][1]-T.P[0][1],Vz=T.P[2][2]-T.P[0][2];
			double Nx=Uy*Vz-Uz*Vy,Ny=Uz*Vx-Ux*Vz,Nz=Ux*Vy-Uy*Vx;
			const double NL=std::max(1e-9,std::sqrt(Nx*Nx+Ny*Ny+Nz*Nz)); Nx/=NL;Ny/=NL;Nz/=NL;
			const double Lit=0.52+0.48*std::fabs(Nx*SunDir[0]+Ny*SunDir[1]+Nz*SunDir[2]);
			const double D=(0.03+0.97*Sun)*Lit;
			Tri(Im,C,T.P[0],T.P[1],T.P[2],T.Rgb[0]*D,T.Rgb[1]*D,T.Rgb[2]*D);
		}
	}
	size_t Counts[6]={};
	for(const Plant& P:Grown) if(static_cast<int>(P.Kind)<6) ++Counts[static_cast<size_t>(P.Kind)];
	std::printf("%s road '%s': %zu stripes painted, %zu plants in range, %zu drawn  [pine %zu oak %zu palm %zu palmetto %zu cypress %zu scrub %zu]\n",
		StateFile, Road.Name.c_str(), Painted, Grown.size(), Drawn, Counts[0],Counts[1],Counts[2],Counts[3],Counts[4],Counts[5]);
	std::string Out=std::string("dusk-")+std::to_string(MinuteOfDay)+".ppm";
	Im.Save(Out.c_str()); std::printf("  -> %s\n", Out.c_str());
	return 0;
}
