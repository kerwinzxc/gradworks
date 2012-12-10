//
//  BSplineRegistration.cpp
//  ParticlesGUI
//
//  Created by Joohwi Lee on 12/6/12.
//
//

#include "myBSplineRegistration.h"
#include "QElapsedTimer"
#include "itkWarpImageFilter.h"
#include "itkVectorMagnitudeImageFilter.h"
#include "iostream"

// estimation of displacement field via particle correspondence

using namespace std;

namespace my {
    typedef itk::WarpImageFilter<SliceType, SliceType, DisplacementFieldType> WarpImageFilterType;

    BSplineRegistration::BSplineRegistration() {
    }

    BSplineRegistration::~BSplineRegistration() {

    }

    void BSplineRegistration::SetReferenceImage(SliceType::Pointer refImage) {
        m_RefImage = refImage;
    }

    void BSplineRegistration::SetLandmarks(int n, double *src, double *dst) {
        if (m_FieldPoints.IsNull()) {
            m_FieldPoints = DisplacementFieldPointSetType::New();
        }
        m_FieldPoints->Initialize();

        // create point structures
        PointSetType::Pointer srcPoints = PointSetType::New();
        PointSetType::Pointer dstPoints = PointSetType::New();

        srcPoints->Initialize();
        dstPoints->Initialize();

        double* pSrc = src;
        double* pDst = dst;
        for (int i = 0; i < n; i++) {
            PointSetType::PointType iPoint;
            iPoint[0] = pSrc[0];
            iPoint[1] = pSrc[1];

            VectorType vector;
            vector[0] = pDst[0] - pSrc[0];
            vector[1] = pDst[1] - pSrc[1];
            m_FieldPoints->SetPoint(i, iPoint);
            m_FieldPoints->SetPointData(i, vector);
            pSrc += 2;
            pDst += 2;
        }
    }

    void BSplineRegistration::Update() {
        int splineOrder = 3;
        int numOfLevels = 1;
        int nSize = 25;

        BSplineFilterType::Pointer bspliner = BSplineFilterType::New();
        BSplineFilterType::ArrayType numControlPoints;
        numControlPoints.Fill(nSize + splineOrder);

        SliceType::SizeType imageSize = m_RefImage->GetBufferedRegion().GetSize();
        SliceType::SpacingType imageSpacing = m_RefImage->GetSpacing();
        SliceType::PointType imageOrigin = m_RefImage->GetOrigin();

        cout << "Image Size: " << imageSize << endl;
        cout << "Image Spacing: " << imageSpacing << endl;
        cout << "Image Origin: " << imageOrigin << endl;
        cout << "# control points: " << numControlPoints << endl;

        try {
            // debug: reparameterized point component is outside
            QElapsedTimer timer;
            timer.start();
            bspliner->SetOrigin(imageOrigin);
            bspliner->SetSpacing(imageSpacing);
            bspliner->SetSize(imageSize);
            bspliner->SetGenerateOutputImage(true);
            bspliner->SetNumberOfLevels(numOfLevels);
            bspliner->SetSplineOrder(splineOrder);
            bspliner->SetNumberOfControlPoints(numControlPoints);
            bspliner->SetInput(m_FieldPoints);
            bspliner->Update();
            m_PhiLattice = bspliner->GetPhiLattice();
            m_DisplacementField = bspliner->GetOutput();
            cout << "BSpline Update Time: " << timer.elapsed() << endl;
        } catch (itk::ExceptionObject& e) {
            e.Print(std::cout);
        }
    }

    SliceType::Pointer BSplineRegistration::WarpImage(SliceType::Pointer srcImage) {
        if (m_DisplacementField.IsNull()) {
            return SliceType::Pointer(NULL);
        }
        WarpImageFilterType::Pointer warpFilter = WarpImageFilterType::New();
        warpFilter->SetInput(srcImage);
        warpFilter->SetDisplacementField(m_DisplacementField);
//        warpFilter->SetOutputSpacing(srcImage->GetSpacing());
//        warpFilter->SetOutputOrigin(srcImage->GetOrigin());
//        warpFilter->SetOutputSize(srcImage->GetBufferedRegion().GetSize());
        warpFilter->SetOutputParametersFromImage(srcImage);
        warpFilter->Update();
        return warpFilter->GetOutput();
    }

    DisplacementFieldType::Pointer BSplineRegistration::GetDisplacementField() {
        return m_DisplacementField;
    }

    DisplacementFieldType::Pointer BSplineRegistration::GetControlPoints() {
        return m_PhiLattice;
    }

    SliceType::Pointer BSplineRegistration::GetDisplacementMagnitude() {
        if (m_DisplacementField.IsNull()) {
            return SliceType::Pointer(NULL);
        }

        typedef itk::VectorMagnitudeImageFilter<DisplacementFieldType, SliceType> VectorFilterType;
        VectorFilterType::Pointer filter = VectorFilterType::New();
        filter->SetInput(m_DisplacementField);
        filter->Update();
        return filter->GetOutput();
    }

}