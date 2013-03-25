#ifndef __PI_IMAGEIO_H__
#define __PI_IMAGEIO_H__

#include "itkImage.h"
#include "itkImageIOBase.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkNumericTraits.h"
#include "itkNrrdImageIOFactory.h"
#include "itkNiftiImageIOFactory.h"
#include "itkImageRegionConstIteratorWithIndex.h"
#include "itkTransformFileReader.h"
#include "itkTransformFileWriter.h"
#include "itkResampleImageFilter.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkCastImageFilter.h"
#include "itkExceptionObject.h"
#include "itkImportImageFilter.h"
#include <itkImageDuplicator.h>
#include "itkMath.h"
#include "iostream"
#include "vector"

#ifndef DIMENSIONS
#define DIMENSIONS 3
#endif

using namespace std;

namespace pi {
    class ImageInfo {
    public:
        int numdims;
        int numcomponents;
        itk::ImageIOBase::IOComponentType componenttypeid;
        itk::ImageIOBase::IOPixelType pixeltypeid;
        std::string componenttype;
        std::string pixeltype;
        std::vector<int> dimensions;
        std::vector<double> spacing;
        std::vector<double> origin;
        std::vector<std::vector<double> > direction;
    };

	template <typename T>
	class ImageIO {
	private:
		itk::ImageIOBase::IOPixelType _pixelType;
		itk::ImageIOBase::IOComponentType _componentType;
        
	public:
		typedef typename T::Pointer ImagePointer;
		typedef typename T::PixelType ImagePixel;
		typedef typename T::SizeType ImageSize;
		typedef typename T::RegionType ImageRegion;
		typedef typename T::IndexType ImageIndex;
        typedef itk::TransformFileReader TransformFileReader;
        typedef itk::TransformFileWriter TransformFileWriter;
        typedef TransformFileReader::TransformType TransformType;
        typedef TransformFileReader::TransformListType   TransformListType;
        
        bool __noverbose;
        
		ImageIO() {
			_pixelType = itk::ImageIOBase::SCALAR;
			_componentType = itk::ImageIOBase::DOUBLE;
            
			itk::ObjectFactoryBase::RegisterFactory(itk::NrrdImageIOFactory::New());
			itk::ObjectFactoryBase::RegisterFactory(itk::NiftiImageIOFactory::New());

            __noverbose = false;
		}
        
		~ImageIO() {
		}

        itk::ImageIOBase::IOComponentType GetComponentType() {
            return itk::ImageIOBase::MapPixelType<typename T::PixelType>::CType;
        }

		const char* GetPixelTypeString(typename itk::ImageIOBase::IOPixelType px) {
			static const char* pixelTypes[] = { "Unknown Pixel Type", "Scalar", "RGB", "RGBA",
				"Offset", "Vector", "Point", "Covariant Vector", "Symmetric Second Rank Tensor",
				"Diffusion Tensor 3D", "Complex", "Fixed Array", "Matrix" };
			return pixelTypes[px];
		}
		const char* GetComponentTypeString(typename itk::ImageIOBase::IOComponentType cx) {
			static const char* componentTypes[] = { "Unknown Component Type", "UCHAR", "CHAR",
				"USHORT", "SHORT", "UINT", "INT", "ULONG", "LONG", "FLOAT", "DOUBLE" };
			return componentTypes[cx];
		}
       	bool FileExists(const char* fileName) {
			ifstream ifile(fileName);
			return ifile != NULL;
		}


        typename itk::ImageIOBase::Pointer GetImageInfo(typename itk::ImageFileReader<T>::Pointer reader) {
            reader->UpdateOutputInformation();
            typename itk::ImageIOBase::Pointer imageIO = reader->GetImageIO();
            _pixelType = imageIO->GetPixelType();
            _componentType = imageIO->GetComponentType();
            return imageIO;
        }


