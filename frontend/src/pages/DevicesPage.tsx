import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { spoolbuddyApi, type SpoolBuddyDevice } from '../api/client';
import { useToast } from '../contexts/ToastContext';

const DEVICE_TYPE_LABELS: Record<string, string> = {
  'spoolbuddy': 'SpoolBuddy',
  'printer-panel': 'Printer Panel',
  'clear-plate': 'Clear Plate',
};

const DEVICE_TYPE_ICONS: Record<string, string> = {
  'spoolbuddy': '🏷️',
  'printer-panel': '🖥️',
  'clear-plate': '🧹',
};

function DeviceStatusBadge({ device }: { device: SpoolBuddyDevice }) {
  const isOnline = device.online;
  const lastSeen = device.last_seen ? new Date(device.last_seen) : null;
  const ago = lastSeen ? Math.floor((Date.now() - lastSeen.getTime()) / 1000) : null;

  let statusText = 'Unknown';
  let statusColor = 'bg-gray-400';

  if (isOnline) {
    statusText = 'Online';
    statusColor = 'bg-green-500';
  } else if (ago !== null) {
    if (ago < 60) statusText = `${ago}s ago`;
    else if (ago < 3600) statusText = `${Math.floor(ago / 60)}m ago`;
    else if (ago < 86400) statusText = `${Math.floor(ago / 3600)}h ago`;
    else statusText = `${Math.floor(ago / 86400)}d ago`;
    statusColor = 'bg-red-400';
  }

  return (
    <span className="inline-flex items-center gap-1.5">
      <span className={`inline-block w-2 h-2 rounded-full ${statusColor}`} />
      <span className="text-xs text-gray-500 dark:text-gray-400">{statusText}</span>
    </span>
  );
}

function OtaBadge({ device }: { device: SpoolBuddyDevice }) {
  if (!device.ota_status || device.ota_status === 'current') return null;

  const colors: Record<string, string> = {
    pending: 'bg-yellow-100 text-yellow-800 dark:bg-yellow-900/30 dark:text-yellow-300',
    downloading: 'bg-blue-100 text-blue-800 dark:bg-blue-900/30 dark:text-blue-300',
    failed: 'bg-red-100 text-red-800 dark:bg-red-900/30 dark:text-red-300',
  };

  return (
    <span className={`text-xs px-1.5 py-0.5 rounded ${colors[device.ota_status] || 'bg-gray-100'}`}>
      OTA: {device.ota_status}
    </span>
  );
}

