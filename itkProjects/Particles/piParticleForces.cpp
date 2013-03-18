//
//  myParticleForces.cpp
//  ParticlesGUI
//
//  Created by Joohwi Lee on 1/26/13.
//
//

#include "piParticleCore.h"
#include "piParticleBSpline.h"
#include "piParticleForces.h"
#include "piParticleSystem.h"
#include "piImageProcessing.h"
#include "itkGradientRecursiveGaussianImageFilter.h"
#include "itkVectorLinearInterpolateImageFunction.h"
#include "itkConstNeighborhoodIterator.h"
#include "armadillo"
#include "boost/numeric/ublas/matrix.hpp"
#include "boost/numeric/ublas/io.hpp"

#include <vtkProcrustesAlignmentFilter.h>
#include <vtkLandmarkTransform.h>
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkPolyDataWriter.h"
#include "vtkFloatArray.h"
#include "vtkPointData.h"
#include "piVTK.h"

#define __show2(t,x,m,n) for (int _=0;_<m;_++) { cout << t << " #" << _ << ":"; for (int __=0;__<n;__++) cout << " " << x[__+_*n]; cout << endl; }
#define __show1(x,m) cout << #x << ":"; for (int _=0;_<m;_++) cout << " " << x[_]; cout << endl;
#define __showmatrix(x,m,n) for (int _=0;_<m;_++) { for (int __=0;__<n;__++) cout << x(_,__); cout << endl; }; cout << endl
#define __showcmd(x,m,n,cmd) for (int _=0;_<m;_++) { for (int __=0;__<n;__++) cmd; cout << endl; }; cout << endl

namespace pi {
    ostream& operator<<(ostream& os, Attr& attr) {
        int p = os.precision();
        os.precision(8);
        for (int i = 0; i < attr.NATTRS; i++) {
            os << " " << attr.x[i];
        }
        os << ";";
        for (int i = 0; i < attr.NATTRS; i++) {
            os << " " << attr.y[i];
        }
        os << ";";
        for (int i = 0; i < attr.NATTRS; i++) {
            fordim (k) {
                if (k > 0) {
                    os << ",";
                }
                os << attr.g[i][k];
            }
            os << " ";
        }
        os.precision(p);
        return os;
    }

    
//    static void ExtractAttributes(RealImage::Pointer image, VectorImage::Pointer grad, Particle& par);

    void InternalForce::ComputeForce(ParticleSubject& subj) {
        const int nPoints = subj.m_Particles.size();
        ParticleArray& particles = subj.m_Particles;
#pragma omp parallel for
        for (int i = 0; i < nPoints; i++) {
            Particle& pi = particles[i];
            for (int j = i+1; j < nPoints; j++) {
                Particle& pj = particles[j];
                ComputeForce(pi, pj);
            }
        }
    }
    
    void InternalForce::ComputeForce(ParticleSubjectArray& shapes) {
        const int nSubjects = shapes.size();
        for (int k = 0; k < nSubjects; k++) {
            ComputeForce(shapes[k]);
        }
    }
    
    void InternalForce::ComputeForce(Particle &pi, Particle &pj) {
        const DataReal sigma = repulsionSigma * 5;
        const DataReal coeff = M_PI_2 / sigma;
        const bool useSimpleForce = false;
        
        DataReal fi[__Dim] = { 0 }, fj[__Dim] = { 0 };
        DataReal dx[__Dim] = { 0 };

        DataReal rij2 = 0;
        pi.Sub(pj, dx);
        fordim(k) {
            rij2 += (dx[k]*dx[k]);
        }
        const DataReal rij = std::sqrt(rij2);

        if (rij <= sigma) {
            if (useSimpleForce) {
                fordim (k) {
                    fj[k] = fi[k] = -dx[k];
                }
            } else {
                fordim(k) {
                    dx[k] /= rij;
                }
                const DataReal crij = rij * coeff;
                const DataReal sin1crij = std::sin(crij);
                const DataReal sin2crij = sin1crij * sin1crij;
                fordim(k) {
                    fj[k] = fi[k] = (dx[k] * (coeff * (1 - (1 / sin2crij))));
                }
            }
            pi.AddForce(fi, -1);
            pj.AddForce(fj);
        }
    }

    
    void EntropyInternalForce::ComputeForce(ParticleSubject& subj) {
        if (useMultiPhaseForce) {
            ComputeHeteroForce(subj);
        } else {
            ComputeHomoForce(subj);
        }
    }

