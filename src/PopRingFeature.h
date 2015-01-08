#pragma once

#include <ofxSoylent.h>
#include <SoyData.h>

class TJobParams;
class SoyPixelsImpl;




class TPopRingFeatureParams
{
public:
	TPopRingFeatureParams();
	TPopRingFeatureParams(const	TJobParams& params);
	
	float		mContrastTolerance;
};




class TPopRingFeature
{
public:
	TPopRingFeature() :
		mBrighters	( 0 )
	{
	}
	
	static bool		GetFeature(TPopRingFeature& Feature,const SoyPixelsImpl& Pixels,int x,int y,const TPopRingFeatureParams& Params,std::stringstream& Error);
	
public:
	uint32		mBrighters;	//	1 brighter, 0 darker
};

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
