#include "PopRingFeature.h"
#include <SoyApp.h>
#include <TJob.h>
#include <math.h>


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
	mMinScore		( 0.7f ),
	mMatchStepX		( 10 ),
	mMatchStepY		( 10 ),
	mRadius			( 4.f ),
	mSampleCount	( 32 )
{
	
}

TPopRingFeatureParams::TPopRingFeatureParams(const TJobParams& Params) :
	TPopRingFeatureParams	()
{
	Params.GetParamAs("MinScore", mMinScore );
	Params.GetParamAs("MatchStepX", mMatchStepX );
	Params.GetParamAs("MatchStepY", mMatchStepY );
	Params.GetParamAs("Radius", mRadius );
	Params.GetParamAs("Samples", mSampleCount );
	
	mRadius = std::max( 1.f, mRadius );
	mSampleCount = std::max( 1, mSampleCount );
	mSampleCount = std::min( 32, mSampleCount );
	mMatchStepX = std::max( 1, mMatchStepX );
	mMatchStepY = std::max( 1, mMatchStepY );
}

const Array<vec2x<int>>& TPopRingFeatureParams::GetSampleOffsets()
{
	mSampleOffsets.Clear();
	
	for ( int i=0;	i<mSampleCount;	i++ )
	{
		float t = i / static_cast<float>( mSampleCount );
		float rad = Soy::DegToRad( 360.f * t );
		float x = cosf( rad ) * mRadius;
		float y = sinf( rad ) * mRadius;
		auto& Offset = mSampleOffsets.PushBack();
		Offset.x = x;
		Offset.y = y;
	}
	return mSampleOffsets;
}

float TPopRingFeature::GetMatchScore(const TPopRingFeature& Match,const TPopRingFeatureParams& Params) const
{
	auto& That = Match;
	//	how many bits match?
	int BitCount = sizeof(mBrighters) * 8;
	
	int MatchCount = 0;
	for ( int b=0;	b<BitCount;	b++ )
	{
		bool ThisBit = (this->mBrighters & (1<<b)) != 0;
		bool ThatBit = (That.mBrighters & (1<<b)) != 0;
	
		if ( ThisBit == ThatBit )
			MatchCount++;
	}
	
	float Score = MatchCount / static_cast<float>( BitCount );
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
	
	float					GetIntensity(int xoff,int yoff)
	{
		int x = mBaseX + xoff;
		int y = mBaseY + yoff;
		Clamp(x,y);
		int PixelTotal = 0;
		auto* Pixel = &mPixels.GetPixel( x, y, 0 );
		for ( int c=0;	c<mChannels;	c++ )
		{
			auto Component = Pixel[c];
			PixelTotal += Component;
		}
		//	get as 0...1
		float Intensity = PixelTotal / (static_cast<float>( mChannels ) * 255.f);

		return Intensity;
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
	float BaseIntensity = Sampler.GetIntensity(0,0);
	
	Array<char> Data;
	TBitWriter BitWriter( GetArrayBridge(Data) );
	
	auto& SampleOffsets = Params.GetSampleOffsets();
	char DebugBits[1000] = {0};
	for ( int c=0;	c<SampleOffsets.GetSize();	c++ )
	{
		auto& Offset = SampleOffsets[c];
		float Intensity = Sampler.GetIntensity( Offset.x, Offset.y );
		bool Bit = ( Intensity >= BaseIntensity );

		BitWriter.WriteBit(Bit);
		DebugBits[c] = Bit;
	}
	
	//	read back
	if ( Data.GetSize() > sizeof(Feature.mBrighters) )
	{
		Error << "Wrote too many bits making feature";
		return false;
	}
	
	TBitReader BitReader( GetArrayBridge(Data) );
	if ( !BitReader.Read( reinterpret_cast<int&>(Feature.mBrighters), BitWriter.BitPosition() ) )
	{
		Error << "Error reading back bits";
		return false;
	}
	
	return true;
}

bool TFeatureExtractor::FindFeatureMatches(ArrayBridge<TFeatureMatch>&& Matches,const SoyPixelsImpl& Pixels,const TPopRingFeature& Feature,TPopRingFeatureParams& Params,std::stringstream& Error)
{
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
			
			auto& Match = Matches.PushBack();
			Match.mCoord.x = x;
			Match.mCoord.y = y;
			Match.mScore = Score;
			Match.mFeature = TestFeature;
		
		}
	}
	
	return true;
}

