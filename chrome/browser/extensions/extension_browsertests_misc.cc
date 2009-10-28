// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/extensions/autoupdate_interceptor.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/extensions/extension_shelf.h"
#endif

#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

const std::wstring kSubscribePage =
    L"files/extensions/samples/subscribe_page_action/subscribe.html";
const std::wstring kValidFeed0 = L"files/feeds/feed_script.xml";
const std::wstring kValidFeed1 = L"files/feeds/feed1.xml";
const std::wstring kValidFeed2 = L"files/feeds/feed2.xml";
const std::wstring kValidFeed3 = L"files/feeds/feed3.xml";
const std::wstring kValidFeed4 = L"files/feeds/feed4.xml";
const std::wstring kValidFeed5 = L"files/feeds/feed5.xml";
const std::wstring kInvalidFeed1 = L"files/feeds/feed_invalid1.xml";
const std::wstring kInvalidFeed2 = L"files/feeds/feed_invalid2.xml";

// Looks for an ExtensionHost whose URL has the given path component (including
// leading slash).  Also verifies that the expected number of hosts are loaded.
static ExtensionHost* FindHostWithPath(ExtensionProcessManager* manager,
                                       const std::string& path,
                                       int expected_hosts) {
  ExtensionHost* host = NULL;
  int num_hosts = 0;
  for (ExtensionProcessManager::const_iterator iter = manager->begin();
       iter != manager->end(); ++iter) {
    if ((*iter)->GetURL().path() == path) {
      EXPECT_FALSE(host);
      host = *iter;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return host;
}

// Tests that toolstrips initializes properly and can run basic extension js.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, Toolstrip) {
  FilePath extension_test_data_dir = test_data_dir_.AppendASCII("good").
      AppendASCII("Extensions").AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj").
      AppendASCII("1.0.0.0");
  ASSERT_TRUE(LoadExtension(extension_test_data_dir));

  // At this point, there should be three ExtensionHosts loaded because this
  // extension has two toolstrips and one background page. Find the one that is
  // hosting toolstrip1.html.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/toolstrip1.html", 3);

  // Tell it to run some JavaScript that tests that basic extension code works.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testTabsAPI()", &result);
  EXPECT_TRUE(result);

#if defined(OS_WIN)
  // Test for compact language detection API. First navigate to a (static) html
  // file with a French sentence. Then, run the test API in toolstrip1.html to
  // actually call the language detection API through the existing extension,
  // and verify that the language returned is indeed French.
  FilePath language_url = extension_test_data_dir.AppendASCII(
      "french_sentence.html");
  ui_test_utils::NavigateToURL(
      browser(),
      GURL(language_url.ToWStringHack()));

  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testTabsLanguageAPI()", &result);
  EXPECT_TRUE(result);
#endif
}

#if defined(OS_WIN)  // TODO(port) -- enable
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ExtensionViews) {
  FilePath extension_test_data_dir = test_data_dir_.AppendASCII("good").
      AppendASCII("Extensions").AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj").
      AppendASCII("1.0.0.0");
  ASSERT_TRUE(LoadExtension(extension_test_data_dir));

  // At this point, there should be three ExtensionHosts loaded because this
  // extension has two toolstrips and one background page. Find the one that is
  // hosting toolstrip1.html.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/toolstrip1.html", 3);

  FilePath gettabs_url = extension_test_data_dir.AppendASCII(
      "test_gettabs.html");
  ui_test_utils::NavigateToURL(
      browser(),
      GURL(gettabs_url.value()));

  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testgetToolstripsAPI()", &result);
  EXPECT_TRUE(result);

  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testgetBackgroundPageAPI()", &result);
  EXPECT_TRUE(result);

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
           "test_gettabs.html"));
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testgetExtensionTabsAPI()", &result);
  EXPECT_TRUE(result);
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN)  // TODO(port) -- enable
// Tests that the ExtensionShelf initializes properly, notices that
// an extension loaded and has a view available, and then sets that up
// properly.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, Shelf) {
  // When initialized, there are no extension views and the preferred height
  // should be zero.
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ExtensionShelf* shelf = browser_view->extension_shelf();
  ASSERT_TRUE(shelf);
  EXPECT_EQ(shelf->GetChildViewCount(), 0);
  EXPECT_EQ(shelf->GetPreferredSize().height(), 0);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // There should now be two extension views and preferred height of the view
  // should be non-zero.
  EXPECT_EQ(shelf->GetChildViewCount(), 2);
  EXPECT_NE(shelf->GetPreferredSize().height(), 0);
}
#endif  // defined(OS_WIN)

