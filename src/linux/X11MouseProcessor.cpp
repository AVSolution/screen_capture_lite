#include "X11FrameProcessor.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <sys/shm.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XShm.h>
#include <assert.h>

namespace SL {
	namespace Screen_Capture {

		struct X11FrameProcessorImpl {

			std::shared_ptr<THREAD_DATA> Data;
			std::unique_ptr<char[]> OldImageBuffer, NewImageBuffer;
			size_t ImageBufferSize;
			bool FirstRun;
			Display* SelectedDisplay;
			Window RootWindow;
			XImage* Image;
			std::unique_ptr<XShmSegmentInfo> ShmInfo;

		};

		X11FrameProcessor::X11FrameProcessor()
		{
			_X11FrameProcessorImpl = std::make_unique<X11FrameProcessorImpl>();
			_X11FrameProcessorImpl->ImageBufferSize = 0;
			_X11FrameProcessorImpl->FirstRun = true;
			_X11FrameProcessorImpl->SelectedDisplay = nullptr;
			_X11FrameProcessorImpl->Image = nullptr;
		}

		X11FrameProcessor::~X11FrameProcessor()
		{

			if (_X11FrameProcessorImpl->ShmInfo) {
				shmdt(_X11FrameProcessorImpl->ShmInfo->shmaddr);
				shmctl(_X11FrameProcessorImpl->ShmInfo->shmid, IPC_RMID, 0);
				XShmDetach(_X11FrameProcessorImpl->SelectedDisplay, _X11FrameProcessorImpl->ShmInfo.get());
			}
			if (_X11FrameProcessorImpl->Image) {
				XDestroyImage(_X11FrameProcessorImpl->Image);
			}
			if (_X11FrameProcessorImpl->SelectedDisplay) {
				XCloseDisplay(_X11FrameProcessorImpl->SelectedDisplay);
			}
		}
		DUPL_RETURN X11FrameProcessor::Init(std::shared_ptr<THREAD_DATA> data) {
			auto ret = DUPL_RETURN::DUPL_RETURN_SUCCESS;
			_X11FrameProcessorImpl->Data = data;
			_X11FrameProcessorImpl->ImageBufferSize = Height(*data->SelectedMonitor)*Width(*data->SelectedMonitor)*PixelStride;
			if (_X11FrameProcessorImpl->Data->CaptureDifMonitor) {//only need the old buffer if difs are needed. If no dif is needed, then the image is always new
				_X11FrameProcessorImpl->OldImageBuffer = std::make_unique<char[]>(_X11FrameProcessorImpl->ImageBufferSize);
			}
			_X11FrameProcessorImpl->NewImageBuffer = std::make_unique<char[]>(_X11FrameProcessorImpl->ImageBufferSize);
			_X11FrameProcessorImpl->SelectedDisplay = XOpenDisplay(NULL);
			if (!_X11FrameProcessorImpl->SelectedDisplay) {
				return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED;
			}


			_X11FrameProcessorImpl->RootWindow = XRootWindow(_X11FrameProcessorImpl->SelectedDisplay, Index(*_X11FrameProcessorImpl->Data->SelectedMonitor));
			if (!_X11FrameProcessorImpl->RootWindow) {
				return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED;
			}
			auto visual = DefaultVisual(_X11FrameProcessorImpl->SelectedDisplay, Index(*_X11FrameProcessorImpl->Data->SelectedMonitor));
			auto depth = DefaultDepth(_X11FrameProcessorImpl->SelectedDisplay, Index(*_X11FrameProcessorImpl->Data->SelectedMonitor));

			_X11FrameProcessorImpl->ShmInfo = std::make_unique<XShmSegmentInfo>();

			_X11FrameProcessorImpl->Image = XShmCreateImage(_X11FrameProcessorImpl->SelectedDisplay, visual, depth, ZPixmap, NULL, _X11FrameProcessorImpl->ShmInfo.get(), Width(*_X11FrameProcessorImpl->Data->SelectedMonitor), Height(*_X11FrameProcessorImpl->Data->SelectedMonitor));
			_X11FrameProcessorImpl->ShmInfo->shmid = shmget(IPC_PRIVATE, _X11FrameProcessorImpl->Image->bytes_per_line * _X11FrameProcessorImpl->Image->height, IPC_CREAT | 0777);

			_X11FrameProcessorImpl->ShmInfo->readOnly = False;
			_X11FrameProcessorImpl->ShmInfo->shmaddr = _X11FrameProcessorImpl->Image->data = (char*)shmat(_X11FrameProcessorImpl->ShmInfo->shmid, 0, 0);

			XShmAttach(_X11FrameProcessorImpl->SelectedDisplay, _X11FrameProcessorImpl->ShmInfo.get());


			return ret;
		}
		//
		// Process a given frame and its metadata
		//
		DUPL_RETURN X11FrameProcessor::ProcessFrame()
		{
			auto Ret = DUPL_RETURN_SUCCESS;
			ImageRect imgrect;
			imgrect.left = imgrect.top = 0;
			imgrect.right = Width(*_X11FrameProcessorImpl->Data->SelectedMonitor);
			imgrect.bottom = Height(*_X11FrameProcessorImpl->Data->SelectedMonitor);


			XShmGetImage(_X11FrameProcessorImpl->SelectedDisplay, _X11FrameProcessorImpl->RootWindow, _X11FrameProcessorImpl->Image, 0, 0, AllPlanes);
			memcpy(_X11FrameProcessorImpl->NewImageBuffer.get(), _X11FrameProcessorImpl->Image->data, PixelStride*imgrect.right*imgrect.bottom);


			if (_X11FrameProcessorImpl->Data->CaptureEntireMonitor) {

				auto img = CreateImage(imgrect, PixelStride, 0, _X11FrameProcessorImpl->NewImageBuffer.get());
				_X11FrameProcessorImpl->Data->CaptureEntireMonitor(*img, *_X11FrameProcessorImpl->Data->SelectedMonitor);
			}
			if (_X11FrameProcessorImpl->Data->CaptureDifMonitor) {
				if (_X11FrameProcessorImpl->FirstRun) {
					//first time through, just send the whole image
					auto wholeimgfirst = CreateImage(imgrect, PixelStride, 0, _X11FrameProcessorImpl->NewImageBuffer.get());
					_X11FrameProcessorImpl->Data->CaptureDifMonitor(*wholeimgfirst, *_X11FrameProcessorImpl->Data->SelectedMonitor);
					_X11FrameProcessorImpl->FirstRun = false;
				}
				else {


					//user wants difs, lets do it!
					auto newimg = CreateImage(imgrect, PixelStride, 0, _X11FrameProcessorImpl->NewImageBuffer.get());
					auto oldimg = CreateImage(imgrect, PixelStride, 0, _X11FrameProcessorImpl->OldImageBuffer.get());
					auto imgdifs = GetDifs(*oldimg, *newimg);

					for (auto& r : imgdifs) {
						auto padding = (r.left *PixelStride) + ((Width(*newimg) - r.right)*PixelStride);
						auto startsrc = _X11FrameProcessorImpl->NewImageBuffer.get();
						startsrc += (r.left *PixelStride) + (r.top *PixelStride *Width(*newimg));

						auto difimg = CreateImage(r, PixelStride, padding, startsrc);
						_X11FrameProcessorImpl->Data->CaptureDifMonitor(*difimg, *_X11FrameProcessorImpl->Data->SelectedMonitor);

					}
				}
				std::swap(_X11FrameProcessorImpl->NewImageBuffer, _X11FrameProcessorImpl->OldImageBuffer);
			}
			return Ret;
		}

	}
}