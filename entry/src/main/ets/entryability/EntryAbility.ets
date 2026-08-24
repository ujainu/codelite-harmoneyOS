import { AbilityConstant, common, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { fileIo as fs } from '@kit.CoreFileKit';
import { resourceManager } from '@kit.LocalizationKit';
import nativeFw2 from 'libentry.so';

const DOMAIN = 0xF002;
const TAG = 'FW2Host';
const RAW_ROOT = 'share/codelite';

/**
 * B4-002 Runtime Assets Deployment (Host only).
 * Copy HAP rawfile/share/codelite → filesDir/share/codelite (official layout).
 * Does not touch Resource Loader / XML / PluginManager.
 */
async function deployRuntimeAssetsWithFallback(context: common.UIAbilityContext,
  windowStage: window.WindowStage): Promise<void> {
  let installDir: string = '';
  try {
    installDir = await deployRuntimeAssets(context);
    try {
      nativeFw2.fw2SetInstallDir(installDir);
      hilog.info(DOMAIN, TAG, '[B-4.3] fw2SetInstallDir %{public}s', installDir);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'fw2SetInstallDir failed: %{public}s', String(e));
    }
  } catch (e) {
    hilog.error(DOMAIN, TAG, '[B-4.3] FAIL Runtime Assets deploy: %{public}s', String(e));
  }
  windowStage.loadContent('pages/Index', (err) => {
    if (err.code) {
      hilog.error(DOMAIN, TAG, 'loadContent failed: %{public}s', JSON.stringify(err));
      return;
    }
    hilog.info(DOMAIN, TAG, 'loadContent OK');
  });
}

async function deployRuntimeAssets(context: common.UIAbilityContext): Promise<string> {
  const destRoot: string = `${context.filesDir}/${RAW_ROOT}`;
  const menuPath: string = `${destRoot}/rc/menu.xrc`;
  try {
    if (fs.accessSync(menuPath)) {
      hilog.info(DOMAIN, TAG, '[B-4.3] Runtime Assets already present: %{public}s', menuPath);
      return destRoot;
    }
  } catch (_e) {
    // missing — deploy
  }

  hilog.info(DOMAIN, TAG, '[B-4.3] destRoot=%{public}s', destRoot);
  const rm: resourceManager.ResourceManager = context.resourceManager;

  // Diagnostic: test getRawFileList at various levels
  try {
    const topNames = await rm.getRawFileList('');
    hilog.info(DOMAIN, TAG, '[B-4.3] rawfile root list: %{public}s', JSON.stringify(topNames));
  } catch (e1) {
    hilog.error(DOMAIN, TAG, '[B-4.3] getRawFileList("") FAIL: %{public}s', String(e1));
  }
  try {
    const shareNames = await rm.getRawFileList('share');
    hilog.info(DOMAIN, TAG, '[B-4.3] rawfile share/ list: %{public}s', JSON.stringify(shareNames));
  } catch (e2) {
    hilog.error(DOMAIN, TAG, '[B-4.3] getRawFileList("share") FAIL: %{public}s', String(e2));
  }
  try {
    const codeliteNames = await rm.getRawFileList(RAW_ROOT);
    hilog.info(DOMAIN, TAG, '[B-4.3] rawfile %{public}s list: %{public}s', RAW_ROOT, JSON.stringify(codeliteNames));
  } catch (e3) {
    hilog.error(DOMAIN, TAG, '[B-4.3] getRawFileList("%{public}s") FAIL: %{public}s', RAW_ROOT, String(e3));
  }

  await copyRawTree(rm, RAW_ROOT, destRoot);

  if (!fs.accessSync(menuPath)) {
    throw new Error(`B4-002 deploy failed: missing ${menuPath}`);
  }
  hilog.info(DOMAIN, TAG, '[B-4.3] Runtime Assets deployed menu.xrc OK → %{public}s', destRoot);
  return destRoot;
}

