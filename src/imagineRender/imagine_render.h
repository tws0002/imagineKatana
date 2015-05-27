#ifndef IMAGINE_RENDER_H
#define IMAGINE_RENDER_H

#ifdef KAT_V_2
#include <FnRender/plugin/RenderBase.h>
#else
#include <Render/RenderBase.h>
#endif

#define ENABLE_PREVIEW_RENDERS 1

#if ENABLE_PREVIEW_RENDERS
#include <FnDisplayDriver/FnKatanaDisplayDriver.h>
#include "image/output_image.h"
#endif

#include "misc_helpers.h"
#include "live_render_helpers.h"

namespace FnKat = Foundry::Katana;
namespace FnKatRender = FnKat::Render;

#include "scene.h"
#include "utils/params.h"
#include "raytracer/raytracer.h"

class ImagineRender : public Foundry::Katana::Render::RenderBase, RaytracerHost
{
public:
	ImagineRender(FnKat::FnScenegraphIterator rootIterator, FnKat::GroupAttribute arguments);

	struct RenderAOV
	{
		RenderAOV() : pChannelMessage(NULL), pFrameMessage(NULL)
		{
		}

		RenderAOV(const std::string& nm, const std::string& tp, unsigned int nc, int fid) : name(nm), type(tp), numChannels(nc),
			frameID(fid), pChannelMessage(NULL), pFrameMessage(NULL)
		{
		}

		std::string			name;
		std::string			type;
		unsigned int		numChannels;
		int					frameID;

		FnKat::NewFrameMessage*		pFrameMessage;
		FnKat::NewChannelMessage*	pChannelMessage;
	};

	virtual int start();
	virtual int stop();

	virtual void configureDiskRenderOutputProcess(FnKatRender::DiskRenderOutputProcess& diskRender, const std::string& outputName,
												  const std::string& outputPath, const std::string& renderMethodName, const float& frameTime) const;


	static Foundry::Katana::Render::RenderBase* create(FnKat::FnScenegraphIterator rootIterator, FnKat::GroupAttribute args)
	{
		return new ImagineRender(rootIterator, args);
	}

	static void flush()
	{

	}

	// stuff for live rendering

	// bizarrely, we seem to need this when the other render plugins don't, otherwise hasPendingDataUpdates() never gets called by Katana...
	virtual int startLiveEditing() { return 1; }

	virtual bool hasPendingDataUpdates() const;
	virtual int applyPendingDataUpdates();

	void restartLiveRender();

	virtual int queueDataUpdates(FnKat::GroupAttribute updateAttribute);



	// progress back from renderer

	virtual void progressChanged(float progress);

	virtual void tileDone(const TileInfo& tileInfo, unsigned int threadID);

protected:

	bool configureGeneralSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator, bool diskRender);
	bool configureRenderSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator, bool diskRender);
	bool configureDiskRenderOutputs(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);

	void buildCamera(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator cameraIterator);
	void buildSceneGeometry(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);

	void performDiskRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);
	void performPreviewRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);
	void performLiveRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);

	void flushCaches();

	//
	bool setupPreviewDataChannel(Foundry::Katana::Render::RenderSettings& settings);

	void startDiskRenderer();
	void startInteractiveRenderer(bool liveRender);

	void renderFinished();

protected:
	Scene*						m_pScene;
	Params						m_renderSettings;

	// for use with live-renders only...
	Raytracer*					m_pRaytracer;

	LiveRenderState				m_liveRenderState;
	std::string					m_renderCameraLocation;

	std::string					m_diskRenderOutputPath;
	bool						m_diskRenderConvertFromLinear;

	//
	std::string					m_katanaHost;
	unsigned int				m_katanaPort;

#if ENABLE_PREVIEW_RENDERS
	OutputImage*				m_pOutputImage;
	FnKat::KatanaPipe*			m_pDataPipe;
	FnKat::NewFrameMessage*		m_pFrame;
	FnKat::NewChannelMessage*	m_pPrimaryChannel;

	std::vector<int>			m_aInteractiveFrameIDs;
	std::vector<std::string>	m_aInteractiveChannelNames;
	std::vector<RenderAOV>		m_aExtraInteractiveAOVs;
#endif

	//
	CreationSettings			m_creationSettings;

	std::string					m_statsOutputPath;
	unsigned int				m_printMemoryStatistics;

	unsigned int				m_integratorType;
	bool						m_ambientOcclusion;
	bool						m_fastLiveRenders;

	bool						m_motionBlur;
	bool						m_frameDeterministic;

	bool						m_ROIActive;
	unsigned int				m_ROIStartX;
	unsigned int				m_ROIStartY;
	unsigned int				m_ROIWidth;
	unsigned int				m_ROIHeight;

	unsigned int				m_renderWidth;
	unsigned int				m_renderHeight;

	int							m_renderThreads;

	size_t						m_rendererOtherMemory;

	int							m_lastProgress;

	unsigned int				m_extraAOVsFlags;
};

#endif // IMAGINE_RENDER_H
