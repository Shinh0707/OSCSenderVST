import './App.css'
import { LogProvider } from './components/LogProvider';
import OSCConfig from './components/OSCConfig';

function App() {
  return (
    <LogProvider>
      <main className="app-main">
        <OSCConfig />
      </main>
    </LogProvider>
  );
}

export default App;
