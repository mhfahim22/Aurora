#include "mobile/widgets.hpp"
#include "mobile/ios.hpp"
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>

/* ════════════════════════════════════════════════════════════
   iOS UIKit renderer for mobile widgets (Phase 7)
   Walks the MwWidget tree and creates/updates UIView hierarchy
   for all 16 widget types with native UIKit controls.
   ════════════════════════════════════════════════════════════ */

static UIView* g_root_view = nil;

void aurora_ios_widgets_set_root_view(UIView* view) {
    g_root_view = view;
}

static UIColor* rgba_to_uicolor(float r, float g, float b, float a) {
    return [UIColor colorWithRed:r green:g blue:b alpha:a];
}

/* ── Tag-based widget lookup ── */
static NSMutableDictionary* g_widget_map = nil;

static int g_next_tag = 1;
static int assign_tag() {
    return g_next_tag++;
}

static UIView* find_view_by_widget(NSMutableDictionary* map, MwWidget* w) {
    return map[@((intptr_t)w)];
}

static void map_widget_to_view(NSMutableDictionary* map, MwWidget* w, UIView* v) {
    if (w && v) map[@((intptr_t)w)] = v;
}

/* ── Update an existing view from widget state ── */
static void update_view_for_widget(UIView* view, MwWidget* w) {
    if (!view || !w) return;

    view.frame = CGRectMake(w->x, w->y, w->w, w->h);
    view.hidden = !w->visible;
    view.userInteractionEnabled = w->enabled ? YES : NO;

    if (w->bg_color[3] > 0) {
        view.backgroundColor = rgba_to_uicolor(w->bg_color[0], w->bg_color[1], w->bg_color[2], w->bg_color[3]);
    } else {
        view.backgroundColor = [UIColor clearColor];
    }

    if ([view isKindOfClass:[UILabel class]]) {
        UILabel* label = (UILabel*)view;
        label.text = w->text ? [NSString stringWithUTF8String:w->text] : @"";
        label.font = [UIFont systemFontOfSize:w->font_size];
        if (w->text_color[3] > 0)
            label.textColor = rgba_to_uicolor(w->text_color[0], w->text_color[1], w->text_color[2], w->text_color[3]);
    } else if ([view isKindOfClass:[UIButton class]]) {
        UIButton* button = (UIButton*)view;
        [button setTitle:(w->text ? [NSString stringWithUTF8String:w->text] : @"") forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont systemFontOfSize:w->font_size];
        if (w->text_color[3] > 0)
            [button setTitleColor:rgba_to_uicolor(w->text_color[0], w->text_color[1], w->text_color[2], w->text_color[3]) forState:UIControlStateNormal];
    } else if ([view isKindOfClass:[UITextField class]]) {
        UITextField* tf = (UITextField*)view;
        tf.text = w->text ? [NSString stringWithUTF8String:w->text] : @"";
        tf.font = [UIFont systemFontOfSize:w->font_size];
    } else if ([view isKindOfClass:[UISlider class]]) {
        UISlider* slider = (UISlider*)view;
        slider.value = w->value;
    } else if ([view isKindOfClass:[UISwitch class]]) {
        UISwitch* sw = (UISwitch*)view;
        sw.on = (w->value > 0) ? YES : NO;
    } else if ([view isKindOfClass:[UIProgressView class]]) {
        UIProgressView* pv = (UIProgressView*)view;
        pv.progress = (w->value > 0) ? w->value / 100.0f : 0.0f;
    } else if ([view isKindOfClass:[UIImageView class]]) {
        UIImageView* iv = (UIImageView*)view;
        if (w->image_path) {
            NSString* path = [NSString stringWithUTF8String:w->image_path];
            iv.image = [UIImage imageNamed:path];
        }
    } else if ([view isKindOfClass:[UITableView class]]) {
        UITableView* tv = (UITableView*)view;
        [tv reloadData];
    }
}