    void EntropyInternalForce::ComputeHomoForce(ParticleSubject& subj) {
        const int nPoints = subj.GetNumberOfPoints();

        bool useKappa = useAdaptiveSampling && subj.kappaSampler.IsNotNull();
#pragma omp parallel for
        for (int i = 0; i < nPoints; i++) {
            Particle& pi = subj.m_Particles[i];
            const DataReal sigma = repulsionSigma;
            const DataReal sigma2 = sigma * sigma;
            const DataReal cutoff = repulsionCutoff;

            // iteration over particles
            // may reduce use symmetric properties
            VNLVector weights(nPoints, 0);
            for (int j = 0; j < nPoints; j++) {
                Particle& pj = subj.m_Particles[j];
                if (i == j) {
                    // there's no self interaction
                    weights[j] = 0;
                } else {
                    DataReal kappa = 1;
                    if (useKappa) {
                        RealIndex jIdx;
                        fordim (k) {
                            jIdx[k] = pj.x[k];
                        }
                        kappa = subj.kappaSampler->EvaluateAtContinuousIndex(jIdx);
                        kappa *= kappa;
                    }
                    DataReal dij = sqrt(pi.Dist2(pj));
                    if (dij > cutoff) {
                        weights[j] = 0;
                    } else {
                        weights[j] = exp(-dij*dij*kappa/(sigma2));
                    }
                }
            }
            DataReal sumForce = weights.sum();
            if (sumForce > 0) {
                weights /= sumForce;
            }
            
            // update force for neighboring particles
            for (int j = 0; j < nPoints; j++) {
                if (i == j || weights[j] == 0) {
                    continue;
                }
                Particle& pj = subj.m_Particles[j];
                DataReal weight = weights[j];
                VNLVector xixj(__Dim, 0);
                fordim (k) {
                    xixj[k] = pi.x[k] - pj.x[k];
                }
                xixj.normalize();
                fordim (k) {
                    //pi.f[k] += (coeff * weight * (pi.x[k] - pj.x[k]));
                    pi.f[k] += (coeff * weight * xixj[k]);
                }
            }
        }
    }

    void EntropyInternalForce::ComputeHeteroForce(ParticleSubject& subj) {
        const int nPoints = subj.GetNumberOfPoints();

        bool useKappa = useAdaptiveSampling && subj.kappaSampler.IsNotNull();
        const DataReal sigma = repulsionSigma;
        const DataReal sigma2 = sigma * sigma;
        const DataReal friendSigma2 = friendSigma*friendSigma;
        const DataReal cutoff = repulsionCutoff;

        if (subj.friendImage.IsNull()) {
            cout << "Multi-phase internal force computation not available without friend sampler." << endl;
            return;
        }
        subj.friendSampler = NNLabelInterpolatorType::New();
        subj.friendSampler->SetInputImage(subj.friendImage);


#pragma omp parallel for
        for (int i = 0; i < nPoints; i++) {


            Particle& pi = subj.m_Particles[i];
            IntIndex idx;
            fordim (k) {
                idx[k] = pi.x[k];
            }
            pi.label = subj.friendSampler->EvaluateAtIndex(idx);

            // iteration over particles
            // may reduce use symmetric properties
            VNLVector weights(nPoints, 0);
            for (int j = 0; j < nPoints; j++) {
                Particle& pj = subj.m_Particles[j];
                if (i == j) {
                    weights[j] = 0;
                    continue;
                }

                // label query
                fordim (k) {
                    idx[k] = pj.x[k] - 0.5;
                }
                pj.label = subj.friendSampler->EvaluateAtIndex(idx);

                DataReal kappa = 1;
                if (useKappa) {
                    RealIndex jIdx;
                    fordim (k) {
                        jIdx[k] = pj.x[k];
                    }
                    kappa = subj.kappaSampler->EvaluateAtContinuousIndex(jIdx);
                    kappa *= kappa;
                }
                DataReal dij = sqrt(pi.Dist2(pj));

                const bool isFriend = pi.label == pj.label;
                if (!isFriend) {
                    if (dij > cutoff) {
                        weights[j] = 0;
                    } else {
                        weights[j] = exp(-dij*dij*kappa/(sigma2));
                    }
                } else {
                    if (dij > friendCutoff) {
                        weights[j] = 0;
                    } else {
                        weights[j] = exp(-dij*dij*kappa/(friendSigma2));
                    }
                }
            }

            DataReal sumForce = weights.sum();
            if (sumForce > 0) {
                weights /= sumForce;
            }

            // update force for neighboring particles
            for (int j = 0; j < nPoints; j++) {
                if (i == j || weights[j] == 0) {
                    continue;
                }
                Particle& pj = subj.m_Particles[j];
                DataReal weight = weights[j];
                VNLVector xixj(__Dim, 0);
                fordim (k) {
                    xixj[k] = pi.x[k] - pj.x[k];
                }
                xixj.normalize();
                fordim (k) {
                    pi.f[k] += (coeff * weight * (pi.x[k] - pj.x[k]));
                }
            }
        }
    }