		ImagePointer NewImageT(int sx, int sy, int sz) {
			ImagePointer newImage = T::New();
			ImageSize size;

			size[0] = sx;
			size[1] = sy;
            if (T::GetImageDimension() == 3) {
                size[2] = sz;
            }

			ImageRegion region;
			region.SetSize(size);

			ImageIndex index;
			index.Fill(0);

			region.SetIndex(index);

			newImage->SetLargestPossibleRegion(region);
			newImage->SetBufferedRegion(region);
			newImage->SetRequestedRegion(region);

            if (T::GetImageDimension() == 3) {
                double spacing[3] = { 1, 1, 1 };
                double origin[3] = { 0, 0, 0 };
                newImage->SetOrigin(origin);
                newImage->SetSpacing(spacing);
            } else if (T::GetImageDimension() == 2) {
                double spacing[2] = { 1, 1 };
                double origin[2] = { 0, 0 };
                newImage->SetOrigin(origin);
                newImage->SetSpacing(spacing);
            }
			newImage->Allocate();
			return newImage;
		}
        ImagePointer CopyImage(ImagePointer src) {
            typename itk::ImageDuplicator<T>::Pointer dub = itk::ImageDuplicator<T>::New();
            dub->SetInputImage(src);
            dub->Update();
			return dub->GetOutput();
		}
        
