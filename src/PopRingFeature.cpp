#include "PopRingFeature.h"
#include <SoyApp.h>
#include <TJob.h>
#include <math.h>
#include <SortArray.h>

namespace Soy
{
	float RadToDeg(float Radians)
	{
		return Radians * (180.f/M_PI);
	}
	float DegToRad(float Degrees)
	{
		return Degrees * (M_PI / 180.f);
	}
}


std::ostream& operator<< (std::ostream &out,const TPopRingFeature &in)
{
	auto& Feature = in;
	for ( int i=0;	i<Feature.mBrighters.GetSize();	i++ )
	{
		bool Bit = Feature.mBrighters[i];
		out << (Bit ? "1" : "0");
	}
	return out;
}


template<> template<>
bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<TPopRingFeature>& Data) const
{
	auto& String = this->mValue;
	auto& Feature = Data.mValue;
	
	//	read char by char
	Feature.mBrighters.Clear();
	for ( int i=0;	i<String.length();	i++ )
	{
		bool Bit = (String[i] == '1');
		Feature.mBrighters.PushBack( Bit );
	}
	
	return true;
}

template <> template<>
bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TPopRingFeature>& Data)
{
	auto& String = this->mValue;
	auto& Feature = Data.mValue;
	
	std::stringstream BitsString;
	BitsString << Feature;
	String = BitsString.str();
	//	String = (std::stringstream() << Feature.mBrighters).str();
	
	return true;
}

template <> template<>
bool SoyData_Impl<json::Object>::Encode(const SoyData_Impl<TFeatureMatch>& Data)
{
	auto& Json = this->mValue;
	auto& Match = Data.mValue;

	Json.Insert( MakeJsonMember("x", Match.mCoord.x ) );
	Json.Insert( MakeJsonMember("y", Match.mCoord.y ) );
	Json.Insert( MakeJsonMember("Score", Match.mScore ) );
	
	SoyData_Stack<std::string> FeatureString;
	FeatureString.Encode( SoyData_Impl<TPopRingFeature>(Match.mFeature) );
	Json.Insert( MakeJsonMember("Feature", FeatureString.mValue ) );

	this->OnEncode( Data );
	return true;
}
	
template <> template<>
bool SoyData_Impl<json::Object>::Encode(const SoyData_Impl<Array<TFeatureMatch>>& Data)
{
	auto& Json = this->mValue;
	auto& Matchs = Data.mValue;
	
	auto MatchesJsonIt = Json.Insert( json::Object::Member("Matches") );
	json::Array& MatchesJson = MatchesJsonIt->element;

	for ( int i=0;	i<Matchs.GetSize();	i++ )
	{
		auto& Match = Matchs[i];
		
		//	make json object
		SoyData_Stack<json::Object> MatchJson;
		bool Encoded = MatchJson.Encode( SoyData_Impl<TFeatureMatch>(Match) );
		if ( !Soy::Assert( Encoded, "Failed to encode TFeatureMatch to json" ) )
			return false;
		
		MatchesJson.Insert( MatchJson.mValue );
	}
			
	OnEncode( Data.GetFormat() );
	return true;
}
				

TPopRingFeatureParams::TPopRingFeatureParams() :
	mMinScore			( 0.7f ),
	mMatchStepX			( 10 ),
	mMatchStepY			( 10 ),
	mRadius				( 4.f ),
	mSampleCount		( 32 ),
	mBrighterTolerance	( 0.10f ),
	mMaxMatches			( 10 ),
	mMinInterestingScore	( 0.2f )
{
	
}

TPopRingFeatureParams::TPopRingFeatureParams(const TJobParams& Params) :
	TPopRingFeatureParams	()
{
	Params.GetParamAs("MinScore", mMinScore );
	Params.GetParamAs("MatchStepX", mMatchStepX );
	Params.GetParamAs("MatchStepY", mMatchStepY );
	Params.GetParamAs("Radius", mRadius );
	Params.GetParamAs("SampleCount", mSampleCount );
	Params.GetParamAs("BrighterTolerance", mBrighterTolerance );
	Params.GetParamAs("MaxMatches", mMaxMatches );
	
	mRadius = std::max( 1.f, mRadius );
	mSampleCount = std::max( 1, mSampleCount );
	mSampleCount = std::min( 32, mSampleCount );
	mMatchStepX = std::max( 1, mMatchStepX );
	mMatchStepY = std::max( 1, mMatchStepY );
}

void CalculateOffsets(Array<vec2x<int>>& Offsets,int SampleCount,float Radius)
{
	for ( int i=0;	i<SampleCount;	i++ )
	{
		float t = i / static_cast<float>( SampleCount );
		float rad = Soy::DegToRad( 360.f * t );
		float x = cosf( rad ) * Radius;
		float y = sinf( rad ) * Radius;
		auto& Offset = Offsets.PushBack();
		Offset.x = x;
		Offset.y = y;
	}
}

const Array<vec2x<int>>& TPopRingFeatureParams::GetSampleOffsets()
{
	//	cached
	if ( !mSampleOffsets.IsEmpty() )
		return mSampleOffsets;
	
	
	//	gr: make 2 rings, inner and outer
	int InnerSamples = (mSampleCount / 3) * 1;
	float InnerRadius = mRadius / 2.f;
	CalculateOffsets( mSampleOffsets, InnerSamples, InnerRadius );

	int OuterSamples = (mSampleCount / 3) * 2;
	float OuterRadius = mRadius / 1.f;
	CalculateOffsets( mSampleOffsets, OuterSamples, OuterRadius );
	
	
	return mSampleOffsets;
}

