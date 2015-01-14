#include "PopFeatures.h"
#include <TParameters.h>
#include <SoyDebug.h>
#include <TProtocolCli.h>
#include <TProtocolHttp.h>
#include <SoyApp.h>
#include <PopMain.h>
#include <TJobRelay.h>
#include <SoyPixels.h>
#include <SoyString.h>
#include "PopRingFeature.h"
#include <SortArray.h>


TPopFeatures::TPopFeatures()
{
	AddJobHandler("exit", TParameterTraits(), *this, &TPopFeatures::OnExit );
	
	AddJobHandler("newframe", TParameterTraits(), *this, &TPopFeatures::OnNewFrame );
	AddJobHandler("re:getframe", TParameterTraits(), *this, &TPopFeatures::OnNewFrame );
	
	TParameterTraits GetFeatureTraits;
	GetFeatureTraits.mAssumedKeys.PushBack("x");
	GetFeatureTraits.mAssumedKeys.PushBack("y");
	GetFeatureTraits.mRequiredKeys.PushBack("image");
	AddJobHandler("getfeature", GetFeatureTraits, *this, &TPopFeatures::OnGetFeature );

	TParameterTraits FindFeatureTraits;
	FindFeatureTraits.mAssumedKeys.PushBack("feature");
	FindFeatureTraits.mRequiredKeys.PushBack("image");
	AddJobHandler("findfeature", FindFeatureTraits, *this, &TPopFeatures::OnFindFeature );
}

void TPopFeatures::AddChannel(std::shared_ptr<TChannel> Channel)
{
	TChannelManager::AddChannel( Channel );
	if ( !Channel )
		return;
	TJobHandler::BindToChannel( *Channel );
}


void TPopFeatures::OnExit(TJobAndChannel& JobAndChannel)
{
	mConsoleApp.Exit();
	
	//	should probably still send a reply
	TJobReply Reply( JobAndChannel );
	Reply.mParams.AddDefaultParam(std::string("exiting..."));
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}


