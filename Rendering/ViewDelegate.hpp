//
//  ViewDelegate.hpp
//  learning-metal
//
//  Created by Lev Mitchell on 8/11/26.
//
#pragma once

#include <MetalKit/MetalKit.hpp>
#include "Renderer.hpp"

class MyMTKViewDelegate : public MTK::ViewDelegate {
public:
    MyMTKViewDelegate(MTL::Device* pDevice, MTK::View* pView);
    virtual ~MyMTKViewDelegate() override;
    virtual void drawInMTKView(MTK::View* pView) override;
private:
    Renderer* _pRenderer;
};
