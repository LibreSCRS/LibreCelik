// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/macagentstarter.h"

#include "utils/libreceliklog.h"

#include <QProcess>
#include <QStringList>

#include <unistd.h>

#import <AppKit/AppKit.h>

namespace librecelik::agent {

namespace {

NSString* const kHostBundleId = @"org.librescrs.LibreMac";
constexpr char kAgentJobLabel[] = "org.librescrs.agent";

/// Applications as the user means it: the system folder (also reachable
/// through the data volume's firmlink) and the per-user one. A copy anywhere
/// else — a mounted disk image, Downloads — runs translocated and cannot
/// register a lasting agent, so it is reported, not launched.
bool inApplications(NSString* path)
{
    NSString* userApplications = [NSHomeDirectory() stringByAppendingString:@"/Applications/"];
    return [path hasPrefix:@"/Applications/"] || [path hasPrefix:@"/System/Volumes/Data/Applications/"] ||
           [path hasPrefix:userApplications];
}

} // namespace

HostLocation MacAgentStarter::locate()
{
    installedHost.clear();
    @autoreleasepool {
        NSArray<NSURL*>* urls = [NSWorkspace.sharedWorkspace URLsForApplicationsWithBundleIdentifier:kHostBundleId];
        for (NSURL* url in urls) {
            NSString* path = url.URLByResolvingSymlinksInPath.path;
            if (path != nil && inApplications(path)) {
                installedHost = QString::fromNSString(path);
                qCInfo(lcGeneral) << "MacAgentStarter: host installed at" << installedHost;
                return HostLocation::Installed;
            }
        }
        qCInfo(lcGeneral) << "MacAgentStarter: host found in Applications: no; elsewhere:" << urls.count;
        return urls.count > 0 ? HostLocation::ElsewhereOnly : HostLocation::NotFound;
    }
}

void MacAgentStarter::launchHost(std::function<void(bool)> done)
{
    if (installedHost.isEmpty()) {
        done(false);
        return;
    }
    @autoreleasepool {
        NSURL* url = [NSURL fileURLWithPath:installedHost.toNSString() isDirectory:YES];
        NSWorkspaceOpenConfiguration* configuration = [NSWorkspaceOpenConfiguration configuration];
        configuration.activates = NO;
        configuration.addsToRecentItems = NO;
        // The block copies `done`; the handler runs on an arbitrary queue, and
        // the coordinator hops it to the GUI thread itself.
        [NSWorkspace.sharedWorkspace openApplicationAtURL:url
                                            configuration:configuration
                                        completionHandler:^(NSRunningApplication* app, NSError* error) {
                                          done(app != nil && error == nil);
                                        }];
    }
}

void MacAgentStarter::kickAgent()
{
    // No -k: an agent that is already running must not be killed and
    // restarted; for a job that is not registered this fails harmlessly.
    QProcess::startDetached(QStringLiteral("/bin/launchctl"),
                            {QStringLiteral("kickstart"),
                             QStringLiteral("gui/%1/%2").arg(::getuid()).arg(QLatin1StringView(kAgentJobLabel))});
}

std::unique_ptr<AgentStarter> makePlatformAgentStarter()
{
    return std::make_unique<MacAgentStarter>();
}

} // namespace librecelik::agent
