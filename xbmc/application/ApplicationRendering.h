/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/IWindowManagerCallback.h"
#include "threads/CriticalSection.h"
#include "threads/Event.h"
#include "threads/IRunnable.h"
#include "threads/SystemClock.h"
#include "utils/Stopwatch.h"
#include "windowing/Resolution.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <string>

class CApplication;
class CGUIComponent;
class CThread;
class CWinSystemBase;

/*!
 * \brief Owns the window system, the GUI and the render loop.
 *
 * Where supported, all of it runs on a GUI thread of its own, which creates the window and its
 * graphics context, so that rendering and the application's processing do not hold each other up.
 * Elsewhere it runs on the thread that drives the application.
 */
class CApplicationRendering : public IWindowManagerCallback, private IRunnable
{
public:
  explicit CApplicationRendering(CApplication& app);
  ~CApplicationRendering() override;

  /*!
   * \brief Creates the window system, the window with its graphics context and the GUI, on the
   * GUI thread.
   */
  bool CreateGUI();

  /*!
   * \brief Destroys what CreateGUI() created and ends the GUI thread.
   */
  void DestroyGUI();

  /*!
   * \brief Starts rendering frames on the GUI thread, once the application is initialized.
   */
  void StartRendering();

  /*!
   * \brief Renders one frame, for when there is no GUI thread.
   */
  void RunIteration();

  /*!
   * \brief Whether the GUI runs on a thread of its own.
   */
  bool IsThreaded() const { return m_thread != nullptr; }

  /*!
   * \brief Runs work on the GUI thread and waits for it to finish.
   */
  void RunOnGUIThread(const std::function<void()>& work);

  bool InitWindow(RESOLUTION res = RES_INVALID);
  void ShowSplash(const std::string& message);

  /*!
   * \brief Lets calls from outside kodi (e.g. python) into the GUI for good, as the application
   * stops.
   */
  void ReleaseFrameMoveGuard();

  void LockFrameMoveGuard();
  void UnlockFrameMoveGuard();
  CCriticalSection& GetFrameMoveGuard() { return m_frameMoveGuard; }

  // IWindowManagerCallback
  void FrameMove(bool processEvents, bool processGUI = true) override;
  void Render() override;
  void Process() override;
  bool GetRenderGUI() const override;

private:
  // IRunnable
  void Run() override;

  bool DoCreateGUI();
  void DoDestroyGUI();
  void RunPendingWork();
  void ProcessSlow();

  /*!
   \brief Take note of where the picture has just been drawn, and confine the GUI to it if the
   viewer asked for that.
   The rectangle is read from the renderer rather than computed, so what is published is what
   happened. Runs before the window manager processes, which computes its dirty regions through
   the same transform it later renders through: changing the confinement between the two would
   leave the interface drawn in one place and repainted in another.
   */
  void UpdateDrawnPicture();

  CApplication& m_app;

  std::unique_ptr<CThread> m_thread;
  CEvent m_created;
  std::atomic<bool> m_createdOk{false};
  std::atomic<bool> m_rendering{false};
  std::atomic<bool> m_destroy{false};

  CCriticalSection m_workSection;
  std::deque<std::function<void()>> m_work;
  CEvent m_workAdded;

  std::unique_ptr<CWinSystemBase> m_pWinSystem;
  std::unique_ptr<CGUIComponent> m_pGUI;

  CStopWatch m_frameTime;
  CStopWatch m_slowTimer;
  XbmcThreads::EndTime<> m_guiRefreshTimer;
  std::chrono::time_point<std::chrono::steady_clock> m_lastRenderTime;
  bool m_skipGuiRender = false;

  //! \brief Whether the last UpdateDrawnPicture() had a picture on screen.
  bool m_drawnPictureLive{false};

  CCriticalSection
      m_frameMoveGuard; /*!< critical section for synchronizing GUI actions from inside and outside (python) */
  bool m_frameMoveGuardReleased{false};
  std::atomic_uint m_WaitingExternalCalls{
      0}; /*!< counts threads which are waiting to be processed in FrameMove */
  unsigned int m_ProcessedExternalCalls =
      0; /*!< counts calls which are processed during one "door open" cycle in FrameMove */
  unsigned int m_ProcessedExternalDecay =
      0; /*!< counts to close door after a few frames of no python activity */
};
