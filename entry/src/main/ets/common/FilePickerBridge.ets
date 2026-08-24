import { hilog } from '@kit.PerformanceAnalysisKit';
import { fileIo as fs, picker, fileUri } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import nativeFw2 from 'libentry.so';

const DOMAIN = 0xF002;
const TAG = 'FW2Host';

function basenameFromUri(uri: string): string {
  try {
    return new fileUri.FileUri(uri).name;
  } catch (_e) {
    const parts: string[] = uri.split('/');
    return parts[parts.length - 1] || 'picked.bin';
  }
}

function resolvePickerPath(context: common.UIAbilityContext, uri: string): string {
  if (!uri) {
    return '';
  }

  const candidates: string[] = [];
  if (uri.startsWith('file://')) {
    const stripped: string = decodeURIComponent(uri.replace(/^file:\/\//, ''));
    candidates.push(stripped);
    if (!stripped.startsWith('/')) {
      candidates.push(`/${stripped}`);
    }
    if (stripped.startsWith('docs/storage/')) {
      candidates.push(`/${stripped.replace(/^docs\//, '')}`);
    }
  } else {
    candidates.push(uri);
  }

  const directRoots: string[] = ['/storage/', '/data/'];
  for (let i = 0; i < candidates.length; i++) {
    const candidate: string = candidates[i];
    if (!candidate.startsWith('/')) {
      continue;
    }
    if (!directRoots.some((root: string) => candidate.startsWith(root))) {
      continue;
    }
    try {
      fs.accessSync(candidate);
      return candidate;
    } catch (_e) {
      // try next candidate
    }
  }

  const pickDir: string = `${context.cacheDir}/picked`;
  try {
    fs.mkdirSync(pickDir, true);
  } catch (_e) {
    // exists
  }
  const dest: string = `${pickDir}/${basenameFromUri(uri)}`;
  try {
    fs.copyFileSync(uri, dest);
    hilog.info(DOMAIN, TAG, '[F-1.4] copyToSandbox OK dest=%{public}s', dest);
    return dest;
  } catch (e) {
    hilog.error(DOMAIN, TAG, '[F-1.4] copyToSandbox failed: %{public}s', JSON.stringify(e));
  }

  return candidates.length > 0 ? candidates[0] : uri;
}

export function installFilePickerBridge(context: common.UIAbilityContext): void {
  nativeFw2.fw2InstallFilePicker(async (): Promise<void> => {
    hilog.info(DOMAIN, TAG, '[F-1.4] picker opened');
    try {
      const documentPicker = new picker.DocumentViewPicker(context);
      const options = new picker.DocumentSelectOptions();
      options.maxSelectNumber = 1;
      const uris: Array<string> = await documentPicker.select(options);
      if (!uris || uris.length === 0) {
        nativeFw2.fw2PickerDeliver('', '');
        return;
      }
      const uri: string = uris[0];
      const path: string = resolvePickerPath(context, uri);
      hilog.info(DOMAIN, TAG, '[F-1.4] uri=%{public}s', uri);
      hilog.info(DOMAIN, TAG, '[F-1.4] path=%{public}s', path);
      nativeFw2.fw2PickerDeliver(uri, path);
    } catch (e) {
      hilog.error(DOMAIN, TAG, '[F-1.4] picker failed: %{public}s', JSON.stringify(e));
      nativeFw2.fw2PickerDeliver('', '');
    }
  });
  hilog.info(DOMAIN, TAG, '[F-1.4] picker bridge installed from Index');
}
