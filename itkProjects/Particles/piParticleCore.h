//
//  ImageViewManager.h
//  laplacePDE
//
//  Created by Joohwi Lee on 11/13/12.
//
//

#ifndef __myParticlesCore__
#define __myParticlesCore__

#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <itkOffset.h>
#include <vnl/vnl_matrix_fixed.h>
#include "piImageDef.h"
#include "piOptions.h"
#include "vtkSmartPointer.h"
#include "vtkPoints.h"
#include "vtkTransform.h"
#include "vtkMatrix4x4.h"
#include "piParticle.h"

class vtkPolyData;

namespace pi {

    typedef vtkSmartPointer<vtkTransform> vtkTransformType;
    typedef vtkSmartPointer<vtkPoints> vtkPointsType;
    typedef vtkSmartPointer<vtkMatrix4x4> vtkMatrixType;

    class ImageContext;
    class ParticleSystem;

    // utility classes
    class ParticleXCaster {
    public:
        inline float castSource(const Particle& p, int i) const { return p.x[i]; }
        inline float castTarget(const Particle& p, int i) const { return p.x[i]; }
    };

    class ParticleYCaster {
    public:
        inline float castSource(const Particle& p, int i) const { return p.y[i]; }
        inline float castTarget(const Particle& p, int i) const { return p.y[i]; }
    };

    class ParticleZCaster {
    public:
        inline float castSource(const Particle& p, int i) const { return p.z[i]; }
        inline float castTarget(const Particle& p, int i) const { return p.z[i]; }
    };
    
    class ParticleYZCaster {
    public:
        inline float castSource(const Particle& p, int i) const { return p.y[i]; }
        inline float castTarget(const Particle& p, int i) const { return p.z[i]; }
    };

    ostream& operator<<(ostream& os, const vtkTransformType& par);
    istream& operator>>(istream& is, vtkTransformType& par);

    typedef boost::numeric::ublas::vector<Particle> ParticleArray;

    void createParticles(ParticleVector&, int subj, int n);

    class ParticleSubject {
    public:
        int m_SubjId;
        std::string m_Name;
        ParticleArray m_Particles;

        RealImage::Pointer kappaImage;
        LinearImageInterpolatorType::Pointer kappaSampler;

        // Label sampler for multi-phase internal force
        LabelImage::Pointer friendImage;
        NNLabelInterpolatorType::Pointer friendSampler;

        RealImage::Pointer realImage;
        LinearImageInterpolatorType::Pointer realSampler;

        GradientImage::Pointer gradImage;
        GradientInterpolatorType::Pointer gradSampler;

        // vtk related variables
        vtkTransformType alignment;
        vtkTransformType inverseAlignment;
        vtkPointsType pointscopy;
//        AffineTransformType::Pointer m_AffineTransform;

        // deformable transforms
        FieldTransformType::Pointer m_DeformableTransform;
        FieldTransformType::Pointer m_InverseDeformableTransform;



        ParticleSubject();
        ParticleSubject(int subjid, int npoints);
        ~ParticleSubject();

        inline const int size() const { return m_Particles.size(); }
        inline const int GetNumberOfPoints() const { return m_Particles.size(); }
        inline int GetNumberOfPoints() { return m_Particles.size(); }

        void Clear();
        void Zero();
        void NewParticles(int nPoints);
        void InitializeRandomPoints(LabelImage::Pointer labelImage);
        void Initialize(int subj, std::string name, int nPoints);
        void Initialize(int subj, std::string name, const ParticleSubject& shape);
        void Initialize(const ParticleArray& array);
        void SyncPointsCopy();

        void ComputeAlignment(ParticleSubject& subj, bool useSimilarity = false);
        void ComputeDensity();

        void AlignmentTransformX2Y();
        void TransformX2Y(TransformType* transform = NULL);
        void TransformY2X(TransformType* transform = NULL);
        void TransformX2X(TransformType* transform);
        void TransformY2Y(TransformType* transform);
        void TransformY2Z(TransformType* transform = NULL);
        void TransformZ2Y(TransformType* transform = NULL);


        void ReadParticlePositions(std::istream& is, int nPoints);
        void WriteParticlePositions(std::ostream& os);
        void ReadParticles(std::istream& is, int nPoints);
        void WriteParticles(std::ostream& os);
        void ReadAlignment(std::istream& is);
        void WriteAlignment(std::ostream& os);

        inline Particle& operator[](int i) {
            return m_Particles[i];
        }

        inline const Particle& operator[](int i) const {
            return m_Particles[i];
        }
    };
    typedef boost::numeric::ublas::vector<ParticleSubject> ParticleSubjectArray;


    class ImageContext {
        friend class ParticleSystem;
    public:
        LabelVector m_LabelImages;

        void LoadLabel(std::string filename);
        void LoadRealImage(std::string filename);

        int ComputeIntersection();
        void ComputeDistanceMaps();
        LabelImage::Pointer GetLabel(int j);
        RealImage::Pointer GetRealImage(int j);
        LabelImage::Pointer GetIntersection();
        void SetIntersection(LabelImage::Pointer intersection);
        OffsetImage GetDistanceMap(int j);
        StringVector& GetRealImageFileNames();
        StringVector& GetFileNames();
        LabelVector& GetLabelVector();
        RealImageVector& GetRealImageVector();
        RealImageVector& GetKappaImages();

        void Clear();
        int Count();

    private:
        StringVector m_RealImageFileNames;
        StringVector m_FileNames;
        LabelImage::Pointer m_Intersection;
        RealImageVector m_Images;
        OffsetImageVector m_DistanceMaps;
        std::string m_IntersectionOutput;
    };

    class ParticleSystem {
    public:
        DataReal currentTime;
        int currentIteration;

        // FIXME: image energy added
        VNLVector ImageEnergy;

        ParticleSystem();
        ~ParticleSystem() {
        }

        const int size() const { return m_Subjects.size(); }
        const int points() const { return size() > 0 ? m_Subjects[0].m_Particles.size() : 0; }
        int GetNumberOfSubjects();
        int GetNumberOfParticles();

        void InitializeSystem(Options& options);
        void InitializeSystem(int nsubjs, int nparticles);
        void LoadKappaImages(Options& options, ImageContext* context);

        ParticleSubject& GetInitialSubject();
        ParticleSubject& InitializeMean();
        ParticleSubject& ComputeXMeanSubject();
        ParticleSubject& ComputeYMeanSubject();
        ParticleSubject& ComputeZMeanSubject();

        ParticleSubject& GetMeanSubject();
        ParticleSubjectArray& GetSubjects();
        
        Options& GetSystemOptions() {
            return (*m_Options);
        }

        inline ParticleSubject& operator[](int j) {
            return m_Subjects[j];
        }

        inline const ParticleSubject& operator[](int j) const {
            return m_Subjects[j];
        }

    private:
        ParticleSubject m_MeanSubject;
        ParticleSubject m_InitialSubject;
        ParticleSubjectArray m_Subjects;
        
        Options* m_Options;
    };


    void export2vtk(ParticleSubject& sub, const char* vtkname, int field);
    vtkPolyData* convert2vtk(ParticleArray& parray);
}

#endif
