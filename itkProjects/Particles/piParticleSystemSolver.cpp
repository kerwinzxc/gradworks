//
//  piParticleSystemSolver.cpp
//  ParticlesGUI
//
//  Created by Joohwi Lee on 2/15/13.
//
//

#include "piParticleSystemSolver.h"
#include "piParticleCollision.h"
#include "piParticleForces.h"
#include "piOptions.h"
#include "piParticleTrace.h"
#include "itkExceptionObject.h"

#include "fstream"
#include "sstream"
#include "piTimer.h"

namespace pi {
    using namespace std;
    
    bool ParticleSystemSolver::LoadConfig(const char* name) {
        ifstream in(name);
        if (!in.is_open()) {
            cout << "Can't open " << name << endl;
            return false;
        }
        
        cout << "loading " << name << " ..." << endl;
        in >> m_Options;
        
        // this requires 'NumberOfParticles:', 'Subjects:'
        m_System = ParticleSystem();
        m_System.InitializeSystem(m_Options);
        
        m_ImageContext.Clear();
        StringVector& labelImages = m_Options.GetStringVector("LabelImages:");
        for (int i = 0; i < labelImages.size(); i++) {
            m_ImageContext.LoadLabel(labelImages[i]);
        }
        
        StringVector& realImages = m_Options.GetStringVector("RealImages:");
        for (int i = 0; i < realImages.size(); i++) {
            m_ImageContext.LoadRealImage(realImages[i]);
        }
        
        while (in.good()) {
            char buf[1024];
            in.getline(buf, sizeof(buf));
            if (in.good()) {
                stringstream ss(buf);
                string name;
                ss >> name;
                
                if (name == "Particles:") {
                    int subjId, nPoints;
                    ss >> subjId >> nPoints;
                    if (subjId < m_System.GetNumberOfSubjects()) {
                        if (m_Options.GetBool("load_position_only")) {
                            m_System[subjId].ReadParticlePositions(in, nPoints);
                        } else {
                            m_System[subjId].ReadParticles(in, nPoints);
                        }
                    }
                } else if (name == "InitialParticles:") {
                    int nPoints;
                    ss >> nPoints;
                    m_System.GetInitialSubject().ReadParticlePositions(in, nPoints);
                    cout << "Read " << nPoints << " initial particles" << endl;
                } else if (name == "ParticleAlignment:") {
                    int nsubj;
                    ss >> nsubj;
                    for (int i = 0; i < nsubj; i++) {
                        in.getline(buf, sizeof(buf));
                        string line(buf);
                        stringstream ss(line);
                        ss >> m_System[i].alignment;
                    }
                }
            }
        }
        return true;
    }
    
    bool ParticleSystemSolver::SaveConfig(const char* name) {
        ofstream out(name);
        if (!out.is_open()) {
            cout << "Can't write " << name << endl;
            return false;
        }
        out << m_Options << OPTION_END << endl << endl;
        const int nInitialPoints = m_System.GetInitialSubject().GetNumberOfPoints();
        if (nInitialPoints > 0) {
            out << "InitialParticles: " << nInitialPoints << endl;
            m_System.GetInitialSubject().WriteParticlePositions(out);
        }
        const int nSubj = m_System.GetNumberOfSubjects();
        const int nParticles = m_System.GetNumberOfParticles();
        if (nSubj > 0) {
            for (int i = 0; i < nSubj; i++) {
                out << "ParticleAlignment: " << i << " " << nParticles << endl;
                out << m_System[i].alignment << endl;
            }
            for (int i = 0; i < nSubj; i++) {
                out << "Particles: " << i << " " << nParticles << endl;
                if (m_Options.GetBool("save_position_only")) {
                    m_System[i].WriteParticlePositions(out);
                } else {
                    m_System[i].WriteParticles(out);
                }
            }
        }
        return true;
    }
    
