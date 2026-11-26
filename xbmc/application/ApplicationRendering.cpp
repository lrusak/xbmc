/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ApplicationRendering.h"

#include "GUIInfoManager.h"
#include "GUILargeTextureManager.h"
#include "SeekHandler.h"
#include "ServiceBroker.h"
#include "application/AppInboundProtocol.h"
#include "application/AppParams.h"
#include "application/Application.h"
#include "application/ApplicationContentGeometry.h"
#include "application/ApplicationMessageHandling.h"
#include "application/ApplicationPlayer.h"
#include "application/ApplicationPowerHandling.h"
#include "application/ApplicationSkinHandling.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/TextureManager.h"
#include "guilib/guiinfo/GUIInfoProviders.h"
#include "input/InertialScrollingHandler.h"
#include "input/InputManager.h"
#include "interfaces/generic/ScriptInvocationManager.h"
#include "messaging/ApplicationMessenger.h"
#include "rendering/RenderSystem.h"
#include "rendering/capture/CaptureMetadata.h"
#include "rendering/capture/CapturePixels.h"
#include "rendering/capture/CaptureService.h"
#include "settings/AdvancedSettings.h"
#include "settings/DisplaySettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/Setting.h"
#include "threads/SingleLock.h"
#include "threads/Thread.h"
#include "utils/Screenshot.h"
#include "utils/SystemInfo.h"
#include "utils/TimeUtils.h"
#include "utils/XTimeUtils.h"
#include "utils/log.h"
#include "video/geometry/EffectiveGeometry.h"
#include "windowing/GraphicContext.h"
#include "windowing/WinSystem.h"
#include "windowing/WindowSystemFactory.h"

#include <algorithm>
#include <mutex>
#include <optional>

using namespace KODI;
using namespace std::chrono_literals;

namespace
{
// capture tap: serve pending requests from the finished frame before it is
// presented.
void ServiceCaptureTaps(CApplicationPlayer& player)
{
  using namespace KODI::RENDERING::CAPTURE;

  const auto captureService = CServiceBroker::GetCaptureService();
  if (!captureService)
    return;

  const auto requests = captureService->TakeActive(CaptureContent::COMPOSITE);
  if (requests.empty())
    return;
  const auto& request = requests.front(); // at most one consumer per frame

  auto* winSystem = CServiceBroker::GetWinSystem();
  auto surface = CScreenShot::CreateSurface();
  if (!winSystem || !surface)
  {
    captureService->Fail(request);
    return;
  }

  const ScreenshotContext ctx{*winSystem};
  if (!surface->Read(ctx))
  {
    captureService->Fail(request);
    return;
  }

  CaptureResult result;
  result.pixels =
      std::make_shared<CHeapCapturePixels>(std::unique_ptr<uint8_t[]>(surface->TakeBuffer()));
  result.width = static_cast<unsigned int>(surface->GetWidth());
  result.height = static_cast<unsigned int>(surface->GetHeight());
  result.stride = surface->GetStride();
  result.format = surface->GetFormat();
  result.color = GetOutputColorMetadata(*winSystem);
  result.content = CaptureContent::COMPOSITE; // this tap is the composite half

  if (!player.CompositeVideoCapture(result))
  {
    captureService->Fail(request);
    return;
  }

  captureService->Complete(request, result);
}

bool UseGUIThread()
{
#if defined(TARGET_WINDOWS_STORE) || defined(TARGET_WASM)
  //! @todo UWP: the window and the view's display information belong to the view thread, so
  //! these calls have to be handed to it first. WASM: the browser drives the main loop and its
  //! WebGL context.
  return false;
#else
  return true;
#endif
}
} // namespace

CApplicationRendering::CApplicationRendering(CApplication& app) : m_app(app)
{
}

CApplicationRendering::~CApplicationRendering() = default;

bool CApplicationRendering::CreateGUI()
{
  if (!UseGUIThread())
    return DoCreateGUI();

  m_thread = std::make_unique<CThread>(static_cast<IRunnable*>(this), "GUI");
  m_thread->Create();
  m_created.Wait();
  return m_createdOk;
}

void CApplicationRendering::DestroyGUI()
{
  if (!m_thread)
  {
    DoDestroyGUI();
    return;
  }

  m_destroy = true;
  m_workAdded.Set();
  m_thread->StopThread(true);
  m_thread.reset();
}

