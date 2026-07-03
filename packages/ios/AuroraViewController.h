/* ════════════════════════════════════════════════════════════
   Aurora iOS App — ViewController
   ════════════════════════════════════════════════════════════ */

#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>

@interface AuroraViewController : UIViewController
@property (strong, nonatomic) MTKView* metalView;
@end
