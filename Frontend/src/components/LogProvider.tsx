import React, { createContext, useState, useContext, useCallback, type ReactNode } from 'react';

// ログの種類
export type LogType = 'error' | 'info' | 'warn' | 'success';

// ログデータの構造
export interface LogEntry {
    id: string;
    message: string;
    type: LogType;
    timestamp: Date;
}

// コンテキストの型定義
interface LogContextType {
    logs: LogEntry[];
    logMessage: (message: string, type?: LogType) => void;
    logError: (message: string) => void;
    logWarn: (message: string) => void;
    logInfo: (message: string) => void;
    log: (message: string) => void;
    clearLogs: () => void;
    removeLog: (id: string) => void;
}

const LogContext = createContext<LogContextType | null>(null);

// 汎用フック：他のコンポーネントからログを操作するために使用
export const useLogger = () => {
    const context = useContext(LogContext);
    if (!context) {
        throw new Error("useLogger must be used within a LogProvider");
    }
    return context;
};

// プロバイダーコンポーネント：UIと状態管理を提供
export const LogProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
    const [logs, setLogs] = useState<LogEntry[]>([]);
    const [showHistory, setShowHistory] = useState(false);

    // ログの追加
    const logMessage = useCallback((message: string, type: LogType = 'info') => {
        const newLog: LogEntry = {
            id: Math.random().toString(36).substring(2, 9),
            message,
            type,
            timestamp: new Date()
        };
        setLogs(prev => [newLog, ...prev]);
        switch (type) {
            case 'error':
                console.error(message);
                break;
            case 'info':
                console.info(message);
                break;
            case 'warn':
                console.warn(message);
                break;
            default:
                console.log(message);
                break;
        }
    }, []);

    const logError = useCallback((message: string) => logMessage(message, 'error'), [logMessage]);
    const logWarn = useCallback((message: string) => logMessage(message, 'warn'), [logMessage]);
    const logInfo = useCallback((message: string) => logMessage(message, 'info'), [logMessage]);
    const log = useCallback((message: string) => logMessage(message, 'success'), [logMessage]);

    // ログのクリア
    const clearLogs = useCallback(() => setLogs([]), []);

    // 特定の通知を閉じる
    const removeLog = useCallback((id: string) => setLogs(prev => prev.filter(l => l.id !== id)), []);

    return (
        <LogContext.Provider value={{ logs, logMessage, logError, logWarn, logInfo, log, clearLogs, removeLog }}>
            {children}

            {/* 最新のエラーダイアログ（フローティング表示） */}
            <div style={{ position: 'fixed', top: '20px', left: '50%', transform: 'translateX(-50%)', zIndex: 9999, display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {logs.slice(0, 3).map(log => (
                    <div key={log.id} style={{
                        backgroundColor: log.type === 'error' ? '#d32f2f' : log.type === 'success' ? '#388e3c' : '#1976d2',
                        color: 'white', padding: '12px 24px', borderRadius: '8px',
                        display: 'flex', alignItems: 'center', gap: '16px',
                        boxShadow: '0 4px 12px rgba(0,0,0,0.5)', fontWeight: 'bold'
                    }}>
                        <span>{log.message}</span>
                        <button onClick={() => removeLog(log.id)} style={{ background: 'transparent', border: '1px solid white', color: 'white', borderRadius: '4px', cursor: 'pointer' }}>
                            閉じる
                        </button>
                    </div>
                ))}
            </div>

            {/* ログ履歴パネルのトグルボタン */}
            <button
                onClick={() => setShowHistory(!showHistory)}
                style={{ position: 'fixed', bottom: '20px', right: '20px', zIndex: 9998, padding: '8px 16px', borderRadius: '20px', background: '#3a3f55', color: '#fff', border: 'none', cursor: 'pointer', boxShadow: '0 2px 8px rgba(0,0,0,0.3)' }}
            >
                {showHistory ? 'ログ履歴を閉じる' : `ログ履歴 (${logs.length})`}
            </button>

            {/* ログ履歴パネル */}
            {showHistory && (
                <div style={{ position: 'fixed', bottom: '70px', right: '20px', width: '350px', maxHeight: '400px', background: '#1e202b', border: '1px solid #4a5168', borderRadius: '8px', zIndex: 9998, display: 'flex', flexDirection: 'column', boxShadow: '0 4px 16px rgba(0,0,0,0.5)' }}>
                    <div style={{ padding: '12px', borderBottom: '1px solid #4a5168', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <h4 style={{ margin: 0, color: '#fff' }}>システムログ</h4>
                        <button onClick={clearLogs} style={{ background: '#d32f2f', color: '#fff', border: 'none', padding: '4px 8px', borderRadius: '4px', cursor: 'pointer', fontSize: '12px' }}>
                            クリア
                        </button>
                    </div>
                    <div style={{ padding: '12px', overflowY: 'auto', flex: 1, display: 'flex', flexDirection: 'column', gap: '8px' }}>
                        {logs.length === 0 ? (
                            <span style={{ color: '#888', fontSize: '14px', textAlign: 'center' }}>ログはありません</span>
                        ) : (
                            logs.map(log => (
                                <div key={log.id} style={{ fontSize: '13px', padding: '8px', background: '#2a2d3d', borderRadius: '4px', borderLeft: `4px solid ${log.type === 'error' ? '#d32f2f' : log.type === 'success' ? '#388e3c' : '#1976d2'}` }}>
                                    <div style={{ color: '#aaa', fontSize: '11px', marginBottom: '4px' }}>
                                        {log.timestamp.toLocaleTimeString()}
                                    </div>
                                    <div style={{ color: '#eee' }}>{log.message}</div>
                                </div>
                            ))
                        )}
                    </div>
                </div>
            )}
        </LogContext.Provider>
    );
};