void CApplicationRendering::StartRendering()
{
  m_slowTimer.StartZero();
  m_rendering = true;
  m_workAdded.Set();
}

void CApplicationRendering::Run()
{
  CServiceBroker::GetAppMessenger()->SetGUIThread(CThread::GetCurrentThreadId());

  m_createdOk = DoCreateGUI();
  m_created.Set();

  // keep going after a failure, the application destroys what was created
  while (!m_destroy)
  {
    if (m_rendering && m_createdOk && !m_app.m_bStop)
    {
      RunIteration();
    }
    else
    {
      RunPendingWork();
      if (const auto messenger = CServiceBroker::GetAppMessenger())
        messenger->ProcessWindowMessages();
      m_workAdded.Wait(10ms);
    }
  }

  DoDestroyGUI();
}

void CApplicationRendering::RunOnGUIThread(const std::function<void()>& work)
{
  if (!m_thread || m_thread->IsCurrentThread())
  {
    work();
    return;
  }

  auto done = std::make_shared<CEvent>(true);
  {
    std::unique_lock lock(m_workSection);
    m_work.emplace_back(
        [&work, done]()
        {
          work();
          done->Set();
        });
  }
  m_workAdded.Set();

  // ensure the thread doesn't hold the graphics lock
  std::optional<CSingleExit> exit;
  if (CWinSystemBase* winSystem = CServiceBroker::GetWinSystem(); winSystem)
    exit.emplace(winSystem->GetGfxContext());

  // the work may wait for the application to handle a message
  const auto messenger = CServiceBroker::GetAppMessenger();
  while (!done->Wait(10ms))
  {
    if (messenger && messenger->IsProcessThread())
      messenger->ProcessMessages();
  }
}

void CApplicationRendering::RunPendingWork()
{
  std::deque<std::function<void()>> work;
  {
    std::unique_lock lock(m_workSection);
    work.swap(m_work);
  }

  for (const auto& item : work)
    item();
}

void CApplicationRendering::RunIteration()
{
  // Animate and render a frame

  const auto lastFrameTime = std::chrono::steady_clock::now();
  Process();

  const bool renderGUI = GetRenderGUI();
  if (!m_app.m_bStop)
  {
    FrameMove(true, renderGUI);
  }

  if (renderGUI && !m_app.m_bStop)
  {
    Render();
  }
  else if (!renderGUI)
  {
    constexpr std::chrono::milliseconds noRenderFrameTime{15}; // Simulates ~66fps
    const auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - lastFrameTime);
    if (frameTime < noRenderFrameTime)
      KODI::TIME::Sleep(noRenderFrameTime - frameTime);
  }
}

