// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "macos_menu.h"

#import <AppKit/AppKit.h>

// Both globals are written by macosRetranslateAppMenu (called from the Qt
// GUI thread) and read by applyMenuItemTranslations (called either directly
// from the setter or from the NSMenuDidBeginTrackingNotification block on
// [NSOperationQueue mainQueue]). Cocoa's main queue IS the Qt GUI thread in
// a Qt-on-Cocoa app, so all reads/writes are single-threaded and no lock
// is needed.
static id s_observer = nil;
static MacOsAppMenuStrings s_strings;

static void applyMenuItemTranslations()
{
    NSMenu* mainMenu = [NSApp mainMenu];
    if (mainMenu.numberOfItems == 0)
        return;

    NSMenu* appMenu = [mainMenu itemAtIndex:0].submenu;
    NSMenu* servicesMenu = [NSApp servicesMenu];

    for (NSMenuItem* item in appMenu.itemArray) {
        // Standard Cocoa items are wired to selectors that never change across
        // macOS versions or locales — match by selector for language-independent
        // and runtime-switch-safe rewrites.
        if (item.action == @selector(terminate:)) {
            item.title = s_strings.quit.toNSString();
            continue;
        }
        if (item.action == @selector(hide:)) {
            item.title = s_strings.hide.toNSString();
            continue;
        }
        if (item.action == @selector(hideOtherApplications:)) {
            item.title = s_strings.hideOthers.toNSString();
            continue;
        }
        if (item.action == @selector(unhideAllApplications:)) {
            item.title = s_strings.showAll.toNSString();
            continue;
        }
        // Services has no action; identify by the submenu pointer that
        // NSApplication tracks as the services menu.
        if (servicesMenu && item.submenu == servicesMenu) {
            item.title = s_strings.services.toNSString();
            continue;
        }
        // Qt promotes both AboutRole and AboutQtRole actions into this menu, and the
        // localized "About Qt" title starts with "About" / "О" too. Skip the Qt item
        // so our retranslate does not collapse both entries to the about string.
        if ([item.title rangeOfString:@"Qt"].location != NSNotFound) {
            continue;
        }
        // The About item has no stable Cocoa selector (Qt synthesises it for
        // AboutRole), so we fall back to a title prefix match. "О " (Cyrillic
        // О + space) is sufficient because no other app-menu item we manage
        // starts with that prefix in either supported locale.
        if ([item.title hasPrefix:@"About"] || [item.title hasPrefix:@"О "]) {
            item.title = s_strings.about.toNSString();
            continue;
        }
        if ([item.title isEqualToString:@"Preferences…"] || [item.title isEqualToString:@"Settings…"] ||
            [item.title isEqualToString:@"Settings..."] || [item.title isEqualToString:@"Preferences..."] ||
            [item.title hasPrefix:@"Подешавања"]) {
            item.title = s_strings.preferences.toNSString();
        }
    }
}

void macosRetranslateAppMenu(const MacOsAppMenuStrings& strings)
{
    s_strings = strings;

    applyMenuItemTranslations();

    if (s_observer == nil) {
        s_observer = [[NSNotificationCenter defaultCenter] addObserverForName:NSMenuDidBeginTrackingNotification
                                                                       object:nil
                                                                        queue:[NSOperationQueue mainQueue]
                                                                   usingBlock:^(NSNotification*) {
                                                                     applyMenuItemTranslations();
                                                                   }];
    }
}
