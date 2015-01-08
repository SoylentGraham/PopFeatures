#include "PopRingFeature.h"
#include <SoyApp.h>



TPopRingFeatureParams::TPopRingFeatureParams() :
	mContrastTolerance	( 0.5f )
{
	
}

TPopRingFeatureParams::TPopRingFeatureParams(const TJobParams& params) :
	TPopRingFeatureParams	()
{
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

template<typename TYPE>
class vec2x
{
public:
	vec2x(TYPE _x,TYPE _y) :
	x	( _x ),
	y	( _y )
	{
	}
	vec2x() :
	x	( 0 ),
	y	( 0 )
	{
	}
	
	TYPE	LengthSq() const	{	return (x*x)+(y*y);	}
	TYPE	Length() const		{	return sqrtf( LengthSq() );	}
	
	vec2x&	operator*=(const TYPE& Scalar)	{	x*=Scalar;		y*=Scalar;	return *this;	}
	vec2x&	operator*=(const vec2x& Scalar)	{	x*=Scalar.x;	y*=Scalar.y;	return *this;	}
	
public:
	TYPE	x;
	TYPE	y;
};


void AddBoxRing(ArrayBridge<vec2x<int>>& Coords,int x,int y)
{
	Coords.PushBack( vec2x<int>( x, y ) );
}

void GetBoxRing(ArrayBridge<vec2x<int>>&& Coords,int Radius,int Step)
{
	//	top row
	{
		int y = -Radius;
		for ( int x=-Radius;	x<=Radius;	x+=Step )
			AddBoxRing( Coords, x, y );
	}
	
	//	right col (ignore top and bottom rows)
	{
		int x = Radius;
		for ( int y=-Radius+1;	y<=Radius-1;	y+=Step )
			AddBoxRing( Coords, x, y );
	}

	//	bottom row
	{
		int y = Radius;
		for ( int x=-Radius;	x<=Radius;	x+=Step )
			AddBoxRing( Coords, x, y );
	}
	
	//	left col (ignore top and bottom rows)
	{
		int x = -Radius;
		for ( int y=-Radius+1;	y<=Radius-1;	y+=Step )
			AddBoxRing( Coords, x, y );
	}
}


bool TPopRingFeature::GetFeature(TPopRingFeature& Feature,const SoyPixelsImpl& Pixels,int x,int y,const TPopRingFeatureParams& Params,std::stringstream& Error)
{
	TSampleWrapper Sampler( Pixels, Params, x, y );

	//	get intensity of root pixel
	float BaseIntensity = Sampler.GetIntensity(0,0);
	
	Array<char> Data;
	TBitWriter BitWriter( GetArrayBridge(Data) );
	
	//	first box ring
	int Radius = 5;
	int Step = 2;
	Array<vec2x<int>> SampleOffsets;
	GetBoxRing( GetArrayBridge(SampleOffsets), Radius, Step );
	for ( int c=0;	c<SampleOffsets.GetSize();	c++ )
	{
		auto& Offset = SampleOffsets[c];
		float Intensity = Sampler.GetIntensity( Offset.x, Offset.y );
		if ( Intensity < BaseIntensity )
			BitWriter.WriteBit(0);
		else
			BitWriter.WriteBit(1);
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

