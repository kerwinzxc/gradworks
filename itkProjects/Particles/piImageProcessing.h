//
//  myImageProcessing.h
//  ParticlesGUI
//
//  Created by Joohwi Lee on 2/11/13.
//
//

#ifndef __ParticlesGUI__myImageProcessing__
#define __ParticlesGUI__myImageProcessing__

#include <iostream>
#include "piOptions.h"
#include "piParticle.h"
#include "piImageDef.h"
#include "itkRescaleIntensityImageFilter.h"
#include <itkImageToHistogramFilter.h>
#include <itkLabelGeometryImageFilter.h>

class vtkPolyData;


using namespace std;

namespace pi {

    typedef itk::Statistics::ImageToHistogramFilter<RealImage> ImageHistogramFilterType;

    class LabelSimilarityScore {
    public:
        int L;
        int A;
        int B;
        int AB;
        double minDist;
        double maxDist;
        int total() const { return (A+B+2*AB); }
        double dice() const { return (2.0*AB)/(A+B+2*AB+0.0); }
        double overlap() const { return AB / total(); }
        LabelSimilarityScore() {
            L = A = B = AB = 0;
            minDist = maxDist = 0;
        }
    };
    typedef std::vector<LabelSimilarityScore> LabelScoreVector;

    class AtlasSimilarityScore {
    public:
        LabelScoreVector labelMap;
        void Compute(LabelImage::Pointer a, LabelImage::Pointer b);
        void Add(LabelPixel a, LabelPixel b);
        LabelSimilarityScore& operator()(int l) {
            return labelMap[l];
        }
    };

    ostream& operator<<(ostream& os, const LabelSimilarityScore& score);
    ostream& operator<<(ostream& os, const AtlasSimilarityScore& score);

    class ImageProcessing {
    public:

#pragma mark -
        /// command-line utilities
        /// only when no option is related, it will return back, otherwise it will terminate.
        void main(Options& opts, StringVector& args);
        void doGaussian(Options& opts, StringVector& args);
        void doBlur2(Options& opts, StringVector& args);
        void doEllipse(Options& opts, StringVector& args);
        void doGradMag(Options& opts, StringVector& args);
        void doTransform2(Options& opts, StringVector& args);
        void computeBoundingBox(Options& opts, StringVector& args);
        void doCrop(Options& opts, StringVector& args);
        void doSlice(Options& opts, StringVector& args);
        void deformImage(Options& opts, StringVector& args);

        

        /// perform affine registration (translation + rotation + scaling)
        /// using NEWUOA
        void doAffineReg(Options& opts, StringVector& args);

        // print gradient histogram
        void doGradHist(Options& opts, StringVector& args);

        // test gradient histogram based registration
        void testGradHistReg(Options& opts, StringVector& args);

        void testDisplacementField(Options& opts, StringVector& args);

#pragma mark -
        /// algorithm functions
        RealImageVector ComputeImagePyramid(RealImage::Pointer img, int level = 1);
        LabelImageVector ComputeImagePyramid(LabelImage::Pointer label, int level = 1);

        // anti-aliasing, connected component, and closing morphology
        LabelImage::Pointer SmoothLabelMap(LabelImage::Pointer img);
        LabelImage::Pointer ErodedBorder(LabelImage::Pointer img);
        
        GradientImage::Pointer ComputeGaussianGradient(LabelImage::Pointer img, double sigma = -1);
        GradientImage::Pointer ComputeGradient(LabelImage::Pointer img);

        GradientImage::Pointer ComputeGaussianGradient(RealImage::Pointer img, double sigma = -1);
        GradientImage::Pointer ComputeGradient(RealImage::Pointer img);

        RealImage::Pointer ComputeMagnitudeMap(VectorImage::Pointer img);
        RealImage::Pointer ComputeMagnitudeMap(GradientImage::Pointer img);
        RealImage2::Pointer ComputeMagnitude2Map(VectorImage2::Pointer img);

        LabelImage::Pointer ComputeZeroCrossing(LabelImage::Pointer img);
        
        RealImage::Pointer ComputeGaussianSmoothing(RealImage::Pointer img, double sigma = -1);

        VectorImage::Pointer ComputeDistanceMap(LabelImage::Pointer img);

        LabelImage::Pointer ThresholdToBinary(LabelImage::Pointer img);
        RealImage::Pointer ComputeGaussianGradientMagnitude(RealImage::Pointer img, double sigma = -1);

        LabelImage::Pointer ExtractLabelFilter(LabelImage::Pointer img, int label);
        
        RealImage::Pointer ProcessFeatureDensityImage(RealImage::Pointer realImage, float kernelSigma, float repulsionSigma);

        RealImage::Pointer ProcessFeatureDensityImage(RealImage::Pointer realImage, LabelImage::Pointer maskImage, float kernelSigma, float regularizationSigma);


        LabelImage::Pointer Ellipse(int* outputSize, double* center, double* radius);
        vtkPolyData* ConvertToMesh(LabelImage::Pointer image);
        RealImage::Pointer NormalizeIntensity(RealImage::Pointer image, LabelImage::Pointer label, double percentile = 0);
        LabelImage::Pointer NormalizeToIntegralType(RealImage::Pointer src, LabelPixel min, LabelPixel max, LabelImage::Pointer label);

        LabelImage::Pointer ZeroCrossing(LabelImage::Pointer src);
        ImageHistogramFilterType::HistogramPointer ComputeHistogram(RealImage::Pointer real, int nbin, DataReal rmin, DataReal rmax);
        string ComputeHistogramToString(RealImage::Pointer real, int nbin, DataReal rmin, DataReal rmax);

        RealImage::Pointer deformImage(RealImage::Pointer input, DisplacementFieldType::Pointer displacement, RealImage::Pointer refImage);

        template <class T>
        typename T::Pointer TransformImage(typename T::Pointer srcImg, std::string transform) {

        }
        
        template <class T>
        typename T::Pointer RescaleIntensity(typename T::Pointer srcImg, typename T::PixelType min, typename T::PixelType max) const {
            typedef itk::RescaleIntensityImageFilter<T> RescaleFilter;
            typename RescaleFilter::Pointer filter = RescaleFilter::New();
            filter->SetInput(srcImg);
            filter->SetOutputMinimum(min);
            filter->SetOutputMaximum(max);
            filter->Update();
            return filter->GetOutput();
        }
    };

}
#endif /* defined(__ParticlesGUI__myImageProcessing__) */
