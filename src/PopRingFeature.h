#pragma once

#include <ofxSoylent.h>
#include <SoyData.h>
#include <SoyApp.h>


class TJobParams;
class SoyPixelsImpl;



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


class TPopRingFeatureParams
{
public:
	TPopRingFeatureParams();
	TPopRingFeatureParams(const	TJobParams& params);
	
	int		mMatchStepX;
	int		mMatchStepY;
	float	mMinScore;
};


class TPopRingFeature
{
public:
	TPopRingFeature() :
		mBrighters	( 0 )
	{
	}
	
	float		GetMatchScore(const TPopRingFeature& Match,const TPopRingFeatureParams& Params) const;
	
public:
	uint32		mBrighters;	//	1 brighter, 0 darker
};


class TFeatureMatch
{
public:
	TFeatureMatch() :
		mScore	( -1.f )
	{
	}
	
	inline bool		operator==(const TFeatureMatch& That) const	{	return this->mScore == That.mScore;	}
	inline bool		operator<(const TFeatureMatch& That) const	{	return this->mScore < That.mScore;	}
	inline bool		operator>(const TFeatureMatch& That) const	{	return this->mScore > That.mScore;	}
	
public:
	vec2x<int>		mCoord;
	float			mScore;
	TPopRingFeature	mFeature;
};


template <> template<>
bool SoyData_Impl<json::Object>::Encode(const SoyData_Impl<Array<TFeatureMatch>>& Data);
template <> template<>
bool SoyData_Impl<json::Object>::Encode(const SoyData_Impl<TFeatureMatch>& Data);




class TFeatureExtractor
{
public:
	
	static bool		GetFeature(TPopRingFeature& Feature,const SoyPixelsImpl& Pixels,int x,int y,const TPopRingFeatureParams& Params,std::stringstream& Error);
	static bool		FindFeatureMatches(ArrayBridge<TFeatureMatch>&& Matches,const SoyPixelsImpl& Pixels,const TPopRingFeature& Feature,const TPopRingFeatureParams& Params,std::stringstream& Error);

};

template<> template<>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<TPopRingFeature>& Data) const
{
	auto& String = this->mValue;
	auto& Feature = Data.mValue;

	//	read char by char
	Array<char> Bits;
	TBitWriter BitWriter( GetArrayBridge(Bits) );
	for ( int i=0;	i<String.length();	i++ )
	{
		bool Bit = (String[i] == '1');
		BitWriter.WriteBit( Bit );
	}
	
	TBitReader BitReader( GetArrayBridge(Bits) );
	if ( !BitReader.Read( reinterpret_cast<int&>(Feature.mBrighters), BitWriter.BitPosition() ) )
	{
		std::Debug << "Error reading back bits from string" << std::endl;
		return false;
	}
	return true;
}
	
template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TPopRingFeature>& Data)
{
	auto& String = this->mValue;
	auto& Feature = Data.mValue;

//	std::bitset<32> Bits( std::string("10101010101010101010101010101010" ));
//	auto BitsLong = Bits.to_ulong();

	std::stringstream BitsString;
	for ( int i=0;	i<sizeof(Feature.mBrighters)*8;	i++ )
	{
		bool Bit = (Feature.mBrighters & (1<<i)) != 0;
		BitsString << (Bit ? "1" : "0");
	}
	String = BitsString.str();
	//	String = (std::stringstream() << Feature.mBrighters).str();

	return true;
}
