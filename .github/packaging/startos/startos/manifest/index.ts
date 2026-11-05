import { setupManifest } from '@start9labs/start-sdk'
import { long, short } from './i18n'

export const manifest = setupManifest({
  id: 'bitcoin-server',
  title: 'Bitcoin Server',
  license: 'AGPL-3.0',
  packageRepo: 'https://github.com/libbitcoin/libbitcoin-server',
  upstreamRepo: 'https://github.com/libbitcoin/libbitcoin-server',
  marketingUrl: 'https://libbitcoin.org',
  donationUrl: null,
  description: { short, long },
  volumes: ['main'],
  images: {
    bs: {
      source: { dockerTag: '@IMAGE@' },
      arch: ['x86_64', 'aarch64'],
      emulateMissing: false,
    },
  },
  hardwareRequirements: {
    ram: 15 * 1024 ** 3,
  },
})