// Tests that installing and uninstalling extensions don't crash with an
// incognito window open.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, Incognito) {
  // Open an incognito window to the extensions management page.  We just
  // want to make sure that we don't crash while playing with extensions when
  // this guy is around.
  Browser::OpenURLOffTheRecord(browser()->profile(),
                               GURL(chrome::kChromeUIExtensionsURL));

  ASSERT_TRUE(InstallExtension(test_data_dir_.AppendASCII("good.crx"), 1));
  UninstallExtension("ldnnhddmnhbkjipkidpdiheffobcpfmf");
}

// Tests that we can load extension pages into the tab area and they can call
// extension APIs.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, TabContents) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));

  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"testTabsAPI()", &result);
  EXPECT_TRUE(result);

  // There was a bug where we would crash if we navigated to a page in the same
  // extension because no new render view was getting created, so we would not
  // do some setup.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"testTabsAPI()", &result);
  EXPECT_TRUE(result);
}

#if defined(OS_WIN) || defined(OS_LINUX)
// Tests that we can load page actions in the Omnibox.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageAction) {
  // This page action will not show an icon, since it doesn't specify one but
  // is included here to test for a crash (http://crbug.com/25562).
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browsertest")
                    .AppendASCII("crash_25562")));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("samples")
                    .AppendASCII("subscribe_page_action")));

  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));

  // Navigate to the feed page.
  FilePath test_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  FilePath feed = test_dir.AppendASCII("feeds")
                          .AppendASCII("feed.html");

  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(feed));

  // We should now have one page action ready to go in the LocationBar.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  FilePath no_feed = test_dir.AppendASCII("feeds")
                             .AppendASCII("nofeed.html");

  // Make sure the page action goes away.
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(no_feed));
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));
}

// Tests that the location bar forgets about unloaded page actions.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, UnloadPageAction) {
  FilePath extension_path(test_data_dir_.AppendASCII("samples")
                                        .AppendASCII("subscribe_page_action"));
  ASSERT_TRUE(LoadExtension(extension_path));

  // Navigation prompts the location bar to load page actions.
  FilePath test_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  FilePath feed = test_dir.AppendASCII("feeds")
                          .AppendASCII("feed.html");

  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(feed));
  ASSERT_TRUE(WaitForPageActionCountChangeTo(1));

  UnloadExtension(last_loaded_extension_id_);

  // Make sure the page action goes away when it's unloaded.
  ASSERT_TRUE(WaitForPageActionCountChangeTo(0));
}
#endif  // defined(OS_WIN) || defined(OS_LINUX)

GURL GetFeedUrl(HTTPTestServer* server, const std::wstring& feed_page) {
  static GURL base_url = server->TestServerPageW(kSubscribePage);
  GURL feed_url = server->TestServerPageW(feed_page);
  return GURL(base_url.spec() + std::string("?") + feed_url.spec() +
         std::string("&synchronous"));
}

static const wchar_t* jscript_feed_title =
    L"window.domAutomationController.send("
    L"  document.getElementById('title') ? "
    L"    document.getElementById('title').textContent : "
    L"    \"element 'title' not found\""
    L");";
static const wchar_t* jscript_anchor =
    L"window.domAutomationController.send("
    L"  document.getElementById('anchor_0') ? "
    L"    document.getElementById('anchor_0').textContent : "
    L"    \"element 'anchor_0' not found\""
    L");";
static const wchar_t* jscript_desc =
    L"window.domAutomationController.send("
    L"  document.getElementById('desc_0') ? "
    L"    document.getElementById('desc_0').textContent : "
    L"    \"element 'desc_0' not found\""
    L");";