bool CApplicationRendering::DoCreateGUI()
{
  m_frameMoveGuard.lock();

  const auto appPower = m_app.GetComponent<CApplicationPowerHandling>();
  appPower->SetRenderGUI(true);

  auto windowSystems = KODI::WINDOWING::CWindowSystemFactory::GetWindowSystems();

  const std::string& windowing = CServiceBroker::GetAppParams()->GetWindowing();

  if (!windowing.empty())
    windowSystems = {windowing};

  for (auto& windowSystem : windowSystems)
  {
    CLog::Log(LOGDEBUG, "CApplicationRendering::{} - trying to init {} windowing system",
              __FUNCTION__, windowSystem);
    m_pWinSystem = KODI::WINDOWING::CWindowSystemFactory::CreateWindowSystem(windowSystem);

    if (!m_pWinSystem)
      continue;

    if (!windowing.empty() && windowing != windowSystem)
      continue;

    CServiceBroker::RegisterWinSystem(m_pWinSystem.get());

    if (!m_pWinSystem->InitWindowSystem())
    {
      CLog::Log(LOGDEBUG, "CApplicationRendering::{} - unable to init {} windowing system",
                __FUNCTION__, windowSystem);
      m_pWinSystem->DestroyWindowSystem();
      m_pWinSystem.reset();
      CServiceBroker::UnregisterWinSystem();
      continue;
    }
    else
    {
      CLog::Log(LOGINFO, "CApplicationRendering::{} - using the {} windowing system", __FUNCTION__,
                windowSystem);
      break;
    }
  }

  if (!m_pWinSystem)
  {
    CLog::Log(LOGFATAL, "CApplicationRendering::{} - unable to init windowing system",
              __FUNCTION__);
    CServiceBroker::UnregisterWinSystem();
    return false;
  }

  // Retrieve the matching resolution based on GUI settings
  bool sav_res = false;
  CDisplaySettings::GetInstance().SetCurrentResolution(
      CDisplaySettings::GetInstance().GetDisplayResolution());
  CLog::Log(LOGINFO, "Checking resolution {}",
            CDisplaySettings::GetInstance().GetCurrentResolution());
  if (!CServiceBroker::GetWinSystem()->GetGfxContext().IsValidResolution(
          CDisplaySettings::GetInstance().GetCurrentResolution()))
  {
    CLog::Log(LOGINFO, "Setting safe mode {}", RES_DESKTOP);
    // defer saving resolution after window was created
    CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP);
    sav_res = true;
  }

  // update the window resolution
  const std::shared_ptr<CSettings> settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  CServiceBroker::GetWinSystem()->SetWindowResolution(
      settings->GetInt(CSettings::SETTING_WINDOW_WIDTH),
      settings->GetInt(CSettings::SETTING_WINDOW_HEIGHT));

  m_app.ApplyRasterSettings();

  if (CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_startFullScreen &&
      CDisplaySettings::GetInstance().GetCurrentResolution() == RES_WINDOW)
  {
    // defer saving resolution after window was created
    CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP);
    sav_res = true;
  }

  if (!CServiceBroker::GetWinSystem()->GetGfxContext().IsValidResolution(
          CDisplaySettings::GetInstance().GetCurrentResolution()))
  {
    // Oh uh - doesn't look good for starting in their wanted screenmode
    CLog::Log(LOGERROR, "The screen resolution requested is not valid, resetting to a valid mode");
    CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP);
    sav_res = true;
  }
  if (!InitWindow())
  {
    return false;
  }

  // Set default screen saver mode
  auto screensaverModeSetting = std::static_pointer_cast<CSettingString>(
      settings->GetSetting(CSettings::SETTING_SCREENSAVER_MODE));
  // Can only set this after windowing has been initialized since it depends on it
  if (CServiceBroker::GetWinSystem()->GetOSScreenSaver())
  {
    // If OS has a screen saver, use it by default
    screensaverModeSetting->SetDefault("");
  }
  else
  {
    // If OS has no screen saver, use Kodi one by default
    screensaverModeSetting->SetDefault("screensaver.xbmc.builtin.dim");
  }

  if (sav_res)
    CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP, true);

  m_pGUI = std::make_unique<CGUIComponent>();
  m_pGUI->Init();

  // Splash requires gui component!!
  CServiceBroker::GetRenderSystem()->ShowSplash("");

  // The key mappings may already have been loaded by a peripheral
  CLog::Log(LOGINFO, "load keymapping");
  if (!CServiceBroker::GetInputManager().LoadKeymaps())
    return false;

  RESOLUTION_INFO info = CServiceBroker::GetWinSystem()->GetGfxContext().GetResInfo();
  CLog::Log(LOGINFO, "GUI format {}x{}, Display {}", info.iWidth, info.iHeight, info.strMode);

  m_lastRenderTime = std::chrono::steady_clock::now();
  return true;
}

void CApplicationRendering::DoDestroyGUI()
{
  if (m_pGUI)
  {
    m_pGUI->Deinit();
    m_pGUI.reset();
  }

  if (m_pWinSystem)
  {
    m_pWinSystem->DestroyWindowSystem();
    CServiceBroker::UnregisterWinSystem();
    m_pWinSystem.reset();
  }
}

bool CApplicationRendering::InitWindow(RESOLUTION res)
{
  if (res == RES_INVALID)
    res = CDisplaySettings::GetInstance().GetCurrentResolution();

  bool bFullScreen = res != RES_WINDOW;
  if (!CServiceBroker::GetWinSystem()->CreateNewWindow(
          CSysInfo::GetAppName(), bFullScreen,
          CDisplaySettings::GetInstance().GetResolutionInfo(res)))
  {
    CLog::Log(LOGFATAL, "CApplicationRendering::InitWindow: Unable to create window");
    return false;
  }

  if (!CServiceBroker::GetRenderSystem()->InitRenderSystem())
  {
    CLog::Log(LOGFATAL, "CApplicationRendering::InitWindow: Unable to init rendering system");
    return false;
  }
  // set GUI res and force the clear of the screen
  CServiceBroker::GetWinSystem()->GetGfxContext().SetVideoResolution(res, false);
  return true;
}

