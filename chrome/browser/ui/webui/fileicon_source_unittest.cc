// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/fileicon_source.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;

namespace {

class TestFileIconSource : public FileIconSource {
 public:
  explicit TestFileIconSource() {}

  MOCK_METHOD4(FetchFileIcon,
               void(const base::FilePath& path,
                    ui::ScaleFactor scale_factor,
                    IconLoader::IconSize icon_size,
                    const content::URLDataSource::GotDataCallback& callback));

  virtual ~TestFileIconSource() {}
};

class FileIconSourceTest : public testing::Test {
 public:
  FileIconSourceTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  static TestFileIconSource* CreateFileIconSource() {
    return new TestFileIconSource();
  }

 private:
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

const struct FetchFileIconExpectation {
  const char* request_path;
  const base::FilePath::CharType* unescaped_path;
  ui::ScaleFactor scale_factor;
  IconLoader::IconSize size;
} kBasicExpectations[] = {
  { "foo?bar", FILE_PATH_LITERAL("foo"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "foo?bar&scale=2x", FILE_PATH_LITERAL("foo"), ui::SCALE_FACTOR_200P,
    IconLoader::NORMAL },
  { "foo?iconsize=small", FILE_PATH_LITERAL("foo"), ui::SCALE_FACTOR_100P,
    IconLoader::SMALL },
  { "foo?iconsize=normal", FILE_PATH_LITERAL("foo"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "foo?iconsize=large", FILE_PATH_LITERAL("foo"), ui::SCALE_FACTOR_100P,
    IconLoader::LARGE },
  { "foo?iconsize=asdf", FILE_PATH_LITERAL("foo"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "foo?blah=b&iconsize=small", FILE_PATH_LITERAL("foo"),
    ui::SCALE_FACTOR_100P, IconLoader::SMALL },
  { "foo?blah&iconsize=small", FILE_PATH_LITERAL("foo"), ui::SCALE_FACTOR_100P,
    IconLoader::SMALL },
  { "a%3Fb%26c%3Dd.txt?iconsize=small", FILE_PATH_LITERAL("a?b&c=d.txt"),
    ui::SCALE_FACTOR_100P, IconLoader::SMALL },
  { "a%3Ficonsize%3Dsmall?iconsize=large",
    FILE_PATH_LITERAL("a?iconsize=small"), ui::SCALE_FACTOR_100P,
    IconLoader::LARGE },
  { "o%40%23%24%25%26*()%20%2B%3D%3F%2C%3A%3B%22.jpg",
    FILE_PATH_LITERAL("o@#$%&*() +=?,:;\".jpg"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
#if defined(OS_WIN)
  { "c:/foo/bar/baz", FILE_PATH_LITERAL("c:\\foo\\bar\\baz"),
    ui::SCALE_FACTOR_100P, IconLoader::NORMAL },
  { "/foo?bar=asdf&asdf", FILE_PATH_LITERAL("\\foo"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "c%3A%2Fusers%2Ffoo%20user%2Fbar.txt",
    FILE_PATH_LITERAL("c:\\users\\foo user\\bar.txt"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "c%3A%2Fusers%2F%C2%A9%202000.pdf",
    FILE_PATH_LITERAL("c:\\users\\\xa9 2000.pdf"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "%E0%B6%9A%E0%B6%BB%E0%B7%9D%E0%B6%B8%E0%B7%8A",
    FILE_PATH_LITERAL("\x0d9a\x0dbb\x0ddd\x0db8\x0dca"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "%2Ffoo%2Fbar", FILE_PATH_LITERAL("\\foo\\bar"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "%2Fbaz%20(1).txt?iconsize=small", FILE_PATH_LITERAL("\\baz (1).txt"),
    ui::SCALE_FACTOR_100P, IconLoader::SMALL },
#else
  { "/foo/bar/baz", FILE_PATH_LITERAL("/foo/bar/baz"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "/foo?bar", FILE_PATH_LITERAL("/foo"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "%2Ffoo%2f%E0%B6%9A%E0%B6%BB%E0%B7%9D%E0%B6%B8%E0%B7%8A",
    FILE_PATH_LITERAL("/foo/\xe0\xb6\x9a\xe0\xb6\xbb\xe0\xb7\x9d")
    FILE_PATH_LITERAL("\xe0\xb6\xb8\xe0\xb7\x8a"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "%2Ffoo%2Fbar", FILE_PATH_LITERAL("/foo/bar"), ui::SCALE_FACTOR_100P,
    IconLoader::NORMAL },
  { "%2Fbaz%20(1).txt?iconsize=small", FILE_PATH_LITERAL("/baz (1).txt"),
    ui::SCALE_FACTOR_100P, IconLoader::SMALL },
#endif
};

// Test that the callback is NULL.
MATCHER(CallbackIsNull, "") {
  return arg.is_null();
}

}  // namespace

TEST_F(FileIconSourceTest, FileIconSource_Parse) {
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported(
      supported_scale_factors);

  for (unsigned i = 0; i < arraysize(kBasicExpectations); i++) {
    scoped_ptr<TestFileIconSource> source(CreateFileIconSource());
    content::URLDataSource::GotDataCallback callback;
    EXPECT_CALL(*source.get(),
                FetchFileIcon(
                    base::FilePath(kBasicExpectations[i].unescaped_path),
                    kBasicExpectations[i].scale_factor,
                    kBasicExpectations[i].size, CallbackIsNull()));
    source->StartDataRequest(kBasicExpectations[i].request_path, false,
                             callback);
  }
}