static const wchar_t* jscript_error =
    L"window.domAutomationController.send("
    L"  document.getElementById('error') ? "
    L"    document.getElementById('error').textContent : "
    L"    \"No error\""
    L");";

void GetParsedFeedData(HTTPTestServer* server,
                       const std::wstring& url,
                       Browser* browser,
                       const std::string& expected_feed_title,
                       const std::string& expected_item_title,
                       const std::string& expected_item_desc,
                       const std::string& expected_error) {
  std::string feed_title;
  std::string item_title;
  std::string item_desc;
  std::string error;

  ui_test_utils::NavigateToURL(browser, GetFeedUrl(server, url));

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser->GetSelectedTabContents()->render_view_host(),
      L"",  // Title is on the main page, all the rest is in the IFRAME.
      jscript_feed_title, &feed_title));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser->GetSelectedTabContents()->render_view_host(),
      L"//html/body/div/iframe[1]",
      jscript_anchor, &item_title));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser->GetSelectedTabContents()->render_view_host(),
      L"//html/body/div/iframe[1]",
      jscript_desc, &item_desc));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser->GetSelectedTabContents()->render_view_host(),
      L"//html/body/div/iframe[1]",
      jscript_error, &error));

  EXPECT_STREQ(expected_feed_title.c_str(), feed_title.c_str());
  EXPECT_STREQ(expected_item_title.c_str(), item_title.c_str());
  EXPECT_STREQ(expected_item_desc.c_str(), item_desc.c_str());
  EXPECT_STREQ(expected_error.c_str(), error.c_str());
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedValidFeed1) {
  HTTPTestServer* server = StartHTTPServer();
  GetParsedFeedData(server, kValidFeed1, browser(),
                    "Feed for 'MyFeedTitle'",
                    "Title 1",
                    "Desc",
                    "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedValidFeed2) {
  HTTPTestServer* server = StartHTTPServer();
  GetParsedFeedData(server, kValidFeed2, browser(),
                    "Feed for 'MyFeed2'",
                    "My item title1",
                    "This is a summary.",
                    "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedValidFeed3) {
  HTTPTestServer* server = StartHTTPServer();
  GetParsedFeedData(server, kValidFeed3, browser(),
                    "Feed for 'Google Code buglist rss feed'",
                    "My dear title",
                    "My dear content",
                    "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedValidFeed4) {
  HTTPTestServer* server = StartHTTPServer();
  GetParsedFeedData(server, kValidFeed4, browser(),
                    "Feed for 'Title chars <script> %23 stop'",
                    "Title chars <script> %23 stop",
                    "My dear content",
                    "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedValidFeed0) {
  HTTPTestServer* server = StartHTTPServer();
  // Try a feed with a link with an onclick handler (before r27440 this would
  // trigger a NOTREACHED).
  GetParsedFeedData(server, kValidFeed0, browser(),
                    "Feed for 'MyFeedTitle'",
                    "Title 1",
                    "Desc VIDEO",
                    "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedValidFeed5) {
  HTTPTestServer* server = StartHTTPServer();
  // Feed with valid but mostly empty xml.
  GetParsedFeedData(server, kValidFeed5, browser(),
                    "Feed for 'Unknown feed name'",
                    "element 'anchor_0' not found",
                    "element 'desc_0' not found",
                    "This feed contains no entries.");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedInvalidFeed1) {
  HTTPTestServer* server = StartHTTPServer();
  // Try an empty feed.
  GetParsedFeedData(server, kInvalidFeed1, browser(),
                    "Feed for 'Unknown feed name'",
                    "element 'anchor_0' not found",
                    "element 'desc_0' not found",
                    "Not a valid feed.");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedInvalidFeed2) {
  HTTPTestServer* server = StartHTTPServer();
  // Try a garbage feed.
  GetParsedFeedData(server, kInvalidFeed2, browser(),
                    "Feed for 'Unknown feed name'",
                    "element 'anchor_0' not found",
                    "element 'desc_0' not found",
                    "Not a valid feed.");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FLAKY_ParseFeedInvalidFeed3) {
  HTTPTestServer* server = StartHTTPServer();
  // Try a feed that doesn't exist.
  GetParsedFeedData(server, L"foo.xml", browser(),
                    "Feed for 'Unknown feed name'",
                    "element 'anchor_0' not found",
                    "element 'desc_0' not found",
                    "Not a valid feed.");
}

#if defined(OS_WIN)  // TODO(port) - enable.
// Tests that message passing between extensions and tabs works.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, MessagingExtensionTab) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));

  // Get the ExtensionHost that is hosting our toolstrip page.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/toolstrip.html", 1);

  // Load the tab that will communicate with our toolstrip.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://bjafgdebaacbbbecmhlhpofkepfkgcpa/page.html"));

  // Test extension->tab messaging.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testPostMessage()", &result);
  EXPECT_TRUE(result);

  // Test tab->extension messaging.
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testPostMessageFromTab()", &result);
  EXPECT_TRUE(result);

  // Test disconnect event dispatch.
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testDisconnect()", &result);
  EXPECT_TRUE(result);

  // Test disconnect is fired on tab close.
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testDisconnectOnClose()", &result);
  EXPECT_TRUE(result);
}
#endif  // defined(OS_WIN)

// Tests that an error raised during an async function still fires
// the callback, but sets chrome.extension.lastError.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, LastError) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browsertest").AppendASCII("last_error")));

  // Get the ExtensionHost that is hosting our toolstrip page.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/toolstrip.html", 1);

  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testLastError()", &result);
  EXPECT_TRUE(result);
}

#if defined(OS_WIN)  // TODO(port) - enable.
// Tests that message passing between extensions and content scripts works.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, MessagingContentScript) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));

  UserScriptMaster* master = browser()->profile()->GetUserScriptMaster();
  if (!master->ScriptsReady()) {
    // Wait for UserScriptMaster to finish its scan.
    NotificationRegistrar registrar;
    registrar.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                  NotificationService::AllSources());
    ui_test_utils::RunMessageLoop();
  }
  ASSERT_TRUE(master->ScriptsReady());

  // Get the ExtensionHost that is hosting our toolstrip page.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/toolstrip.html", 1);

  // Load the tab whose content script will communicate with our toolstrip.
  FilePath test_file;
  PathService::Get(chrome::DIR_TEST_DATA, &test_file);
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("test_file.html");
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  // Test extension->tab messaging.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testPostMessage()", &result);
  EXPECT_TRUE(result);

  // Test port naming
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testPortName()", &result);
  EXPECT_TRUE(result);

  // Test tab->extension messaging.
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testPostMessageFromTab()", &result);
  EXPECT_TRUE(result);

  // Test disconnect event dispatch.
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testDisconnect()", &result);
  EXPECT_TRUE(result);

  // Test disconnect is fired on tab close.
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testDisconnectOnClose()", &result);
  EXPECT_TRUE(result);
}
#endif  // defined(OS_WIN)

// TODO(mpcomplete): reenable after figuring it out.
#if 0
// Tests the process of updating an extension to one that requires higher
// permissions.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, UpdatePermissions) {
  TabContents* contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);

  // Install the initial version, which should happen just fine.
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("permissions-low-v1.crx"), 1));
  DCHECK_EQ(0, contents->infobar_delegate_count());

  // Upgrade to a version that wants more permissions. We should disable the
  // extension and prompt the user to reenable.
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("permissions-high-v2.crx"), -1));
  EXPECT_EQ(1, contents->infobar_delegate_count());

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  EXPECT_EQ(0u, service->extensions()->size());
  ASSERT_EQ(1u, service->disabled_extensions()->size());

  // Now try reenabling it, which should also dismiss the infobar.
  service->EnableExtension(service->disabled_extensions()->at(0)->id());
  EXPECT_EQ(0, contents->infobar_delegate_count());
  EXPECT_EQ(1u, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
}
#endif