/* ── Create or reuse view for widget type ── */
static UIView* create_view_for_widget(MwWidget* w) {
    if (!w) return nil;

    switch (w->type) {
        case MW_BUTTON: {
            UIButton* btn = [UIButton buttonWithType:UIButtonTypeSystem];
            btn.frame = CGRectMake(w->x, w->y, w->w, w->h);
            [btn setTitle:(w->text ? [NSString stringWithUTF8String:w->text] : @"") forState:UIControlStateNormal];
            btn.titleLabel.font = [UIFont systemFontOfSize:w->font_size];
            btn.layer.cornerRadius = 8;
            btn.clipsToBounds = YES;
            return btn;
        }

        case MW_TEXT: {
            UILabel* label = [[UILabel alloc] init];
            label.frame = CGRectMake(w->x, w->y, w->w, w->h);
            label.text = w->text ? [NSString stringWithUTF8String:w->text] : @"";
            label.font = [UIFont systemFontOfSize:w->font_size];
            label.numberOfLines = 0;
            label.lineBreakMode = NSLineBreakByWordWrapping;
            return label;
        }

        case MW_INPUT: {
            UITextField* tf = [[UITextField alloc] init];
            tf.frame = CGRectMake(w->x, w->y, w->w, w->h);
            tf.borderStyle = UITextBorderStyleRoundedRect;
            tf.font = [UIFont systemFontOfSize:w->font_size];
            tf.placeholder = @"Enter text...";
            return tf;
        }

        case MW_IMAGE: {
            UIImageView* iv = [[UIImageView alloc] init];
            iv.frame = CGRectMake(w->x, w->y, w->w, w->h);
            iv.contentMode = UIViewContentModeScaleAspectFit;
            iv.clipsToBounds = YES;
            if (w->image_path) {
                NSString* path = [NSString stringWithUTF8String:w->image_path];
                iv.image = [UIImage imageNamed:path];
            }
            return iv;
        }

        case MW_SLIDER: {
            UISlider* slider = [[UISlider alloc] init];
            slider.frame = CGRectMake(w->x, w->y, w->w, w->h);
            slider.minimumValue = 0;
            slider.maximumValue = 100;
            slider.value = w->value;
            slider.continuous = YES;
            return slider;
        }

        case MW_SWITCH: {
            UISwitch* sw = [[UISwitch alloc] init];
            sw.frame = CGRectMake(w->x, w->y, w->w, w->h);
            sw.on = (w->value > 0) ? YES : NO;
            return sw;
        }

        case MW_CHECKBOX: {
            /* Custom UIView styled as checkbox */
            UIView* cb = [[UIView alloc] init];
            cb.frame = CGRectMake(w->x, w->y, w->w, w->h);
            cb.layer.borderWidth = 1.5;
            cb.layer.borderColor = [UIColor colorWithWhite:0.5 alpha:1.0].CGColor;
            cb.layer.cornerRadius = 4;
            cb.clipsToBounds = YES;
            if (w->value > 0) {
                cb.backgroundColor = [UIColor colorWithRed:0.3 green:0.6 blue:1.0 alpha:1.0];
            }
            /* Checkmark label */
            if (w->value > 0) {
                UILabel* mark = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, w->w, w->h)];
                mark.text = @"✓";
                mark.font = [UIFont boldSystemFontOfSize:w->h * 0.7];
                mark.textColor = [UIColor whiteColor];
                mark.textAlignment = NSTextAlignmentCenter;
                [cb addSubview:mark];
            }
            return cb;
        }

        case MW_RADIO: {
            /* Custom UIView styled as radio button */
            UIView* rb = [[UIView alloc] init];
            rb.frame = CGRectMake(w->x, w->y, w->w, w->h);
            rb.layer.borderWidth = 1.5;
            rb.layer.borderColor = [UIColor colorWithWhite:0.5 alpha:1.0].CGColor;
            rb.layer.cornerRadius = w->h / 2;
            rb.clipsToBounds = YES;
            if (w->value > 0) {
                UIView* dot = [[UIView alloc] initWithFrame:CGRectMake(
                    w->w * 0.25, w->h * 0.25, w->w * 0.5, w->h * 0.5)];
                dot.backgroundColor = [UIColor colorWithRed:0.3 green:0.6 blue:1.0 alpha:1.0];
                dot.layer.cornerRadius = w->h * 0.25;
                dot.clipsToBounds = YES;
                [rb addSubview:dot];
            }
            return rb;
        }

        case MW_PROGRESS: {
            UIProgressView* pv = [[UIProgressView alloc] init];
            pv.frame = CGRectMake(w->x, w->y, w->w, w->h);
            pv.progressViewStyle = UIProgressViewStyleDefault;
            pv.progress = (w->value > 0) ? w->value / 100.0f : 0.0f;
            pv.trackTintColor = [UIColor colorWithWhite:0.8 alpha:1.0];
            pv.progressTintColor = [UIColor colorWithRed:0.3 green:0.6 blue:1.0 alpha:1.0];
            return pv;
        }

        case MW_LIST: {
            UITableView* tv = [[UITableView alloc] init];
            tv.frame = CGRectMake(w->x, w->y, w->w, w->h);
            tv.dataSource = (id<UITableViewDataSource>)g_root_view;
            /* Note: proper data source would be set by app using the list */
            return tv;
        }

        case MW_GRID: {
            /* UIView container — children positioned by layout engine */
            UIView* grid = [[UIView alloc] init];
            grid.frame = CGRectMake(w->x, w->y, w->w, w->h);
            grid.clipsToBounds = YES;
            return grid;
        }

        case MW_SCROLL: {
            UIScrollView* sv = [[UIScrollView alloc] init];
            sv.frame = CGRectMake(w->x, w->y, w->w, w->h);
            sv.clipsToBounds = YES;
            sv.scrollEnabled = YES;
            sv.showsVerticalScrollIndicator = YES;
            sv.showsHorizontalScrollIndicator = YES;
            return sv;
        }

        case MW_NAV_BAR: {
            UINavigationBar* nb = [[UINavigationBar alloc] init];
            nb.frame = CGRectMake(w->x, w->y, w->w, w->h);
            UINavigationItem* item = [[UINavigationItem alloc] init];
            item.title = w->text ? [NSString stringWithUTF8String:w->text] : @"";
            [nb setItems:@[item] animated:NO];
            return nb;
        }

        case MW_TAB_BAR: {
            UITabBar* tb = [[UITabBar alloc] init];
            tb.frame = CGRectMake(w->x, w->y, w->w, w->h);
            NSMutableArray* items = [NSMutableArray array];
            if (w->item_count > 0) {
                for (int i = 0; i < w->item_count; i++) {
                    NSString* label = w->items[i] ? [NSString stringWithUTF8String:w->items[i]] : @"Tab";
                    UITabBarItem* tabItem = [[UITabBarItem alloc] initWithTitle:label image:nil tag:i];
                    [items addObject:tabItem];
                }
            } else {
                [items addObject:[[UITabBarItem alloc] initWithTitle:@"Tab" image:nil tag:0]];
            }
            tb.items = items;
            tb.selectedItem = (w->selected_index < (int)items.count) ? items[w->selected_index] : items[0];
            return tb;
        }

        case MW_DRAWER: {
            /* Container UIView — drawer handling is done via animation */
            UIView* drawer = [[UIView alloc] init];
            drawer.frame = CGRectMake(w->x, w->y, w->w, w->h);
            drawer.clipsToBounds = YES;
            return drawer;
        }

        case MW_DIALOG: {
            /* Custom UIView as modal overlay */
            UIView* dlg = [[UIView alloc] init];
            dlg.frame = CGRectMake(w->x, w->y, w->w, w->h);
            dlg.backgroundColor = [UIColor colorWithWhite:1.0 alpha:1.0];
            dlg.layer.cornerRadius = 12;
            dlg.clipsToBounds = YES;
            dlg.layer.shadowColor = [UIColor blackColor].CGColor;
            dlg.layer.shadowOpacity = 0.3;
            dlg.layer.shadowOffset = CGSizeMake(0, 2);
            dlg.layer.shadowRadius = 8;
            /* Title label */
            if (w->text && w->text[0]) {
                UILabel* titleLabel = [[UILabel alloc] initWithFrame:CGRectMake(16, 16, w->w - 32, 24)];
                titleLabel.text = [NSString stringWithUTF8String:w->text];
                titleLabel.font = [UIFont boldSystemFontOfSize:w->font_size + 4];
                titleLabel.textColor = [UIColor blackColor];
                [dlg addSubview:titleLabel];
            }
            return dlg;
        }

        case MW_BOTTOM_SHEET: {
            UIView* sheet = [[UIView alloc] init];
            sheet.frame = CGRectMake(w->x, w->y, w->w, w->h);
            sheet.backgroundColor = [UIColor whiteColor];
            sheet.layer.cornerRadius = 16;
            sheet.clipsToBounds = YES;
            sheet.layer.maskedCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
            return sheet;
        }

        case MW_FAB: {
            UIButton* fab = [UIButton buttonWithType:UIButtonTypeCustom];
            fab.frame = CGRectMake(w->x, w->y, w->w, w->h);
            fab.backgroundColor = [UIColor colorWithRed:0.3 green:0.6 blue:1.0 alpha:1.0];
            fab.layer.cornerRadius = MIN(w->w, w->h) / 2;
            fab.clipsToBounds = YES;
            [fab setTitle:(w->text ? [NSString stringWithUTF8String:w->text] : @"+") forState:UIControlStateNormal];
            [fab setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
            fab.titleLabel.font = [UIFont boldSystemFontOfSize:24];
            return fab;
        }

        case MW_SNACKBAR: {
            UILabel* snack = [[UILabel alloc] init];
            snack.frame = CGRectMake(w->x, w->y, w->w, w->h);
            snack.text = w->text ? [NSString stringWithUTF8String:w->text] : @"";
            snack.font = [UIFont systemFontOfSize:14];
            snack.textColor = [UIColor whiteColor];
            snack.backgroundColor = [UIColor colorWithWhite:0.2 alpha:0.9];
            snack.layer.cornerRadius = w->h / 2;
            snack.clipsToBounds = YES;
            snack.textAlignment = NSTextAlignmentCenter;
            return snack;
        }

        case MW_COLUMN:
        case MW_ROW:
        default: {
            /* Transparent container */
            UIView* container = [[UIView alloc] init];
            container.frame = CGRectMake(w->x, w->y, w->w, w->h);
            container.clipsToBounds = YES;
            return container;
        }
    }
}

