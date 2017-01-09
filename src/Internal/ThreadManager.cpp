#include "ThreadManager.h"

SL::Screen_Capture::ThreadManager::ThreadManager()
{

}
SL::Screen_Capture::ThreadManager::~ThreadManager() {
	for (auto& t : m_ThreadData) {
		*t->TerminateThreadsEvent = true;
	}
	Join();
}

void SL::Screen_Capture::ThreadManager::Init(const Base_Thread_Data& data,  const ScreenCapture_Settings& settings)
{
	Reset();
	m_ThreadHandles.resize(settings.Monitors.size() + (settings.CaptureMouse ? 1 :0));// add another thread for mouse capturing
	m_ThreadData.resize(settings.Monitors.size());

	for (size_t i = 0; i < settings.Monitors.size(); ++i)
	{
		m_ThreadData[i] = std::make_shared<Monitor_Thread_Data>();
		m_ThreadData[i]->UnexpectedErrorEvent = data.UnexpectedErrorEvent;
		m_ThreadData[i]->ExpectedErrorEvent = data.ExpectedErrorEvent;
		m_ThreadData[i]->TerminateThreadsEvent = data.TerminateThreadsEvent;
		m_ThreadData[i]->SelectedMonitor = settings.Monitors[i];
		m_ThreadData[i]->CaptureDifMonitor = settings.CaptureDifMonitor;
		m_ThreadData[i]->CaptureEntireMonitor = settings.CaptureEntireMonitor;
		m_ThreadData[i]->CaptureInterval = settings.Monitor_Capture_Interval;
		m_ThreadHandles[i] = std::thread(&SL::Screen_Capture::RunCapture, m_ThreadData[i]);
	}
	if (settings.CaptureMouse) {
		auto mousedata = std::make_shared<Mouse_Thread_Data>();
		//m_ThreadHandles.back() = std::thread(&SL::Screen_Capture::RunCapture, mousedata);
	}
}

void SL::Screen_Capture::ThreadManager::Join()
{
	for (auto& t : m_ThreadHandles) {
		if (t.joinable()) {
			t.join();
		}
	}
}

void SL::Screen_Capture::ThreadManager::Reset()
{
	m_ThreadHandles.clear();
	m_ThreadData.clear();
}