// Tests that we can uninstall a disabled extension.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, UninstallDisabled) {
  // Install and upgrade, so that we have a disabled extension.
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("permissions-low-v1.crx"), 1));
  ASSERT_TRUE(UpdateExtension("pgdpcfcocojkjfbgpiianjngphoopgmo",
      test_data_dir_.AppendASCII("permissions-high-v2.crx"), -1));

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  EXPECT_EQ(0u, service->extensions()->size());
  ASSERT_EQ(1u, service->disabled_extensions()->size());

  // Now try uninstalling it.
  UninstallExtension(service->disabled_extensions()->at(0)->id());
  EXPECT_EQ(0u, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
}

// Tests that disabling and re-enabling an extension works.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, DisableEnable) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();

  // Load an extension, expect the toolstrip to be available.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));
  EXPECT_EQ(1u, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
  EXPECT_TRUE(FindHostWithPath(manager, "/toolstrip.html", 1));

  // After disabling, the toolstrip should go away.
  service->DisableExtension("bjafgdebaacbbbecmhlhpofkepfkgcpa");
  EXPECT_EQ(0u, service->extensions()->size());
  EXPECT_EQ(1u, service->disabled_extensions()->size());
  EXPECT_FALSE(FindHostWithPath(manager, "/toolstrip.html", 0));

  // And bring it back.
  service->EnableExtension("bjafgdebaacbbbecmhlhpofkepfkgcpa");
  EXPECT_EQ(1u, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
  EXPECT_TRUE(FindHostWithPath(manager, "/toolstrip.html", 1));
}

// Helper function for common code shared by the 3 WindowOpen tests below.
static TabContents* WindowOpenHelper(Browser* browser, const GURL& start_url,
                                    const std::string& newtab_url) {
  ui_test_utils::NavigateToURL(browser, start_url);

  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser->GetSelectedTabContents()->render_view_host(), L"",
      L"window.open('" + UTF8ToWide(newtab_url) + L"');"
      L"window.domAutomationController.send(true);", &result);
  EXPECT_TRUE(result);

  // Now the current tab should be the new tab.
  TabContents* newtab = browser->GetSelectedTabContents();
  GURL expected_url = start_url.Resolve(newtab_url);
  if (newtab->GetURL() != expected_url) {
    ui_test_utils::WaitForNavigation(
        &browser->GetSelectedTabContents()->controller());
  }
  EXPECT_EQ(newtab->GetURL(), expected_url);

  return newtab;
}

