import { i18n } from './i18n'
import { sdk } from './sdk'
import { adminPort } from './utils'

export const main = sdk.setupMain(async ({ effects }) => {
  const server = await sdk.SubContainer.eager(
    effects,
    { imageId: 'bs' },
    sdk.Mounts.of().mountVolume({
      volumeId: 'main',
      subpath: null,
      mountpoint: '/data',
      readonly: false,
    }),
    'bs-sub',
  )

  await server.execFail(['chown', '1000:1000', '/data'], { user: 'root' })

  return sdk.Daemons.of(effects).addDaemon('primary', {
    subcontainer: server,
    exec: {
      command: sdk.useEntrypoint(),
      sigtermTimeout: 300_000,
    },
    ready: {
      display: i18n('Web Interface'),
      fn: () =>
        sdk.healthCheck.checkPortListening(effects, adminPort, {
          successMessage: i18n('The web interface is ready'),
          errorMessage: i18n('The web interface is not ready'),
        }),
    },
    requires: [],
  })
})
