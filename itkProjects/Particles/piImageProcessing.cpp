//
//  myImageProcessing.cpp
//  ParticlesGUI
//
//  Created by Joohwi Lee on 2/11/13.
//
//

#include "piImageProcessing.h"
#include "itkAntiAliasBinaryImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkBinaryBallStructuringElement.h"
#include "itkBinaryCrossStructuringElement.h"
#include "itkBinaryDilateImageFilter.h"
#include "itkBinaryErodeImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkSubtractImageFilter.h"
#include "itkRecursiveGaussianImageFilter.h"
#include "itkGradientRecursiveGaussianImageFilter.h"
#include "itkEllipseSpatialObject.h"
#include "itkSpatialObjectToImageFilter.h"
#include "itkMesh.h"
#include "itkVTKPolyDataWriter.h"
#include "itkUnaryFunctorImageFilter.h"
#include <itkGradientMagnitudeRecursiveGaussianImageFilter.h>
#include "itkSignedDanielssonDistanceMapImageFilter.h"
#include "itkDanielssonDistanceMapImageFilter.h"
#include "itkVectorMagnitudeImageFilter.h"
#include "itkMesh.h"
#include "itkBinaryMask3DMeshSource.h"

#include "itkImageIO.h"

#ifdef DIMENSION3
#include "itkBinaryMask3DMeshSource.h"
#endif

namespace pi {
#ifdef DIMENSION3
    typedef itk::EllipseSpatialObject<__Dim> EllipseType;
    typedef itk::SpatialObjectToImageFilter<EllipseType, LabelImage> SpatialObjectToImageFilterType;
    typedef itk::Mesh<PointReal> MeshType;
    typedef itk::BinaryMask3DMeshSource<LabelImage, MeshType> MeshSourceType;
#endif
    typedef itk::ZeroCrossingImageFilter<LabelImage, LabelImage> ZeroCrossingFilterType;
    typedef itk::CastImageFilter<LabelImage ,RealImage > CastToRealFilterType;
    typedef itk::AntiAliasBinaryImageFilter<RealImage, RealImage> AntiAliasFilterType;
    typedef itk::RescaleIntensityImageFilter<RealImage, LabelImage > RescaleFilter;
    typedef itk::ConnectedComponentImageFilter<LabelImage, LabelImage> ConnectedComponentFilterType;
    typedef itk::RelabelComponentImageFilter<LabelImage, LabelImage> RelabelFilterType;
    typedef itk::BinaryThresholdImageFilter<LabelImage, LabelImage> BinaryThreshFilterType;
    typedef itk::BinaryBallStructuringElement<LabelImage::PixelType,__Dim> StructuringElementType;
    typedef itk::BinaryCrossStructuringElement<LabelImage::PixelType,__Dim> AsymStructuringElementType;
    typedef itk::BinaryDilateImageFilter<LabelImage, LabelImage, StructuringElementType> DilateFilterType;
    typedef itk::BinaryErodeImageFilter<LabelImage, LabelImage, StructuringElementType> ErodeFilterType;
    typedef itk::SubtractImageFilter<LabelImage, LabelImage, LabelImage> SubtractFilterType;
    typedef itk::RecursiveGaussianImageFilter<RealImage> GaussianFilterType;
    typedef itk::SignedDanielssonDistanceMapImageFilter<LabelImage, RealImage> SignedDistanceMapFilterType;
    typedef itk::DanielssonDistanceMapImageFilter<LabelImage, RealImage> DistanceMapFilterType;
    typedef DistanceMapFilterType::VectorImageType DistanceVectorImageType;
    typedef DistanceVectorImageType::PixelType DistancePixelType;
    typedef itk::VectorMagnitudeImageFilter<VectorImage, RealImage> GradientMagnitudeFilterType;

    class OffsetToVector {
    public:
        bool operator!=(const OffsetToVector& o) {
            return false;
        }
        bool operator==(const OffsetToVector& o) {
            return true;
        }
        inline VectorType operator()(SignedDistanceMapFilterType::VectorImageType::PixelType o) {
            VectorType v;
            fordim (k) {
                v[k] = o[k];
            }
            return v;
        }
    };