    void EntropyInternalForce::ComputeForce(ParticleSubjectArray& subjs) {
        const int nSubjs = subjs.size();
        for (int n = 0; n < nSubjs; n++) {
            ComputeForce(subjs[n]);
        }
    }

    void EntropyInternalForce::ComputeForce(Particle& a, Particle& b) {

    }


    EnsembleForce::EnsembleForce() : coeff(1), useBSplineAlign(false) {

    }

    EnsembleForce::~EnsembleForce() {

    }

    void EnsembleForce::SetImageContext(ImageContext* context) {
        m_ImageContext = context;
    }

//    void EnsembleForce::ComputeMeanShape(ParticleSubjectArray& shapes) {
//        if (shapes.size() < 1) {
//            return;
//        }
//        
//        const int nSubjects = shapes.size();
//        const int nPoints = shapes[0].GetNumberOfPoints();
//
//        m_MeanShape.m_SubjId = -1;
//        m_MeanShape.NewParticles(shapes[0].GetNumberOfPoints());
//
//        // for every dimension k
//        fordim(k) {
//            // for every point i
//            for (int i = 0; i < nPoints; i++) {
//                // sum over all subject j
//                for (int j = 0; j < nSubjects; j++) {
//                    m_MeanShape[i].x[k] += shapes[j][i].x[k];
//                }
//                m_MeanShape[i].x[k] /= nSubjects;
//            }
//        }
//    }

