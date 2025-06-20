//=============================================================================
/// Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for a OpenCL-specific implementation of the status bar.
//=============================================================================

// C++.
#include <cassert>
// Qt.
#include <QStatusBar>
#include <QColor>

// Local.
#include "radeon_gpu_analyzer_gui/qt/rg_status_bar.h"
#include "radeon_gpu_analyzer_gui/qt/rg_status_bar_opencl.h"
#include "radeon_gpu_analyzer_gui/qt/rg_mode_push_button.h"

static const QString kStrModePushButtonStylesheet(
    "QPushButton"
    "{"
    "padding: 5px;"
    "border: none;"
    "color: white;"
    "}"
    "QPushButton:hover"
    "{"
    "background-color: rgba(20, 175, 0, 255);"
    "}"
);
static const QString kStrStatusBarBackgroundColorOpencl = "background-color: rgb(18, 152, 0);  color: white;";
static const QColor kModePushButtonHoverColor = QColor(20, 175, 0, 255);
static const QColor kModePushButtonNonHoverColor = QColor(18, 152, 0);

RgStatusBarOpencl::RgStatusBarOpencl(QStatusBar* status_bar, QWidget* parent) :
    RgStatusBar(status_bar, parent),
    parent_(parent)
{
    // Set style sheets.
    SetStylesheets(status_bar);
}

void RgStatusBarOpencl::SetStylesheets(QStatusBar* status_bar)
{
    assert(mode_push_button_);
    if (mode_push_button_)
    {
        mode_push_button_->setStyleSheet(kStrModePushButtonStylesheet);
        mode_push_button_->SetHoverColor(kModePushButtonHoverColor);
        mode_push_button_->SetNonHoverColor(kModePushButtonNonHoverColor);
    }

    // Set status bar stylesheet.
    assert(status_bar != nullptr);
    if (status_bar != nullptr)
    {
        status_bar->setStyleSheet(kStrStatusBarBackgroundColorOpencl);
    }
}