/* ── Touch callback for buttons ── */
@interface AuroraButtonTarget : NSObject
@property (nonatomic, assign) MwWidget* widget;
- (void)buttonTapped;
@end

@implementation AuroraButtonTarget
- (void)buttonTapped {
    if (self.widget && self.widget->callback) {
        self.widget->callback(self.widget, MW_EVENT_CLICK, nullptr);
    }
}
@end

/* ── Slider change callback ── */
@interface AuroraSliderTarget : NSObject
@property (nonatomic, assign) MwWidget* widget;
- (void)sliderChanged:(UISlider*)sender;
@end

@implementation AuroraSliderTarget
- (void)sliderChanged:(UISlider*)sender {
    if (self.widget) {
        self.widget->value = sender.value;
        if (self.widget->callback)
            self.widget->callback(self.widget, MW_EVENT_CHANGE, nullptr);
    }
}
@end

/* ── Switch change callback ── */
@interface AuroraSwitchTarget : NSObject
@property (nonatomic, assign) MwWidget* widget;
- (void)switchChanged:(UISwitch*)sender;
@end

@implementation AuroraSwitchTarget
- (void)switchChanged:(UISwitch*)sender {
    if (self.widget) {
        self.widget->value = sender.on ? 1.0f : 0.0f;
        if (self.widget->callback)
            self.widget->callback(self.widget, MW_EVENT_CHANGE, nullptr);
    }
}
@end

