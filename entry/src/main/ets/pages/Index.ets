import { hilog } from '@kit.PerformanceAnalysisKit';
import { common } from '@kit.AbilityKit';
import nativeFw2 from 'libentry.so';
import { installFilePickerBridge } from '../common/FilePickerBridge';

const DOMAIN = 0xF002;
const TAG = 'FW2Host';
const XCOMPONENT_ID = 'fw2_host_xcomponent';

const POINTER_ACTION_MOVE: number = 0;
const POINTER_ACTION_DOWN: number = 1;
const POINTER_ACTION_UP: number = 2;
const POINTER_ACTION_CANCEL: number = 3;

@Entry
@Component
struct Index {
  private xController: XComponentController = new XComponentController();

  private sentDownAt: number = -1;
  private lastX: number = 0;
  private lastY: number = 0;

  aboutToAppear(): void {
    hilog.info(DOMAIN, TAG, 'Index — surface host for official CodeLite MainFrame');
    try {
      const ping: string = nativeFw2.fw2Ping();
      hilog.info(DOMAIN, TAG, 'native: %{public}s', ping);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'native load failed: %{public}s', JSON.stringify(e));
    }
    try {
      const ctx = getContext(this) as common.UIAbilityContext;
      installFilePickerBridge(ctx);
    } catch (e) {
      hilog.error(DOMAIN, TAG, '[F-1.4] installFilePickerBridge failed: %{public}s', JSON.stringify(e));
    }
    // Capture `this` in a local closure so setTimeout callbacks are valid even
    // if the ArkTS strict-mode wrapper does not rebind the callback receiver.
    //
    // SWEEP across the entire menubar region: last run showed (60,14) resolved
    // to wxPanel instead of wxMenuBar, so we sweep 5 x-positions × 4 y-positions
    // + two extra right-aligned clicks for Help/Harmony. This guarantees we land
    // at least one synthetic click inside the real wxMenuBar rect no matter what
    // the MENUBAR-GEOM probe reports, giving us an I-5 popup-opened trace.
    const self: Index = this;
    const xs: number[] = [40, 60, 80, 100, 120];
    const ys: number[] = [14, 20, 26, 32];
    const extras: Array<Array<number>> = [[700, 20], [780, 20]];
    let delayMs: number = 2000;
    let idx: number = 0;
    const scheduleOne = (label: string, cx: number, cy: number): void => {
      const tag: string = label;
      const cd: number = delayMs;
      setTimeout(() => {
        hilog.info(DOMAIN, TAG, '[AUTO-%{public}s] t+%{public}sms click (%{public}s,%{public}s)',
                   tag, cd.toString(), cx.toString(), cy.toString());
        try { self.syntheticClick(cx, cy); } catch (ecl) {
          hilog.error(DOMAIN, TAG, '[AUTO-%{public}s] FAIL: %{public}s',
                      tag, JSON.stringify(ecl));
        }
      }, cd);
    };
    for (let xi: number = 0; xi < xs.length; xi++) {
      for (let yi: number = 0; yi < ys.length; yi++) {
        const cx2: number = xs[xi];
        const cy2: number = ys[yi];
        const lab: string = 'F' + idx.toString();
        scheduleOne(lab, cx2, cy2);
        delayMs += 180;
        idx++;
      }
    }
    for (let ei: number = 0; ei < extras.length; ei++) {
      const cx3: number = extras[ei][0];
      const cy3: number = extras[ei][1];
      const lab2: string = (ei === 0 ? 'Help' : 'Harm');
      scheduleOne(lab2, cx3, cy3);
      delayMs += 250;
    }
    // One last File sweep at y=40 just in case menubar rect dips that far.
    scheduleOne('Fbottom', 70, 40);
  }

  private forwardPointer(action: number, button: number, x: number, y: number): void {
    const tag: string =
      action === POINTER_ACTION_MOVE ? 'MOVE' :
      action === POINTER_ACTION_DOWN ? 'DOWN' :
      action === POINTER_ACTION_UP   ? 'UP'   : 'CANCEL';
    const xs: string = Math.round(x).toString();
    const ys: string = Math.round(y).toString();
    hilog.info(DOMAIN, TAG, '[I-0.ETS] %{public}s b=%{public}d (%{public}s,%{public}s)', tag, button, xs, ys);
    nativeFw2.fw2DispatchPointer(action, button, x, y);
  }

  private localXY(event: GestureEvent): number[] {
    let x: number = 0;
    let y: number = 0;
    if (event.fingerList && event.fingerList.length > 0) {
      x = event.fingerList[0].localX;
      y = event.fingerList[0].localY;
    }
    return [x, y];
  }

  private syntheticClick(x: number, y: number): void {
    this.lastX = x;
    this.lastY = y;
    const now: number = Date.now();
    if (now - this.sentDownAt < 200) {
      this.forwardPointer(POINTER_ACTION_UP, 0, x, y);
      this.sentDownAt = -1;
      return;
    }
    this.forwardPointer(POINTER_ACTION_DOWN, 0, x, y);
    this.sentDownAt = now;
    setTimeout(() => {
      this.forwardPointer(POINTER_ACTION_UP, 0, x, y);
      this.sentDownAt = -1;
    }, 10);
  }

  build() {
    Stack() {
      XComponent({
        id: XCOMPONENT_ID,
        type: XComponentType.SURFACE,
        libraryname: 'entry',
        controller: this.xController
      })
        .width('100%')
        .height('100%')
        .focusable(true)
        .focusOnTouch(true)
        .enabled(true)
        .defaultFocus(true)
        .hitTestBehavior(HitTestMode.Default)
        .onLoad(() => {
          hilog.info(DOMAIN, TAG, 'XComponent onLoad (CodeLite MainFrame surface)');
        })
        .onDestroy(() => {
          hilog.info(DOMAIN, TAG, 'XComponent onDestroy');
        });

      // Core architecture for 6.1.1 PC ArkUI input (lessons learned):
      //   * onMouse / onTouch / onClick on ANY control = do NOT fire on PC.
      //   * Two separate controls with gesture() = two independent coords.
      //   * A control fully transparent (alpha == 0 / no pixels) = ArkUI
      //     won't actually run gesture recognisers against it.
      // So:
      //   * Use a SINGLE control covering the WHOLE surface so fingerList
      //     localX/localY map 1:1 to wx backing store coords.
      //   * Paint a real pixel — backgroundColor with alpha >= 0.01. The
      //     wx XComponent sits beneath this control in the Stack, and
      //     XComponent content is a separate NativeWindow surface (not an
      //     ArkUI paint pass) so the native pixels are still visible.
      //   * Bind TapGesture + PanGesture separately via chained .gesture()
      //     calls (6.1.1 GestureGroup API does not compile).
      // Single full-surface interceptor. One real (non-transparent-pixel)
      // ArkUI control = gesture recognisers actually run on PC 6.1.1.
      // fingerList local coordinates match 1:1 with the XComponent
      // underlying area, so the x/y we forward are correct for wx hit-test.
      Text('CodeLite FW2 Host — input interceptor (official CodeLite UI is rendered underneath, do not hide this)')
        .width('100%')
        .height('100%')
        .fontSize(0)
        .textAlign(TextAlign.Start)
        .backgroundColor('rgba(10,10,10,0.025)')
        .focusable(true)
        .defaultFocus(true)
        .focusOnTouch(true)
        .hitTestBehavior(HitTestMode.Default)
        .gesture(
          TapGesture({ count: 1, fingers: 1 })
            .onAction((event: GestureEvent) => {
              const p: number[] = this.localXY(event);
              hilog.info(DOMAIN, TAG, '[I-0.TAP]');
              this.syntheticClick(p[0], p[1]);
            })
        )
        .gesture(
          PanGesture({ fingers: 1, direction: PanDirection.All, distance: 0 })
            .onActionStart((event: GestureEvent) => {
              const p: number[] = this.localXY(event);
              const x: number = p[0];
              const y: number = p[1];
              const now: number = Date.now();
              if (now - this.sentDownAt < 200) {
                hilog.info(DOMAIN, TAG, '[I-0.PAN-START] dedup');
                this.lastX = x;
                this.lastY = y;
                return;
              }
              this.lastX = x;
              this.lastY = y;
              this.forwardPointer(POINTER_ACTION_DOWN, 0, x, y);
              this.sentDownAt = now;
            })
            .onActionUpdate((event: GestureEvent) => {
              const p: number[] = this.localXY(event);
              const x: number = p[0];
              const y: number = p[1];
              this.forwardPointer(POINTER_ACTION_MOVE, 0, x, y);
              this.lastX = x;
              this.lastY = y;
            })
            .onActionEnd(() => {
              hilog.info(DOMAIN, TAG, '[I-0.PAN-END]');
              if (this.sentDownAt > 0) {
                this.forwardPointer(POINTER_ACTION_UP, 0, this.lastX, this.lastY);
                this.sentDownAt = -1;
              }
            })
            .onActionCancel(() => {
              hilog.info(DOMAIN, TAG, '[I-0.PAN-CANCEL]');
              if (this.sentDownAt > 0) {
                this.forwardPointer(POINTER_ACTION_CANCEL, 0, this.lastX, this.lastY);
                this.sentDownAt = -1;
              }
            })
        );
    }
    .width('100%')
    .height('100%');
  }
}
