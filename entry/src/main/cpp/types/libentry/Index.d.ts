export const loadmodel: () => string;
export const modelinfer: (quenstion: string) => void;
export const answerget: (callback: (token: string) => void) =>void;
export const deinitmodel: () => void;