    void EnsembleForce::ComputeEnsembleForce(ParticleSystem& system) {
        if (system.GetNumberOfSubjects() < 2) {
            return;
        }
        const int nPoints = system.GetNumberOfParticles();
        const int nSubjects = system.GetNumberOfSubjects();

        // Here, we align particles with the mean subject
        // Let y be an aligned coordinate of a particle position x.
        // The deformed coordinate z is estimated from the bspline landmark transform from y
        // An ensemble force f_z in z-space is computed.
        // An ensemble force f_y in y-space is computed by multiplying jacobian of the bspline
        // An ensemble force f_x in the original x-space is computed by multiplying the jacobian of the alignment transform

        // we assume that the mean and its corresponding alignment is already computed
        // therefore, we store y from x
        /*
        // compute mean(Y)
        for (int j = 0; j < nPoints; j++) {
            fordim (k) {
                meanSubj[j].y[k] = 0;
            }
            for (int i = 0; i < nSubjects; i++) {
                fordim (k) {
                    meanSubj[j].y[k] += system[i][j].y[k];
                }
            }
            fordim (k) {
                meanSubj[j].y[k] /= nSubjects;
            }
        }
        */


        // now working at y-space estimating b-spline transform from the subject to the mean
        system.ComputeZMeanSubject();
        ParticleSubject& meanSubj = system.GetMeanSubject();

        for (int i = 0; i < nSubjects; i++) {
            ParticleBSpline bspline;
            bspline.SetReferenceImage(m_ImageContext->GetLabel(i));
            bspline.EstimateTransformYZ(system[i], meanSubj);
            FieldTransformType::Pointer deformableTransform = bspline.GetTransform();
            system[i].TransformY2Z(deformableTransform.GetPointer());
            system[i].m_DeformableTransform = deformableTransform;
        }


        // now we work at z-space and compute the gradient
        // the gradient is calculated from the entropy of the position matrix;
        // later we may have to change to consolidate attribute function
        // for that, we create a big array at a system so that we can compute it later
        // let's use plenty of memory space!
        // now i'm back, and let's compute point gradient
        
        EntropyComputer comp(nSubjects, nPoints, __Dim);
        comp.dataIter.FirstData();
        for (int i = 0; i < nSubjects; i++) {
            ParticleSubject& subj = system[i];
            for (int j = 0; j < nPoints; j++) {
                Particle& p = subj[j];
                fordim (k) {
                    comp.dataIter.sample[k] = p.z[k];
                }
                comp.dataIter.NextSample();
            }
            comp.dataIter.NextData();
        }
        comp.MoveToCenter();
        comp.ComputeCovariance();
        comp.ComputeGradient();

        DataIterator3 gradIter(comp.gradient.data_block(), nPoints, __Dim);
        gradIter.FirstData();
        for (int i = 0; i < nSubjects; i++) {
            ParticleSubject& iSubj = system[i];
            #pragma omp parallel for
            for (int j = 0; j < nPoints; j++) {
                Particle& pj = iSubj[j];
                FieldTransformType::InputPointType xJac;
                FieldTransformType::JacobianType invJac;
                invJac.set_size(__Dim,__Dim);
                fordim(k) {
                    xJac[k] = pj.y[k];
                }
                iSubj.m_DeformableTransform->ComputeInverseJacobianWithRespectToPosition(xJac, invJac);
                DataReal fM[__Dim];
                fordim(k) {
                    fM[k] = 0;
                    fordim(l) {
                        fM[k] += invJac[l][k]*gradIter.sample[l];
                    }
                }
                DataReal fV[__Dim];
                iSubj.inverseAlignment->TransformVector(fM, fV);
                pj.AddForce(fV, -coeff);
                gradIter.NextSample();
            }
            gradIter.NextData();
        }
    }


    
    //

    IntensityForce::IntensityForce()
    : coeff(1.0), useGaussianGradient(true), gaussianSigma(1), useAttributesAtWarpedSpace(true) {
    }

    IntensityForce::~IntensityForce() {

    }

    void IntensityForce::SetImageContext(ImageContext* context) {
        m_ImageContext = context;
    }



