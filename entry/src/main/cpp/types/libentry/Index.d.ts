export const fw2Ping: () => string;
export const fw2Lifecycle: (phase: string) => void;
/** B4-002: path to filesDir/share/codelite after Runtime Assets deploy */
export const fw2SetInstallDir: (installDir: string) => void;
/**
 * ArkTS → wx Mouse/Touch event fallback bridge.
 * action: 0=move, 1=down, 2=up, 3=cancel
 * button: 0=left, 1=right, 2=middle
 */
export const fw2DispatchPointer: (action: number, button: number, x: number, y?: number) => void;
/** ArkTS → wx wheel dispatch fallback. rotation ±120 per step. */
export const fw2DispatchWheel: (rotation: number, x?: number, y?: number) => void;