void CApplicationRendering::ShowSplash(const std::string& message)
{
  RunOnGUIThread(
      [&message]()
      {
        if (CRenderSystemBase* renderSystem = CServiceBroker::GetRenderSystem(); renderSystem)
          renderSystem->ShowSplash(message);
      });
}

void CApplicationRendering::ReleaseFrameMoveGuard()
{
  RunOnGUIThread(
      [this]()
      {
        m_frameMoveGuard.unlock();
        m_frameMoveGuardReleased = true;
      });
}

void CApplicationRendering::LockFrameMoveGuard()
{
  ++m_WaitingExternalCalls;
  m_frameMoveGuard.lock();
  ++m_ProcessedExternalCalls;
  CServiceBroker::GetWinSystem()->GetGfxContext().lock();
}

void CApplicationRendering::UnlockFrameMoveGuard()
{
  --m_WaitingExternalCalls;
  CServiceBroker::GetWinSystem()->GetGfxContext().unlock();
  m_frameMoveGuard.unlock();
}

bool CApplicationRendering::GetRenderGUI() const
{
  return m_app.GetComponent<CApplicationPowerHandling>()->GetRenderGUI();
}

void CApplicationRendering::Process()
{
  RunPendingWork();

  // dispatch the messages generated by python or other threads to the current window
  CServiceBroker::GetGUI()->GetWindowManager().DispatchThreadMessages();

  // process messages which have to be send to the gui
  // (this can only be done after CServiceBroker::GetGUI()->GetWindowManager().Render())
  CServiceBroker::GetAppMessenger()->ProcessWindowMessages();

  // handle any active scripts

  {
    // Allow processing of script threads to let them shut down properly.
    // Stop() has already released the guard; don't take it back on the way out
    const bool releaseGuard = !m_frameMoveGuardReleased;
    CSingleExit ex(CServiceBroker::GetWinSystem()->GetGfxContext());
    if (releaseGuard)
      m_frameMoveGuard.unlock();
    CScriptInvocationManager::GetInstance().Process();
    if (releaseGuard)
      m_frameMoveGuard.lock();
  }

  // without a GUI thread, the application is processed along with the GUI
  if (!IsThreaded())
    m_app.Process();
  if (m_app.m_bStop)
    return; //we're done, everything has been unloaded

  // the main loop reaches this with no GUI message handler suspended on the stack, which
  // is what a deferred skin reload is waiting for
  m_app.GetComponent<CApplicationSkinHandling>()->ProcessPendingSkinReload();

  // do any processing that isn't needed on each run
  if (m_slowTimer.GetElapsedMilliseconds() > 500)
  {
    m_slowTimer.Reset();
    ProcessSlow();
  }
}

void CApplicationRendering::ProcessSlow()
{
  // process skin resources (skin timers)
  m_app.GetComponent<CApplicationSkinHandling>()->ProcessSkin();

  // Check if we need to activate the screensaver / DPMS.
  const auto appPower = m_app.GetComponent<CApplicationPowerHandling>();
  appPower->CheckScreenSaverAndDPMS();

  CServiceBroker::GetGUI()->GetLargeTextureManager().CleanupUnusedImages();

  CServiceBroker::GetGUI()->GetTextureManager().FreeUnusedTextures(5000);

  // if we don't render the gui there's no reason to start the screensaver.
  // that way the screensaver won't kick in if we maximize the XBMC window
  // after the screensaver start time.
  if (!appPower->GetRenderGUI())
    appPower->ResetScreenSaverTimer();
}

void CApplicationRendering::UpdateDrawnPicture()
{
  CGraphicContext& context = CServiceBroker::GetWinSystem()->GetGfxContext();
  const auto geometry = m_app.GetComponent<CApplicationContentGeometry>();

  const bool playing = context.IsFullScreenVideo();
  if (!playing && !m_drawnPictureLive)
    return;

  m_drawnPictureLive = playing;

  VIDEO::GEOMETRY::DrawnGeometry drawn;
  CRect source;
  CRect video;
  CRect view;
  if (playing && m_app.GetComponent<CApplicationPlayer>()->GetRects(source, video, view))
  {
    drawn.picture =
        VIDEO::GEOMETRY::PictureOnScreen(geometry->GetRenderInputs().geometry, source, video);
    drawn.raster = context.GetRasterRect();
  }

  geometry->SetDrawn(drawn);

  const bool confineToPicture =
      geometry->OsdPlacementInForce() == VIDEO::GEOMETRY::OsdPlacement::Picture;
  if (context.SetGuiContentRect(confineToPicture ? drawn.picture : CRect{}))
    CServiceBroker::GetGUI()->GetWindowManager().MarkDirty();
}

