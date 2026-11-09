export async function copyTextToClipboard(text: string): Promise<boolean> {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  const document_ = (globalThis as unknown as { document?: Document }).document;
  if (!window_ || !document_) return false;

  if (window_.isSecureContext) {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch {
      // Fall back to execCommand below.
    }
  }

  const textarea = document_.createElement('textarea');
  textarea.value = text;
  textarea.setAttribute('readonly', '');
  textarea.style.position = 'fixed';
  textarea.style.top = '0';
  textarea.style.left = '0';
  textarea.style.opacity = '0';

  document_.body.append(textarea);
  textarea.select();
  textarea.setSelectionRange(0, textarea.value.length);

  let succeeded = false;
  try {
    // eslint-disable-next-line @typescript-eslint/no-deprecated -- Legacy fallback for older browsers.
    succeeded = document_.execCommand('copy');
  } catch {
    succeeded = false;
  } finally {
    textarea.remove();
  }

  return succeeded;
}
