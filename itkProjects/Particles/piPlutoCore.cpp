//
//  piPlutoCore.cpp
//  ParticleGuidedRegistration
//
//  Created by Joohwi Lee on 5/2/13.
//
//

#include "piPlutoCore.h"

namespace pi {
    ImageIO<RealImage3> __real3IO;
    ImageIO<RealImage2> __real2IO;
    
    PlutoCore::PlutoCore(QObject* parent): QObject(parent) {
        _samples = NULL;
    }

    PlutoCore::~PlutoCore() {
        if (_samples) {
            delete[] _samples;
            _samples = NULL;
        }
    }

    void PlutoCore::setImages(RealImage2Vector images) {
        if (_samples) {
            delete[] _samples;
        }
        _images = images;
        _gradientImages = __realImageTools.computeGaussianGradient(_images, 0.5);
        _samples = new RealSamples2[_images.size()];
    }
    
    void PlutoCore::setInitialParticles(ParticleVector initialParticles) {
        _initialParticles = initialParticles;
    }
    

    void PlutoCore::track(int model, int test) {
        for (int i = 0; i < 100; i++) {
            // 1) Compute data
            _samples[test].sampleValues();
            _samples[test].sampleGradients();

            // 2) Compute gradient from current data
            computeGradient(model, test);
            
            // 3) Update particle positions
            updateParticles(test);
            
            // 4) Repeat until converge
        }
    }

    void PlutoCore::run() {
        
    }
    
    void PlutoCore::initialize() {
        const int radius = 3;
        const int size = 7;
        
        RealImage2::RegionType region;
        region.SetIndex(0, -radius);
        region.SetIndex(1, -radius);
        region.SetSize(0, size);
        region.SetSize(1, size);
        
        _particles.resize(_images.size());
        for (int i = 0; i < _images.size(); i++) {
            // set up each particle per image
            _particles[i] = _initialParticles;
            
            // interpolator creation
            typedef itk::LinearInterpolateImageFunction<RealImage2> InterpolatorType;
            InterpolatorType::Pointer interp = InterpolatorType::New();
            interp->SetInputImage(_images[i]);
            
            Gradient2InterpolatorType::Pointer gradientInterp = Gradient2InterpolatorType::New();
            gradientInterp->SetInputImage(_gradientImages[i]);
            
            // sampler initialization
            RealSamples2& samples = _samples[i];
            samples.setSampleRegion(region);
            samples.addInterpolator(interp);
            samples.addGradientInterpolator(gradientInterp);
            samples.addParticles(&_particles[i][0], _particles[i].size());
            samples.allocateValues();
            samples.allocateGradients();
            samples.sampleValues();
            samples.sampleGradients();
        }
    }
}