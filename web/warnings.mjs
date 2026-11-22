// Drops rollup warnings for annotation-like text in dependency comments.
// Usage: build: { rollupOptions: { onwarn } }

export function onwarn(warning, warn)
{
    if (warning.code === 'INVALID_ANNOTATION' &&
        warning.id?.includes('node_modules'))
        return;

    warn(warning);
}
