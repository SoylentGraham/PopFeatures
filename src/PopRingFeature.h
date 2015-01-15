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
	
	const Array<vec2x<int>>&	GetSampleOffsets();
	
public:
	int		mMatchStepX;
	int		mMatchStepY;
	float	mMinScore;
	float	mBrighterTolerance;
	int		mMaxMatches;
	float	mMinInterestingScore;
	
	//	gr: todo: split into multiple rings/guassian sample
	float	mRadius;
	int		mSampleCount;

private:
	Array<vec2x<int>>	mSampleOffsets;		//	cached from radius & step
};


class TPopRingFeature
{
public:
	TPopRingFeature()
	{
	}
	
	float		GetInterestingScore() const;
	float		GetMatchScore(const TPopRingFeature& Match,const TPopRingFeatureParams& Params) const;
	
public:
	BufferArray<bool,100>	mBrighters;	//	1 brighter, 0 darker
};
std::ostream& operator<< (std::ostream &out,const TPopRingFeature &in);


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
bool SoyData_Impl<json::Object>::Encode(const SoyData_Impl<TFeatureMatch>& Data);
template <> template<>
bool SoyData_Impl<json::Object>::Encode(const SoyData_Impl<Array<TFeatureMatch>>& Data);




class TFeatureExtractor
{
public:
	
	static bool		GetFeature(TPopRingFeature& Feature,const SoyPixelsImpl& Pixels,int x,int y,TPopRingFeatureParams& Params,std::stringstream& Error);
	static bool		FindFeatureMatches(ArrayBridge<TFeatureMatch>&& Matches,const SoyPixelsImpl& Pixels,const TPopRingFeature& Feature,TPopRingFeatureParams& Params,std::stringstream& Error);

};

template<> template<>
bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<TPopRingFeature>& Data) const;
template <> template<>
bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TPopRingFeature>& Data);