    RealImage::Pointer WarpToMean(ParticleSubject& mean, ParticleSubject& subject, RealImage::Pointer m) {
        typedef itk::AffineTransform<PointReal, __Dim> AffineTransformType;
        AffineTransformType::Pointer affineTransform = AffineTransformType::New();
        AffineTransformType::ParametersType affineParams;
        affineParams.SetSize((__Dim+1)*(__Dim));
        for (int i = 0; i < __Dim; i++) {
            for (int j = 0; j < __Dim; j++) {
                affineParams[i*__Dim + j] = subject.inverseAlignment->GetMatrix()->GetElement(i,j);
            }
            affineParams[__Dim*__Dim + i] = subject.inverseAlignment->GetMatrix()->GetElement(i,3);
        }
        affineTransform->SetParameters(affineParams);

        ParticleBSpline bsp;
        bsp.EstimateTransform<ParticleYCaster,RealImage>(mean, subject, mean.GetNumberOfPoints(), m);

        typedef itk::CompositeTransform<PointReal,__Dim> CompositeTransformType;
        CompositeTransformType::Pointer transform = CompositeTransformType::New();
        transform->AddTransform(bsp.GetTransform());
        transform->AddTransform(affineTransform);

        typedef itk::ResampleImageFilter<RealImage,RealImage> ResampleFilterType;
        ResampleFilterType::Pointer resampleFilter = ResampleFilterType::New();
        resampleFilter->SetInput(m);
        resampleFilter->SetTransform(transform);
        resampleFilter->SetReferenceImage(m);
        resampleFilter->UseReferenceImageOn();
        resampleFilter->Update();
        subject.m_InverseDeformableTransform = bsp.GetTransform();
        return resampleFilter->GetOutput();
    }

    
    void IntensityForce::ComputeAttributes(ParticleSystem* system) {
        const bool useResampling = true;

        ParticleSubjectArray& shapes = system->GetSubjects();
        ParticleSubject& meanSubject = system->GetMeanSubject();

        const int nSubj = shapes.size();
        const int nPoints = shapes[0].GetNumberOfPoints();
        const int nRadius = (int) (ATTR_SIZE / 2);
        itkcmds::itkImageIO<RealImage> io;

        m_attrs.resize(nSubj, nPoints);
        m_attrsMean.set_size(nPoints, Attr::NATTRS);
        warpedImages.resize(nSubj);
        gradImages.resize(nSubj);

        // first create a warped image into the mean transform space
        // second create a gradient vector image per subject
        // third extract attributes (features)
        ImageProcessing proc;
        if (useAttributesAtWarpedSpace) {
            for (int i = 0; i < nSubj; i++) {
                ParticleSubject& subject = shapes[i];
                LabelImage::Pointer refImage = m_ImageContext->GetLabel(i);

                if (useResampling) {
                    warpedImages[i] = WarpToMean(meanSubject, subject, m_ImageContext->GetRealImage(i));
                    if (useGaussianGradient) {
                        gradImages[i] = proc.ComputeGaussianGradient(warpedImages[i], gaussianSigma);
                    } else {
                        gradImages[i] = proc.ComputeGradient(warpedImages[i]);
                    }
                } else {
                    // Assume No Alignment!!!!!
                    ParticleBSpline backwardBspline;
                    backwardBspline.EstimateTransform<ParticleYCaster,RealImage>(meanSubject, subject, meanSubject.GetNumberOfPoints(), subject.realImage);
                    FieldTransformType::Pointer backwardTransform = backwardBspline.GetTransform();
                    subject.m_InverseDeformableTransform = backwardTransform;
                }

                if (subject.m_DeformableTransform.IsNull()) {
                    ParticleBSpline forwardBspline;
                    forwardBspline.SetReferenceImage(refImage);
                    forwardBspline.EstimateTransform(subject, meanSubject);
                    FieldTransformType::Pointer forwardTransform = forwardBspline.GetTransform();
                    subject.TransformY2Z(forwardTransform);
                    cout << "Warping in intensity force not desirable" << endl;
                }
            }
        } else {
            for (int i = 0; i < nSubj; i++) {
                warpedImages[i] = m_ImageContext->GetRealImage(i);
                ParticleSubject& subject = shapes[i];
                for (int j = 0; j < nPoints; j++) {
                    forcopy (subject.m_Particles[j].y, subject.m_Particles[j].z);
                }
            }
        }

        for (int i = 0; i < nSubj; i++) {
            ParticleSubject& subject = shapes[i];

            //            char warpedname[128];
            //            sprintf(warpedname, "warped%d_%03d.nrrd", i, system->currentIteration);
            //            io.WriteImageT(warpedname, warpedImages[i]);

            
            RealImage& warpedImage = *(warpedImages[i].GetPointer());
            GradientImage& gradImage = *(gradImages[i].GetPointer());

            // extract attributes
            RealImage::SizeType radius;
            radius.Fill(nRadius);
            RealImageNeighborhoodIteratorType iiter(radius, subject.realImage, subject.realImage->GetBufferedRegion());
            RealImage::IndexType idx;
#pragma omp parallel for
            for (int j = 0; j < nPoints; j++) {
                Particle& par = subject.m_Particles[j];
                fordim(k) {
                    idx[k] = round(par.z[k]);
                }
                iiter.SetLocation(idx);
                Attr& jAttr = m_attrs(i, j);

                FieldTransformType::InputPointType inputPoint;
                FieldTransformType::OutputPointType outputPoint;
                RealIndex samplePointAtSubjectSpace;
                for (int k = 0; k < Attr::NATTRS; k++) {
                    IntIndex idx = iiter.GetIndex(k);
                    fordim (l) {
                        inputPoint[l] = idx[l];
                    }

                    // mean to shape
                    outputPoint = subject.m_InverseDeformableTransform->TransformPoint(inputPoint);
                    DataReal y[__Dim], x[__Dim];
                    fordim (l) {
                        y[l] = outputPoint[l];
                    }
                    subject.inverseAlignment->TransformPoint(y, x);
                    fordim (l) {
                        samplePointAtSubjectSpace[l] = x[l];
                    }

                    if (!useResampling) {
                        jAttr.x[k] = subject.realSampler->EvaluateAtContinuousIndex(samplePointAtSubjectSpace);
                        GradientPixel grad = subject.gradSampler->EvaluateAtContinuousIndex(samplePointAtSubjectSpace);
                        fordim (u) {
                            jAttr.g[k][u] = grad[u];
                        }
                    } else {
                    // sample intensity at the mean space
                        jAttr.x[k] = warpedImage[idx];
                        fordim (u) {
                            jAttr.g[k][u] = gradImage[idx][u];
                        }
                    }


                }
            }
        }
    }


