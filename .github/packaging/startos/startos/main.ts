import { i18n } from './i18n'
import { sdk } from './sdk'
import { adminPort } from './utils'

const entrypoint = '/opt/libbitcoin-server/entrypoint'

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

  const probe = await server.exec([entrypoint, '--version'])
  const stderr = String(probe.stderr)
  if (probe.exitCode !== 0 && stderr.includes('no packaged variant')) {
    const required =
      /^libbitcoin-server: required: (.*)$/m.exec(stderr)?.[1] ?? ''
    console.error(stderr.trim())
    return sdk.Daemons.of(effects).addDaemon('primary', {
      subcontainer: server,
      exec: { command: ['sleep', 'infinity'] },
      ready: {
        display: i18n('Processor'),
        gracePeriod: 0,
        trigger: sdk.trigger.onceTrigger,
        fn: () => ({
          result: 'failure' as const,
          message: i18n(
            'This processor is not supported. Bitcoin Server requires: ${required}',
            { required },
          ),
        }),
      },
      requires: [],
    })
  }

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
