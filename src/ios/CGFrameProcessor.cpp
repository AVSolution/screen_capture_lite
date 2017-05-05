#include "CGFrameProcessor.h"
#include "TargetConditionals.h"
#include <ApplicationServices/ApplicationServices.h>
#include <iostream>

namespace SL {
    namespace Screen_Capture {
        struct CGFrameProcessorImpl {
            std::shared_ptr<Monitor_Thread_Data> Data;
 
        };


        CGFrameProcessor::CGFrameProcessor()
        {
            _CGFrameProcessorImpl = std::make_unique<CGFrameProcessorImpl>();
        }

        CGFrameProcessor::~CGFrameProcessor()
        {

        }
        DUPL_RETURN CGFrameProcessor::Init(std::shared_ptr<Monitor_Thread_Data> data) {
            auto ret = DUPL_RETURN::DUPL_RETURN_SUCCESS;
            _CGFrameProcessorImpl->Data = data;

            return ret;
        }
        //
        // Process a given frame and its metadata
        //
        DUPL_RETURN CGFrameProcessor::ProcessFrame()
        {
            auto Ret = DUPL_RETURN_SUCCESS;
            auto imageRef = CGDisplayCreateImage(Id(_CGFrameProcessorImpl->Data->SelectedMonitor));
            if(!imageRef) return DUPL_RETURN_ERROR_EXPECTED;//this happens when the monitors change.
            
            auto width = CGImageGetWidth(imageRef);
            auto height = CGImageGetHeight(imageRef);
            if(width!= Width(_CGFrameProcessorImpl->Data->SelectedMonitor) || height!= Height(_CGFrameProcessorImpl->Data->SelectedMonitor)){
                 CGImageRelease(imageRef);
                return DUPL_RETURN_ERROR_EXPECTED;//this happens when the monitors change.
            }
            
            auto prov = CGImageGetDataProvider(imageRef);
            if(!prov){
                 CGImageRelease(imageRef);
                return DUPL_RETURN_ERROR_EXPECTED;
            }
            auto bytesperrow = CGImageGetBytesPerRow(imageRef);
            auto bitsperpixel = CGImageGetBitsPerPixel(imageRef);
            // right now only support full 32 bit images.. Most desktops should run this as its the most efficent
            assert(bitsperpixel == PixelStride*8);
            
            auto rawdatas= CGDataProviderCopyData(prov);
            auto buf = CFDataGetBytePtr(rawdatas);
         
            auto datalen = width*height*PixelStride;
            _CGFrameProcessorImpl->NewImageBuffer.resize(datalen);
            if(bytesperrow == PixelStride*width){
                //most efficent, can be done in a single memcpy
                memcpy(_CGFrameProcessorImpl->NewImageBuffer.data(),buf, datalen);
            } else {
                //for loop needed to copy each row
                auto dst =_CGFrameProcessorImpl->NewImageBuffer.data();
                auto src =buf;
                for (auto h =0; h<height;h++) {
                    memcpy(dst,src, PixelStride*width);
                    dst +=PixelStride*width;
                    src +=bytesperrow;
                }
            }

            CFRelease(rawdatas);
 
            //this is not needed. It is freed when the image is released
            //CGDataProviderRelease(prov);
            
            CGImageRelease(imageRef);
            
            
            
            ImageRect imgrect;
            imgrect.left =  imgrect.top=0;
            imgrect.right =width;
            imgrect.bottom = height;
            if(_CGFrameProcessorImpl->Data->CaptureEntireMonitor){
           
                auto img = Create(imgrect, PixelStride, 0,_CGFrameProcessorImpl->NewImageBuffer.data());
                _CGFrameProcessorImpl->Data->CaptureEntireMonitor(img, _CGFrameProcessorImpl->Data->SelectedMonitor);
            }
            if (_CGFrameProcessorImpl->Data->CaptureDifMonitor) {
                if (_CGFrameProcessorImpl->FirstRun) {
                        //first time through, just send the whole image
                        auto wholeimgfirst = Create(imgrect, PixelStride, 0, _CGFrameProcessorImpl->NewImageBuffer.data());
                        _CGFrameProcessorImpl->Data->CaptureDifMonitor(wholeimgfirst, _CGFrameProcessorImpl->Data->SelectedMonitor);
                        _CGFrameProcessorImpl->FirstRun = false;
                } else {		
                
                
                    //user wants difs, lets do it!
                    auto newimg = Create(imgrect, PixelStride, 0, _CGFrameProcessorImpl->NewImageBuffer.data());
                    auto oldimg = Create(imgrect, PixelStride, 0, _CGFrameProcessorImpl->OldImageBuffer.data());
                    auto imgdifs = GetDifs(oldimg, newimg);

                    for (auto& r : imgdifs) {
                        auto padding = (r.left *PixelStride) + ((Width(newimg) - r.right)*PixelStride);
                        auto startsrc = _CGFrameProcessorImpl->NewImageBuffer.data();
                        startsrc += (r.left *PixelStride) + (r.top *PixelStride *Width(newimg));

                        auto difimg = Create(r, PixelStride, padding, startsrc);
                        _CGFrameProcessorImpl->Data->CaptureDifMonitor(difimg, _CGFrameProcessorImpl->Data->SelectedMonitor);

                    }
                }
                    _CGFrameProcessorImpl->NewImageBuffer.swap(_CGFrameProcessorImpl->OldImageBuffer);
            }
            return Ret;
        }

    }
}