/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#ifndef __itkEntropyImageToImageMetricv4GetValueAndDerivativeThreader_h
#define __itkEntropyImageToImageMetricv4GetValueAndDerivativeThreader_h

#include "itkImageToImageMetricv4GetValueAndDerivativeThreader.h"

namespace itk
{

/** \class EntropyImageToImageMetricv4GetValueAndDerivativeThreader
 * \brief Processes points for EntropyImageToImageMetricv4 \c
 * GetValueAndDerivative.
 *
 * \ingroup ITKMetricsv4
 */
template < class TDomainPartitioner, class TImageToImageMetric, class TEntropyMetric >
class EntropyImageToImageMetricv4GetValueAndDerivativeThreader
  : public ImageToImageMetricv4GetValueAndDerivativeThreader< TDomainPartitioner, TImageToImageMetric >
{
public:
  /** Standard class typedefs. */
  typedef EntropyImageToImageMetricv4GetValueAndDerivativeThreader                                      Self;
  typedef ImageToImageMetricv4GetValueAndDerivativeThreader< TDomainPartitioner, TImageToImageMetric > Superclass;
  typedef SmartPointer< Self >                                                                         Pointer;
  typedef SmartPointer< const Self >                                                                   ConstPointer;

  itkTypeMacro( EntropyImageToImageMetricv4GetValueAndDerivativeThreader, ImageToImageMetricv4GetValueAndDerivativeThreader );

  itkNewMacro( Self );

  typedef typename Superclass::DomainType    DomainType;
  typedef typename Superclass::AssociateType AssociateType;

  typedef typename Superclass::ImageToImageMetricv4Type ImageToImageMetricv4Type;
  typedef typename Superclass::VirtualIndexType         VirtualIndexType;
  typedef typename Superclass::VirtualPointType         VirtualPointType;
  typedef typename Superclass::FixedImagePointType      FixedImagePointType;
  typedef typename Superclass::FixedImagePixelType      FixedImagePixelType;
  typedef typename Superclass::FixedImageGradientType   FixedImageGradientType;
  typedef typename Superclass::MovingImagePointType     MovingImagePointType;
  typedef typename Superclass::MovingImagePixelType     MovingImagePixelType;
  typedef typename Superclass::MovingImageGradientType  MovingImageGradientType;
  typedef typename Superclass::MeasureType              MeasureType;
  typedef typename Superclass::DerivativeType           DerivativeType;
  typedef typename Superclass::DerivativeValueType      DerivativeValueType;

  typedef typename ImageToImageMetricv4Type::FixedTransformType      FixedTransformType;
  typedef typename FixedTransformType::OutputPointType               FixedOutputPointType;
  typedef typename ImageToImageMetricv4Type::MovingTransformType     MovingTransformType;
  typedef typename MovingTransformType::OutputPointType              MovingOutputPointType;

  typedef typename Superclass::InternalComputationValueType InternalComputationValueType;
  typedef typename Superclass::NumberOfParametersType       NumberOfParametersType;

protected:
  EntropyImageToImageMetricv4GetValueAndDerivativeThreader() {}

  /** Overload: Resize and initialize per thread objects:
   *    number of valid points
   *    moving transform jacobian
   *    cross-Entropy specific variables
   *  */
  virtual void BeforeThreadedExecution();

  /** Overload:
   * Collects the results from each thread and sums them.  Results are stored
   * in the enclosing class \c m_Value and \c m_DerivativeResult.  Behavior
   * depends on m_AverageValueAndDerivativeByNumberOfValuePoints,
   * m_NumberOfValidPoints, to average the value sum, and to average
   * derivative sums for global transforms only (i.e. transforms without local
   * support).  */
  virtual void AfterThreadedExecution();

  /** Overload to avoid execution of adding entries to m_MeasurePerThread
   * StorePointDerivativeResult() after this function calls ProcessPoint().
   * Method called by the threaders to process the given virtual point.  This
   * in turn calls \c TransformAndEvaluateFixedPoint, \c
   * TransformAndEvaluateMovingPoint, and \c ProcessPoint. */
  virtual bool ProcessVirtualPoint( const VirtualIndexType & virtualIndex,
                                    const VirtualPointType & virtualPoint,
                                    const ThreadIdType threadId );

  /** This function computes the local voxel-wise contribution of
   *  the metric to the global integral of the metric/derivative.
   */
  virtual bool ProcessPoint(const VirtualIndexType &          virtualIndex,
        const VirtualPointType &          virtualPoint,
        const FixedImagePointType &       mappedFixedPoint,
        const FixedImagePixelType &       mappedFixedPixelValue,
        const FixedImageGradientType &    mappedFixedImageGradient,
        const MovingImagePointType &      mappedMovingPoint,
        const MovingImagePixelType &      mappedMovingPixelValue,
        const MovingImageGradientType &   mappedMovingImageGradient,
        MeasureType &                     metricValueReturn,
        DerivativeType &                  localDerivativeReturn,
        const ThreadIdType                threadID ) const;

private:
  EntropyImageToImageMetricv4GetValueAndDerivativeThreader( const Self & ); // purposely not implemented
  void operator=( const Self & ); // purposely not implemented

  /*
   * the per-thread memory for computing the Entropy and its derivatives
   * \bar f (EntropyImageToImageMetricv4::m_AverageFix ) and
   * \bar m (EntropyImageToImageMetricv4::m_AverageMov ), the average pixel
   * intensity, computed using the helper
   * class EntropyHelperImageToImageMetricv4GetValueAndDerivativeThreader.
   * say f_i is the i-th pixel of fixed image, m_i is the i-th pixel of moving
   * image: see the comments below
   */
  struct InternalCumSumType{    // keep cumlative summation over points for:
      InternalComputationValueType fm;  // (f_i - \bar f) * (m_i - \bar m)
      InternalComputationValueType m2;  // (m_i - \bar m)^2
      InternalComputationValueType f2;  // (f_i - \bar m)^2
      InternalComputationValueType m;   // m_i
      InternalComputationValueType f;   // f_i
      DerivativeType fdm; // (f_i - \bar f) * dm_i/dp
      DerivativeType mdm; // (m_i - \bar m) * dm_i/dp
  };

  /* per thread variables for Entropy and its derivatives */
  mutable std::vector< InternalCumSumType > m_InternalCumSumPerThread;

  /** Internal pointer to the metric object in use by this threader.
   *  This will avoid costly dynamic casting in tight loops. */
  TEntropyMetric * m_EntropyAssociate;
};

} // end namespace itk

#ifndef ITK_MANUAL_INSTANTIATION
#include "itkEntropyImageToImageMetricv4GetValueAndDerivativeThreader.hxx"
#endif

#endif
