export function getNativeFunction(name: string): (...args: any[]) => Promise<any>;
export function getSliderState(name: string): any;
export function getToggleState(name: string): any;
export function getComboBoxState(name: string): any;
export function getBackendResourceAddress(path: string): string;
export class ControlParameterIndexUpdater {
    constructor(controlParameterIndexAnnotation: string);
    handleMouseMove(event: MouseEvent): void;
}