    LabelImage::Pointer ImageProcessing::SmoothLabelMap(LabelImage::Pointer img) {
        cout << "Computing label map smoothing ..." << flush;

        LabelImage::Pointer procImage;

        const int LABEL_VAL = 255;
        CastToRealFilterType::Pointer toReal = CastToRealFilterType::New();
        toReal->SetInput(img);
        toReal->Update();

        AntiAliasFilterType::Pointer antiAliasFilter = AntiAliasFilterType::New();
        antiAliasFilter->SetInput( toReal->GetOutput() );
        antiAliasFilter->SetMaximumRMSError(1);
        antiAliasFilter->SetNumberOfIterations(10);
        antiAliasFilter->SetNumberOfLayers(2);
        antiAliasFilter->Update();

//        itkcmds::itkImageIO<RealImage> iox;
//        iox.WriteImageT("/tmpfs/aadouble.nrrd", antiAliasFilter->GetOutput());

        RescaleFilter::Pointer rescale = RescaleFilter::New();
        rescale->SetInput( antiAliasFilter->GetOutput() );
        rescale->SetOutputMinimum(   0 );
        rescale->SetOutputMaximum( 255 );
        rescale->Update();
        procImage = rescale->GetOutput();

        BinaryThreshFilterType::Pointer binTreshFilter = BinaryThreshFilterType::New();
        binTreshFilter->SetInput(rescale->GetOutput());
        binTreshFilter->SetUpperThreshold(255);
        binTreshFilter->SetLowerThreshold(127);
        binTreshFilter->SetOutsideValue (0);
        binTreshFilter->SetInsideValue (1);
        binTreshFilter->Update();
        procImage = binTreshFilter->GetOutput();

        ConnectedComponentFilterType::Pointer concompFilter = ConnectedComponentFilterType::New();
        RelabelFilterType::Pointer relabelFilter = RelabelFilterType::New();
        BinaryThreshFilterType::Pointer binTreshFilter2 = BinaryThreshFilterType::New();
        concompFilter->SetInput(procImage);
        concompFilter->Update();
        procImage = concompFilter->GetOutput();

        relabelFilter->SetInput(concompFilter->GetOutput());
        relabelFilter->Update();
        procImage = relabelFilter->GetOutput();

        binTreshFilter2->SetInput(relabelFilter->GetOutput());
        binTreshFilter2->SetUpperThreshold(1);
        binTreshFilter2->SetLowerThreshold(1);// only largest object (labels are sorted largest to smallest)
        binTreshFilter2->SetOutsideValue (0);
        binTreshFilter2->SetInsideValue (LABEL_VAL);
        binTreshFilter2->Update();
        procImage   = binTreshFilter2->GetOutput();

        StructuringElementType structuringElement;
        structuringElement.SetRadius(1);  // 3x3x3 structuring element
        structuringElement.CreateStructuringElement();

        DilateFilterType::Pointer dilateFilter = DilateFilterType::New();
        ErodeFilterType::Pointer erodeFilter = ErodeFilterType::New();

        dilateFilter->SetDilateValue (LABEL_VAL);
        dilateFilter->SetKernel(structuringElement);
        dilateFilter->SetInput(procImage);
        dilateFilter->Update();

        erodeFilter->SetErodeValue(LABEL_VAL);
        erodeFilter->SetKernel(structuringElement);
        erodeFilter->SetInput(dilateFilter->GetOutput());
        erodeFilter->Update();

        procImage = erodeFilter->GetOutput();

        cout << "done" << endl;
        return procImage;
    }