void TPopFeatures::OnGetFeature(TJobAndChannel& JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	
	//	pull image
	auto ImageParam = Job.mParams.GetParam("image");

	/*
	//	assume format is now set right, decode the pixels out of it
	SoyPixels Pixels;
	if ( !ImageParam.Decode(Pixels) )
	{
		std::stringstream Error;
		Error << "Failed to decode " << Job.mParams.GetParamAs<std::string>("image") << " to an image";
		TJobReply Reply( JobAndChannel );
		Reply.mParams.AddErrorParam( Error.str() );
		
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	*/
	
	SoyPixels Image;
	if ( !Job.mParams.GetParamAs("image",Image) )
	{
		std::stringstream Error;
		Error << "Failed to decode image param";
		TJobReply Reply( JobAndChannel );
		Reply.mParams.AddErrorParam( Error.str() );
		
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	auto PixelxParam = Job.mParams.GetParam("x");
	auto PixelyParam = Job.mParams.GetParam("y");
	int x = PixelxParam.Decode<int>();
	int y = PixelyParam.Decode<int>();
	


	//	return descriptor and stuff
	std::stringstream Error;
	TPopRingFeatureParams Params( Job.mParams );
	TPopRingFeature Feature;
	TFeatureExtractor::GetFeature( Feature, Image, x, y, Params, Error );
	
	TJobReply Reply( JobAndChannel );
	
	SoyData_Impl<TPopRingFeature> FeatureData( Feature );
	std::shared_ptr<SoyData> FeatureEncoded( new SoyData_Stack<std::string>() );
	static_cast<SoyData_Stack<std::string>&>(*FeatureEncoded).Encode( FeatureData );
	
	Reply.mParams.AddDefaultParam( FeatureEncoded );
	Reply.mParams.AddParam( PixelxParam );
	Reply.mParams.AddParam( PixelyParam );
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}



void TPopFeatures::OnFindFeature(TJobAndChannel& JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	
	//	pull image
	auto Image = Job.mParams.GetParamAs<SoyPixels>("image");
	auto Feature = Job.mParams.GetParamAs<TPopRingFeature>("Feature");
	
	if ( !Image.IsValid() )
	{
		std::stringstream Error;
		Error << "Failed to decode image param";
		TJobReply Reply( JobAndChannel );
		Reply.mParams.AddErrorParam( Error.str() );
		
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}

	//	run a search
	TPopRingFeatureParams Params( Job.mParams );
	Array<TFeatureMatch> FeatureMatches;
	std::stringstream Error;
	TFeatureExtractor::FindFeatureMatches( GetArrayBridge(FeatureMatches), Image, Feature, Params, Error );
	
	//	some some params back with the reply
	TJobReply Reply( JobAndChannel );
	Reply.mParams.AddParam( Job.mParams.GetParam("Feature") );
	
	
	//	gr: the internal SoyData system doesn't know this type, so won't auto encode :/ need to work on this!
	std::shared_ptr<SoyData_Impl<json::Object>> FeatureMatchesJsonData( new SoyData_Stack<json::Object>() );
	if ( FeatureMatchesJsonData->EncodeRaw( FeatureMatches ) )
	{
		std::shared_ptr<SoyData> FeatureMatchesJsonDataGen( FeatureMatchesJsonData );
		Reply.mParams.AddDefaultParam( FeatureMatchesJsonDataGen );
	}
	else
	{
		Reply.mParams.AddDefaultParam( FeatureMatches );
	}

	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}



void TPopFeatures::OnNewFrame(TJobAndChannel& JobAndChannel)
{
	auto& Job = JobAndChannel.GetJob();
	
	//	pull image
	auto ImageParam = Job.mParams.GetDefaultParam();
	SoyPixels Image;
	std::Debug << "Getting image from " << ImageParam.GetFormat() << std::endl;
	if ( !ImageParam.Decode( Image ) )
	{
		std::Debug << "Failed to decode image" << std::endl;
		return;
	}
	std::Debug << "Decoded image " << Image.GetWidth() << "x" << Image.GetHeight() << " " << Image.GetFormat() << std::endl;
}



class TChannelLiteral : public TChannel
{
public:
	TChannelLiteral(SoyRef ChannelRef) :
	TChannel	( ChannelRef )
	{
	}
	
	virtual void				GetClients(ArrayBridge<SoyRef>&& Clients)
	{

	}
	
	bool				FixParamFormat(TJobParam& Param,std::stringstream& Error) override
	{
		return true;
	}
	void		Execute(std::string Command)
	{
		TJobParams Params;
		Execute( Command, Params );
	}
	void		Execute(std::string Command,const TJobParams& Params)
	{
		auto& Channel = *this;
		TJob Job;
		Job.mParams = Params;
		Job.mParams.mCommand = Command;
		Job.mChannelMeta.mChannelRef = Channel.GetChannelRef();
		Job.mChannelMeta.mClientRef = SoyRef("x");
		
		//	send job to handler
		Channel.OnJobRecieved( Job );
	}
	
	//	we don't do anything, but to enable relay, we say it's "done"
	virtual bool				SendJobReply(const TJobReply& Job) override
	{
		OnJobSent( Job );
		return true;
	}
	virtual bool				SendCommandImpl(const TJob& Job) override
	{
		OnJobSent( Job );
		return true;
	}
};



//	horrible global for lambda
std::shared_ptr<TChannel> gStdioChannel;
std::shared_ptr<TChannel> gCaptureChannel;



TPopAppError::Type PopMain(TJobParams& Params)
{
	std::cout << Params << std::endl;
	
	TPopFeatures App;

	auto CommandLineChannel = std::shared_ptr<TChan<TChannelLiteral,TProtocolCli>>( new TChan<TChannelLiteral,TProtocolCli>( SoyRef("cmdline") ) );
	
	//	create stdio channel for commandline output
	gStdioChannel = CreateChannelFromInputString("std:", SoyRef("stdio") );
	auto HttpChannel = CreateChannelFromInputString("http:8080-8090", SoyRef("http") );
	auto WebSocketChannel = CreateChannelFromInputString("ws:json:9090-9099", SoyRef("websock") );
//	auto WebSocketChannel = CreateChannelFromInputString("ws:cli:9090-9099", SoyRef("websock") );
//	auto SocksChannel = CreateChannelFromInputString("cli:7070-7079", SoyRef("socks") );
	
	
	App.AddChannel( CommandLineChannel );
	App.AddChannel( gStdioChannel );
	App.AddChannel( HttpChannel );
	App.AddChannel( WebSocketChannel );
//	App.AddChannel( SocksChannel );

	//	when the commandline SENDs a command (a reply), send it to stdout
	auto RelayFunc = [](TJobAndChannel& JobAndChannel)
	{
		if ( !gStdioChannel )
			return;
		TJob Job = JobAndChannel;
		Job.mChannelMeta.mChannelRef = gStdioChannel->GetChannelRef();
		Job.mChannelMeta.mClientRef = SoyRef();
		gStdioChannel->SendCommand( Job );
	};
	CommandLineChannel->mOnJobSent.AddListener( RelayFunc );
	
	//	connect to another app, and subscribe to frames
	bool CreateCaptureChannel = false;
	if ( CreateCaptureChannel )
	{
		auto CaptureChannel = CreateChannelFromInputString("cli://localhost:7070", SoyRef("capture") );
		gCaptureChannel = CaptureChannel;
		CaptureChannel->mOnJobRecieved.AddListener( RelayFunc );
		App.AddChannel( CaptureChannel );
		
		//	send commands from stdio to new channel
		auto SendToCaptureFunc = [](TJobAndChannel& JobAndChannel)
		{
			TJob Job = JobAndChannel;
			Job.mChannelMeta.mChannelRef = gStdioChannel->GetChannelRef();
			Job.mChannelMeta.mClientRef = SoyRef();
			gCaptureChannel->SendCommand( Job );
		};
		gStdioChannel->mOnJobRecieved.AddListener( SendToCaptureFunc );
		
		auto StartSubscription = [](TChannel& Channel)
		{
			TJob GetFrameJob;
			GetFrameJob.mChannelMeta.mChannelRef = Channel.GetChannelRef();
			//GetFrameJob.mParams.mCommand = "subscribenewframe";
			//GetFrameJob.mParams.AddParam("serial", "isight" );
			GetFrameJob.mParams.mCommand = "getframe";
			GetFrameJob.mParams.AddParam("serial", "isight" );
			GetFrameJob.mParams.AddParam("memfile", "1" );
			Channel.SendCommand( GetFrameJob );
		};
		
		CaptureChannel->mOnConnected.AddListener( StartSubscription );
	}
	
	std::string TestFilename = "/users/grahamr/Desktop/ringo.png";
	
	//	gr: bootup commands
	auto BootupGet = [TestFilename](TChannel& Channel)
	{
		TJob GetFrameJob;
		GetFrameJob.mChannelMeta.mChannelRef = Channel.GetChannelRef();
		GetFrameJob.mParams.mCommand = "getfeature";
		GetFrameJob.mParams.AddParam("x", 120 );
		GetFrameJob.mParams.AddParam("y", 120 );
		GetFrameJob.mParams.AddParam("image", TestFilename, TJobFormat("text/file/png") );
		Channel.OnJobRecieved( GetFrameJob );
	};
	
	auto BootupMatch = [TestFilename](TChannel& Channel)
	{
		TJob GetFrameJob;
		GetFrameJob.mChannelMeta.mChannelRef = Channel.GetChannelRef();
		GetFrameJob.mParams.mCommand = "findfeature";
		GetFrameJob.mParams.AddParam("feature", "01011000000000001100100100000000" );
		GetFrameJob.mParams.AddParam("image", TestFilename, TJobFormat("text/file/png") );
		Channel.OnJobRecieved( GetFrameJob );
	};
	
	
	/*
	//	auto BootupFunc = BootupMatch;
	//auto BootupFunc = BootupGet;
	auto BootupFunc = BootupMatch;
	if ( CommandLineChannel->IsConnected() )
		BootupFunc( *CommandLineChannel );
	else
		CommandLineChannel->mOnConnected.AddListener( BootupFunc );
*/
	
	
	//	run
	App.mConsoleApp.WaitForExit();

	gStdioChannel.reset();
	return TPopAppError::Success;
}




