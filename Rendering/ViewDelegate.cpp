//
//  ViewDelegate.cpp
//  learning-metal
//
//  Created by Lev Mitchell on 8/11/26.
//
#include "ViewDelegate.hpp"

MyMTKViewDelegate::MyMTKViewDelegate(MTL::Device* pDevice, MTK::View* pView) : MTK::ViewDelegate(), _pRenderer(new Renderer(pDevice, pView)) {}

MyMTKViewDelegate::~MyMTKViewDelegate() {
    delete _pRenderer;
}

void MyMTKViewDelegate::drawInMTKView(MTK::View* pView) {
    _pRenderer->draw(pView);
}
