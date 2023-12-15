/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <Cocoa/Cocoa.h>
#include <SDL.h>

@interface DPPCResponder : NSResponder
@end

static DPPCResponder *dppcResponder;

/** @file SDL-specific main function additions for macOS hosts. */

// Replace Command shortcuts with Control shortcuts in the menu bar, so that
// they're not going to conflict with ones in the guest OS (but we still allow
// some of them to be used in the host OS, e.g. Control+Q to quit).
void remap_appkit_menu_shortcuts(void) {
    for (NSMenuItem *menuItem in [NSApp mainMenu].itemArray) {
        if (menuItem.hasSubmenu) {
            for (NSMenuItem *item in menuItem.submenu.itemArray) {
                if (item.keyEquivalentModifierMask & NSEventModifierFlagCommand) {
                    item.keyEquivalentModifierMask &= ~NSEventModifierFlagCommand;
                    item.keyEquivalentModifierMask |= NSEventModifierFlagControl;
                }
            }
        }
    }

    // Add a "Grab Mouse" commamand to the Window menu.
    NSMenuItem *windowMenuItem = [[NSApp mainMenu] itemWithTitle:@"Window"];
    if (windowMenuItem) {
        NSMenu *windowMenu = [windowMenuItem submenu];
        if (windowMenu) {
            NSMenuItem *grabMouseMenuItem = [[NSMenuItem alloc] initWithTitle:@"Grab Mouse"
                                                                        action:@selector(grabMouse:)
                                                                 keyEquivalent:@"g"];
            [grabMouseMenuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl];
            [windowMenu addItem:grabMouseMenuItem];
        }
    }

    // Insert a responder in the chain to handle the command (SDL does not
    // expose its NSAppDelegate implementation so we can't subclass it).
    dppcResponder = [DPPCResponder new];
    [dppcResponder setNextResponder:[NSApp nextResponder]];
    [NSApp setNextResponder:dppcResponder];
}

@implementation DPPCResponder

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if ([menuItem action] == @selector(grabMouse:)) {
        [menuItem setState:SDL_GetRelativeMouseMode() ? NSControlStateValueOn : NSControlStateValueOff];
        return YES;
    }
    return [super validateMenuItem:menuItem];
}

- (void)grabMouse:(id)sender {
    NSWindow *mainWindow = NSApp.mainWindow;
    if (mainWindow) {
        // If we're about to grab the mouse, move the mouse to the middle of the
        // window so that SDL knows that this window is what we want coordinates
        // to be relative to.
        if (!SDL_GetRelativeMouseMode()) {
            NSRect frame = mainWindow.frame;
            NSPoint center = NSMakePoint(frame.size.width/2, frame.size.height/2);
            center = [mainWindow convertPointToScreen:center];
            CGWarpMouseCursorPosition(center);
            mainWindow.subtitle = @"Mouse grabbed";
        } else {
            mainWindow.subtitle = @"";
        }
    }

    SDL_SetRelativeMouseMode(!SDL_GetRelativeMouseMode());
}

@end