// Tests that an extension page can call window.open to an extension URL and
// the new window has extension privileges.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenExtension) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  TabContents* newtab = WindowOpenHelper(
      browser(),
      GURL(std::string("chrome-extension://") + last_loaded_extension_id_ +
           "/test.html"),
      "newtab.html");

  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      newtab->render_view_host(), L"", L"testExtensionApi()", &result);
  EXPECT_TRUE(result);
}

// Tests that if an extension page calls window.open to an invalid extension
// URL, the browser doesn't crash.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenInvalidExtension) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  WindowOpenHelper(
      browser(),
      GURL(std::string("chrome-extension://") + last_loaded_extension_id_ +
           "/test.html"),
      "chrome-extension://thisissurelynotavalidextensionid/newtab.html");

  // If we got to this point, we didn't crash, so we're good.
}

// Tests that calling window.open from the newtab page to an extension URL
// does not give the new window extension privileges - because the opening page
// does not have extension privileges.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenNoPrivileges) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  TabContents* newtab = WindowOpenHelper(
      browser(),
      GURL("about:blank"),
      std::string("chrome-extension://") + last_loaded_extension_id_ +
          "/newtab.html");

  // Extension API should fail.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      newtab->render_view_host(), L"", L"testExtensionApi()", &result);
  EXPECT_FALSE(result);
}