function DeviceCard({ device, printerName, onDelete }: {
  device: SpoolBuddyDevice;
  printerName?: string;
  onDelete: (id: string) => void;
}) {
  const [showDetails, setShowDetails] = useState(false);
  const typeLabel = DEVICE_TYPE_LABELS[device.device_type] || device.device_type;
  const typeIcon = DEVICE_TYPE_ICONS[device.device_type] || '📟';

  return (
    <div className="bg-white dark:bg-gray-800 rounded-lg border border-gray-200 dark:border-gray-700 p-4 hover:shadow-sm transition-shadow">
      {/* Header */}
      <div className="flex items-start justify-between">
        <div className="flex items-center gap-2">
          <span className="text-xl">{typeIcon}</span>
          <div>
            <h3 className="font-medium text-gray-900 dark:text-gray-100">
              {device.friendly_name || device.device_id}
            </h3>
            <p className="text-xs text-gray-500 dark:text-gray-400">{typeLabel}</p>
          </div>
        </div>
        <DeviceStatusBadge device={device} />
      </div>

      {/* Info row */}
      <div className="mt-3 flex flex-wrap gap-x-4 gap-y-1 text-sm text-gray-600 dark:text-gray-300">
        {device.firmware_version && (
          <span>FW: {device.firmware_version}</span>
        )}
        {device.printer_id !== null && (
          <span>Printer: {printerName || `#${device.printer_id}`}</span>
        )}
        {device.ip_address && (
          <span className="text-xs text-gray-400">{device.ip_address}</span>
        )}
        <OtaBadge device={device} />
      </div>

      {/* Expandable details */}
      <div className="mt-2">
        <button
          onClick={() => setShowDetails(!showDetails)}
          className="text-xs text-blue-600 dark:text-blue-400 hover:underline"
        >
          {showDetails ? 'Hide details' : 'Details'}
        </button>

        {showDetails && (
          <div className="mt-2 text-xs space-y-1 text-gray-500 dark:text-gray-400 border-t border-gray-100 dark:border-gray-700 pt-2">
            <p>Hostname: {device.hostname}</p>
            <p>Uptime: {formatUptime(device.uptime_s)}</p>
            {device.location && <p>Location: {device.location}</p>}
            {device.device_type === 'spoolbuddy' && (
              <>
                <p>NFC: {device.nfc_ok ? '✅' : '❌'} | Scale: {device.scale_ok ? '✅' : '❌'}</p>
              </>
            )}
            {device.system_stats?.cpu_temp_c && (
              <p>CPU Temp: {device.system_stats.cpu_temp_c.toFixed(1)}°C</p>
            )}
            {device.created_at && (
              <p>Registered: {new Date(device.created_at).toLocaleDateString()}</p>
            )}
            <div className="pt-1">
              <button
                onClick={() => onDelete(device.device_id)}
                className="text-red-600 dark:text-red-400 hover:underline"
              >
                Remove device
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

function formatUptime(seconds: number): string {
  if (seconds < 60) return `${seconds}s`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m`;
  if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ${Math.floor((seconds % 3600) / 60)}m`;
  return `${Math.floor(seconds / 86400)}d ${Math.floor((seconds % 86400) / 3600)}h`;
}

export function DevicesPage() {
  const { showToast } = useToast();
  const queryClient = useQueryClient();
  const [typeFilter, setTypeFilter] = useState<string>('all');

  const { data: devices = [], isLoading } = useQuery({
    queryKey: ['devices'],
    queryFn: () => spoolbuddyApi.getDevices(),
    refetchInterval: 10000,
  });

  const deleteMutation = useMutation({
    mutationFn: (deviceId: string) => spoolbuddyApi.deleteDevice(deviceId),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['devices'] });
      showToast('Device removed', 'success');
    },
    onError: () => {
      showToast('Failed to remove device', 'error');
    },
  });

  const handleDelete = (deviceId: string) => {
    if (confirm(`Remove device ${deviceId}? This cannot be undone.`)) {
      deleteMutation.mutate(deviceId);
    }
  };

  // Group by type
  const filteredDevices = typeFilter === 'all'
    ? devices
    : devices.filter((d) => d.device_type === typeFilter);

  const typeCounts = devices.reduce((acc, d) => {
    acc[d.device_type] = (acc[d.device_type] || 0) + 1;
    return acc;
  }, {} as Record<string, number>);

  const onlineCount = devices.filter((d) => d.online).length;

  if (isLoading) {
    return (
      <div className="p-6">
        <div className="animate-pulse space-y-4">
          <div className="h-8 bg-gray-200 dark:bg-gray-700 rounded w-48" />
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
            {[1, 2, 3].map((i) => (
              <div key={i} className="h-40 bg-gray-200 dark:bg-gray-700 rounded-lg" />
            ))}
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="p-6 space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 dark:text-gray-100">
            Devices
          </h1>
          <p className="text-sm text-gray-500 dark:text-gray-400">
            {devices.length} registered · {onlineCount} online
          </p>
        </div>
      </div>

      {/* Type filter tabs */}
      {Object.keys(typeCounts).length > 1 && (
        <div className="flex gap-2 flex-wrap">
          <button
            onClick={() => setTypeFilter('all')}
            className={`px-3 py-1.5 text-sm rounded-full transition-colors ${
              typeFilter === 'all'
                ? 'bg-blue-600 text-white'
                : 'bg-gray-100 dark:bg-gray-700 text-gray-700 dark:text-gray-300 hover:bg-gray-200 dark:hover:bg-gray-600'
            }`}
          >
            All ({devices.length})
          </button>
          {Object.entries(typeCounts).map(([type, count]) => (
            <button
              key={type}
              onClick={() => setTypeFilter(type)}
              className={`px-3 py-1.5 text-sm rounded-full transition-colors ${
                typeFilter === type
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-100 dark:bg-gray-700 text-gray-700 dark:text-gray-300 hover:bg-gray-200 dark:hover:bg-gray-600'
              }`}
            >
              {DEVICE_TYPE_ICONS[type] || '📟'} {DEVICE_TYPE_LABELS[type] || type} ({count})
            </button>
          ))}
        </div>
      )}

      {/* Device grid */}
      {filteredDevices.length === 0 ? (
        <div className="text-center py-12 text-gray-500 dark:text-gray-400">
          <p className="text-lg">No devices registered</p>
          <p className="text-sm mt-1">Devices will appear here after they register with the server.</p>
        </div>
      ) : (
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {filteredDevices.map((device) => (
            <DeviceCard
              key={device.device_id}
              device={device}
              onDelete={handleDelete}
            />
          ))}
        </div>
      )}
    </div>
  );
}
