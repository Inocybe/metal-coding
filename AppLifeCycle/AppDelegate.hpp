//
//  AppDelegate.hpp
//  learning-metal
//
//  Created by Lev Mitchell on 8/11/26.
//
#pragma once
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
#include "ViewDelegate.hpp"

class MyAppDelegate : public NS::ApplicationDelegate {
public:
    ~MyAppDelegate();
    
    NS::Menu* createMenuBar();
    
    virtual void applicationWillFinishLaunching(NS::Notification* pNotification) override;
    virtual void applicationDidFinishLaunching(NS::Notification* pNotification) override;
    virtual bool applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender ) override;
private:
    NS::Window* _pWindow;
    MTK::View* _pMtkView;
    MTL::Device* _pDevice;
    MyMTKViewDelegate* _pViewDelegate = nullptr;
};