// Tests that a renderer's plugin list is properly updated when we load and
// unload an extension that contains a plugin.
// TODO(mpcomplete): need cross platform plugin support.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PluginLoadUnload) {
  FilePath extension_dir =
      test_data_dir_.AppendASCII("uitest").AppendASCII("plugins");

  ui_test_utils::NavigateToURL(browser(),
      net::FilePathToFileURL(extension_dir.AppendASCII("test.html")));
  TabContents* tab = browser()->GetSelectedTabContents();

  // With no extensions, the plugin should not be loaded.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result);
  EXPECT_FALSE(result);

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  ASSERT_TRUE(LoadExtension(extension_dir));
  EXPECT_EQ(1u, service->extensions()->size());
  // Now the plugin should be in the cache, but we have to reload the page for
  // it to work.
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result);
  EXPECT_FALSE(result);
  browser()->Reload();
  ui_test_utils::WaitForNavigationInCurrentTab(browser());
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result);
  EXPECT_TRUE(result);

  EXPECT_EQ(1u, service->extensions()->size());
  UnloadExtension(service->extensions()->at(0)->id());
  EXPECT_EQ(0u, service->extensions()->size());

  // Now the plugin should be out of the cache again, but existing pages will
  // still work until we reload them.

  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result);
  EXPECT_TRUE(result);
  browser()->Reload();
  ui_test_utils::WaitForNavigationInCurrentTab(browser());

  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result);
  EXPECT_FALSE(result);
}
#endif

// Tests extension autoupdate.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, AutoUpdate) {
  FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
  // Note: This interceptor gets requests on the IO thread.
  scoped_refptr<AutoUpdateInterceptor> interceptor(new AutoUpdateInterceptor());
  URLFetcher::enable_interception_for_tests(true);

  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/manifest",
                                     basedir.AppendASCII("manifest_v2.xml"));
  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/v2.crx",
                                     basedir.AppendASCII("v2.crx"));

  // Install version 1 of the extension.
  ASSERT_TRUE(InstallExtension(basedir.AppendASCII("v1.crx"), 1));
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  const ExtensionList* extensions = service->extensions();
  ASSERT_EQ(1u, extensions->size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf", extensions->at(0)->id());
  ASSERT_EQ("1.0", extensions->at(0)->VersionString());

  // We don't want autoupdate blacklist checks.
  service->updater()->set_blacklist_checks_enabled(false);

  // Run autoupdate and make sure version 2 of the extension was installed.
  service->updater()->CheckNow();
  ASSERT_TRUE(WaitForExtensionInstall());
  extensions = service->extensions();
  ASSERT_EQ(1u, extensions->size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf", extensions->at(0)->id());
  ASSERT_EQ("2.0", extensions->at(0)->VersionString());

  // Now try doing an update to version 3, which has been incorrectly
  // signed. This should fail.
  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/manifest",
                                     basedir.AppendASCII("manifest_v3.xml"));
  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/v3.crx",
                                     basedir.AppendASCII("v3.crx"));

  service->updater()->CheckNow();
  ASSERT_TRUE(WaitForExtensionInstallError());

  // Make sure the extension state is the same as before.
  extensions = service->extensions();
  ASSERT_EQ(1u, extensions->size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf", extensions->at(0)->id());
  ASSERT_EQ("2.0", extensions->at(0)->VersionString());
}

// Used to simulate a click on the first button named 'Options'.
static const wchar_t* jscript_click_option_button =
    L"(function() { "
    L"  var button = document.evaluate(\"//button[text()='Options']\","
    L"      document, null, XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE,"
    L"      null).snapshotItem(0);"
    L"  button.click();"
    L"  window.domAutomationController.send(0);"
    L"})();";

// Test that an extension with an options page makes an 'Options' button appear
// on chrome://extensions, and that clicking the button opens a new tab with the
// extension's options page.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OptionsPage) {
  // Install an extension with an options page.
  ASSERT_TRUE(InstallExtension(test_data_dir_.AppendASCII("options.crx"), 1));
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  const ExtensionList* extensions = service->extensions();
  ASSERT_EQ(1u, extensions->size());
  Extension* extension = extensions->at(0);

  // Go to the chrome://extensions page and click the Options button.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIExtensionsURL));
  TabStripModel* tab_strip = browser()->tabstrip_model();
  TabContents* extensions_tab = browser()->GetSelectedTabContents();
  ui_test_utils::ExecuteJavaScript(extensions_tab->render_view_host(), L"",
                                   jscript_click_option_button);

  // If the options page hasn't already come up, wait for it.
  if (tab_strip->count() == 1) {
    ui_test_utils::WaitForNewTab(browser());
  }
  ASSERT_EQ(2, tab_strip->count());

  EXPECT_EQ(extension->GetResourceURL("options.html"),
            tab_strip->GetTabContentsAt(1)->GetURL());
}

