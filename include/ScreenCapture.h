#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <ostream>
#include <assert.h>

namespace SL {
    namespace Screen_Capture {

        struct Monitor {
            int Id = INT32_MAX;
            int Index = INT32_MAX;
            int Height = 0;
            int Width = 0;
            //Offsets are the number of pixels that a monitor can be from the origin. For example, users can shuffle their monitors around so this affects their offset.
            int OffsetX = 0;
            int OffsetY = 0;
            char Name[128] = { 0 };
        };

        struct ImageRect {
            int    left = 0;
            int    top = 0;
            int    right = 0;
            int    bottom = 0;
            bool Contains(const ImageRect& a) const {
                return left <= a.left && right >= a.right && top <= a.top && bottom >= a.bottom;
            }
        };
        struct Image {
            ImageRect Bounds;
            int Pixelstride = 4;
            int RowPadding = 0;
            const char* Data = nullptr;
        };

        inline bool operator==(const ImageRect& a, const ImageRect& b) {
            return b.left == a.left && b.right == a.right && b.top == a.top && b.bottom == a.bottom;
        }
        inline std::ostream& operator<<(std::ostream &os, const ImageRect& p)
        {
            return os << "left=" << p.left << " top=" << p.top << " right=" << p.right << " bottom=" << p.bottom;
        }

        //index to self in the GetMonitors() function
        int Index(const Monitor& mointor);
        //unique identifier
        int Id(const Monitor& mointor);
        int OffsetX(const Monitor& mointor);
        int OffsetY(const Monitor& mointor);
        const char* Name(const Monitor& mointor);
        int Height(const Monitor& mointor);
        int Width(const Monitor& mointor);

        int Height(const ImageRect& rect);
        int Width(const ImageRect& rect);

        int Height(const Image& img);
        int Width(const Image& img);
        const ImageRect& Rect(const Image& img);

        //number of bytes per row, NOT including the Rowpadding
        int RowStride(const Image& img);
        //number of bytes per row of padding
        int RowPadding(const Image& img);
        //the start of the image data, this is not guarenteed to be contiguos. You must use the Rowstride and rowpadding to examine the image
        const char* StartSrc(const Image& img);

        inline void Extract(const Image& img, char* dst, size_t dst_size) {
            auto totalsize = RowStride(img)*Height(img);
            assert(dst_size >= (int)totalsize);
            auto startdst = dst;
            auto startsrc = StartSrc(img);
            if (RowPadding(img) == 0) { //no padding, the entire copy can be a single memcpy call
                memcpy(startdst, startsrc, RowStride(img)*Height(img));
            }
            else {
                for (auto i = 0; i < Height(img); i++) {
                    memcpy(startdst, startsrc, RowStride(img));
                    startdst += RowStride(img);//advance to the next row
                    startsrc += RowStride(img) + RowPadding(img);//advance to the next row
                }
            }
        }

        std::vector<std::shared_ptr<Monitor>> GetMonitors();

        typedef std::function<void(const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor)> CaptureCallback;
        typedef std::function<void(const SL::Screen_Capture::Image* img, int x, int y)> MouseCallback;
        typedef std::function<std::vector<std::shared_ptr<Monitor>>()> MonitorCallback;

        class ScreenCaptureManagerImpl;
        class ScreenCaptureManager {
            std::unique_ptr<ScreenCaptureManagerImpl> _ScreenCaptureManagerImpl;

        public:
            ScreenCaptureManager();
            ~ScreenCaptureManager();

            //Used by the library to determine which monitors to watch
            void setMonitorsToCapture(MonitorCallback& cb);
            //Used by the library to determine which monitors to watch
            void setMonitorsToCapture(const MonitorCallback& cb);
            //Used by the library to determine the callback frequency
            void setFrameChangeInterval(int interval);
            //When a new frame is available the callback is invoked
            void onNewFrame(CaptureCallback& cb);
            //When a new frame is available the callback is invoked
            void onNewFrame(const CaptureCallback& cb);
            //When a change in a frame is detected, the callback is invoked
            void onFrameChanged(CaptureCallback& cb);
            //When a change in a frame is detected, the callback is invoked
            void onFrameChanged(const CaptureCallback& cb);
            //When a mouse image changes or the mouse changes position, the callback is invoked.
            void onMouseChanged(MouseCallback& cb);
            //When a mouse image changes or the mouse changes position, the callback is invoked.
            void onMouseChanged(const MouseCallback& cb);
            //Used by the library to determine the callback frequency
            void setMouseChangeInterval(int interval);

            void Start();
            void Stop();
        };
    
    }
}