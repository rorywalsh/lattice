#ifndef LATTICE_IOS

#import <Cocoa/Cocoa.h>

extern "C" {
    bool attachViewToParent(void* childView, void* parentView) {
        @autoreleasepool {
            NSView* child = (__bridge NSView*)childView;
            NSView* parent = (__bridge NSView*)parentView;

            NSLog(@"Child view frame before: %@", NSStringFromRect([child frame]));
            NSLog(@"Parent view frame: %@", NSStringFromRect([parent frame]));

            [parent addSubview:child];

            // Make the child view fill the parent
            [child setFrame:[parent bounds]];
            [child setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

            NSLog(@"Child view frame after: %@", NSStringFromRect([child frame]));

            return true;
        }
    }

    bool resizeView(void* view, uint32_t width, uint32_t height) {
        @autoreleasepool {
            if (!view) {
                return false;
            }

            NSView* nsView = (__bridge NSView*)view;
            NSView* parentView = [nsView superview];

            // CRITICAL: Use parent's bounds instead of requested size to ensure we match
            // the actual window size that the DAW has already resized
            if (parentView) {
                [nsView setFrame:[parentView bounds]];
            } else {
                // Fallback if no parent (shouldn't happen in normal plugin use)
                NSRect newFrame = NSMakeRect(0, 0, width, height);
                [nsView setFrame:newFrame];
            }

            // IMPORTANT: Resize all subviews (including WKWebView) to match
            // This is necessary because autoresizing mask doesn't always work correctly
            for (NSView* subview in [nsView subviews]) {
                [subview setFrame:[nsView bounds]];
            }

            // Force layout update for the view hierarchy
            [nsView setNeedsLayout:YES];
            [nsView layoutSubtreeIfNeeded];
            [nsView setNeedsDisplay:YES];

            // Notify parent to update its layout as well
            if (parentView) {
                [[parentView superview] setNeedsLayout:YES];
            }

            return true;
        }
    }
}
#endif