    void IntensityForce::NormalizeAttributes(AttrMatrix& attrs) {
        // compute average attr value across subject
        const int nPoints = attrs.size2();
        const int nSubj = attrs.size1();

        for (int i = 0; i < nPoints; i++) {
            for (int j = 0; j < Attr::NATTRS; j++) {
                double sum = 0;
                for (int s = 0; s < nSubj; s++) {
                    sum += attrs(s, i).x[j];
                }
                for (int s = 0; s < nSubj; s++) {
                    attrs(s,i).y[j] =  attrs(s, i).x[j] - sum/nSubj;
                }
            }
        }
    }


    bool IntensityForce::ComputeCovariance(AttrMatrix& attrs, int pointIdx, VNLDoubleMatrix& cov, double alpha) {
        const int L = Attr::NATTRS;
        const int S = attrs.size1();
        
        const bool useDualCOV = L > S;
        const int dimCov = useDualCOV ? S : L;
        cov.set_size(dimCov, dimCov);
        cov.fill(0);

        if (useDualCOV) {
            // compute Y'Y
            // Y : (l x s) matrix
            // result: s x s matrix
            // SxL LxS
            // loop in the order of S S L
            for (int j = 0; j < S; j++) {
                Attr& jattr = attrs(j,pointIdx);
                for (int k = j; k < S; k++) {
                    Attr& kattr = attrs(k,pointIdx);
                    for (int l = 0; l < L; l++) {
                        cov[j][k] += jattr.y[l] * kattr.y[l];
                    }
                    cov[k][j] = cov[j][k];
                }
            }
            cov /= (L - 1);
        } else {
            // compute YY'
            // Y : (L x S) matrix
            // Y' : (S x L) matrix
            // result: L x L matrix

            // loop in the order of L S S L
            for (int j = 0; j < L; j++) {
                for (int k = 0; k < L; k++) {
                    for (int l = 0; l < S; l++) {
                        Attr& lattr = attrs(l,pointIdx);
                        cov[j][k] += lattr.y[j] * lattr.y[k];
                    }
                }
            }
            cov /= (S - 1);
        }

        for (int i = 0; i < dimCov; i++) {
            cov[i][i] = cov[i][i] + alpha;
        }

        return useDualCOV;
    }

    void IntensityForce::ComputeGradient(AttrMatrix& attrs, VNLDoubleMatrix& invC, int pointIdx, bool useDual) {
        const int L = Attr::NATTRS;
        const int S = attrs.size1();

        if (useDual) {
            // invC: S x S
            // Y: L x S
            // grad = Y x invC
            // multply inverse of covariance and compute gradient of entropy
            for (int j = 0; j < S; j++) {
                Attr& jattr = attrs(j,pointIdx);
                fordim (k) {
                    jattr.f[k] = 0;
                }
                for (int l = 0; l < Attr::NATTRS; l++) {
                    jattr.x[l] = 0;
                    for (int k = 0; k < S; k++) {
                        Attr& kattr = attrs(k,pointIdx);
                        jattr.x[l] += kattr.y[l] * invC[k][j];
                    }
                    fordim (k) {
                        jattr.f[k] += jattr.x[l] * jattr.g[l][k];
                    }
                }
            }
        } else {
            // invC: L x L
            // Y: L x S
            // grad =  invC x Y
            // LxS * SxL
            // loop over L S L
            // initialize force for particle
            for (int s = 0; s < S; s++) {
                fordim (k) {
                    attrs(s, pointIdx).f[k] = 0;
                }
            }

            for (int j = 0; j < L; j++) {
                for (int l = 0; l < S; l++) {
                    double o = 0;
                    Attr& attr = attrs(l, pointIdx);
                    for (int k = 0; k < L; k++) {
                        o += invC[j][k] * attr.y[l];
                    }
                    attr.x[j] = o;
                    fordim (k) {
                        attr.f[k] += attr.x[l] * attr.g[l][k];
                    }
                }
            }
        }
    }

