// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2019.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#import "GameViewController.h"

#import "Renderer.h"
#import "VantageView.h"

@implementation GameViewController {
    VantageView * _view;

    Renderer * _renderer;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _view = (VantageView *)self.view;

    [_view allowDragAndDrop];

    _view.device = MTLCreateSystemDefaultDevice();

    if (!_view.device) {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
        return;
    }

    _renderer = [[Renderer alloc] initWithMetalKitView:_view];
    _view.renderer = _renderer;

    [_renderer mtkView:_view drawableSizeWillChange:_view.bounds.size];

    _view.delegate = _renderer;
}

@end