    LabelImage::Pointer ImageProcessing::ErodedBorder(pi::LabelImage::Pointer img) {
        LabelImage::Pointer procImage;

        StructuringElementType structuringElement;
        structuringElement.SetRadius(1);  // 3x3x3 structuring element
        structuringElement.CreateStructuringElement();

        ErodeFilterType::Pointer erodeFilter = ErodeFilterType::New();
        erodeFilter->SetErodeValue(255);
        erodeFilter->SetKernel(structuringElement);
        erodeFilter->SetInput(img);
        erodeFilter->Update();
        procImage = erodeFilter->GetOutput();

        SubtractFilterType::Pointer subFilter = SubtractFilterType::New();
        subFilter->SetInput1(img);
        subFilter->SetInput2(procImage);
        subFilter->Update();

        return subFilter->GetOutput();
    }

    GradientImage::Pointer ImageProcessing::ComputeGaussianGradient(LabelImage::Pointer img, double sigma) {
        CastToRealFilterType::Pointer toReal = CastToRealFilterType::New();
        toReal->SetInput(img);
        toReal->Update();

        GaussianGradientFilterType::Pointer gradientFilter = GaussianGradientFilterType::New();
        gradientFilter->SetInput(toReal->GetOutput());
        if (sigma > -1) {
            gradientFilter->SetSigma(sigma);
        }
        gradientFilter->Update();
        return gradientFilter->GetOutput();
    }
    
    GradientImage::Pointer ImageProcessing::ComputeGradient(LabelImage::Pointer img) {
        CastToRealFilterType::Pointer toReal = CastToRealFilterType::New();
        toReal->SetInput(img);
        toReal->Update();

        GradientFilterType::Pointer gradientFilter = GradientFilterType::New();
        gradientFilter->SetInput(toReal->GetOutput());
        gradientFilter->Update();
        return gradientFilter->GetOutput();
    }

    GradientImage::Pointer ImageProcessing::ComputeGaussianGradient(RealImage::Pointer img, double sigma) {
        GaussianGradientFilterType::Pointer gradientFilter = GaussianGradientFilterType::New();
        gradientFilter->SetInput(img);
        if (sigma > -1) {
            gradientFilter->SetSigma(sigma);
        }
        gradientFilter->Update();
        return gradientFilter->GetOutput();
    }

    GradientImage::Pointer ImageProcessing::ComputeGradient(RealImage::Pointer img) {
        GradientFilterType::Pointer gradientFilter = GradientFilterType::New();
        gradientFilter->SetInput(img);
        gradientFilter->Update();
        return gradientFilter->GetOutput();
    }

    RealImage::Pointer ImageProcessing::ComputeGaussianGradientMagnitude(RealImage::Pointer img, double sigma) {
        typedef itk::GradientMagnitudeRecursiveGaussianImageFilter<RealImage> FilterType;
        FilterType::Pointer filter = FilterType::New();
        if (sigma > 0) {
            filter->SetSigma(sigma);
        }
        filter->SetInput(img);
        filter->Update();
        return filter->GetOutput();
    }

    LabelImage::Pointer ImageProcessing::Ellipse(int* outputSize, double *center, double *radius) {
#ifdef DIMENSION3
        SpatialObjectToImageFilterType::Pointer imageFilter = SpatialObjectToImageFilterType::New();
        RealImage::SizeType size;
        fordim (k) {
            size[k] = outputSize[k];
        }
        imageFilter->SetSize(size);

        EllipseType::Pointer ellipse = EllipseType::New();
        ellipse->SetDefaultInsideValue(255);
        ellipse->SetDefaultOutsideValue(0);

        EllipseType::ArrayType axes;
        fordim (k) {
            axes[k] = radius[k];
        }
        ellipse->SetRadius(axes);

        EllipseType::TransformType::Pointer transform = EllipseType::TransformType::New();
        transform->SetIdentity();
        TransformType::OutputVectorType translation;
        fordim (k) {
            translation[k] = center[k];
        }
        transform->Translate(translation, false);

        ellipse->SetObjectToParentTransform(transform);
        imageFilter->SetInput(ellipse);
        imageFilter->SetUseObjectValue(true);
        imageFilter->SetOutsideValue(0);
        imageFilter->Update();
        return imageFilter->GetOutput();
#else
        return LabelImage::Pointer();
#endif
    }