        ImagePointer ReadImage(std::string filename) {
            return ReadImage(filename.c_str());
        }
		ImagePointer ReadImage(const char* filename) {
            if (!__noverbose) {
                cout << "Reading '" << filename << flush;
            }
            if (FileExists(filename)) {
                typename itk::ImageFileReader<T>::Pointer reader = itk::ImageFileReader<T>::New();
                reader->SetFileName(filename);
                itk::ImageIOBase::Pointer baseIO = GetImageInfo(reader);
                if (!__noverbose) {
                    std::cout << "' [" << GetComponentTypeString(_componentType) << ", " << GetPixelTypeString(_pixelType) << "," << baseIO->GetNumberOfComponents() << "]";
                    std::cout << " done." << std::endl;
                }
                reader->Update();
                return reader->GetOutput();
            } else {
                if (!__noverbose) {
                    cout << "' failed. (file not exist)" << endl;
                }
                return ImagePointer();
            }
		}
        ImagePointer ReadCastedImage(std::string inputFilename) {
            ImageInfo info;
            return ReadCastedImage(inputFilename, info);
        }
        ImagePointer ReadCastedImage(std::string inputFilename, ImageInfo& type) {
            if (!FileExists(inputFilename.c_str())) {
                return ImagePointer();
            }

            // construct a generic imageIO factory
            itk::ImageIOBase::Pointer imageIO =
            itk::ImageIOFactory::CreateImageIO(inputFilename.c_str(), itk::ImageIOFactory::ReadMode);
            imageIO->SetFileName(inputFilename);
            imageIO->ReadImageInformation();

            type.numdims = imageIO->GetNumberOfDimensions();
            type.numcomponents = imageIO->GetNumberOfComponents();
            type.dimensions.resize(type.numdims);
            type.spacing.resize(type.numdims);
            type.origin.resize(type.numdims);
            type.direction.resize(type.numdims);
            for (int i = 0; i < type.numdims; i++) {
                type.dimensions[i] = imageIO->GetDimensions(i);
                type.spacing[i] = imageIO->GetSpacing(i);
                type.origin[i] = imageIO->GetOrigin(i);
                type.direction[i] = imageIO->GetDirection(i);
            }
            type.componenttype = itk::ImageIOBase::GetComponentTypeAsString(imageIO->GetComponentType());
            type.pixeltype = itk::ImageIOBase::GetPixelTypeAsString(imageIO->GetPixelType());
            type.componenttypeid = imageIO->GetComponentType();
            type.pixeltypeid = imageIO->GetPixelType();

            // if no casting is required
            if (type.componenttypeid == GetComponentType()) {
                return ReadImage(inputFilename);
            }

            // identify pixel type
            typename itk::ImageIOBase::IOComponentType pixelType = imageIO->GetComponentType();
            // process depending on pixel type
            switch (pixelType) {
                case itk::ImageIOBase::DOUBLE:
                {
                    typedef itk::Image<double,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::FLOAT:
                {
                    typedef itk::Image<float,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::SHORT:
                {
                    typedef itk::Image<short,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::USHORT:
                {
                    typedef itk::Image<unsigned short,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::UCHAR:
                {
                    typedef itk::Image<unsigned char,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::CHAR:
                {
                    typedef itk::Image<char,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::ULONG:
                {
                    typedef itk::Image<unsigned long,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::LONG:
                {
                    typedef itk::Image<long,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::INT:
                {
                    typedef itk::Image<int,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                case itk::ImageIOBase::UINT:
                {
                    typedef itk::Image<unsigned int,DIMENSIONS> I;
                    return CastImageFromS<I>(ReadImageS<I>(inputFilename));
                    break;
                }
                default:
                    break;
            };
            return ImagePointer();
        }
        bool WriteCastedImage(std::string inputFilename, ImagePointer img, std::string type) {
            // otherwise
            itk::ImageIOBase::IOComponentType pixelType = itk::ImageIOBase::GetComponentTypeFromString(type);
//            cout << "converting " << itk::ImageIOBase::GetComponentTypeAsString(GetComponentType()) << " to  " << type << endl;

            // process depending on pixel type
            switch (pixelType) {
                case itk::ImageIOBase::DOUBLE:
                {
                    typedef itk::Image<double,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::FLOAT:
                {
                    typedef itk::Image<float,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::SHORT:
                {
                    typedef itk::Image<short,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::USHORT:
                {
                    typedef itk::Image<unsigned short,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::UCHAR:
                {
                    typedef itk::Image<unsigned char,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::CHAR:
                {
                    typedef itk::Image<char,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::ULONG:
                {
                    typedef itk::Image<unsigned long,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::LONG:
                {
                    typedef itk::Image<long,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::INT:
                {
                    typedef itk::Image<int,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                case itk::ImageIOBase::UINT:
                {
                    typedef itk::Image<unsigned int,DIMENSIONS> I;
                    return WriteImageS<I>(inputFilename, CastImageToS<I>(img));
                    break;
                }
                default:
                    break;
            };
            return false;
        }
        int WriteImage(std::string file, typename T::Pointer img) {
            return WriteImage(file.c_str(), img, true);
        }
        int WriteImage(const char* filename, typename T::Pointer image) {
			WriteImage(filename, image, true);
			return 0;
		}
		int WriteImage(const char* filename, ImagePointer image, bool compression) {
			typename itk::ImageFileWriter<T>::Pointer writer = itk::ImageFileWriter<T>::New();
			writer->SetFileName(filename);
			if (compression) {
				writer->UseCompressionOn();
			}
			writer->SetInput(image);
			std::cout << "Writing '" << filename;
			std::cout.flush();
            try {
                writer->Write();
            } catch (itk::ExceptionObject& e) {
                e.Print(cout);
            }
			std::cout << "' done." << std::endl;
			return 0;
		}


        ////////////////////////////////////////////////////////////////////////////////////////////////
        //
        // Typed Functions
        //

        template <typename S>
        std::string GetComponentTypeAsString(typename S::Pointer img) {
            string type = itk::ImageIOBase::GetComponentTypeAsString(itk::ImageIOBase::MapPixelType<typename S::PixelType>::CType);
            return type;
        }

        // Typed image IO
        template <typename S>
        typename S::Pointer ReadImageS(std::string filename) {
            typename itk::ImageFileReader<S>::Pointer reader = itk::ImageFileReader<S>::New();
            reader->SetFileName(filename);
            reader->Update();
            return reader->GetOutput();
        }
        
        template <typename S>
        typename S::Pointer CopyImageS(typename S::Pointer src) {
            typename itk::ImageDuplicator<S>::Pointer dub = itk::ImageDuplicator<S>::New();
            dub->SetInputImage(src);
            dub->Update();
			return dub->GetOutput();
		}

        // Casting an image to a different typed image
        template <typename S>
        ImagePointer CastImageFromS(typename S::Pointer img) {
            typedef itk::CastImageFilter<S,T> CastFilterType;
            typename CastFilterType::Pointer caster = CastFilterType::New();
            caster->SetInput(img);
            caster->Update();
            return caster->GetOutput();
        }

        // Casting an image to a different typed image
        template <typename S>
        typename S::Pointer CastImageToS(ImagePointer img) {
            typedef itk::CastImageFilter<T,S> CastFilterType;
            typename CastFilterType::Pointer caster = CastFilterType::New();
            caster->SetInput(img);
            caster->Update();
            return caster->GetOutput();
        }

        // Write an image with different type
        template <typename S>
        bool WriteImageS(std::string filename, typename S::Pointer img) {
            if (!__noverbose) {
                cout << "Writing " << filename << flush;
            }
            typename itk::ImageFileWriter<S>::Pointer writer = itk::ImageFileWriter<S>::New();
            writer->SetFileName(filename.c_str());
            writer->SetInput(img);
            writer->UseCompressionOn();
            writer->Write();
            if (!__noverbose) {
                cout << " .. done " << endl;
            }
            return true;
        }

        // Read a transform from file
        typename TransformType::Pointer ReadTransform(char* fileName) {
            typename TransformFileReader::Pointer transformFileReader = TransformFileReader::New();
            transformFileReader->SetFileName(fileName);
            // Create the transforms
            transformFileReader->Update();
            TransformListType* transformList = transformFileReader->GetTransformList();
            return transformList->front();
        }
        
        void WriteSingleTransform(char* fileName, typename TransformType::Pointer transform) {
            typename TransformFileWriter::Pointer writer = TransformFileWriter::New();
            writer->SetFileName(fileName);
            writer->AddTransform(transform);
            writer->Update();
        }
        
        void WriteMultipleTransform(char* fileName, typename TransformFileWriter::ConstTransformListType transformList) {
            typename TransformFileWriter::Pointer writer = TransformFileWriter::New();
            writer->SetFileName(fileName);
            typename TransformFileWriter::ConstTransformListType::iterator transformIter = transformList.begin();
            while (transformIter != transformList.end()) {
                writer->AddTransform(*transformIter);
                transformIter++;
            }
            writer->Update();
        }
        
        typename T::Pointer ResampleImageAs(typename T::Pointer image, typename T::Pointer reference) {
            typedef itk::ResampleImageFilter<T,T> FilterType;
            typedef itk::LinearInterpolateImageFunction<T> InterpolatorType;
            typename FilterType::Pointer filter = FilterType::New();
            filter->SetInput(image);
            filter->SetReferenceImage(reference);
            filter->UseReferenceImageOn();
            filter->SetInterpolator(InterpolatorType::New());
            filter->Update();
            return filter->GetOutput();
        }
        

        static int GetImageDimension() {
            return T::GetImageDimension();
        }
        
        static typename T::Pointer ImportFromMemory3(typename T::PixelType* src, int num, bool freememory, typename T::SizeType sz) {
            typename T::RegionType region;
            region.SetSize(sz);
            typedef itk::ImportImageFilter<typename T::PixelType, 3> FilterType;
            typename FilterType::Pointer filter = FilterType::New();
            filter->SetRegion(region);
            filter->SetImportPointer(src, num, freememory);
            filter->Update();
            return filter->GetOutput();
        }

        static typename T::Pointer ImportFromMemory2(typename T::PixelType* src, int num, bool freememory, typename T::SizeType sz) {
            typename T::RegionType region;
            region.SetSize(sz);
            typedef itk::ImportImageFilter<typename T::PixelType, 2> FilterType;
            typename FilterType::Pointer filter = FilterType::New();
            filter->SetRegion(region);
            filter->SetImportPointer(src, num, freememory);
            filter->Update();
            return filter->GetOutput();
        }


        /*

         typename itk::ImageIOBase::Pointer ReadImageInfo(const char* filename) {
         typename itk::ImageFileReader<T>::Pointer reader = itk::ImageFileReader<T>::New();
         reader->SetFileName(filename);
         return GetImageInfo(reader);
         }




         ImagePointer NewImageT(int sx, int sy, int sz) {
         return NewImageT(sx, sy, sz, static_cast<typename T::PixelType>(0));
         }

         template <typename S>
         void CopyHeaderT(typename S::Pointer src, ImagePointer dst) {
         dst->SetSpacing(src->GetSpacing());
         dst->SetOrigin(src->GetOrigin());
         dst->SetDirection(src->GetDirection());
         return;
         }

         ImagePointer NewImageT(ImagePointer srcImg) {
         return NewImageT<T>(srcImg);
         }

         template <class S>
         ImagePointer NewImageT(typename S::Pointer srcImg) {
         typename S::RegionType srcRegion = srcImg->GetRequestedRegion();
         typename S::SizeType srcSize = srcRegion.GetSize();
         typename T::Pointer newImg = NewImageT(srcSize[0], srcSize[1], srcSize[2], static_cast<typename T::PixelType>(0));
         //CopyHeaderT<S>(srcImg, newImg);
         newImg->CopyInformation(srcImg);
         return newImg;
         }
         */
    };
    
}
#endif
