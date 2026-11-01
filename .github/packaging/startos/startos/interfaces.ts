import { i18n } from './i18n'
import { sdk } from './sdk'
import { adminPort, explorerPort, peerPort } from './utils'

export const setInterfaces = sdk.setupInterfaces(async ({ effects }) => {
  const uiMulti = sdk.MultiHost.of(effects, 'ui-multi')
  const uiMultiOrigin = await uiMulti.bindPort(adminPort, {
    protocol: 'http',
  })
  const ui = sdk.createInterface(effects, {
    name: i18n('Web UI'),
    id: 'ui',
    description: i18n('The management console of Bitcoin Server'),
    type: 'ui',
    masked: false,
    schemeOverride: null,
    username: null,
    path: '',
    query: {},
  })
  const uiReceipt = await uiMultiOrigin.export([ui])

  const explorerMulti = sdk.MultiHost.of(effects, 'explorer-multi')
  const explorerMultiOrigin = await explorerMulti.bindPort(explorerPort, {
    protocol: 'http',
  })
  const explorer = sdk.createInterface(effects, {
    name: i18n('Block Explorer'),
    id: 'explorer',
    description: i18n('The block explorer of Bitcoin Server'),
    type: 'ui',
    masked: false,
    schemeOverride: null,
    username: null,
    path: '',
    query: {},
  })
  const explorerReceipt = await explorerMultiOrigin.export([explorer])

  const peerMulti = sdk.MultiHost.of(effects, 'peer-multi')
  const peerMultiOrigin = await peerMulti.bindPort(peerPort, {
    protocol: null,
    preferredExternalPort: peerPort,
    addSsl: null,
    secure: { ssl: false },
  })
  const peer = sdk.createInterface(effects, {
    name: i18n('Peer'),
    id: 'peer',
    description: i18n(
      'Listens for incoming connections from peers on the bitcoin network',
    ),
    type: 'p2p',
    masked: false,
    schemeOverride: { ssl: null, noSsl: null },
    username: null,
    path: '',
    query: {},
  })
  const peerReceipt = await peerMultiOrigin.export([peer])

  return [uiReceipt, explorerReceipt, peerReceipt]
})