    void ParticleSystemSolver::Preprocessing() {
        string traceFile = m_Options.GetString("PreprocessingTrace:", "");
        const bool traceOn = traceFile != "";
        ParticleTrace trace;
        
        ParticleCollision boundary;
        boundary.applyMaskSmoothing = true;
        m_Options.GetStringTo("InitialIntersectionMaskCache:", boundary.binaryMaskCache);
        m_Options.GetStringTo("InitialIntersectionDistanceMapCache:", boundary.distanceMapCache);
        
        ParticleSubject& initial = m_System.GetInitialSubject();
        if (initial.GetNumberOfPoints() == 0) {
            
            // compute intersection in combination with particle boundary
            itkcmds::itkImageIO<LabelImage> io;
            if (io.FileExists(boundary.binaryMaskCache.c_str())) {
                boundary.SetBinaryMask(io.ReadImageT(boundary.binaryMaskCache.c_str()));
            } else {
                int npixels = m_ImageContext.ComputeIntersection();
                if (npixels == 0) {
                    cout << "invalid intersection.. halt" << endl;
                    exit(0);
                    return;
                }
                boundary.SetLabelImage(m_ImageContext.GetIntersection());
            }
            
            boundary.UpdateImages();
            m_ImageContext.SetIntersection(boundary.GetBinaryMask());
            
            initial.m_Name = "Intersection";
            initial.NewParticles(m_Options.GetInt("NumberOfParticles:", 0));
            initial.InitializeRandomPoints(m_ImageContext.GetIntersection());
            string initialConfigOutput = m_Options.GetString("InitialConfigOutput:", "");
            if (initialConfigOutput != "") {
                SaveConfig(initialConfigOutput.c_str());
            }
        } else {
            cout << "Skip preprocessing and use pre-generated data..." << endl;
            return;
        }
        
        if (initial.GetNumberOfPoints() == 0) {
            cout << "Fail to initializing with random points" << endl;
            return;
        }
        
        DataReal t0 = m_Options.GetRealVectorValue("PreprocessingTimeRange:", 0);
        DataReal dt = m_Options.GetRealVectorValue("PreprocessingTimeRange:", 1);
        DataReal t1 = m_Options.GetRealVectorValue("PreprocessingTimeRange:", 2);
        
        EntropyInternalForce internalForce;
        m_Options.GetRealTo("InternalForceSigma:", internalForce.repulsionSigma);
        m_Options.GetRealTo("InternalForceCutoff:", internalForce.repulsionCutoff);
        
        // this must be off for preprocessing
        internalForce.useAdaptiveSampling = false;
        
        const int nPoints = initial.GetNumberOfPoints();
        Timer timer;
        for (DataReal t = t0; t < t1; t += dt) {
            timer.start();
            cout << "t: " << t << " " << flush;
            ParticleSubject& sub = initial;
            
            for (int i = 0; i < nPoints; i++) {
                Particle& pi = sub[i];
                forset(pi.x, pi.w);
                forfill(pi.f, 0);
            }
            
            internalForce.ComputeForce(initial);
            boundary.HandleCollision(initial);
            
            for (int i = 0; i < nPoints; i++) {
                Particle& p = sub[i];
                LabelImage::IndexType pIdx;
                fordim (k) {
                    p.f[k] -= p.v[k];
                    p.v[k] += dt*p.f[k];
                    p.x[k] += dt*p.v[k];
                    pIdx[k] = p.x[k] + 0.5;
                }
                
                
                if (!boundary.IsBufferInside(pIdx)) {
                    cout << "Stop system: out of region" << endl;
                    goto quit;
                }
            }
            
            if (traceOn) {
                trace.Add(t, sub.m_Particles, 0);
            }
            
            double elapsedTime = timer.getElapsedTimeInSec();
            cout << "; elapsed time: " << elapsedTime << " sec                   \r" << flush;
        }
        
        if (traceOn) {
            ofstream out(traceFile.c_str());
            trace.Write(out);
            out.close();
        }
    quit:
        cout << "Preprocessing done ..." << endl;
        return;
    }
    
    
    void ParticleSystemSolver::Run() {
        ParticleSystem& system = m_System;
        const int nSubz = system.GetNumberOfSubjects();
        const int nPoints = system.GetNumberOfParticles();
        ParticleSubjectArray& subs = system.GetSubjects();
        
        system.ComputeMeanSubject();
        
        string traceFile = m_Options.GetString("RunTrace:", "");
        const bool traceOn = traceFile != "";
        
        string systemSnapshot;
        m_Options.GetStringTo("SystemSnapshot:", systemSnapshot);
        
        ParticleTrace trace;
        trace.Resize(nSubz);
        
        if (!m_Options.GetBool("use_previous_position")) {
            for (int n = 0; n < nSubz; n++) {
                m_System[n].Initialize(system.GetInitialSubject().m_Particles);
            }
        }
        
        const bool useEnsemble = m_Options.GetBool("ensemble");
        const bool useIntensity = m_Options.GetBool("intensity");
        const bool noInternal = m_Options.GetBool("no_internal");
        const bool noBoundary = m_Options.GetBool("no_boundary");
        
        // check label images are loaded
        if (!noBoundary && nSubz != m_ImageContext.GetLabelVector().size()) {
            cout << "the same number of boundary mask files are required" << endl;
            return;
        }
        
        EntropyInternalForce internalForce;
        m_Options.GetRealTo("InternalForceSigma:", internalForce.repulsionSigma);
        m_Options.GetRealTo("InternalForceCutoff:", internalForce.repulsionCutoff);
        m_Options.GetBoolTo("adaptive_sampling", internalForce.useAdaptiveSampling);
        if (internalForce.useAdaptiveSampling) {
            m_System.LoadKappaImages(m_Options, &m_ImageContext);
        }
        if (internalForce.repulsionCutoff < internalForce.repulsionSigma) {
            internalForce.repulsionCutoff = internalForce.repulsionSigma * 5;
            cout << "repulsion sigma: " << internalForce.repulsionSigma << endl;
            cout << "adjusted repulsion cutoff: " << internalForce.repulsionCutoff << endl;
        }

        
        
        EnsembleForce ensembleForce;
        ensembleForce.SetImageContext(&m_ImageContext);
        
        IntensityForce intensityForce;
        intensityForce.SetImageContext(&m_ImageContext);
        intensityForce.useAttributesAtWarpedSpace = true;

        internalForce.coeff = 0.3;
        ensembleForce.coeff = 0.3;
        intensityForce.coeff = 0.3;
        
#ifdef ATTR_SIZE
        m_Options.SetInt("AttributeDimension:", ATTR_SIZE);
#endif
        
        std::vector<ParticleCollision> collisionHandlers;
        collisionHandlers.resize(nSubz);
        
        for (int n = 0; n < nSubz; n++) {
            collisionHandlers[n].applyMaskSmoothing = true;
            m_Options.GetStringVectorValueTo("BinaryMaskCache:", n, collisionHandlers[n].binaryMaskCache);
            m_Options.GetStringVectorValueTo("BinaryMaskDistanceMapCache:", n, collisionHandlers[n].distanceMapCache);
            collisionHandlers[n].SetLabelImage(m_ImageContext.GetLabel(n));
            collisionHandlers[n].UpdateImages();
        }
        
        const DataReal t0 = m_Options.GetRealVectorValue("TimeRange:", 0);
        const DataReal dt = m_Options.GetRealVectorValue("TimeRange:", 1);
        const DataReal t1 = m_Options.GetRealVectorValue("TimeRange:", 2);
        
        if (useEnsemble) cout << "ensemble term enabled" << endl; else cout << "ensemble term disabled" << endl;
        if (useIntensity) cout << "intensity term enabled" << endl; else cout << "intensity term disabled" << endl;
        if (noInternal) cout << "internal force disabled" << endl; else cout << "internal force enabled" << endl;
        if (noBoundary) cout << "boundary term disabled" << endl; else cout << "boundary term enabled" << endl;
        if (internalForce.useAdaptiveSampling) cout << "adaptive sampling enabled" << endl; else cout << "adaptive sampling disabled" << endl;
        cout << Attr::NATTRS << " attributes per particle" << endl;
        
        m_System.currentIteration = -1;
        Timer timer;
//        ofstream err("error.txt");

        try {
            for (DataReal t = t0; t < t1; t += dt) {
                
                timer.start();
                m_System.currentTime = t;
                m_System.currentIteration++;
                
                cout << "t: " << t << " " << flush;
                m_System.ComputeMeanSubject();
                if (systemSnapshot != "") {
                    SaveConfig(systemSnapshot.c_str());
                }

                // compute internal force
                for (int n = 0; n < nSubz; n++) {
                    for (int i = 0; i < nPoints; i++) {
                        Particle& pi = m_System[n][i];
                        forfill(pi.f, 0);
                    }
                    collisionHandlers[n].ConstrainPoint(m_System[n]);
                }
                m_System.ComputeMeanSubject();
                if (!noInternal) {
                    for (int n = 0; n < nSubz; n++) {
                        m_System[n].ComputeAlignment(m_System.GetMeanSubject());
                        m_System[n].AlignmentTransformX2Y();
                        internalForce.ComputeForce(m_System[n]);
                    }
                    m_System.ComputeMeanSubject();
                }
                if (useEnsemble) {
                    ensembleForce.ComputeEnsembleForce(m_System);
                }
                if (useIntensity) {
                    intensityForce.ComputeIntensityForce(&m_System);
                }
                // system update
                for (int n = 0; n < nSubz; n++) {
                    ParticleSubject& sub = subs[n];
                    collisionHandlers[n].ProjectForceAndVelocity(sub);
                    for (int i = 0; i < nPoints; i++) {
                        Particle& p = sub[i];
                        LabelImage::IndexType pIdx;
                        fordim (k) {
                            p.f[k] -= p.v[k];
                            p.v[k] += dt*p.f[k];
                            p.x[k] += dt*p.v[k];
                            pIdx[k] = p.x[k] + 0.5;
                        }
                        if (!collisionHandlers[n].IsBufferInside(pIdx)) {
                            cout << "\nStop system: out of region" << endl;
                            goto quit;
                        }
                    }

                    if (traceOn) {
                        trace.Add(t, sub);
                    }
                }
                double elapsedTime = timer.getElapsedTimeInSec();
                cout << "; elapsed time: " << elapsedTime << " secs; estimated remaining time: about " << int((elapsedTime*(t1 - t)/dt)/60+1) << " mins                   \r" << flush;
            }
            
            // last collision handling
            for (int i = 0 ; i < nSubz; i++) {
                collisionHandlers[i].HandleCollision(system[i]);
            }
            
        quit:
            cout << "Done ..." << endl;
        } catch (itk::ExceptionObject& ex) {
            ex.Print(cout);
        }
//        err.close();
        if (traceOn) {
            ofstream traceOut(traceFile.c_str());
            trace.Write(traceOut);
            traceOut.close();
        }
        return;
    }
}