float TPopRingFeature::GetMatchScore(const TPopRingFeature& Match,const TPopRingFeatureParams& Params) const
{
	auto& That = Match;

	//	mis-matched features
	if ( this->mBrighters.GetSize() != That.mBrighters.GetSize() )
		return 0.f;
	if ( this->mBrighters.IsEmpty() )
		return 0.f;
	
	//	how many bits match?
	int MatchCount = 0;
	for ( int b=0;	b<mBrighters.GetSize();	b++ )
	{
		bool ThisBit = this->mBrighters[b];
		bool ThatBit = That.mBrighters[b];
	
		if ( ThisBit == ThatBit )
			MatchCount++;
	}
	
	float Score = MatchCount / static_cast<float>( mBrighters.GetSize() );
	return Score;
}


class TSampleWrapper
{
public:
	TSampleWrapper(const SoyPixelsImpl& Pixels,const TPopRingFeatureParams& Params,int BaseX,int BaseY) :
		mPixels	( Pixels ),
		mParams	( Params ),
		mBaseX	( BaseX ),
		mBaseY	( BaseY ),
		mMinX	( 0 ),
		mMinY	( 0 ),
		mMaxX	( Pixels.GetWidth()-1 ),
		mMaxY	( Pixels.GetHeight()-1 ),
		mChannels	( std::max<int>(3,Pixels.GetChannels()) )
	{
		
	}
	
	float					GetLuminosity(int r8,int g8,int b8)
	{
		float r = r8 / 255.f;
		float g = g8 / 255.f;
		float b = b8 / 255.f;
		
		//	http://stackoverflow.com/a/24213274/355753
		//	3rd picture - HSP Color Model
		float rWeight = 0.299f;
		float gWeight = 0.587f;
		float bWeight = 0.114f;
		
		float y = (r*rWeight) + (g*gWeight) + (b*bWeight);
		return y;
	}
	
	float					GetIntensity(int xoff,int yoff)
	{
		int x = mBaseX + xoff;
		int y = mBaseY + yoff;
		Clamp(x,y);
		
		int r = mPixels.GetPixel( x, y, 0 );
		int g = mPixels.GetPixel( x, y, 1 );
		int b = mPixels.GetPixel( x, y, 2 );
		
		return GetLuminosity( r, g, b );
	}
	
	void					Clamp(int& x,int& y)
	{
		x = std::min( x, mMaxX );
		y = std::min( y, mMaxY );
		x = std::max( x, mMinX );
		y = std::max( y, mMinY );
	}
	
public:
	const SoyPixelsImpl&	mPixels;
	const TPopRingFeatureParams&	mParams;
	int						mChannels;
	int						mMinX;
	int						mMinY;
	int						mMaxX;
	int						mMaxY;
	int						mBaseX;
	int						mBaseY;
};



bool TFeatureExtractor::GetFeature(TPopRingFeature& Feature,const SoyPixelsImpl& Pixels,int x,int y,TPopRingFeatureParams& Params,std::stringstream& Error)
{
	TSampleWrapper Sampler( Pixels, Params, x, y );

	//	get intensity of root pixel
	float BaseIntensity = Sampler.GetIntensity(0,0) - Params.mBrighterTolerance;
	
	auto& SampleOffsets = Params.GetSampleOffsets();
	char DebugBits[1000] = {0};
	for ( int c=0;	c<SampleOffsets.GetSize();	c++ )
	{
		auto& Offset = SampleOffsets[c];
		float Intensity = Sampler.GetIntensity( Offset.x, Offset.y );
		bool Bit = ( Intensity >= BaseIntensity );

		Feature.mBrighters.PushBack( Bit );
		DebugBits[c] = Bit;
	}
		
	return true;
}

bool TFeatureExtractor::FindFeatureMatches(ArrayBridge<TFeatureMatch>&& Matches,const SoyPixelsImpl& Pixels,const TPopRingFeature& Feature,TPopRingFeatureParams& Params,std::stringstream& Error)
{
	auto SortedMatches = GetSortArray( Matches, TSortPolicy_Descending<TFeatureMatch>() );

	//	step through the image
	for ( int x=0;	x<Pixels.GetWidth();	x+=Params.mMatchStepX )
	{
		for ( int y=0;	y<Pixels.GetHeight();	y+=Params.mMatchStepY )
		{
			//	get feature here
			TPopRingFeature TestFeature;
			if ( !GetFeature( TestFeature, Pixels, x, y, Params, Error ) )
				return false;
				
			//	get score
			float Score = Feature.GetMatchScore( TestFeature, Params );
			if ( Score < Params.mMinScore )
				continue;
			
			TFeatureMatch Match;
			Match.mCoord.x = x;
			Match.mCoord.y = y;
			Match.mScore = Score;
			Match.mFeature = TestFeature;
			SortedMatches.Push(Match);
			
			//	gr: cap during push to reduce allocations
			if ( Matches.GetSize() > Params.mMaxMatches )
				Matches.SetSize( Params.mMaxMatches );
		}
	}
	
	//	cap number of outputs
	if ( Matches.GetSize() > Params.mMaxMatches )
		Matches.SetSize( Params.mMaxMatches );
	
	return true;
}