    VectorImage::Pointer ImageProcessing::DistanceMap(LabelImage::Pointer img) {
        cout << "Computing distance map ..." << flush;
        // create binary image for a mask for a correct distance map
        BinaryThreshFilterType::Pointer binThreshFilter = BinaryThreshFilterType::New();
        binThreshFilter->SetInput(img);
        binThreshFilter->SetInsideValue(1);
        binThreshFilter->SetOutsideValue(0);
        binThreshFilter->SetLowerThreshold(1);
        binThreshFilter->SetUpperThreshold(255);
        LabelImage::Pointer binaryMap = binThreshFilter->GetOutput();

        // construct signed distance filter
        SignedDistanceMapFilterType::Pointer distmapFilter = SignedDistanceMapFilterType::New();
        distmapFilter->SetInput(binaryMap);
        distmapFilter->Update();
        SignedDistanceMapFilterType::OutputImagePointer distmap = distmapFilter->GetOutput();

        typedef
        itk::UnaryFunctorImageFilter<SignedDistanceMapFilterType::VectorImageType, VectorImage, OffsetToVector> OffsetToVectorCastFilterType;

        OffsetToVectorCastFilterType::Pointer caster = OffsetToVectorCastFilterType::New();
        caster->SetInput(distmapFilter->GetVectorDistanceMap());
        caster->Update();
        cout << "done" << endl;
        return caster->GetOutput();
    }

    RealImage::Pointer ImageProcessing::ComputeMagnitudeMap(VectorImage::Pointer img) {
        GradientMagnitudeFilterType::Pointer magFilter = GradientMagnitudeFilterType::New();
        magFilter->SetInput(img);
        magFilter->Update();
        return magFilter->GetOutput();
    }

    RealImage::Pointer ImageProcessing::ComputeMagnitudeMap(GradientImage::Pointer img) {
        typedef itk::VectorMagnitudeImageFilter<GradientImage, RealImage> MagnitudeFilterType;
        MagnitudeFilterType::Pointer magFilter = MagnitudeFilterType::New();
        magFilter->SetInput(img);
        magFilter->Update();
        return magFilter->GetOutput();
    }

    vtkPolyData* ImageProcessing::ConvertToMesh(LabelImage::Pointer image) {
#ifdef DIMENSION3
        typedef itk::Mesh<double> MeshType;
        typedef itk::BinaryMask3DMeshSource<LabelImage, MeshType> MeshSourceType;

        MeshSourceType::Pointer meshSource = MeshSourceType::New();
        meshSource->SetInput(image);
        meshSource->SetObjectValue(255);
        meshSource->Update();
        MeshType::Pointer mesh = meshSource->GetOutput();
#endif
        return NULL;
    }
    
    RealImage::Pointer ImageProcessing::NormalizeIntensity(RealImage::Pointer image, LabelImage::Pointer label) {
        double sum = 0, sum2 = 0;
        itkcmds::itkImageIO<RealImage> io;
        RealImage::Pointer output = io.NewImageT(image);
        RealImageIteratorType iter(image, image->GetBufferedRegion());
        LabelImageIteratorType iter2(label, image->GetBufferedRegion());
        RealImageIteratorType oiter(output, image->GetBufferedRegion());
        int n = 0;
        iter.GoToBegin();
        iter2.GoToBegin();
        while (!iter.IsAtEnd()) {
            if (iter2.Get() > 0) {
                ImageReal v = iter.Get();
                sum += v;
                sum2 += (v*v);
                ++n;
            }
            ++iter;
            ++iter2;
        }
        double mean = sum/(n-1);
        double sigma  = sqrt(sum2/(n-1) - mean*mean);
        iter.GoToBegin();
        iter2.GoToBegin();
        while (!iter.IsAtEnd()) {
            if (iter2.Get() > 0) {
                oiter.Set((iter.Get() - mean)/sigma);
            }
            ++iter;
            ++iter2;
            ++oiter;
        }
        return output;
    }

}