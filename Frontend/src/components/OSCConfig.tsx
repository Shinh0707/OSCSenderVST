import React, { useState, useEffect } from 'react';
import { Network, Hash, Send } from 'lucide-react';
import { getNativeFunction } from '../juce_interop';

interface OSCSettings {
  ip: string;
  port: number;
  oscAddress: string;
}

const OSCConfig: React.FC = () => {
  const [settings, setSettings] = useState<OSCSettings>({
    ip: '127.0.0.1',
    port: 9000,
    oscAddress: '/midi/note'
  });

  const updatePluginState = getNativeFunction('updatePluginState');

  useEffect(() => {
    const timer = setTimeout(() => {
      updatePluginState(JSON.stringify(settings));
    }, 500);
    return () => clearTimeout(timer);
  }, [settings, updatePluginState]);

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value } = e.target;
    setSettings(prev => ({
      ...prev,
      [name]: name === 'port' ? (parseInt(value) || 0) : value
    }));
  };

  return (
    <div className="config-card">
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '24px' }}>
        <Send className="status-dot" size={24} style={{ color: 'var(--accent-color)' }} />
        <h1>OSC MIDI Sender</h1>
      </div>
      <p className="subtitle">Configure your OSC destination settings below.</p>

      <div className="input-group">
        <label>IP Address</label>
        <div style={{ position: 'relative' }}>
          <Network size={16} style={{ position: 'absolute', left: '12px', top: '14px', color: 'var(--text-secondary)' }} />
          <input
            style={{ paddingLeft: '40px' }}
            name="ip"
            value={settings.ip}
            onChange={handleChange}
            placeholder="127.0.0.1"
          />
        </div>
      </div>

      <div className="input-group">
        <label>UDP Port</label>
        <div style={{ position: 'relative' }}>
          <Hash size={16} style={{ position: 'absolute', left: '12px', top: '14px', color: 'var(--text-secondary)' }} />
          <input
            style={{ paddingLeft: '40px' }}
            name="port"
            type="number"
            value={settings.port}
            onChange={handleChange}
            placeholder="9000"
          />
        </div>
      </div>

      <div className="input-group">
        <label>OSC Address Path</label>
        <div style={{ position: 'relative' }}>
          <Hash size={16} style={{ position: 'absolute', left: '12px', top: '14px', color: 'var(--text-secondary)' }} />
          <input
            style={{ paddingLeft: '40px' }}
            name="oscAddress"
            value={settings.oscAddress}
            onChange={handleChange}
            placeholder="/midi/note"
          />
        </div>
      </div>

      <div className="osc-status">
        <div className="status-dot"></div>
        <span>Broadcasting MIDI via OSC</span>
      </div>
    </div>
  );
};

export default OSCConfig;
