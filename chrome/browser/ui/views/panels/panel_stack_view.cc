// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/panel_stack_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/views/panels/panel_view.h"
#include "chrome/common/extensions/extension.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/views/panels/taskbar_window_thumbnailer_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

// static
NativePanelStack* NativePanelStack::Create(
    scoped_ptr<StackedPanelCollection> stacked_collection) {
#if defined(OS_WIN)
  return new PanelStackView(stacked_collection.Pass());
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

PanelStackView::PanelStackView(
    scoped_ptr<StackedPanelCollection> stacked_collection)
    : stacked_collection_(stacked_collection.Pass()),
      delay_initialized_(false),
      is_drawing_attention_(false),
      window_(NULL) {
  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.remove_standard_frame = true;
  params.transparent = true;
  // Empty size is not allowed so a temporary small size is passed. SetBounds
  // will be called later to update the bounds.
  params.bounds = gfx::Rect(0, 0, 1, 1);
  window_->Init(params);
  window_->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);
  window_->set_focus_on_creation(false);
  window_->SetOpacity(0x00);
  window_->AddObserver(this);
  window_->ShowInactive();
}

PanelStackView::~PanelStackView() {
}

void PanelStackView::EnsureInitialized() {
  // The stack view cannot be fully initialized until the first panel has been
  // added to the stack because we need the information from that panel.
  if (delay_initialized_)
    return;
  Panel* panel = stacked_collection_->top_panel();
  if (!panel)
    return;
  delay_initialized_ = true;

#if defined(OS_WIN)
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetAppModelIdForProfile(UTF8ToWide(panel->app_name()),
                                                panel->profile()->GetPath()),
      views::HWNDForWidget(window_));
#endif

  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

void PanelStackView::Close() {
  window_->Close();
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void PanelStackView::OnPanelAddedOrRemoved(Panel* panel) {
  EnsureInitialized();

  UpdateWindowOwnerForTaskbarIconAppearance(panel);

  window_->UpdateWindowTitle();
  window_->UpdateWindowIcon();
}

void PanelStackView::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

void PanelStackView::Minimize() {
  // When the owner stack window is minimized by the system, its live preview
  // is lost. We need to set it explicitly. This has to be done before the
  // minimization.
  CaptureThumbnailForLivePreview();

  window_->Minimize();
}

bool PanelStackView::IsMinimized() const {
  return window_->IsMinimized();
}

void PanelStackView::DrawSystemAttention(bool draw_attention) {
  // The underlying call of FlashFrame, FlashWindowEx, seems not to work
  // correctly if it is called more than once consecutively.
  if (draw_attention == is_drawing_attention_)
    return;
  is_drawing_attention_ = draw_attention;

  window_->FlashFrame(draw_attention);
}

string16 PanelStackView::GetWindowTitle() const {
  Panel* panel = stacked_collection_->top_panel();
  if (!panel)
    return string16();

  const extensions::Extension* extension = panel->GetExtension();
  return UTF8ToUTF16(extension && !extension->name().empty() ?
      extension->name() : panel->app_name());
}

gfx::ImageSkia PanelStackView::GetWindowAppIcon() {
  Panel* panel = stacked_collection_->top_panel();
  if (panel) {
    gfx::Image app_icon = panel->app_icon();
    if (!app_icon.IsEmpty())
      return *app_icon.ToImageSkia();
  }
  return gfx::ImageSkia();
}

gfx::ImageSkia PanelStackView::GetWindowIcon() {
  return GetWindowAppIcon();
}

views::Widget* PanelStackView::GetWidget() {
  return window_;
}

const views::Widget* PanelStackView::GetWidget() const {
  return window_;
}

void PanelStackView::DeleteDelegate() {
  delete this;
}

void PanelStackView::OnWidgetDestroying(views::Widget* widget) {
  window_ = NULL;
}

void PanelStackView::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
#if defined(OS_WIN)
  if (active && thumbnailer_)
    thumbnailer_->Stop();
#endif
}

void PanelStackView::OnNativeFocusChange(gfx::NativeView focused_before,
                                         gfx::NativeView focused_now) {
  // When the user selects the stacked panels via ALT-TAB or WIN-TAB, the
  // background stack window, instead of the foreground panel window, receives
  // WM_SETFOCUS message. To deal with this, we listen to the focus change event
  // and activate the most recently active panel.
#if defined(OS_WIN)
  if (focused_now == window_->GetNativeView()) {
    Panel* panel_to_focus = stacked_collection_->most_recently_active_panel();
    if (panel_to_focus)
      panel_to_focus->Activate();
  }
#endif
}

void PanelStackView::UpdateWindowOwnerForTaskbarIconAppearance(Panel* panel) {
#if defined(OS_WIN)
  HWND panel_window = views::HWNDForWidget(
      static_cast<PanelView*>(panel->native_panel())->window());

  HWND stack_window = NULL;
  StackedPanelCollection* stack = panel->stack();
  if (stack) {
    stack_window = views::HWNDForWidget(
        static_cast<PanelStackView*>(stack->native_stack())->window_);
  }

  // The extended style WS_EX_APPWINDOW is used to force a top-level window onto
  // the taskbar. In order for multiple stacked panels to appear as one, this
  // bit needs to be cleared.
  int value = ::GetWindowLong(panel_window, GWL_EXSTYLE);
  ::SetWindowLong(
      panel_window,
      GWL_EXSTYLE,
      stack_window ? (value & ~WS_EX_APPWINDOW) : (value | WS_EX_APPWINDOW));

  // All the windows that share the same owner window will appear as a single
  // window on the taskbar.
  ::SetWindowLongPtr(panel_window,
                     GWLP_HWNDPARENT,
                     reinterpret_cast<LONG>(stack_window));

#else
  NOTIMPLEMENTED();
#endif
}

void PanelStackView::CaptureThumbnailForLivePreview() {
#if defined(OS_WIN)
  // Live preview is only available since Windows 7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  HWND native_window = views::HWNDForWidget(window_);

  if (!thumbnailer_.get()) {
    DCHECK(native_window);
    thumbnailer_.reset(new TaskbarWindowThumbnailerWin(native_window));
    ui::HWNDSubclass::AddFilterToTarget(native_window, thumbnailer_.get());
  }

  std::vector<HWND> native_panel_windows;
  for (StackedPanelCollection::Panels::const_iterator iter =
            stacked_collection_->panels().begin();
        iter != stacked_collection_->panels().end(); ++iter) {
    Panel* panel = *iter;
    native_panel_windows.push_back(
        views::HWNDForWidget(
            static_cast<PanelView*>(panel->native_panel())->window()));
  }
  thumbnailer_->Start(native_panel_windows);
#endif
}