async function copyRawTree(rm: resourceManager.ResourceManager, rawPath: string, destPath: string): Promise<void> {
  try {
    fs.mkdirSync(destPath, true);
  } catch (_e) {
    // exists
  }

  let names: string[] = [];
  try {
    names = await rm.getRawFileList(rawPath);
    hilog.info(DOMAIN, TAG, '[B-4.3] copyRawTree %{public}s → %{public}s, items=%{public}s',
      rawPath, destPath, JSON.stringify(names));
  } catch (e) {
    hilog.info(DOMAIN, TAG, '[B-4.3] getRawFileList(%{public}s) threw, trying as file: %{public}s',
      rawPath, String(e));
    try {
      const data = await rm.getRawFileContent(rawPath);
      const file = fs.openSync(destPath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
      fs.writeSync(file.fd, data.buffer);
      fs.closeSync(file);
      hilog.info(DOMAIN, TAG, '[B-4.3] copied file %{public}s (%{public}s bytes)', rawPath, data.byteLength);
    } catch (e2) {
      hilog.error(DOMAIN, TAG, '[B-4.3] getRawFileContent(%{public}s) FAIL: %{public}s',
        rawPath, String(e2));
    }
    return;
  }

  if (names.length === 0) {
    hilog.warn(DOMAIN, TAG, '[B-4.3] getRawFileList(%{public}s) returned EMPTY', rawPath);
    return;
  }

  for (let i = 0; i < names.length; i++) {
    const name: string = names[i];
    const childRaw: string = `${rawPath}/${name}`;
    const childDest: string = `${destPath}/${name}`;
    try {
      await rm.getRawFileList(childRaw);
      await copyRawTree(rm, childRaw, childDest);
    } catch (_isFile) {
      try {
        const data = await rm.getRawFileContent(childRaw);
        const file = fs.openSync(childDest, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, data.buffer);
        fs.closeSync(file);
      } catch (e3) {
        hilog.error(DOMAIN, TAG, '[B-4.3] copy file %{public}s FAIL: %{public}s',
          childRaw, String(e3));
      }
    }
  }
}

export default class EntryAbility extends UIAbility {
  onCreate(_want: Want, _launchParam: AbilityConstant.LaunchParam): void {
    // Hardcoded probe — fires first thing during ability init, BEFORE any
    // native library load or JS try/catch. If we don't see this in hilog the
    // process never actually reached the ArkTS runtime for this ability at
    // all (packaging/install mismatch or crash pre-ArkTS init).
    hilog.warn(DOMAIN, TAG, '[ENTRY-HEARTBEAT-1] EntryAbility.onCreate START (bundle=codelite.fw2.host)');
    hilog.info(DOMAIN, TAG, 'Ability onCreate');
    hilog.warn(DOMAIN, TAG, '[ENTRY-HEARTBEAT-1] EntryAbility.onCreate END');
  }

  onDestroy(): void {
    hilog.info(DOMAIN, TAG, 'Ability onDestroy');
    try {
      nativeFw2.fw2Lifecycle('destroy');
    } catch (_e) {
      hilog.warn(DOMAIN, TAG, 'fw2Lifecycle destroy skipped');
    }
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, TAG, 'onWindowStageCreate');
    this.context.filesDir; // touch filesDir to ensure it exists
    // Deploy before XComponent → EmbeddedStart (avoids race with B-4.3).
    deployRuntimeAssetsWithFallback(this.context, windowStage);
  }

  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, TAG, 'onWindowStageDestroy');
    try {
      nativeFw2.fw2Lifecycle('destroy');
    } catch (_e) {
      hilog.warn(DOMAIN, TAG, 'fw2Lifecycle destroy skipped');
    }
  }

  onForeground(): void {
    hilog.info(DOMAIN, TAG, 'onForeground');
  }

  onBackground(): void {
    hilog.info(DOMAIN, TAG, 'onBackground');
  }
}