/* ── Text field delegate ── */
@interface AuroraTextFieldDelegate : NSObject <UITextFieldDelegate>
@property (nonatomic, assign) MwWidget* widget;
@end

@implementation AuroraTextFieldDelegate
- (void)textFieldDidEndEditing:(UITextField*)textField {
    if (self.widget) {
        if (self.widget->text) free(self.widget->text);
        self.widget->text = strdup([textField.text UTF8String]);
        if (self.widget->callback)
            self.widget->callback(self.widget, MW_EVENT_CHANGE, nullptr);
    }
}
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
    [textField resignFirstResponder];
    if (self.widget && self.widget->callback)
        self.widget->callback(self.widget, MW_EVENT_SUBMIT, nullptr);
    return YES;
}
@end

static NSMutableDictionary* g_button_targets = nil;
static NSMutableDictionary* g_slider_targets = nil;
static NSMutableDictionary* g_switch_targets = nil;
static NSMutableDictionary* g_textfield_delegates = nil;

void aurora_ios_widgets_render_tree(void* root) {
    if (!root || !g_root_view) return;
    MwWidget* root_w = (MwWidget*)root;

    /* Initialize maps on first call */
    if (!g_widget_map) {
        g_widget_map = [NSMutableDictionary dictionary];
        g_button_targets = [NSMutableDictionary dictionary];
        g_slider_targets = [NSMutableDictionary dictionary];
        g_switch_targets = [NSMutableDictionary dictionary];
        g_textfield_delegates = [NSMutableDictionary dictionary];
    }

    /* Walk widget tree and create/update views */
    __block void (^walk_tree)(MwWidget*, UIView*) = ^(MwWidget* w, UIView* parent) {
        if (!w) return;

        UIView* view = find_view_by_widget(g_widget_map, w);
        BOOL is_new = (view == nil);

        if (is_new) {
            view = create_view_for_widget(w);
            if (!view) return;
            map_widget_to_view(g_widget_map, w, view);
            [parent addSubview:view];
        }

        update_view_for_widget(view, w);

        /* Attach event handlers for interactive widgets (only on creation) */
        if (is_new) {
            switch (w->type) {
                case MW_BUTTON:
                case MW_FAB: {
                    if ([view isKindOfClass:[UIButton class]]) {
                        AuroraButtonTarget* target = [[AuroraButtonTarget alloc] init];
                        target.widget = w;
                        g_button_targets[@((intptr_t)w)] = target;
                        [(UIButton*)view addTarget:target action:@selector(buttonTapped)
                            forControlEvents:UIControlEventTouchUpInside];
                    }
                    break;
                }
                case MW_SLIDER: {
                    if ([view isKindOfClass:[UISlider class]]) {
                        AuroraSliderTarget* target = [[AuroraSliderTarget alloc] init];
                        target.widget = w;
                        g_slider_targets[@((intptr_t)w)] = target;
                        [(UISlider*)view addTarget:target action:@selector(sliderChanged:)
                            forControlEvents:UIControlEventValueChanged];
                    }
                    break;
                }
                case MW_SWITCH: {
                    if ([view isKindOfClass:[UISwitch class]]) {
                        AuroraSwitchTarget* target = [[AuroraSwitchTarget alloc] init];
                        target.widget = w;
                        g_switch_targets[@((intptr_t)w)] = target;
                        [(UISwitch*)view addTarget:target action:@selector(switchChanged:)
                            forControlEvents:UIControlEventValueChanged];
                    }
                    break;
                }
                case MW_INPUT: {
                    if ([view isKindOfClass:[UITextField class]]) {
                        AuroraTextFieldDelegate* del = [[AuroraTextFieldDelegate alloc] init];
                        del.widget = w;
                        g_textfield_delegates[@((intptr_t)w)] = del;
                        ((UITextField*)view).delegate = del;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        /* Update scroll view content size */
        if ([view isKindOfClass:[UIScrollView class]] && w->type == MW_SCROLL) {
            UIScrollView* sv = (UIScrollView*)view;
            CGFloat content_h = 0;
            for (int i = 0; i < w->child_count; i++) {
                if (w->children[i]->visible)
                    content_h = MAX(content_h, w->children[i]->y + w->children[i]->h);
            }
            sv.contentSize = CGSizeMake(w->w, content_h + 20);
            sv.contentOffset = CGPointMake(w->scroll_x, w->scroll_y);
        }

        /* Recursively render children */
        for (int i = 0; i < w->child_count; i++)
            walk_tree(w->children[i], view);
    };

    walk_tree(root_w, g_root_view);
}

void aurora_ios_widgets_render(void* widget) {
    if (widget)
        aurora_ios_widgets_render_tree(widget);
}
