import { StartSdk } from '@start9labs/start-sdk'
import { manifest } from './manifest'

export const sdk = StartSdk.of().withManifest(manifest).build(true)
