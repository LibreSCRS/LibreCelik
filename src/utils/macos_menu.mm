// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QString>
#import <AppKit/AppKit.h>

static id s_observer = nil;
static QString s_aboutText;
static QString s_prefsText;

static void applyMenuItemTranslations()
{
    NSMenu* mainMenu = [NSApp mainMenu];
    if (mainMenu.numberOfItems == 0)
        return;

    NSMenu* appMenu = [mainMenu itemAtIndex:0].submenu;

    for (NSMenuItem* item in appMenu.itemArray) {
        if ([item.title hasPrefix:@"About"] || [item.title hasPrefix:@"О програму"]) {
            item.title = s_aboutText.toNSString();
            continue;
        }
        if ([item.title isEqualToString:@"Preferences\u2026"] || [item.title isEqualToString:@"Settings\u2026"] ||
            [item.title isEqualToString:@"Settings..."] || [item.title isEqualToString:@"Preferences..."] ||
            [item.title hasPrefix:@"Подешавања"]) {
            item.title = s_prefsText.toNSString();
        }
    }
}

void macosRetranslateAppMenu(const QString& aboutText, const QString& preferencesText)
{
    s_aboutText = aboutText;
    s_prefsText = preferencesText;

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
