// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#import "AppDelegate.h"

#import "GameViewController.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename
{
    NSDictionary * userInfo = @{ @"filename" : filename };
    [[NSNotificationCenter defaultCenter] postNotificationName:@"openFile" object:self userInfo:userInfo];
    return YES;
}

// --------------------------------------------------------------------------------------
// Menu Handlers

// File / Open
- (IBAction)openDocument:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"openDocument" object:self];
}

// View / Reset Image Position
- (IBAction)resetImagePosition:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"resetImagePosition" object:self];
}

// View / Previous Image
- (IBAction)previousImage:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"previousImage" object:self];
}

// View / Next Image
- (IBAction)nextImage:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"nextImage" object:self];
}

// View / Toggle SRGB Highlight
- (IBAction)toggleSRGB:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"toggleSRGB" object:self];
}

// View / Show Overlay
- (IBAction)showOverlay:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"showOverlay" object:self];
}

// View / Hide Overlay
- (IBAction)hideOverlay:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"hideOverlay" object:self];
}

// View / Toggle HDR
- (IBAction)toggleHDR:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"toggleHDR" object:self];
}

// Diff / Diff Current Image Against...
- (IBAction)diffCurrentImageAgainst:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"diffCurrentImageAgainst" object:self];
}

// Diff / Show Image 1
- (IBAction)showImage1:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"showImage1" object:self];
}

// Diff / Show Image 2
- (IBAction)showImage2:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"showImage2" object:self];
}

// Diff / Show Diff
- (IBAction)showDiff:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"showDiff" object:self];
}

// Diff / Adjust Threshold -1
- (IBAction)adjustThresholdM1:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"adjustThresholdM1" object:self];
}

// Diff / Adjust Threshold -5
- (IBAction)adjustThresholdM5:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"adjustThresholdM5" object:self];
}

// Diff / Adjust Threshold -50
- (IBAction)adjustThresholdM50:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"adjustThresholdM50" object:self];
}

// Diff / Adjust Threshold -500
- (IBAction)adjustThresholdM500:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"adjustThresholdM500" object:self];
}

// Diff / Diff Intensity: Original
- (IBAction)diffIntensityOriginal:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"diffIntensityOriginal" object:self];
}

// Diff / Diff Intensity: Bright
- (IBAction)diffIntensityBright:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"diffIntensityBright" object:self];
}

// Diff / Diff Intensity: Diff Oly
- (IBAction)diffIntensityDiffOnly:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"diffIntensityDiffOnly" object:self];
}

// Diff / Adjust Threshold +1
- (IBAction)adjustThresholdP1:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"adjustThresholdP1" object:self];
}

// Diff / Adjust Threshold +5
- (IBAction)adjustThresholdP5:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"adjustThresholdP5" object:self];
}

// Diff / Adjust Threshold +50
- (IBAction)adjustThresholdP50:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"adjustThresholdP50" object:self];
}

// Diff / Adjust Threshold +500
- (IBAction)adjustThresholdP500:sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"adjustThresholdP500" object:self];
}

@end