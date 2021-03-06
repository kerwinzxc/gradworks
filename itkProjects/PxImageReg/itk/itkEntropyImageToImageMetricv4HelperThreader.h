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
#ifndef __itkEntropyImageToImageMetricv4HelperThreader_h
#define __itkEntropyImageToImageMetricv4HelperThreader_h

#include "itkImageToImageMetricv4GetValueAndDerivativeThreader.h"

namespace itk
{

/** \class EntropyImageToImageMetricv4GetValueAndDerivativeThreader
 * \brief Helper class for EntropyImageToImageMetricv4 \c
 * To compute the average pixel intensities of the fixed image and the moving image
 * on the sampled points or inside the virtual image region:
 * \f$ \bar f (EntropyImageToImageMetricv4::m_AverageFix ) \f$
 * \f$ \bar m (EntropyImageToImageMetricv4::m_AverageMov ) \f$
 *
 * \ingroup ITKMetricsv4
 */
template < class TDomainPartitioner, class TImageToImageMetric, class TEntropyMetric >
class EntropyImageToImageMetricv4HelperThreader
  : public ImageToImageMetricv4GetValueAndDerivativeThreader< TDomainPartitioner, TImageToImageMetric >
{
public:
  /** Standard class typedefs. */
  typedef EntropyImageToImageMetricv4HelperThreader                                      Self;
  typedef ImageToImageMetricv4GetValueAndDerivativeThreader< TDomainPartitioner, TImageToImageMetric > Superclass;
  typedef SmartPointer< Self >                                                                         Pointer;
  typedef SmartPointer< const Self >                                                                   ConstPointer;

  itkTypeMacro( EntropyImageToImageMetricv4HelperThreader, ImageToImageMetricv4GetValueAndDerivativeThreader );

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

  typedef typename Superclass::InternalComputationValueType InternalComputationValueType;
  typedef typename Superclass::NumberOfParametersType       NumberOfParametersType;

  typedef typename Superclass::FixedOutputPointType     FixedOutputPointType;
  typedef typename Superclass::MovingOutputPointType    MovingOutputPointType;

protected:
  EntropyImageToImageMetricv4HelperThreader() {}

  /** Overload: Resize and initialize per thread objects. */
  virtual void BeforeThreadedExecution();

  /** Overload:
   * Collects the results from each thread and sums them.  Results are stored
   * in the enclosing class \c m_Value and \c m_DerivativeResult.  Behavior
   * depends on m_AverageValueAndDerivativeByNumberOfValuePoints,
   * m_NumberOfValidPoints, to average the value sum, and to average
   * derivative sums for global transforms only (i.e. transforms without local
   * support).  */
  virtual void AfterThreadedExecution();


  /* Overload: don't need to compute the image gradients and store derivatives
   *
   * Method called by the threaders to process the given virtual point.  This
   * in turn calls \c TransformAndEvaluateFixedPoint, \c
   * TransformAndEvaluateMovingPoint, and \c ProcessPoint.
   */
  virtual bool ProcessVirtualPoint( const VirtualIndexType & virtualIndex,
                                    const VirtualPointType & virtualPoint,
                                    const ThreadIdType threadId );


  /**
   * Not using. All processing is done in ProcessVirtualPoint.
   */
  virtual bool ProcessPoint(
        const VirtualIndexType &          ,
        const VirtualPointType &          ,
        const FixedImagePointType &       ,
        const FixedImagePixelType &       ,
        const FixedImageGradientType &    ,
        const MovingImagePointType &      ,
        const MovingImagePixelType &      ,
        const MovingImageGradientType &   ,
        MeasureType &                     ,
        DerivativeType &                  ,
        const ThreadIdType                 ) const
  {
    return false;
  }

private:
  EntropyImageToImageMetricv4HelperThreader( const Self & ); // purposely not implemented
  void operator=( const Self & ); // purposely not implemented

  /* per thread variables for correlation and its derivatives */
  mutable std::vector< InternalComputationValueType > m_FixSumPerThread;
  mutable std::vector< InternalComputationValueType > m_MovSumPerThread;

  /** Internal pointer to the metric object in use by this threader.
   *  This will avoid costly dynamic casting in tight loops. */
  TEntropyMetric * m_EntropyAssociate;
};

} // end namespace itk

#ifndef ITK_MANUAL_INSTANTIATION
#include "itkEntropyImageToImageMetricv4HelperThreader.hxx"
#endif

#endif
