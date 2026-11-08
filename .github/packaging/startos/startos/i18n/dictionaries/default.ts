export const DEFAULT_LANG = 'en_US'

const dict = {
  // main.ts
  'Web Interface': 0,
  'The web interface is ready': 1,
  'The web interface is not ready': 2,
  Processor: 9,
  'This processor is not supported. Bitcoin Server requires: ${required}': 10,

  // interfaces.ts
  'Web UI': 3,
  'The management console of Bitcoin Server': 4,
  'Block Explorer': 5,
  'The block explorer of Bitcoin Server': 6,
  Peer: 7,
  'Listens for incoming connections from peers on the bitcoin network': 8,
} as const

export type I18nKey = keyof typeof dict
export type LangDict = Record<(typeof dict)[I18nKey], string>
export default dict
