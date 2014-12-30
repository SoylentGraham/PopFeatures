#pragma once
#include <ofxSoylent.h>
#include <SoyApp.h>
#include <TJob.h>
#include <TChannel.h>




class TPopFeatures : public SoyApp, public TJobHandler, public TChannelManager
{
public:
	TPopFeatures();
	
	virtual void	AddChannel(std::shared_ptr<TChannel> Channel) override;
	virtual bool	Update()	{	return mRunning;	}

	void			OnExit(TJobAndChannel& JobAndChannel);
	void			OnGetFeature(TJobAndChannel& JobAndChannel);
	
public:
	
	bool				mRunning;
};



