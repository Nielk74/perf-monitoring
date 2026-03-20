import { Routes, Route } from 'react-router-dom'
import Layout from './components/Layout'
import Home from './pages/Home'
import SilentDegrader from './pages/SilentDegrader'
import BlastRadius from './pages/BlastRadius'
import EnvironmentalFingerprint from './pages/EnvironmentalFingerprint'
import ImpersonationAudit from './pages/ImpersonationAudit'
import FeatureSunset from './pages/FeatureSunset'
import FrictionHeatmap from './pages/FrictionHeatmap'
import WorkflowDiscovery from './pages/WorkflowDiscovery'
import AdoptionVelocity from './pages/AdoptionVelocity'
import PeakPressure from './pages/PeakPressure'
import AnomalyGuard from './pages/AnomalyGuard'

export default function App() {
  return (
    <Layout>
      <Routes>
        <Route path="/"                         element={<Home />} />
        <Route path="/silent-degrader"          element={<SilentDegrader />} />
        <Route path="/blast-radius"             element={<BlastRadius />} />
        <Route path="/environmental-fingerprint" element={<EnvironmentalFingerprint />} />
        <Route path="/impersonation-audit"      element={<ImpersonationAudit />} />
        <Route path="/feature-sunset"           element={<FeatureSunset />} />
        <Route path="/friction-heatmap"         element={<FrictionHeatmap />} />
        <Route path="/workflow-discovery"       element={<WorkflowDiscovery />} />
        <Route path="/adoption-velocity"        element={<AdoptionVelocity />} />
        <Route path="/peak-pressure"            element={<PeakPressure />} />
        <Route path="/anomaly-guard"            element={<AnomalyGuard />} />
      </Routes>
    </Layout>
  )
}