void CApplicationRendering::FrameMove(bool processEvents, bool processGUI)
{
  const auto appPlayer = m_app.GetComponent<CApplicationPlayer>();
  bool renderGUI = m_app.GetComponent<CApplicationPowerHandling>()->GetRenderGUI();
  if (processEvents)
  {
    // currently we calculate the repeat time (ie time from last similar keypress) just global as fps
    float frameTime = m_frameTime.GetElapsedSeconds();
    m_frameTime.StartZero();
    // never set a frametime less than 2 fps to avoid problems when debugging and on breaks
    if (frameTime > 0.5f)
      frameTime = 0.5f;

    if (processGUI && renderGUI)
    {
      std::unique_lock lock(CServiceBroker::GetWinSystem()->GetGfxContext());
      // check if there are notifications to display
      CGUIDialogKaiToast* toast =
          CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogKaiToast>(
              WINDOW_DIALOG_KAI_TOAST);
      if (toast && toast->DoWork())
      {
        // also reopens the dialog if it is still running its close animation
        toast->Open();
      }
    }

    m_app.m_pMsgHandling->HandleEvents();
    CServiceBroker::GetInputManager().Process(
        CServiceBroker::GetGUI()->GetWindowManager().GetActiveWindowOrDialog(), frameTime);

    if (processGUI && renderGUI)
    {
      m_app.m_pInertialScrollingHandler->ProcessInertialScroll(frameTime);
      appPlayer->GetSeekHandler().FrameMove();
    }

    // Open the door for external calls e.g python exactly here.
    // Window size can be between 2 and 10ms and depends on number of continuous requests
    // Stop() has already released the guard; don't take it back on the way out
    if (m_WaitingExternalCalls && !m_app.m_bStop)
    {
      CSingleExit ex(CServiceBroker::GetWinSystem()->GetGfxContext());
      m_frameMoveGuard.unlock();

      // Calculate a window size between 2 and 10ms, 4 continuous requests let the window grow by 1ms
      // When not playing video we allow it to increase to 80ms
      unsigned int max_sleep = 10;
      if (!appPlayer->IsPlayingVideo() || appPlayer->IsPausedPlayback())
        max_sleep = 80;
      unsigned int sleepTime = std::max(static_cast<unsigned int>(2),
                                        std::min(m_ProcessedExternalCalls >> 2, max_sleep));
      KODI::TIME::Sleep(std::chrono::milliseconds(sleepTime));
      m_frameMoveGuard.lock();
      m_ProcessedExternalDecay = 5;
    }
    if (m_ProcessedExternalDecay && --m_ProcessedExternalDecay == 0)
      m_ProcessedExternalCalls = 0;
  }

  if (processGUI && renderGUI)
  {
    m_skipGuiRender = false;

    // a new request, or a latched one-shot still awaiting service, marks the
    // window manager dirty so an otherwise-idle GUI really renders a frame to tap
    if (const auto captureService = CServiceBroker::GetCaptureService();
        captureService && captureService->LatchFrame())
      CServiceBroker::GetGUI()->GetWindowManager().MarkDirty();

    /*! @todo look into the possibility to use this for GBM
    int fps = 0;

    // This code reduces rendering fps of the GUI layer when playing videos in fullscreen mode
    // it makes only sense on architectures with multiple layers
    if (CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo() && !m_appPlayer.IsPausedPlayback() && m_appPlayer.IsRenderingVideoLayer())
      fps = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_VIDEOPLAYER_LIMITGUIUPDATE);

    auto now = std::chrono::steady_clock::now();

    auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRenderTime).count();
    if (fps > 0 && frameTime * fps < 1000)
      m_skipGuiRender = true;
    */

    if (CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_guiSmartRedraw &&
        m_guiRefreshTimer.IsTimePast())
    {
      CServiceBroker::GetGUI()->GetWindowManager().SendMessage(GUI_MSG_REFRESH_TIMER, 0, 0);
      m_guiRefreshTimer.Set(500ms);
    }

    UpdateDrawnPicture();

    if (!m_app.m_bStop)
      CServiceBroker::GetGUI()->GetWindowManager().Process(CTimeUtils::GetFrameTime());

    // Dirty-driven skip: on paths with a persistent framebuffer (D2P plane or
    // HDR GUI compositing FBO), skip Render when no controls dirtied themselves
    // this frame. The persistence keeps the previous OSD on screen for free.
    if (!m_skipGuiRender && appPlayer->IsRenderingVideoLayer() &&
        !CServiceBroker::GetGUI()->GetWindowManager().HasDirtyRegions())
      m_skipGuiRender = true;
    CServiceBroker::GetGUI()->GetWindowManager().FrameMove();
  }

  appPlayer->FrameMove();

  // this will go away when render systems gets its own thread
  CServiceBroker::GetWinSystem()->DriveRenderLoop();
}