    void IntensityForce::ComputeIntensityForce(ParticleSystem* system) {
        ParticleSubjectArray& shapes = system->GetSubjects();
        const int nSubj = shapes.size();
        const int nPoints = shapes[0].GetNumberOfPoints();

        ComputeAttributes(system);

        // assume all attributes are computed already
        assert(m_attrs.size1() == nSubj && m_attrs.size2() == nPoints);

        NormalizeAttributes(m_attrs);

#pragma omp parallel for
        for (int i = 0; i < nPoints; i++) {
            // covariance matrix
            VNLDoubleMatrix cov;
            bool useDual = ComputeCovariance(m_attrs, i, cov);
            VNLDoubleMatrix invC = vnl_matrix_inverse<double>(cov);
            ComputeGradient(m_attrs, invC, i, useDual);
        }

        // compute force at subject space
        for (int i = 0; i < nSubj; i++) {
            if (useAttributesAtWarpedSpace) {
                FieldTransformType::Pointer fieldTransform = shapes[i].m_InverseDeformableTransform;
                for (int j = 0; j < nPoints; j++) {
                    FieldTransformType::InputPointType y;
                    fordim(k) {
                        y[k] = shapes[i][j].y[k];
                    }
                    FieldTransformType::JacobianType jac;
                    jac.set_size(__Dim, __Dim);
                    fieldTransform->ComputeInverseJacobianWithRespectToPosition(y, jac);
                    Attr& attr = m_attrs(i,j);
                    fordim(k) {
                        DataReal ff = 0;
                        if (__Dim == 3) {
                            ff = jac[0][k]*attr.f[k] + jac[1][k]*attr.f[k] + jac[2][k]*attr.f[k];
                        } else if (__Dim == 2) {
                            ff = jac[0][k]*attr.f[k] + jac[1][k]*attr.f[k];
                        }
                        attr.F[k] = ff;
                    }
                }
            } else {
                for (int j = 0; j < nPoints; j++) {
                    forcopy (m_attrs(i,j).f, m_attrs(i,j).F);
                }
            }
        }
//        __showcmd(attrs, nSubj, nPoints, cout << " " << m_attrs(_,__).F[0] << "," << m_attrs(_,__).F[1]);


        for (int i = 0; i < nSubj; i++) {
            ParticleSubject& subj = shapes[i];
            for (int j = 0; j < nPoints; j++) {
                Particle& par = subj[j];
                VNLVector ff(__Dim);
                fordim (k) {
                    ff[k] = m_attrs(i,j).F[k];
                }
//                ff.normalize();
                subj.inverseAlignment->TransformVector(ff.data_block(), m_attrs(i,j).F);
#ifndef BATCH
                if (::abs(m_attrs(i,j).F[0]) > 2 || ::abs(m_attrs(i,j).F[1]) > 2 || ::abs(m_attrs(i,j).F[2]) > 2) {
                    //cout << "too big intensity term! at " << i << " subj " << j << " points: x=" << par.x[0]<< "," << par.x[1] << "," << par.x[2] <<  ";f=" << m_attrs(i,j).F[0] << "," << m_attrs(i,j).F[1] << "," << m_attrs(i,j).F[2] << endl;
                    cout << i << "." << j << " ";
                    ff.copy_in(m_attrs(i,j).F);
                    ff.normalize();
                    fordim (k) {
                        m_attrs(i,j).F[k] = ff[k];
                    }
                }
#endif
                par.AddForce(m_attrs(i,j).F, -coeff);
            }
        }
    }
}
