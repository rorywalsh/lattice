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
} 
#endif