void CApplicationRendering::Render()
{
  // do not render if we are stopped or in background
  if (m_app.m_bStop)
    return;

  const auto appPlayer = m_app.GetComponent<CApplicationPlayer>();
  const auto appPower = m_app.GetComponent<CApplicationPowerHandling>();

  bool hasRendered = false;

  // Whether externalplayer is playing and we're unfocused
  bool extPlayerActive = appPlayer->IsExternalPlaying() && !m_app.m_AppFocused;

  if (!extPlayerActive && CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo() &&
      !appPlayer->IsPausedPlayback())
  {
    appPower->ResetScreenSaver();
  }

  if (!CServiceBroker::GetRenderSystem()->BeginRender())
    return;

  // render gui layer
  const bool guiWillRender = appPower->GetRenderGUI() && !m_skipGuiRender;
  bool compositing = CServiceBroker::GetWinSystem()->BeginGuiComposite(guiWillRender);

  if (guiWillRender)
  {
    if (CServiceBroker::GetWinSystem()->GetGfxContext().GetStereoMode() != RenderStereoMode::OFF)
    {
      CServiceBroker::GetWinSystem()->GetGfxContext().SetStereoView(RenderStereoView::LEFT);
      hasRendered |= CServiceBroker::GetGUI()->GetWindowManager().Render();

      if (CServiceBroker::GetWinSystem()->GetGfxContext().GetStereoMode() != RenderStereoMode::MONO)
      {
        CServiceBroker::GetWinSystem()->GetGfxContext().SetStereoView(RenderStereoView::RIGHT);
        hasRendered |= CServiceBroker::GetGUI()->GetWindowManager().Render();
      }
      CServiceBroker::GetWinSystem()->GetGfxContext().SetStereoView(RenderStereoView::OFF);
    }
    else
    {
      hasRendered |= CServiceBroker::GetGUI()->GetWindowManager().Render();
    }
    // execute post rendering actions (finalize window closing)
    CServiceBroker::GetGUI()->GetWindowManager().AfterRender();

    m_lastRenderTime = std::chrono::steady_clock::now();
  }

  if (compositing)
    CServiceBroker::GetWinSystem()->EndGuiComposite();

  // render video layer
  CServiceBroker::GetGUI()->GetWindowManager().RenderEx();

  if (compositing)
    CServiceBroker::GetWinSystem()->CompositeGui();

  // serve pending requests from the finished frame, then fail any left
  // unserved. Both need a frame that really drew: on a skipped frame nothing
  // could be served, so FrameComplete would kill live requests.
  if (hasRendered)
  {
    ServiceCaptureTaps(*appPlayer);
    if (const auto captureService = CServiceBroker::GetCaptureService())
      captureService->FrameComplete();
  }

  CServiceBroker::GetRenderSystem()->EndRender();

  // reset our info cache - we do this at the end of Render so that it is
  // fresh for the next process(), or after a windowclose animation (where process()
  // isn't called)
  CGUIInfoManager& infoMgr = CServiceBroker::GetGUI()->GetInfoManager();
  infoMgr.ResetCache();
  infoMgr.GetInfoProviders().GetGUIControlsInfoProvider().ResetContainerMovingCache();

  if (hasRendered)
  {
    infoMgr.GetInfoProviders().GetSystemInfoProvider().UpdateFPS();
  }

  CServiceBroker::GetWinSystem()->GetGfxContext().Flip(hasRendered,
                                                       appPlayer->IsRenderingVideoLayer());

  CTimeUtils::UpdateFrameTime(hasRendered);
}
