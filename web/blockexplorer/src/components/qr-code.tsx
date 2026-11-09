import QRCode from 'react-qr-code';

export interface AddressQRCodeProps {
  value: string;
  size?: number;
}

export function AddressQRCode({ value, size = 128 }: AddressQRCodeProps) {
  if (!value.trim()) return null;
  return (
    <div className="border-accent/30 bg-surface/60 rounded-xl border p-4 shadow-sm">
      <QRCode
        value={value}
        size={size}
        style={{ height: 'auto', maxWidth: '100%', width: '100%' }}
        bgColor="transparent"
        fgColor="#ffa022"
        level="Q"
      />
    </div>
  );
}
